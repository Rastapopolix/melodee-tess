# Codebase map

Written 2026-10-14 against `tessera-dev` (`bb3067c` + docs). File sizes are line counts.

## How the firmware is built

- **One compilation unit.** `firmware/src/melodee.c` `#include`s every `.c` file in a fixed
  order; each file may use what the files above it define. `tests/hostsim.c` does the same for the
  PC with stubs in place of the hardware. A new source file must be added to both.
- Some `.c` files include further files: `engines.c` (the engines), `seq.c` (chord, motion,
  recording, click, erase, MIDI control and clock), `ui.c` (favourites, screen, browser, step edit,
  undo), `project.c` (pattern store, settings, slice store), `fx.c` (perform, delay), `editor.c`
  (the web editor's protocol pieces).
- Build switches are environment variables read by `tools/build.py` (table in `BUILDING.md`).
  `MELODEE_SLICE`, `MELODEE_FM4`, `MELODEE_PROPHET` and `MELODEE_DUAL_CORE` already remove or add
  engines.

## Platform (hardware) vs application

### Platform: keep in the hard fork

| Where | What |
| --- | --- |
| `firmware/hal/*.h`, `fm1_isr.S`, `fm1_vec.S` | Header-only hardware layer: clock, power, IRQ, cache, GPIO, ADC, I2S audio, LCD SPI, key/knob matrix (`fm1_input.h`), timers, UART, USB, flash/XIP, second core (`fm1_multicore.h`) |
| `firmware/crt0.S`, `firmware/app.ld` | Startup and memory layout (the app area ends at 0x93000) |
| `firmware/loader/` | The OTA/update loader. **Risky area.** |
| `main.c` (369) | Boot order, watchdog, boot-loop guard, CPU1 start/retire, the main loop |
| `audio.c` | I2S output and the audio interrupt: per half buffer, blocks of control-rate samples through `fx.c mix_block` |
| `lcd.c`, `gfx.c`, `screen.c` | ST7789 240×240 panel, the small-canvas renderer, partial redraws |
| `panel.c` | Which matrix button / encoder is which printed label; panel calibration |
| `usb.c`, `usb_audio*.c`, `midi_uart.c`, `midi_clock.c`, `midi_control.c` | USB device (MIDI, audio, CDC), TRS MIDI in, MIDI clock and CCs |
| `storage.c`, `storage_hw.c` | A/B flash objects with CRC and commit records (see the flash map in `storage.c`). **Risky area.** |
| `ota.c`, `ota_hw.c` | Update entry. **Risky area.** |
| `resources.c` | The audio working arena: engine state, recordings, effect lines allocated on demand (at least 152 KiB) |
| `libc.c` | memcpy & co., and the one global `rng()` (xorshift32) |
| `console.c`, `editor*.c` | USB serial console; the web editor's SysEx protocol |

The strict platform layer is `firmware/hal/` (headers only). The `.c` files above use it and are
"platform-like" application code: they compile on the host because `hostsim.c` stubs `hal/`.

### Application

| Area | Files |
| --- | --- |
| Engines | `engines.c` (the table), `dsp.c`, `voice.c` (allocation, envelopes, LFO), `eng_*.c`, `x0x/` (909 drums), `drum_808.c`, `phys_*.c`, `fm6_*.c`, `cz_*.c`, `prophet_*.c` |
| Modulation | `mod.c`: per track four slots, SRC → DST by AMT, added to the fixed parameters at control rate; read by `voice.c` and `fx.c` |
| Effects | `fx.c` (DIST insert, chorus/delay/reverb sends), `delay.c`, `perform.c` (FX hold layer), `slicer.c` |
| Sequencer | `seq.c` (1208): keyboard, scales, arpeggiator, step sequencer, transport; `song_chain.c`, `pattern.c`, `recording.c`, `motion.c`, `chord.c`, `seq_edit/notes/undo/erase.c` |
| Generators | `tessera.c` (923), `tessera.h` (131) |
| Parameters | `params.c`: parameter descriptors (`P_*` track, `G_*` global), formatting, the page table `PAGES[]` |
| UI | `ui.c` (state, pages), `ui_input.c` (2144: knobs, buttons, keys, LEDs), `ui_layer.c` (hold layers), `ui_draw.c` (header, knob cards, footer), `ui_graph.c` (the picture between cards and footer), `ui_pages.c`, `ui_stage.c`, `ui_menu.c`, `ui_popup.c`, `ui_list.c`, `ui_name.c`, `ui_new.c`, `ui_patterns.c`, `ui_scales.c`, `ui_sections.c`, `ui_slots.c`, `ui_motion.c`, `icons.c` |
| Storage formats | `project.c` (1675: four project slots, each 32 pattern banks), `pattern_store.c`, `settings_persist.c`, `upreset.c`, `favorites.c`, `*_store.c`, `*_bank.c` |

### Execution contexts

- **Audio interrupt** (`audio.c` → `fx.c mix_block` → `seq.c events_block` → `seq_advance` →
  `seq_tick` per track): the sequencer, and therefore **the generators**, run here, once per
  step, inside the audio ISR.
- **Main loop** (`main.c`): UI input and drawing, storage, USB/editor. Knob edits write parameters
  that the ISR reads.
- **CPU1** (optional): FM6 and Prophet kernels and, upstream, shared FX DSP.

## Where Tessera hooks in

Tessera adds about 1,500 lines; 923 in `tessera.c` and about 600 spread over shared files.

### Sequencer (`seq.c`, inside the audio ISR)

- `seq_grid_tick`: on every step of a track whose Tessera mode is not OFF, `tessera_clock_step(t)`
  replaces `seq_step(...)`. The step timing still comes from the track's own grid: `P_SDIV` (the
  division), `P_SLEN` (the loop length the step index wraps at), swing, pattern switches. Motion,
  erase and recording hooks still run on that step.
