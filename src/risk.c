#include "helix_risk.h"

#include "helix.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static bool checked_add(uint64_t left, uint64_t right, uint64_t *out) {
    if (UINT64_MAX - left < right) {
        return false;
    }
    *out = left + right;
    return true;
}

static bool shares_to_assets(uint64_t shares, uint64_t index, uint64_t *out) {
    if (index == 0 || (shares != 0 && index > UINT64_MAX / shares)) {
        return false;
    }
    *out = (shares * index) / HLX_INDEX_SCALE;
    return true;
}

static uint64_t apply_bps(uint64_t value, uint32_t bps) {
    uint64_t whole = (value / HLX_RISK_BPS_SCALE) * bps;
    uint64_t remainder = ((value % HLX_RISK_BPS_SCALE) * bps) / HLX_RISK_BPS_SCALE;
    return whole + remainder;
}

static uint64_t ceil_ratio_threshold(uint64_t denominator, uint32_t bps) {
    uint64_t whole = (denominator / HLX_RISK_BPS_SCALE) * bps;
    uint64_t remainder_product = (denominator % HLX_RISK_BPS_SCALE) * bps;
    uint64_t remainder = remainder_product / HLX_RISK_BPS_SCALE;
    if (remainder_product % HLX_RISK_BPS_SCALE != 0) {
        remainder++;
    }
    return whole + remainder;
}

static uint32_t coverage_bps(uint64_t available, uint64_t liability) {
    uint32_t low = 0;
    uint32_t high = HLX_RISK_BPS_SCALE;

    if (liability == 0 || available >= liability) {
        return HLX_RISK_BPS_SCALE;
    }
    while (low < high) {
        uint32_t middle = low + (high - low + 1U) / 2U;
        if (available >= ceil_ratio_threshold(liability, middle)) {
            low = middle;
        } else {
            high = middle - 1U;
        }
    }
    return low;
}

static bool valid_policy(const HlxRiskPolicy *policy) {
    return policy->slash_shock_bps <= HLX_RISK_BPS_SCALE &&
           policy->liquidity_haircut_bps <= HLX_RISK_BPS_SCALE &&
           policy->redemption_run_bps <= HLX_RISK_BPS_SCALE &&
           policy->backstop_recovery_bps <= HLX_RISK_BPS_SCALE &&
           policy->reserve_floor_bps <= HLX_RISK_BPS_SCALE &&
           policy->minimum_queue_coverage_bps <= HLX_RISK_BPS_SCALE &&
           policy->concentration_limit_bps <= HLX_RISK_BPS_SCALE;
}

HlxRiskStatus hlx_risk_project(
    const HlxRiskInput *input,
    const HlxRiskPolicy *policy,
    HlxRiskProjection *projection
) {
    uint64_t post_slash;
    uint64_t post_liquidity;

    if (input == NULL || policy == NULL || projection == NULL || input->share_index == 0) {
        return HLX_RISK_ERR_ARGUMENT;
    }
    if (!valid_policy(policy)) {
        return HLX_RISK_ERR_POLICY;
    }
    if (input->largest_account_shares > input->live_shares) {
        return HLX_RISK_ERR_ARGUMENT;
    }

    memset(projection, 0, sizeof(*projection));
    if (!shares_to_assets(input->live_shares, input->share_index,
                          &projection->live_liability_assets)) {
        return HLX_RISK_ERR_ARITHMETIC;
    }
    projection->locked_liability_assets = input->locked_claim_assets;
    projection->pending_liability_assets = input->pending_assets;

    if (!checked_add(projection->live_liability_assets,
                     projection->locked_liability_assets,
                     &projection->total_liability_assets) ||
        !checked_add(projection->total_liability_assets,
                     projection->pending_liability_assets,
                     &projection->total_liability_assets)) {
        return HLX_RISK_ERR_ARITHMETIC;
    }

    projection->redemption_run_assets =
        apply_bps(projection->live_liability_assets, policy->redemption_run_bps);
    if (!checked_add(input->pending_assets, projection->redemption_run_assets,
                     &projection->queue_liability_assets)) {
        return HLX_RISK_ERR_ARITHMETIC;
    }

    projection->slash_loss_assets = apply_bps(input->reserves, policy->slash_shock_bps);
    post_slash = input->reserves - projection->slash_loss_assets;
    projection->liquidity_loss_assets =
        apply_bps(post_slash, policy->liquidity_haircut_bps);
    post_liquidity = post_slash - projection->liquidity_loss_assets;
    projection->recovered_backstop_assets =
        apply_bps(input->backstop_assets, policy->backstop_recovery_bps);
    if (!checked_add(post_liquidity, projection->recovered_backstop_assets,
                     &projection->effective_reserves)) {
        return HLX_RISK_ERR_ARITHMETIC;
    }

    projection->required_reserve_assets =
        apply_bps(projection->total_liability_assets, policy->reserve_floor_bps);
    projection->system_coverage_bps =
        coverage_bps(projection->effective_reserves, projection->total_liability_assets);
    projection->queue_coverage_bps =
        coverage_bps(projection->effective_reserves, projection->queue_liability_assets);
    projection->concentration_bps =
        input->live_shares == 0
            ? 0
            : coverage_bps(input->largest_account_shares, input->live_shares);

    if (projection->effective_reserves >= projection->total_liability_assets) {
        projection->residual_buffer_assets =
            projection->effective_reserves - projection->total_liability_assets;
    } else {
        projection->capital_shortfall_assets =
            projection->total_liability_assets - projection->effective_reserves;
    }

    if (projection->effective_reserves < projection->queue_liability_assets ||
        projection->queue_coverage_bps < policy->minimum_queue_coverage_bps) {
        projection->band = HLX_RISK_CRITICAL;
    } else if (projection->capital_shortfall_assets != 0 ||
               projection->concentration_bps > policy->concentration_limit_bps) {
        projection->band = HLX_RISK_RESTRICTED;
    } else if (projection->residual_buffer_assets < projection->required_reserve_assets) {
        projection->band = HLX_RISK_WATCH;
    } else {
        projection->band = HLX_RISK_NORMAL;
    }
    return HLX_RISK_OK;
}

const char *hlx_risk_status_message(HlxRiskStatus status) {
    switch (status) {
        case HLX_RISK_OK:
            return "ok";
        case HLX_RISK_ERR_ARGUMENT:
            return "invalid argument";
        case HLX_RISK_ERR_POLICY:
            return "invalid policy";
        case HLX_RISK_ERR_ARITHMETIC:
            return "arithmetic range exceeded";
        default:
            return "unknown risk status";
    }
}

const char *hlx_risk_band_name(HlxRiskBand band) {
    switch (band) {
        case HLX_RISK_NORMAL:
            return "normal";
        case HLX_RISK_WATCH:
            return "watch";
        case HLX_RISK_RESTRICTED:
            return "restricted";
        case HLX_RISK_CRITICAL:
            return "critical";
        default:
            return "unknown";
    }
}
