# Tessera: every candidate compared

This puts every idea from the surveys in one table: norns.community, Grids, Bounce, Logic and Branches, O_C Phazerville, EuroPi, Pamela's Pro Workout, and organic LFOs. Its purpose is to help weigh up what is worth adding. Source notes are in `docs/design/NORNS_GENERATORS.md` and `docs/design/MODULE_GENERATORS.md`.

**How to read the columns**
- **Effort** is measured against Grids, which counts as M. Grids needed a new mode, three parameters, three cord destinations, a picture, tests and a save-format step.
  - S is a mode that fits in the existing generator slot with no format change beyond its own bytes.
  - L needs new plumbing.
- **RAM** is static RAM per track unless marked "shared".
- **Flash** is a rough figure for code plus tables.
- **Value** is my musical judgement. It weighs how different the result sounds from what Tessera already does, how playable the mode is with three knobs, and how good it looks on screen. ★★★ means distinct and immediately useful.
- **Needs** lists the shared work an item depends on:
  - **4T** is the four-track hook (shared state and playing on another track), built once for MATRIX.
  - **GLIDE** is portamento on the voices.
  - **FRAME** is a per-frame redraw of the mode's picture.
  - **SUB** is timing finer than a step.
  - **CRD** is a cord-format step (two source tracks per cord).

## Generator modes

| Candidate | From | Effort | RAM | Flash | Value | Needs | Overlap and notes |
|---|---|---|---|---|---|---|---|
| Grids | MI | done | 8 B | 2.4 KB | ★★★ | | PR #4 |
| Bounce / Pong | Theo, rebound, Ponglet | S | 8 B | ~1 KB | ★★★ | FRAME | Distinct, and the best showpiece on screen. A gravity knob (off by default) would cover Particle Physics |
| Awake | norns | S | 34 B | ~1 KB | ★★★ | | Much loved. The work is entering its step values |
| MATRIX (initenere) | norns | M+ | 30 B shared + 3 B | ~2 KB | ★★★ | 4T | Its four-track polyrhythm is new to Tessera, and it builds 4T for others to use |
| nørgård | norns | S | 4 B | <1 KB | ★★ | | Self-similar melody. Cheap and unusual |
| Polyrhythmic Seq (Subharmonicon) | EuroPi | S | 8 B | <1 KB | ★★★ | | Instantly musical, and different from anything Tessera has |
| ProbMeloD | Phazerville | S | 14 B | <1 KB | ★★ | | Overlaps Turing; the per-note weights are the new part |
| less concepts (1D CA) | norns | S | 6 B | <1 KB | ★★ | | Good picture with rows scrolling down |
| Fibonacci / Pigeons | norns, Phazerville | S | 4 B | <1 KB | ★ | | Merge into one mode. Repetitive on its own |
| Modulo Magic / Infinite Melody | norns (Serge) | S | 6 B | <1 KB | ★★ | | Infinite Melody is close to Turing; Modulo Magic is the new half |
| Cumulus (Nibbler) | Phazerville | S | 2 B | <1 KB | ★★ | | Bit-pattern rhythms with an evolving feel |
| RunglBook (Rungler) | Phazerville | S | 2 B | <1 KB | ★★ | | A Turing cousin with a darker, chaotic character |
| ProbDiv / DCSN-2 / Probapoly | Phazerville, EuroPi | S | 2-6 B | <1 KB | ★★ | | Rhythm modes. Pick one, or fold them into Euclid as variants |
| orbital | norns | S | 10 B | <1 KB | ★★ | FRAME | Polyrhythm with free ratios. Close to Euclid |
| Shredder (Mimetic Digitalis) | Phazerville | S+ | 18 B | ~1 KB | ★★ | | Shares the 4×4 picture with MATRIX |
| The Darkest Timeline | Phazerville | M | 66 B | ~1 KB | ★★ | | Overlaps FORM's sections |
| Fall | norns | M | 24 B | ~1 KB | ★★ | FRAME | Gentle ambient. Good picture |
| First Light chimes | norns | S | 6 B | <1 KB | ★ | | Bursts and rests. Narrow use |
| zellen (Life) | norns, EuroPi, Phazerville | M | 16 B | ~1 KB | ★★ | | Picture-led. Musically uneven |
| TB-3PO (303) | Phazerville | M | 40 B | ~2 KB | ★★★ | GLIDE | Acid lines are a big draw. Glide is useful beyond this mode too |
| Automatonnetz / H1200 | Phazerville | M | 30 B | ~2 KB | ★★★ | chords | A chord-progression generator. Do it with the Chords item |
| Gate Phaser (Reich) | EuroPi | M | 4 B | <1 KB | ★★ | 4T, SUB | Phasing needs offsets finer than a step to drift smoothly |
| Ocean Surge (Swell Physics) | EuroPi | M | 20 B shared | ~1 KB | ★★ | 4T, FRAME | Four buoys for four tracks. Lovely picture |
| meadowphysics | norns | M | 16 B shared | ~1 KB | ★★ | 4T | Cascading counters across tracks |
| Relabi | Phazerville | M | 24 B | ~1 KB | ★★ | | Near-rhythm. Better as a cord source than a mode |
| Metropolis-style | norns | M | 24 B | ~1 KB | ★ | | More a step sequencer than a generator |
| flora (L-systems) | norns | L | 128 B | ~2 KB | ★ | | Needs a hard cap on string growth |
| Bloom | norns | M | 40 B | ~1 KB | ★★ | | Needs note entry from the keys |

