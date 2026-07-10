#include "helix.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static void copy_label(char *dst, size_t dst_size, const char *src) {
    if (dst_size == 0) {
        return;
    }
    if (src == NULL) {
        src = "";
    }
    snprintf(dst, dst_size, "%s", src);
}

static HlxStatus fail(HlxLedger *ledger, HlxStatus status, const char *fmt, ...) {
    va_list args;
    if (ledger != NULL) {
        va_start(args, fmt);
        vsnprintf(ledger->last_error, sizeof(ledger->last_error), fmt, args);
        va_end(args);
    }
    return status;
}

static HlxAccount *find_account(HlxLedger *ledger, uint32_t id) {
    for (size_t i = 0; i < HLX_MAX_ACCOUNTS; ++i) {
        if (ledger->accounts[i].active && ledger->accounts[i].id == id) {
            return &ledger->accounts[i];
        }
    }
    return NULL;
}

static HlxVault *find_vault(HlxLedger *ledger, uint32_t id) {
    for (size_t i = 0; i < HLX_MAX_VAULTS; ++i) {
        if (ledger->vaults[i].active && ledger->vaults[i].id == id) {
            return &ledger->vaults[i];
        }
    }
    return NULL;
}

static HlxPosition *find_position(HlxLedger *ledger, uint32_t account_id, uint32_t vault_id) {
    for (size_t i = 0; i < HLX_MAX_POSITIONS; ++i) {
        HlxPosition *position = &ledger->positions[i];
        if (position->active && position->account_id == account_id && position->vault_id == vault_id) {
            return position;
        }
    }
    return NULL;
}

static HlxLock *find_lock(HlxLedger *ledger, uint32_t id) {
    for (size_t i = 0; i < HLX_MAX_LOCKS; ++i) {
        if (ledger->locks[i].active && ledger->locks[i].id == id) {
            return &ledger->locks[i];
        }
    }
    return NULL;
}

static HlxRedemption *find_redemption(HlxLedger *ledger, uint32_t id) {
    for (size_t i = 0; i < HLX_MAX_REDEMPTIONS; ++i) {
        if (ledger->redemptions[i].active && ledger->redemptions[i].id == id) {
            return &ledger->redemptions[i];
        }
    }
    return NULL;
}

static HlxPosition *allocate_position(HlxLedger *ledger) {
    for (size_t i = 0; i < HLX_MAX_POSITIONS; ++i) {
        if (!ledger->positions[i].active) {
            HlxPosition *position = &ledger->positions[i];
            memset(position, 0, sizeof(*position));
            position->active = true;
            position->id = ledger->next_position_id++;
            return position;
        }
    }
    return NULL;
}

static HlxLock *allocate_lock(HlxLedger *ledger) {
    for (size_t i = 0; i < HLX_MAX_LOCKS; ++i) {
        if (!ledger->locks[i].active) {
            HlxLock *lock = &ledger->locks[i];
            memset(lock, 0, sizeof(*lock));
            lock->active = true;
            lock->id = ledger->next_lock_id++;
            return lock;
        }
    }
    return NULL;
}

static HlxRedemption *allocate_redemption(HlxLedger *ledger) {
    for (size_t i = 0; i < HLX_MAX_REDEMPTIONS; ++i) {
        if (!ledger->redemptions[i].active) {
            HlxRedemption *redemption = &ledger->redemptions[i];
            memset(redemption, 0, sizeof(*redemption));
            redemption->active = true;
            redemption->id = ledger->next_redemption_id++;
            return redemption;
        }
    }
    return NULL;
}

static void add_event(HlxLedger *ledger, const char *kind, uint32_t account_id, uint32_t vault_id, uint32_t epoch, uint64_t amount, const char *note) {
    HlxEvent *event = NULL;
    for (size_t i = 0; i < HLX_MAX_EVENTS; ++i) {
        if (!ledger->events[i].active) {
            event = &ledger->events[i];
            break;
        }
    }
    if (event == NULL) {
        return;
    }
    memset(event, 0, sizeof(*event));
    event->active = true;
    event->serial = ledger->next_event_serial++;
    event->epoch = epoch;
    event->account_id = account_id;
    event->vault_id = vault_id;
    event->amount = amount;
    copy_label(event->kind, sizeof(event->kind), kind);
    copy_label(event->note, sizeof(event->note), note);
}

