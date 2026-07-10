#include "helix.h"

#include <stdio.h>
#include <string.h>

static void json_string(FILE *out, const char *text) {
    fputc('"', out);
    if (text != NULL) {
        for (const unsigned char *p = (const unsigned char *)text; *p != '\0'; ++p) {
            switch (*p) {
                case '"':
                    fputs("\\\"", out);
                    break;
                case '\\':
                    fputs("\\\\", out);
                    break;
                case '\b':
                    fputs("\\b", out);
                    break;
                case '\f':
                    fputs("\\f", out);
                    break;
                case '\n':
                    fputs("\\n", out);
                    break;
                case '\r':
                    fputs("\\r", out);
                    break;
                case '\t':
                    fputs("\\t", out);
                    break;
                default:
                    if (*p < 0x20) {
                        fprintf(out, "\\u%04x", *p);
                    } else {
                        fputc(*p, out);
                    }
                    break;
            }
        }
    }
    fputc('"', out);
}

static void json_index(FILE *out, const char *field, uint64_t index, bool comma) {
    char text[32];
    hlx_format_index(index, text, sizeof(text));
    fprintf(out, "\"%s\":%llu,\"%s_text\":", field, (unsigned long long)index, field);
    json_string(out, text);
    if (comma) {
        fputc(',', out);
    }
}

static void write_accounts(FILE *out, const HlxLedger *ledger) {
    bool first = true;
    fputs("\"accounts\":[", out);
    for (size_t i = 0; i < HLX_MAX_ACCOUNTS; ++i) {
        const HlxAccount *account = &ledger->accounts[i];
        if (!account->active) {
            continue;
        }
        if (!first) {
            fputc(',', out);
        }
        first = false;
        fprintf(out, "{\"id\":%u,\"label\":", account->id);
        json_string(out, account->label);
        fprintf(out, ",\"external_assets\":%llu,\"withdrawn_assets\":%llu}", (unsigned long long)account->external_assets, (unsigned long long)account->withdrawn_assets);
    }
    fputc(']', out);
}

static void write_vaults(FILE *out, const HlxLedger *ledger) {
    bool first = true;
    fputs("\"vaults\":[", out);
    for (size_t i = 0; i < HLX_MAX_VAULTS; ++i) {
        const HlxVault *vault = &ledger->vaults[i];
        if (!vault->active) {
            continue;
        }
        if (!first) {
            fputc(',', out);
        }
        first = false;
        fprintf(out, "{\"id\":%u,\"symbol\":", vault->id);
        json_string(out, vault->symbol);
        fprintf(out, ",\"epoch\":%u,", vault->epoch);
        json_index(out, "share_index", vault->share_index, true);
        json_index(out, "previous_index", vault->previous_index, true);
        fprintf(out, "\"reserves\":%llu,", (unsigned long long)vault->reserves);
        fprintf(out, "\"live_shares\":%llu,", (unsigned long long)vault->live_shares);
        fprintf(out, "\"locked_shares\":%llu,", (unsigned long long)vault->locked_shares);
        fprintf(out, "\"pending_shares\":%llu,", (unsigned long long)vault->pending_shares);
        fprintf(out, "\"retired_shares\":%llu,", (unsigned long long)vault->retired_shares);
        fprintf(out, "\"pending_assets\":%llu,", (unsigned long long)vault->pending_assets);
        fprintf(out, "\"settled_assets\":%llu,", (unsigned long long)vault->settled_assets);
        fprintf(out, "\"accrued_assets\":%llu,", (unsigned long long)vault->accrued_assets);
        fprintf(out, "\"deficit_assets\":%llu,", (unsigned long long)vault->deficit_assets);
        fprintf(out, "\"excess_quoted_assets\":%llu,", (unsigned long long)vault->excess_quoted_assets);
        fprintf(out, "\"insolvent\":%s}", vault->insolvent ? "true" : "false");
    }
    fputc(']', out);
}

