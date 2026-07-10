#include "helix.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static HlxStatus quote_fail(HlxLedger *ledger, HlxStatus status, const char *fmt, ...) {
    va_list args;
    if (ledger != NULL) {
        va_start(args, fmt);
        vsnprintf(ledger->last_error, sizeof(ledger->last_error), fmt, args);
        va_end(args);
    }
    return status;
}

static void quote_copy(char *dst, size_t dst_size, const char *src) {
    if (dst_size == 0) {
        return;
    }
    snprintf(dst, dst_size, "%s", src == NULL ? "" : src);
}

static HlxQuote *allocate_quote(HlxLedger *ledger) {
    for (size_t i = 0; i < HLX_MAX_QUOTES; ++i) {
        if (!ledger->quotes[i].active) {
            HlxQuote *quote = &ledger->quotes[i];
            memset(quote, 0, sizeof(*quote));
            quote->active = true;
            quote->id = ledger->next_quote_id++;
            return quote;
        }
    }
    return NULL;
}

HlxStatus hlx_quote_deposit(HlxLedger *ledger, uint32_t account_id, uint32_t vault_id, uint64_t assets, uint32_t *quote_id_out) {
    const HlxAccount *account = hlx_get_account(ledger, account_id);
    const HlxVault *vault = hlx_get_vault(ledger, vault_id);
    HlxQuote *quote;
    uint64_t shares;

    if (account == NULL) {
        return quote_fail(ledger, HLX_ERR_NOT_FOUND, "account %u not found", account_id);
    }
    if (vault == NULL) {
        return quote_fail(ledger, HLX_ERR_NOT_FOUND, "vault %u not found", vault_id);
    }
    if (assets == 0) {
        return quote_fail(ledger, HLX_ERR_BALANCE, "deposit quote cannot be zero");
    }

    quote = allocate_quote(ledger);
    if (quote == NULL) {
        return quote_fail(ledger, HLX_ERR_CAPACITY, "quote capacity reached");
    }

    shares = hlx_assets_to_shares(assets, vault->share_index);
    quote->account_id = account_id;
    quote->vault_id = vault_id;
    quote->epoch = vault->epoch;
    quote->input_amount = assets;
    quote->shares = shares;
    quote->assets = assets;
    quote->index = vault->share_index;
    quote->accepted = shares > 0 && account->external_assets >= assets && !vault->insolvent;
    quote_copy(quote->kind, sizeof(quote->kind), "deposit");
    quote_copy(quote->note, sizeof(quote->note), quote->accepted ? "deposit quote accepted" : "deposit quote would not execute");

    if (quote_id_out != NULL) {
        *quote_id_out = quote->id;
    }
    return HLX_OK;
}

HlxStatus hlx_quote_redemption(HlxLedger *ledger, uint32_t account_id, uint32_t lock_id, uint64_t shares, uint32_t *quote_id_out) {
    const HlxAccount *account = hlx_get_account(ledger, account_id);
    const HlxLock *lock = hlx_get_lock(ledger, lock_id);
    const HlxVault *vault;
    HlxQuote *quote;
    uint64_t assets;
    uint64_t expected_assets;

    if (account == NULL) {
        return quote_fail(ledger, HLX_ERR_NOT_FOUND, "account %u not found", account_id);
    }
    if (lock == NULL || lock->closed) {
        return quote_fail(ledger, HLX_ERR_NOT_FOUND, "lock %u not found", lock_id);
    }
    if (lock->owner_id != account_id) {
        return quote_fail(ledger, HLX_ERR_STATE, "account %u does not own lock %u", account_id, lock_id);
    }
    if (shares == 0 || lock->shares < shares) {
        return quote_fail(ledger, HLX_ERR_SHARES, "lock %u has insufficient shares for quote", lock_id);
    }
    vault = hlx_get_vault(ledger, lock->vault_id);
    if (vault == NULL) {
        return quote_fail(ledger, HLX_ERR_NOT_FOUND, "vault %u not found", lock->vault_id);
    }

    quote = allocate_quote(ledger);
    if (quote == NULL) {
        return quote_fail(ledger, HLX_ERR_CAPACITY, "quote capacity reached");
    }

    assets = hlx_shares_to_assets(shares, lock->redemption_index);
    expected_assets = hlx_shares_to_assets(shares, lock->issued_index);
    quote->account_id = account_id;
    quote->vault_id = lock->vault_id;
    quote->lock_id = lock_id;
    quote->epoch = vault->epoch;
    quote->input_amount = shares;
    quote->shares = shares;
    quote->assets = assets;
    quote->expected_assets = expected_assets;
    quote->index = lock->redemption_index;
    quote->issued_index = lock->issued_index;
    quote->accepted = true;
    quote->over_issued_quote = assets > expected_assets;
    quote_copy(quote->kind, sizeof(quote->kind), "redemption");
    quote_copy(quote->note, sizeof(quote->note), quote->over_issued_quote ? "redemption quote exceeds issued index" : "redemption quote accepted");

    if (quote_id_out != NULL) {
        *quote_id_out = quote->id;
    }
    return HLX_OK;
}
