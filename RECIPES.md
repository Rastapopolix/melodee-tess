# Tessera recipes and tips

Short, tested ways to get somewhere with Tessera 2.0. Each uses only what the firmware has; the page and knob names
are the ones on the screen, and [MANUAL.md](MANUAL.md) explains each page in full. Where a step names a page, tap
the button in brackets until it shows: GEN, EVOLVE, FORM, TWEAK, TWEAK 2 and LENGTH are under **SEQ**, MODS, MATRIX
and ORGANIC under **LFO**, SCL and CHORD under **SCL**.

**Recipes**

- [A first generative patch in a minute](#a-first-generative-patch-in-a-minute)
- [A drone that breathes](#a-drone-that-breathes)
- [Chords that lead, melodies that follow (QNT CHRD)](#chords-that-lead-melodies-that-follow-qnt-chrd)
- [Your own progression (CHORD USER)](#your-own-progression-chord-user)
- [Two tracks answering each other (BRANCH)](#two-tracks-answering-each-other-branch)
- [A harmony line that follows (FOLLOW)](#a-harmony-line-that-follows-follow)
- [Atlas: a groove with fills](#atlas-a-groove-with-fills)
- [Atlas: variations that repeat (FORM)](#atlas-variations-that-repeat-form)
- [Atlas: movement that repeats (an LFO on X or Y)](#atlas-movement-that-repeats-an-lfo-on-x-or-y)
- [Polyrhythms: POLY and QUAD](#polyrhythms-poly-and-quad)
- [Notes that travel between tracks (QUAD columns)](#notes-that-travel-between-tracks-quad-columns)
- [A melody you can come back to (CONSTELLATION)](#a-melody-you-can-come-back-to-constellation)
- [A slow lead from the infinity series (SERIES)](#a-slow-lead-from-the-infinity-series-series)
- [Leaves in the wind (FALL)](#leaves-in-the-wind-fall)
- [Pads from sparse notes (LENGTH HIT)](#pads-from-sparse-notes-length-hit)
- [Entries and arrangements (COND and FORM)](#entries-and-arrangements-cond-and-form)
- [From generator to pattern (print, then STEP EVOLVE)](#from-generator-to-pattern-print-then-step-evolve)
- [Movement over minutes (TIDE and SWARM)](#movement-over-minutes-tide-and-swarm)
- [Driving another synth (MIDI OUT)](#driving-another-synth-midi-out)

**[Tips](#tips)**

---

## A first generative patch in a minute

1. Power on. Track 1 is **GLASS PAD**, track 2 **STRINGS**, track 3 **BELLS**, track 4 the **808 KIT**.
2. Track 3 (BELLS): **SEQ** for GEN, **MODE** to **TURING**. Press **PLAY**: a looping melody in the track's scale.
3. **PROB** at about 20: the loop changes a little each time round. At 0 (Lock) it holds.
4. Track 4: MODE **ATLAS**, **DENS** at 50. A drum groove under it.
5. Track 3 again, **EVOLVE**: turn **ROLL** a click at a time until you like the line. The number is its seed: write
   it down and you can always come back to it.
6. **SCL** on track 3 to change the scale; the generator follows at once.

## A drone that breathes

A held chord that changes slowly, for ambient beds.

1. Track 1 (GLASS PAD): GEN, MODE **CHORD**, **PROG DRONE** (the I chord alone), **BARS 4**, **HIT HOLD**.
2. Hold **OCT+** on GEN: **CHRD DIA7** (a seventh in the scale), **VOICING OPEN**, **OCTAVES −1** for weight.
3. **MODS**: a cord from **T1 TIDE 1** (TIDE's slowest circuit) to **T1 CUTOFF**, amount about +30 %.
4. A second cord from **T1 SWARM 2** to **T1 PAN**, about +20 %: the pad drifts across the stereo field.
5. For a drone that moves through the scale, set PROG to **WALK**: four chords from the seed, and EVOLVE (EVERY 8,
   AMOUNT 30) changes one now and then.

## Chords that lead, melodies that follow (QNT CHRD)

1. Track 2 (STRINGS): MODE **CHORD**, **PROG POP**, **BARS 1**, **HIT BEAT**.
2. Track 3 (BELLS): MODE **MARKOV**, **LEAP 30**, **DENS 60**.
3. On track 3, **SCL**: **QNT** to **CHRD**. Its notes now snap to the chord track 2 plays, so the melody always
   fits the harmony. The keys and MIDI input snap too, so you can play along on track 3.
4. Try PROG **2-5-1** or **CIRCLE** for a jazzier walk; the melody follows every change.

## Your own progression (CHORD USER)

1. A CHORD track: **PROG USER**.
2. Hold **OCT+** on GEN and play the roots on the keys, one key a chord: for example C, A, F, G. The keys are silent
   while you enter. Notes outside the scale round down into it.
3. Let OCT+ go. LENGTH (KNOB 4 with OCT+ held) is now 4; shorten it to leave the last chords out.
4. The chords' kind comes from **CHRD** (OCT+ layer, KNOB 2): OFF for triads, **DIA7** for sevenths.

## Two tracks answering each other (BRANCH)

1. Two melodic tracks, for example track 1 GLASS PAD and track 3 BELLS. Track 3: MODE **TURING**.
2. Track 3, **TWEAK 2**: **TO** to **T1 COIN**, **BRANCH 40 %**. About four hits in ten now play on the pad.
3. **TOGGLE** instead of COIN alternates: at 100 % every other hit goes across, a strict call and response.
4. **LATCH** tosses once a bar: whole bars swap between the two sounds.
5. The tosses replay with the seed. A cord from a **SWARM** to **BRANCH** sweeps how often hits cross over.

## A harmony line that follows (FOLLOW)

1. Track 3 plays a generator melody.
2. Track 2: any generator, then **TWEAK**: **FOLLOW T3**, **SHIFT +2**. Track 2 plays track 3's pitches two scale
   steps up, in its own scale and its own rhythm.
3. **SHIFT −7** for a line an octave below in a seven-note scale. **DROP 50** thins the harmony so it only answers
   now and then.

## Atlas: a groove with fills

1. Track 4 (808 KIT): MODE **ATLAS**, **X** and **Y** where you like the groove, **DENS 60**.
2. Hold **OCT+** on GEN: **TOMS 40** brings in the hits the kick and snare only just miss, so fills follow the
   groove. Press a **black key** (still holding OCT+) to put that part on **CLAP**, **OPEN HAT**, **RIM** or **BELL**
   instead.
3. Still holding OCT+: **HAT 30** thins the hats, **KICK 100** keeps the kick full.
4. **TWEAK 2**: **COND 4:4** on a second ATLAS track, with the fourth part on CLAP and DENS high, gives a fill on
   every fourth bar only.

## Atlas: variations that repeat (FORM)

FORM turns an ATLAS track into a short song form: sections A, B and C, each a point on the Atlas map, played in an
order such as AABA. A section played again comes back exactly, so the variations repeat.

1. GEN: **MODE ATLAS**; set **X** and **Y** (where on the map) and **DENS** (how full). This is section A.
2. EVOLVE: **EVERY** sets how long each section lasts: 1, 2, 4, 8 or 16 bars. (Turning FORM on with EVERY OFF sets
   4 bars.)
3. FORM: **FORM** picks the order: AB, ABAC, ABCB, AABA, ABCA or AABC. The whole form lasts the sections times
   EVERY: AB at 2 bars repeats every 4 bars, AABA at 4 bars every 16.
4. FORM: **CONTR** (contrast) sets how far a new section moves from the one before: low, a small change of pattern;
   high, a different groove.
5. For exact repeats, take out the other randomness: EVOLVE **LOCK** at 100 (for ATLAS, LOCK is CHAOS turned round:
   100 is no chaos), and TWEAK **DROP** and **HUMAN** at 0. A lower LOCK brings back a little variation in every bar
   on top of the form.
6. FORM **FLRSH**, TWEAK **SHIFT** and **FOLLOW** do nothing on an ATLAS track (they would change which drum plays).

Shaping the sections by hand:

- While a section plays, turn **X** and **Y**: the section keeps where you leave them and plays from there every
  time it comes round. (DENS is one setting for the whole track, not per section.)
- FORM **CLEAR** forgets B and C: the section playing becomes A, and B and C are made again from it by CONTR when
  they are next reached.
- **ROLL** on EVOLVE picks the track's seed, which decides how B and C are pushed away from A.

EVOLVE without FORM (EVERY and AMOUNT set, FORM OFF) is the other way round: every EVERY bars X and Y take a random
step of up to AMOUNT, so the groove keeps wandering and does not come back.

## Atlas: movement that repeats (an LFO on X or Y)

A cord from an LFO synced to the bars moves the point across the map in a loop, the same every time. FORM can be on
as well: the sections set the point, the LFO moves around it.

1. On the ATLAS track, the LFO pages:
   - **LFO 2**: **SYNC** to **4BAR**, **2BAR** or **1/1** (the loop's length); **TRIG** to **FREE**. TRIG NOTE (the
     default) restarts the LFO with every note, and a drum track plays nearly every step, so the LFO would hardly
     move. FREE starts it on PLAY and keeps it in time with the bars.
   - **LFO**: **WAVE** SIN or TRI sweeps back and forth; SAW ramps and jumps back; SQR swaps between two points (an
     A/B change every half loop). **PHS** sets where in its loop it starts. **FADE** at 0.
   - **LFO DEST**: leave PIT, FLT, SHP and AMP at 0, so the LFO moves only the cord, not the drum sound.
2. **MODS**: a cord with source **LFO** (this track's), destination **AT X** (or **AT Y**, **AT DENS**, **AT TOMS**) on
   this track, and an amount: how far it moves X from the knob's setting.
3. For a figure on the map, move X and Y with two different loops: a second cord from another track's LFO (with its
   own SYNC, say 2BAR against 4BAR) to **AT Y**. With that track's LFO DEST at 0 its sound stays as it is.

Not for repeating: **DRIFT** wanders freely, and **RAND** and the LFO's **S&H** wave pick new values. They make a
groove that keeps changing, which is the other use.

## Polyrhythms: POLY and QUAD

**Inside one track (POLY):**

1. MODE **POLY**. Its four notes step on whenever one of its rhythms fires. By default rhythm 1 is **/4** and
   rhythm 2 **/3**: 4 against 3.
2. **RHY 3**, then **DIVISION /5**: a third rhythm against the other two. Where rhythms land together the note
   plays once, louder, so the accents show the pattern.
3. **SPAN 2** spreads the four notes over two octaves.

**Across tracks (QUAD):**

1. Tracks 1, 2 and 3: MODE **QUAD**. They share one 4 x 4 grid of notes, a row each.
2. Give each a different **DIV** on **PATTERN**: 1/16, 8T and 1/8. Each plays its row at its own speed, and the
   rows drift against each other.
3. Track 3: **DIR PEND** (back and forth) or **RAND**.

## Notes that travel between tracks (QUAD columns)

1. Two or more tracks in **QUAD**, as above.
2. Hold **OCT+** on GEN. KNOB 1 picks a column, KNOB 2 its **MOVE** (**DOWN**: each note moves a track down,
   track 4's to track 1), KNOB 3 its **EVERY** (/4: every four steps of the first QUAD track).
3. Set a different MOVE on another column (UP, or RAND). A phrase on one track now turns up on the next a few beats
   later, as a canon.
4. **RANGE** (KNOB 4 on the layer) **1 OCT** folds every row into one octave from the root: tighter, more like a
   riff.
5. **LINK** on GEN: a fast QUAD track pulls slower ones along. With DIR **HOLD** a track plays only when the linked
   one moves it.

## A melody you can come back to (CONSTELLATION)

1. MODE **CONSTL**, **DENS 60**, **SPEED 1/16**. The stars that cross the crosshair play.
2. Hold **OCT+**: **SIZE 3** catches stars three rows either side, for wider leaps. **LEN 8** and **LOCK 70**: most
   steps now play the eight-step loop of stars already caught, so a phrase forms and repeats with small changes.
3. **FILL ARP** fills the steps between stars with the last four stars, low to high and softer.
4. Let the ship steer itself: **MODS**, a cord with no source (`--`) to **C AUTO** on this track, offset to taste:
   low values wander slowly, **SEEK** flies to the next star.
5. **Y** sets where the melody sits; a slow LFO or SWARM on **C Y** moves it up and down the sky.

## A slow lead from the infinity series (SERIES)

1. MODE **SERIES** on a lead sound (BELLS or an FM6 tine). PATTERN **DIV 1/8** or slower.
2. **INTV 1**: close, stepwise lines. **INTV 3** or **4**: wider leaps (folded back into two octaves).
3. **LOOP OFF** never repeats; **LOOP 8** repeats eight notes as a motif. EVOLVE (EVERY 8) picks new STARTs, so the
   motif moves on now and then.
4. **LENGTH** page: **HIT**, so each note rings until the next one.

## Leaves in the wind (FALL)

1. MODE **BOUNCE**, **BALLS** turned past 4 to **Fall**. Eight leaves fall and play where they land: left low, right
   high, one in seven an octave down.
2. **ANGLE** is now **WIND**: the middle is calm; turning it leans the melody low or high. Further out, gusts send the
   leaves down together, in bursts.
3. **MODS**: a cord from a **SWARM** (SWARM 3, about 25 s a turn) to **B ANGLE**: the wind rises and falls.
4. **SPEED** slower (10 to 20) for a sparse, rain-like part.

## Pads from sparse notes (LENGTH HIT)

1. Any generator on a pad sound (GLASS PAD, STRINGS). Set **DENS** or **PROB** so it plays few notes.
2. **LENGTH** page: **LENGTH HIT**. Each note holds until the next hit, so rests let notes ring and the line turns
   into a slowly changing pad.
3. For plucks instead: **FIXED**, **LEN 1/16**, **KEYS 2**.

## Entries and arrangements (COND and FORM)

1. Give two tracks the same **EVERY** (EVOLVE) and turn **FORM** on both, for example AABA on the drums and ABAC on
   the bass. Their sections change together.
2. **TWEAK 2**, **COND**: **!FIRST** keeps a track silent for the first bar after PLAY (or each section start, with
   FORM on), so it enters late. **1:4** plays only on the first bar of every four; **4:4** only on the last, for a
   turnaround.
3. **CONTR** high on the bass and low on the drums: the bass moves a lot between sections, the groove only a little.
4. **FLRSH +7** on a melodic track moves the notes on the last beat of every phrase up a fifth, a small pickup into
   the next.

## From generator to pattern (print, then STEP EVOLVE)

1. Let a generator play until you hear a line you like.
2. Hold **REC**: its last bars go into the track's STEP pattern (PRINTED), and the track switches to STEP and plays
   on without a gap. Hold **EDIT** to undo, which brings the generator back.
3. Now it is ordinary steps: edit notes on **NOTES**, or the keys.
4. **EVOLVE** (STEP EVOLVE): **AMOUNT 30**, **EVERY 2**, **LOCK 50**. The steps you wrote are varied as they play:
   a note moved, one dropped, a ghost note. The steps themselves do not change.
5. Like a variation? Hold **REC** to keep it (KEPT n STEPS). **CLEAR** goes back to the steps as written.

## Movement over minutes (TIDE and SWARM)

1. **LFO** to **ORGANIC** on any track: **SPEED** and **RATE** at 50 to start.
2. **MODS**: **T1 TIDE 3** (about a minute a turn) to **T2 CUTOFF**; **T1 TIDE 2** (about 3 minutes) to **T3 FX
   SEND**; **T1 SWARM 5** (about 10 s) to **T4 AT DENS**.
3. Press **OCT+** on a mod to make it **UNI**: it moves only up from the knob, never below, which is often what you
   want for FX SEND and density.
4. **MATRIX** shows the patch at a glance: rows are sources, columns destinations, lit as they move.
5. Because PLAY restarts TIDE and SWARM from the seed, the same project unfolds the same way each time.

## Driving another synth (MIDI OUT)

1. Hold **HOME** for the menu: **MIDI OUT** to **ALL**. The steps, the ARP and every generator now go out over USB
   MIDI, each track on its own channel: AUTO gives 1 to 4 (GLO > SYSTEM ROUT at CH1-4; the drum track on its DRUM
   channel), or pick each track's channel, or OFF, on **GLO > MIDI OUT**.
2. **CLOCK OUT** on: the other synth or DAW follows Tessera's tempo, START and STOP included.
3. Turn the track's **LEVEL** down on the MIXER if you only want to hear the other synth.

---

## Tips

**Finding your way**

- Hold **ARP** and turn any knob for its help card. Keep turning the same knob to scroll a long text.
- **EDIT** on an EDIT page opens the map of the sound's pages: KNOB 1 across, KNOB 2 down, OCT+ opens. Any other
  page button leaves it for its own page.
- In lists (the menu, SCL, VOICE, sheets), **KNOB 1 picks the row and KNOB 2 changes it**.
- Prefer the left knob for picking tracks? MENU > **TRACK KNOB LEFT**.

**Seeds and repeatability**

- The small number on **ROLL** is the track's seed. Note the ones you like: the same seed with the same settings
  always plays the same line.
- **PLAY** restarts every generator's random choices, and TIDE and SWARM, from the top: with EVERY OFF a project
  sounds the same every time. Set EVERY to let it drift.
- On TURING, **PROB 0** (Lock) freezes the loop you are hearing; turn it up a little to let it evolve again.

**Mods**

- Hold **FX** and turn a knob for the quickest mod: it makes the cord for you. Turn SELECT while FX is held to pick
  another source.
- A cord with no source (`--`) is a fixed offset: use it to set things that have no knob, such as **C AUTO**.
- A knob with a mod on it shows a moving mark: you can see what the cords are doing without opening MODS.
- **OCT+** on MATRIX jumps straight to that cord in MODS.

**Saving and safety**

- **SAVE + REC** saves the project to its slot at once (stopping playback first).
- **EDIT held + OCT-** undoes, **+ OCT+** redoes.
- If your FM-1 crashes with the standard firmware, use the `-1core` build. After a crash, MENU > ABOUT shows what
  happened until power-off: please add a photo of it to a bug report.