static void write_positions(FILE *out, const HlxLedger *ledger) {
    bool first = true;
    fputs("\"positions\":[", out);
    for (size_t i = 0; i < HLX_MAX_POSITIONS; ++i) {
        const HlxPosition *position = &ledger->positions[i];
        if (!position->active) {
            continue;
        }
        if (!first) {
            fputc(',', out);
        }
        first = false;
        fprintf(out, "{\"id\":%u,\"account_id\":%u,\"vault_id\":%u,\"liquid_shares\":%llu,", position->id, position->account_id, position->vault_id, (unsigned long long)position->liquid_shares);
        json_index(out, "issue_index", position->issue_index, true);
        fprintf(out, "\"last_epoch\":%u}", position->last_epoch);
    }
    fputc(']', out);
}

static void write_locks(FILE *out, const HlxLedger *ledger) {
    bool first = true;
    fputs("\"locks\":[", out);
    for (size_t i = 0; i < HLX_MAX_LOCKS; ++i) {
        const HlxLock *lock = &ledger->locks[i];
        if (!lock->active) {
            continue;
        }
        if (!first) {
            fputc(',', out);
        }
        first = false;
        fprintf(out, "{\"id\":%u,\"owner_id\":%u,\"origin_account_id\":%u,\"vault_id\":%u,", lock->id, lock->owner_id, lock->origin_account_id, lock->vault_id);
        fprintf(out, "\"origin_position_id\":%u,\"opened_epoch\":%u,\"transfer_depth\":%u,\"parent_lock_id\":%u,", lock->origin_position_id, lock->opened_epoch, lock->transfer_depth, lock->parent_lock_id);
        fprintf(out, "\"shares\":%llu,\"redeemed_shares\":%llu,", (unsigned long long)lock->shares, (unsigned long long)lock->redeemed_shares);
        json_index(out, "issued_index", lock->issued_index, true);
        json_index(out, "redemption_index", lock->redemption_index, true);
        fprintf(out, "\"flags\":%u,\"index_refreshed\":%s,\"closed\":%s,", lock->flags, (lock->flags & HLX_LOCK_FLAG_INDEX_REFRESH) ? "true" : "false", lock->closed ? "true" : "false");
        fprintf(out, "\"excess_quote_assets\":%llu}", (unsigned long long)lock->excess_quote_assets);
    }
    fputc(']', out);
}

static void write_redemptions(FILE *out, const HlxLedger *ledger) {
    bool first = true;
    fputs("\"redemptions\":[", out);
    for (size_t i = 0; i < HLX_MAX_REDEMPTIONS; ++i) {
        const HlxRedemption *redemption = &ledger->redemptions[i];
        uint64_t expected_assets;
        if (!redemption->active) {
            continue;
        }
        expected_assets = hlx_shares_to_assets(redemption->shares, redemption->issued_index);
        if (!first) {
            fputc(',', out);
        }
        first = false;
        fprintf(out, "{\"id\":%u,\"owner_id\":%u,\"vault_id\":%u,\"lock_id\":%u,", redemption->id, redemption->owner_id, redemption->vault_id, redemption->lock_id);
        fprintf(out, "\"requested_epoch\":%u,\"settled_epoch\":%u,", redemption->requested_epoch, redemption->settled_epoch);
        fprintf(out, "\"shares\":%llu,\"assets\":%llu,\"expected_assets\":%llu,", (unsigned long long)redemption->shares, (unsigned long long)redemption->assets, (unsigned long long)expected_assets);
        json_index(out, "issued_index", redemption->issued_index, true);
        json_index(out, "quote_index", redemption->quote_index, true);
        fprintf(out, "\"excess_quote_assets\":%llu,\"settled\":%s,\"withdrawn\":%s}", (unsigned long long)redemption->excess_quote_assets, redemption->settled ? "true" : "false", redemption->withdrawn ? "true" : "false");
    }
    fputc(']', out);
}

