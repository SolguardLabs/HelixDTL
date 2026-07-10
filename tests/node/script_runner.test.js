import assert from "node:assert/strict";
import test from "node:test";

import { assertCommon, assertNoRisk, byId, runBadScript, runScript } from "../helpers/helix.js";

test("line-oriented scripts can execute normal vault share flows", () => {
    const payload = runScript(`
    network script-safe
    account 1 alice 2000
    account 2 bob 500
    vault 7 hxSOL 1.000000
    deposit 1 7 1000
    lock 1 7 500
    transfer-lock 1 2 1 200
    advance 7
    redeem 2 2 200
    settle 7
    withdraw 2 1
    snapshot
  `);

    assertCommon(payload, "scenario.hlx");
    assertNoRisk(payload);
    assert.equal(payload.network_id, "script-safe");
    assert.equal(byId(payload.accounts, 2).external_assets, 700);
    assert.equal(byId(payload.vaults, 7).reserves, 800);
    assert.equal(byId(payload.locks, 2).index_refreshed, false);
});

test("script errors are returned as JSON with line numbers", () => {
    const { result, payload } = runBadScript(`
    network bad-script
    account 1 alice 100
    vault 7 hxSOL 1.000000
    deposit 1 7 200
  `);

    assert.notEqual(result.status, 0);
    assert.equal(payload.error, true);
    assert.equal(payload.status, "balance");
    assert.equal(payload.line, 5);
    assert.match(payload.detail, /insufficient external assets/);
});

test("script quotes expose preflight values without mutating vault state", () => {
    const payload = runScript(`
    network quote-safe
    account 1 alice 1000
    vault 7 hxSOL 1.000000
    quote-deposit 1 7 250
    deposit 1 7 500
    lock 1 7 200
    quote-redeem 1 1 100
    snapshot
  `);

    assertCommon(payload, "scenario.hlx");
    assertNoRisk(payload);

    const depositQuote = byId(payload.quotes, 1);
    const redemptionQuote = byId(payload.quotes, 2);
    const accountView = payload.account_views.find(
        (view) => view.account_id === 1 && view.vault_id === 7,
    );
    const vaultView = payload.vault_views.find((view) => view.vault_id === 7);

    assert.equal(depositQuote.kind, "deposit");
    assert.equal(depositQuote.assets, 250);
    assert.equal(depositQuote.shares, 250);
    assert.equal(depositQuote.accepted, true);
    assert.equal(redemptionQuote.kind, "redemption");
    assert.equal(redemptionQuote.assets, 100);
    assert.equal(redemptionQuote.expected_assets, 100);
    assert.equal(redemptionQuote.over_issued_quote, false);
    assert.equal(byId(payload.accounts, 1).external_assets, 500);
    assert.equal(byId(payload.vaults, 7).reserves, 500);
    assert.equal(byId(payload.locks, 1).shares, 200);
    assert.deepEqual(accountView, {
        account_id: 1,
        vault_id: 7,
        liquid_shares: 300,
        locked_shares: 200,
        pending_assets: 0,
        withdrawable_assets: 0,
        open_lock_count: 1,
    });
    assert.deepEqual(vaultView, {
        vault_id: 7,
        open_lock_shares: 200,
        pending_redemption_assets: 0,
        pending_redemption_count: 0,
    });
});
