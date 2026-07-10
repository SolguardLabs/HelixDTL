import assert from "node:assert/strict";
import test from "node:test";

import { assertCommon, assertNoRisk, byId, runScenario } from "../helpers/helix.js";

test("index changes only accrue value to live shares", () => {
    const payload = runScenario("index");
    assertCommon(payload, "index");
    assertNoRisk(payload);

    const vault = byId(payload.vaults, 7);
    const redemption = byId(payload.redemptions, 1);
    const alice = byId(payload.accounts, 1);

    assert.equal(vault.epoch, 3);
    assert.equal(vault.previous_index_text, "1.250000");
    assert.equal(vault.share_index_text, "1.400000");
    assert.equal(vault.accrued_assets, 815);
    assert.equal(vault.reserves, 3190);
    assert.equal(redemption.quote_index_text, "1.250000");
    assert.equal(redemption.assets, 125);
    assert.equal(alice.external_assets, 2125);
});

test("partial redemptions leave the remaining locked balance claimable", () => {
    const payload = runScenario("partial");
    assertCommon(payload, "partial");
    assertNoRisk(payload);

    const vault = byId(payload.vaults, 7);
    const lock = byId(payload.locks, 1);
    const first = byId(payload.redemptions, 1);
    const second = byId(payload.redemptions, 2);
    const alice = byId(payload.accounts, 1);

    assert.equal(first.assets, 300);
    assert.equal(second.assets, 200);
    assert.equal(lock.shares, 400);
    assert.equal(lock.redeemed_shares, 500);
    assert.equal(lock.closed, false);
    assert.equal(vault.locked_shares, 400);
    assert.equal(vault.retired_shares, 500);
    assert.equal(vault.reserves, 700);
    assert.equal(alice.external_assets, 1100);
});
