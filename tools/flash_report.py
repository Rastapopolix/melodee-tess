#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 melodee-tess contributors
"""Where the device image's flash goes: section sizes, the biggest symbols, and the totals per source file.

  tools/flash_report.py [build/melodee.elf] [--top N]

Runs the JieLi objdump (JIELI_TOOLCHAIN, as tools/build.py) on the app ELF and prints a Markdown report. The image
is the boot stub plus every section stored in XIP: .text (code and constants) and the load copies of .ram_text,
.dsp_text, .dsp_tables, .data and .audio_text. melodee.c is one translation unit, so symbols are matched to the
source file that defines them by name (firmware/src, firmware/hal, build/gen); a static name defined in more than
one file goes to the first. Bytes without a symbol (string literals, padding, merged constants) are listed per
section as "no symbol".
"""
import argparse
import re
import sys
from collections import defaultdict
from pathlib import Path

SRC = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SRC / "tools"))
import build  # noqa: E402

XIP_LEN = 0x8C000                    # firmware/app.ld: XIP LENGTH
FLASH_SECTIONS = (".text", ".ram_text", ".dsp_text", ".dsp_tables", ".data", ".audio_text")
SYM = re.compile(r"^([0-9a-f]+)\s.{7}\s(\S+)\s+([0-9a-f]+)\s+(?:\.hidden\s+)?(\S+)$")
HDR = re.compile(r"^\s*\d+\s+(\S+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)")
DEF = re.compile(r"^(?!\s)(?!#)(?!typedef\b)(?!return\b)[^;=(]*?\b([A-Za-z_]\w*)\s*(\(|\[|=)")


def definitions():
    """name -> source file (relative to the tree), from top-level definitions"""
    where = {}
    roots = [SRC / "firmware" / "src", SRC / "firmware" / "hal", build.GEN, SRC / "firmware" / "src" / "x0x"]
    for root in roots:
        if not root.is_dir():
            continue
        for f in sorted([*root.glob("*.c"), *root.glob("*.h")]):
            rel = str(f.relative_to(SRC))
            for line in f.read_text(errors="replace").splitlines():
                m = DEF.match(line)
                if m and m.group(1) not in ("if", "for", "while", "switch", "sizeof"):
                    where.setdefault(m.group(1), rel)
    return where


def base_name(name):
    return re.sub(r"\.\d+$", "", name.split(".llvm.")[0])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("elf", nargs="?", default=str(build.OUT / "melodee.elf"))
    ap.add_argument("--top", type=int, default=80)
    ap.add_argument("--csv", help="also write every symbol (section, name, file, bytes) to this file")
    a = ap.parse_args()
    elf = Path(a.elf).resolve()
    heads = build.tc("common/bin/objdump", "-h", elf)
    syms = build.tc("common/bin/objdump", "-t", elf)
    sect = {}
    for line in heads.splitlines():
        m = HDR.match(line)
        if m:
            sect[m.group(1)] = int(m.group(2), 16)
    stub = build.STUB_LEN
    image = stub + sum(sect.get(s, 0) for s in FLASH_SECTIONS)
    where = definitions()
    per_sym = defaultdict(int)
    per_sect_named = defaultdict(int)
    for line in syms.splitlines():
        m = SYM.match(line)
        if not m or m.group(2) not in FLASH_SECTIONS:
            continue
        size = int(m.group(3), 16)
        if size:
            per_sym[(m.group(2), base_name(m.group(4)))] += size
            per_sect_named[m.group(2)] += size
    per_file = defaultdict(int)
    per_file_n = defaultdict(int)
    for (s, name), size in per_sym.items():
        f = where.get(name, "(not found in the source)")
        per_file[f] += size
        per_file_n[f] += 1

    out = ["# App flash", "", f"Image about {image:,} B of {XIP_LEN:,} B (XIP): {XIP_LEN - image:,} B left "
           "(sections rounded to 4 B; tools/build.py prints the exact figure).", "",
           "| Section | Size | With a symbol | No symbol |", "| --- | ---: | ---: | ---: |",
           f"| boot stub | {stub:,} | | |"]
    for s in FLASH_SECTIONS:
        if sect.get(s):
            out.append(f"| {s} | {sect[s]:,} | {per_sect_named[s]:,} | {sect[s] - per_sect_named[s]:,} |")
    out += ["", "## By source file", "", "| File | Bytes | Symbols |", "| --- | ---: | ---: |"]
    for f, size in sorted(per_file.items(), key=lambda kv: -kv[1]):
        out.append(f"| {f} | {size:,} | {per_file_n[f]} |")
    out += ["", f"## The {a.top} biggest symbols", "", "| Symbol | Section | File | Bytes |",
            "| --- | --- | --- | ---: |"]
    for (s, name), size in sorted(per_sym.items(), key=lambda kv: -kv[1])[:a.top]:
        out.append(f"| {name} | {s} | {where.get(name, '?')} | {size:,} |")
    print("\n".join(out))
    if a.csv:
        rows = sorted(per_sym.items(), key=lambda kv: -kv[1])
        Path(a.csv).write_text("section,symbol,file,bytes\n" + "".join(
            f"{s},{name},{where.get(name, '')},{size}\n" for (s, name), size in rows))


if __name__ == "__main__":
    main()
