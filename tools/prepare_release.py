#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Validate a complete release's firmware and native recovery archives; write provenance/checksums."""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import subprocess
import zipfile

from fm1_recover import APP_LO, Package, VERSION, sha256
from fm1pkg_make import sfc

ROOT = Path(__file__).resolve().parents[1]
TARGETS = ("windows-x64", "macos-arm64", "macos-x64", "linux-x64")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("version")
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    if args.version != VERSION:
        raise RuntimeError("Recovery tool and release versions differ")
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    out = args.directory
    major, minor, patch = args.version.split(".")
    expected = f"FM-1_9{major}{int(minor):02d}{patch}"
    packages = {}
    for suffix, cores in (("", 1), ("-1core", 0)):
        name = f"melodee-{args.version}{suffix}.fwsc"
        package = Package(out / name)
        if package.product != expected:
            raise RuntimeError(f"Wrong package identity: {name}")
        app = out / f"melodee-{args.version}{suffix}-app.bin"
        if not app.is_file():
            raise RuntimeError(f"Missing application binary: {app}")
        decoded = bytearray(package.flash[APP_LO:])
        sfc(decoded, 0, len(decoded), 0, package.key)
        app_data = app.read_bytes()
        if decoded[0x120:0x120 + len(app_data)] != app_data:
            raise RuntimeError(f"Application binary differs from package: {app}")
        packages[name] = {"MELODEE_DUAL_CORE": cores, "MELODEE_RAM_AUDIO": 1,
                          "sha256": package.digest}
    if len({p["sha256"] for p in packages.values()}) != 2:
        raise RuntimeError("Dual and single core packages must differ")
    recovery = {}
    for target in TARGETS:
        name = f"fm1-recover-{VERSION}-{target}.zip"
        prefix = name[:-4] + "/"
        with zipfile.ZipFile(out / name) as archive:
            info = json.loads(archive.read(prefix + "BUILD_INFO.json"))
            if info["commit"] != commit or info["version"] != VERSION or info["target"] != target or not info["smoke_tests_passed"]:
                raise RuntimeError(f"Wrong or untested recovery binary: {name}")
            for line in archive.read(prefix + "SHA256SUMS").decode().splitlines():
                digest, path = line.split("  ", 1)
                if sha256(archive.read(prefix + path)) != digest:
                    raise RuntimeError(f"Recovery archive checksum mismatch: {name}/{path}")
            recovery[name] = info
    with zipfile.ZipFile(out / f"melodee-{VERSION}-licenses.zip", "w", zipfile.ZIP_DEFLATED) as archive:
        for file in (ROOT / "LICENSE", ROOT / "LICENSING.md", *(ROOT / "LICENSES").glob("*.txt")):
            archive.write(file, str(file.relative_to(ROOT)))
    info = {"version": VERSION, "product": expected, "branch": "main", "commit": commit,
            "builtAt": datetime.now(timezone.utc).isoformat(), "core_builds": packages, "recovery_builds": recovery}
    (out / "BUILD_INFO.json").write_text(json.dumps(info, indent=2) + "\n")
    (out / "SHA256SUMS").write_text("".join(f"{sha256(p.read_bytes())}  {p.name}\n"
                                             for p in sorted(out.iterdir()) if p.is_file() and p.name != "SHA256SUMS"))


if __name__ == "__main__":
    main()