void hlx_ledger_init(HlxLedger *ledger, const char *network_id) {
    memset(ledger, 0, sizeof(*ledger));
    copy_label(ledger->network_id, sizeof(ledger->network_id), network_id == NULL ? "helix-local" : network_id);
    ledger->next_position_id = 1;
    ledger->next_lock_id = 1;
    ledger->next_redemption_id = 1;
    ledger->next_quote_id = 1;
    ledger->next_event_serial = 1;
    copy_label(ledger->last_error, sizeof(ledger->last_error), "ok");
}

HlxStatus hlx_add_account(HlxLedger *ledger, uint32_t id, const char *label, uint64_t assets) {
    if (find_account(ledger, id) != NULL) {
        return fail(ledger, HLX_ERR_EXISTS, "account %u already exists", id);
    }
    for (size_t i = 0; i < HLX_MAX_ACCOUNTS; ++i) {
        if (!ledger->accounts[i].active) {
            HlxAccount *account = &ledger->accounts[i];
            memset(account, 0, sizeof(*account));
            account->active = true;
            account->id = id;
            account->external_assets = assets;
            copy_label(account->label, sizeof(account->label), label);
            add_event(ledger, "account", id, 0, 0, assets, "account registered");
            return HLX_OK;
        }
    }
    return fail(ledger, HLX_ERR_CAPACITY, "account capacity reached");
}

HlxStatus hlx_add_vault(HlxLedger *ledger, uint32_t id, const char *symbol, uint64_t index) {
    if (index == 0) {
        return fail(ledger, HLX_ERR_INDEX, "vault %u index cannot be zero", id);
    }
    if (find_vault(ledger, id) != NULL) {
        return fail(ledger, HLX_ERR_EXISTS, "vault %u already exists", id);
    }
    for (size_t i = 0; i < HLX_MAX_VAULTS; ++i) {
        if (!ledger->vaults[i].active) {
            HlxVault *vault = &ledger->vaults[i];
            memset(vault, 0, sizeof(*vault));
            vault->active = true;
            vault->id = id;
            vault->epoch = 1;
            vault->share_index = index;
            vault->previous_index = index;
            copy_label(vault->symbol, sizeof(vault->symbol), symbol);
            add_event(ledger, "vault", 0, id, vault->epoch, index, "vault registered");
            return HLX_OK;
        }
    }
    return fail(ledger, HLX_ERR_CAPACITY, "vault capacity reached");
}

HlxStatus hlx_deposit(HlxLedger *ledger, uint32_t account_id, uint32_t vault_id, uint64_t assets, uint64_t *shares_out) {
    HlxAccount *account = find_account(ledger, account_id);
    HlxVault *vault = find_vault(ledger, vault_id);
    HlxPosition *position;
    uint64_t shares;
    uint64_t old_shares;

    if (account == NULL) {
        return fail(ledger, HLX_ERR_NOT_FOUND, "account %u not found", account_id);
    }
    if (vault == NULL) {
        return fail(ledger, HLX_ERR_NOT_FOUND, "vault %u not found", vault_id);
    }
    if (assets == 0) {
        return fail(ledger, HLX_ERR_BALANCE, "deposit cannot be zero");
    }
    if (account->external_assets < assets) {
        return fail(ledger, HLX_ERR_BALANCE, "account %u has insufficient external assets", account_id);
    }

    shares = hlx_assets_to_shares(assets, vault->share_index);
    if (shares == 0) {
        return fail(ledger, HLX_ERR_SHARES, "deposit rounded to zero shares");
    }

    position = find_position(ledger, account_id, vault_id);
    if (position == NULL) {
        position = allocate_position(ledger);
        if (position == NULL) {
            return fail(ledger, HLX_ERR_CAPACITY, "position capacity reached");
        }
        position->account_id = account_id;
        position->vault_id = vault_id;
        position->issue_index = vault->share_index;
    }

    old_shares = position->liquid_shares;
    if (old_shares == 0) {
        position->issue_index = vault->share_index;
    } else {
        uint64_t old_weight = old_shares * position->issue_index;
        uint64_t new_weight = shares * vault->share_index;
        position->issue_index = (old_weight + new_weight) / (old_shares + shares);
    }

    position->liquid_shares += shares;
    position->last_epoch = vault->epoch;
    account->external_assets -= assets;
    vault->reserves += assets;
    vault->live_shares += shares;
    if (shares_out != NULL) {
        *shares_out = shares;
    }
    add_event(ledger, "deposit", account_id, vault_id, vault->epoch, assets, "assets converted to live shares");
    return HLX_OK;
}

