# Roadmap

## Vision

A generative sequencing playground for the FM-1: "modular in a box". Inspired by Marbles,
Turing Machine, Bloom 2, Proteus, o_c, OXI and norns scripts.

The core idea: **sequencers are modulation, modulation is sequencing.** Every source (generator,
LFO, clock, logic, envelope follower) outputs a gate stream and a CV stream. Any source can be patched to
any destination, including other generators' parameters. A normalled default patch sounds good
with no patching; cords override it. Voices stay simple but full of character. Modulation is shown
live on the parameters it moves.

This repo (melodee-tess) is the Melodee fork where the Tessera engines were built. The new firmware
will be a hard fork in its own repo (`~/src/tessera`). It keeps Melodee's platform layer (audio,
flash, MIDI, USB audio, UI renderer, web installer and editor, dual core) and replaces the
application layer. GPL-3.0 and the upstream attribution stay.

## Phases

1. **Foundation**: a clean repo, a passing build and test suite, a working Claude Code setup.
2. **Extract Tessera**: generators behind a clean interface with no UI or Melodee knowledge,
   with host tests.
3. **Hard fork**: new repo; strip the heavy engines; a project format of its own.
4. **Patching and modulation**: sources, cords, the default patch, live visual feedback.
5. **Generators and clock**: new generators, a patchable clock, a quantizer, utilities.
6. **Voices and FX**: simple voices through low-pass gates, plus character effects.

Open question: whether to do phase 4 before phase 5, or the other way round. Decide before
starting phase 4.

## Tasks

### 1. Foundation
- [x] Remove the stray root files (shell-paste leftovers, tessera-updates.diff)
- [x] `./build.sh` passes in WSL
- [x] Host tests compile again (hostsim includes Tessera, package name, flash guard, CZ-1 type)
- [x] CLAUDE.md
- [ ] Fix the perform test: FX black key 1 leaves track 1 at peak 318 (limit 200)
- [ ] `tests/run_tests.sh` passes end to end
- [ ] Stop tracking `__pycache__/`; add it and the `*.bak*` / `*.pre*` backups to `.gitignore`
- [ ] Delete the backup files in `firmware/src` and `tools/` once nothing in them is needed
- [ ] Decide what happens to the `master`, `tools-util` and `cleanup-claude-md` branches

### 2. Extract Tessera
- [ ] Define the interface, for example `gen_tick(state, clock) -> events + cv`
- [ ] Move Turing, Euclid, Markov and Morph behind it, out of seq.c / project.c / UI coupling
- [ ] Host tests per generator (deterministic output for a seed, CPU cost)

### 3. Hard fork
- [ ] Create the `tessera` repo from this one, with GPL-3.0 and the attribution kept
- [ ] Strip Prophet, FM6, CZ banks, SLICE and samples; check the freed flash, RAM and CPU
- [ ] A project format of its own; drop the OBJ_TESSERA side object
- [ ] Rename identities (package, USB, menu, web installer and editor)

### 4. Patching and modulation
- [ ] Every source outputs gate + CV; a fixed set of cords (about 16) at control rate,
      once per audio block
- [ ] Extend Melodee's modulation matrix rather than start again
- [ ] Normalled default patch
- [ ] Live visual feedback of modulation on parameters
- [ ] Logic and utilities: AND/XOR, flip-flop, S&H, comparator, slew, Maths-style rise/fall
- [ ] Audio envelope follower as a source

### 5. Generators and clock
- [ ] Marbles-style clock: jitter, Bernoulli gates, deja vu, ratchets and bursts; clock as a signal
- [ ] Quantizer with weighted scale degrees (spread / bias / steps) and learn-from-playing
- [ ] Kria-style polymetric lanes (note, trigger, octave, gate)
- [ ] 1D cellular automata, Meadowphysics cascading counters, Bloom-style branching mutations
- [ ] "Keep": freeze the last N generated notes into an editable STEP pattern

### 6. Voices and FX
- [ ] Voices: VA, 2-op FM, wavefolder, Karplus pluck, noise drums, each through an LPG
- [ ] FX: clock-synced tape delay with wow, the inherited reverb, bitcrusher, ring mod,
      comb resonator

## Constraints

AC79 at 360 MHz, 44.1 kHz audio, at least 152 KiB of audio working arena (see CLAUDE.md).
Modulation runs at control rate with a fixed number of cords to keep headroom.
