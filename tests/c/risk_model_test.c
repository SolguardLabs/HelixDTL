#include "helix_risk.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static HlxRiskInput base_input(void) {
    HlxRiskInput input = {
        .reserves = 2500,
        .live_shares = 1500,
        .share_index = 1000000,
        .locked_claim_assets = 300,
        .pending_assets = 200,
        .largest_account_shares = 600,
        .backstop_assets = 500,
    };
    return input;
}

static HlxRiskPolicy base_policy(void) {
    HlxRiskPolicy policy = {
        .slash_shock_bps = 1000,
        .liquidity_haircut_bps = 500,
        .redemption_run_bps = 2500,
        .backstop_recovery_bps = 5000,
        .reserve_floor_bps = 1000,
        .minimum_queue_coverage_bps = 10000,
        .concentration_limit_bps = 5000,
    };
    return policy;
}

static void test_normal_projection(void) {
    HlxRiskInput input = base_input();
    HlxRiskPolicy policy = base_policy();
    HlxRiskProjection result;

    assert(hlx_risk_project(&input, &policy, &result) == HLX_RISK_OK);
    assert(result.live_liability_assets == 1500);
    assert(result.total_liability_assets == 2000);
    assert(result.redemption_run_assets == 375);
    assert(result.queue_liability_assets == 575);
    assert(result.slash_loss_assets == 250);
    assert(result.liquidity_loss_assets == 112);
    assert(result.recovered_backstop_assets == 250);
    assert(result.effective_reserves == 2388);
    assert(result.residual_buffer_assets == 388);
    assert(result.required_reserve_assets == 200);
    assert(result.system_coverage_bps == 10000);
    assert(result.queue_coverage_bps == 10000);
    assert(result.concentration_bps == 4000);
    assert(result.band == HLX_RISK_NORMAL);
}

static void test_restricted_capital_state(void) {
    HlxRiskInput input = base_input();
    HlxRiskPolicy policy = base_policy();
    HlxRiskProjection result;

    policy.slash_shock_bps = 6000;
    assert(hlx_risk_project(&input, &policy, &result) == HLX_RISK_OK);
    assert(result.effective_reserves == 1200);
    assert(result.capital_shortfall_assets == 800);
    assert(result.queue_coverage_bps == 10000);
    assert(result.band == HLX_RISK_RESTRICTED);
}

static void test_critical_queue_state(void) {
    HlxRiskInput input = base_input();
    HlxRiskPolicy policy = base_policy();
    HlxRiskProjection result;

    policy.slash_shock_bps = 6000;
    policy.redemption_run_bps = 10000;
    assert(hlx_risk_project(&input, &policy, &result) == HLX_RISK_OK);
    assert(result.queue_liability_assets == 1700);
    assert(result.queue_coverage_bps == 7058);
    assert(result.band == HLX_RISK_CRITICAL);
}

static void test_watch_reserve_floor(void) {
    HlxRiskInput input = base_input();
    HlxRiskPolicy policy = base_policy();
    HlxRiskProjection result;

    policy.slash_shock_bps = 0;
    policy.liquidity_haircut_bps = 0;
    policy.reserve_floor_bps = 5000;
    assert(hlx_risk_project(&input, &policy, &result) == HLX_RISK_OK);
    assert(result.effective_reserves == 2750);
    assert(result.residual_buffer_assets == 750);
    assert(result.required_reserve_assets == 1000);
    assert(result.band == HLX_RISK_WATCH);
}

static void test_concentration_restriction(void) {
    HlxRiskInput input = base_input();
    HlxRiskPolicy policy = base_policy();
    HlxRiskProjection result;

    input.largest_account_shares = 900;
    assert(hlx_risk_project(&input, &policy, &result) == HLX_RISK_OK);
    assert(result.concentration_bps == 6000);
    assert(result.band == HLX_RISK_RESTRICTED);
}

static void test_invalid_inputs_fail_closed(void) {
    HlxRiskInput input = base_input();
    HlxRiskPolicy policy = base_policy();
    HlxRiskProjection result;

    policy.redemption_run_bps = 10001;
    assert(hlx_risk_project(&input, &policy, &result) == HLX_RISK_ERR_POLICY);
    policy = base_policy();
    input.largest_account_shares = input.live_shares + 1;
    assert(hlx_risk_project(&input, &policy, &result) == HLX_RISK_ERR_ARGUMENT);
    input = base_input();
    input.live_shares = UINT64_MAX;
    input.share_index = 2000000;
    input.largest_account_shares = 0;
    assert(hlx_risk_project(&input, &policy, &result) == HLX_RISK_ERR_ARITHMETIC);
}

int main(void) {
    test_normal_projection();
    test_restricted_capital_state();
    test_critical_queue_state();
    test_watch_reserve_floor();
    test_concentration_restriction();
    test_invalid_inputs_fail_closed();
    puts("Helix risk model: 6 checks passed");
    return 0;
}