HlxStatus hlx_lock_shares(HlxLedger *ledger, uint32_t account_id, uint32_t vault_id, uint64_t shares, uint32_t *lock_id_out) {
    HlxAccount *account = find_account(ledger, account_id);
    HlxVault *vault = find_vault(ledger, vault_id);
    HlxPosition *position;
    HlxLock *lock;

    if (account == NULL) {
        return fail(ledger, HLX_ERR_NOT_FOUND, "account %u not found", account_id);
    }
    if (vault == NULL) {
        return fail(ledger, HLX_ERR_NOT_FOUND, "vault %u not found", vault_id);
    }
    if (shares == 0) {
        return fail(ledger, HLX_ERR_SHARES, "lock cannot be zero shares");
    }
    position = find_position(ledger, account_id, vault_id);
    if (position == NULL || position->liquid_shares < shares) {
        return fail(ledger, HLX_ERR_SHARES, "account %u has insufficient liquid shares in vault %u", account_id, vault_id);
    }

    lock = allocate_lock(ledger);
    if (lock == NULL) {
        return fail(ledger, HLX_ERR_CAPACITY, "lock capacity reached");
    }

    position->liquid_shares -= shares;
    position->last_epoch = vault->epoch;
    vault->live_shares -= shares;
    vault->locked_shares += shares;

    lock->owner_id = account_id;
    lock->origin_account_id = account_id;
    lock->vault_id = vault_id;
    lock->origin_position_id = position->id;
    lock->opened_epoch = vault->epoch;
    lock->shares = shares;
    lock->issued_index = vault->share_index;
    lock->redemption_index = vault->share_index;

    if (lock_id_out != NULL) {
        *lock_id_out = lock->id;
    }
    add_event(ledger, "lock", account_id, vault_id, vault->epoch, shares, "live shares moved to locked shares");
    return HLX_OK;
}

