# Melodee (Tess)

A fork of the M-Vave FM-1 alt-firmware [Melodee](https://github.com/keremimo/melodee) (Leo Kuroshita / Hügelton Instruments, GPL-3.0-only). **Status: Experimental.**

---

## What It Is

**Melodee (Tess)** adds algorithmic note generators to the four tracks, plus controls for evolving them over time, building song forms from them, and saving them with the project. Each track can stay a normal step sequencer or be switched to a Tessera engine. 

The sound engines, effects, and core infrastructure are unchanged. The build outputs `melodee-tess.fwsc`.

---

## Engines

On the **PATTERN** page, the **MODE** knob switches a track between `STEP` (the original sequencer) and one of four generative engines:

* **Turing**: A looping shift register in the style of a Turing Machine. 
  * `LEN`: Loop length (2 to 16).
  * `PROB`: How often bits flip (0 locks the loop).
  * `SPAN`: Pitch range in octaves.
* **Euclid**: Euclidean rhythms. 
  * `STEPS`: Total steps (1 to 32).
  * `PULSE`: Active hits.
  * `ROT`: Rhythm rotation offset.
  * *Note: Pitches come from a seed saved with the track.*
* **Markov**: A random walk over 8 scale degrees. 
  * `LEAP`: Sets jump probability (0 favors neighboring degrees, 100 favors leaps).
  * `DENS`: Active step density.
  * `SPAN`: Pitch range.
  * *Note: The first pass records the phrase, which then replays.*
* **Morph**: A rhythm blended between two of eight preset patterns (four-on-the-floor, offbeats, straight eighths, son/rumba clave, 3-3-2, alternating, dense). 
  * `POS`: Blends from pattern A to B.
  * `SNP A` / `SNP B`: Select the two base patterns.

All engines play within the track's scale and root. Existing track controls (transpose, gate, swing, etc.) still apply. **FEEL** is a new page that keeps `DIV`, `SWING`, and `GATE` reachable while Tessera owns the PATTERN page. Muting a track cleanly releases any sounding Tessera note.

---

## Evolve, Form, Tweak & Flourish Pages

* **EVOLVE**: 
  * `LOCK`: Deja vu control determining how much of the phrase repeats (inverse of PROB for Turing).
  * `EVERY`: Mutates the phrase every (`OFF`, `1`, `2`, `4`, `8`, or `16` bars).
  * `AMOUNT`: Degree of change per mutation.
  * `ROLL`: Instantly triggers a mutation.
* **FORM**: Builds sections from mutated phrases (`AB`, `ABAC`, `ABCB`, `AABA`, `ABCA`, or `AABC`). 
  * `CONTRAST`: Varies new sections relative to previous ones.
  * `CLEAR`: Forgets B and C, resetting the current phrase to A.
  * *Note: Section changes happen on the `EVERY` boundary; selecting a form automatically enables `EVERY` if it was off.*
* **TWEAK**: 
  * `SHIFT`: Transposes up to 24 semitones.
  * `DROP`: Probability of dropping a hit across any engine.
  * `HUMAN`: Adds velocity and gate variation.
  * `FOLLOW`: Plays another Tessera track's pitch, shifted, in this track's scale.
* **FLOURISH**: Located as the fourth knob on the **FORM** page (-12 to +12 semitones). It shifts the last beat of the last bar of each phrase so sections lift into transitions. Applied before scale-snapping to stay in key without modifying recorded base phrases (requires `EVERY` enabled).

---

## Visualisations

Visualisations are integrated for each engine:
* The **pattern panel** displays the Turing register, Euclid ring, Markov bars, and Morph blend.
* The **FORM page** shows the section order and current playing position.

---

## Saving & Storage

Tessera state is saved with the project into its own dedicated flash object (`OBJ_TESSERA` at `0x9D000`, utilizing the free sector pair) because the main project record is near capacity. 

* **Payload**: Holds each track's engine, engine settings, EVOLVE, FORM, TWEAK, FLOURISH, and section settings (~180 bytes per slot).
* **Tagging**: Each slot's block carries a hash of its parent project record. If hashes mismatch (e.g., interrupted saves, archive imports, or overwritten slots), the project loads with Tessera `OFF` to prevent stale data. Renaming a project preserves block validity.
* **Validation**: Every field is range-checked; blocks containing any out-of-range value are rejected entirely.
* **Exclusions**: Recorded Markov/Morph phrases and playheads are not saved (re-recorded on the first pass). Turing and Euclid states restore identically.
* **Resets**: Loading a slot, loading a template, or starting a new project resets Tessera to `OFF` before applying saved data.
* **UI Feedback**: `SAVED (TSR ERR)` indicates a Tessera block write failure, though the core project saves successfully.
* **Autosave**: Autosave inspects the core project record only; tweaks to Tessera settings alone wait for an explicit save.
* **Compatibility**: Projects saved by stock Melodee load normally with Tessera `OFF`, and projects saved by this fork remain readable.

---

## Memory & Build Constraints

Operating within strict hardware limitations (~36 bytes of `.bss` slack and ~600 bytes of `.pool` slack):
* The static Markov transition matrix (256 bytes) was removed; rows are computed dynamically from `LEAP` at runtime with identical results. An unused 32-byte array was also pruned.
* The Tessera save buffer is minimized (~740 bytes) to respect audio arena minimum requirements.
* The build system outputs `melodee-tess.fwsc` (and `melodee-tess-X.Y.fwsc` for releases).

---

## Known Limitations

* Tessera engines are not recorded directly into the step grid, and legacy step features (probability, ratchets) do not apply to a track while running a Tessera mode.
* Per-step and per-pattern Tessera automation states are unsupported.
* The save buffer size prevents caching raw Markov/Morph phrases to flash.

---

## Licence

GPL-3.0-only, inherited from Melodee. Original copyright and SPDX headers are preserved. Source code for any distributed firmware variants must be made available under the same licence.
