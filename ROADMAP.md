# Roadmap

## Name

The firmware is called **Tessera**. Its generative engines are called **generators** as a group,
each with its own name (Turing, Euclid, Markov, Morph, ...).

This repo (melodee-tess) is the Melodee fork where the generators were first built. Tessera will
be a hard fork in its own repo (`~/src/tessera`).

## Vision

A generative sequencing playground for the FM-1: "modular in a box". Standalone first, and
satisfying as an instrument on its own. Inspired by Marbles, Turing Machine, Bloom 2, Proteus,
o_c, OXI and norns scripts.

- Maximise happy accidents while the player still feels they authored the result. Rule-based
  and seed-driven, not AI.
- **Sequencers are modulation, modulation is sequencing.** Every source outputs a gate stream
  and a CV value; any source can be patched to any destination, including other generators'
  parameters. Modulation is shown live on the parameters it moves.
- Tessera can build complete, non-repeating pieces without manual arranging, but every
  automatic layer is opt-in.
- Not included for now: a song-mode arranger, sample playback, deep synth editing.

## Design rules

These are repeated in short form in CLAUDE.md.

- Every automatic layer (Energy, Entries, Wander, Evolve, Gravity, the Piece Length arc) has a
  zero/off position with no effect. Automation is opt-in.
- Every parameter has a central ID and range, used by the mod bus, the performance page and
  macros.
- All hardware access sits behind the platform layer (`firmware/hal/`), so generator,
  sequencer, mod and UI code compiles for the host tests and a future web simulator.
- Fixed maximums for cords, sources, scopes and voices, so the worst case is known. Modulation
  runs at control rate (once per audio block).
- Follow the existing code conventions. Prefer adding new files over heavily rewriting shared
  upstream files, to keep upstream syncing easy.
- Run `tests/run_tests.sh` after every change. Small commits.
- Never release a `MELODEE_CORE1_TEST` build.
- Flag risky areas clearly before changing them: bootloader, flash layout, the update/install
  process, dual-core startup.
- Read this file at the start of each session; update it and commit at the end.

## Architecture

- **Hard fork.** Keep Melodee's platform layer: audio, flash, MIDI, USB audio, the UI renderer,
  the web installer and editor, the tests. Remove Prophet, FM6, CZ, SLICE and samples to free
  flash, RAM and CPU. GPL-3.0 and the upstream attribution stay.
- **Project format of our own.** Drop the `OBJ_TESSERA` side object.
- **Generator interface:** `gen_tick(state, input_phrase, clock)`; the input phrase may be empty.
- **Signal bus.** Every source outputs a gate stream and a CV value. A fixed destination table.
  N cords (about 16), each with source, destination, amount and offset. Extend Melodee's existing
  mod matrix (`mod.c`) rather than writing a parallel one.
- **Normalled default patch** that sounds good with no patching; cords override the normals.
- **Tracks:** 4 audio tracks, plus 2–4 sequencer-only tracks (MIDI out / modulation only).
- **Single core first.** The second CPU core is an optional bonus, since it misbehaves on some
  units; everything must work on one core.

## Features

### STEP sequencer as seed

STEP patterns can feed generators, which then act as processors on the player's melody:

- Turing loads the melody into its register and drifts it.
- Markov learns transitions from it.
- Euclid re-rhythms it.
- Morph blends the original and the mutated version.
- New **Ornament** generator: passing and neighbour notes, octave jumps, ratchets.
- New **Serial** generator: retrograde, inversion, rotation, augmentation.

Rules:
- Per-step locks, and lane locks (pitch vs rhythm). Gravity pulls back toward the original.
- Non-destructive: the original is never overwritten. **Keep** commits the current version to a
  new STEP pattern.
- Form uses the player's melody as section A.
- The grid shows the original as ghost notes and the current version solid, plus a
  distance-from-home meter.

### Mod sources and sequencing tools

- LFOs; AD/ADSR envelopes triggerable from any gate; logic (AND/OR/XOR, flip-flop, comparator);
  sample & hold; slew; Maths-style rise/fall; chaotic sources (Lorenz, bouncing ball, Brownian);
  audio envelope follower; drum ducking; macros.
- Clock tools: dividers/multipliers, Marbles-style jitter, Bernoulli gates, ratchets/bursts,
  per-track swing.
- Quantizer with weighted scale degrees; learn the scale from played notes.
- Kria-style polymetric lanes (note, trigger, octave, gate, each with its own length).
- Later generators: 1D cellular automata, cascading counters, Bloom-style branching mutations.
- Later: modulatable cord amounts, probability per cord.

