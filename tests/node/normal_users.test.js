import assert from "node:assert/strict";
import test from "node:test";

import { assertCommon, assertNoRisk, byId, runScenario } from "../helpers/helix.js";

test("basic users can deposit, lock, redeem and withdraw", () => {
    const payload = runScenario("basic");
    assertCommon(payload, "basic");
    assertNoRisk(payload);

    const alice = byId(payload.accounts, 1);
    const bob = byId(payload.accounts, 2);
    const vault = byId(payload.vaults, 7);
    const lock = byId(payload.locks, 1);
    const redemption = byId(payload.redemptions, 1);

    assert.equal(alice.external_assets, 1120);
    assert.equal(alice.withdrawn_assets, 120);
    assert.equal(bob.external_assets, 500);
    assert.equal(vault.reserves, 880);
    assert.equal(vault.live_shares, 700);
    assert.equal(vault.locked_shares, 180);
    assert.equal(vault.retired_shares, 120);
    assert.equal(lock.shares, 180);
    assert.equal(lock.closed, false);
    assert.equal(redemption.settled, true);
    assert.equal(redemption.withdrawn, true);
});

test("locked shares transferred before epoch rollover keep their original quote index", () => {
    const payload = runScenario("transfer");
    assertCommon(payload, "transfer");
    assertNoRisk(payload);

    const vault = byId(payload.vaults, 7);
    const bob = byId(payload.accounts, 2);
    const child = byId(payload.locks, 2);
    const redemption = byId(payload.redemptions, 1);

    assert.equal(child.owner_id, 2);
    assert.equal(child.index_refreshed, false);
    assert.equal(child.issued_index, child.redemption_index);
    assert.equal(redemption.quote_index, redemption.issued_index);
    assert.equal(redemption.assets, 150);
    assert.equal(bob.external_assets, 1050);
    assert.equal(vault.reserves, 850);
    assert.equal(vault.locked_shares, 250);
});
