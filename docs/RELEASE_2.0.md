**Tessera 2.0** is a generative firmware for the M-Vave FM-1. It is the next release of Melodee (Tess), now under its
own name: a fork of [Melodee](https://github.com/keremimo/melodee) by keremimo, itself built on *Felucca* by Leo
Kuroshita / Hügelton Instruments (GPL-3.0-only). The [manual](../MANUAL.md) covers every page and the
[recipes](../RECIPES.md) show ways in.

Two **demo projects** come with it ([demos/](../demos/README.md)): an ambient piece and a koto garden.
Put them on your FM-1 to hear what 2.0 does before you build your own.

### ⚠️ Before you install
* **Projects have a new format.** Projects saved by Melodee (Tess) 1.x do not load in 2.0: their slots show
  **OLD FORMAT**, and the first save writes over them. There is no converter. To keep a 1.x project, keep it on 1.x,
  or print the parts you like to STEP and write them down.
* **Back up first.** In the web editor, **Complete backup** > **Save backup** saves the music, slots A to D, the
  sound banks and the settings. A 1.x backup restores on 1.x only.
* The CZ-1 and Prophet sound banks are gone (their flash now holds the projects). FM6's banks and your user presets
  stay.
* **The FM-1 now shows up on your computer as Tessera** (MIDI port **Tessera**, audio **Tessera Out** and **Tessera In**;
  it was Melodee). A DAW that remembered the Melodee port needs the Tessera one picked once. During an update it
  still shows up as Melodee Update, briefly.

### 🎹 Synth engines
2.0 plays three engines, each tuned for generative music.

| Engine | What it is | New in 2.0 |
|---|---|---|
| **FM6** | Six-operator FM in the manner of the DX7: 32 algorithms, operator envelopes and scaling, pitch EG, LFO, the function settings; ENGINE picks MODERN, MARK I or OPL. The main melodic voice. | Back as the main voice. **15 new presets** after INIT VOICE (25 to 39), from ZEN-1 and FuMi-1: SOFT PAD, SOFT SWELL, DEEP PAD, DROP, MALLET, ZEN BELL, RIN, BAMBOO, FURIN, HYOSHIGI, MOKUGYO, KOTO PLUCK, KOTO RING, KOTO WARM, KUGO HARP. The factory presets' releases ring out, so short sequenced notes are not cut off. LFO opens MODS first, as on every engine. |
| **TRIO** | Three oscillators (WAVE, INT2, INT3, DETUNE) into a filter: basses, leads and pads. | Back, with **a warm set of 11 presets** for what FM does not do: WARM PAD, STRING MACH, HOLLOW PAD, SOFT PAD, SUB BASS, SOFT LEAD, BREATH FLUTE, WIND, ANALOG BRASS, FIFTH DRONE, SWEEP PAD. |
| **DRUM** | Eight lanes (BD, SD, CP, CH, OH, TM, RS, CB) on three kits: the **808**, the **909** and the **SID KIT** (every drum on a C64 chip). | The 808 has six tom pitches. ATLAS plays a fourth part on the toms or another lane (below). |

**Gone from the device build:** **SID** as a melodic engine (DRUM's SID KIT stays), **PHYS**, **CZ-1** and
**Prophet**, to free flash, RAM and CPU for the generators. A track saved with SID loads FM6's INIT VOICE; one with
PHYS loads FM6's first voice.

The four sounds a new project starts with are an ambient set: **GLASS PAD**, **STRINGS** and **BELLS** on FM6, the
**808 KIT** on DRUM.

### 🎼 Scales
Seven Japanese and Okinawan scales after **SIKAH** in SCL: **IN**, **YO**, **HIRA** (Hirajoshi), **IWATO**, **KUMOI**,
**RYUKYU** and **MINYO** (Min'yo, the folk-song scale), as 12-tone pitches.

### ✨ Generators and pages
* **CHORD** generator: a progression's chords in the track's scale (POP, 50S, 2-5-1, CIRCLE, TWO, DRONE, WALK, or
  your own with USER), held, on the beat or on every step. On other tracks, **SCL > QNT CHRD** snaps their notes to
  the chord playing now.
* **BREATH** on CHORD (hold OCT+ on GEN, KNOB 4): a held chord's voices each swell and fade on slow cycles of their
  own (11 to 19 s), one now and then sitting out.
* **ENERGY** page (SEQ, after TIMING), the project's slow shape after Zen-1's conductor: **ARC** swings the
  generators between full and sparse over 3 to 6 minutes; **RESTS** lets them fall silent for 8 to 18 s now and then,
  while a CHORD drone holds. Both 0 by default. ENERGY is also a mod source.
* **TIMING** page (after LENGTH): **RATCH** x1 to x8 plays a generator's step 2 to 8 times, as often as **CHANCE**
  says; **SHAPE** places the hits: EVEN, ACCEL (a roll that speeds up) or DECEL (a bounce). On a STEP track TIMING
  holds SHAPE alone, for every step's Ratchet.
* **Ratchet and Nudge per step** on a STEP track: OCT+ on a note (or a drum hit) opens its sheet, where **Ratchet**
  plays the step x1 to x8 in equal parts and **Nudge** moves it up to 8/16 of a step early or 7/16 late.
* **ANSWER** generator (after Zen-1's call and response): play a phrase on the keys and it answers with variations
  that change a little more each time and fade (VARY, ECHO, FADE; OCT+: ORDER, SHIFT, SPACE, LEN).
* **One key for the project**: SCL > **Key link** (LINKED in a new project) sets ROOT on every melodic track at
  once, as the scale already was; OWN keeps a key per track. Saved with the project.
* **MIDI OUT** page (GLO, after SYSTEM): each track's notes on a MIDI channel of its own, 1 to 16, so each track can
  play its own instrument; AUTO works as before (ROUT, the DRUM channel), OFF sends nothing. Saved with the project.
* **HUMAN** (TWEAK) now moves timing too: a generator's step lands up to 20 ms behind the beat at 100, never early,
  the same each time the piece plays.
* **STEP EVOLVE**: on a STEP track, EVOLVE varies your written steps as they play; hold REC to keep the changes.
* **Print**: hold REC on a generator track to put its last bars into STEP and play on from there.
* **ATLAS** (was GRIDS): KICK, SNR and HAT each take their own share of DENSITY, and **TOMS** adds a fourth part from
  the hits the kick and snare just miss, on TOM, CLAP, OPEN HAT, RIM or BELL (a black key with OCT+ held).
* **CONSTELLATION**: straight SPEEDs, a loop LEN, FILL ARP and an OCT+ layer. **MARKOV** and **MORPH** loop over
  PATTERN's LEN.
* **OCT+ layers** on GEN for ATLAS, CHORD, QUAD and CONSTELLATION: hold OCT+ for a second set of knobs.
* **FOLLOW** draws the followed track in its own colour, and can follow STEP tracks.
* **MENU > LIGHTS AUTO**: the panel goes dark 30 s after the last touch and wakes on the first, for long ambient sets.
* **LFO on an FM6 track** opens MODS first, as on every engine; FM6's own FM LFO page comes after the others.
* **Tessera** on ABOUT, the installer and the editor; the power-on mosaic now holds a moment (a button skips it).
* Six TRIO presets (FAT BASS, SUB BASS, ARP LEAD, SOFT LEAD, ANALOG BRASS, BREATH FLUTE) release with a short fade
  instead of stopping dead on short steps.
* Tracks stay on the beat after a DIV or tempo change.

### 🏷️ New names
Other makers' product names are no longer used on screen; the credits still name the originals.
* **GRIDS → ATLAS** (inspired by Émilie Gillet's Grids, Mutable Instruments; mod destinations AT X, AT Y, AT DENS,
  AT KICK, AT SNR, AT HAT, AT TOMS)
* **SLOTH → TIDE** (inspired by Nonlinear Circuits' Triple Sloth): TIDE 1, 2 and 3 by speed
* **OCHD → SWARM** (inspired by Instruō and DivKid's ochd): SWARM 1 to 8

Saved projects and mods keep working across the renames: only the names on screen changed.

### 🎨 The look
Tessera's own palette **SMALTI** (lapis and gold), square tiles, Elektron-style dials, the Chakra Petch typeface, a
mosaic splash, page tabs, a colour per track, bigger GEN pictures, and a help card that zooms and scrolls (hold ARP,
turn a knob). MARMO, VITRO, NIGHT and CONTRAST are in MENU > COLOR. Lists: KNOB 1 picks the row, KNOB 2 changes it.
MENU > TRACK KNOB sets which knob picks the track.

### 📦 Installation
Flash it with the [web installer](https://rastapopolix.github.io/tessera/webapp/installer/) (Chrome or Edge), or
download a `.fwsc` from the assets below:
* `tessera-2.0.fwsc`: the standard (dual-core) build. Start here.
* `tessera-2.0-1core.fwsc`: single core, for FM-1s that crash with the standard build (red TESSERA CRASH
  screen, freezes, or restarts into update mode).

Do not unplug the FM-1 while it is writing. This is experimental firmware: use it at your own risk.

### 🙏 Credits
* **Melodee** by keremimo, and **Felucca** by Leo Kuroshita / Hügelton Instruments: the firmware Tessera is built on.
  TRIO comes from Felucca.
* **Grids** by Émilie Gillet / Mutable Instruments: ATLAS' drum map and the way it is read.
* **ZEN-1** by aceyprime and **FuMi-1** by cartesive: the 15 FM6 presets above (SOFT PAD to FURIN from ZEN-1;
  HYOSHIGI, MOKUGYO, the kotos and KUGO HARP from FuMi-1), and ZEN-1's ideas behind BREATH and ENERGY.
* **norns** scripts, as ideas restated in C: **Awake** (tehn), **Fall** (ambalek), **Constellation**
  (timothy-taylor), **initenere** (QUAD) and **nørgård** (SERIES).
* **Turing Machine** (Music Thing Modular), **Triple Sloth** (Nonlinear Circuits) and **ochd** (Instruō, DivKid).

Every third-party part and its licence is listed in [LICENSING.md](../LICENSING.md).
