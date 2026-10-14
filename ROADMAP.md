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

### FOLLOW: one track following another's pitch

Today (TWEAK > FOLLOW): a generator track plays the other track's last note plus SHIFT semitones, snapped to
its own scale, in its own rhythm. It follows generator tracks only (a STEP track as source does nothing), plays
one note, and SHIFT in semitones can leave the key. User feedback (2026-10-14) asked to build on a written
melody, and for clearer control over how a track follows.

Controls (TWEAK, or a FOLLOW page of its own): **FOLLOW** (source track, OFF = no effect), **HOW** (the mode),
**INTERVAL**, **RHYTHM**. Modes:
- **Interval:** a fixed interval in scale degrees (third, fifth, sixth, octave; above or below), so the line
  stays in key. Replaces today's semitone SHIFT for following.
- **Chord tone:** a note of the chord built on the source's note (third, fifth, seventh), the nearest one to the
  last note played: smooth complementary lines.
- **Contrary:** the opposite direction to the source (it rises, this falls), mirrored around a centre note.
- **Canon / echo:** the source's notes again N steps later, optionally at an interval.
- **Bass:** the source's note one or two octaves down, on the strong beats only.
- **RHYTHM:** this generator's own rhythm (today), only when the source plays, or both.
- **Any source:** STEP tracks as well as generators (the source's sounding note, after its scale and
  transpose; fixes today's double transpose).

Rules: every mode has an off / no-effect position; each control gets a central parameter ID, range and help
text. Fits phase 1 (`gen_clock_t` / `input_phrase`: the source as input) and phase 2 (parameter IDs).

In the web simulator: it runs the firmware, so the modes come with it. Ship them there as part of the work:
rebuild `web/sim/` and the site copy (`docs/webapp/sim/`), extend `web/sim/test_sim.mjs` (a STEP melody followed
in each mode), and add FOLLOW to the page's "Try the generators" guide and its demo (one track following
another), so people can try the modes in the browser before installing.

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

## Keys

The FM-1 has 27 keys (F3–G5: 16 white, 11 black) and 14 buttons (FX, SCL, ENV, LFO, EDIT, GLO,
HOME, SAVE, ARP, SEQ, PLAY, REC, OCT-, OCT+).

### How Melodee uses them today

| Gesture | What it does |
| --- | --- |
| Keys alone | Play notes; on STEP, write the cursor step (live at the play head while recording) |
| FX held + keys | White keys hold performance effects, black keys 1–4 mute tracks 1–4; KNOB 1–4 macros |
| GLO held + keys | Black keys 1–4 latch mutes, F3–B3 solo while held, C4 unmute all, F4 tap tempo; KNOB 1–4 levels |
| SCL held + keys | The key pressed sets the root; KNOB 1–4 root, scale, chord, voicing |
| EDIT held + keys | White keys jump to sound sections, black keys to FM6 operators, Init, Favourite, Undo, Store |
| SEQ held + keys | Eight white keys (F3–C5) select patterns; hold one and press another to copy |
| ENV / SCL / EDIT held on STEP | Note gestures: SELECT lengthens or moves the note; EDIT tapped deletes; EDIT + OCT-/+ undo/redo |
| LFO held + knob | Momentary tweak: the value springs back on release; OCT+ keeps it |
| Taps / holds | HOME hold: menu. REC hold: Capture. SEQ hold: PATTERNS. SAVE hold: undo. OCT+ hold: the page's sheet |
| Button pairs | REC + PLAY: record now. SAVE + REC: quick save. GLO + PLAY: restart. SAVE + OCT-/+: undo/redo. OCT- + OCT+: octave reset; held 5 s: update mode; at power-on: panel calibration |
| Grid pages (drums) | White keys toggle the lane's steps; black keys select lanes and pages |

