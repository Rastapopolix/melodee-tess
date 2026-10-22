#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Theo Dainis (melodee-tess)
"""The parameters' help texts (firmware/src/pid.c PIDS), packed for the device build.

  tools/gen_help.py OUT.h

pid.c stays the source: this reads its T_ / G_ / X_ entries (and the text macros defined among them) in order and
writes OUT.h with the texts byte-pair coded: every byte from PID_HELP_LO up stands for two bytes (either of which
may be a code again), the codes chosen most frequent pair first while one is left. Texts that are equal are kept
once. The help gesture expands one text at a time (pid_help_unpack.h); the device build leaves the plain texts
out (MELODEE_HELP_PACKED, firmware/src/pid.c), the host build keeps them, and tests/help_pack_test.c checks every
expanded text against them. The round trip is checked here too.
"""
import argparse
import re
import sys
from collections import Counter
from pathlib import Path

SRC = Path(__file__).resolve().parents[1]
PID = SRC / "firmware" / "src" / "pid.c"
STR = re.compile(r'"((?:[^"\\\n]|\\.)*)"')
ESC = {"\\\"": "\"", "\\\\": "\\", "\\'": "'", "\\n": "\n"}


def unquote(lit):
    out = re.sub(r"\\.", lambda m: ESC[m.group(0)], lit)          # (KeyError: an escape this does not know)
    return out


def strings(expr, macros):
    """a C expression of string literals and text macros -> its text"""
    rest, out = expr.strip(), []
    while rest:
        m = STR.match(rest)
        if m:
            out.append(unquote(m.group(1)))
        else:
            m = re.match(r"[A-Za-z_]\w*", rest)
            if not m or m.group(0) not in macros:
                raise SystemExit(f"gen_help: cannot read {rest[:60]!r}")
            out.append(macros[m.group(0)])
        rest = rest[m.end():].lstrip()
    return "".join(out)


def args_of(text, i):
    """the arguments of the call whose '(' is at text[i], and the index after its ')'"""
    depth, start, args, j = 0, i + 1, [], i
    while True:
        c = text[j]
        if c == '"':
            j = STR.match(text, j).end()
            continue
        if c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                args.append(text[start:j])
                return args, j + 1
        elif c == "," and depth == 1:
            args.append(text[start:j])
            start = j + 1
        j += 1


def texts():
    src = PID.read_text()
    src = re.sub(r"/\*.*?\*/", lambda m: " " * len(m.group(0)), src, flags=re.S)
    begin = src.index("static const pid_entry_t PIDS[] = {")
    end = src.index("\n};", begin)
    body = src[begin:end].replace("\\\n", " ")
    macros, out, i = {}, [], body.index("{") + 1
    for m in re.finditer(r"^#define\s+(\w+)\s+(.*)$", body, re.M):
        macros[m.group(1)] = strings(m.group(2), macros)
    for m in re.finditer(r"\b([TGX])_\(", body):
        if body[:m.start()].rfind("#define") > body[:m.start()].rfind("\n"):
            continue
        a, _ = args_of(body, m.end() - 1)
        out.append(strings(a[-1], macros))
    return out


def pack(items):
    """byte-pair code the distinct texts: (lo, pairs, coded texts)"""
    raw = [t.encode("latin-1") for t in items]
    lo = max(max(r) for r in raw if r) + 1
    assert lo <= 128, "help text outside ASCII: fewer codes are left"
    data, pairs = [list(r) for r in raw], []
    for code in range(lo, 256):
        c = Counter((a, b) for d in data for a, b in zip(d, d[1:]))
        if not c:
            break
        pair, n = c.most_common(1)[0]
        if n < 3:                                     # a pair seen twice saves 2 B and costs 2 B of table
            break
        pairs.append(pair)
        for k, d in enumerate(data):
            o, i = [], 0
            while i < len(d):
                if i + 1 < len(d) and (d[i], d[i + 1]) == pair:
                    o.append(code)
                    i += 2
                else:
                    o.append(d[i])
                    i += 1
            data[k] = o
    return lo, pairs, data


def unpack(lo, pairs, d):
    """as pid_help_unpack.h: each coded byte expanded on a stack; (the text, the deepest stack)"""
    out, depth = [], 0
    for b in d:
        stack = [b]
        while stack:
            depth = max(depth, len(stack))
            c = stack.pop()
            if c >= lo:
                stack += [pairs[c - lo][1], pairs[c - lo][0]]
            else:
                out.append(c)
    return bytes(out), depth


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("out")
    a = ap.parse_args()
    items = texts()
    distinct = list(dict.fromkeys(items))
    lo, pairs, coded = pack(distinct)
    data, at, deepest, longest = [], {}, 0, 0
    for t, d in zip(distinct, coded):
        got, depth = unpack(lo, pairs, d)
        assert got == t.encode("latin-1"), f"gen_help: round trip fails for {t[:40]!r}"
        at[t] = len(data)
        data += d + [0]
        deepest, longest = max(deepest, depth), max(longest, len(t))
    assert len(data) < 65536
    plain = sum(len(t) + 1 for t in distinct)
    lines = ["/* generated by tools/gen_help.py from firmware/src/pid.c: the help texts, byte-pair coded */",
             "#pragma once", "#include <stdint.h>", "",
             f"#define PID_HELP_N {len(items)}            /* PIDS entries, in their order */",
             f"#define PID_HELP_LO {lo}u           /* bytes from here up are pairs (PID_HELP_PAIR[b - LO]) */",
             f"#define PID_HELP_MAX {longest}         /* the longest text, without its 0 */",
             f"#define PID_HELP_DEPTH {deepest}       /* the expansion stack it needs */", "",
             f"static const uint8_t PID_HELP_PAIR[{max(1, len(pairs))}][2] = {{"]
    for i in range(0, len(pairs), 12):
        lines.append("    " + " ".join(f"{{{p[0]}, {p[1]}}}," for p in pairs[i:i + 12]))
    lines += ["};", f"static const uint16_t PID_HELP_AT[{len(items)}] = {{"]
    offs = [at[t] for t in items]
    for i in range(0, len(offs), 16):
        lines.append("    " + ", ".join(map(str, offs[i:i + 16])) + ",")
    lines += ["};", f"static const uint8_t PID_HELP_DATA[{len(data)}] = {{"]
    for i in range(0, len(data), 24):
        lines.append("    " + ", ".join(map(str, data[i:i + 24])) + ",")
    lines.append("};")
    Path(a.out).parent.mkdir(parents=True, exist_ok=True)
    Path(a.out).write_text("\n".join(lines) + "\n")
    packed = len(data) + 2 * len(pairs) + 2 * len(items)
    print(f"help: {len(items)} texts ({len(distinct)} distinct), {plain} B -> {len(data)} B coded + "
          f"{2 * len(pairs)} B pairs + {2 * len(items)} B offsets = {packed} B -> {a.out}")


if __name__ == "__main__":
    main()
