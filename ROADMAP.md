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
  N cords (room for 32, 16 in use at first), each with source, destination, amount and offset. Extend Melodee's existing
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
- **Logic and Branches** (Theo 2026-10-16; Mutable Instruments Branches: a Bernoulli gate, each trigger goes to
  output A or B by a coin toss of probability P; toggle and latch modes). Three pieces, smallest first:
  - BRANCH on TWEAK: with probability P a hit plays on another track (TO 1..4) instead of this one, so two tracks
    answer each other; TOGGLE alternates, LATCH keeps the last side until the toss changes. DROP is already the
    one-sided case (hit or nothing). The toss uses the track's seeded stream, so it replays with its SEED, and LOCK
    could repeat the last tosses (deja vu). Playing on another track shares MATRIX's four-track hook (that track's
    gate-off). About 3 B a track.
  - LOGIC on TWEAK (foulplay-style): this track's hits combined with another track's: AND (only when both hit), OR,
    XOR, NOT (only when the other rests). The tracks step in index order, so a track reads a lower one's hit from
    this step and a higher one's from the last. About 2 B a track.
  - Cord sources: COIN A / COIN B (a source track's gate split by a toss, P set by the cord's amount), then
    AND/OR/XOR of two tracks' gates, flip-flop, comparator. Two-track logic needs a second source track in the cord
    (a CRD format step); COIN fits the present cord.
  Every piece has an OFF position (P = 0, LOGIC off). Each is a few lines in `tessera.c` beside DROP; most of the
  work is the help texts, the parameter IDs and a host test per mode.
- Quantizer with weighted scale degrees; learn the scale from played notes.
- Kria-style polymetric lanes (note, trigger, octave, gate, each with its own length).
- Later generators: 1D cellular automata, cascading counters, Bloom-style branching mutations.

### More generators (Theo 2026-10-16)

Candidates beside Turing, Euclid, Markov and Morph, each a `gen_<name>_tick` in `gen.c` that plays one note a step
(Grids: up to three). Feasibility checked against each original (thread "generative sequencing scripts").

- **Grids** (Mutable Instruments Grids): kick, snare and hat from a point on its 5 x 5 map of drum patterns. X, Y,
  DENSITY on GEN; CHAOS is LOCK turned round; EVOLVE walks X and Y. Its map is 2.4 KB of flash, 8 B a track of RAM.
  GPL-3.0-or-later, so its table and logic are used as they are. First built: branch `grids`.
- **Awake** (norns, tehn): two looping sequences of different lengths (up to 16 each) added into the scale, and a
  PROBABILITY. Easy as a generator; the work is entering 32 step values (ROLL to fill them, editing on the keys).
  About 34 B a track saved.
- **Fall** (norns, ambalek, MIT): up to 8 leaves, each with a note and a falling speed, playing when they land; a
  leaf lands, fades and falls again. Steps on its own 1/16 clock already, so it maps directly; leaves landing on the
  same step play one note, and its collision "rustle" sound is left out.
- **First Light chimes** (norns, tehn): only the wind chimes carry over: bursts from a small set of notes, then a
  rest of 3 to 9 seconds, in steps. Its sequencer moves a delay's loop length (shared FX here): not ported.
