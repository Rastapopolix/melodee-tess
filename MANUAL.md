# Melodee (Tess) manual

This covers what Tess adds to Melodee, as in **v1.3.3**: the generators, the SEQ MODE pages, seeds, TWEAK 2, the
modulation cords (MODS) with their organic sources and MATRIX, the mod shortcut and the help card. Everything else (the step sequencer, the effects, saving) works as
in upstream [Melodee](https://github.com/keremimo/melodee).

Melodee (Tess) is the testbed for **Tessera**, a standalone FM-1 firmware in progress with its own generative character. Expect features to change between releases.

**Sound engines.** v1.3.3 offers SID, TRIO, PHYS and DRUM. FM6, CZ-1 and Prophet are not in the build: a track
saved with one of them loads SID INIT. [v1.2.7](https://github.com/Rastapopolix/melodee-tess/releases/tag/v1.2.7)
still has them. At power-on and on a new project, track 1 starts on PHYS DRONE STRING.

Page and knob names are the ones on the screen. KNOB 1 to KNOB 4 are the four knobs left to right, and each
page's four settings sit along the top of the screen in the same order.

- [Shortcuts](#shortcuts)
- [SEQ and the MODE](#seq-and-the-mode)
- [The generators (GEN)](#the-generators-gen)
- [EVOLVE: mutation, LOCK and ROLL](#evolve-mutation-lock-and-roll)
- [FORM: sections](#form-sections)
- [TWEAK: shift, drop, human, follow](#tweak-shift-drop-human-follow)
- [TWEAK 2: branch, logic, cond](#tweak-2-branch-logic-cond)
- [MODS: the modulation cords](#mods-the-modulation-cords)
- [ORGANIC: SLOTH and OCHD](#organic-sloth-and-ochd)
- [MATRIX: the mods at a glance](#matrix-the-mods-at-a-glance)
- [Mod shortcut](#mod-shortcut)
- [Saving](#saving)
- [Recipe: Grids variations that repeat](#recipe-grids-variations-that-repeat)

---

## Shortcuts

| Do this | And this happens |
|---|---|
| Tap **SEQ** | Opens **GEN** first, then the pages of the track's MODE (see [SEQ and the MODE](#seq-and-the-mode)). |
| **GEN**, KNOB 1 | **MODE**: STEP, ARP, TURING, EUCLID, MARKOV, MORPH, GRIDS, BOUNCE, AWAKE or POLY for the selected track. |
| Tap **ARP** | Switches the track to ARP's MODE and opens **ARP**; tap again for **ARP 2**. |
| Hold **ARP** and turn any knob | **Help**: a card with that knob's parameter, its value and what it does. The value is not changed. Let go of ARP to close it (no page opens). Works on every page. |
| Hold **FX** and turn any knob | **Mod shortcut**: sets how far a cord moves that knob's parameter (see [Mod shortcut](#mod-shortcut)). |
| **GEN** on an AWAKE track, a white key | Picks that step: KNOB 1 sets its A, KNOB 2 its B. The same key lets it go. |
| Tap **LFO** until **MODS** | The cords page, after LFO, LFO 2, LFO DEST and ORGANIC. MODS replaces upstream's MOD page. MATRIX follows it. |
| **MATRIX**, OCT+ | Opens the picked mod in MODS. |
| **EVOLVE**, KNOB 4 (**ROLL**) | Each click is a new seed (1 to 128), so a new line. Turn back to hear the previous one again, note for note. A quick turn jumps further. While playing, the new line starts on the track's next bar line and the number blinks until then. |
| **FORM**, KNOB 1 with EVERY OFF | Turning FORM on sets EVERY to 4 bars, so the sections have a length. |
| **FORM**, KNOB 3 (**CLEAR**) | Turn it either way: B and C are forgotten and the section playing becomes A. |
| **MODS**, KNOB 2 | Picks the cord (1 to 16) the other three knobs edit. |
| **MODS**, KNOB 3 on a new cord | Choosing a destination sets the amount to 50 % (or a fixed move of 25 % with no source) so you hear it at once. |
| **PLAY** | Each generator's random choices start again from the top of its seed: with EVERY OFF, a seed always plays the same line. |

---

## SEQ and the MODE

SEQ holds the step sequencer, the arpeggiator and the generators. Tap **SEQ** for **GEN**, where **KNOB 1, MODE**
picks what the selected track plays. SEQ then shows only that mode's pages:

| MODE | SEQ's pages |
|---|---|
| **STEP** | GEN, NOTES, PATTERN, TEMPO, PATTERNS, SONG |
| **ARP** | ARP, ARP 2, NOTES, PATTERN, TEMPO, PATTERNS, SONG, GEN |
| a generator | GEN, EVOLVE, FORM, TWEAK, TWEAK 2, PATTERN, TEMPO, PATTERNS, SONG |

MOTION and DRUM HIT appear as in upstream. The **ARP** button switches the track to ARP and opens its page.
Choosing STEP or a generator turns the arpeggiator off; choosing ARP again brings back its last setting.

---

## The generators (GEN)

Each of the four tracks can play its own steps (**STEP**, upstream's sequencer) or have its notes made by a
generator. Tap **SEQ** for the **GEN** page and pick one with **KNOB 1, MODE**. KNOB 2 to 4 are the
generator's own settings:

| MODE | KNOB 2 | KNOB 3 | KNOB 4 | What it plays |
|---|---|---|---|---|
| **TURING** | LENGTH (2–16 steps) | PROB (0–100) | SPAN (1–3 oct) | A looping shift register, as a Turing Machine. PROB is the chance a step changes as it comes round: 0 locks the loop, 100 is new every pass. |
| **EUCLID** | STEPS (1–32) | PULSE (0–STEPS) | ROT | Hits spread as evenly as they go over STEPS, turned round by ROT, on pitches picked by the seed. |
| **MARKOV** | LEAP (0–100) | DENS (5–100) | SPAN (1–3 oct) | A random walk over 8 scale degrees. LEAP 0 moves to neighbours, 100 leaps widely. DENS is the chance a step plays. |
| **MORPH** | POS (0–127) | SNP A (0–7) | SNP B (0–7) | A rhythm blended from preset A (POS 0) into preset B (POS full). |
| **GRIDS** | X (0–255) | Y (0–255) | DENS (0–100) | Mutable Instruments Grids' drum map: kick, snare and closed hat (notes 36, 38, 42) from the point X, Y; between patterns, a blend. Best on a DRUM track. |
| **BOUNCE** | SPEED (1–64) | ANGLE (0–90°) | BALLS (1–4) | Balls bouncing in a box: a note where a ball hits a wall. Faster means more hits. 0° and 90° go back and forth; an angle between draws a longer loop. Each extra ball starts from its own place and angle; balls hitting on the same step play together, three notes at most. |
| **AWAKE** | LEN A (1–16) | LEN B (1–16) | PROB (0–100) | Two loops, as norns' Awake. A is the melody (scale degrees or rests); B lifts it up the scale. Lengths that differ shift B against A. PROB is the chance each of A's notes plays. ROLL draws the loops, EVOLVE changes a few steps. Set steps by hand from the keys (below). |
| **POLY** | RHY (1–4) | DIV (OFF, 1–16) | SPAN (1–3 oct) | Four notes stepped on by four rhythms. RHY picks which rhythm DIV edits: every how many steps it moves the notes on. Rhythms that land together play once, louder: 4 against 3 is a polyrhythm. SPAN spreads the notes over up to 3 octaves. |

**AWAKE step entry.** On GEN with an AWAKE track, the keys are its steps and play no sound. A white key picks a
step: KNOB 1 sets its A value, KNOB 2 its B. Press the same key again, or leave the page, to go back to the usual
knobs.

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

The page draws its four settings: SHIFT as a bar raised from a dashed home line, DROP as eight hits with the dropped
ones hollow, HUMAN as uneven velocity bars, and FOLLOW as the four tracks with the followed one lit.

| Knob | Setting | What it does |
|---|---|---|
| 1 | **SHIFT** (±24) | Transposes by semitones; while following, by scale steps from the followed note. |
| 2 | **DROP** (0–100) | The chance a hit is left out, whatever the generator. |
| 3 | **HUMAN** (0–100) | Varies velocity and gate a little, as a player would. |
| 4 | **FOLLOW** | OFF, or T1 to T4: plays that track's pitch, SHIFT away, in this track's scale. STEP tracks can be followed too. |

On a GRIDS track only DROP and HUMAN apply: SHIFT, FOLLOW and FLOURISH would change which drum plays.

---

## TWEAK 2: branch, logic, cond

TWEAK 2 comes after TWEAK on a generator track. It decides which hits play, and where. Everything starts OFF, so
a track plays as before until you turn it on. The picture shows BRANCH as a fork (in COIN mode each arm shaded by
its share of the hits), the four tracks with the one it branches to lit, the hits, and the bars COND lets
through.

| Knob | Setting | What it does |
|---|---|---|
| 1 | **BRANCH** (0–100 %) | The chance a hit plays on track TO's sound instead of this one's, so two tracks answer each other. 0 is off. The tosses replay with the seed. |
| 2 | **TO** | OFF, or another track and a mode: **COIN** tosses each hit, **TOGGLE** (TGL) switches side on a toss won (100 % alternates), **LATCH** tosses once a bar and keeps the bar's hits on one side. |
| 3 | **LOGIC** | OFF, or AND, OR, XOR or NOT with another track's hits: AND plays only when both hit, OR when either does, XOR when one does, NOT only when the other rests. A lower track's hit is read on this step, a higher one's on the step before. |
| 4 | **COND** | The bars a hit may play in, counted from PLAY (with FORM on, from the section's start): OFF, 1:2, 2:2, 1:4 to 4:4, 1:8, 8:8, FIRST (only the first bar) or !FIRST (the rest). 1:4 is the first bar of every four. |

BRANCH is also a MODS destination, so an LFO or OCHD can sweep how often hits branch.

---

## MODS: the modulation cords

MODS is a modulation bus: 16 cords, each from a source on any track to a destination on any track. Tap
**LFO** until **MODS**. It shows four cords to a screen with an activity light each.

| Knob | What it sets |
|---|---|
| 1 | **Source**: `--`, then per track (T1 to T4) LFO, ENV, VEL, KEY, RAND, MODW, AT, EXPR, S&H, SLEW, the generator's GATE, NOTE and CV, DRIFT, SLOTH's TORPOR, APATHY and INERTIA, and OCHD 1 to 8. |
| 2 | **Cord**: which of the 16 the other knobs edit. |
| 3 | **Destination**, on a track: the voice (PITCH, CUTOFF, SHAPE, GAIN), the mix (LEVEL, PAN, SPREAD, FX SEND), the envelope and LFO settings, the engine's E1 to E8, and the generators' settings (T PROB, T LEN, E PULSE, E ROT, M LEAP, M DENS, MP POS, SHIFT, DROP, HUMAN, BRANCH, EV AMT, CONTR, G X, G Y, G DENS). |
| 4 | **Amount**, −100 to +100 %. With no source (`--`) the cord is a fixed move and KNOB 4 sets its offset. |

- **DRIFT** is each track's slow wander: a new point about every 12 seconds, eased between, not in time with the bars.
- **SLOTH** and **OCHD** are slow organic sources, set on the ORGANIC page (below).
- A new project starts with all 16 cords empty: nothing moves until you patch it. (Before v1.3.3 a new project
  came with DRIFT on each track's CUTOFF and FX SEND.)
- **Live marks**: when a cord reaches a knob, that knob's card shows a mark where the cords are moving it now, and
  each MODS row has a mark that moves with its cord.
- Routes saved by upstream's MOD page (in a project or a sound) become cords when they load. A sound's cords
  go with it when another sound is loaded on the track, unless you edited them. If all 16 are in use, the
  screen says **MODS FULL**.
- The cords are saved with the project.

---

## ORGANIC: SLOTH and OCHD

Each track has its own SLOTH and OCHD, slowly moving sources for the cords in the style of Nonlinearcircuits'
Sloth and Instruō's ochd. Tap **LFO** until **ORGANIC** to set their speed and watch them move.

| Knob | Setting | What it does |
|---|---|---|
| 1 | **SPEED** (0–100) | How fast the track's SLOTH drifts. At 50, its three chaotic circuits take about 17 minutes (**TORPOR**), 1 minute (**APATHY**) and 3 minutes (**INERTIA**) a turn; each 12.5 more is twice as fast. |
| 2 | **RATE** (0–100) | How fast the track's eight OCHD LFOs run. At 50, **OCHD 1** takes 64 s a turn and **OCHD 8** 2.4 s; each 12.5 more is twice as fast. |

The picture shows SLOTH's three circuits as dots on their lines and the eight OCHD LFOs as bars. **PLAY** restarts
them from the track's seed, so a piece replays the same way. Pick them as sources on MODS.

---

## MATRIX: the mods at a glance

MATRIX comes after MODS. It shows the cords as a 4 x 4 grid, source track down the left (FROM) and destination
track along the top (TO). A cell is lit when a cord goes from its row's track to its column's, as bright as the
cords move it now. A cell holding more than one cord shows how many.

| Knob | What it does |
|---|---|
| 1 | **FROM**: the row. |
| 2 | **TO**: the column. |
| 3 | **MOD**: which of the cell's cords, when it holds more than one (its number in MODS). |
| 4 | **OPEN**: press **OCT+** to open that cord in MODS and change it. |

---

## Mod shortcut

Hold **FX** and turn a knob on any page: a cord from the track's shortcut source (its LFO, unless you pick
another by turning SELECT while FX is held) to that knob's parameter on the same track. Turning sets the cord's
amount, −100 to +100 %, like the attenuverter beside a knob on a Mutable module.

- The first turn makes the cord in a free MODS slot; back at 0 the slot is free again.
- If a cord in MODS already reaches that parameter on this track, that cord is the one edited, whatever its source.
- A parameter no cord can reach shows **no mod** and stays as it is.
- While FX is held, the four knobs' amounts show over the page. Letting FX go closes them; no page opens.

---

## Saving

Generator settings, seeds, sections and cords are saved with the project, in their own block next to it.
Projects saved by earlier Tess versions load and convert (the format is TSR5 since v1.3.3; BOUNCE, AWAKE and POLY
start from their defaults on projects older than v1.3.1, and TWEAK 2, SLOTH and OCHD on projects older than
v1.3.3, so old projects play as before). Stock Melodee projects load with every track on STEP.

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
