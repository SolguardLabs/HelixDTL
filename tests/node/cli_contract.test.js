import assert from "node:assert/strict";
import test from "node:test";

import { assertCommon, assertNoRisk, listScenarios, runScenario } from "../helpers/helix.js";

const normalScenarios = ["basic", "multi", "index", "partial", "transfer", "snapshot"];

test("CLI lists all bundled scenarios", () => {
    const scenarios = listScenarios();
    for (const name of [...normalScenarios, "drift"]) {
        assert.ok(scenarios.includes(name), `${name} is not listed`);
    }
});

test("normal scenarios expose the stable JSON contract", () => {
    for (const name of normalScenarios) {
        const payload = runScenario(name);
        assertCommon(payload, name);
        assert.ok(payload.account_count >= 2);
        assert.ok(payload.vault_count >= 1);
    }
});

test("normal scenarios do not quote locked shares above their issued index", () => {
    for (const name of ["basic", "multi", "index", "partial", "transfer"]) {
        const payload = runScenario(name);
        assertNoRisk(payload);
        for (const redemption of payload.redemptions) {
            assert.equal(
                redemption.assets,
                redemption.expected_assets,
                `${name} redemption ${redemption.id}`,
            );
            assert.equal(redemption.excess_quote_assets, 0);
        }
    }
});
