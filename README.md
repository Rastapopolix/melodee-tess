# Melodee (Tess)

A fork of the M-Vave FM-1 alt-firmware [Melodee](https://github.com/keremimo/melodee) developed by **keremimo** (itself built upon *Felucca* by **Leo Kuroshita / Hügelton Instruments**). GPL-3.0-only. **Status: Experimental.**

---

## Install

Flash it from your browser (Chrome or Edge, FM-1 connected directly by USB):

**[Web installer](https://rastapopolix.github.io/melodee-tess/webapp/installer/)**

After installing, you can edit sounds and sequences from the computer with the [web editor](https://rastapopolix.github.io/melodee-tess/webapp/editor/).

Do not unplug the FM-1 while it is writing. This is experimental firmware and use is at your own risk. The installer page also has a way back to the official V15 firmware, with a backup first.

---

## What It Is

**Melodee (Tess)** adds algorithmic note generators to the four tracks, plus controls for evolving them over time, building song forms from them, and saving them with the project. Each track can stay a normal step sequencer or be switched to a Tessera engine. 

The sound engines, effects, and core infrastructure are unchanged. The build outputs `melodee-tess.fwsc`.

---

## The Name

*Tess* is short for *tessera* (plural *tesserae*), the Latin word for the small tile of stone or glass in a mosaic. Each engine makes a short phrase; EVOLVE and FORM set mutated phrases side by side into sections, so a piece is built from small tiles arranged into a larger pattern. The word also meant a die, which suits the chance-driven engines.

---

## Engines & Workflow

On the **PATTERN** page, the **MODE** knob switches a track between the original step sequencer (`STEP`) inherited from upstream Melodee and one of four new generative engines:

* **Turing**: A looping shift register in the style of a Turing Machine (`LEN`, `PROB`, `SPAN`).
* **Euclid**: Euclidean rhythms (`STEPS`, `PULSE`, `ROT`) using track-saved seed pitches.
* **Markov**: A random walk over 8 scale degrees (`LEAP`, `DENS`, `SPAN`).
* **Morph**: A rhythm blended between two preset patterns (`POS`, `SNP A`, `SNP B`).

---

## Control Pages

* **Evolve Page**: 
  * `LOCK`: Deja vu control determining how much of the phrase repeats.
  * `EVERY`: Mutates the phrase every (`OFF`, `1`, `2`, `4`, `8`, or `16` bars).
  * `AMOUNT`: Degree of change per mutation.
  * `ROLL`: Each step of the knob is a new line; turning back brings the last one again, note for note. The
    small number is the track's seed (1–9999), a marker to find a line again. A seed sets the track's random choices and its starting material (Turing's
    register, Euclid's pitches, Markov's first state); PLAY starts its random choices from the top, so with
    EVERY off a seed always plays the same line. A new track gets a random seed when a generator is first
    selected or played, so each power-on starts somewhere new; a saved project keeps its seeds.

* **Form Page**: 
  * Builds sections from mutated phrases (`AB`, `ABAC`, `ABCB`, `AABA`, `ABCA`, or `AABC`). 
  * `CONTRAST`: Varies new sections relative to previous ones.
  * `CLEAR`: Forgets B and C, resetting the current phrase to A.

* **Tweak Page**: 
  * `SHIFT`: Transposes up to 24 semitones.
  * `DROP`: Probability of dropping a hit across any engine.
  * `HUMAN`: Adds velocity and gate variation.
  * `FOLLOW`: Plays another Tessera track's pitch, shifted, in this track's scale.

*(Note: The **FLOURISH** knob is located on the FORM page, shifting the last beat of the last bar of each phrase into section transitions).*

---

## Saving & Storage

Tessera state is saved with the project into its own dedicated flash object (`OBJ_TESSERA` at `0x9D000`, utilizing the free sector pair) because the main project record is near capacity. Since SEED the object holds blocks of version 2 ("TSR2", with each track's seed); projects and backups saved before it still load, every setting kept, each track given a seed of its own. 

* **Tagging & Validation**: Each slot's block carries a hash of its parent project record. If hashes mismatch, the project loads with Tessera `OFF` to prevent stale data. Every field is range-checked.
* **Compatibility**: Projects saved by stock Melodee load normally with Tessera `OFF`, and projects saved by this fork remain readable.

---

## Visual Reference Gallery

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
