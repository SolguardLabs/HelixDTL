#include "helix.h"

#include <stdio.h>
#include <string.h>

#define TRY_LEDGER(call)       \
    do {                       \
        HlxStatus _st = (call); \
        if (_st != HLX_OK) {    \
            return _st;         \
        }                       \
    } while (0)

static uint64_t one(void) {
    return HLX_INDEX_SCALE;
}

static uint64_t index_lit(const char *text) {
    bool ok = false;
    uint64_t value = hlx_parse_index_literal(text, &ok);
    return ok ? value : HLX_INDEX_SCALE;
}

static HlxStatus scenario_snapshot(HlxLedger *ledger) {
    hlx_ledger_init(ledger, "helix-snapshot");
    TRY_LEDGER(hlx_add_account(ledger, 1, "alice", 1000));
    TRY_LEDGER(hlx_add_account(ledger, 2, "bob", 1000));
    TRY_LEDGER(hlx_add_vault(ledger, 7, "hxSOL", one()));
    TRY_LEDGER(hlx_add_vault(ledger, 8, "hxUSD", index_lit("2.000000")));
    return HLX_OK;
}

static HlxStatus scenario_basic(HlxLedger *ledger) {
    uint64_t minted = 0;
    uint32_t lock_id = 0;
    uint32_t redemption_id = 0;

    hlx_ledger_init(ledger, "helix-basic");
    TRY_LEDGER(hlx_add_account(ledger, 1, "alice", 2000));
    TRY_LEDGER(hlx_add_account(ledger, 2, "bob", 500));
    TRY_LEDGER(hlx_add_vault(ledger, 7, "hxSOL", one()));
    TRY_LEDGER(hlx_deposit(ledger, 1, 7, 1000, &minted));
    TRY_LEDGER(hlx_lock_shares(ledger, 1, 7, 300, &lock_id));
    TRY_LEDGER(hlx_request_redemption(ledger, 1, lock_id, 120, &redemption_id));
    TRY_LEDGER(hlx_settle_epoch(ledger, 7));
    TRY_LEDGER(hlx_withdraw_redemption(ledger, 1, redemption_id));
    return HLX_OK;
}

static HlxStatus scenario_multi(HlxLedger *ledger) {
    uint64_t minted = 0;
    uint32_t alice_lock = 0;
    uint32_t bob_lock = 0;
    uint32_t carol_lock = 0;
    uint32_t r1 = 0;
    uint32_t r2 = 0;

    hlx_ledger_init(ledger, "helix-multi");
    TRY_LEDGER(hlx_add_account(ledger, 1, "alice", 3000));
    TRY_LEDGER(hlx_add_account(ledger, 2, "bob", 2500));
    TRY_LEDGER(hlx_add_account(ledger, 3, "carol", 1500));
    TRY_LEDGER(hlx_add_vault(ledger, 7, "hxSOL", one()));
    TRY_LEDGER(hlx_add_vault(ledger, 8, "hxUSD", index_lit("2.000000")));

    TRY_LEDGER(hlx_deposit(ledger, 1, 7, 1000, &minted));
    TRY_LEDGER(hlx_deposit(ledger, 2, 8, 1200, &minted));
    TRY_LEDGER(hlx_revalue_vault(ledger, 8, index_lit("2.100000")));
    TRY_LEDGER(hlx_deposit(ledger, 3, 7, 500, &minted));
    TRY_LEDGER(hlx_lock_shares(ledger, 1, 7, 250, &alice_lock));
    TRY_LEDGER(hlx_lock_shares(ledger, 2, 8, 200, &bob_lock));
    TRY_LEDGER(hlx_transfer_locked(ledger, 2, 3, bob_lock, 80, &carol_lock));
    TRY_LEDGER(hlx_request_redemption(ledger, 1, alice_lock, 100, &r1));
    TRY_LEDGER(hlx_request_redemption(ledger, 3, carol_lock, 40, &r2));
    TRY_LEDGER(hlx_settle_epoch(ledger, 7));
    TRY_LEDGER(hlx_settle_epoch(ledger, 8));
    TRY_LEDGER(hlx_withdraw_redemption(ledger, 1, r1));
    TRY_LEDGER(hlx_withdraw_redemption(ledger, 3, r2));
    return HLX_OK;
}

