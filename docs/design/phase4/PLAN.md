# Phase 4 plan: retiring engines, making room for Tessera's voices

Status (2026-10-16): Theo decided: retire FM6, CZ-1 and Prophet; keep SID; a retired engine's track loads
SID's INIT; the new voices both Tessera's own and a Plaits port. Nothing below is done yet.

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

## Measured with the switches (step 1 done, 2026-10-16, branch `engine-switches`)

`MELODEE_FM6`, `MELODEE_CZ`, `MELODEE_PROPHET` = 0 (build.py passes them): each built for the device. All on, the
image is byte for byte the size it was (525,548 B).

| Left out | Image | Flash freed | Static RAM freed | Audio arena freed |
| --- | --- | --- | --- | --- |
| FM6 | 495,320 | 30.2 KB | 9.5 KB | 8.2 KB |
| Prophet | 500,192 | 25.4 KB | 0.3 KB | 2.8 KB |
| CZ-1 | 511,884 | 13.7 KB | 0 | 0 |
| all three | 455,900 | **69.6 KB** | **13.9 KB** | **11.0 KB** |

So with all three out, **117.5 KB of app flash is free** (47.9 today + 69.6) and 18.2 KB of static RAM. A little
more is still reachable from the UI and the editor (each engine's pages, its editor commands); it can be taken out
later, round by round (`whorefs.py`-style: the disassembly's references walked back to the first non-engine
caller, a guard there).

How: an engine left out is never offered (`eng_ok`, `ENGINE_ORDER`, `NENG_SHOWN`), its table entry is
`ENG_RETIRED`, and the functions that reach its big parts return at once (`if (!MELODEE_FM6) ...` at the top of its
factory reader, its SysEx, its banks, its pages in `page_desc`; the generated FM6 RAM tables skipped). Everything
still compiles, so a guard missed costs flash, never a broken build. `tests/engine_switch_test.c` (all three off):
never offered, the power-on sounds built ones, a project's track of them SID INIT with its common settings, the
parts play.

## The plan

1. **Build switches, nothing removed** (done, above). `MELODEE_FM6`, `MELODEE_CZ` and `MELODEE_SID` beside the existing
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

## Decisions (Theo, 2026-10-16)

1. Retire FM6, CZ-1 and Prophet: yes (once the new voices are ready).
2. SID: kept.
3. A retired engine's track in an old project loads as SID's INIT (later: the nearest new voice).
4. The new voices: both Tessera's own and a Plaits port.

## Plaits / Braids (Mutable Instruments)

Both are open source (the code is MIT, compatible with GPL-3.0; the attribution kept).

- **Plaits** (2016–2020): 16 models (24 in the later firmware): virtual analog, waveshaping, FM, grains,
  additive, wavetable, chords, speech, swarm, noise, particles, string, modal, and three drums; one voice with
  HARMONICS, TIMBRE and MORPH and a built-in low-pass gate. Float DSP at 48 kHz in small blocks, written for a
  72 MHz Cortex-M4F with an FPU. The FM-1's AC79 (360 MHz, fp32) should run several voices; how many is a
  device measurement.
- **Braids** (2012): 30+ models, integer DSP for a smaller chip; lighter but older-sounding and aliasing more.
  Plaits is the better fit.
- **Measured** (2026-10-16, `tools/plaits_measure`): Plaits (eurorack f4cfa76, stmlib d18def8) compiles for
  the AC79 with the JieLi clang as C++ (no exceptions, no RTTI, a 20-line stand-in for `<algorithm>`), and
  links with unused code dropped. Flash and static RAM, one voice:

  | Plaits models | Flash | RAM |
  | --- | --- | --- |
  | all 24 (the whole voice, its LPG and post-processing) | 155 KB | 25 KB |
  | 17 without the wavetables, speech and 6-OP (VA, VA VCF, waveshaping, FM, phase distortion, grain, additive, string machine, chiptune, swarm, noise, particle, string, modal, the three drums) | 57 KB | 20 KB |
  | those and the three that share the wavetables (wavetable, chords, wave terrain) | 113 KB | 20 KB |
  | the planned four voices: VA VCF, waveshaping, string, modal, chords, the three drums | 86 KB | 19 KB |
  | each alone (with what it shares) | 4–8 KB most; 6-OP 14; speech 24; wavetable, chords, terrain 55–57 | 17–21 KB |

  The big tables: the wavetables 51 KB (wavetable, chords and wave terrain share them), the 6-OP's three DX7
  banks 12 KB, the speech data ~11 KB. The RAM is mostly the 16 KB working buffer each voice has; it can come
  from the audio arena (157 KB free there) rather than static RAM.
- **CPU, on this computer only** (ns per sample of one voice, the same machine and compiler level; the ratios
  are what count): the FM-1's SID 23, CZ-1 31, FM6 33, Prophet 54; Plaits' models 16 (speech) to 114 (FM),
  most 55–105. So **one Plaits voice costs about one to two Prophet voices** (3 of the 16 voice-budget units
  each): a few Plaits voices across the tracks, not four-voice chords on every track. To measure on the device.
- **What it brings back:** its 6-OP plays DX7 patches (3 banks of 32) and its phase distortion is CZ-like, so a
  little of FM6's and CZ-1's character returns in 22 KB.
- **Work:** it is C++ (the firmware is C): shown to compile; the build needs a C++ step and the link of its
  objects beside `melodee.o`. Then an engine wrapper: MODEL, HARMONICS, TIMBRE, MORPH and the
  LPG's decay and colour as the engine's settings, so MODS and help reach them all; 44.1 kHz in place of 48;
  the voice budget. A first prototype with a few models is a few sessions of work.
- **Fit:** its models cover the planned pad (chords, wavetable), LPG pluck (its LPG), lead (virtual analog,
  waveshaping) and percussion (its drums), and HARMONICS / TIMBRE / MORPH are made for modulation, which suits
  MODS and the generators well.