### Visual feedback

- The audio side writes each destination's final value to a snapshot array; the UI reads it at
  about 30 fps with partial redraws.
- Knob cards: a marker for the set value, plus a moving arc for the live modulated value.
- Patch page: a sparkline per source and an activity "LED" per cord. Gates flash on the lanes.
- Scope page: ring buffers (about 128 points, 8-bit) per viewed source; clock-synced trigger;
  time zoom (1 beat to 16 bars); freeze; gate lane; pre/post-cord view; two-trace overlay and XY
  mode; stepped, quantized pitch view.

### Voices and FX

- Voices: pad, LPG pluck, lead, percussion (keep Melodee's DRUM engine). Basic amp and filter
  envelopes on each. Designed to sit well together.
- Chords are a per-track note-generation option (triads/sevenths from the current scale), not a
  voice type, so they work with any voice and over MIDI.
- FX: delay (clock-synced, modulatable wow; capped at about 0.5 s, or stored lo-fi to save RAM),
  reverb (inherited), bitcrusher, wavefolder, ring mod, comb resonator. Every FX parameter is a
  mod destination.
- Budget: pads + reverb + delay together is the worst case; enforce firm caps. Add a
  "maximum everything" scenario to the CPU budget tests.

### Scales

Keep Melodee's scale system. Microtuning (just intonation, Scala files via the web editor) is a
later idea.

### External MIDI (USB only, modest)

- The FM-1 has no TRS MIDI out, so external MIDI is USB only and needs a USB host (a computer, a
  norns shield bridge, or a host-capable device).
- Per-track routing: internal / external / both; per-track MIDI channel; notes, gates, velocity;
  clock out.
- Document the DAW use case (computer as USB host). CC output of modulators is maybe-later.
- To check: whether the OXI ONE MKII's USB host accepts notes from the FM-1.

### Form chaining

- A global chain of forms (ABAC, AABA, ...) that changes on phrase boundaries, possibly with
  repeat counts per form.
- A is home and recurs. Contrast rises across departures and resets on returns.
- **Wander:** 0 plays the chain exactly as written; higher makes a weighted choice of the next
  form.
- A small per-track chance of skipping a section change for one phrase.

### Entries (tracks entering and leaving)

- **Entries** parameter: 0 is fully manual (mute/solo); higher gives more chance of tracks
  dropping out or re-entering at section boundaries.
- Manual mute/solo always overrides automation. Optional quantized mutes that land on the next
  phrase boundary.
- Per-track anchor flag (never auto-drops). Guardrail: at least one track always plays. A brief
  visual warning before an automatic change.
- Track roles: foundation, rhythm, harmony, melody, defaulted from the voice type.
- Music-theory rules:
  - Hypermeter: small changes on 4 bars, bigger on 8/16, biggest at sections. Entries at phrase
    starts, exits at phrase ends.
  - Layering order foundation → rhythm → harmony → melody, reversed for exits.
  - Melody needs harmony or foundation present.
  - One change per boundary during builds.
  - Breakdown to the anchors before a return to A; full return on A's downbeat.
  - A minimum dwell of N phrases.
  - A Flourish before entries. Entry style cut or fade (fade for ambient).

### Piece length

- Free-running, or a fixed length in bars or minutes.
- A fixed length plans an arc from the seed: intro (layering in), development (rising contrast),
  peak around two-thirds, final return to A, outro (layering out in reverse), resolve to the
  root, tails ring out.
- Wander varies details within sections. The plan recalculates if parameters, mutes or tempo
  change; manual mutes still win.
- Show the current section, the position and the time remaining.
- Later: arc shapes (arch / slow build / flat); end behaviour (stop / new seed / continue free).

### Energy

- A central parameter and mod bus source. Modes: Off / Manual / Arc. Off by default in new
  projects. Per-destination depth.
- Biases (never overrides) user settings within their ranges: track count via Entries, note
  density and drops, filter brightness, envelope/LPG decay, velocity, pitch range, reverb send
  (more at low energy), mutation rate. The response varies by track role.
- Manual: a playable knob, the headline performance-page control. Arc: written by the
  piece-length planner, with an Energy bias control.
- Manual mutes and anchors always win.

### Long-form ambient

- Guardrails: Gravity, a density floor, pitch range limits.
- The default patch normals slow, gentle modulation to filter cutoff, LPG decay and FX sends, so
  the timbre evolves too.
- A host test that simulates 8 hours of a patch and checks it stays varied (no collapse to
  silence, single notes or frozen loops).

### Performance page (design later)

Parameters from any page assigned freely to the four knobs, in banks of four, plus macros.
Relies on the central parameter IDs.

### Boot behaviour

- Setting: random curated patch / last project / silent. A short volume fade-in.
- Random patches: curated templates; pentatonic family (occasionally Dorian); a gentle tempo
  range; light mutation; moderate density; reverb on; random seed, root and voice pairings;
  nothing wild.
- Show the seed and allow saving it. Scrap the feature if it doesn't work well.

### Later ideas

- Deterministic seeds for all randomness (recall exact states).
- Retroactive capture of the last N bars.
- Scenes with morphing.
- A constrained "dice" randomize.
- Mutation history using the existing undo levels.
- Patch sharing via the web editor, and a community library page.

## UI

- **Keep Melodee's interaction model:** knob cards and its navigation. Players coming from
  Melodee should feel at home.
- **A visual language of our own:** a tile/mosaic motif (the name), modulation visuals as the
  signature look, and consistent colours per track or role.

Principles:
- The same knob grammar on every page (the same knob does the same kind of thing everywhere).
- Shallow navigation: every page within a press or two.
- Progressive disclosure: the essentials first, depth on request.
- Parameters at their off position are shown dimmed.
- An automation status strip shows which automatic layers (Energy, Entries, Wander, Evolve,
  Gravity, Piece Length) are active.
- Musical language in labels (for example "Density", "Home", "Wander"), not engineering terms.

In-box help:
- Hold the help combo and touch a knob: a 1–2 line description of the parameter, plus a
  live-state line saying what it is doing now (for example "Wander 0: the chain plays exactly
  as written").
- A one-line summary per page, shown on the first visit or on request.
- The full manual online, with a QR code to it on the About page.
- Help text lives in the central parameter table, next to each parameter's ID and range. The
  online parameter reference is generated from that table, so the device and the docs stay in
  sync.

Process: draft a page map before the end of phase 2, and review designs with the PNGs the test
suite renders (`tests/ui_render.c` → `build/ui_new/`, with one contact sheet per palette).
Visual styling comes later.

## Web simulator

The firmware's own generator, sequencer, mod and UI code compiled to WebAssembly, so it can be
played in a browser. It does not validate the hardware CPU budget: check cost with the host
tests and on the device, which stays essential.

1. Now: keep the platform layer strict. New code must not touch registers outside `firmware/hal/`.
2. Mid-project: an Emscripten/WebAssembly build that reuses the host-test stubs. AudioWorklet
   audio, the display on a canvas, an on-screen panel (our own illustration, not M-Vave photos),
   Web MIDI, browser storage in place of flash. Single-core build.
3. Near release: a public demo deployed to GitHub Pages by GitHub Actions.

## Upstream sync

- Remote `upstream` = `https://github.com/keremimo/melodee.git` (fetched with tags).
- **Synced upstream version: `0bc95e8` "chore(release): prepare 1.1.0"** (2026-10-10). The fork's
  history isn't connected to upstream's; attach it (rebase or `git replace --graft`) before the
  first merge.
- Latest upstream reviewed: **v1.3.1** (2026-10-13), 60 commits after the base. Merging it is about
  10 small textual conflicts plus semantic checks of `seq.c` (transport, recording) and `project.c`
  (format rewrite) against the generators' hooks. Upstream also retired ANALOG, DIGITAL and all
  external effects (`perform.c`, `delay.c`, `slicer.c`), and saved RAM and flash.
- Process: before each merge, summarise upstream changes into platform/bug fixes to take, changes
  touching code we've modified, and changes to engines we're removing. Merge whole releases while
  feasible; cherry-pick after the hard fork. Update the synced version above after every merge.

## Phases

0. **Foundation:** cleanup, CLAUDE.md, ROADMAP.md, a working build and tests; upstream sync review.
1. **Extract the generators** behind the new interface, with host tests (output identical to
   before).
2. **Signal bus** and central parameter IDs.
3. **Normalled default patch.**
4. **Strip engines** and add the new voices; FX with caps.
5. **Visual feedback** and the scope.
6. **Form chaining, Entries, Piece Length, Energy.**
7. **Performance page, boot behaviour, web simulator.**

**Version 1.0 target (small and early):** the signal bus, the existing generators on it, a few
mod sources, visual feedback. Release it, gather feedback from FM-1 users, then grow.

## Tasks

### 0. Foundation
- [x] Remove the stray root files (shell-paste leftovers, tessera-updates.diff)
- [x] `./build.sh` passes in WSL
- [x] Host tests compile again (hostsim includes Tessera, package name, flash guard, CZ-1 type)
- [x] CLAUDE.md and ROADMAP.md
- [x] Stop tracking `__pycache__/`; add it and the `*.bak*` / `*.pre*` backups to `.gitignore`
- [x] Upstream sync review (v1.3.1 against the base, see Upstream sync)
- [ ] **Bug (flash layout, risky):** `OBJ_TESSERA` at 0x9D000/0x9E000 shares its sectors with
      project slot 4 (`PROJECT3` A/B), so saving one can corrupt the other. Move it to free
      sectors and add an overlap check to `storage_test.c`. (Upstream's Prophet banks 3–5 also
      share sectors with project slots 1–3; ask upstream whether that is intended.)
- [ ] Decide whether to merge upstream v1.3.1 before phase 1 (it removes `perform_test.c` and
      gains RAM headroom: only 608 B of arena are free today)
- [ ] Fix the perform test: FX black key 1 leaves track 1 at peak 318 (limit 200; upstream v1.1.0: 63)
- [ ] Fix regress HEALTH: song/4track_mix and slicer/song_gate_stut keep voices and DC after the
      note-offs (upstream v1.1.0 passes, so the fork causes it)
- [ ] CZ-1 knob test fails the same way on upstream v1.1.0 (GCC 15 / Linux): report upstream or skip
- [ ] `tests/run_tests.sh` passes end to end
- [ ] Delete the backup files in `firmware/src` and `tools/` once nothing in them is needed
- [ ] Decide what happens to the `master`, `tools-util` and `cleanup-claude-md` branches

### 1. Extract the generators
- [ ] Define `gen_tick(state, input_phrase, clock)` and its output (gate stream + CV)
- [ ] Move Turing, Euclid, Markov and Morph behind it, out of seq.c / project.c / UI coupling
- [ ] Host tests per generator: output identical to before, deterministic for a seed, CPU cost

### 2. Signal bus and parameter IDs
- [ ] Central parameter IDs and ranges
- [ ] Fixed destination table; about 16 cords (source, destination, amount, offset) at control
      rate, extending `mod.c`
- [ ] Generators and a first set of mod sources (LFO, envelope, S&H) on the bus
- [ ] UI page map drafted before the phase ends; review it with the rendered UI PNGs
- [ ] Help text field in the central parameter table, filled for every parameter
- [ ] Generate the online parameter reference from the table

### 3. Normalled default patch
- [ ] Default patch that sounds good with no cords; cords override the normals
- [ ] Slow, gentle normals to filter cutoff, LPG decay and FX sends

### 4. Strip engines, new voices, FX
- [ ] Hard fork: create the `tessera` repo, GPL-3.0 and attribution kept; rename identities
      (package, USB, menu, web installer and editor)
- [ ] Strip Prophet, FM6, CZ, SLICE and samples; measure the freed flash, RAM and CPU
- [ ] Our own project format; drop `OBJ_TESSERA`
- [ ] Voices: pad, LPG pluck, lead, percussion (DRUM kept)
- [ ] FX with caps: delay, reverb, bitcrusher, wavefolder, ring mod, comb resonator
- [ ] "Maximum everything" CPU budget scenario

### 5. Visual feedback and scope
- [ ] Destination snapshot array; UI at about 30 fps with partial redraws
- [ ] Knob cards with live modulation arcs; patch page sparklines and cord LEDs
- [ ] Scope page

### 6. Form chaining, Entries, Piece Length, Energy
- [ ] Form chaining with Wander
- [ ] Entries with roles and the music-theory rules
- [ ] Piece Length planner
- [ ] Energy
- [ ] 8-hour long-form ambient host test

### 7. Performance page, boot behaviour, web simulator
- [ ] Performance page
- [ ] In-box help: help combo + knob touch, live-state line, page summaries
- [ ] Online manual; QR code on the About page
- [ ] Boot behaviour and curated random patches
- [ ] WebAssembly build; GitHub Pages demo

### Not yet placed in a phase
- [ ] STEP as seed: locks, Gravity, Keep, ghost-note grid; Ornament and Serial generators
      (phase 1's `input_phrase` prepares for it)
- [ ] The remaining mod sources, clock tools, quantizer, Kria-style lanes
- [ ] Chords as a per-track note-generation option
- [ ] External MIDI over USB: per-track routing and channel, clock out

## Constraints

AC79 at 360 MHz, 44.1 kHz audio, at least 152 KiB of audio working arena (see CLAUDE.md).

## Open questions

- Repeat counts per form.
- Performance page: the assignment gesture and the number of banks.
- Exact voice and FX budget numbers.
- Gravity and Energy curves.