static void write_quotes(FILE *out, const HlxLedger *ledger) {
    bool first = true;
    fputs("\"quotes\":[", out);
    for (size_t i = 0; i < HLX_MAX_QUOTES; ++i) {
        const HlxQuote *quote = &ledger->quotes[i];
        if (!quote->active) {
            continue;
        }
        if (!first) {
            fputc(',', out);
        }
        first = false;
        fprintf(out, "{\"id\":%u,\"kind\":", quote->id);
        json_string(out, quote->kind);
        fprintf(out, ",\"account_id\":%u,\"vault_id\":%u,\"lock_id\":%u,\"epoch\":%u,", quote->account_id, quote->vault_id, quote->lock_id, quote->epoch);
        fprintf(out, "\"input_amount\":%llu,\"shares\":%llu,\"assets\":%llu,\"expected_assets\":%llu,", (unsigned long long)quote->input_amount, (unsigned long long)quote->shares, (unsigned long long)quote->assets, (unsigned long long)quote->expected_assets);
        json_index(out, "index", quote->index, true);
        json_index(out, "issued_index", quote->issued_index, true);
        fprintf(out, "\"accepted\":%s,\"over_issued_quote\":%s,\"note\":", quote->accepted ? "true" : "false", quote->over_issued_quote ? "true" : "false");
        json_string(out, quote->note);
        fputc('}', out);
    }
    fputc(']', out);
}

static void write_account_views(FILE *out, const HlxLedger *ledger) {
    bool first = true;
    fputs("\"account_views\":[", out);
    for (size_t i = 0; i < HLX_MAX_ACCOUNTS; ++i) {
        const HlxAccount *account = &ledger->accounts[i];
        if (!account->active) {
            continue;
        }
        for (size_t j = 0; j < HLX_MAX_VAULTS; ++j) {
            const HlxVault *vault = &ledger->vaults[j];
            uint64_t liquid_shares;
            uint64_t locked_shares;
            uint64_t pending_assets;
            uint64_t withdrawable_assets;
            if (!vault->active) {
                continue;
            }
            liquid_shares = hlx_account_liquid_shares(ledger, account->id, vault->id);
            locked_shares = hlx_account_locked_shares(ledger, account->id, vault->id);
            pending_assets = hlx_account_pending_assets(ledger, account->id, vault->id);
            withdrawable_assets = hlx_account_withdrawable_assets(ledger, account->id, vault->id);
            if (liquid_shares == 0 && locked_shares == 0 && pending_assets == 0 && withdrawable_assets == 0) {
                continue;
            }
            if (!first) {
                fputc(',', out);
            }
            first = false;
            fprintf(out, "{\"account_id\":%u,\"vault_id\":%u,", account->id, vault->id);
            fprintf(out, "\"liquid_shares\":%llu,\"locked_shares\":%llu,", (unsigned long long)liquid_shares, (unsigned long long)locked_shares);
            fprintf(out, "\"pending_assets\":%llu,\"withdrawable_assets\":%llu,", (unsigned long long)pending_assets, (unsigned long long)withdrawable_assets);
            fprintf(out, "\"open_lock_count\":%u}", hlx_account_open_lock_count(ledger, account->id));
        }
    }
    fputc(']', out);
}

static void write_vault_views(FILE *out, const HlxLedger *ledger) {
    bool first = true;
    fputs("\"vault_views\":[", out);
    for (size_t i = 0; i < HLX_MAX_VAULTS; ++i) {
        const HlxVault *vault = &ledger->vaults[i];
        if (!vault->active) {
            continue;
        }
        if (!first) {
            fputc(',', out);
        }
        first = false;
        fprintf(out, "{\"vault_id\":%u,", vault->id);
        fprintf(out, "\"open_lock_shares\":%llu,", (unsigned long long)hlx_vault_open_lock_shares(ledger, vault->id));
        fprintf(out, "\"pending_redemption_assets\":%llu,", (unsigned long long)hlx_vault_pending_redemption_assets(ledger, vault->id));
        fprintf(out, "\"pending_redemption_count\":%u}", hlx_vault_pending_redemption_count(ledger, vault->id));
    }
    fputc(']', out);
}

static void write_events(FILE *out, const HlxLedger *ledger) {
    bool first = true;
    fputs("\"events\":[", out);
    for (size_t i = 0; i < HLX_MAX_EVENTS; ++i) {
        const HlxEvent *event = &ledger->events[i];
        if (!event->active) {
            continue;
        }
        if (!first) {
            fputc(',', out);
        }
        first = false;
        fprintf(out, "{\"serial\":%u,\"kind\":", event->serial);
        json_string(out, event->kind);
        fprintf(out, ",\"epoch\":%u,\"account_id\":%u,\"vault_id\":%u,\"amount\":%llu,\"note\":", event->epoch, event->account_id, event->vault_id, (unsigned long long)event->amount);
        json_string(out, event->note);
        fputc('}', out);
    }
    fputc(']', out);
}

