# Generator review

Written 2026-10-14 from `firmware/src/tessera.c` (923 lines) on branch `overnight-analysis`. The long-run
numbers come from a two-hour simulation per setting through the real sequencer (the same harness as
`tests/tessera_test.c`, 120 BPM, 1/16 steps).

## How the generators run

Each of tracks 1–4 has a generator setting: OFF (the normal step sequencer) or one of the four
generators. With a generator on, every step of the track's own grid (its division, swing and length)
calls `tessera_clock_step` instead of playing the step. That function:

1. counts bars and, every EVERY bars, mutates the generator or moves the FORM on;
2. asks the generator whether this step plays, and which note and velocity;
3. applies LOCK (replay of a recorded phrase), DROP and HUMAN;
4. picks the pitch: FOLLOW, FLOURISH, then snaps it **down** to the nearest note of the track's
   scale, adds the track's transpose and SHIFT;
5. plays it as a single note (monophonic), lasting GATE % of a step.

All randomness comes from `rng()`, the one random generator the whole firmware shares.

## The four generators in plain language

### Turing (a shift register, after Music Thing's Turing Machine)

- A loop of 2–16 bits (LEN) turns one step at a time; the bit that falls off the end comes back in at
  the start. When the newest bit is 1, the step plays.
- **PROB** (shown as LOCK = 100 − PROB on EVOLVE) is the chance each step that the returning bit
  is flipped. At 0 the loop repeats exactly (LEN steps long); at 100 it is coin flips.
- The pitch comes from the low 8 bits of the register: a degree of a fixed **minor** scale
  (`TESS_SCALE_MIN`) and an octave within the octave span (1–3), from C2 (MIDI 36) up. Velocity
  comes from the high bits (90–121).
- Default: register `0xACE1`, LEN 16, PROB 0: a fixed 16-step riff.

### Euclid (Euclidean rhythm)

- PULSES hits spread as evenly as possible over STEPS (1–32), turned by ROTATION.
- Pitch: each step has its own pitch, a chromatic note in the octave above C3 (48), from a fixed
  pattern (seed 0: `(step × 3) mod 12`) or from a hash of a seed that ROLL changes. Snapped to the
  scale. Velocity is always 110.
- Default: 16 steps, 4 pulses (track 2: 5), no rotation.

### Markov (a melodic random walk)

- Eight states (scale degrees). Each step the next state is drawn from a row of weights built from
  TENSION: low tension favours steps of one degree and the ends of the range; high tension makes
  leaps of three or more more likely.
- The note is the track's root + **two semitones per state** (a whole-tone ladder, not the scale's
  degrees) from C2, plus a random octave within the span, snapped down to the scale. Velocity 95–114.
- **DENSITY** is the chance a step plays at all. Its default is 100: every step plays.

### Morph (crossfade between two rhythms)

- Two 16-step rhythms, A and B, chosen from eight presets (four-on-the-floor, offbeats, eighths,
  son clave, rumba clave, 3-3-2, alternating, dense). Each step, MORPH (0–127) is the chance the step
  comes from B instead of A.
- The pitch is a fixed degree of the minor scale for the step, one formula for A and one for B,
  in the octave above C3. Velocity accents every fourth step.

## The layers on top (EVOLVE, FORM, TWEAK)

- **EVERY** (off, 1, 2, 4, 8, 16 bars) and **AMOUNT** (0–100): at the end of each phrase the generator
  mutates: Turing flips AMOUNT % of its LEN bits; Euclid nudges the rotation, sometimes a pulse more
  or fewer, and may re-roll the pitches; Markov's tension moves by up to ±AMOUNT/2; Morph flips 1–3
  bits of rhythm A and moves MORPH. ROLL does one mutation now.
- With EVERY on, Turing's register goes back to the phrase's starting value at the start of every
  loop, so PROB's variations do not accumulate within a phrase.
- **LOCK** for Markov and Morph ("déjà vu"): the first loop after a phrase boundary is recorded; on
  later loops each step replays the recording with probability LOCK %, otherwise plays fresh.
