# Melodee (Tess)

A fork of the M-Vave FM-1 alt-firmware [Melodee](https://github.com/keremimo/melodee) developed by **keremimo** (itself built upon *Felucca* by **Leo Kuroshita / Hügelton Instruments**). GPL-3.0-only. **Status: Experimental.**

**Latest release: [v1.2.7](https://github.com/Rastapopolix/melodee-tess/releases/latest)**, based on Melodee 1.3.6 with picks from 1.4.1. Also listed on [altstrument.org](https://altstrument.org/firmware/m-vave-fm-1/melodee-tess).

**[Try it in your browser](https://rastapopolix.github.io/melodee-tess/webapp/sim/)**: the firmware running on an on-screen FM-1, with the same engines, sequencer and Tessera generators. No FM-1 or install needed; press *Play the demo* to hear the generators.

---

## Install

Flash it from your browser (Chrome or Edge, FM-1 connected directly by USB):

**[Web installer](https://rastapopolix.github.io/melodee-tess/webapp/installer/)**

After installing, you can edit sounds and sequences from the computer with the [web editor](https://rastapopolix.github.io/melodee-tess/webapp/editor/).

Do not unplug the FM-1 while it is writing. This is experimental firmware and use is at your own risk. The installer page also has a way back to the official V15 firmware, with a backup first.

---

## What It Is

**Melodee (Tess)** adds algorithmic note generators to the four tracks, plus controls for evolving them over time, building song forms from them, and saving them with the project. Each track can stay a normal step sequencer or be switched to a Tessera engine. 

The build outputs `melodee-tess.fwsc` (standard, dual core) and `melodee-tess-1core.fwsc` (single core, for FM-1s that crash with the standard build).

---

## Using it

**[Read the manual](MANUAL.md)** for every Tess page, knob and shortcut, and recipes such as
[Grids variations that repeat](MANUAL.md#recipe-grids-variations-that-repeat).

The short version:

* **Tap ARP** for the generator pages: **GEN**, **EVOLVE**, **FORM**, **TWEAK**, then the arpeggiator's ARP and ARP 2. Tap again for the next page.
* **GEN, KNOB 1 (MODE)** switches a track between the step sequencer (`STEP`) and a generator:
  * **Turing**: a looping shift register in the style of a Turing Machine (`LENGTH`, `PROB`, `SPAN`).
  * **Euclid**: Euclidean rhythms (`STEPS`, `PULSE`, `ROT`) on pitches picked by the track's seed.
  * **Markov**: a random walk over 8 scale degrees (`LEAP`, `DENS`, `SPAN`).
  * **Morph**: a rhythm blended between two preset patterns (`POS`, `SNP A`, `SNP B`).
  * **Grids**: Mutable Instruments Grids' drum map, kick, snare and hat (`X`, `Y`, `DENS`).
* **EVOLVE** mutates the phrase every few bars (`LOCK`, `EVERY`, `AMOUNT`); **ROLL** picks the track's seed, a line you can always come back to.
* **FORM** builds sections A, B and C into forms such as `AABA`; **TWEAK** transposes, thins, humanises or follows another track.
* **Tap LFO to MODS** for 16 modulation cords between any source and destination on any track, generators included.
* **Hold ARP and turn a knob** to see what that knob does.

---

## Saving & Storage

Generator settings, seeds, FORM sections and the cords are saved with the project, in their own flash object (`OBJ_TESSERA` at `0x9D000`) because the main project record is near capacity. The current block format is version 3 ("TSR3", with Grids); projects and backups saved by earlier Tess versions still load and convert.

* **Tagging & Validation**: Each slot's block carries a hash of its parent project record. If hashes mismatch, the project loads with Tessera `OFF` to prevent stale data. Every field is range-checked.
* **Compatibility**: Projects saved by stock Melodee load normally with Tessera `OFF`, and projects saved by this fork remain readable.

---

## Visual Reference Gallery

*Screens from v1.1: the generator settings were on SEQ's PATTERN page then; they are on GEN now.*

### 1. Turing Engine
![Turing Engine](assets/02_Pattern_Turing.jpg)

### 2. Euclidean Engine
![Euclid Engine](assets/03_Pattern_Euclid.jpg)

### 3. Markov Engine
![Markov Engine](assets/04_Pattern_Markov.jpg)

### 4. Morph Engine
![Morph Engine](assets/05_Pattern_Morph.jpg)

### 5. Evolve Page
![Evolve Page](assets/06_Evolve.jpg)

### 6. Form Page
![Form Page](assets/07_Form.jpg)

### 7. Tweak Page
![Tweak Page](assets/08_Tweak.jpg)

---

## Licence

GPL-3.0-only, inherited from Melodee. Original copyright and SPDX headers are preserved.