HlxStatus hlx_transfer_locked(HlxLedger *ledger, uint32_t from_account_id, uint32_t to_account_id, uint32_t lock_id, uint64_t shares, uint32_t *child_lock_id_out) {
    HlxAccount *from = find_account(ledger, from_account_id);
    HlxAccount *to = find_account(ledger, to_account_id);
    HlxLock *source = find_lock(ledger, lock_id);
    HlxVault *vault;
    HlxLock *child;
    bool refreshed_index = false;

    if (from == NULL) {
        return fail(ledger, HLX_ERR_NOT_FOUND, "source account %u not found", from_account_id);
    }
    if (to == NULL) {
        return fail(ledger, HLX_ERR_NOT_FOUND, "target account %u not found", to_account_id);
    }
    if (source == NULL || source->closed) {
        return fail(ledger, HLX_ERR_NOT_FOUND, "lock %u not found", lock_id);
    }
    if (source->owner_id != from_account_id) {
        return fail(ledger, HLX_ERR_STATE, "account %u does not own lock %u", from_account_id, lock_id);
    }
    if (shares == 0 || source->shares < shares) {
        return fail(ledger, HLX_ERR_SHARES, "lock %u has insufficient shares", lock_id);
    }
    vault = find_vault(ledger, source->vault_id);
    if (vault == NULL) {
        return fail(ledger, HLX_ERR_NOT_FOUND, "vault %u not found", source->vault_id);
    }

    child = allocate_lock(ledger);
    if (child == NULL) {
        return fail(ledger, HLX_ERR_CAPACITY, "lock capacity reached");
    }

    source->shares -= shares;
    source->flags |= HLX_LOCK_FLAG_PARTIAL_SOURCE;
    if (source->shares == 0) {
        source->closed = true;
    }

    child->owner_id = to_account_id;
    child->origin_account_id = source->origin_account_id;
    child->vault_id = source->vault_id;
    child->origin_position_id = source->origin_position_id;
    child->opened_epoch = vault->epoch;
    child->transfer_depth = source->transfer_depth + 1;
    child->parent_lock_id = source->id;
    child->shares = shares;
    child->issued_index = source->issued_index;
    child->redemption_index = source->redemption_index;

    /*
     * Challenge bug: a locked share transfer after an epoch change should preserve
     * the source lock's redemption index. This branch treats the child lock as a
     * fresh epoch claim and gives it the vault's current index.
     */
    if (from_account_id != to_account_id && vault->epoch > source->opened_epoch) {
        child->redemption_index = vault->share_index;
        child->flags |= HLX_LOCK_FLAG_INDEX_REFRESH;
        refreshed_index = true;
    }

    if (child_lock_id_out != NULL) {
        *child_lock_id_out = child->id;
    }
    add_event(ledger, "transfer_lock", to_account_id, source->vault_id, vault->epoch, shares, refreshed_index ? "locked transfer refreshed redemption index" : "locked transfer preserved redemption index");
    return HLX_OK;
}

HlxStatus hlx_request_redemption(HlxLedger *ledger, uint32_t account_id, uint32_t lock_id, uint64_t shares, uint32_t *redemption_id_out) {
    HlxAccount *account = find_account(ledger, account_id);
    HlxLock *lock = find_lock(ledger, lock_id);
    HlxVault *vault;
    HlxRedemption *redemption;
    uint64_t assets;
    uint64_t expected_assets;
    uint64_t excess_assets = 0;

    if (account == NULL) {
        return fail(ledger, HLX_ERR_NOT_FOUND, "account %u not found", account_id);
    }
    if (lock == NULL || lock->closed) {
        return fail(ledger, HLX_ERR_NOT_FOUND, "lock %u not found", lock_id);
    }
    if (lock->owner_id != account_id) {
        return fail(ledger, HLX_ERR_STATE, "account %u does not own lock %u", account_id, lock_id);
    }
    if (shares == 0 || lock->shares < shares) {
        return fail(ledger, HLX_ERR_SHARES, "lock %u has insufficient shares for redemption", lock_id);
    }
    vault = find_vault(ledger, lock->vault_id);
    if (vault == NULL) {
        return fail(ledger, HLX_ERR_NOT_FOUND, "vault %u not found", lock->vault_id);
    }

    redemption = allocate_redemption(ledger);
    if (redemption == NULL) {
        return fail(ledger, HLX_ERR_CAPACITY, "redemption capacity reached");
    }

    assets = hlx_shares_to_assets(shares, lock->redemption_index);
    expected_assets = hlx_shares_to_assets(shares, lock->issued_index);
    if (assets > expected_assets) {
        excess_assets = assets - expected_assets;
    }

    lock->shares -= shares;
    lock->redeemed_shares += shares;
    lock->excess_quote_assets += excess_assets;
    if (lock->shares == 0) {
        lock->closed = true;
    }

    vault->locked_shares -= shares;
    vault->pending_shares += shares;
    vault->pending_assets += assets;
    vault->excess_quoted_assets += excess_assets;

    redemption->owner_id = account_id;
    redemption->vault_id = vault->id;
    redemption->lock_id = lock_id;
    redemption->requested_epoch = vault->epoch;
    redemption->shares = shares;
    redemption->issued_index = lock->issued_index;
    redemption->quote_index = lock->redemption_index;
    redemption->assets = assets;
    redemption->excess_quote_assets = excess_assets;

    if (redemption_id_out != NULL) {
        *redemption_id_out = redemption->id;
    }
    add_event(ledger, "redeem", account_id, vault->id, vault->epoch, assets, excess_assets > 0 ? "redemption quoted above issued index" : "redemption queued");
    return HLX_OK;
}