void hlx_write_snapshot_json(const HlxLedger *ledger, const char *scenario, FILE *out) {
    char digest[HLX_DIGEST_SIZE];
    HlxInvariantReport report;
    hlx_state_digest(ledger, digest);
    hlx_invariant_report(ledger, &report);

    fputc('{', out);
    fputs("\"scenario\":", out);
    json_string(out, scenario == NULL ? "snapshot" : scenario);
    fputc(',', out);
    fputs("\"network_id\":", out);
    json_string(out, ledger->network_id);
    fprintf(out, ",\"index_scale\":%llu,", (unsigned long long)HLX_INDEX_SCALE);
    fputs("\"state_digest\":", out);
    json_string(out, digest);
    fputc(',', out);
    fprintf(out, "\"account_count\":%u,\"vault_count\":%u,", report.active_accounts, report.active_vaults);
    fprintf(out, "\"totals\":{\"external_assets\":%llu,\"vault_reserves\":%llu,\"deficit_assets\":%llu,\"excess_quote_assets\":%llu},", (unsigned long long)report.account_assets, (unsigned long long)report.vault_reserves, (unsigned long long)report.deficit_assets, (unsigned long long)report.excess_quote_assets);
    fprintf(out, "\"risk\":{\"insolvent_vaults\":%u,\"has_excess_quote\":%s},", hlx_count_insolvent_vaults(ledger), hlx_total_excess_quote_assets(ledger) > 0 ? "true" : "false");
    fprintf(out, "\"invariants\":{\"solvent\":%s,\"quotes_ok\":%s,\"pending_ok\":%s,", report.solvent ? "true" : "false", report.quotes_ok ? "true" : "false", report.pending_ok ? "true" : "false");
    fprintf(out, "\"live_share_assets\":%llu,\"locked_issue_assets\":%llu,\"locked_redemption_assets\":%llu,", (unsigned long long)report.live_share_assets, (unsigned long long)report.locked_issue_assets, (unsigned long long)report.locked_redemption_assets);
    fprintf(out, "\"pending_assets\":%llu,\"settled_assets\":%llu,\"claim_assets\":%llu,", (unsigned long long)report.pending_assets, (unsigned long long)report.settled_assets, (unsigned long long)report.claim_assets);
    fprintf(out, "\"claim_gap_assets\":%llu,\"undercollateralized_assets\":%llu,", (unsigned long long)report.claim_gap_assets, (unsigned long long)report.undercollateralized_assets);
    fprintf(out, "\"open_locks\":%u,\"closed_locks\":%u,\"refreshed_locks\":%u,", report.open_locks, report.closed_locks, report.refreshed_locks);
    fprintf(out, "\"pending_redemptions\":%u,\"settled_redemptions\":%u,\"withdrawn_redemptions\":%u},", report.pending_redemptions, report.settled_redemptions, report.withdrawn_redemptions);
    write_accounts(out, ledger);
    fputc(',', out);
    write_vaults(out, ledger);
    fputc(',', out);
    write_positions(out, ledger);
    fputc(',', out);
    write_locks(out, ledger);
    fputc(',', out);
    write_redemptions(out, ledger);
    fputc(',', out);
    write_quotes(out, ledger);
    fputc(',', out);
    write_account_views(out, ledger);
    fputc(',', out);
    write_vault_views(out, ledger);
    fputc(',', out);
    write_events(out, ledger);
    fputs("}\n", out);
}

void hlx_write_error_json(FILE *out, HlxStatus status, const char *detail, size_t line_no) {
    fputs("{\"error\":true,\"status\":", out);
    json_string(out, hlx_status_message(status));
    fputs(",\"detail\":", out);
    json_string(out, detail == NULL ? "" : detail);
    if (line_no > 0) {
        fprintf(out, ",\"line\":%llu", (unsigned long long)line_no);
    }
    fputs("}\n", out);
}
