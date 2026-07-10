import assert from "node:assert/strict";
import test from "node:test";

import { assertCommon, assertNoRisk, byId, runScenario } from "../helpers/helix.js";

test("multiple vaults keep independent epochs, reserves and share indexes", () => {
    const payload = runScenario("multi");
    assertCommon(payload, "multi");
    assertNoRisk(payload);

    const sol = byId(payload.vaults, 7);
    const usd = byId(payload.vaults, 8);
    const alice = byId(payload.accounts, 1);
    const carol = byId(payload.accounts, 3);
    const carolRedemption = byId(payload.redemptions, 2);

    assert.equal(sol.symbol, "hxSOL");
    assert.equal(usd.symbol, "hxUSD");
    assert.equal(sol.share_index_text, "1.000000");
    assert.equal(usd.share_index_text, "2.100000");
    assert.equal(sol.epoch, 1);
    assert.equal(usd.epoch, 2);
    assert.equal(alice.withdrawn_assets, 100);
    assert.equal(carol.withdrawn_assets, 84);
    assert.equal(carolRedemption.assets, 84);
    assert.equal(carolRedemption.expected_assets, 84);
});

test("positions remain scoped by vault id", () => {
    const payload = runScenario("multi");
    const positionsByVault = new Map();

    for (const position of payload.positions) {
        const key = position.vault_id;
        positionsByVault.set(key, (positionsByVault.get(key) ?? 0) + 1);
        assert.ok(position.liquid_shares >= 0);
    }

    assert.equal(positionsByVault.get(7), 2);
    assert.equal(positionsByVault.get(8), 1);
});
