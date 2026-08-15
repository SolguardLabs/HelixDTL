import { existsSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

const projectRoot = resolve(fileURLToPath(new URL("..", import.meta.url)));
const defaultBinary = join(
    projectRoot,
    "build",
    process.platform === "win32" ? "helixdtl.exe" : "helixdtl",
);

export class HelixCommandError extends Error {
    constructor(message, { status, payload, stderr } = {}) {
        super(message);
        this.name = "HelixCommandError";
        this.status = status;
        this.payload = payload;
        this.stderr = stderr;
    }
}

function nonNegativeInteger(value, field) {
    if (!Number.isSafeInteger(value) || value < 0) {
        throw new TypeError(`${field} must be a non-negative safe integer`);
    }
    return value;
}

function requiredArray(payload, field) {
    if (!Array.isArray(payload[field])) {
        throw new TypeError(`snapshot.${field} must be an array`);
    }
    return payload[field];
}

function parsePayload(stdout, status, stderr) {
    let payload;
    try {
        payload = JSON.parse(stdout);
    } catch {
        throw new HelixCommandError("Helix returned a non-JSON response", {
            status,
            stderr,
        });
    }
    if (status !== 0 || payload.error) {
        throw new HelixCommandError(payload.detail ?? payload.error ?? "Helix command failed", {
            status,
            payload,
            stderr,
        });
    }
    return payload;
}

export class HelixClient {
    constructor({ binary = process.env.HELIX_BIN ?? defaultBinary, cwd = projectRoot } = {}) {
        this.binary = resolve(binary);
        this.cwd = resolve(cwd);
    }

    invoke(args) {
        if (!existsSync(this.binary)) {
            throw new HelixCommandError(`Helix binary not found at ${this.binary}`);
        }
        const result = spawnSync(this.binary, args, {
            cwd: this.cwd,
            encoding: "utf8",
            stdio: "pipe",
        });
        if (result.error) {
            throw new HelixCommandError(result.error.message, { stderr: result.stderr });
        }
        return {
            status: result.status ?? 1,
            stdout: result.stdout,
            stderr: result.stderr,
        };
    }

    listScenarios() {
        const result = this.invoke(["--list"]);
        if (result.status !== 0) {
            throw new HelixCommandError("Unable to list Helix scenarios", result);
        }
        return result.stdout.trim().split(/\r?\n/).filter(Boolean);
    }

    runScenario(name) {
        if (!/^[a-z][a-z0-9-]*$/.test(name)) {
            throw new TypeError("scenario name has an invalid format");
        }
        const result = this.invoke([name]);
        return HelixClient.validateSnapshot(
            parsePayload(result.stdout, result.status, result.stderr),
        );
    }

    runScript(source, { filename = "request.hlx" } = {}) {
        if (typeof source !== "string" || source.trim() === "") {
            throw new TypeError("script source must be a non-empty string");
        }
        if (!/^[a-zA-Z0-9_.-]+\.hlx$/.test(filename)) {
            throw new TypeError("filename must be a simple .hlx name");
        }
        const directory = mkdtempSync(join(tmpdir(), "helix-client-"));
        const path = join(directory, filename);
        writeFileSync(path, source, { encoding: "utf8", flag: "wx" });
        try {
            const result = this.invoke(["run", path]);
            return HelixClient.validateSnapshot(
                parsePayload(result.stdout, result.status, result.stderr),
            );
        } finally {
            rmSync(directory, { recursive: true, force: true });
        }
    }

    static validateSnapshot(payload) {
        if (payload === null || typeof payload !== "object" || Array.isArray(payload)) {
            throw new TypeError("snapshot must be an object");
        }
        if (!/^[0-9a-f]{16}$/.test(payload.state_digest ?? "")) {
            throw new TypeError("snapshot.state_digest has an invalid format");
        }
        for (const field of [
            "accounts",
            "vaults",
            "positions",
            "locks",
            "redemptions",
            "quotes",
            "events",
        ]) {
            requiredArray(payload, field);
        }
        if (payload.index_scale !== 1_000_000) {
            throw new TypeError("snapshot.index_scale is unsupported");
        }
        return payload;
    }

    static byId(items, id, entity = "entity") {
        nonNegativeInteger(id, `${entity} id`);
        const item = items.find((entry) => entry.id === id);
        if (!item) {
            throw new RangeError(`${entity} ${id} was not found`);
        }
        return item;
    }

    static accountPortfolio(snapshot, accountId) {
        const account = HelixClient.byId(snapshot.accounts, accountId, "account");
        const positions = snapshot.positions.filter((item) => item.account_id === accountId);
        const locks = snapshot.locks.filter(
            (item) => item.owner_id === accountId && item.closed === false,
        );
        const redemptions = snapshot.redemptions.filter(
            (item) => item.owner_id === accountId && item.withdrawn === false,
        );
        return Object.freeze({
            accountId,
            externalAssets: account.external_assets,
            withdrawnAssets: account.withdrawn_assets,
            liquidShares: positions.reduce((sum, item) => sum + item.liquid_shares, 0),
            lockedShares: locks.reduce((sum, item) => sum + item.shares, 0),
            pendingAssets: redemptions.reduce((sum, item) => sum + item.assets, 0),
            positionCount: positions.length,
            openLockCount: locks.length,
            pendingRedemptionCount: redemptions.length,
        });
    }

    static vaultRiskInput(snapshot, vaultId, { backstopAssets = 0 } = {}) {
        const vault = HelixClient.byId(snapshot.vaults, vaultId, "vault");
        nonNegativeInteger(backstopAssets, "backstopAssets");
        const positions = snapshot.positions.filter((item) => item.vault_id === vaultId);
        const locks = snapshot.locks.filter(
            (item) => item.vault_id === vaultId && item.closed === false,
        );
        const accountShares = new Map();
        for (const position of positions) {
            accountShares.set(
                position.account_id,
                (accountShares.get(position.account_id) ?? 0) + position.liquid_shares,
            );
        }
        const lockedClaimAssets = locks.reduce(
            (sum, lock) => sum + Math.floor((lock.shares * lock.issued_index) / 1_000_000),
            0,
        );
        return Object.freeze({
            reserves: vault.reserves,
            liveShares: vault.live_shares,
            shareIndex: vault.share_index,
            lockedClaimAssets,
            pendingAssets: vault.pending_assets,
            largestAccountShares: Math.max(0, ...accountShares.values()),
            backstopAssets,
        });
    }
}