- **Bounce / Pong** (Theo's idea): a ball moving a fixed distance each step, no gravity, bouncing off the screen's
  edges; a wall hit plays a note (which wall and where along it, snapped to the scale). SPEED and ANGLE set how often
  it hits; it loops when the angle brings it back to its start, and EVOLVE nudges the angle. PADDLE (knob, or a
  cord) adds Pong: a miss rests and serves again. One generator, the paddle optional. About 8 B a track.
- **More from norns.community** (survey 2026-10-16, full notes in `docs/design/NORNS_GENERATORS.md`), easiest
  first: nørgård (infinity series, from the step number's bits), fibonacci (mod N), rebound (= Bounce above),
  orbital (rings at free ratios), less concepts (1D cellular automaton, rule/seed/range), Infinite Melody / Modulo
  Magic (Ken Stone), foulplay (Euclid + logic against another track). Then zellen (Life), meadowphysics, acid
  basslines (bline), Metropolis-style (skylines, metrix, m18s). Skipped: cyrene (= Grids), grurder, luck/jala/
  raindrops (Turing covers them), kria/faeng (sequencers, not generators).
- **MATRIX** (initenere, linusschrab): a shared 4 x 4 note matrix; a track in MATRIX mode plays its own row, on its
  own clock (direction pause/fwd/back/random, play probability), so the polyrhythm comes from the track speeds and
  EVOLVE/TWEAK/cords stay per track. Columns rotate up/down/shuffle on the song clock. Time routings: track A's step
  also advances track B (B's gate-off handled from A's step; tracks step in index order, so it stays
  deterministic). Shared block about 30 B plus about 3 B a track; a save-format step (TSR4) like Grids'. One screen
  shows the whole matrix and all four playheads whatever track is selected (the one break from editing
  `song.sel`). Build the shared four-track hook once: quence, meadowphysics, fugu/streams and foulplay's logic
  reuse it.
- **From O_C Phazerville, EuroPi and Pam's Pro Workout** (survey 2026-10-16, notes in
  `docs/design/MODULE_GENERATORS.md`; EuroPi is CC0, Hemisphere MIT, O_C GPL-3.0), easiest first: ProbMeloD
  (a weight per note), Pigeons (Fibonacci mod N, merge with fibonacci), Polyrhythmic Sequencer (Subharmonicon),
  Cumulus (Nibbler accumulator), RunglBook (Rungler), ProbDiv, DCSN-2, Probapoly, The Darkest Timeline, Shredder
  (Mimetic Digitalis). Moderate: TB-3PO (seeded 303 lines; needs glide), Gate Phaser (Reich phasing) and Ocean Surge
  (Swell Physics, four buoys) on the four-track hook, Automatonnetz / Harrington 1200 (neo-Riemannian chords, with
  Chords), Relabi. Modifiers: RESEED another track and mask (Pam's cross-ops), timing HUMAN and RAMP (flex-ops),
  Euclid PAD, ratchets, Palimpsest accents, threshold-logic neurons.
- **Comparison of every candidate** (Theo 2026-10-16: weigh them up before choosing): effort, RAM, flash, musical
  value and shared dependencies in `docs/design/GENERATOR_COMPARISON.md`. Shared work that unlocks several items: the
  four-track hook (MATRIX), sub-step timing (ratchets, swing, phasing), glide (TB-3PO), Chords (Automatonnetz).
  Shortlist: Bounce, Awake, Subharmonicon-style polyrhythm sequencer, organic LFOs, BRANCH, LOGIC; then MATRIX and
  ratchets; then TB-3PO and Automatonnetz. Flash, not RAM, limits how many modes fit (~1 KB RAM, 25-30 KB flash for
  all S and M modes).
- **Organic LFOs** (Triple Sloth, ochd): cord sources SLOTH (Torpor ~15-20 min, Apathy ~1 min, Inertia ~2-4 min:
  fixed-point jerk systems at control rate, seeded, clamped) and OCHD (8 free LFOs at related non-integer rates, one
  RATE, organic shape table), shared, chosen by the cord's source-track byte (no CRD step). About 70 B RAM and
  1.5 KB flash for both; DRIFT stays as the simple one.
- **Visuals**: each mode draws its norns picture in `graph_tessera_viz` (Theo's wish). The 240 x 240 colour screen
  holds norns' 128 x 64 grey layouts at about 1.9x, grey levels mapped to theme colours. Moving pictures (Bounce,
  orbital) need a per-frame redraw of that area, not per step: measure the CPU on the device. Copying a script's
  code or layout closely brings its licence (add to LICENSING.md); our own drawing of the idea does not.
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
| FX held + keys | Nothing now: upstream removed this layer (performance effects, mutes on black keys 1–4, KNOB macros) |
| GLO held + keys | Black keys 1–4 latch mutes, F3–B3 solo while held, C4 unmute all, F4 tap tempo; KNOB 1–4 levels |
| SCL held + keys | The key pressed sets the root; KNOB 1–4 root, scale, chord, voicing |
| EDIT held | Undo (since upstream 1.3.6, which removed the EDIT quick layer); EDIT + OCT-/+ undo/redo |
| SEQ held + keys | Eight white keys (F3–C5) select patterns; hold one and press another to copy |
| ENV / SCL / EDIT held on STEP | Note gestures: SELECT lengthens or moves the note; EDIT tapped deletes; EDIT + OCT-/+ undo/redo |
| LFO held + knob | Momentary tweak: the value springs back on release; OCT+ keeps it |
| Taps / holds | HOME hold: menu. REC hold: Capture. SEQ hold: PATTERNS. SAVE hold: save the project. OCT+ hold: the page's sheet |
| Button pairs | REC + PLAY: record now. SAVE + REC: quick save. GLO + PLAY: restart. OCT- + OCT+: octave reset; held 5 s: update mode; at power-on: panel calibration |
| Grid pages (drums) | White keys toggle the lane's steps; black keys select lanes and pages |

Free today: **ARP held** (no gesture at all), **FX held** (its layer removed upstream), **ENV held
off STEP**, and keys with **ARP, ENV or LFO held** (they still play notes). Melodee's quick layers are the model to keep:
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
- **Synced upstream version: `v1.3.6`** (`a4e6f22`, 2026-10-15), merged 2026-10-16 on branch
  `merge-upstream-1.3.6` (`e725e23`). Before it: `v1.3.1`, merged 2026-10-14 in `1b4c9e9`.
  The `v1.4.0-rc` tags are release candidates: wait for 1.4.0.
  The fork's history is connected to upstream's by `4621fa4` (a no-change "ours" merge of `0bc95e8`,
  the fork's real base), so git finds the merge base by itself. Not rebased: `main` is already on GitHub.
- Upstream retired ANALOG, DIGITAL and all external effects (`perform.c`, `delay.c`, `slicer.c`),
  and saved RAM and flash. `upstream/next` adds a CPU1 retirement fix (`7025143`): cherry-pick it if
  CPU1 misbehaves.
- Process: before each merge, summarise upstream changes into platform/bug fixes to take, changes
  touching code we've modified, and changes to engines we're removing. Merge whole releases while
  feasible; cherry-pick after the hard fork. Update the synced version above after every merge.
- **From 1.4 on: picks, not merges** (Theo 2026-10-17: only what still makes sense for Tessera as its own
  firmware, feature set, character and look). Review: `docs/analysis/upstream-1.4.md`. Taken from `v1.4.1`
  (branch `upstream-1.4-picks`): the CZ-1 test fix, RAM reclaimed for audio, the DSP optimizations and shared
  voice BUDGET, single-core batching, SID's C64 models, POLY and the SID KIT / DIGI drums. Left out: the
  "Large type" look, the shared preset bank, 1..4 tracks, dual-core only. Still to take, each in its own
  session: OTA recovery of interrupted updates (`229dacd`), the boot storage safeguards (`569b820`), the
  installer's stock-restore fix, the FX TONE / cuts DSP (with "FX with caps").
- **Melodee data is not carried over** (Theo 2026-10-17): loading Melodee presets, projects or other data into
  melodee-tess or Tessera is not a goal. Tessera owns the flash (Melodee 1.4's shared preset bank also uses
  0x9D000, Tessera's object 28: no matter); its own layout comes with the "own project format" task.

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
- [x] Check the merged build on the device: play, stop (no hanging notes), GLO mute, MIDI Start/Stop,
      save and load, a project saved before the merge, autosave of generator edits. Host-checked
- [x] Merge upstream v1.3.6 (Theo 2026-10-16: "work in any sensible changes"): the boot stub
      (an interrupted update falls back to update mode), 360 MHz boot without a PLL write, the 320 MHz
      option removed, stock V15 power startup, startup checks of foreign settings and projects, MIDI
      channel ranges, EDIT held undoes. Whole release merged, nothing left out: picking parts would
      diverge more. Both builds pass, the Tessera baseline, gen_test and upgrade check are unchanged
- [x] Check the 1.3.6 merge on the device (build 1.1.9): installed, power-on and the start sound, EDIT
      held undo, SAVE held save, MIDI in, a project saved by 1.1.8. Passed 2026-10-16 (Theo: "All tests
      check out"); merged into `tessera-dev`
      2026-10-14: old projects load with every Tessera setting (`tests/upgrade_check.sh`), MIDI clock
      Start/Stop/Continue, GLO mute and stop leave no note held (Tessera baseline). Passed on the device
      2026-10-15 (the `follow-step` firmware, standard profile); USB MIDI out of the keys reaches Ableton
      (sequenced notes are not sent yet: External MIDI task)
- [ ] `MELODEE_RAM_AUDIO=1` (upstream's resident audio profile) no longer links: "audio working arena below
      worst-case budget", the Tessera generators leave too little SRAM. Standard profile until then; revisit
      when phase 4 frees RAM
- [x] Autosave lost generator-only edits (their state is not in the project record): fixed `38e9950`
- [x] Backups include the generators: objects 28 (saved slots) and 29 (live); the site's editor and installer
      must be rebuilt with the merged firmware (`web/fm1backup.js` knows 30 objects)
- [x] Review of the merge resolutions: hooks in `seq.c`, `ui_input.c`, `ui_layer.c`, `storage.c`,
      `params.c`, `project.c` as before the merge; backups leave Tessera out (QUESTIONS.md 8)
- [x] Simulator on the merged firmware: branch `web-sim-1.3.1` (not on the site yet)
- [x] Perform test and regress HEALTH: one bug, `tessera_kill` cleared the note list before releasing
      it, so stopping left step-sequencer notes hanging. Fixed in `279f08c` (branch `overnight-analysis`)
- [ ] CZ-1 knob test fails the same way on upstream v1.1.0 (GCC 15 / Linux): two identical renders
      differ from sample 11; report upstream or skip (QUESTIONS.md 5). Reported as keremimo/melodee#47;
      our reply (2026-10-15) gives the audio suite with the fix: 335 golden renders, 0 changed. Waiting upstream
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
- [x] Define the generator interface and its output (gate, note, velocity + CV): `gen.h` (2026-10-15). One
      `gen_<name>_tick(state, clock, out)` per generator instead of one `gen_tick`; the input phrase joins with
      STEP as seed (`docs/analysis/phase1-plan.md`, Outcome)
- [x] Move Turing, Euclid, Markov and Morph behind it: `gen.c` (steps, mutation, FORM sections), `tessera.c`
      the adapter; the UI reads through `tessera_turing()` .. `tessera_morph()`; knob handlers guarded
      against the audio interrupt. Branch `phase1-gen`, merged into `tessera-dev` 2026-10-15
- [x] Host tests per generator: output identical to before (`tessera_test`, now with the state after the
      stop), `tests/gen_test.c` (gen.c alone: ranges, draws per step, determinism, cost), generator UI renders
- [x] Play test of `phase1-gen` on the device (engine changes and ROLL while playing: no stuck note): passed
      2026-10-15 (build 1.1.1), merged into `tessera-dev`
- [x] Turing's fresh register random (Theo 2026-10-15): picked from the hardware timer when TURING is first
      selected or played; saved projects keep theirs. Branch `turing-random-start` (baseline rewritten: the 14
      Turing scenarios); device check passed with `track-seed` (build 1.1.4), merged into `tessera-dev`
- [x] Seeded random generator per track (Theo 2026-10-15: shown and editable): each track's own stream from its
      SEED (1..9999, knob 4 of EVOLVE, in place of ROLL); a seed sets the stream, Turing's register, Euclid's
      pitches, Markov's first state; PLAY restarts the stream; a fresh track a random seed (all generators,
      superseding the Turing-only start). Saved: block version 2 ("TSR2", 47 bytes a track), version 1 projects
      and backups converted on reading. Branch `track-seed` (on `turing-random-start`); device check passed
      2026-10-15 (build 1.1.4), merged into `tessera-dev`. Knob 4 is labelled ROLL, the seed in small grey under it (Theo 2026-10-15:
      the number is only a marker, to find a line again; no coarse steps or acceleration, which would skip lines).
      ROLL waits for the track's next bar line while playing (Theo 2026-10-15): a click restarted the line mid-bar
      and a quick turn stuttered; stopped, at once. Not saved: a seed still waiting when saved is not kept.
      Device check passed on build 1.1.4 (test installer /test/1.1.4/webapp/installer/ on the site, as 1.1.3's)
- [x] Mutation bounds (Theo 2026-10-15, from the overnight analysis): EVOLVE keeps Euclid's PULSES 1..STEPS-1
      (it reached 0: silent bars), Turing's active bits never all 0 or all 1 (silence, or a note every step,
      for good when locked), and Markov's TENSION (LEAP) turns back at 0 and 100 instead of sitting there. The
      same draws: only a phrase at a bound plays otherwise (baseline: midi_clock_start_stop_continue). Morph's
      rhythm A and the drift itself (Gravity, phase 6) still open. Passed on the device in build 1.1.9
      (2026-10-16); merged into `tessera-dev`
- [x] A start for testing the generators (Theo 2026-10-15, "a matter of convenience for testing"): at power-on and
      NEW, track 1 CZ-1 VIBRAPHONE (was PROPHET FUNK BASS II), 85 BPM, the melodic parts in minor pentatonic.
      `TRK_START_TESS` (engines.c); the host tests keep Melodee's start (hostsim.c sets it 0), and format 1
      projects their power-on sounds (TRK_DEF_V1). Revisit before a public release. Passed on the device (1.1.9), merged
- [x] ROLL stops at 1 and 128 instead of wrapping (Theo 2026-10-15); a seed over 128 from 1.1.3-1.1.5 goes to
      128. Passed on the device (1.1.9), merged
- [x] ROLL's range 1..128, with the knobs' acceleration (Theo 2026-10-15: 9999 too many to turn through): a quick
      half turn across it. Seeds saved by 1.1.3-1.1.5 (up to 9999) load and play as they were; ROLL from one lands
      in 1..128 (turning back does not return to it). Baseline rewritten (every fresh seed another). Passed on
      the device (1.1.9), merged
- [x] Markov's DENSITY default to about 70 (decided 2026-10-14 for "the next intended baseline change"; not
      done with SEED, to keep that change to one thing): 70 (2026-10-15), for a fresh track only; saved projects
      keep theirs. Baseline rewritten (the Markov scenarios' notes, every state and the saved block's hash).
      Passed on the device (1.1.9), merged

### 2. Signal bus and parameter IDs
- [x] Central parameter IDs and ranges (2026-10-16, branch `param-ids`): `firmware/src/pid.c`, 144 ids.
      0x000 + P_*, 0x100 + G_*, 0x200 + Tessera's TX_* (tessera.h, frozen, append only); Melodee's
      descriptors reused, Tessera's added with get/set by value. Frozen against `tests/pid_ids.txt`
      (`tests/pid_test.c`). Which ids a cord may reach: the destination table (`cord.c`)
- [x] Fixed destination table; cords (source, destination, amount, offset) at control rate,
      extending `mod.c` (2026-10-16, branch `cord-dests`). Room for 32 cords, 16 in use (Theo
      2026-10-16: raising the number in use changes a number, not the format). 41 destinations, frozen
      codes (`tests/cord_dests.txt`): per voice (pitch, cutoff, shape, gain), per block (level, pan,
      spread, the envelope, ENV and LFO amounts, LFO rate and depth, FX send, E1..E8; each one checked
      against where the firmware reads it), at the generator's step (Turing PROB, LEN; Euclid PULSES,
      ROT; Markov LEAP, DENS; Morph POS; SHIFT, DROP, HUMAN; EVOLVE AMOUNT; FORM CONTRAST). Left out,
      with the reason in `cord.c`: note-on parameters, FX SEND 2/3, the sequencer's, globals, choices.
      Applied in `mod_begin` (out of line; no cords: the sound bit for bit as before) and around the
      generator's step. Host cost with 16 cords on four playing tracks: about 3-6 % more (`cord_test`);
      on the device still to measure
- [x] Cords saved with the project: a "CRD1" part in the Tessera object (no new sector), tagged per
      slot as the blocks; older objects and backups load without cords (`backup_test`)
- [x] CORDS page (LFO family, after MOD), as the mock: KNOB 1 source, 2 the cord, 3 destination, 4
      amount (a cord without a source: its OFFSET). Not yet: the activity dots of the mock
- [x] Generators and a first set of mod sources (LFO, envelope, S&H) on the bus: sources LFO, ENV,
      VEL, KEY, RAND, MODW, AT, EXPR, S&H, SLEW (mod.c's) and the generators' GATE, NOTE and CV
      (gen.c: Turing its register, Euclid and Morph a ramp, Markov the degree; no draw of its own: the
      notes unchanged, only the baseline's `.state` hashes rewritten)
- [x] UI page map drafted before the phase ends; review it with the rendered UI PNGs (draft and
      decisions 2026-10-16, linked under Open questions; mocks in `docs/design/phase2/`, "look good")
- [x] GEN family under ARP's tap: GEN, EVOLVE, FORM, TWEAK, ARP, ARP 2 (2026-10-16, branch `gen-family`).
      GEN is a second page onto PATTERN's knobs (MODE and the engine's three); PATTERN stays in SEQ for
      the step sequencer, so upstream's page is untouched. `FAM_GEN` = `FAM_ARP` (no new family).
      2026-10-16 (Theo: one page reached from both ARP and SEQ is confusing; branch `gen-owns-knobs`):
      GEN alone shows the generator; PATTERN is upstream's again (LEN DIV SWING GATE, the step grid),
      and FEEL, which stood in for those, is gone. Passed on the device (1.2.5), with the ARP reach
- [x] Help text field in the central parameter table, filled for every parameter (all 144; checked
      against the code where a label said too little)
- [x] Generate the online parameter reference from the table: `docs/PARAMETERS.md` (`pid_test`
      writes it and fails when it is out of date), with the cords' sources and destinations
- [x] Help gesture: ARP held + a knob turned shows the knob's help card, the value left as it is;
      ARP's release then opens no page (`ui_help.c`). Engine panels (FM6, CZ-1, PROPHET): label only
- [x] Selector lists (Theo 2026-10-16): CORDS' SOURCE and DESTINATION turned show the picker's list
      (each entry with its track), MOD's DST list by the engine's names; the help card names a value whole
- [x] MODS replaces MOD (Theo 2026-10-16, branch `mods-merge`): CORDS renamed MODS; MOD hidden
      from LFO's taps (its code kept for upstream syncs). Routes that come in (an old project, a sound
      saved with them, the editor) become mods on their track, sounding bit for bit as before
      (cord_test); a sound's go when another sound loads, unless edited; a project's stay; no room:
      MODS FULL and the route keeps playing. Same-track ENV/VEL/KEY/RAND mods are per voice now.
      User sounds are saved without modulation (it is the project's)
- [x] Device check of phases 2 and 3 (build 1.2.5, Theo 2026-10-16: "tested and happy"; merged into `tessera-dev`): GEN, PATTERN, the ARP reach, the selector lists, MODS, help, CORDS (a cord heard, saved,
      loaded), and the cost of 16 cords on the device
- [x] Choose the function key (ARP held, 2026-10-16); the key map with it still to draft

### 3. Normalled default patch
- [x] Default patch (2026-10-16, Theo: "proceed to the next step"; branch `mods-merge`, build 1.2.5):
      ordinary mods a new project starts with (no template; `cord_defaults`), seen and edited in MODS:
      each track's CUTOFF +12 % by its own DRIFT, its FX SEND +10 % by another track's. Old projects
      and templates keep theirs, so they sound as saved. The NORMALS switch first recommended needed
      a new saved field (the flash layout): dropped for this, no format change
- [x] DRIFT: a slow source per track (a point every 11.9 s, eased), clocked by `mix_block`
- [x] Default patch heard on the device (1.2.5, Theo: happy); LPG decay as a default mod left for later
- [x] MODS: the list without a flash (only it and the chips sent), eight names; activity lights

### 4. Strip engines, new voices, FX
- [ ] Hard fork: create the `tessera` repo, GPL-3.0 and attribution kept; rename identities
      (package, USB, menu, web installer and editor)
- [ ] Strip Prophet, FM6, CZ, SLICE and samples; measure the freed flash, RAM and CPU
      (measured 2026-10-16, plan for Theo's decisions: `docs/design/phase4/PLAN.md`: switches first, IDs
      reserved, flash banks untouched, new voices before the old ones go; Plaits considered)
      Decided 2026-10-16: FM6, CZ-1, Prophet go (SID stays), a track of them loads SID INIT, new voices both
      our own and a Plaits port. Build switches done (branch `engine-switches`): all three out frees 69.6 KB
      of flash, 13.9 KB of RAM; on by default (the build unchanged) until the new voices are ready
- [ ] MACRO, the Plaits port (2026-10-17, branch `macro-plaits`, prototype): engine 21 behind `MELODEE_PLAITS`
      (off by default), 20 reserved; `macro_test` in `run_tests.sh` passes (host cost about 2x SID a note).
      With FM6, CZ-1, Prophet in, flash overflows by 8 KB; with the three out it links (app 515,872 B, arena
      157,984 B). Still to do: cost on the device, presets, merge with `grids`, then the switch on by default
      when the three go
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
      (material so far: `docs/manual/recipes.md`: SID chip arpeggios, Grids variations with FORM and LFO cords)
- [ ] Boot behaviour and curated random patches
- [ ] WebAssembly build; GitHub Pages demo

### Not yet placed in a phase
- [x] FOLLOW, first piece (branch `follow-step`): STEP tracks as sources (the highest sounding note, held across
      rests); while following, SHIFT counts scale degrees (TWEAK shows "deg"). Chromatic tracks play as before
- [x] **Grids** generator (Theo 2026-10-16, branch `grids`): GEN's X Y DENSITY, LOCK as CHAOS, EVOLVE and FORM walk
      X and Y; every part a GM drum note (36 38 42; a DRUM track's kit). Saved: block version 3 ("TSR3", 51 bytes a
      track), versions 1 and 2 converted on reading (projects, backups); cords on X, Y, DENSITY. Host tests: gen_test
      (the map against Grids' own values), three tessera scenarios, the saved block. Device check and the web
      simulator still to do. 2026-10-17: the device build failed to link (memmove, none in libc.c), fixed;
      merged with `macro-plaits` into `phase4-dev`; test page 1.2.6 (MACRO off) for the device check:
      https://rastapopolix.github.io/melodee-tess/test/1.2.6/webapp/installer/
- [ ] More generators (Features > More generators): Awake, Fall, First Light chimes, Bounce / Pong
- [ ] MATRIX (initenere) and the shared four-track hook (Features > More generators); then the norns.community list
- [ ] **FOLLOW modes** (Features > FOLLOW): interval in scale degrees, chord tone, contrary, canon, bass; RHYTHM
      own / source / both; STEP tracks as sources. With phases 1-2; then in the web simulator (rebuild, site copy,
      test, guide and demo)
- [ ] STEP as seed: locks, Gravity, Keep, ghost-note grid; Ornament and Serial generators
      (phase 1's `input_phrase` prepares for it)
- [ ] Organic LFOs SLOTH and OCHD as cord sources (Features > More generators)
- [ ] Choose from the generator comparison (docs/design/GENERATOR_COMPARISON.md) with Theo
- [ ] Logic and Branches (Features > Mod sources): BRANCH and LOGIC on TWEAK, then COIN and logic cord sources
- [ ] The remaining mod sources, clock tools, quantizer, Kria-style lanes
- [ ] Chords as a per-track note-generation option
- [ ] External MIDI over USB: per-track routing and channel, clock out
- [ ] Key modes: the play gestures (transpose, scale by held keys, phrase recording, chord input,
      Gravity home) and the function-key gestures (performance, patching, jumps, form chains,
      step locks); legend and mode indicator

## Constraints

AC79 at 360 MHz, 44.1 kHz audio, the audio working arena above the build's minimum (142,336 B since
v1.3.1; see CLAUDE.md).
Headroom after phase 2's cords and help (2026-10-16, dual core): static RAM 4,444 B free, audio arena
4,640 B above the minimum, `melodee.bin` 521,212 B of the 573,440 B the XIP region holds (about
52 KB left; Melodee's help texts take about 7 KB of it). Phase 4's strip frees far more.
Headroom after the v1.3.6 merge (2026-10-16, dual core): static RAM 4,908 B free, audio arena
5,440 B above the minimum, `melodee.bin` 503,340 B.
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
- ~~Which physical button is the function key.~~ Decided 2026-10-16, all as the draft recommends
  ("Tessera: function key and page map", https://claude.ai/artifact/3zcUFMPB6EhtJ93qxgtjLf):
  ARP held is the function key (a reach check on the device still to do); a GEN family under ARP's
  tap holds the generator pages, EVOLVE, FORM, TWEAK and ARP, taken out of SEQ; CORDS (room for 32, 16 in use,
  across tracks) under LFO, beside each track's 4 MOD routes; PIECE and ABOUT under GLO; help is the
  function key held while turning a knob (the knobs cannot sense touch). Upstream 1.3.6 made EDIT
  held undo and SAVE held save; ARP held is still free.
  Mock screens for review (2026-10-16): `docs/design/phase2/` (GEN family, help, CORDS, NORMALS,
  PIECE, ABOUT; `make_mocks.py` draws them), branch `phase2-mocks`.
- How the modifier convention fits Melodee's existing layers (FX, GLO, SCL, EDIT, SEQ held):
  keep them as they are, or fold them into the function key.