- **FORM** (AB, ABAC, ABCB, AABA, ABCA, AABC): each section lasts EVERY bars (FORM switches EVERY on at
  4 bars). Section A is the phrase that was playing; a new section is made once from the previous one
  by mutations of strength **CONTRAST**; a section seen before is recalled exactly. CLEAR forgets B
  and C. **FLOURISH** shifts the notes of the last beat of every phrase by −12…+12 semitones.
- **TWEAK**: SHIFT (±24 semitones), DROP (chance a hit is skipped), HUMAN (velocity ±HUMAN/5 and
  shorter gates), FOLLOW (play another track's last note, shifted, in this track's scale).
- Saved with the project: every setting above and the sections' settings. Not saved: the recorded
  LOCK phrases (re-recorded on the first loop after loading), playheads and counters.

## Long-form playback

Two hours per setting (the `longform` probe; windows of 4 bars):

| Setting | Notes/bar | Silent windows (longest) | One-pitch windows | Window = previous window (longest run) |
| --- | --- | --- | --- | --- |
| Turing, EVERY 1, AMOUNT 30, PROB 0 | 7.9 | 0 % | 0 % | 0 % |
| Turing, PROB 10, EVERY off | 7.9 | 0 % | 0 % | 0.1 % (8 bars) |
| Euclid, EVERY 1, AMOUNT 30 | 7.0 | 4.6 % (32 bars) | 5.2 % | 2.7 % (12 bars) |
| Euclid, EVERY 1, AMOUNT 80 | 7.2 | 3.6 % (16 bars) | 2.9 % | 0 % |
| Markov, EVERY 2, AMOUNT 30, LOCK 0 | 16.0 | 0 % | 0 % | 0 % |
| Markov, EVERY 2, LOCK 60 | 16.0 | 0 % | 0 % | 0 % |
| Morph, EVERY 1, AMOUNT 30 | 8.0 | 0 % | 0 % | 2.7 % (20 bars) |
| Morph, EVERY 1, AMOUNT 40, LOCK 100 | 8.0 | 0 % | 0 % | 1.9 % (16 bars) |
| Turing, FORM ABAC, EVERY 1, CONTRAST 60 | 8.0 | 0 % | 0 % | **99.9 % (the whole 2 hours)** |
| All four tracks: Euclid on track 2 (EVERY 4) | 3.6 | **13.2 % (72 bars)** | 12.8 % | 6.7 % (16 bars) |

### Risks

1. **Frozen loops: FORM never develops.** Once A, B and C exist, every return recalls the section
   exactly and EVERY's mutations are replaced by section changes. A Turing form is then the same
   4 × EVERY bars forever (measured: identical for two hours). Markov and Morph sections with LOCK
   below 100 still vary note by note, but around fixed settings. For long-form this is the biggest gap:
   the form is fixed after its first cycle.
2. **Collapse to silence: Euclid's pulse count is a random walk with nothing keeping it off 0.** Mutation
   adds or removes a pulse with equal chance, stopping only at 0 and at STEPS. At 0 the track is silent until the walk comes
   back; at 1 it plays a single note per loop. Measured: up to 72 silent bars in two hours. The same can
   happen to Turing at small LEN (a register whose LEN bits are all 0: silence; all 1: one note on every
   step), and to Morph if rhythm A loses all its bits while MORPH is low.
3. **Single notes and narrow pitch: fixed registers and downward snapping.** Pitches come from fixed
   ranges (at the default span of 2 octaves: Turing C2–A#3, Markov C2–D4; Euclid and Morph one octave
   from C3), built on a minor scale
   (Turing, Morph), whole tones (Markov) or chromatic steps (Euclid), then snapped **down** to the track's
   scale. In major or pentatonic scales several source notes land on the same note, so melodies use
   fewer pitches than they appear to. Nothing pulls the register toward the track's own range.
4. **Unbounded drift (no home).** Every mutation is a random step away from where the phrase was:
   Turing's register, Euclid's rotation and pulses, Markov's tension (which can sit at 0 or 100 for
   long stretches because it is clamped), Morph's rhythm A. Over hours the music wanders anywhere in
   the parameter space and never returns to the opening idea, the opposite of the ROADMAP's "A is home".