HlxStatus hlx_revalue_vault(HlxLedger *ledger, uint32_t vault_id, uint64_t new_index) {
    HlxVault *vault = find_vault(ledger, vault_id);
    uint64_t old_live_assets;
    uint64_t new_live_assets;
    uint64_t delta;

    if (vault == NULL) {
        return fail(ledger, HLX_ERR_NOT_FOUND, "vault %u not found", vault_id);
    }
    if (new_index == 0) {
        return fail(ledger, HLX_ERR_INDEX, "new index cannot be zero");
    }

    old_live_assets = hlx_shares_to_assets(vault->live_shares, vault->share_index);
    new_live_assets = hlx_shares_to_assets(vault->live_shares, new_index);
    vault->previous_index = vault->share_index;
    vault->share_index = new_index;
    vault->epoch++;

    if (new_live_assets >= old_live_assets) {
        delta = new_live_assets - old_live_assets;
        vault->reserves += delta;
        vault->accrued_assets += delta;
    } else {
        delta = old_live_assets - new_live_assets;
        if (delta > vault->reserves) {
            vault->deficit_assets += delta - vault->reserves;
            vault->reserves = 0;
            vault->insolvent = true;
        } else {
            vault->reserves -= delta;
        }
    }
    add_event(ledger, "revalue", 0, vault->id, vault->epoch, new_index, "vault share index changed");
    return HLX_OK;
}

HlxStatus hlx_advance_epoch(HlxLedger *ledger, uint32_t vault_id) {
    HlxVault *vault = find_vault(ledger, vault_id);
    if (vault == NULL) {
        return fail(ledger, HLX_ERR_NOT_FOUND, "vault %u not found", vault_id);
    }
    vault->previous_index = vault->share_index;
    vault->epoch++;
    add_event(ledger, "advance", 0, vault->id, vault->epoch, vault->share_index, "epoch advanced without index change");
    return HLX_OK;
}

HlxStatus hlx_settle_epoch(HlxLedger *ledger, uint32_t vault_id) {
    HlxVault *vault = find_vault(ledger, vault_id);
    uint64_t total_assets = 0;
    uint64_t total_shares = 0;
    uint32_t settled_count = 0;

    if (vault == NULL) {
        return fail(ledger, HLX_ERR_NOT_FOUND, "vault %u not found", vault_id);
    }

    for (size_t i = 0; i < HLX_MAX_REDEMPTIONS; ++i) {
        HlxRedemption *redemption = &ledger->redemptions[i];
        if (!redemption->active || redemption->settled || redemption->vault_id != vault_id) {
            continue;
        }
        if (redemption->requested_epoch > vault->epoch) {
            continue;
        }
        redemption->settled = true;
        redemption->settled_epoch = vault->epoch;
        total_assets += redemption->assets;
        total_shares += redemption->shares;
        settled_count++;
    }

    if (settled_count == 0) {
        add_event(ledger, "settle", 0, vault->id, vault->epoch, 0, "no redemptions pending");
        return HLX_OK;
    }

    if (total_assets > vault->pending_assets) {
        vault->pending_assets = 0;
    } else {
        vault->pending_assets -= total_assets;
    }
    if (total_shares > vault->pending_shares) {
        vault->pending_shares = 0;
    } else {
        vault->pending_shares -= total_shares;
    }
    vault->retired_shares += total_shares;
    vault->settled_assets += total_assets;

    if (total_assets > vault->reserves) {
        vault->deficit_assets += total_assets - vault->reserves;
        vault->reserves = 0;
        vault->insolvent = true;
    } else {
        vault->reserves -= total_assets;
    }
    add_event(ledger, "settle", 0, vault->id, vault->epoch, total_assets, "pending redemptions settled for epoch");
    return HLX_OK;
}

