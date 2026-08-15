#ifndef HELIX_RISK_H
#define HELIX_RISK_H

#include <stdint.h>

#define HLX_RISK_BPS_SCALE 10000U

typedef enum HlxRiskStatus {
    HLX_RISK_OK = 0,
    HLX_RISK_ERR_ARGUMENT,
    HLX_RISK_ERR_POLICY,
    HLX_RISK_ERR_ARITHMETIC
} HlxRiskStatus;

typedef enum HlxRiskBand {
    HLX_RISK_NORMAL = 0,
    HLX_RISK_WATCH,
    HLX_RISK_RESTRICTED,
    HLX_RISK_CRITICAL
} HlxRiskBand;

typedef struct HlxRiskInput {
    uint64_t reserves;
    uint64_t live_shares;
    uint64_t share_index;
    uint64_t locked_claim_assets;
    uint64_t pending_assets;
    uint64_t largest_account_shares;
    uint64_t backstop_assets;
} HlxRiskInput;

typedef struct HlxRiskPolicy {
    uint32_t slash_shock_bps;
    uint32_t liquidity_haircut_bps;
    uint32_t redemption_run_bps;
    uint32_t backstop_recovery_bps;
    uint32_t reserve_floor_bps;
    uint32_t minimum_queue_coverage_bps;
    uint32_t concentration_limit_bps;
} HlxRiskPolicy;

typedef struct HlxRiskProjection {
    uint64_t live_liability_assets;
    uint64_t locked_liability_assets;
    uint64_t pending_liability_assets;
    uint64_t total_liability_assets;
    uint64_t redemption_run_assets;
    uint64_t queue_liability_assets;
    uint64_t slash_loss_assets;
    uint64_t liquidity_loss_assets;
    uint64_t recovered_backstop_assets;
    uint64_t effective_reserves;
    uint64_t required_reserve_assets;
    uint64_t capital_shortfall_assets;
    uint64_t residual_buffer_assets;
    uint32_t system_coverage_bps;
    uint32_t queue_coverage_bps;
    uint32_t concentration_bps;
    HlxRiskBand band;
} HlxRiskProjection;

HlxRiskStatus hlx_risk_project(
    const HlxRiskInput *input,
    const HlxRiskPolicy *policy,
    HlxRiskProjection *projection
);
const char *hlx_risk_status_message(HlxRiskStatus status);
const char *hlx_risk_band_name(HlxRiskBand band);

#endif
