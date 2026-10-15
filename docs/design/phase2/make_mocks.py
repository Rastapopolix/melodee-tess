#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Mock screens for phase 2's new pages, for review before any firmware is written: the GEN family, CORDS,
NORMALS, PIECE, ABOUT and the help gesture. Drawn at the FM-1's 240x240 in the NIGHT palette and the UI font;
the GEN and help mocks start from the real renders (build/ui_new/NIGHT, from tests/run_tests.sh).

  python3 docs/design/phase2/make_mocks.py      -> docs/design/phase2/*.png and sheet.png

Values and examples are illustrative, not decided."""
import os
import sys
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
OUT = os.path.dirname(os.path.abspath(__file__))
RENDERS = os.path.join(ROOT, "build", "ui_new", "NIGHT")
FONT = os.path.join(ROOT, "assets", "fonts", "InterTight[wght].ttf")

# NIGHT (build/gen/ui_palettes.h), the derived colours sampled from the renders
BG, SURF, RAISE, CHIP = (16, 20, 24), (32, 32, 41), (41, 40, 49), (57, 56, 65)
TEXT, MID, DIM = (246, 238, 255), (180, 174, 189), (115, 109, 123)
TRK = [(255, 121, 90), (255, 194, 66), (66, 218, 181), (156, 142, 255)]
ACC = TRK[0]


def mix(a, b, pct):
    return tuple(int(a[i] + (b[i] - a[i]) * pct / 100) for i in range(3))


TINT, SEL = mix(BG, ACC, 30), mix(BG, ACC, 45)


def font(size, weight=600):
    f = ImageFont.truetype(FONT, size)
    try:
        f.set_variation_by_axes([weight])
    except OSError:
        pass
    return f


def text(d, xy, s, size=11, weight=600, fill=TEXT, anchor="la"):
    d.text(xy, s, font=font(size, weight), fill=fill, anchor=anchor)


def screen():
    im = Image.new("RGB", (240, 240), BG)
    return im, ImageDraw.Draw(im)


def header(d, dots, active, title, bpm="85"):
    """the transport, the family's page dots, the page title pill"""
    d.polygon([(5, 4), (5, 14), (12, 9)], fill=TEXT)
    text(d, (14, 3), bpm, 12, 600)
    x = 56
    for i in range(dots):
        if i == active:
            d.rounded_rectangle([x, 7, x + 11, 10], 2, fill=ACC)
            x += 16
        else:
            d.ellipse([x, 7, x + 3, 10], fill=CHIP)
            x += 8
    w = int(d.textlength(title, font=font(10, 700))) + 16
    d.rounded_rectangle([232 - w, 2, 232, 15], 4, fill=ACC)
    text(d, (232 - w / 2, 9), title, 10, 700, BG, "mm")


def badge(d, x, y, n):
    d.rounded_rectangle([x, y, x + 10, y + 10], 2, fill=TRK[n - 1])
    text(d, (x + 5, y + 5), str(n), 8, 800, BG, "mm")


def footer(d, trk, eng, name, right=""):
    badge(d, 8, 218, trk)
    text(d, (24, 223), eng, 11, 600, TRK[trk - 1], "lm")
    text(d, (24 + d.textlength(eng, font=font(11, 600)) + 8, 223), name, 11, 500, TEXT, "lm")
    if right:
        text(d, (232, 223), right, 11, 500, MID, "rm")


def cards(d, y, items, sel=-1, h=34):
    """the knob cards along the bottom: (label, value, unit)"""
    for i, (lab, val, unit) in enumerate(items):
        x = 4 + i * 58
        d.rounded_rectangle([x, y, x + 54, y + h], 4, fill=SURF, outline=ACC if i == sel else None)
        text(d, (x + 6, y + 4), lab, 9, 700, ACC)
        text(d, (x + 6, y + 15), val, 12, 600)
        if unit:
            text(d, (x + 8 + d.textlength(val, font=font(12, 600)), y + 19), unit, 8, 600, MID)


def chip(d, x, y, w, s, fill, trk=0):
    d.rounded_rectangle([x, y, x + w, y + 16], 4, fill=fill)
    if trk:
        badge(d, x + 3, y + 3, trk)
        text(d, (x + 16 + (w - 16) / 2, y + 8), s, 9, 600, TEXT, "mm")
    else:
        text(d, (x + w / 2, y + 8), s, 9, 600, TEXT, "mm")


def cords():
    im, d = screen()
    header(d, 6, 4, "CORDS")
    rows = [  # source (track, word), destination (track, word), amount, activity 0..1
        ((1, "Turing"), (2, "Cutoff"), "+40 %", 0.8),
        ((2, "Euclid hit"), (4, "Decay"), "-25 %", 1.0),
        ((3, "Markov"), (1, "PULSES"), "+30 %", 0.3),
        ((0, "LFO 1"), (2, "FX send"), "+15 %", 0.6),
    ]
    for i, ((st, sw), (dt, dw), amt, act) in enumerate(rows):
        y = 22 + i * 46
        if i == 1:
            d.rounded_rectangle([3, y - 3, 236, y + 39], 5, fill=mix(BG, ACC, 8), outline=ACC)
        text(d, (10, y + 8), str(i + 1), 11, 600, ACC if i == 1 else MID, "mm")
        d.ellipse([7, y + 21, 12, y + 26], fill=mix(CHIP, ACC, int(act * 100)))   # activity LED
        f = SEL if i == 1 else mix(BG, ACC, 25)
        chip(d, 20, y, 78, sw, f, st)
        text(d, (106, y + 8), "→", 11, 600, MID, "mm")
        chip(d, 114, y, 78, dw, f, dt)
        text(d, (232, y + 8), amt, 10, 600, ACC if i == 1 else MID, "rm")
        # amount bar about its centre
        d.line([(20, y + 27), (232, y + 27)], fill=CHIP, width=2)
        c, v = 126, int(amt.split()[0]) * 106 // 100
        d.line([(c, y + 27), (c + v, y + 27)], fill=ACC if i == 1 else mix(BG, ACC, 50), width=2)
        d.line([(c, y + 23), (c, y + 31)], fill=TEXT)
    for i, s in enumerate(["Euclid hit", "Cord 2/16", "Decay", "-25 %"]):
        x = 4 + i * 58
        d.rounded_rectangle([x, 206, x + 54, 222], 4, fill=SURF)
        text(d, (x + 27, 214), s, 9, 600, TEXT, "mm")
    text(d, (8, 232), "cords 1-4 of 16 · 4 in use", 9, 500, DIM, "lm")
    return im


def normals():
    im, d = screen()
    header(d, 6, 5, "NORMALS")
    text(d, (8, 26), "The default patch: it plays with no cords.", 10, 500, MID)
    text(d, (8, 39), "A cord to the same place overrides it.", 10, 500, MID)
    rows = [("Energy", "Cutoff, all tracks", "+20 %", None),
            ("Velocity", "Level, own track", "+50 %", None),
            ("Turing", "Cutoff, own track", "+10 %", "cord 1"),
            ("Mod wheel", "Vibrato, all", "+30 %", None)]
    for i, (s, dst, amt, over) in enumerate(rows):
        y = 56 + i * 36
        d.rounded_rectangle([4, y, 235, y + 30], 4, fill=SURF)
        col = DIM if over else TEXT
        text(d, (12, y + 9), s, 10, 700, ACC if not over else DIM, "lm")
        text(d, (12, y + 22), dst, 9, 500, col, "lm")
        text(d, (228, y + 9), amt, 10, 600, col, "rm")
        text(d, (228, y + 22), ("overridden by " + over) if over else "on", 9, 500, ACC if over else MID, "rm")
    footer(d, 1, "CZ-1", "Vibraphone", "4 normals")
    return im


def piece():
    im, d = screen()
    header(d, 5, 3, "PIECE")
    cards(d, 20, [("LENGTH", "64", "BAR"), ("ENERGY", "Arc", ""), ("ENTRIES", "Stagger", ""), ("END", "Loop", "")],
          sel=1)
    gx0, gx1, gy0, gy1 = 8, 232, 62, 196
    d.rounded_rectangle([4, 58, 235, 202], 5, fill=SURF)
    for k, letter in enumerate("ABAC"):          # FORM's sections across the piece
        x = gx0 + k * (gx1 - gx0) / 4
        d.line([(x, 66), (x, 192)], fill=RAISE)
        text(d, (x + 4, 66), letter, 9, 700, MID)
    pts = []                                      # the Energy curve: an arc, low at both ends
    for i in range(65):
        e = 0.25 + 0.7 * (1 - ((i - 40) / 40) ** 2) if i < 40 else 0.25 + 0.7 * (1 - ((i - 40) / 24) ** 2)
        pts.append((gx0 + i * (gx1 - gx0) / 64, gy1 - 50 - e * 70))
    d.line(pts, fill=ACC, width=2)
    for t, start in enumerate([0, 4, 12, 20]):    # when each track enters (ENTRIES)
        y = 170 + t * 6
        x = gx0 + start * (gx1 - gx0) / 64
        d.line([(x, y), (gx1, y)], fill=TRK[t], width=3)
    ph = gx0 + 21 * (gx1 - gx0) / 64              # the play head at bar 21
    d.line([(ph, 64), (ph, 194)], fill=TEXT)
    text(d, (ph + 3, 150), "21", 9, 600, TEXT)
    footer(d, 1, "CZ-1", "Vibraphone", "BAR 21/64")
    return im


def qr_placeholder(d, x, y, n=21, m=4):
    import random
    rnd = random.Random(7)
    d.rectangle([x - 4, y - 4, x + n * m + 3, y + n * m + 3], fill=TEXT)
    for r in range(n):
        for c in range(n):
            if rnd.random() < 0.45:
                d.rectangle([x + c * m, y + r * m, x + c * m + m - 1, y + r * m + m - 1], fill=BG)
    for fx, fy in [(0, 0), (n - 7, 0), (0, n - 7)]:      # the finder squares
        X, Y = x + fx * m, y + fy * m
        d.rectangle([X - 1, Y - 1, X + 8 * m - 1, Y + 8 * m - 1], fill=TEXT)
        d.rectangle([X, Y, X + 7 * m - 1, Y + 7 * m - 1], fill=BG)
        d.rectangle([X + m, Y + m, X + 6 * m - 1, Y + 6 * m - 1], fill=TEXT)
        d.rectangle([X + 2 * m, Y + 2 * m, X + 5 * m - 1, Y + 5 * m - 1], fill=BG)


def about():
    im, d = screen()
    header(d, 5, 4, "ABOUT")
    text(d, (120, 34), "TESSERA", 22, 800, ACC, "mm")
    text(d, (120, 54), "melodee-tess 1.1.9 · 2 cores", 10, 500, MID, "mm")
    qr_placeholder(d, 78, 70)
    text(d, (120, 166), "Scan for the manual", 10, 600, TEXT, "mm")
    text(d, (120, 180), "rastapopolix.github.io/melodee-tess", 9, 500, MID, "mm")
    text(d, (120, 200), "Based on Melodee 1.3.6 (keremimo) and", 9, 500, DIM, "mm")
    text(d, (120, 212), "Felucca (Hügelton Instruments). GPL-3.0", 9, 500, DIM, "mm")
    text(d, (120, 229), "QR code here is a placeholder", 8, 500, DIM, "mm")
    return im


def from_render(name):
    path = os.path.join(RENDERS, name + ".png")
    if not os.path.exists(path):
        sys.exit(f"{path} missing: run tests/run_tests.sh first")
    im = Image.open(path).convert("RGB")
    d = ImageDraw.Draw(im)
    d.rectangle([19, 2, 48, 15], fill=BG)            # the start for testing: 85 BPM, CZ-1 VIBRAPHONE
    text(d, (20, 3), "85", 12, 600)
    d.rectangle([0, 214, 239, 232], fill=BG)
    footer(d, 1, "CZ-1", "Vibraphone")
    return im, d


def gen_family():
    """the real EVOLVE page, its dots and footer as they read in the GEN family (6 pages, not SEQ's 10)"""
    im, d = from_render("gen_evolve")
    d.rectangle([50, 3, 150, 14], fill=BG)
    x = 56
    for i in range(6):
        if i == 1:
            d.rounded_rectangle([x, 7, x + 11, 10], 2, fill=ACC)
            x += 16
        else:
            d.ellipse([x, 7, x + 3, 10], fill=CHIP)
            x += 8
    text(d, (232, 223), "GEN 2/6", 11, 500, MID, "rm")
    return im


def help_overlay():
    """ARP held, KNOB 1 turned on EVOLVE: LOCK's help, its value left as it was"""
    im = gen_family()
    veil = Image.new("RGB", im.size, BG)
    im = Image.blend(im, veil, 0.6)
    d = ImageDraw.Draw(im)
    d.rounded_rectangle([3, 21, 52, 66], 5, outline=ACC, width=2)      # the card it explains, picked out
    d.rounded_rectangle([10, 60, 229, 206], 6, fill=SURF, outline=ACC)
    text(d, (20, 72), "LOCK", 14, 800, ACC)
    text(d, (228 - 10, 75), "60 %", 12, 600, TEXT, "ra")
    lines = ["How much of the phrase stays when", "EVOLVE mutates it.", "",
             "100 %  the phrase loops as it is", "0 %  something new every time", "",
             "Range 0-100 %. Euclid has no LOCK."]
    for i, s in enumerate(lines):
        text(d, (20, 96 + i * 13), s, 10, 500, MID if s[:1].isdigit() or s.startswith("Range") else TEXT)
    d.line([(20, 186), (219, 186)], fill=RAISE)
    text(d, (120, 196), "ARP held: turn a knob for its help", 9, 600, DIM, "mm")
    return im


def sheet(images, captions):
    s = 2
    W = 240 * s
    out = Image.new("RGB", (len(images) * (W + 20) + 20, W + 70), (28, 28, 34))
    d = ImageDraw.Draw(out)
    for i, (im, cap) in enumerate(zip(images, captions)):
        x = 20 + i * (W + 20)
        out.paste(im.resize((W, W), Image.NEAREST), (x, 20))
        text(d, (x, W + 34), cap, 18, 600, TEXT)
    return out


def main():
    made = [("gen_evolve", gen_family(), "GEN: EVOLVE, page 2 of 6"),
            ("help", help_overlay(), "Help: ARP held + KNOB 1"),
            ("cords", cords(), "CORDS (LFO family)"),
            ("normals", normals(), "NORMALS (LFO family)"),
            ("piece", piece(), "PIECE (GLO family)"),
            ("about", about(), "ABOUT (GLO family)")]
    for name, im, _ in made:
        im.save(os.path.join(OUT, name + ".png"))
    sheet([m[1] for m in made], [m[2] for m in made]).save(os.path.join(OUT, "sheet.png"))
    print("wrote", ", ".join(m[0] for m in made), "and sheet.png to", os.path.relpath(OUT, ROOT))


if __name__ == "__main__":
    main()