HlxStatus hlx_withdraw_redemption(HlxLedger *ledger, uint32_t account_id, uint32_t redemption_id) {
    HlxAccount *account = find_account(ledger, account_id);
    HlxRedemption *redemption = find_redemption(ledger, redemption_id);
    HlxVault *vault;

    if (account == NULL) {
        return fail(ledger, HLX_ERR_NOT_FOUND, "account %u not found", account_id);
    }
    if (redemption == NULL) {
        return fail(ledger, HLX_ERR_NOT_FOUND, "redemption %u not found", redemption_id);
    }
    if (redemption->owner_id != account_id) {
        return fail(ledger, HLX_ERR_STATE, "account %u does not own redemption %u", account_id, redemption_id);
    }
    if (!redemption->settled) {
        return fail(ledger, HLX_ERR_STATE, "redemption %u is not settled", redemption_id);
    }
    if (redemption->withdrawn) {
        return fail(ledger, HLX_ERR_STATE, "redemption %u already withdrawn", redemption_id);
    }
    vault = find_vault(ledger, redemption->vault_id);
    if (vault == NULL) {
        return fail(ledger, HLX_ERR_NOT_FOUND, "vault %u not found", redemption->vault_id);
    }

    redemption->withdrawn = true;
    account->external_assets += redemption->assets;
    account->withdrawn_assets += redemption->assets;
    add_event(ledger, "withdraw", account_id, redemption->vault_id, vault->epoch, redemption->assets, "settled redemption withdrawn");
    return HLX_OK;
}

uint64_t hlx_total_external_assets(const HlxLedger *ledger) {
    uint64_t total = 0;
    for (size_t i = 0; i < HLX_MAX_ACCOUNTS; ++i) {
        if (ledger->accounts[i].active) {
            total += ledger->accounts[i].external_assets;
        }
    }
    return total;
}

uint64_t hlx_total_vault_reserves(const HlxLedger *ledger) {
    uint64_t total = 0;
    for (size_t i = 0; i < HLX_MAX_VAULTS; ++i) {
        if (ledger->vaults[i].active) {
            total += ledger->vaults[i].reserves;
        }
    }
    return total;
}

uint64_t hlx_total_deficit_assets(const HlxLedger *ledger) {
    uint64_t total = 0;
    for (size_t i = 0; i < HLX_MAX_VAULTS; ++i) {
        if (ledger->vaults[i].active) {
            total += ledger->vaults[i].deficit_assets;
        }
    }
    return total;
}

uint64_t hlx_total_excess_quote_assets(const HlxLedger *ledger) {
    uint64_t total = 0;
    for (size_t i = 0; i < HLX_MAX_VAULTS; ++i) {
        if (ledger->vaults[i].active) {
            total += ledger->vaults[i].excess_quoted_assets;
        }
    }
    return total;
}

uint32_t hlx_count_insolvent_vaults(const HlxLedger *ledger) {
    uint32_t total = 0;
    for (size_t i = 0; i < HLX_MAX_VAULTS; ++i) {
        if (ledger->vaults[i].active && ledger->vaults[i].insolvent) {
            total++;
        }
    }
    return total;
}

uint32_t hlx_count_active_accounts(const HlxLedger *ledger) {
    uint32_t total = 0;
    for (size_t i = 0; i < HLX_MAX_ACCOUNTS; ++i) {
        if (ledger->accounts[i].active) {
            total++;
        }
    }
    return total;
}

uint32_t hlx_count_active_vaults(const HlxLedger *ledger) {
    uint32_t total = 0;
    for (size_t i = 0; i < HLX_MAX_VAULTS; ++i) {
        if (ledger->vaults[i].active) {
            total++;
        }
    }
    return total;
}

