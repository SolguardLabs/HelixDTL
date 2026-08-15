import assert from "node:assert/strict";
import test from "node:test";

import { HelixClient, HelixCommandError } from "../../client/helix-client.mjs";

const client = new HelixClient();

test("client lists only supported operational scenarios", () => {
    assert.deepEqual(client.listScenarios(), [
        "basic",
        "multi",
        "index",
        "partial",
        "transfer",
        "snapshot",
    ]);
});

test("client validates snapshots and builds account portfolios", () => {
    const snapshot = client.runScenario("basic");
    const portfolio = HelixClient.accountPortfolio(snapshot, 1);
    assert.equal(portfolio.externalAssets, 1120);
    assert.equal(portfolio.withdrawnAssets, 120);
    assert.equal(portfolio.liquidShares, 700);
    assert.equal(portfolio.lockedShares, 180);
    assert.equal(portfolio.pendingAssets, 0);
});

test("client derives deterministic vault inputs for the risk engine", () => {
    const snapshot = client.runScenario("transfer");
    const riskInput = HelixClient.vaultRiskInput(snapshot, 7, { backstopAssets: 250 });
    assert.deepEqual(riskInput, {
        reserves: 850,
        liveShares: 600,
        shareIndex: 1_000_000,
        lockedClaimAssets: 250,
        pendingAssets: 0,
        largestAccountShares: 600,
        backstopAssets: 250,
    });
});

test("client returns typed command errors", () => {
    assert.throws(
        () => client.runScript("network helix-client\nunknown-command"),
        (error) => {
            assert.ok(error instanceof HelixCommandError);
            assert.equal(error.status, 1);
            assert.equal(error.payload.line, 2);
            return true;
        },
    );
});
