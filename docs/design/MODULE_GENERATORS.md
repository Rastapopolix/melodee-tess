# O_C Phazerville, EuroPi and Pamela's Pro Workout ideas for Tessera

Survey of 2026-10-16. Sources read:
- the O_C Phazerville app and applet index and the docs for the main candidates (github.com/djphazer/O_C-Phazerville);
- the EuroPi community scripts index (github.com/Allen-Synthesis/EuroPi, software/contrib);
- the published feature list for Pamela's Pro Workout. Pam's is closed firmware, so the ideas there come from that list, not from code.

I left out utilities (quantizers, VCAs, mixers, tuners, scopes, MIDI, calibration), audio applets and plain step sequencers. Things already on the roadmap are listed at the end, not ranked.

**Licences.** EuroPi scripts are CC0, so they can be ported freely. Hemisphere applets are MIT and the original O_C apps are GPL-3.0; both are fine for melodee-tess. Add a LICENSING.md row for any code taken over, as was done for Grids.

## Generator modes, easiest first

| # | Source | What it does | Fit for Tessera |
|---|---|---|---|
| 1 | **ProbMeloD** (Phazerville) | Every note in a range gets its own probability weight, and each step draws from them | Very easy: 12 weights plus a range. It also covers the roadmap's "quantizer with weighted scale degrees" idea as a generator |
| 2 | **Pigeons** (Phazerville) | Two Fibonacci-style melodies: each note is the sum of the two before it, modulo N | Very easy. It sits beside the norns fibonacci idea and could be merged with it |
| 3 | **Polyrhythmic Sequencer** (EuroPi, after the Moog Subharmonicon) | A 4-step pitch sequence advanced by up to 4 polyrhythmic clock dividers | Easy and distinctive. The four dividers are knobs and the four notes come from ROLL or the keys |
| 4 | **Cumulus** (Phazerville, after the Schlappi Nibbler) | An 8-bit accumulator: each clock adds, subtracts or XORs a constant, and chosen bits become gates | Easy: two bytes of state. The notes come from the accumulator value |
| 5 | **RunglBook** (Phazerville, after the Benjolin's Rungler) | A chaotic shift register fed back by its own output | Easy. It's a Turing cousin that wanders differently |
| 6 | **ProbDiv** (Phazerville) | Divisions /1, /2, /4 and /8, each with a weight, drawn at random and loopable | Easy rhythm mode. LOCK gives the loop |
| 7 | **DCSN-2** (EuroPi) | A random gate pattern grown from a binary tree, loopable | Easy rhythm mode |
| 8 | **Probapoly** (EuroPi) | Two polyrhythm ratios combined into the shortest non-repeating loop, with a probability on each gate | Easy. Euclid-adjacent |
| 9 | **The Darkest Timeline** (Phazerville) | A pitch timeline and a probability timeline (up to 32 steps), plus a "parallel universe" alternate timeline to swap to | Easy to moderate. The alternate timeline is close to FORM's sections |
| 10 | **Shredder** (Phazerville, after Mimetic Digitalis) | A 16-step sequence laid out 4×4 and "shredded" to random values in a range, with X/Y navigation | Easy to moderate, with a good picture. It's a single-track cousin of MATRIX |
| 11 | **TB-3PO** (Phazerville) | Seeded 303 acid lines with density, scale, accents and slides | Moderate. Slides need glide on the FM-1 voices. It replaces bline/acid-test from the norns list and is tuned for musicality |
| 12 | **Gate Phaser** (EuroPi) | Steve Reich phasing: identical gate patterns slowly drifting out of phase and back | Moderate. Across tracks it's a natural user of MATRIX's four-track hook |
| 13 | **Ocean Surge** (EuroPi, after ADDAC Swell Physics) | Four buoys on a moving wave; heights give CV and crossings give gates | Moderate. Four buoys fit four tracks, it has a lovely picture, and it needs the four-track hook |
| 14 | **Automatonnetz / Harrington 1200** (Phazerville) | Neo-Riemannian chord moves (P, L, R) walked across a 5×5 grid | Moderate. It goes with the roadmap's "Chords" item as a chord-progression generator for one track |
| 15 | **Relabi** (Phazerville) | Three cross-modulated LFOs whose crossings make near-rhythmic but irregular gates | Moderate. Works as a rhythm mode or a cord source |

## Modifiers rather than modes

These come mostly from Pam's Pro Workout and would sit on TWEAK, FORM or the cords:
- **RESEED another track** (Pam's cross-ops): a hit on track A re-seeds track B's generator, so one track keeps refreshing another. It's a new cross-track idea; about 1 B a track, using the four-track hook.
- **Mask by another track** (Pam's cross-ops): already planned as LOGIC (AND/NOT).
- **Timing feel** (Pam's flex-ops):
  - HUMAN timing (small early or late shifts) beside the existing HUMAN velocity;
  - RAMP, which speeds the steps up or slows them down across a phrase;
  - swing, already planned.
- **Euclid PAD** (Pam's): empty steps added to the end of a Euclid pattern. A one-knob addition to the Euclid mode.
- **Ratchets ×2 / ×4** (Pam's gate shapes, Phazerville Burst): already among the planned clock tools. It could be a per-track TWEAK.
- **Palimpsest** (Phazerville): an accent pattern written gradually, with each pass adding to or wearing away step velocities. It works as a velocity layer on any generator, close to EVOLVE.
- **Neural Net / TL Neuron** (Phazerville): threshold logic over several tracks' gates. It's the advanced end of the LOGIC plan.
- **Non-integer clock ratios** (Pam's): free per-track ratios for polymeter. Goes with the planned clock tools.

## Already covered (no new work)

- **Euclid:** EuclidX, euclid.py.
- **Turing:** TwoRings, Enigma, turing_machine.py.
- **Grids:** DrumMap, Consequencer, Hamlet.
- **Markov:** MarkoV, MarkovPerc.
- **Game of Life:** GameOfLife, conway.py. This is zellen in the norns list.
- **Bernoulli and logic:** Brancher, Bernoulli Gates, Coin Toss, Logic. These are the new Logic and Branches plan.
- **Bounce:** Ponglet, Pong, Bouncing Pixels. Ponglet's random velocity on each bounce is worth borrowing. Particle Physics is the gravity version, which you didn't want for Bounce, but it could be a GRAVITY knob that is off by default.
- **Clock and modulation tools:**
  - Burst, Shuffle, ClockSkip, Piconacci, Bit Garden: covered by DROP and the planned clock tools;
  - Lorenz, Strange Attractor, LFOs, Sigma: covered by the planned mod sources.
- **Pam's loop-the-random:** covered by LOCK / deja vu.

## Skipped

- **No clock on the FM-1:** Pet Rock and Daily Random need a real-time clock.
- **Too slight:**
  - Morse, which turns text into rhythm;
  - Itty Bitty and Binary Counter, which turn binary numbers into gates;
  - Traffic.