- `seq_start`: `tessera_ensure_init()`. `seq_stop`: `tessera_stop_all()` (releases notes, resets
  counters, takes forms back to A).
- Tessera calls into the sequencer and voices: `trk_note_on/off`, `seq_release`, `seq_div_samples`,
  `scale_mask`, `trk_index`, `rng`, and writes `t->seq_notes`, `t->seq_n`, `t->seq_off` so the
  sequencer's gate timer ends the note.

### UI

- `params.c`: three page kinds `GR_EVOLVE`, `GR_FORM`, `GR_TWEAK`; four pages in the SEQ family:
  EVOLVE (LOCK EVERY AMOUNT ROLL), FORM (FORM CONTRAST CLEAR FLOURISH), TWEAK (SHIFT DROP HUMAN
  FOLLOW), and FEEL (division, swing, gate: the timing knobs, because Tessera takes over PATTERN).
- `ui_input.c edit_param`: knobs on those pages go to `tessera_*_knob`. On PATTERN (`GR_STEPS`),
  KNOB 1 picks the generator (OFF / TURING / EUCLID / MARKOV / MORPH) and, with a generator on,
  KNOBS 2–4 are its parameters (`tessera_knob`). The list picker is suppressed there.
- `ui_input.c tracks_edit` (MIXER mute) and `ui_layer.c` (GLO mute key): muting a track kills its
  Tessera note.
- `ui_draw.c`: the knob cards for the Tessera pages. `ui_graph.c`: `graph_tessera_viz` (register
  bits, Euclid ring, Markov states, Morph A/B) and the phrase / form bars. `ui_pages.c`: the panel
  picture and its playhead; redraw signatures include `tessera_sig`.
- `ui_menu.c`: the About text and credits.

### Storage

- `storage.c`: one extra object `OBJ_TESSERA` at 0x9D000 / 0x9E000 (A/B), a pair upstream documents
  as free (it overlaps only the retired legacy project objects, which nothing reads or writes).
- `project.c`: the object holds four slots of tag + 180 bytes (`tessera_pack`). A slot is valid only
  while its tag equals the project record's hash (`proj_saved_hash`). Saved after a project save,
  retagged after a rename, loaded after a project load; reset on template load and new project.
- Not saved: the recorded Markov / Morph phrases, playheads, counters.

### Modulation matrix

- **No connection.** Tessera neither reads `mod.c` sources nor exposes its values as sources or
  destinations. Its notes enter the voices through `trk_note_on`, after which the track's own mod
  slots apply as for any note. Phase 2 (the signal bus) starts from zero here.

### Tests

- `tests/hostsim.c` includes `tessera.h` and `tessera.c`, so every host test builds them, but no
  test exercised a generator before tonight (see `tests/tessera_test.c`).

### Things noticed while mapping

- `tessera.c` and `tessera.h` have no GPL-3.0 SPDX header (CLAUDE.md asks for one).
- `tessera.h` defines a global object (`tessera_state_t tessera`) and non-static functions in a
  unity build where everything else is `static`; harmless, but unlike the rest of the code.
