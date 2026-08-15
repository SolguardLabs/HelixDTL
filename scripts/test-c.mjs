import { existsSync } from "node:fs";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

const root = resolve(fileURLToPath(new URL("..", import.meta.url)));
const executable = join(
    root,
    "build",
    process.platform === "win32" ? "helix-risk-test.exe" : "helix-risk-test",
);

function run(command, args) {
    const result = spawnSync(command, args, {
        cwd: root,
        encoding: "utf8",
        stdio: "inherit",
    });
    if (result.error) {
        throw result.error;
    }
    if (result.status !== 0) {
        process.exit(result.status ?? 1);
    }
}

run(process.execPath, ["scripts/build.mjs", "--risk-test"]);
if (!existsSync(executable)) {
    throw new Error(`Risk test executable was not created at ${executable}`);
}
run(executable, []);
