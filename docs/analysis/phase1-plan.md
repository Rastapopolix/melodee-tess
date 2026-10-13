# Phase 1 plan: the generators behind `gen_tick`

Written 2026-10-14. Goal (ROADMAP phase 1): move Turing, Euclid, Markov and Morph behind
`gen_tick(state, input_phrase, clock)`, free of UI and Melodee knowledge, with host tests, and **play
exactly what they play today**. The check is `tests/tessera_test.c` against `tests/tessera_baseline.txt`
(22 scenarios plus the saved block), added tonight.

## The interface

A proposal, small enough for the audio interrupt and with no function pointers:

```c
/* gen.h */
typedef struct { uint8_t hit, note, vel; } gen_step_t;            /* one step of an input phrase */
typedef struct { const gen_step_t *step; uint8_t len; } gen_phrase_t;   /* len 0: empty (phase 1: always) */

typedef struct {
    uint8_t pos;            /* the step's position in the phrase loop (tess_loop_len) */
    uint8_t steps_per_bar;  /* from the track's division (tess_steps_per_bar) */
    uint8_t root;           /* the track's key, 0..11 (Markov uses it today) */
} gen_clock_t;

typedef struct {
    uint8_t gate;           /* 1 = a note starts on this step */
    uint8_t note, vel;      /* the raw note (before scale snap, transpose, SHIFT), as today */
    int16_t cv;             /* the generator's CV for the signal bus (phase 2; 0 until then) */
} gen_out_t;

typedef struct {
    uint8_t kind;           /* GEN_TURING .. GEN_MORPH */
    union { gen_turing_t turing; gen_euclid_t euclid; gen_markov_t markov; gen_morph_t morph; } u;
} gen_t;

void gen_tick(gen_t *g, const gen_phrase_t *in, const gen_clock_t *clk, gen_out_t *out);
void gen_mutate(gen_t *g, uint32_t amount);                       /* EVOLVE's mutation, ROLL */
```

- **What stays in `tessera.c` (the adapter):** reading the track (scale, root, transpose, division,
  mute), EVOLVE / FORM / TWEAK, LOCK's phrase recording, the scale snap, `trk_note_on/off`, the gate
  length, the saved block. `tessera.c` becomes the bridge between the sequencer and the generators, and
  the generators know nothing about tracks.
- **Randomness:** the generators call `gen_rnd(n)`, which in phase 1 is `rng() % n` (the shared
  generator), so the draws stay the same. A per-track seeded generator comes after the phase (see the end).
- `gen_out_t.cv` and `gen_phrase_t` exist so phases 2 and 6 do not change the interface again; phase 1
  passes an empty phrase and ignores `cv`.

## Order of work

Each step is one small commit on a feature branch. After each: `./build.sh` (both core variants for
the steps that touch shared files), `tests/run_tests.sh`, and the baseline must be unchanged. A changed
baseline means the step altered the music: find out why before going on (diff the logs in
`build/tessera_log/` against a run of the previous commit).

0. **Before starting**
   - Done 2026-10-14: v1.3.1 merged (`1b4c9e9`), baseline unchanged, static RAM 5,200 B free. The RAM
     and flash risks below were written before the merge. Was: decide whether to merge upstream v1.3.1
     first (see QUESTIONS.md). Recommended: yes. It touches
     the same `seq.c` hooks, and frees RAM (see the risks). If merged, re-run the baseline: it must be
     unchanged, because upstream did not touch the generators (only the code around them).
   - Add the GPL-3.0 SPDX header to `tessera.c` and `tessera.h` (done 2026-10-14).

1. **Widen the baseline first** (tests only; done 2026-10-14 except loading mid-play, which the firmware does not
   do: the sequencer stops for a load): scenarios for swing, external MIDI clock (`G_CLOCK`),
   pattern switches and SONG chains with a generator track, a track's length (`P_SLEN`) shorter than the
   generator's loop, the GLO mute key, and loading a saved block mid-play. These are paths the
   extraction could disturb that tonight's 22 scenarios do not cover.

2. **`gen.h` with the types**, included from `melodee.c` and `tests/hostsim.c` before `seq.c`. No users
   yet. Check the build size report.

3. **Pure helpers into `gen_*.c`:** `tessera_euclid_hit`, `tess_euclid_degree`, `tess_markov_row`, the
   scale tables, `MORPH_PRESETS`. Same code, new homes; `tessera.c` calls them.

