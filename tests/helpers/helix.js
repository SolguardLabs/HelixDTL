import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import { existsSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

export const root = resolve(fileURLToPath(new URL("../../", import.meta.url)));
const exe = process.platform === "win32" ? "helixdtl.exe" : "helixdtl";
const defaultBin = join(root, "build", exe);

let built = false;

export function binaryPath() {
    return process.env.HELIX_BIN ?? defaultBin;
}

export function ensureBuilt() {
    if (process.env.HELIX_BIN) {
        return;
    }
    if (existsSync(defaultBin)) {
        built = true;
        return;
    }
    const result = spawnSync(process.execPath, ["scripts/build.mjs"], {
        cwd: root,
        encoding: "utf8",
        stdio: "pipe",
    });
    if (result.status !== 0) {
        throw new Error(
            ["failed to build HelixDTL", result.stdout.trim(), result.stderr.trim()]
                .filter(Boolean)
                .join("\n"),
        );
    }
    built = true;
}

export function runBinary(args, options = {}) {
    const { expectSuccess = true } = options;
    ensureBuilt();
    const result = spawnSync(binaryPath(), args, {
        cwd: root,
        encoding: "utf8",
        stdio: "pipe",
    });
    if (expectSuccess && result.status !== 0) {
        throw new Error(
            [
                `helixdtl ${args.join(" ")} failed with ${result.status}`,
                result.stdout.trim(),
                result.stderr.trim(),
            ]
                .filter(Boolean)
                .join("\n"),
        );
    }
    return result;
}

export function runScenario(name) {
    const result = runBinary([name]);
    return JSON.parse(result.stdout);
}

export function listScenarios() {
    const result = runBinary(["--list"]);
    return result.stdout.trim().split(/\r?\n/).filter(Boolean);
}

export function runScript(text) {
    const dir = mkdtempSync(join(tmpdir(), "helixdtl-"));
    const path = join(dir, "scenario.hlx");
    writeFileSync(path, text);
    try {
        const result = runBinary(["run", path]);
        return JSON.parse(result.stdout);
    } finally {
        rmSync(dir, { recursive: true, force: true });
    }
}

export function runBadScript(text) {
    const dir = mkdtempSync(join(tmpdir(), "helixdtl-"));
    const path = join(dir, "bad.hlx");
    writeFileSync(path, text);
    try {
        const result = runBinary(["run", path], { expectSuccess: false });
        return { result, payload: JSON.parse(result.stdout) };
    } finally {
        rmSync(dir, { recursive: true, force: true });
    }
}

export function byId(items, id) {
    const item = items.find((entry) => entry.id === id);
    assert.ok(item, `missing item ${id}`);
    return item;
}

export function assertDigest(value) {
    assert.match(value, /^[0-9a-f]{16}$/);
}

export function assertCommon(payload, scenario) {
    assert.equal(payload.error, undefined);
    assert.equal(payload.scenario, scenario);
    assert.equal(typeof payload.network_id, "string");
    assert.equal(payload.index_scale, 1_000_000);
    assertDigest(payload.state_digest);
    assert.ok(Array.isArray(payload.accounts));
    assert.ok(Array.isArray(payload.vaults));
    assert.ok(Array.isArray(payload.positions));
    assert.ok(Array.isArray(payload.locks));
    assert.ok(Array.isArray(payload.redemptions));
    assert.ok(Array.isArray(payload.quotes));
    assert.ok(Array.isArray(payload.account_views));
    assert.ok(Array.isArray(payload.vault_views));
    assert.ok(Array.isArray(payload.events));
    assert.equal(typeof payload.totals.external_assets, "number");
    assert.equal(typeof payload.totals.vault_reserves, "number");
    assert.equal(typeof payload.risk.insolvent_vaults, "number");
    assert.equal(typeof payload.risk.has_excess_quote, "boolean");
    assert.equal(typeof payload.invariants.solvent, "boolean");
    assert.equal(typeof payload.invariants.quotes_ok, "boolean");
    assert.equal(typeof payload.invariants.pending_ok, "boolean");
    assert.equal(typeof payload.invariants.live_share_assets, "number");
    assert.equal(typeof payload.invariants.locked_issue_assets, "number");
}

export function assertNoRisk(payload) {
    assert.equal(payload.totals.deficit_assets, 0);
    assert.equal(payload.totals.excess_quote_assets, 0);
    assert.equal(payload.risk.insolvent_vaults, 0);
    assert.equal(payload.risk.has_excess_quote, false);
    assert.equal(payload.invariants.solvent, true);
    assert.equal(payload.invariants.quotes_ok, true);
    assert.equal(payload.invariants.pending_ok, true);
    assert.equal(payload.invariants.undercollateralized_assets, 0);
}
