# Melodee (Tess)

<p align="center"><img src="docs/media/splash_tess.png" alt="Melodee (Tess) boot splash: MELODEE, TESS, MULTI-ENGINE SYNTH" width="360"></p>

A fork of the M-Vave FM-1 alt-firmware [Melodee](https://github.com/keremimo/melodee) developed by **keremimo** (itself built upon *Felucca* by **Leo Kuroshita / Hügelton Instruments**). GPL-3.0-only. **Status: Experimental.**

Melodee (Tess) is the testbed for **Tessera**, a standalone FM-1 firmware in progress with its own generative character. Expect features to change between releases.

**Latest release: [v1.3.3](https://github.com/Rastapopolix/melodee-tess/releases/latest)**, based on Melodee 1.3.6 with picks from 1.4.1. Also listed on [altstrument.org](https://altstrument.org/firmware/m-vave-fm-1/melodee-tess).

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

* **Tap SEQ** for **GEN**, where **KNOB 1 (MODE)** picks what the track plays; SEQ then shows only that mode's pages. The modes are the step sequencer (`STEP`), the arpeggiator (`ARP`, also the ARP button) and the generators:
  * **Turing**: a looping shift register in the style of a Turing Machine (`LENGTH`, `PROB`, `SPAN`).
  * **Euclid**: Euclidean rhythms (`STEPS`, `PULSE`, `ROT`) on pitches picked by the track's seed.
  * **Markov**: a random walk over 8 scale degrees (`LEAP`, `DENS`, `SPAN`).
  * **Morph**: a rhythm blended between two preset patterns (`POS`, `SNP A`, `SNP B`).
  * **Grids**: Mutable Instruments Grids' drum map, kick, snare and hat (`X`, `Y`, `DENS`).
  * **Bounce**: balls bouncing in a box, a note at each wall hit (`SPEED`, `ANGLE`, `BALLS`).
  * **Awake**: two loops in the style of norns' Awake, steps settable from the keys (`LEN A`, `LEN B`, `PROB`).
  * **Poly**: four notes stepped on by four rhythms (`RHY`, `DIV`, `SPAN`).
* **EVOLVE** mutates the phrase every few bars (`LOCK`, `EVERY`, `AMOUNT`); **ROLL** picks the track's seed, a line you can always come back to.
* **FORM** builds sections A, B and C into forms such as `AABA`; **TWEAK** transposes, thins, humanises or follows another track.
* **TWEAK 2** branches hits to another track's sound (BRANCH), combines them with another track's (LOGIC), and plays them only on certain bars (COND).
* **Tap LFO to MODS** for 16 modulation cords between any source and destination on any track, generators included. Each track has slow organic sources, SLOTH and OCHD (LFO > ORGANIC); knobs show live marks where the cords move them, and **MATRIX** shows which tracks modulate which.
* **Hold FX and turn a knob** to set how far a cord modulates that knob's parameter.
* **Hold ARP and turn a knob** to see what that knob does.

---

## Saving & Storage

Generator settings, seeds, FORM sections and the cords are saved with the project, in their own flash object (`OBJ_TESSERA` at `0x9D000`) because the main project record is near capacity. The current block format is version 5 ("TSR5", with BRANCH, LOGIC, COND, SLOTH and OCHD); projects and backups saved by earlier Tess versions still load and convert.

* **Tagging & Validation**: Each slot's block carries a hash of its parent project record. If hashes mismatch, the project loads with Tessera `OFF` to prevent stale data. Every field is range-checked.
* **Compatibility**: Projects saved by stock Melodee load normally with Tessera `OFF`, and projects saved by this fork remain readable.

---

## To come

Planned for later Tess builds. Plans can change, and nothing here has a date yet.

* **More sounds**: a pad voice, a chord pad/drone mode that holds one chord a bar and changes it with the FORM sections, and our own pad, pluck and lead voices.
* **More generators**: a few smaller modes.
* **A new UI**: an optional Monomachine-style look (black and white, shapes instead of colours) as a separate firmware build.

---

## Visual Reference Gallery

*The pages as v1.3.3 draws them, rendered on the host from the firmware's own drawing code (`tests/gallery_render.c` in the firmware sources) and shown at 2x.*

### Sequencing

<table>
<tr>
<td align="center" valign="top" width="50%"><img src="assets/gallery/seq_mode.png" alt="SEQ MODE" width="240"><br><b>SEQ MODE</b><br><sub>Tap SEQ for GEN: KNOB 1 picks STEP, ARP or a generator, and SEQ then shows only that mode's pages.</sub></td>
<td align="center" valign="top" width="50%"><img src="assets/gallery/home.png" alt="HOME" width="240"><br><b>HOME</b><br><sub>The four tracks, their sounds and knobs, with the scope below.</sub></td>
</tr>
</table>

### Generators

<table>
<tr>
<td align="center" valign="top" width="50%"><img src="assets/gallery/gen_turing.png" alt="Turing" width="240"><br><b>Turing</b><br><sub>A looping shift register: LENGTH, PROB (Lock holds the loop), SPAN.</sub></td>
<td align="center" valign="top" width="50%"><img src="assets/gallery/gen_euclid.png" alt="Euclid" width="240"><br><b>Euclid</b><br><sub>Euclidean rhythms: STEPS, PULSE, ROT, on pitches from the track's seed.</sub></td>
</tr>
<tr>
<td align="center" valign="top" width="50%"><img src="assets/gallery/gen_markov.png" alt="Markov" width="240"><br><b>Markov</b><br><sub>A random walk over 8 scale degrees: LEAP, DENS, SPAN.</sub></td>
<td align="center" valign="top" width="50%"><img src="assets/gallery/gen_morph.png" alt="Morph" width="240"><br><b>Morph</b><br><sub>A rhythm blended between two preset patterns: POS, SNP A, SNP B.</sub></td>
</tr>
<tr>
<td align="center" valign="top" width="50%"><img src="assets/gallery/gen_grids.png" alt="Grids" width="240"><br><b>Grids</b><br><sub>Grids' drum map, kick, snare and hat: X, Y, DENS.</sub></td>
<td align="center" valign="top" width="50%"><img src="assets/gallery/gen_bounce.png" alt="Bounce" width="240"><br><b>Bounce</b><br><sub>Balls bouncing in a box, a note at each wall hit: SPEED, ANGLE, BALLS.</sub></td>
</tr>
<tr>
<td align="center" valign="top" width="50%"><img src="assets/gallery/gen_awake.png" alt="Awake" width="240"><br><b>Awake</b><br><sub>Two loops in the style of norns' Awake, steps settable from the keys: LEN A, LEN B, PROB.</sub></td>
<td align="center" valign="top" width="50%"><img src="assets/gallery/gen_poly.png" alt="Poly" width="240"><br><b>Poly</b><br><sub>Four notes stepped on by four rhythms over up to 3 octaves: RHY, DIVISION, SPAN.</sub></td>
</tr>
</table>

### Shaping the phrase

<table>
<tr>
<td align="center" valign="top" width="50%"><img src="assets/gallery/gen_evolve.png" alt="EVOLVE" width="240"><br><b>EVOLVE</b><br><sub>Mutates the phrase every few bars: LOCK, EVERY, AMOUNT; ROLL picks the seed.</sub></td>
<td align="center" valign="top" width="50%"><img src="assets/gallery/gen_tweak.png" alt="TWEAK" width="240"><br><b>TWEAK</b><br><sub>SHIFT, DROP, HUMAN and FOLLOW, each drawn on the page.</sub></td>
</tr>
<tr>
<td align="center" valign="top" width="50%"><img src="assets/gallery/gen_tweak2.png" alt="TWEAK 2 (new)" width="240"><br><b>TWEAK 2 (new)</b><br><sub>BRANCH hits to another track's sound, LOGIC with another track's hits, COND for certain bars.</sub></td>
<td align="center" valign="top" width="50%"><img src="assets/gallery/organic.png" alt="ORGANIC (new)" width="240"><br><b>ORGANIC (new)</b><br><sub>Each track's SLOTH (TORPOR, APATHY, INERTIA) and eight OCHD LFOs, slow sources for the cords.</sub></td>
</tr>
<tr>
<td align="center" valign="top" width="50%"><img src="assets/gallery/help.png" alt="Help card" width="240"><br><b>Help card</b><br><sub>Hold ARP and turn a knob to see what it does.</sub></td>
<td align="center" valign="top" width="50%"><img src="assets/gallery/cords.png" alt="MODS" width="240"><br><b>MODS</b><br><sub>Tap LFO for 16 modulation cords between any source and destination, generators included.</sub></td>
</tr>
<tr>
<td align="center" valign="top" width="50%"><img src="assets/gallery/mod_shortcut.png" alt="Mod shortcut" width="240"><br><b>Mod shortcut</b><br><sub>Hold FX and turn a knob to set how far a cord moves that parameter.</sub></td>
<td align="center" valign="top" width="50%"><img src="assets/gallery/mods_live.png" alt="Live mod mark (new)" width="240"><br><b>Live mod mark (new)</b><br><sub>A knob a cord reaches shows where the cords move it now: here an OCHD sweeping BRANCH.</sub></td>
</tr>
<tr>
<td align="center" valign="top" width="50%"><img src="assets/gallery/matrix.png" alt="MATRIX (new)" width="240"><br><b>MATRIX (new)</b><br><sub>The cords track by track, lit as they move. OCT+ opens one in MODS.</sub></td>
</tr>
</table>

### Sound engines

<table>
<tr>
<td align="center" valign="top" width="50%"><img src="assets/gallery/edit_sid.png" alt="SID" width="240"><br><b>SID</b><br><sub>The C64 chip, 6581 or 8580 model.</sub></td>
<td align="center" valign="top" width="50%"><img src="assets/gallery/edit_trio.png" alt="TRIO" width="240"><br><b>TRIO</b><br><sub>Three oscillators: WAVE, INT2, INT3, DETUNE.</sub></td>
</tr>
<tr>
<td align="center" valign="top" width="50%"><img src="assets/gallery/edit_phys.png" alt="PHYS" width="240"><br><b>PHYS</b><br><sub>Physical modelling (Rings / DaisySP): MODEL, STRC, BRIT, DAMP.</sub></td>
</tr>
</table>

---

## Licence

GPL-3.0-only, inherited from Melodee. Original copyright and SPDX headers are preserved.
