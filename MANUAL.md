# Melodee (Tess) manual

This covers what Tess adds to Melodee, as in **v1.2.7**: the generators, the GEN pages, seeds, the modulation
cords (MODS) and the help card. Everything else (the sound engines, the step sequencer, the effects, saving)
works as in upstream [Melodee](https://github.com/keremimo/melodee).

Page and knob names are the ones on the screen. KNOB 1 to KNOB 4 are the four knobs left to right, and each
page's four settings sit along the top of the screen in the same order.

- [Shortcuts](#shortcuts)
- [The generators (GEN)](#the-generators-gen)
- [EVOLVE: mutation, LOCK and ROLL](#evolve-mutation-lock-and-roll)
- [FORM: sections](#form-sections)
- [TWEAK: shift, drop, human, follow](#tweak-shift-drop-human-follow)
- [MODS: the modulation cords](#mods-the-modulation-cords)
- [Saving](#saving)
- [Recipe: Grids variations that repeat](#recipe-grids-variations-that-repeat)

---

## Shortcuts

| Do this | And this happens |
|---|---|
| Tap **ARP** | Opens the GEN pages, on the one you used last (GEN the first time). |
| Tap **ARP** again | Next page: **GEN**, **EVOLVE**, **FORM**, **TWEAK**, then the arpeggiator's **ARP** and **ARP 2**, and round again. |
| Hold **ARP** and turn any knob | **Help**: a card with that knob's parameter, its value and what it does. The value is not changed. Let go of ARP to close it (no page opens). Works on every page. |
| Tap **LFO** until **MODS** | The cords page, after LFO, LFO 2 and LFO DEST. MODS replaces upstream's MOD page. |
| **GEN**, KNOB 1 | **MODE**: STEP, TURING, EUCLID, MARKOV, MORPH or GRIDS for the selected track. |
| **EVOLVE**, KNOB 4 (**ROLL**) | Each click is a new seed (1 to 128), so a new line. Turn back to hear the previous one again, note for note. A quick turn jumps further. While playing, the new line starts on the track's next bar line and the number blinks until then. |
| **FORM**, KNOB 1 with EVERY OFF | Turning FORM on sets EVERY to 4 bars, so the sections have a length. |
| **FORM**, KNOB 3 (**CLEAR**) | Turn it either way: B and C are forgotten and the section playing becomes A. |
| **MODS**, KNOB 2 | Picks the cord (1 to 16) the other three knobs edit. |
| **MODS**, KNOB 3 on a new cord | Choosing a destination sets the amount to 50 % (or a fixed move of 25 % with no source) so you hear it at once. |
| **PLAY** | Each generator's random choices start again from the top of its seed: with EVERY OFF, a seed always plays the same line. |

---

## The generators (GEN)

Each of the four tracks can play its own steps (**STEP**, upstream's sequencer) or have its notes made by a
generator. Tap **ARP** for the **GEN** page and pick one with **KNOB 1, MODE**. KNOB 2 to 4 are the
generator's own settings:

| MODE | KNOB 2 | KNOB 3 | KNOB 4 | What it plays |
|---|---|---|---|---|
| **TURING** | LENGTH (2–16 steps) | PROB (0–100) | SPAN (1–3 oct) | A looping shift register, as a Turing Machine. PROB is the chance a step changes as it comes round: 0 locks the loop, 100 is new every pass. |
| **EUCLID** | STEPS (1–32) | PULSE (0–STEPS) | ROT | Hits spread as evenly as they go over STEPS, turned round by ROT, on pitches picked by the seed. |
| **MARKOV** | LEAP (0–100) | DENS (5–100) | SPAN (1–3 oct) | A random walk over 8 scale degrees. LEAP 0 moves to neighbours, 100 leaps widely. DENS is the chance a step plays. |
| **MORPH** | POS (0–127) | SNP A (0–7) | SNP B (0–7) | A rhythm blended from preset A (POS 0) into preset B (POS full). |
| **GRIDS** | X (0–255) | Y (0–255) | DENS (0–100) | Mutable Instruments Grids' drum map: kick, snare and closed hat (notes 36, 38, 42) from the point X, Y; between patterns, a blend. Best on a DRUM track. |

With **STEP** selected, KNOB 2 to 4 are the step sequencer's DIV, SWING and GATE. SEQ's **PATTERN** page is
upstream's (LEN, DIV, SWING, GATE). The generators run at the track's DIV and play in the track's scale (SCL).

Every generator is also a modulation source: its **GATE**, its **NOTE** and a **CV** (Turing: the register as a
number; Euclid and Morph: a ramp over the cycle; Markov: the degree it moved to; Grids: the kick's chance).
See [MODS](#mods-the-modulation-cords).

---

## EVOLVE: mutation, LOCK and ROLL

| Knob | Setting | What it does |
|---|---|---|
| 1 | **LOCK** (0–100) | How much of the phrase stays: 100 loops it, 0 is new every pass. Turing's LOCK is its PROB turned round, Grids' is its CHAOS turned round (100 = no chaos). Euclid is already a fixed loop and has none. |
| 2 | **EVERY** | Mutates the phrase every OFF, 1, 2, 4, 8 or 16 bars. OFF keeps the phrase as it is. |
| 3 | **AMOUNT** (0–100) | How much changes at each mutation. |
| 4 | **ROLL** (1–128) | The track's seed. |

**Seeds.** The seed sets a track's random stream and its starting material: Turing's register, Euclid's
pitches, Markov's first state. Each seed is a line of its own and the same every time, so the small number on
ROLL is a marker to find a line again. A track gets a random seed the first time a generator is selected or
played, so each power-on starts somewhere new; a saved project keeps its seeds.

---

## FORM: sections

FORM strings mutated phrases into a short song form of sections A, B and C. Each section lasts EVERY bars.

| Knob | Setting | What it does |
|---|---|---|
| 1 | **FORM** | The order: OFF, AB, ABAC, ABCB, AABA, ABCA or AABC. OFF plays one phrase. |
| 2 | **CONTR** (0–100) | Contrast: how far a new section moves from the one before. |
| 3 | **CLEAR** | Forgets B and C: the phrase playing is A again. |
| 4 | **FLRSH** (−12 to +12) | Flourish: semitones added on the last beat of every phrase (0 is off). |

A section played again comes back exactly as it was. B and C are made from the section before by CONTR the
first time they are reached. Changing MODE or ROLL starts the form again at A.

---

## TWEAK: shift, drop, human, follow

| Knob | Setting | What it does |
|---|---|---|
| 1 | **SHIFT** (±24) | Transposes by semitones; while following, by scale steps from the followed note. |
| 2 | **DROP** (0–100) | The chance a hit is left out, whatever the generator. |
| 3 | **HUMAN** (0–100) | Varies velocity and gate a little, as a player would. |
| 4 | **FOLLOW** | OFF, or T1 to T4: plays that track's pitch, SHIFT away, in this track's scale. STEP tracks can be followed too. |

On a GRIDS track only DROP and HUMAN apply: SHIFT, FOLLOW and FLOURISH would change which drum plays.

---

## MODS: the modulation cords

MODS is a modulation bus: 16 cords, each from a source on any track to a destination on any track. Tap
**LFO** until **MODS**. It shows four cords to a screen with an activity light each.

| Knob | What it sets |
|---|---|
| 1 | **Source**: `--`, then per track (T1 to T4) LFO, ENV, VEL, KEY, RAND, MODW, AT, EXPR, S&H, SLEW, the generator's GATE, NOTE and CV, and DRIFT. |
| 2 | **Cord**: which of the 16 the other knobs edit. |
| 3 | **Destination**, on a track: the voice (PITCH, CUTOFF, SHAPE, GAIN), the mix (LEVEL, PAN, SPREAD, FX SEND), the envelope and LFO settings, the engine's E1 to E8, and the generators' settings (T PROB, T LEN, E PULSE, E ROT, M LEAP, M DENS, MP POS, SHIFT, DROP, HUMAN, EV AMT, CONTR, G X, G Y, G DENS). |
| 4 | **Amount**, −100 to +100 %. With no source (`--`) the cord is a fixed move and KNOB 4 sets its offset. |

- **DRIFT** is each track's slow wander: a new point about every 12 seconds, eased between, not in time with the bars.
- A new project starts with a default patch: each track's DRIFT moves its own CUTOFF a little, and another
  track's DRIFT its FX SEND. Edit or clear these like any other cord.
- Routes saved by upstream's MOD page (in a project or a sound) become cords when they load. A sound's cords
  go with it when another sound is loaded on the track, unless you edited them. If all 16 are in use, the
  screen says **MODS FULL**.
- The cords are saved with the project.

---

## Saving

Generator settings, seeds, sections and cords are saved with the project, in their own block next to it.
Projects saved by earlier Tess versions load and convert. Stock Melodee projects load with every track on STEP.

---

## Recipe: Grids variations that repeat

Two ways to make a GRIDS drum track vary and come back to the same variations every few bars.

### Sections with FORM

FORM turns a GRIDS track into a short song form: sections A, B and C, each a point on the Grids map, played in
an order such as AABA. A section played again comes back exactly, so the variations repeat.

1. **GEN**: **MODE GRIDS**; set **X** and **Y** (where on the map) and **DENS** (how full). This is section A.
2. **EVOLVE**: **EVERY** sets how long each section lasts: 1, 2, 4, 8 or 16 bars. (Turning FORM on with EVERY
   OFF sets 4 bars.)
3. **FORM**: **FORM** picks the order: AB, ABAC, ABCB, AABA, ABCA or AABC. The whole form lasts the number of
   sections times EVERY: AB at 2 bars repeats every 4 bars, AABA at 4 bars every 16.
4. **FORM**: **CONTR** sets how far a new section moves from the one before: low, a small change of pattern;
   high, a different groove.
5. For exact repeats, take out the other randomness: EVOLVE **LOCK** at 100 (for GRIDS, LOCK is CHAOS turned
   round: 100 is no chaos), and TWEAK **DROP** and **HUMAN** at 0. A lower LOCK brings back a little variation
   in every bar on top of the form.
6. FORM **FLRSH**, TWEAK **SHIFT** and **FOLLOW** do nothing on a GRIDS track (they would change which drum
   plays), so they can stay as they are.

Shaping the sections by hand:

- While a section plays, turn **X** and **Y**: the section keeps where you leave them and plays from there
  every time it comes round. (DENS is one setting for the whole track, not per section.)
- FORM **CLEAR** forgets B and C: the section playing becomes A, and B and C are made again from it by CONTR
  when they are next reached.
- **ROLL** on EVOLVE picks the track's seed, which decides how B and C are pushed away from A.
- The sections are saved with the project.

EVOLVE without FORM (EVERY and AMOUNT set, FORM OFF) is the other way round: every EVERY bars X and Y take a
random step of up to AMOUNT, so the groove keeps wandering and does not come back.

### Movement with an LFO on X or Y

A cord from an LFO synced to the bars moves the point across the map in a loop, the same every time. FORM can
be on as well: the sections set the point, the LFO moves around it.

1. On the GRIDS track, the LFO pages:
   - **LFO 2**: **SYNC** to **4BAR**, **2BAR** or **1/1** (the loop's length); **TRIG** to **FREE**. TRIG NOTE
     (the default) restarts the LFO with every note, and a drum track plays nearly every step, so the LFO would
     hardly move. FREE starts it on PLAY and keeps it in time with the bars.
   - **LFO**: **WAVE** SIN or TRI sweeps back and forth; SAW ramps and jumps back; SQR swaps between two points
     (an A/B change every half loop). **PHS** sets where in its loop it starts. **FADE** at 0.
   - **LFO DEST**: leave PIT, FLT, SHP and AMP at 0, so the LFO moves only the cord, not the drum sound.
2. **MODS**: a cord with source **LFO** (this track's), destination **G X** (or **G Y**, **G DENS**) on this
   track, and an amount: how far it moves X from the knob's setting.
3. For a figure on the map, move X and Y with two different loops: a second cord from another track's LFO (with
   its own SYNC, say 2BAR against 4BAR) to **G Y**. With that track's LFO DEST at 0 its sound stays as it is.

Not for repeating: **DRIFT** wanders freely, and **RAND** and the LFO's **S&H** wave pick new values. They make
a groove that keeps changing, which is the other use.