Free today: **ARP held** (no gesture at all), **ENV held off STEP**, and keys with **ARP, ENV or
LFO held** (they still play notes). The FX layer's four unassigned white keys are free too, but
upstream has since removed the performance effects. Melodee's quick layers are the model to keep:
hold a button, keys and knobs become its shortcuts, the map shows over the page, letting go
restores.

### Plan

Without a modifier, **keys play music**:
- Live transpose of the generators.
- Hold keys to set a quantizer scale.
- Record a live phrase as a STEP seed.
- Chord input for chord mode.
- Set Gravity's home note.

With the **function key** held, **keys select**:
- Performance mode: quantized mutes and solos.
- Patching: hold a source key, then press a destination key.
- Quick jumps to tracks, pages, banks, scenes and seeds.
- Typing form chains.
- Step locks and probability.

Rules:
- One consistent modifier convention across the whole firmware.
- An on-screen legend while the modifier is held (as Melodee's layer maps do).
- A visible indicator whenever the keys are in a non-default mode (for example a held-scale or
  transpose mode), so the player always knows what a key will do.

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
- **Synced upstream version: `v1.3.1`** (`757b65b`, 2026-10-13), merged 2026-10-14 in `1b4c9e9`.
  The fork's history is connected to upstream's by `4621fa4` (a no-change "ours" merge of `0bc95e8`,
  the fork's real base), so git finds the merge base by itself. Not rebased: `main` is already on GitHub.
- Upstream retired ANALOG, DIGITAL and all external effects (`perform.c`, `delay.c`, `slicer.c`),
  and saved RAM and flash. `upstream/next` adds a CPU1 retirement fix (`7025143`): cherry-pick it if
  CPU1 misbehaves.
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
- [x] Flash layout checked: `OBJ_TESSERA` (0x9D000/0x9E000) is in a free pair. It overlaps only the
      retired legacy project objects, which nothing reads or writes (the 2026-10-13 "bug" was wrong;
      see `docs/analysis/upstream.md`)
- [x] Storage test: no two live flash objects share a sector (guards the free pair at upstream merges)
- [x] Merge upstream v1.3.1 before phase 1 (decided 2026-10-14; `1b4c9e9`): both builds pass, the
      Tessera baseline is unchanged, FORM's labels shortened to CONTR / FLRSH for the new fonts
- [ ] Check the merged build on the device: play, stop (no hanging notes), GLO mute, MIDI Start/Stop,
      save and load, a project saved before the merge, autosave of generator edits. Host-checked
      2026-10-14: old projects load with every Tessera setting (`tests/upgrade_check.sh`), MIDI clock
      Start/Stop/Continue, GLO mute and stop leave no note held (Tessera baseline)
- [x] Autosave lost generator-only edits (their state is not in the project record): fixed `38e9950`
- [x] Backups include the generators: objects 28 (saved slots) and 29 (live); the site's editor and installer
      must be rebuilt with the merged firmware (`web/fm1backup.js` knows 30 objects)
- [x] Review of the merge resolutions: hooks in `seq.c`, `ui_input.c`, `ui_layer.c`, `storage.c`,
      `params.c`, `project.c` as before the merge; backups leave Tessera out (QUESTIONS.md 8)
- [x] Simulator on the merged firmware: branch `web-sim-1.3.1` (not on the site yet)
- [x] Perform test and regress HEALTH: one bug, `tessera_kill` cleared the note list before releasing
      it, so stopping left step-sequencer notes hanging. Fixed in `279f08c` (branch `overnight-analysis`)
- [ ] CZ-1 knob test fails the same way on upstream v1.1.0 (GCC 15 / Linux): two identical renders
      differ from sample 11; report upstream or skip (QUESTIONS.md 5)
- [ ] `tests/run_tests.sh` passes end to end
- [x] Delete the backup files in `firmware/src` and `tools/` once nothing in them is needed (checked:
      `docs/analysis/housekeeping.md`): deleted 2026-10-14, the older save format kept in `docs/analysis/attic/`
- [x] Decide what happens to the `master`, `tools-util` and `cleanup-claude-md` branches: deleted 2026-10-14 (the first
      two held nothing new; `tools-util`, a refactor of six upstream tools files, dropped)

### 1. Extract the generators
Plan: `docs/analysis/phase1-plan.md`.
- [x] Baseline of what the generators play today: `tests/tessera_test.c`, 22 scenarios and the saved
      block against `tests/tessera_baseline.txt` (`TESSERA_UPDATE=1` rewrites it)
- [x] Widen the baseline: swing, MIDI clock, pattern switches, SONG chains, short `P_SLEN`, GLO mute (28 scenarios)
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
- [ ] Choose the function key and draft the key map alongside the page map

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
- [ ] **FOLLOW modes** (Features > FOLLOW): interval in scale degrees, chord tone, contrary, canon, bass; RHYTHM
      own / source / both; STEP tracks as sources. With phases 1-2; then in the web simulator (rebuild, site copy,
      test, guide and demo)
- [ ] STEP as seed: locks, Gravity, Keep, ghost-note grid; Ornament and Serial generators
      (phase 1's `input_phrase` prepares for it)
- [ ] The remaining mod sources, clock tools, quantizer, Kria-style lanes
- [ ] Chords as a per-track note-generation option
- [ ] External MIDI over USB: per-track routing and channel, clock out
- [ ] Key modes: the play gestures (transpose, scale by held keys, phrase recording, chord input,
      Gravity home) and the function-key gestures (performance, patching, jumps, form chains,
      step locks); legend and mode indicator

## Constraints

AC79 at 360 MHz, 44.1 kHz audio, the audio working arena above the build's minimum (142,336 B since
v1.3.1; see CLAUDE.md).
Headroom after the v1.3.1 merge (2026-10-14, dual core): static RAM (`.data + .bss`) 5,200 B free,
audio arena 5,472 B above the minimum, `melodee.bin` 496,896 B (about 84 KB below the 579,832 B the
fork had before). Before the merge: 36 B, 608 B, about 1.7 KB.

## Overnight analysis (2026-10-14)

Branch `overnight-analysis`, not pushed, nothing merged. Reports in `docs/analysis/`:
- `upstream.md`: changes from our base to v1.3.1 / next in three groups, merge estimate (small text
  conflicts, moderate semantic checks), and the correction of the flash "bug".
- `codebase-map.md`: modules, platform vs application, the generators' hooks (sequencer in the audio
  interrupt, UI, storage; none into the mod matrix), and what removing Prophet/FM6/CZ/SLICE involves
  (about 120 KB of flash).
- `generators.md`: each generator in plain language, two-hour simulations: Euclid goes silent for up to
  72 bars, FORM replays exactly forever, Markov plays every step; where Gravity and guardrails fit.
- `phase1-plan.md`: the `gen_tick` interface, the order of work, risks (the order of random draws, RAM).
- `QUESTIONS.md`: decisions for Theo.
Also: the Tessera baseline test, and the stuck-notes fix (`279f08c`). The suite now fails only the
CZ-1 knob test (an upstream issue).

Decisions the same day (QUESTIONS.md 1–7, Theo: "go with your recommendations"): tonight's work brought
into `tessera-dev`; the history connected without rewriting (`main` is already pushed, so no rebase);
v1.3.1 merged before phase 1 on branch `merge-upstream-1.3.1`; the stuck-notes fix kept; CZ-1 reported
upstream and left failing; FORM to develop with Gravity in phase 6; Markov's DENSITY default to about 70
at the next intended baseline change.

## Open questions

- Repeat counts per form.
- Performance page: the assignment gesture and the number of banks.
- Exact voice and FX budget numbers.
- Gravity and Energy curves.
- Which physical button is the function key. The FM-1 has none labelled FN. Candidates: ARP (its
  hold is unused) or FX (its effects layer would then move into performance mode).
- How the modifier convention fits Melodee's existing layers (FX, GLO, SCL, EDIT, SEQ held):
  keep them as they are, or fold them into the function key.
