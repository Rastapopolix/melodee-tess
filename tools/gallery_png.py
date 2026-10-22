#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Theo Dainis (melodee-tess)
"""The gallery's PNGs: tests/gallery_render.c's PPMs (240 x 240) at 2x, nearest, into assets/gallery.

  cc -O1 -w -DMELODEE_USB_AUDIO=1 -DMELODEE_SID=0 -DMELODEE_PHYS=0 '-DMELODEE_VERSION="v2.0"' \\
     -Ibuild/gen -Ifirmware/src -Itests \\
     -o build/gallery_render tests/gallery_render.c -lm
  mkdir -p build/gallery/ppm && build/gallery_render build/gallery && python3 tools/gallery_png.py build/gallery/ppm
"""
import sys
from pathlib import Path
from PIL import Image

src = Path(sys.argv[1] if len(sys.argv) > 1 else "build/gallery/ppm")
out = Path(__file__).resolve().parent.parent / "assets/gallery"
n = 0
for ppm in sorted(src.glob("*.ppm")):
    name = ppm.stem.split("_", 1)[1] if ppm.stem.startswith(("SMALTI_", "smalti_")) else ppm.stem
    im = Image.open(ppm).convert("RGB")
    im.resize((im.width * 2, im.height * 2), Image.NEAREST).save(out / f"{name}.png", optimize=True)
    n += 1
print(f"gallery_png: {n} PNGs into {out}")
