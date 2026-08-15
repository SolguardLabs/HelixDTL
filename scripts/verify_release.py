from __future__ import annotations

import hashlib
import json
import re
import struct
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROTECTED_BLOBS = {
    "include/helix.h": "30f20038b7227263186538be7be3e97893dfd29a",
    "src/amount.c": "25f9c2e23f238fcdde0aaf2f258fcf382ac97f72",
    "src/quote.c": "81ed94c928a0de83fce125b26b5555a9e564e976",
}
PROTECTED_CODE = {
    "src/ledger.c": "5a5bd2b739b87f7e44d82055e8e77cad195d71cc58aba46298e7ad6052577c18",
    "src/quote.c": "2187e22a6f25e9dc6c7b093df2f549e8b4a6a4797de431fa7700b889b0edda5e",
}
RESTRICTED = re.compile(
    r"\b(?:ctf|labs?|laboratorios?|vulnerabil(?:ity|idad|idades)|vulnerable|bugs?|exploits?|bypass|attackers?|atacantes?)\b",
    re.IGNORECASE,
)
TEXT_SUFFIXES = {
    ".c",
    ".h",
    ".hlx",
    ".js",
    ".json",
    ".md",
    ".mjs",
    ".py",
    ".toml",
    ".txt",
    ".yaml",
    ".yml",
}


def git(*args: str) -> str:
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True).strip()


def source_fingerprint(path: Path) -> str:
    source = path.read_text(encoding="utf-8")
    source = re.sub(r"/\*.*?\*/|//[^\n]*", "", source, flags=re.DOTALL)
    source = re.sub(r"\s+", "", source)
    return hashlib.sha256(source.encode()).hexdigest()


def nonblank(path: Path) -> int:
    return sum(bool(line.strip()) for line in path.read_text(encoding="utf-8").splitlines())


def public_files() -> list[str]:
    names = git("ls-files", "--cached", "--others", "--exclude-standard").splitlines()
    return sorted(
        name
        for name in names
        if name and not name.startswith("tests/private/") and (ROOT / name).is_file()
    )


def main() -> int:
    errors: list[str] = []
    for name, expected in PROTECTED_BLOBS.items():
        if git("hash-object", name) != expected:
            errors.append(f"{name} does not match its approved source blob")
    for name, expected in PROTECTED_CODE.items():
        if source_fingerprint(ROOT / name) != expected:
            errors.append(f"{name} changes approved executable semantics")

    docs = sorted((ROOT / "docs").glob("*.md"))
    if len(docs) != 7:
        errors.append(f"expected 7 operational documents, found {len(docs)}")
    markdown = [ROOT / "README.md", ROOT / "SECURITY.md", *docs]
    diagrams = sum(path.read_text(encoding="utf-8").count("```mermaid") for path in markdown)
    if diagrams != 27:
        errors.append(f"expected 27 Mermaid diagrams, found {diagrams}")

    banner = (ROOT / "assets" / "banner.png").read_bytes()
    width, height = struct.unpack(">II", banner[16:24])
    if not banner.startswith(b"\x89PNG") or (width, height) != (1672, 941):
        errors.append("banner must be a 1672x941 PNG")

    release = json.loads((ROOT / "RELEASE.json").read_text(encoding="utf-8"))
    package = json.loads((ROOT / "package.json").read_text(encoding="utf-8"))
    lock = json.loads((ROOT / "package-lock.json").read_text(encoding="utf-8"))
    if release.get("version") != "1.0.0" or release.get("tag") != "v1.0.0":
        errors.append("release metadata must identify v1.0.0")
    if package.get("version") != "1.0.0" or lock.get("version") != "1.0.0":
        errors.append("Node package metadata must identify 1.0.0")
    if not git("check-ignore", "tests/private/proof.test.js"):
        errors.append("private evidence path must remain ignored")
    if (ROOT / "CHALLENGE.md").exists():
        errors.append("retired challenge document must not be present")

    names = public_files()
    excluded = {"LICENSE", "scripts/verify_release.py"}
    for name in names:
        path = ROOT / name
        if name in excluded or path.suffix not in TEXT_SUFFIXES:
            continue
        if RESTRICTED.search(path.read_text(encoding="utf-8")):
            errors.append(f"{name} contains restricted public terminology")

    workflows = "\n".join(
        path.read_text(encoding="utf-8")
        for path in (ROOT / ".github" / "workflows").glob("*.yml")
    )
    for marker in (
        "actions/checkout@v7",
        "actions/setup-node@v7",
        "actions/setup-python@v7",
        "ubuntu-latest",
        "windows-latest",
    ):
        if marker not in workflows:
            errors.append(f"workflow marker missing: {marker}")

    c_files = [name for name in names if name.startswith(("src/", "include/")) and Path(name).suffix in {".c", ".h"}]
    c_nonblank = sum(nonblank(ROOT / name) for name in c_files)
    node_tests = sum(
        len(re.findall(r"\btest\(", (ROOT / name).read_text(encoding="utf-8")))
        for name in names
        if name.startswith("tests/node/") and name.endswith(".js")
    )
    c_tests = sum(
        len(re.findall(r"\bstatic void test_", (ROOT / name).read_text(encoding="utf-8")))
        for name in names
        if name.startswith("tests/c/") and name.endswith(".c")
    )
    public_text = sum(
        nonblank(ROOT / name)
        for name in names
        if Path(name).suffix in TEXT_SUFFIXES or name in {"LICENSE", ".gitignore", ".gitattributes"}
    )
    documentation = "\n".join(path.read_text(encoding="utf-8") for path in markdown)

    if errors:
        for error in errors:
            print(f"- {error}")
        return 1
    print(
        json.dumps(
            {
                "protocol": "HelixDTL",
                "version": "1.0.0",
                "c_nonblank": c_nonblank,
                "node_tests": node_tests,
                "c_checks": c_tests,
                "public_tests": node_tests + c_tests,
                "public_text_nonblank": public_text,
                "docs": len(docs),
                "diagrams": diagrams,
                "protected_blobs": len(PROTECTED_BLOBS),
                "protected_code_fingerprints": len(PROTECTED_CODE),
                "documentation_sha256": hashlib.sha256(documentation.encode()).hexdigest(),
            },
            indent=2,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
