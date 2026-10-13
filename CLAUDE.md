# CLAUDE.md

melodee-tess: a fork of Melodee (FM-1 alternative firmware, GPL-3.0) that adds the Tessera
generative sequencing engines (`firmware/src/tessera.c`, `tessera.h`).

## Start of each session

Read `ROADMAP.md` first: it holds the current goals and what to work on next.

## Build

```
./build.sh                       # build/melodee.bin, build/loader/ota.bin, build/melodee-tess.fwsc
./build.sh --release X.Y[.Z]     # release package and build/release-X.Y/
MELODEE_DUAL_CORE=0 ./build.sh   # serial (one-core) variant
```

WSL Ubuntu, Linux x86-64: the JieLi toolchain runs natively (`~/.jieli/toolchain`), no Docker.
The AC79 SDK is at `~/fw-AC79_AIoT_SDK`. Build options are listed in `BUILDING.md`.

## Test

```
tests/run_tests.sh                       # host tests; run after ./build.sh
GOLDEN_UPDATE=1 sh tests/run_tests.sh    # only after an intended change of the sound
BUDGET_UPDATE=1 sh tests/run_tests.sh    # only after an intended change of the CPU cost
```

The suite renders every engine against `tests/golden.txt`, checks CPU cost, and renders the UI.
Generators can be developed and tested on the PC; flash the device only to check how it plays.

## Hardware constraints

- JieLi AC79 at 360 MHz; CPU1 is optional and retires itself on units that misread SRAM.
- Audio at 44.1 kHz. Keep DSP and modulation work bounded per audio block.
- The build requires at least 152 KiB of audio working arena (engine state, recording,
  effects); it reports the free arena and fails below the minimum. Watch RAM and flash headroom.

## Rules

- Follow the existing code conventions: the unity build (`melodee.c` and `tests/hostsim.c`
  `#include` the `.c` files), C style, naming, comment density. New sources go in both.
- Run `tests/run_tests.sh` after every change. Report failures; do not hide them.
- Small commits, one change each, on a feature branch. Leave `main` untouched unless asked.
- Never release a `MELODEE_CORE1_TEST` build (fault-injection test firmware).
- Keep GPL-3.0 headers and the upstream attribution (Melodee, Felucca).
- Do not commit build output, `__pycache__/` or backup files (`*.bak*`, `*.pre*`).
