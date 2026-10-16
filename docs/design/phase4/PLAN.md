# Phase 4 plan: retiring engines, making room for Tessera's voices

Status: a plan for Theo to decide on (2026-10-16). Nothing below is done yet.

## Measurements (tessera-dev, 9c6da85)

The app area in flash is 573,440 B (`firmware/app.ld`, XIP). The image is 525,548 B, so **47.9 KB is free
now**. Static RAM (`.data` + `.bss`): 93,964 of 98,304 B (4.3 KB free). The audio arena (pool): 187,392 of
344,064 B used.

**Lower bound, measured:** the engine taken out of the engine table only (`engines.c`, its ID kept reserved),
each built for the device. What the UI, the editor and the stores still reach stays in, so these are the least
each frees:

| Taken out | Image | Flash freed | Static RAM freed |
| --- | --- | --- | --- |
| (none) | 525,548 | | |
| FM6 | 500,632 | 24.9 KB | 9.4 KB |
| Prophet | 512,848 | 12.7 KB | 0.3 KB |
| SID | 514,604 | 10.9 KB | 0 |
| CZ-1 | 521,324 | 4.2 KB | 0 |
| FM6 + CZ-1 + Prophet | 484,188 | 41.4 KB | 9.7 KB |

**Upper bound, estimated:** the symbols in `build/melodee.elf` attributed to their source files (an engine's
engine, patch, bank, store, edit and editor files, its tables). Named symbols only, so rough:

| Engine | Flash, all its files | Of it, factory sounds |
| --- | --- | --- |
| FM6 | ~40 KB | ROM voices, tables ~10 KB |
| Prophet | ~35 KB | `P5_FACTORY` ~16 KB (bit-packed by upstream) |
| CZ-1 | ~20 KB | `CZ_FACTORY` ~5 KB (LZ) |
| SID | ~11 KB | its waves ~4 KB |
| DRUM (kept) | ~33 KB | the 909's tables |

So retiring FM6, CZ-1 and Prophet whole should free **about 80–95 KB of flash** (with today's 48 KB:
roughly 130–140 KB for Tessera's voices and FX) and **about 10–15 KB of static RAM**.

**CPU:** the heaviest renders are FM6's (`fm6_render`, the largest loop in `tests/target_budget.txt`) and the
Prophet's (a Prophet voice costs 3 of the 16 voice-budget units). CPU1 exists mostly for their kernels. The
device's real per-engine CPU is measured on the device (the editor's CPU meter): to do when the first new
voice exists, against the same mix.

**Flash storage (not the app area):** each engine has its own banks in flash, which retiring it leaves unused:
Prophet's 5 banks (40 KB at 0x93000, right after the app area), FM6's bank and native banks, CZ-1's 8 banks,
the FM6 user extension. They are not freed or reused in this phase.

## The plan

1. **Build switches, nothing removed.** `MELODEE_FM6`, `MELODEE_CZ` and `MELODEE_SID` beside the existing
   `MELODEE_PROPHET` (and `MELODEE_SLICE`), each taking out the engine with its UI pages, editor commands,
   stores and tests. Default: all on, so the firmware is as today. Each switch off is built and the suite run
   (the engine's own tests skipped). This gives the exact freed flash, RAM and CPU per engine.
2. **Engine IDs stay reserved** (`ENG_RETIRED`), as upstream does for every engine it retired: a project with a
   retired engine's track still loads. That track gets a kept engine's INIT sound (decision 3 below).
3. **Flash storage untouched.** The retired engines' bank objects keep their numbers and places, only no
   longer read or written. No sector moves, no format changes: the risky part (the flash layout) is left for
   a separate step with its own plan, if the space is wanted later (the Prophet banks next to the app area
   could let the app area grow by 40 KB, which needs the update loader to handle it).
4. **New voices first, then switch the old ones off.** With only SID and DRUM left, the FM-1 would be poorer
   until the new voices exist. The 48 KB free today is enough to prototype the first voice; the old engines
   are switched off in the release that brings the new ones.
5. **The code stays in the repo** behind its switch. Deleting files is a later, separate decision.
6. **The web editor and installer** hide what a build leaves out (the package says which engines it has).

## Bringing an engine back

With steps 1–5, bringing an engine back is turning its switch on again, if there is room: its code, stores
and tests are all still there, and its saved banks are still in flash where it left them. What would need
work: making its settings mod destinations (MODS reaches 8 of its settings today, as now) and help text for
its own panels. It gets harder only if its files are deleted or its flash banks reused, which the plan does
not do.

## Decisions for Theo

1. Retire FM6, CZ-1 and Prophet, as the ROADMAP planned? (Recommended, once the new voices are ready.)
2. Keep SID? (11 KB, light on CPU, a distinct sound. Recommended: keep for now, decide with the new voices.)
3. A retired engine's track in an old project loads as: SID's INIT (recommended until the new voices), later
   the nearest new voice.
4. The new voices: Tessera's own (pad, LPG pluck, lead, percussion: ROADMAP), or a port of Mutable
   Instruments' Plaits (see below), or both.

## Plaits / Braids (Mutable Instruments)

Both are open source (the code is MIT, compatible with GPL-3.0; the attribution kept).

- **Plaits** (2016–2020): 16 models (24 in the later firmware): virtual analog, waveshaping, FM, grains,
  additive, wavetable, chords, speech, swarm, noise, particles, string, modal, and three drums; one voice with
  HARMONICS, TIMBRE and MORPH and a built-in low-pass gate. Float DSP at 48 kHz in small blocks, written for a
  72 MHz Cortex-M4F with an FPU. The FM-1's AC79 (360 MHz, fp32) should run several voices; how many is a
  device measurement.
- **Braids** (2012): 30+ models, integer DSP for a smaller chip; lighter but older-sounding and aliasing more.
  Plaits is the better fit.
- **Space:** Plaits' firmware fits in a 256 KB chip with its UI. All of its models, with the wavetables and the
  speech data, are likely **about 100–150 KB**; a subset (virtual analog, waveshaping, FM, wavetable, chords,
  string, modal, the drums) much less. **RAM:** each voice wants a working buffer (about 16 KB in Plaits),
  from the audio arena (157 KB free there). To measure before deciding: compile Plaits' DSP for the AC79 with
  this toolchain and read the sizes.
- **Work:** it is C++ (the firmware is C); the toolchain is clang, so it can likely be compiled as C++ without
  exceptions or RTTI (Plaits needs no heap). Then an engine wrapper: MODEL, HARMONICS, TIMBRE, MORPH and the
  LPG's decay and colour as the engine's settings, so MODS and help reach them all; 44.1 kHz in place of 48;
  the voice budget. A first prototype with a few models is a few sessions of work.
- **Fit:** its models cover the planned pad (chords, wavetable), LPG pluck (its LPG), lead (virtual analog,
  waveshaping) and percussion (its drums), and HARMONICS / TIMBRE / MORPH are made for modulation, which suits
  MODS and the generators well.
