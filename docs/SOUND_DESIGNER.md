# Sound designer

Open **Sound designer** from the HOME hold menu, or from a sound page's sheet (OCT+ held).
It is built from Melodee's own screens (docs/design): START is laid out like NEW SONG's key
screen, FEATURES is a list page, and the shaping stages are the sound pages themselves.

## Start

Three rings over a large panel (the engine, and the choice in words), the keys under it:

| Control | Choice |
| --- | --- |
| KNOB 1 | ENGINE, from the normal selectable engine list |
| KNOB 2 | START: CURRENT, INIT, or INIT with a recipe: BASS, PAD, LEAD, KEYS, PLUCK, FX |
| KNOB 3 | PAGES: MAIN (the engine's own pages) or ALL |
| OCT+ | Design |
| OCT- or HOME | Close without changing the sound |

CURRENT keeps the selected sound exactly. INIT starts from the engine's native init patch; a
recipe also applies a small starting gesture for that kind of sound. DRUM starts with its kit. On another engine CURRENT is not offered: it starts from
INIT. The recipes are suggestions for exploration, not claims of reproducing an instrument.

## Stages

**Features → Source → Tone → Env → Motion → Play → FX.** Stages without pages are skipped.

| Control | Action |
| --- | --- |
| OCT+ / OCT- | The next / previous stage; past the last or the first: the designer's sheet |
| PRESETS | The next / previous stage |
| SELECT | The pages, across the stages |
| KNOB 1–4 | The page's own controls (a list page: KNOB 2 the row, KNOB 1 its value) |
| SAVE | The original sound (header chip ORIGINAL); SAVE again: back to the design |
| HOME | The designer's sheet |

On a sound page the stages are the tabs under the header and the stage's pages are the header's
dots; the chip names the page (with its operator or CZ line). Under every stage, as under NEW
SONG, the keys: OCT- back on the left, the sound's name, OCT+ on the right with the next stage's
name (the last stage: Finish). Pages, rings, pictures and pickers are the normal ones.

Each stage shows the engine's own pages first. FEATURES decides what is shaped: an FM operator
switched off, or a CZ line that its line mode does not play, has no pages; an FM operator's pages
come together, operator by operator. MAIN leaves out what repeats or refines: the track LFO and
the modulation matrix on engines with LFOs of their own (Prophet, FM6, CZ-1) and on drum kits,
LFO 2 and the matrix on SID, FM6's macro pages, keyboard scaling, CZ pitch envelopes and the
second halves of CZ envelopes, bend and controller pages. ALL shows them.

Nothing plays by itself: audition the sound on the keys, which always play it while designing
(never the drum grid or a layer's shortcuts). PLAY starts or stops the pattern only when pressed. The track and the preset browsing stay
on the designed track. Recorded motion on this track is paused so it cannot overwrite edits; its
events and enabled state remain unchanged and resume when the designer closes. Knob edits in the
designer do not record motion. Other tracks' motion continues normally.

## Features

The first stage builds the signal path before any shaping: a list page, a row for each feature
of the instrument, its state at the right (Off is dim). KNOB 2 moves through the rows; KNOB 1
switches the row on (right) or off (left), or steps Voice and Lines.

| Engine | Features |
| --- | --- |
| Prophet | Osc B, Pulse A, Pulse B, Noise, Sync, Poly-Mod, LFO, Filter env, Unison, Release, Velocity, Glide |
| FM6 | OP1–OP6 (each named carrier or modulator in its algorithm), Feedback, Pitch EG, LFO, Voice |
| CZ-1 | Lines, Ring, Noise, Detune, Vibrato, Voice |
| SID | Voice, Glide, Spread, Pitch env, Vibrato, Wobble, Tremolo, Filter env |

Switching a feature on wires a working path with the instrument's own controls and keeps any
amount that was already set:

- Prophet Pulse A/B selects the pulse wave and gives the oscillator a mixer level; Osc B adds a
  B waveform and level; Sync makes OSC A audible. Poly-Mod connects a source amount (filter
  envelope) to the filter destination; LFO supplies a waveform, initial depth (motion without the
  mod wheel), rate and, when no wheel destination is set, the filter. Unison switches on native
  unison (the native five-voice default when no count is set). Velocity switches both velocity
  sensitivities.
- An FM operator is switched on with an output level (90 for a carrier, 60 for a modulator).
  Pitch EG gives a short upward blip as notes start; LFO gives a gentle vibrato.
- CZ-1 Ring, Noise and Detune act on line 2, so they select the 1+2' line mode when only one line
  is in use. INIT's two lines are identical, so 1+2' sounds the same until line 2 is shaped.
- SID Wobble and Filter env move the chip filter's cutoff. A cutoff above three quarters leaves a
  sweep no room, so switching either on brings it to the middle. Glide is clearest in MONO.

Switching off returns the switch or amount to zero; for Poly-Mod and the Prophet LFO it opens the
destinations and keeps the source amounts for the next ON. A build that cannot sound is reported
in the header as it is made: MIXER SILENT (every Prophet source off) or NO CARRIER ON (FM).
Drum kits have no feature stage.

## Dependencies

A control whose path is off is dimmed, as on every sound page. In the designer, turning a dim
control first switches on the feature it needs, as FEATURES would, and the header says which
(PULSE A ON, POLY-MOD ON, UNISON ON, OP3 ON, LINE 2 ON ..); the turn then edits the control. A
control that already sounds changes nothing else.

- Prophet WIDTH A switches Pulse A on; Poly-Mod PW A switches Pulse A and Poly-Mod on; OSC B's
  tuning switches Osc B on; filter envelope times switch Filter env on, its REL also Release.
- Prophet VOICES and DETUNE switch native Unison on; LFO RATE, waves and INITIAL switch LFO on.
- An FM operator that is off is switched on when its controls are turned (its level turned down
  leaves it silent).
- CZ vibrato's WAVE, RATE and DELAY give it depth; line 2's values, DETUNE and MOD select 1+2'.
  An envelope step after its END stays as it is (header: AFTER THE END POINT): END and SUS are
  set on the POINT page.
- Track LFO controls with no destination give it a gentle pitch destination; DETUNE on VOICE
  selects unison.

Pulse, Poly-Mod and unison are already implemented in the Prophet renderer; the designer makes
them reachable. This does not change the synthesis model or claim calibrated hardware fidelity.

## Finish

HOME, or OCT past either end of the stages, opens the designer's sheet (KNOB 2 the row, OCT+
does it, OCT- goes back to designing):

- **Keep the sound**: the designer closes, the designed sound stays on the track.
- **Save as user…**: the designer closes with the sound kept and opens USER on its slot, as SAVE
  does; OCT+ then the slot's sheet on Save here, the existing overwrite question and NAME screen.
  Prophet, FM6 and CZ-1 save their native synth patch to the engine's user bank; other engines
  use the general preset format. To keep the complete setup (voices, effects, patterns), save the
  project from PROJECT as ever.
- **Discard**: asks first, then restores the original sound, including its native patch and
  user-bank origin, while leaving patterns, recordings, the track mix, scale and arpeggiator
  settings intact.

No storage formats or engine IDs change. Loading a project while designing ends the designer
without restoring anything over the loaded project.

## Validation

`tests/designer_test.c` drives the actual panel input and native setters: current-sound
preservation, START's steps, dependencies switched on by turning dim controls (and nothing else
changed by controls that sound), the pages FEATURES leaves out, compare/discard across engines, the sheet's Keep, Save as
user (to the native bank, named) and Discard, OCT and PRESETS/SELECT navigation, the menu over an
open design, project reload, motion isolation and expert-page coverage. It renders every
engine/goal recipe through the DSP. Every feature on Prophet, FM6, CZ-1 and SID is switched off
and on through its row, and each one changes the rendered audio (against a repeatable reference
render). Two are checked differently: CZ-1 Lines, whose INIT lines are identical, by its line-2
controls becoming live; SID Spread, which places voices in the stereo mix after the track, by its
mixer condition. Glide is rendered in MONO.
`tests/ui_render.c` draws START, every FEATURES row, every stage page, the original, the sheet and
the question in every palette with the real drawing code and the layout/monochrome checks.
Physical encoders, key timing and listening still require validation on the FM-1.