static uint64_t fnv_update_u64(uint64_t hash, uint64_t value) {
    for (size_t i = 0; i < 8; ++i) {
        hash ^= (value >> (i * 8)) & 0xffU;
        hash *= 1099511628211ULL;
    }
    return hash;
}

static uint64_t fnv_update_text(uint64_t hash, const char *text) {
    if (text == NULL) {
        return hash;
    }
    while (*text != '\0') {
        hash ^= (unsigned char)*text++;
        hash *= 1099511628211ULL;
    }
    return hash;
}

void hlx_state_digest(const HlxLedger *ledger, char out[HLX_DIGEST_SIZE]) {
    uint64_t hash = 1469598103934665603ULL;
    hash = fnv_update_text(hash, ledger->network_id);
    for (size_t i = 0; i < HLX_MAX_ACCOUNTS; ++i) {
        const HlxAccount *account = &ledger->accounts[i];
        if (!account->active) {
            continue;
        }
        hash = fnv_update_u64(hash, account->id);
        hash = fnv_update_text(hash, account->label);
        hash = fnv_update_u64(hash, account->external_assets);
        hash = fnv_update_u64(hash, account->withdrawn_assets);
    }
    for (size_t i = 0; i < HLX_MAX_VAULTS; ++i) {
        const HlxVault *vault = &ledger->vaults[i];
        if (!vault->active) {
            continue;
        }
        hash = fnv_update_u64(hash, vault->id);
        hash = fnv_update_u64(hash, vault->epoch);
        hash = fnv_update_text(hash, vault->symbol);
        hash = fnv_update_u64(hash, vault->share_index);
        hash = fnv_update_u64(hash, vault->reserves);
        hash = fnv_update_u64(hash, vault->live_shares);
        hash = fnv_update_u64(hash, vault->locked_shares);
        hash = fnv_update_u64(hash, vault->pending_assets);
        hash = fnv_update_u64(hash, vault->deficit_assets);
    }
    for (size_t i = 0; i < HLX_MAX_LOCKS; ++i) {
        const HlxLock *lock = &ledger->locks[i];
        if (!lock->active) {
            continue;
        }
        hash = fnv_update_u64(hash, lock->id);
        hash = fnv_update_u64(hash, lock->owner_id);
        hash = fnv_update_u64(hash, lock->vault_id);
        hash = fnv_update_u64(hash, lock->shares);
        hash = fnv_update_u64(hash, lock->issued_index);
        hash = fnv_update_u64(hash, lock->redemption_index);
        hash = fnv_update_u64(hash, lock->flags);
    }
    for (size_t i = 0; i < HLX_MAX_REDEMPTIONS; ++i) {
        const HlxRedemption *redemption = &ledger->redemptions[i];
        if (!redemption->active) {
            continue;
        }
        hash = fnv_update_u64(hash, redemption->id);
        hash = fnv_update_u64(hash, redemption->owner_id);
        hash = fnv_update_u64(hash, redemption->vault_id);
        hash = fnv_update_u64(hash, redemption->assets);
        hash = fnv_update_u64(hash, redemption->settled);
        hash = fnv_update_u64(hash, redemption->withdrawn);
    }
    for (size_t i = 0; i < HLX_MAX_QUOTES; ++i) {
        const HlxQuote *quote = &ledger->quotes[i];
        if (!quote->active) {
            continue;
        }
        hash = fnv_update_u64(hash, quote->id);
        hash = fnv_update_text(hash, quote->kind);
        hash = fnv_update_u64(hash, quote->account_id);
        hash = fnv_update_u64(hash, quote->vault_id);
        hash = fnv_update_u64(hash, quote->lock_id);
        hash = fnv_update_u64(hash, quote->shares);
        hash = fnv_update_u64(hash, quote->assets);
        hash = fnv_update_u64(hash, quote->expected_assets);
        hash = fnv_update_u64(hash, quote->accepted);
        hash = fnv_update_u64(hash, quote->over_issued_quote);
    }
    snprintf(out, HLX_DIGEST_SIZE, "%016llx", (unsigned long long)hash);
}