static HlxStatus scenario_index(HlxLedger *ledger) {
    uint64_t minted = 0;
    uint32_t lock_id = 0;
    uint32_t redemption_id = 0;

    hlx_ledger_init(ledger, "helix-index");
    TRY_LEDGER(hlx_add_account(ledger, 1, "alice", 4000));
    TRY_LEDGER(hlx_add_account(ledger, 2, "bob", 1000));
    TRY_LEDGER(hlx_add_vault(ledger, 7, "hxSOL", one()));
    TRY_LEDGER(hlx_deposit(ledger, 1, 7, 2000, &minted));
    TRY_LEDGER(hlx_revalue_vault(ledger, 7, index_lit("1.250000")));
    TRY_LEDGER(hlx_deposit(ledger, 2, 7, 500, &minted));
    TRY_LEDGER(hlx_lock_shares(ledger, 1, 7, 300, &lock_id));
    TRY_LEDGER(hlx_request_redemption(ledger, 1, lock_id, 100, &redemption_id));
    TRY_LEDGER(hlx_settle_epoch(ledger, 7));
    TRY_LEDGER(hlx_withdraw_redemption(ledger, 1, redemption_id));
    TRY_LEDGER(hlx_revalue_vault(ledger, 7, index_lit("1.400000")));
    return HLX_OK;
}

static HlxStatus scenario_partial(HlxLedger *ledger) {
    uint64_t minted = 0;
    uint32_t lock_id = 0;
    uint32_t first = 0;
    uint32_t second = 0;

    hlx_ledger_init(ledger, "helix-partial");
    TRY_LEDGER(hlx_add_account(ledger, 1, "alice", 1800));
    TRY_LEDGER(hlx_add_account(ledger, 2, "bob", 700));
    TRY_LEDGER(hlx_add_vault(ledger, 7, "hxSOL", one()));
    TRY_LEDGER(hlx_deposit(ledger, 1, 7, 1200, &minted));
    TRY_LEDGER(hlx_lock_shares(ledger, 1, 7, 900, &lock_id));
    TRY_LEDGER(hlx_request_redemption(ledger, 1, lock_id, 300, &first));
    TRY_LEDGER(hlx_settle_epoch(ledger, 7));
    TRY_LEDGER(hlx_withdraw_redemption(ledger, 1, first));
    TRY_LEDGER(hlx_advance_epoch(ledger, 7));
    TRY_LEDGER(hlx_request_redemption(ledger, 1, lock_id, 200, &second));
    TRY_LEDGER(hlx_settle_epoch(ledger, 7));
    TRY_LEDGER(hlx_withdraw_redemption(ledger, 1, second));
    return HLX_OK;
}

static HlxStatus scenario_transfer(HlxLedger *ledger) {
    uint64_t minted = 0;
    uint32_t source_lock = 0;
    uint32_t bob_lock = 0;
    uint32_t redemption_id = 0;

    hlx_ledger_init(ledger, "helix-transfer");
    TRY_LEDGER(hlx_add_account(ledger, 1, "alice", 1600));
    TRY_LEDGER(hlx_add_account(ledger, 2, "bob", 900));
    TRY_LEDGER(hlx_add_vault(ledger, 7, "hxSOL", one()));
    TRY_LEDGER(hlx_deposit(ledger, 1, 7, 1000, &minted));
    TRY_LEDGER(hlx_lock_shares(ledger, 1, 7, 400, &source_lock));
    TRY_LEDGER(hlx_transfer_locked(ledger, 1, 2, source_lock, 150, &bob_lock));
    TRY_LEDGER(hlx_advance_epoch(ledger, 7));
    TRY_LEDGER(hlx_request_redemption(ledger, 2, bob_lock, 150, &redemption_id));
    TRY_LEDGER(hlx_settle_epoch(ledger, 7));
    TRY_LEDGER(hlx_withdraw_redemption(ledger, 2, redemption_id));
    return HLX_OK;
}

typedef HlxStatus (*ScenarioFn)(HlxLedger *ledger);

typedef struct ScenarioEntry {
    const char *name;
    ScenarioFn run;
} ScenarioEntry;

static const ScenarioEntry SCENARIOS[] = {
    {"basic", scenario_basic},
    {"multi", scenario_multi},
    {"index", scenario_index},
    {"partial", scenario_partial},
    {"transfer", scenario_transfer},
    {"snapshot", scenario_snapshot},
};

HlxStatus hlx_run_scenario(const char *name, FILE *out) {
    HlxLedger ledger;
    const char *selected = (name == NULL || *name == '\0') ? "basic" : name;

    for (size_t i = 0; i < sizeof(SCENARIOS) / sizeof(SCENARIOS[0]); ++i) {
        if (strcmp(selected, SCENARIOS[i].name) == 0) {
            HlxStatus status = SCENARIOS[i].run(&ledger);
            if (status != HLX_OK) {
                hlx_write_error_json(out, status, ledger.last_error, 0);
                return status;
            }
            hlx_write_snapshot_json(&ledger, selected, out);
            return HLX_OK;
        }
    }

    hlx_write_error_json(out, HLX_ERR_NOT_FOUND, "unknown scenario", 0);
    return HLX_ERR_NOT_FOUND;
}

void hlx_print_scenarios(FILE *out) {
    for (size_t i = 0; i < sizeof(SCENARIOS) / sizeof(SCENARIOS[0]); ++i) {
        fprintf(out, "%s\n", SCENARIOS[i].name);
    }
}
