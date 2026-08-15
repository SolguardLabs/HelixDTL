import { existsSync, mkdirSync } from "node:fs";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

const root = resolve(fileURLToPath(new URL("..", import.meta.url)));
const isWindows = process.platform === "win32";
const riskTest = process.argv.includes("--risk-test");
const exe = isWindows
    ? riskTest
        ? "helix-risk-test.exe"
        : "helixdtl.exe"
    : riskTest
      ? "helix-risk-test"
      : "helixdtl";
const outDir = join(root, "build");
const output = join(outDir, exe);
const sources = (
    riskTest
        ? ["src/amount.c", "src/risk.c", "tests/c/risk_model_test.c"]
        : [
              "src/amount.c",
              "src/codec.c",
              "src/invariants.c",
              "src/ledger.c",
              "src/query.c",
              "src/quote.c",
              "src/risk.c",
              "src/script.c",
              "src/scenarios.c",
              "src/main.c",
          ]
).map((file) => join(root, file));

function tryCompiler(command) {
    const probe = spawnSync(command, command === "cl" ? [] : ["--version"], {
        cwd: root,
        encoding: "utf8",
        stdio: "pipe",
    });
    if (probe.error) {
        return false;
    }
    return probe.status === 0 || command === "cl";
}

function selectCompiler() {
    if (process.env.HELIX_CC) {
        return process.env.HELIX_CC;
    }
    for (const candidate of ["cc", "gcc", "clang", "cl"]) {
        if (tryCompiler(candidate)) {
            return candidate;
        }
    }
    return null;
}

function findVsDevCmd() {
    const programFilesX86 = process.env["ProgramFiles(x86)"];
    if (!programFilesX86) {
        return null;
    }

    const vswhere = join(programFilesX86, "Microsoft Visual Studio", "Installer", "vswhere.exe");
    if (existsSync(vswhere)) {
        const found = spawnSync(
            vswhere,
            [
                "-latest",
                "-products",
                "*",
                "-requires",
                "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                "-property",
                "installationPath",
            ],
            {
                cwd: root,
                encoding: "utf8",
                stdio: "pipe",
            },
        );
        const installationPath = found.stdout.trim();
        const candidate = join(installationPath, "Common7", "Tools", "VsDevCmd.bat");
        if (found.status === 0 && installationPath && existsSync(candidate)) {
            return candidate;
        }
    }

    const fallback = join(
        programFilesX86,
        "Microsoft Visual Studio",
        "2022",
        "BuildTools",
        "Common7",
        "Tools",
        "VsDevCmd.bat",
    );
    return existsSync(fallback) ? fallback : null;
}

function relaunchWithVsDevCmd(vsDevCmd) {
    const forwarded = process.argv
        .slice(2)
        .map((value) => `"${value.replaceAll('"', '\\"')}"`)
        .join(" ");
    const commandLine = `call "${vsDevCmd}" -arch=x64 >nul && "${process.execPath}" "${fileURLToPath(import.meta.url)}" ${forwarded}`;
    const result = spawnSync(commandLine, {
        cwd: root,
        encoding: "utf8",
        env: { ...process.env, HELIX_VSDEV_REENTRY: "1" },
        shell: "cmd.exe",
        stdio: "inherit",
    });
    process.exit(result.status ?? 1);
}

function run(command, args) {
    const result = spawnSync(command, args, {
        cwd: root,
        encoding: "utf8",
        stdio: "pipe",
    });
    if (result.status !== 0 || result.error) {
        if (result.stdout) process.stdout.write(result.stdout);
        if (result.stderr) process.stderr.write(result.stderr);
        if (result.error) process.stderr.write(`${result.error.message}\n`);
        process.exit(result.status ?? 1);
    }
}

mkdirSync(outDir, { recursive: true });

const compiler = selectCompiler();
if (!compiler) {
    const vsDevCmd = isWindows && !process.env.HELIX_VSDEV_REENTRY ? findVsDevCmd() : null;
    if (vsDevCmd) {
        relaunchWithVsDevCmd(vsDevCmd);
    }
    process.stderr.write(
        "No C compiler found. Install gcc/clang, use a Visual Studio Developer shell for cl, or set HELIX_CC.\n",
    );
    process.exit(1);
}

if (compiler === "cl" || compiler.endsWith("\\cl.exe") || compiler.endsWith("/cl")) {
    run(compiler, [
        "/nologo",
        "/std:c11",
        "/W4",
        "/WX",
        "/D_CRT_SECURE_NO_WARNINGS",
        `/I${join(root, "include")}`,
        `/Fo${outDir}\\`,
        ...sources,
        `/Fe:${output}`,
    ]);
} else {
    run(compiler, [
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-Iinclude",
        ...sources,
        "-o",
        output,
    ]);
}

console.log(output);
