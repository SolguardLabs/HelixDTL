#include "helix.h"

const HlxAccount *hlx_get_account(const HlxLedger *ledger, uint32_t id) {
    for (size_t i = 0; i < HLX_MAX_ACCOUNTS; ++i) {
        const HlxAccount *account = &ledger->accounts[i];
        if (account->active && account->id == id) {
            return account;
        }
    }
    return NULL;
}

const HlxVault *hlx_get_vault(const HlxLedger *ledger, uint32_t id) {
    for (size_t i = 0; i < HLX_MAX_VAULTS; ++i) {
        const HlxVault *vault = &ledger->vaults[i];
        if (vault->active && vault->id == id) {
            return vault;
        }
    }
    return NULL;
}

const HlxPosition *hlx_get_position(const HlxLedger *ledger, uint32_t id) {
    for (size_t i = 0; i < HLX_MAX_POSITIONS; ++i) {
        const HlxPosition *position = &ledger->positions[i];
        if (position->active && position->id == id) {
            return position;
        }
    }
    return NULL;
}

const HlxLock *hlx_get_lock(const HlxLedger *ledger, uint32_t id) {
    for (size_t i = 0; i < HLX_MAX_LOCKS; ++i) {
        const HlxLock *lock = &ledger->locks[i];
        if (lock->active && lock->id == id) {
            return lock;
        }
    }
    return NULL;
}

const HlxRedemption *hlx_get_redemption(const HlxLedger *ledger, uint32_t id) {
    for (size_t i = 0; i < HLX_MAX_REDEMPTIONS; ++i) {
        const HlxRedemption *redemption = &ledger->redemptions[i];
        if (redemption->active && redemption->id == id) {
            return redemption;
        }
    }
    return NULL;
}

const HlxQuote *hlx_get_quote(const HlxLedger *ledger, uint32_t id) {
    for (size_t i = 0; i < HLX_MAX_QUOTES; ++i) {
        const HlxQuote *quote = &ledger->quotes[i];
        if (quote->active && quote->id == id) {
            return quote;
        }
    }
    return NULL;
}

uint64_t hlx_account_liquid_shares(const HlxLedger *ledger, uint32_t account_id, uint32_t vault_id) {
    uint64_t total = 0;
    for (size_t i = 0; i < HLX_MAX_POSITIONS; ++i) {
        const HlxPosition *position = &ledger->positions[i];
        if (!position->active) {
            continue;
        }
        if (position->account_id == account_id && position->vault_id == vault_id) {
            total += position->liquid_shares;
        }
    }
    return total;
}

uint64_t hlx_account_locked_shares(const HlxLedger *ledger, uint32_t account_id, uint32_t vault_id) {
    uint64_t total = 0;
    for (size_t i = 0; i < HLX_MAX_LOCKS; ++i) {
        const HlxLock *lock = &ledger->locks[i];
        if (!lock->active || lock->closed) {
            continue;
        }
        if (lock->owner_id == account_id && lock->vault_id == vault_id) {
            total += lock->shares;
        }
    }
    return total;
}

uint64_t hlx_account_pending_assets(const HlxLedger *ledger, uint32_t account_id, uint32_t vault_id) {
    uint64_t total = 0;
    for (size_t i = 0; i < HLX_MAX_REDEMPTIONS; ++i) {
        const HlxRedemption *redemption = &ledger->redemptions[i];
        if (!redemption->active || redemption->settled) {
            continue;
        }
        if (redemption->owner_id == account_id && redemption->vault_id == vault_id) {
            total += redemption->assets;
        }
    }
    return total;
}

uint64_t hlx_account_withdrawable_assets(const HlxLedger *ledger, uint32_t account_id, uint32_t vault_id) {
    uint64_t total = 0;
    for (size_t i = 0; i < HLX_MAX_REDEMPTIONS; ++i) {
        const HlxRedemption *redemption = &ledger->redemptions[i];
        if (!redemption->active || !redemption->settled || redemption->withdrawn) {
            continue;
        }
        if (redemption->owner_id == account_id && redemption->vault_id == vault_id) {
            total += redemption->assets;
        }
    }
    return total;
}

uint64_t hlx_vault_open_lock_shares(const HlxLedger *ledger, uint32_t vault_id) {
    uint64_t total = 0;
    for (size_t i = 0; i < HLX_MAX_LOCKS; ++i) {
        const HlxLock *lock = &ledger->locks[i];
        if (!lock->active || lock->closed) {
            continue;
        }
        if (lock->vault_id == vault_id) {
            total += lock->shares;
        }
    }
    return total;
}

uint64_t hlx_vault_pending_redemption_assets(const HlxLedger *ledger, uint32_t vault_id) {
    uint64_t total = 0;
    for (size_t i = 0; i < HLX_MAX_REDEMPTIONS; ++i) {
        const HlxRedemption *redemption = &ledger->redemptions[i];
        if (!redemption->active || redemption->settled) {
            continue;
        }
        if (redemption->vault_id == vault_id) {
            total += redemption->assets;
        }
    }
    return total;
}

uint32_t hlx_account_open_lock_count(const HlxLedger *ledger, uint32_t account_id) {
    uint32_t total = 0;
    for (size_t i = 0; i < HLX_MAX_LOCKS; ++i) {
        const HlxLock *lock = &ledger->locks[i];
        if (!lock->active || lock->closed) {
            continue;
        }
        if (lock->owner_id == account_id) {
            total++;
        }
    }
    return total;
}

uint32_t hlx_vault_pending_redemption_count(const HlxLedger *ledger, uint32_t vault_id) {
    uint32_t total = 0;
    for (size_t i = 0; i < HLX_MAX_REDEMPTIONS; ++i) {
        const HlxRedemption *redemption = &ledger->redemptions[i];
        if (!redemption->active || redemption->settled) {
            continue;
        }
        if (redemption->vault_id == vault_id) {
            total++;
        }
    }
    return total;
}

bool hlx_lock_has_index_drift(const HlxLedger *ledger, uint32_t lock_id) {
    const HlxLock *lock = hlx_get_lock(ledger, lock_id);
    if (lock == NULL) {
        return false;
    }
    if (lock->redemption_index > lock->issued_index) {
        return true;
    }
    return (lock->flags & HLX_LOCK_FLAG_INDEX_REFRESH) != 0;
}