5. **Monotony: Markov plays every step.** DENSITY defaults to 100 and nothing varies it: 16 notes a bar
   forever, an unbroken run of sixteenths.
6. **Not reproducible from a seed.** `rng()` is shared with the arpeggiator, step chance, LFO S&H on
   tracks 1–3 and some engines' noise, so a generator's choices depend on what else is playing. The
   same project does not replay the same music.

### Smaller issues found

- Turing never advances `step_idx`, so the panel picture's playhead for Turing (`ui_pages.c pv_picture`)
  stays on step 0.
- Turing ignores the track's LEN (`P_SLEN`): with LEN 5 it plays exactly what it plays with 16 (its own LENGTH sets its
  loop), while Euclid's hits follow the shorter track loop (found widening the baseline; `track_len_shorter_than_loop`).
  Decide whether a track's LEN should bound every generator.
- Morph keeps its B preset in the field named `oct_span`, and Morph has no octave span.
- `tessera.h` documents Markov's `density` as "0 = Minor, 1 = Major, ..." (a leftover).
- SHIFT is added after the scale snap when not following, so a SHIFT that is not a scale interval plays
  outside the scale; with FOLLOW it is added before the snap. FOLLOW reads the other track's final note,
  which already includes that track's transpose, so a followed track's TRN is applied twice.
- The knob handlers (main loop) change several fields at once without stopping the audio interrupt that
  runs the generators (a step may see half an edit).
- Fixed and resolved tonight: `tessera_kill` cleared the sequencer's note list before releasing it, so
  stopping the transport left normal step-sequencer notes hanging (the regress HEALTH and perform test
  failures). Commit `279f08c`.

## Where Gravity and the guardrails fit

Phase 1 puts every generator behind `gen_tick(state, input_phrase, clock)`. That gives three natural
places:

1. **Gravity inside each generator's mutation** (its own notion of distance from home). The home is the
   input phrase (a STEP seed), or section A, or the state when EVOLVE was switched on. With Gravity G, a
   mutation is drawn toward home with probability G and away from it otherwise:
   - Turing: flip a bit where the register differs from home (Hamming distance shrinks) instead of a random bit.
   - Euclid: move PULSES and ROTATION one step toward home's values.
   - Markov: move TENSION (and later the transition row) toward home; with an input phrase, toward the
     transitions learnt from it.
   - Morph: flip a bit of A toward home's A; move MORPH toward home's.

   The same distance gives the **distance-from-home meter** for the grid. At G = 0 nothing changes
   (the ROADMAP rule: off = no effect). Gravity also fixes risk 4 and, with FORM, gives a way to let
   recalled sections develop (risk 1): recall a section, then apply AMOUNT mutations pulled toward that
   section's own home, so a return is recognisable but not identical.

2. **Guardrails as one shared stage after `gen_tick`** (the same for every generator, before the note
   reaches the voice), each with an off position:
   - **Density floor:** after N empty steps, the next strong beat plays (fixes risk 2 without touching
     the generators); a ceiling for Markov-style runs (risk 5).
   - **Pitch range:** fold notes by octaves into a range around the track's root (risk 3), and snap to the
     nearest scale note rather than always down.
   - **Repetition breaker:** when the last K phrases are identical and EVERY or FORM is on, force one
     mutation (risk 1 for forms with LOCK 100 or Turing).

3. **Bounds on the mutations themselves:** keep Euclid's PULSES in [1, STEPS − 1] (or a floor set by the
   density guardrail), never let Turing's LEN bits all be equal, reflect Markov's tension instead of
   clamping it.

4. **A seeded random generator per track** (`gen_rng` in the generator state) instead of the shared
   `rng()`, so a seed replays the same music (risk 6, and the ROADMAP's "deterministic seeds"). This
   changes what the generators play, so it must come **after** the phase-1 extraction has been proven
   identical against the baseline, as its own intended change with a new baseline.

The checks for all of this are the same measurements as the table above, which the ROADMAP's 8-hour
host test can assert: no silent window longer than N bars unless the user asked for it, no single-pitch
windows beyond a small share, no identical windows for more than M phrases while automation is on, and
the pitch range staying inside its limits.
