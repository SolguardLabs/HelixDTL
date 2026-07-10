#include "helix.h"

#include <string.h>

static void add_account_metrics(const HlxLedger *ledger, HlxInvariantReport *report) {
    for (size_t i = 0; i < HLX_MAX_ACCOUNTS; ++i) {
        const HlxAccount *account = &ledger->accounts[i];
        if (!account->active) {
            continue;
        }
        report->active_accounts++;
        report->account_assets += account->external_assets;
    }
}

static void add_vault_metrics(const HlxLedger *ledger, HlxInvariantReport *report) {
    for (size_t i = 0; i < HLX_MAX_VAULTS; ++i) {
        const HlxVault *vault = &ledger->vaults[i];
        if (!vault->active) {
            continue;
        }
        report->active_vaults++;
        report->vault_reserves += vault->reserves;
        report->deficit_assets += vault->deficit_assets;
        report->excess_quote_assets += vault->excess_quoted_assets;
        report->pending_assets += vault->pending_assets;
        report->settled_assets += vault->settled_assets;
        report->live_share_assets += hlx_shares_to_assets(vault->live_shares, vault->share_index);
        if (vault->insolvent || vault->deficit_assets > 0) {
            report->solvent = false;
        }
        if (vault->pending_assets > vault->reserves + vault->deficit_assets) {
            report->pending_ok = false;
        }
    }
}

static void add_lock_metrics(const HlxLedger *ledger, HlxInvariantReport *report) {
    for (size_t i = 0; i < HLX_MAX_LOCKS; ++i) {
        const HlxLock *lock = &ledger->locks[i];
        uint64_t issued_value;
        uint64_t redemption_value;

        if (!lock->active) {
            continue;
        }

        if (lock->closed) {
            report->closed_locks++;
        } else {
            report->open_locks++;
        }

        if ((lock->flags & HLX_LOCK_FLAG_INDEX_REFRESH) != 0) {
            report->refreshed_locks++;
        }

        issued_value = hlx_shares_to_assets(lock->shares, lock->issued_index);
        redemption_value = hlx_shares_to_assets(lock->shares, lock->redemption_index);
        report->locked_issue_assets += issued_value;
        report->locked_redemption_assets += redemption_value;

        if (lock->redemption_index > lock->issued_index || lock->excess_quote_assets > 0) {
            report->quotes_ok = false;
        }
    }
}

static void add_redemption_metrics(const HlxLedger *ledger, HlxInvariantReport *report) {
    for (size_t i = 0; i < HLX_MAX_REDEMPTIONS; ++i) {
        const HlxRedemption *redemption = &ledger->redemptions[i];
        uint64_t expected_assets;

        if (!redemption->active) {
            continue;
        }

        expected_assets = hlx_shares_to_assets(redemption->shares, redemption->issued_index);
        report->claim_assets += redemption->assets;

        if (redemption->withdrawn) {
            report->withdrawn_redemptions++;
        } else if (redemption->settled) {
            report->settled_redemptions++;
        } else {
            report->pending_redemptions++;
        }

        if (redemption->assets > expected_assets || redemption->quote_index > redemption->issued_index || redemption->excess_quote_assets > 0) {
            report->quotes_ok = false;
        }
    }
}

static void finalize_backing_metrics(HlxInvariantReport *report) {
    uint64_t backing = report->vault_reserves + report->deficit_assets;
    uint64_t claims = report->live_share_assets + report->locked_issue_assets + report->pending_assets;

    if (backing >= claims) {
        report->claim_gap_assets = backing - claims;
        report->undercollateralized_assets = 0;
    } else {
        report->claim_gap_assets = 0;
        report->undercollateralized_assets = claims - backing;
        report->solvent = false;
    }

    if (report->excess_quote_assets > 0) {
        report->quotes_ok = false;
    }
}

void hlx_invariant_report(const HlxLedger *ledger, HlxInvariantReport *report) {
    memset(report, 0, sizeof(*report));
    report->solvent = true;
    report->quotes_ok = true;
    report->pending_ok = true;

    add_account_metrics(ledger, report);
    add_vault_metrics(ledger, report);
    add_lock_metrics(ledger, report);
    add_redemption_metrics(ledger, report);
    finalize_backing_metrics(report);
}
