#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Theo Dainis (melodee-tess)
"""The printable cheatsheet: CHEATSHEET.md as an A4 page in two columns, two sheets.

  tools/gen_cheatsheet.py [--pdf BROWSER]

CHEATSHEET.md stays the source: this reads its title, the paragraphs under it and its tables (a heading, then a
table of two columns) and writes docs/cheatsheet/cheatsheet.html. With --pdf, a Chromium-based browser (Chrome,
Edge, Chromium) prints that page headless to docs/cheatsheet/tessera-cheatsheet.pdf.
"""
import argparse
import html
import re
import subprocess
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parents[1]
MD = SRC / "CHEATSHEET.md"
OUT = SRC / "docs" / "cheatsheet"

CSS = """
@page { size: A4; margin: 11mm 10mm 10mm 10mm; }
:root { --lapis: #1d3461; --gold: #b8892b; --ink: #1a1a1a; --line: #d9d4c7; --tint: #f6f3ea; }
* { box-sizing: border-box; }
body { margin: 0; color: var(--ink); background: #fff; font: 7.6pt/1.28 "Segoe UI", "Helvetica Neue", Arial, sans-serif; }
header { border-bottom: 2px solid var(--gold); margin-bottom: 3mm; padding-bottom: 1.5mm; }
h1 { margin: 0; color: var(--lapis); font-size: 15pt; letter-spacing: .02em; }
header p { margin: 1mm 0 0; color: #444; }
main { column-count: 2; column-gap: 6mm; }
section { break-inside: avoid; margin: 0 0 3mm; }
h2 { margin: 0 0 1mm; padding: .6mm 1.6mm; color: #fff; background: var(--lapis); font-size: 8.4pt; }
table { width: 100%; border-collapse: collapse; }
td { padding: .55mm 1.4mm; vertical-align: top; border-bottom: .3mm solid var(--line); }
td:first-child { width: 42%; }
tr:nth-child(even) td { background: var(--tint); }
b { color: var(--lapis); }
footer { margin-top: 2mm; color: #666; font-size: 6.6pt; text-align: right; }
"""


def inline(text):
    """**bold** and [links](..) (their text only) as HTML; OCT- never broken at its hyphen"""
    text = html.escape(re.sub(r"\[([^\]]+)\]\([^)]*\)", r"\1", text), quote=False).replace("OCT-", "OCT&#8209;")
    return re.sub(r"\*\*(.+?)\*\*", r"<b>\1</b>", text)


def parse(md):
    """the title, the paragraphs under it, the sections: (heading, [(combo, effect)])"""
    title, intro, sections, para = "", [], [], []
    for line in md.splitlines():
        if line.startswith("# "):
            title = line[2:].strip()
        elif line.startswith("## "):
            sections.append((line[3:].strip(), []))
        elif line.startswith("|") and sections:
            cells = [c.strip() for c in line.strip().strip("|").split("|")]
            if len(cells) != 2:
                raise SystemExit(f"CHEATSHEET.md: a row of {len(cells)} cells: {line}")
            if not re.fullmatch(r"-+", cells[0]):
                sections[-1][1].append(cells)
        elif not sections and line.strip():
            para.append(line.strip())
        elif not sections and para:
            intro.append(" ".join(para))
            para = []
    if para:
        intro.append(" ".join(para))
    return title, intro, [(h, rows[1:]) for h, rows in sections]   # (each table's head row: the page needs none)


def page(title, intro, sections):
    out = [f"<!doctype html>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\">\n<title>{html.escape(title)}</title>",
           f"<!-- made by tools/gen_cheatsheet.py from CHEATSHEET.md: edit that, not this -->\n<style>{CSS}</style>\n"
           "</head>\n<body>\n<header>", f"<h1>{html.escape(title)}</h1>"]
    out += [f"<p>{inline(p)}</p>" for p in intro[1:]]          # (the first: where the PDF is, not for the page)
    out.append("</header>\n<main>")
    for head, rows in sections:
        out.append(f"<section>\n<h2>{inline(head)}</h2>\n<table>")
        out += [f"<tr><td>{inline(a)}</td><td>{inline(b)}</td></tr>" for a, b in rows]
        out.append("</table>\n</section>")
    out.append("</main>\n<footer>Tessera 2.0, generative firmware. GPL-3.0. The manual: MANUAL.md</footer>\n"
               "</body>\n</html>\n")
    return "\n".join(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--pdf", metavar="BROWSER", help="a Chromium-based browser to print the PDF with")
    a = ap.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    page_path = OUT / "cheatsheet.html"
    page_path.write_text(page(*parse(MD.read_text(encoding="utf-8"))), encoding="utf-8")
    print(page_path)
    if a.pdf:
        pdf = OUT / "tessera-cheatsheet.pdf"
        subprocess.run([a.pdf, "--headless", "--disable-gpu", "--no-pdf-header-footer",
                        f"--print-to-pdf={pdf}", page_path.resolve().as_uri()], check=True)
        print(pdf)
    return 0


if __name__ == "__main__":
    sys.exit(main())
