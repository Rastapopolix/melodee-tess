# Recipes (for the manual)

Notes gathered for the online manual (ROADMAP phase 7). Each recipe uses only what the firmware already has;
the page and knob names are the ones on the screen. Checked against the code; marked where a step still
needs a check on the device.

## SID: chip arpeggios (C64 POLY)

A held chord played the way a C64 tune plays one: a note per frame, fast enough to sound like a chord.
From 1.2.7 (upstream Melodee 1.4's SID).

1. Pick **SID** as the track's engine.
2. EDIT, **CHIP** page: **MODEL** to **C64 6581** or **C64 8580**. The track is now one channel of a C64:
   one voice, and its pitch, pulse width and filter change once a PAL frame (50 Hz), as a player routine
   writes them.
3. EDIT, **VOICE** page: **VCE** on **POLY** (the default).
4. Hold a chord on the keys. Each frame plays the next held note up from the one sounding, back to the lowest
   past the top. The envelope runs once for the whole chord; the arpeggio goes on through the release.

- A new note is heard first, then joins the arpeggio.
- **MONO** or **LEG** in VCE: one note at a time (PRIO picks which), with the C64's hard restart.
- The speed is the frame rate, as on the C64: it does not follow the tempo.
- The 6581 is the older chip: darker, its filter less even. The 8580 is cleaner.
- Generators play one note a step, so they do not make chords here. *To check on the device:* whether a STEP
  pattern's chords arpeggiate like held keys.

## Grids: variations that repeat every few bars (FORM)

FORM turns a GRIDS track into a short song form: sections A, B and C, each a point on the Grids map, played
in an order such as AABA. A section played again comes back exactly, so the variations repeat.

1. GEN: **MODE GRIDS**; set **X** and **Y** (where on the map) and **DENS** (how full). This is section A.
2. EVOLVE: **EVERY** sets how long each section lasts: 1, 2, 4, 8 or 16 bars. (Turning FORM on with EVERY
   OFF sets 4 bars.)
3. FORM: **FORM** picks the order: AB, ABAC, ABCB, AABA, ABCA or AABC. The whole form lasts the sections
   times EVERY: AB at 2 bars repeats every 4 bars, AABA at 4 bars every 16.
4. FORM: **CONTR** (contrast) sets how far a new section moves from the one before: low, a small change of
   pattern; high, a different groove.
5. For exact repeats, take out the other randomness: EVOLVE **LOCK** at 100 (for GRIDS, LOCK is CHAOS turned
   round: 100 is no chaos), and TWEAK **DROP** and **HUMAN** at 0. Lower LOCK brings back a little variation
   in every bar on top of the form.
6. FORM **FLRSH** (flourish) stays **OFF** on a drum track: it shifts the last beat's notes by semitones, which
   on a DRUM kit means other drums.

Shaping the sections by hand:
- While a section plays, turn **X** and **Y**: the section keeps where you leave them, and plays from there
  every time it comes round. (DENS is one setting for the whole track, not per section.)
- FORM **CLEAR** (turn it) forgets B and C: the section playing becomes A, and B and C are made again from it
  by CONTR when they are next reached.
- **ROLL** on EVOLVE picks the track's seed, which decides how B and C are pushed away from A.
- The sections are saved with the project.

EVOLVE without FORM (EVERY and AMOUNT, FORM OFF) is the other way round: every EVERY bars X and Y take a
random step of up to AMOUNT, so the groove keeps wandering and does not come back.

## Grids: movement that repeats (an LFO on X or Y)

A cord from an LFO synced to the bars moves the point across the map in a loop, the same every time.
FORM can be on as well: the sections set the point, the LFO moves around it.

1. On the GRIDS track, LFO pages:
   - **LFO 2**: **SYNC** to **4BAR**, **2BAR** or **1/1** (the loop's length); **TRIG** to **FREE**.
     TRIG NOTE (the default) restarts the LFO with notes, and a drum track plays nearly every step, so the LFO
     would hardly move. FREE starts it on PLAY and keeps it in time with the bars.
   - **LFO**: **WAVE**: SIN or TRI sweep back and forth; SAW ramps and jumps back; SQR swaps between two
     points (an A/B change every half loop). **PHS** sets where in its loop it starts. **FADE** at 0.
   - **LFO DEST**: leave PIT, FLT, SHP and AMP at 0, so the LFO moves only the cord, not the drum sound.
2. **MODS** page (the cords): a cord with source **LFO** (this track's), destination **G X** (or **G Y**,
   **G DENS**), and an amount: how far it moves X from the knob's setting.
3. For a figure on the map, move X and Y with two different loops: a second cord from another track's LFO
   (its own SYNC, say 2BAR against 4BAR) to **G Y**. With that track's LFO DEST at 0 its sound stays as it is.

Not for repeating: **DRIFT** wanders freely (about 12 seconds a point, not in time with the bars), **RAND**
and the LFO's **S&H** wave pick new values. They make a groove that keeps changing, which is the other use.
