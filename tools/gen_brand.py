#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Tessera's brand pictures for GitHub and the project page, in the SMALTI palette with Chakra Petch:

  assets/brand/logo.svg            the mark: 3 x 3 lapis tiles, the gold tessera top right (firmware/src/splash.c)
  assets/brand/banner.png          README header, 1280 x 400: the mark, TESSERA, the line, three screens
  assets/brand/social-preview.png  GitHub's social preview (Settings > General), 1280 x 640
  assets/brand/profile.png         a profile picture (the Firmware Hub, forums), 1080 x 1080: the mark, TESSERA, the
                                   line and the four tracks' colours, no screens and no version
  assets/brand/profile-wide.png    the same, 1280 x 640

  python3 tools/gen_brand.py [assets/gallery]   (the screens: tests/gallery_render.c's PNGs)

The mark's colours are the splash's (SPL_LAPIS, SPL_GOLD_C), the rest gen_ui_palettes.py's SMALTI."""
import sys
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
FONTS = ROOT / "assets/fonts"
OUT = ROOT / "assets/brand"
BG, SURF, TEXT, ACCENT = (13, 19, 38), (24, 33, 60), (246, 240, 224), (255, 226, 150)
TRACKS = ((222, 202, 96), (64, 200, 214), (110, 214, 104), (214, 124, 236))
LAPIS = ((44, 64, 120), (58, 84, 150), None, (66, 96, 170), (50, 74, 136),
         (40, 60, 112), (62, 90, 160), (46, 68, 126), (54, 80, 146))
GOLD = (236, 184, 72)
JIT = ((0, 1), (1, -1), (-1, 0), (1, 0), (0, 0), (-1, 1), (0, -1), (1, 1), (-1, -1))   # splash.c SPL_JIT
TAGLINE = "GENERATIVE FIRMWARE FOR THE M-VAVE FM-1"


def mix(a, b, p):
    return tuple(round(x + (y - x) * p / 100) for x, y in zip(a, b))


MID, LINE = mix(BG, TEXT, 70), mix(BG, TEXT, 18)


def font(weight, size):
    return ImageFont.truetype(str(FONTS / f"ChakraPetch-{weight}.ttf"), size)


def mark(d, cx, cy, u):
    """the mosaic centred on cx, cy, tiles u px, grout u/5.5 (splash.c: 22 and 4), each a jitter off the grid"""
    g, j = round(u * 4 / 22), max(1, round(u / 22))
    for i in range(9):
        x = cx - u // 2 + (i % 3 - 1) * (u + g) + JIT[i][0] * j
        y = cy - u // 2 + (i // 3 - 1) * (u + g) + JIT[i][1] * j
        d.rectangle((x, y, x + u - 1, y + u - 1), fill=LAPIS[i] or GOLD)


def logo_svg():
    u, g = 22, 4
    rects = []
    for i in range(9):
        x = 2 + (i % 3) * (u + g) + JIT[i][0]
        y = 2 + (i // 3) * (u + g) + JIT[i][1]
        rects.append(f'<rect x="{x}" y="{y}" width="{u}" height="{u}" fill="#{(LAPIS[i] or GOLD)[0]:02x}'
                     f'{(LAPIS[i] or GOLD)[1]:02x}{(LAPIS[i] or GOLD)[2]:02x}"/>')
    size = 3 * u + 2 * g + 4
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {size} {size}" width="{size}" height="{size}">'
            f'<title>Tessera</title>{"".join(rects)}</svg>\n')


def screens(gallery, names, h):
    out = []
    for n in names:
        im = Image.open(gallery / f"{n}.png").convert("RGB")
        out.append(im.resize((h, h), Image.NEAREST if h % 240 == 0 else Image.LANCZOS))
    return out


def tile(img, shot, x, y, frame=6):
    d = ImageDraw.Draw(img)
    d.rectangle((x - frame, y - frame, x + shot.width + frame - 1, y + shot.height + frame - 1), fill=SURF)
    img.paste(shot, (x, y))


def tracks_bar(d, x, y, w, h):
    seg = (w - 3 * h) // 4
    for k, c in enumerate(TRACKS):
        d.rectangle((x + k * (seg + h), y, x + k * (seg + h) + seg - 1, y + h - 1), fill=c)


def wordmark(d, x, y, size, sub):
    d.text((x, y), "TESSERA", font=font("SemiBold", size), fill=TEXT)
    d.text((x + 2, y + round(size * 1.18)), TAGLINE, font=font("Medium", sub), fill=MID)


def banner(gallery):
    w, h = 1280, 400
    img = Image.new("RGB", (w, h), BG)
    d = ImageDraw.Draw(img)
    mark(d, 124, 200, 44)
    wordmark(d, 222, 140, 84, 19)
    tracks_bar(d, 224, 282, 440, 8)
    shots = screens(gallery, ("gen_quad", "gen_chord"), 240)
    for k, s in enumerate(shots):
        tile(img, s, 766 + k * 258, 80)
    return img


def social(gallery):
    w, h = 1280, 640
    img = Image.new("RGB", (w, h), BG)
    d = ImageDraw.Draw(img)
    mark(d, 152, 168, 50)
    d.text((262, 104), "TESSERA", font=font("SemiBold", 96), fill=TEXT)
    d.text((266, 218), TAGLINE, font=font("Medium", 24), fill=MID)
    shots = screens(gallery, ("gen_constel", "gen_grids", "gen_chord", "matrix"), 240)
    x0 = (w - (4 * 240 + 3 * 28)) // 2
    for k, s in enumerate(shots):
        tile(img, s, x0 + k * 268, 330)
    tracks_bar(d, x0, 600, 4 * 240 + 3 * 28, 6)
    return img


def centred(d, w, y, text, f, fill):
    d.text(((w - d.textlength(text, font=f)) / 2, y), text, font=f, fill=fill)


def profile(w, h):
    """the mark over TESSERA, the line and the tracks' colours, centred"""
    img = Image.new("RGB", (w, h), BG)
    d = ImageDraw.Draw(img)
    k = min(w, h) / 1080                              # (1080 square: the sizes below)
    u = round(92 * k)
    mark(d, w // 2, round(h * 0.30), u)
    name, line = font("SemiBold", round(176 * k)), font("Medium", round(37 * k))
    centred(d, w, round(h * 0.30 + 1.9 * u), "TESSERA", name, TEXT)
    y = round(h * 0.30 + 1.9 * u + 176 * k * 1.22)
    centred(d, w, y, TAGLINE, line, MID)
    bw = round(d.textlength(TAGLINE, font=line))
    tracks_bar(d, (w - bw) // 2, y + round(80 * k), bw, max(6, round(10 * k)))
    box = ImageChops.difference(img, Image.new("RGB", (w, h), BG)).getbbox()   # then the whole group centred
    out = Image.new("RGB", (w, h), BG)
    out.paste(img.crop((0, box[1], w, box[3])), (0, (h - (box[3] - box[1])) // 2))
    return out


def main():
    gallery = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "assets/gallery"
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "logo.svg").write_text(logo_svg(), encoding="utf-8")
    banner(gallery).save(OUT / "banner.png", optimize=True)
    social(gallery).save(OUT / "social-preview.png", optimize=True)
    profile(1080, 1080).save(OUT / "profile.png", optimize=True)
    profile(1280, 640).save(OUT / "profile-wide.png", optimize=True)
    print(f"brand: {OUT}: logo.svg, banner.png, social-preview.png, profile.png, profile-wide.png")


if __name__ == "__main__":
    main()
