#ifndef HELIX_H
#define HELIX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define HLX_INDEX_SCALE 1000000ULL
#define HLX_MAX_ACCOUNTS 24
#define HLX_MAX_VAULTS 12
#define HLX_MAX_POSITIONS 64
#define HLX_MAX_LOCKS 96
#define HLX_MAX_REDEMPTIONS 96
#define HLX_MAX_QUOTES 64
#define HLX_MAX_EVENTS 192
#define HLX_LABEL_SIZE 32
#define HLX_SYMBOL_SIZE 16
#define HLX_ERROR_SIZE 192
#define HLX_DIGEST_SIZE 17

typedef enum HlxStatus {
    HLX_OK = 0,
    HLX_ERR_NOT_FOUND,
    HLX_ERR_EXISTS,
    HLX_ERR_CAPACITY,
    HLX_ERR_BALANCE,
    HLX_ERR_SHARES,
    HLX_ERR_INDEX,
    HLX_ERR_STATE,
    HLX_ERR_PARSE,
    HLX_ERR_IO
} HlxStatus;

typedef enum HlxLockFlags {
    HLX_LOCK_FLAG_NONE = 0,
    HLX_LOCK_FLAG_INDEX_REFRESH = 1 << 0,
    HLX_LOCK_FLAG_PARTIAL_SOURCE = 1 << 1
} HlxLockFlags;

typedef struct HlxAccount {
    bool active;
    uint32_t id;
    char label[HLX_LABEL_SIZE];
    uint64_t external_assets;
    uint64_t withdrawn_assets;
} HlxAccount;

typedef struct HlxVault {
    bool active;
    bool insolvent;
    uint32_t id;
    uint32_t epoch;
    char symbol[HLX_SYMBOL_SIZE];
    uint64_t share_index;
    uint64_t previous_index;
    uint64_t reserves;
    uint64_t live_shares;
    uint64_t locked_shares;
    uint64_t pending_shares;
    uint64_t retired_shares;
    uint64_t pending_assets;
    uint64_t settled_assets;
    uint64_t accrued_assets;
    uint64_t deficit_assets;
    uint64_t excess_quoted_assets;
} HlxVault;

typedef struct HlxPosition {
    bool active;
    uint32_t id;
    uint32_t account_id;
    uint32_t vault_id;
    uint64_t liquid_shares;
    uint64_t issue_index;
    uint32_t last_epoch;
} HlxPosition;

typedef struct HlxLock {
    bool active;
    bool closed;
    uint32_t id;
    uint32_t owner_id;
    uint32_t origin_account_id;
    uint32_t vault_id;
    uint32_t origin_position_id;
    uint32_t opened_epoch;
    uint32_t transfer_depth;
    uint32_t parent_lock_id;
    uint32_t flags;
    uint64_t shares;
    uint64_t issued_index;
    uint64_t redemption_index;
    uint64_t redeemed_shares;
    uint64_t excess_quote_assets;
} HlxLock;

typedef struct HlxRedemption {
    bool active;
    bool settled;
    bool withdrawn;
    uint32_t id;
    uint32_t owner_id;
    uint32_t vault_id;
    uint32_t lock_id;
    uint32_t requested_epoch;
    uint32_t settled_epoch;
    uint64_t shares;
    uint64_t issued_index;
    uint64_t quote_index;
    uint64_t assets;
    uint64_t excess_quote_assets;
} HlxRedemption;

typedef struct HlxQuote {
    bool active;
    bool accepted;
    bool over_issued_quote;
    uint32_t id;
    uint32_t account_id;
    uint32_t vault_id;
    uint32_t lock_id;
    uint32_t epoch;
    uint64_t input_amount;
    uint64_t shares;
    uint64_t assets;
    uint64_t expected_assets;
    uint64_t index;
    uint64_t issued_index;
    char kind[24];
    char note[72];
} HlxQuote;

typedef struct HlxEvent {
    bool active;
    uint32_t serial;
    uint32_t epoch;
    uint32_t account_id;
    uint32_t vault_id;
    uint64_t amount;
    char kind[24];
    char note[72];
} HlxEvent;

typedef struct HlxLedger {
    char network_id[HLX_LABEL_SIZE];
    HlxAccount accounts[HLX_MAX_ACCOUNTS];
    HlxVault vaults[HLX_MAX_VAULTS];
    HlxPosition positions[HLX_MAX_POSITIONS];
    HlxLock locks[HLX_MAX_LOCKS];
    HlxRedemption redemptions[HLX_MAX_REDEMPTIONS];
    HlxQuote quotes[HLX_MAX_QUOTES];
    HlxEvent events[HLX_MAX_EVENTS];
    uint32_t next_position_id;
    uint32_t next_lock_id;
    uint32_t next_redemption_id;
    uint32_t next_quote_id;
    uint32_t next_event_serial;
    char last_error[HLX_ERROR_SIZE];
} HlxLedger;