- Knob handlers (main loop) change generator state that the audio ISR is using, with no IRQ guard.
  Most writes are single bytes, but `tessera_knob` mode changes, `tessera_mutate` (ROLL) and
  `tess_form_restart` change several fields at once: a step can see half an update. Usually
  harmless (one odd note), but worth fixing in the extraction.
- `seq.c` and `ui_layer.c` declare `extern` functions inside function bodies.

## Removing Prophet, FM6, CZ, SLICE and samples

### What is there

Sizes from the symbol table of `build/melodee.elf` (named symbols only, so lower bounds; inlined
code and jump tables are not attributed):

| Engine | Files | Flash (text + rodata + data) | Static RAM |
| --- | --- | --- | --- |
| Prophet | `eng_prophet_test.c` (the engine), `prophet_patch.c`, `prophet_user.c`, `prophet_edit.h`, `editor_prophet.c` | about 53 KB (factory bank `P5_FACTORY` 27 KB, `P5_TEST_PRESETS` 4.7 KB) | about 7 KB |
| FM6 (and DIGITAL's conversion) | `eng_fm6.c`, `eng_fm6_rom.h`, `fm6_core.c`, `fm6_bank.c`, `fm6_store.c`, `up_fm6.c`, `editor_fm6.c`, `fm4_convert.c` | about 42 KB | about 11 KB |
| CZ-1 | `eng_cz.c`, `cz_native.c`, `cz_bank.c`, `cz_store.c`, `cz_patch.h`, `cz_edit.h`, `cz_legacy.h`, `editor_cz.c` (and `eng_phase.c` shares its core) | about 22 KB (`CZ_FACTORY` 9 KB) | about 4 KB |
| SLICE | `eng_slice.c` (behind `MELODEE_SLICE`), `slicer.c`, `slice_store.c` | small in code; its recordings use 32 KB of the audio arena | arena |
| Samples | The SAMPLE engine is already retired (ID 4 reserved); `assets/samples-cc0/` only feeds old builds. `perform.c` borrows the slicer's 32 KB buffer | — | — |

Flash matters: `build/melodee.bin` is 579,848 B and the app area (`app.ld`, to 0x93000) holds
581,564 B, so **only about 1.7 KB of app flash is left today**. Removing these engines frees
roughly 120 KB or more, which the new voices, FX, help text and generators need. (The build
reports the free audio arena but not the free app flash; a flash report is worth adding.)

### What removal involves

1. **Engine table** (`engines.c`): keep the stored engine IDs reserved (`ENG_RETIRED`), as upstream
   does for every engine it retires, so old projects still load (a removed engine's track loads as a
   safe default).
2. **Build switches:** `MELODEE_SLICE` exists; `MELODEE_PROPHET` exists in `engines.c`. Adding
   `MELODEE_FM6` and `MELODEE_CZ` switches first lets the removal be done and tested one engine at a
   time before deleting files.
3. **UI:** the sound sections and maps (`ui_sections.c`), EDIT layer operator keys (`ui_layer.c`),
   the browser's engine list (`browse.c`), CZ TOOLS and FM STORE pages (`params.c`, `ui_input.c`
   `act_do`), the sound designer (upstream), the momentary-tweak native patch copies (`ui.c`).
4. **Storage:** the FM6, CZ and Prophet banks and user stores (`storage.c` objects, `*_store.c`,
   `*_bank.c`, `upreset.c` native slots, `project.c` native patch fields). Their flash sectors become
   free, but **the object numbers and the flash map must not move** for the objects that stay
   (**risky area: flash layout**).
5. **Editor and web:** `editor_fm6.c`, `editor_cz.c`, `editor_prophet.c`, `editor_native.c` and the
   web editor's pages for them; the installer's package checks.
6. **Dual core:** CPU1 exists mainly for "paired FM6/Prophet kernels". Without them CPU1 has little
   to do; it could later run shared FX (as upstream does) or simply stay off, which also avoids the
   units that misread SRAM.
7. **Tests:** 19 files are specific to these engines (`fm6_*`, `cz1_*`, `prophet_*`,
   `slice*`, `phase_test`, `digital_test`, `fm4_test`, parts of `regress.c` and `golden.txt`).
   They go with the engines; `golden.txt` loses their rows.
8. **Upstream sync:** after this step merges from upstream stop being whole-release merges; cherry-pick
   platform fixes (ROADMAP: "cherry-pick after the hard fork").
