#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Web simulator: the firmware to WebAssembly (web/sim/sim.c) and the page into build/sim/.
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
FN=_sim_init,_sim_demo,_sim_render,_sim_frame,_sim_screen,_sim_leds,_sim_button,_sim_key,_sim_turn,_sim_midi,_sim_button_id
emcc -O2 -w -DMELODEE_USB_AUDIO=1 '-DMELODEE_VERSION="v1.1.0 web"' -Ibuild/gen -Ifirmware/src -Itests web/sim/sim.c -o "$OUT/melodee.wasm" \
    --no-entry -sSTANDALONE_WASM -sINITIAL_MEMORY=33554432 -sEXPORTED_FUNCTIONS=$FN
cp web/sim/sim-worklet.js "$OUT/"
{   # index.html is written as a page body (it also publishes as is): the document around it for a plain server
    printf '<!doctype html>\n<html lang="en">\n<head>\n<meta charset="utf-8">\n'
    printf '<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">\n</head>\n<body>\n'
    cat web/sim/index.html
    printf '</body>\n</html>\n'
} > "$OUT/index.html"
echo "sim      $OUT/melodee.wasm  $(wc -c < "$OUT/melodee.wasm") B   (serve $OUT/ and open index.html)"
