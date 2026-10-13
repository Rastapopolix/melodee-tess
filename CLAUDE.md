# CLAUDE.md

melodee-tess: a fork of Melodee (FM-1 alternative firmware, GPL-3.0) where the Tessera
generators (Turing, Euclid, Markov, Morph; `firmware/src/tessera.c`, `tessera.h`) were built.
It becomes the Tessera firmware, a hard fork. Plan, phases and tasks: `ROADMAP.md`.

## Start and end of each session

Read `ROADMAP.md` first. At the end, update it (tick tasks, record decisions) and commit.

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

- JieLi AC79 at 360 MHz. CPU1 is optional and retires itself on units that misread SRAM:
  everything must work single-core.
- Audio at 44.1 kHz. Modulation runs at control rate (once per audio block).
- The build requires at least 152 KiB of audio working arena; it reports the free arena and
  fails below the minimum. Watch RAM and flash headroom.

## Rules

- Follow the existing code conventions: the unity build (`melodee.c` and `tests/hostsim.c`
  `#include` the `.c` files), C style, naming, comment density. New sources go in both.
- Prefer new files over heavy rewrites of shared upstream files, to keep upstream syncing easy.
- All hardware access stays behind the platform layer (`firmware/hal/`), so generator,
  sequencer, mod and UI code compiles for host tests and the web simulator.
- Every automatic layer (Energy, Entries, Wander, Evolve, Gravity, Piece Length) has an
  off/zero position with no effect. Automation is opt-in.
- Every parameter has a central ID and range. Fixed maximums for cords, sources, scopes, voices.
- Flag risky areas before changing them: bootloader, flash layout, update/install, dual-core start.
- Run `tests/run_tests.sh` after every change. Report failures; do not hide them.
- Small commits, one change each, on a feature branch. Leave `main` untouched unless asked.
- Never release a `MELODEE_CORE1_TEST` build (fault-injection test firmware).
- Keep GPL-3.0 headers and the upstream attribution (Melodee, Felucca).
- Do not commit build output, `__pycache__/` or backup files (`*.bak*`, `*.pre*`).