typedef struct HlxInvariantReport {
    uint64_t account_assets;
    uint64_t vault_reserves;
    uint64_t deficit_assets;
    uint64_t excess_quote_assets;
    uint64_t live_share_assets;
    uint64_t locked_issue_assets;
    uint64_t locked_redemption_assets;
    uint64_t pending_assets;
    uint64_t settled_assets;
    uint64_t claim_assets;
    uint64_t claim_gap_assets;
    uint64_t undercollateralized_assets;
    uint32_t active_accounts;
    uint32_t active_vaults;
    uint32_t open_locks;
    uint32_t closed_locks;
    uint32_t refreshed_locks;
    uint32_t pending_redemptions;
    uint32_t settled_redemptions;
    uint32_t withdrawn_redemptions;
    bool solvent;
    bool quotes_ok;
    bool pending_ok;
} HlxInvariantReport;

const char *hlx_status_message(HlxStatus status);

uint64_t hlx_assets_to_shares(uint64_t assets, uint64_t index);
uint64_t hlx_shares_to_assets(uint64_t shares, uint64_t index);
uint64_t hlx_parse_index_literal(const char *text, bool *ok);
void hlx_format_index(uint64_t index, char *out, size_t out_size);

void hlx_ledger_init(HlxLedger *ledger, const char *network_id);
HlxStatus hlx_add_account(HlxLedger *ledger, uint32_t id, const char *label, uint64_t assets);
HlxStatus hlx_add_vault(HlxLedger *ledger, uint32_t id, const char *symbol, uint64_t index);
HlxStatus hlx_deposit(HlxLedger *ledger, uint32_t account_id, uint32_t vault_id, uint64_t assets, uint64_t *shares_out);
HlxStatus hlx_lock_shares(HlxLedger *ledger, uint32_t account_id, uint32_t vault_id, uint64_t shares, uint32_t *lock_id_out);
HlxStatus hlx_transfer_locked(HlxLedger *ledger, uint32_t from_account_id, uint32_t to_account_id, uint32_t lock_id, uint64_t shares, uint32_t *child_lock_id_out);
HlxStatus hlx_request_redemption(HlxLedger *ledger, uint32_t account_id, uint32_t lock_id, uint64_t shares, uint32_t *redemption_id_out);
HlxStatus hlx_revalue_vault(HlxLedger *ledger, uint32_t vault_id, uint64_t new_index);
HlxStatus hlx_advance_epoch(HlxLedger *ledger, uint32_t vault_id);
HlxStatus hlx_settle_epoch(HlxLedger *ledger, uint32_t vault_id);
HlxStatus hlx_withdraw_redemption(HlxLedger *ledger, uint32_t account_id, uint32_t redemption_id);
HlxStatus hlx_quote_deposit(HlxLedger *ledger, uint32_t account_id, uint32_t vault_id, uint64_t assets, uint32_t *quote_id_out);
HlxStatus hlx_quote_redemption(HlxLedger *ledger, uint32_t account_id, uint32_t lock_id, uint64_t shares, uint32_t *quote_id_out);

uint64_t hlx_total_external_assets(const HlxLedger *ledger);
uint64_t hlx_total_vault_reserves(const HlxLedger *ledger);
uint64_t hlx_total_deficit_assets(const HlxLedger *ledger);
uint64_t hlx_total_excess_quote_assets(const HlxLedger *ledger);
uint32_t hlx_count_insolvent_vaults(const HlxLedger *ledger);
uint32_t hlx_count_active_accounts(const HlxLedger *ledger);
uint32_t hlx_count_active_vaults(const HlxLedger *ledger);
void hlx_invariant_report(const HlxLedger *ledger, HlxInvariantReport *report);

const HlxAccount *hlx_get_account(const HlxLedger *ledger, uint32_t id);
const HlxVault *hlx_get_vault(const HlxLedger *ledger, uint32_t id);
const HlxPosition *hlx_get_position(const HlxLedger *ledger, uint32_t id);
const HlxLock *hlx_get_lock(const HlxLedger *ledger, uint32_t id);
const HlxRedemption *hlx_get_redemption(const HlxLedger *ledger, uint32_t id);
const HlxQuote *hlx_get_quote(const HlxLedger *ledger, uint32_t id);
uint64_t hlx_account_liquid_shares(const HlxLedger *ledger, uint32_t account_id, uint32_t vault_id);
uint64_t hlx_account_locked_shares(const HlxLedger *ledger, uint32_t account_id, uint32_t vault_id);
uint64_t hlx_account_pending_assets(const HlxLedger *ledger, uint32_t account_id, uint32_t vault_id);
uint64_t hlx_account_withdrawable_assets(const HlxLedger *ledger, uint32_t account_id, uint32_t vault_id);
uint64_t hlx_vault_open_lock_shares(const HlxLedger *ledger, uint32_t vault_id);
uint64_t hlx_vault_pending_redemption_assets(const HlxLedger *ledger, uint32_t vault_id);
uint32_t hlx_account_open_lock_count(const HlxLedger *ledger, uint32_t account_id);
uint32_t hlx_vault_pending_redemption_count(const HlxLedger *ledger, uint32_t vault_id);
bool hlx_lock_has_index_drift(const HlxLedger *ledger, uint32_t lock_id);

void hlx_state_digest(const HlxLedger *ledger, char out[HLX_DIGEST_SIZE]);
void hlx_write_snapshot_json(const HlxLedger *ledger, const char *scenario, FILE *out);
void hlx_write_error_json(FILE *out, HlxStatus status, const char *detail, size_t line_no);

HlxStatus hlx_run_scenario(const char *name, FILE *out);
HlxStatus hlx_run_script_path(const char *path, FILE *out);
void hlx_print_scenarios(FILE *out);

#endif