## Modifiers (TWEAK, FORM and cords), usable with every mode

| Candidate | From | Effort | RAM | Value | Needs | Notes |
|---|---|---|---|---|---|---|
| BRANCH (Bernoulli) | MI Branches, EuroPi | S | 3 B | ★★★ | 4T | Call and response between tracks |
| LOGIC AND/OR/XOR/NOT | foulplay, Pam's mask | S | 2 B | ★★ | | Rhythmic interplay between tracks |
| COIN cord source | Branches | S | 0 | ★★ | | Fits the present cord format |
| Two-track logic cord sources | Phazerville Logic, TL Neuron | M | 0 | ★ | CRD | Advanced |
| RESEED another track | Pam's | S | 1 B | ★★ | 4T | One track keeps refreshing another |
| Ratchets ×2/×4 | Pam's, Burst | S | 1 B | ★★★ | SUB | Big feel for little cost. Needs sub-step gates |
| Timing HUMAN / RAMP / swing | Pam's flex-ops | M | 2 B | ★★ | SUB | Swing is already planned. RAMP is new |
| Euclid PAD | Pam's | S | 1 B | ★ | | One knob added to Euclid |
| Palimpsest accents | Phazerville | S | 16 B | ★★ | | A velocity layer on any mode |
| Organic LFOs (Sloth, ochd) | NLC, DivKid | S | 70 B shared | ★★★ | | See below |

## Organic drifting LFOs: Triple Sloth and ochd

Both fit Tessera well. Both are slow, never-quite-repeating motion, which is what a generative piece needs to stay alive over many minutes. Tessera already has the beginnings of this in DRIFT (`cord.c` `cord_drift`: one eased point every ~12 s per track). These two would add character DRIFT doesn't have.

- **Triple Sloth (Nonlinear Circuits):** three chaotic circuits at very different speeds.
  - Torpor takes about 15-20 minutes per cycle, Apathy about a minute, and Inertia about 2-4 minutes.
  - Each circuit is a three-variable "jerk" system, which goes digital cleanly: fixed-point integration at control rate, a few multiplies per block, 3 × 3 × 4 B = 36 B.
  - Outputs: Torpor, Apathy and Inertia (plus their sum and difference, as on the module).
  - Each circuit needs its start state seeded from the project seed, so it replays.
  - Values must be clamped so the fixed-point integration can't run away. A host test can check that each runs for hours and stays in range.
- **ochd (DivKid):** eight free-running LFOs at related but non-integer rates, with one master RATE control and a slightly organic (non-triangle) shape.
  - Eight phase accumulators plus a shaping table: 8 × 4 B = 32 B and a 256-byte shape.
  - Outputs: ochd 1-8, slowest to fastest.
  - This would be an implementation of the idea, not a port of ochd's firmware.
- **How they plug in:** two new cord sources, SLOTH and OCHD, appended to the frozen source list. The cord's source-track byte (0..3, already in the format) picks which output, so no CRD step is needed. The sources are shared rather than per track, and they keep running when the song stops. A global page would hold SLOTH SPEED and OCHD RATE. On the scope page they'd make the nicest traces of anything here.
- **Cost:** CPU is negligible at control rate. About 70 B of RAM shared and about 1.5 KB of flash for both.

## Shortlist by merit (my recommendation)

1. **Small and high-value:** Bounce, Awake, the Subharmonicon sequencer, organic LFOs, BRANCH, LOGIC.
2. **Shared plumbing that unlocks more:** the four-track hook via MATRIX (then Ocean Surge, meadowphysics, RESEED), and sub-step timing via ratchets (then swing, timing HUMAN and Gate Phaser).
3. **Big draws needing one piece of support each:** TB-3PO (with glide) and Automatonnetz (with Chords).
4. **Cheap fillers when there's room:** nørgård, less concepts, Cumulus, RunglBook, one rhythm mode from ProbDiv / DCSN-2 / Probapoly.
5. **Leave for later:** flora, Metropolis-style, The Darkest Timeline, First Light, Fibonacci on its own.

## Budget check

Headroom on 2026-10-16 was about 4.9 KB of static RAM free and about 52 KB of flash. All the S and M modes together come to roughly 1 KB of RAM and 25-30 KB of flash. Flash, not RAM, is what limits how many modes fit. Phase 4's strip of unused Melodee engines frees far more.