4. **One generator per commit:** Turing, then Euclid, Markov, Morph. Each moves its `case` from
   `tessera_clock_step` into `gen_<name>_tick` and its `case` from `tessera_mutate` into
   `gen_<name>_mutate`. The adapter builds `gen_clock_t`, calls `gen_tick`, then does exactly what the code
   after the `switch` does today.
   - **Keep the order of every `rng()` call.** Today's order per step: EVOLVE's mutation (on a phrase
     boundary) → the generator's draws → LOCK's draw (Markov, Morph) → DROP → HUMAN's velocity → HUMAN's
     gate. Moving one draw earlier or later changes every note after it.
   - Markov reads `t->p[P_ROOT]` inside its case: pass it as `clk->root`.
   - Morph keeps its B preset in `oct_span`: rename the field in the new struct (`preset_b`) but keep the
     saved-block byte where it is.

5. **Sections (FORM):** move `tess_save_sec` / `tess_load_sec`'s per-generator parts into
   `gen_<name>_save` / `_load` working on a generic `gen_sec_t`. This is the "home" Gravity will need in
   phase 6.

6. **UI reads through accessors.** `ui_draw.c`, `ui_graph.c` and `ui_pages.c` read generator fields
   directly (`tessera.turing[tr].reg`, `euclid.pulses`, ...). Give them `gen_view(tr, &view)` (or keep
   the struct names stable) so later generator changes do not touch the UI. The UI renders are part of
   the test suite (`tests/ui_render.c`): the PNGs in `build/ui_new/` must not change.

7. **Saved block unchanged:** `tessera_pack` / `tessera_unpack` keep the same bytes (the
   `saved_block` baseline line). If the state layout changes, pack from the new fields in the old order.

8. **Interrupt safety:** knob edits that change several fields (engine change, ROLL, FORM restart,
   CLEAR) either run inside a short `fm1_irq_off()` / `fm1_irq_on()` window or post a request the audio
   side applies at the next step. No musical change; the host tests cannot see it, so review it by hand.

9. **Per-generator unit tests** (`tests/gen_test.c`, calling `gen_tick` directly with a stub random
   source): the same output for the same seed, notes and velocities inside the documented ranges, one
   event at most per tick, an empty input phrase accepted, the cost per tick (host time, and the
   `target_budget` check if `gen_tick` lands in its list).

10. **Close the phase:** tick the ROADMAP tasks, update `docs/analysis/codebase-map.md` (where the
    generators now live), and record the RAM and flash figures before and after.

**After phase 1, as its own intended change:** a seeded random generator per track (`gen_rng` in each
generator's state, seeded from the project or a displayed seed). This deliberately changes what the
generators play: rewrite the baseline with `TESSERA_UPDATE=1`, review the logs, commit the new baseline
separately. Doing it inside the extraction would make the "identical output" check useless.

## Risks

| Risk | Why | Mitigation |
| --- | --- | --- |
| The music changes without anyone noticing | Any change in the order of `rng()` calls, in rounding, or in when a step's state is read | The baseline test; one generator per commit; never rewrite the baseline during the phase |
| **No static RAM left** | The build reports `.data + .bss` 98,268 B of 98,304 (36 B free). Generator state is static (`tessera` 1,592 B in `.bss`); a few new bytes break the build | Keep the state the same size, or merge upstream's RAM savings first, or move generator state to the audio arena / pool |
| **Almost no app flash left** | `melodee.bin` 579,832 B in an app area of 581,564 B (about 1.7 KB free) | Keep the dispatch a `switch`, avoid duplicated helpers, measure each commit; removing engines (phase 4) is where real room comes from |
| Upstream merge conflicts | Upstream reworked `seq_start` / `seq_stop` and recording, exactly where Tessera hooks in | Merge v1.3.1 before starting (step 0) |
| UI breaks | The UI reads generator internals in three files | Step 6; the UI render tests |
| Saved projects stop loading | The saved block's layout is tied to the state's fields | Step 7; the `saved_block` baseline and the round-trip check |
| Half-applied edits in the audio interrupt | Main-loop knob handlers write several fields the ISR reads | Step 8 |
| Baseline blind spots | MIDI clock, swing, chains, recording are not in the 22 scenarios | Step 1, before any code moves |
| Hidden coupling | Markov reads the track's root; steps per bar come from the sequencer's division table; the adapter writes `seq_notes` / `seq_n` / `seq_off` for the sequencer to end the note | Pass these through `gen_clock_t` and keep the note handling in the adapter |

## Size of the work

About two or three sessions: one for steps 0–3, one for the four generators (step 4), one for steps 5–10.
The generators are small (Turing, Euclid, Markov and Morph are about 25 lines each in `tessera_clock_step`);
most of the time goes into keeping the adapter's behaviour exact and into the tests.
