#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Web simulator: the firmware to WebAssembly (web/sim/sim.c) and the page into build/sim/ (index.html, worklet.js:
# Felucca's, hugelton/Felucca web/emu).
#   web/sim/build.sh        (after ./build.sh, which generates build/gen/)
# Needs Emscripten (emcc): on the PATH, or emsdk in ~/emsdk (or $EMSDK).
# Serve build/sim/ over http(s) (python3 -m http.server -d build/sim) and open it: AudioWorklet needs a server.
set -e
cd "$(dirname "$0")/../.."
command -v emcc >/dev/null 2>&1 || PATH="${EMSDK:-$HOME/emsdk}/upstream/emscripten:$PATH"   # emsdk, not activated
command -v emcc >/dev/null 2>&1 || { echo "emcc not found: install Emscripten (emsdk)"; exit 1; }
[ -f build/gen/melodee_tables.h ] || { echo "run ./build.sh first (build/gen/)"; exit 1; }
OUT=build/sim
mkdir -p "$OUT"
# as Felucca's web/emu/build.sh: no fused multiply-adds (as the host build), a 1 MiB stack, no file system
VERSION=$(sed -n 's/^#define MELODEE_VERSION "\([^"]*\)".*/\1/p' firmware/src/melodee.c)   # upstream's, shown in the menu
emcc -O2 -ffp-contract=off -w -DMELODEE_USB_AUDIO=1 "-DMELODEE_VERSION=\"$VERSION web\"" -Ibuild/gen -Ifirmware/src -Itests \
    --no-entry -sSTANDALONE_WASM -sSTACK_SIZE=1048576 -sINITIAL_MEMORY=33554432 -sFILESYSTEM=0 \
    -o "$OUT/melodee.wasm" web/sim/sim.c
cp web/sim/worklet.js "$OUT/"
{   # index.html is written as a page body (it also publishes as is): the document around it for a plain server
    printf '<!doctype html>\n<html lang="en">\n<head>\n<meta charset="utf-8">\n'
    printf '<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">\n</head>\n<body>\n'
    sed "s/@BASE@/$VERSION/" web/sim/index.html
    printf '</body>\n</html>\n'
} > "$OUT/index.html"
echo "sim      $OUT/melodee.wasm  $(wc -c < "$OUT/melodee.wasm") B   (serve $OUT/ and open index.html)"
