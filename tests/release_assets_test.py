#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Release aggregation refuses mismatched firmware, stale native builds and damaged ZIPs."""
import contextlib
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import prepare_release as A
from fm1_recover import APP_LO, Package, sha256
from fm1pkg_make import sfc, ufw
from recovery_test import make_package

MAJOR, MINOR, PATCH = A.VERSION.split(".")
PRODUCT = f"FM-1_9{MAJOR}{int(MINOR):02d}{PATCH}"


class ReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.folder = Path(self.temp.name)
        self.commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
        for suffix in ("", "-1core"):
            package = self.folder / f"melodee-{A.VERSION}{suffix}.fwsc"
            package.write_bytes(make_package())
            if suffix:
                flash = bytearray(Package(package).flash)
                flash[APP_LO + 0x120] ^= 1
                package.write_bytes(ufw(bytes(flash), b"loader", PRODUCT))
            else:
                package.write_bytes(ufw(Package(package).flash, b"loader", PRODUCT))
            obj = Package(package)
            decoded = bytearray(obj.flash[APP_LO:])
            sfc(decoded, 0, len(decoded), 0, obj.key)
            (self.folder / f"melodee-{A.VERSION}{suffix}-app.bin").write_bytes(decoded[0x120:0x121])
        for target in A.TARGETS:
            self.write_archive(target)

    def tearDown(self):
        self.temp.cleanup()

    def write_archive(self, target, commit=None, damaged=False):
        name = f"fm1-recover-{A.VERSION}-{target}"
        info = {"commit": commit or self.commit, "version": A.VERSION, "target": target, "smoke_tests_passed": True}
        blob = json.dumps(info).encode()
        with zipfile.ZipFile(self.folder / (name + ".zip"), "w") as archive:
            archive.writestr(name + "/BUILD_INFO.json", blob)
            archive.writestr(name + "/SHA256SUMS", f"{sha256(blob) if not damaged else '0' * 64}  BUILD_INFO.json\n")

    def prepare(self):
        with patch.object(sys, "argv", ["prepare_release.py", A.VERSION, str(self.folder)]):
            A.main()

    def test_complete_release_and_padded_firmware_identity(self):
        self.prepare()
        info = json.loads((self.folder / "BUILD_INFO.json").read_text())
        self.assertEqual(info["product"], PRODUCT)
        self.assertEqual(info["commit"], self.commit)
        self.assertEqual(len(info["recovery_builds"]), 4)
        for line in (self.folder / "SHA256SUMS").read_text().splitlines():
            digest, name = line.split("  ", 1)
            self.assertEqual(sha256((self.folder / name).read_bytes()), digest)

    def test_application_mismatch_is_rejected(self):
        (self.folder / f"melodee-{A.VERSION}-app.bin").write_bytes(b"wrong app")
        with self.assertRaisesRegex(RuntimeError, "differs from package"):
            self.prepare()

    def test_stale_native_commit_is_rejected(self):
        self.write_archive("windows-x64", commit="0" * 40)
        with self.assertRaisesRegex(RuntimeError, "Wrong or untested"):
            self.prepare()

    def test_corrupted_archive_is_rejected(self):
        self.write_archive("macos-arm64", damaged=True)
        with self.assertRaisesRegex(RuntimeError, "checksum mismatch"):
            self.prepare()


if __name__ == "__main__":
    unittest.main()
