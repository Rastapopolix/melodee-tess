# Upstream review

Written 2026-10-14 on branch `overnight-analysis`. Nothing was merged.

**Update, same day:** v1.3.1 is merged (`1b4c9e9`), the histories connected by `4621fa4`. The
conflicts were as listed below, plus two found by the tests: upstream's site test expected the
`melodee-` package name, and the new fonts cut off FORM's CONTRAST / FLOURISH labels.

- Remote: `upstream` = `https://github.com/keremimo/melodee.git`, fetched with tags.
- **Our base:** upstream `0bc95e8` "chore(release): prepare 1.1.0" (2026-10-10). The fork's first
  commit (`f93496f`) is exactly that tree plus Tessera; the histories are not connected, so git
  cannot find the common base by itself. Pass `--merge-base 0bc95e8`, or graft the history first.
- **Latest release:** `v1.3.1` (`757b65b`, 2026-10-13): 60 non-merge commits after our base,
  254 files changed, about +12.9k / −12.8k lines.
- **Upstream `next`** (`7025143`) has 5 more commits after v1.3.1: a CPU1 retirement fix and
  recovery-release tooling (see below). `upstream/main` (`16daf99`) is older than v1.3.1.

List the commits with:

```
git log --no-merges --format='%h %ad %s' --date=short 0bc95e8..v1.3.1
```

## 1. Platform and bug fixes worth taking

Clock and CPU:
- `7c0dd33` MELODEE_SYS_CLK=320: boot-time PLL retune for units whose CPU1 misreads shared SRAM.
- `b97579f` OVERCLOCK menu (320 / 360 MHz); falls back to 320 after a CPU fault.
- `c6a3409`, `c6ff76a` the PLL settles before SYS is restored; the SYS PLL is PLL2.
- `7025143` (next only) bars the CPU1 worker at retirement.

RAM and flash (we have only 608 B of audio arena headroom today):
- `1e47387` audio loops in fast RAM, larger voice budget (part of the effects removal, below).
- `df76ee3` one bank cache for user, FM6, CZ-1 and Prophet banks.
- `094d7c0` reclaims duplicate staging and snapshot buffers.
- `3ad8542` two-phase UI fonts with class kerning; `423a4c6` more main-loop files optimised for size.
- `1bc4555`, `3023205` packed Prophet and CZ factory banks; FM6 RAM tables built at boot.

Fixes:
- `2598876` transport (local PLAY runs until MIDI transport takes over), SysEx imports, note
  edits; also mutes the codec through boot (no startup pop).
- `0535a37` 32-bit LCD diff hashes; `06ed897` header BPM redraw; `007e18f` list picker columns;
  `a1c941e` step-number lists inline.
- `be9def5`, `8c290b4`, `478bc2d` LED dimming (the PWM experiments ended back on GPIO frame
  dimming).
- `b123378` backup restore keeps recording preferences.
- `7cd66fb` Prophet DC block at the part's output.

Tooling:
- `ac3c214` host tests in parallel, `SOUND=0|only` switch.
- `017c941`, `e4e45b1`, `a6db751`, `01a78bd` GitHub Actions nightly builds and Pages
  publication (useful for the web simulator track later).
- `757b65b` release archives validated before tagging; `c829f7f` recovery tools; `eeb8d98`,
  `19e4850`, `41f6e03`, `43699cf` (next only) recovery-release provenance.
- `2d26ce2` the mod-matrix overhead test leaves room for counter noise.

Storage:
- `aeb35e3` older formats rewritten once at boot; `306cba2`, `72d3698`, `2f7c023` only today's
  formats are read; `c829f7f` bank migration retired.
- `8aabd6b` autosave toggle.

## 2. Changes touching code we've modified

The fork changed these shared files (sizes from `git diff 0bc95e8 tessera-dev`), and upstream
changed them too:

| File | Fork change | Upstream change | Merge result |
| --- | --- | --- | --- |
| `seq.c` | +10: `tessera_ensure_init` in `seq_start`, `tessera_stop_all` in `seq_stop`, Tessera clocks the step in `seq_grid_tick` | +170 / −93: one-pass recording, transport, MIDI Start cancels count-in | 2 conflict hunks (start, stop) |
| `project.c` | +77: Tessera save / load / retag hooks, reset on load, template, new | +213 / −618: old formats dropped | merges clean; hooks land in the right functions (checked in the merged tree) |
| `storage.c` | +3: `OBJ_TESSERA` and its sector | +10 / −4 | 1 conflict hunk (the object enum) |
| `params.c` | +6: `GR_EVOLVE/FORM/TWEAK`, four pages | +101 / −52: `GR_FX`, `GR_SLCR` removed | 1 conflict hunk (the enum) |
| `ui_input.c` | +39: Tessera knob routing, mute kills Tessera notes | +253 / −194 | 1 conflict (37 lines) |
| `ui_pages.c` | +21: Tessera panel picture | +65 / −110 | merges clean |
| `ui_graph.c`, `ui_draw.c` | +223, +119: Tessera graphs | −104, +97 | merge clean |
| `ui_layer.c` | +7: GLO mute kills Tessera notes | +36 / −153 | merges clean |
| `ui_menu.c`, `melodee.c`, `console.c` | fork name, credits, includes | small | merge clean |
| `tests/run_tests.sh`, `tools/build.py`, `web/index_pkg.html`, `web/make_site.py` | package rename | reworked | conflicts (120, 9, 12, 10 lines) |
| `README.md`, `.gitignore` | ours | theirs | conflicts: take ours, then add their new ignores |

Points to check by hand after a merge:
- `seq.c`: upstream's one-pass recording changes `seq_start` / `seq_stop`. Tessera must still
  initialise before the first step and release its held notes on stop, also when MIDI Start or
  Stop drives the transport.
- `project.c`: the Tessera save is tagged with `proj_saved_hash`. Upstream now rewrites older
  formats once at boot (`aeb35e3`): a rewrite changes the record's hash, so a project saved by
  the fork before the merge would load with Tessera OFF the first time. Re-save once, or retag
  in the boot rewrite.
- `ui_pages.c`: the merged `pv_picture` no longer references the removed slicer (checked).
- Upstream deleted `perform_test.c` with the effects it tested. (Its failure here was the
  `tessera_kill` bug, fixed tonight in `279f08c`.)

### Flash layout: no conflict (correction)

On 2026-10-13 I reported that `OBJ_TESSERA` (0x9D000 / 0x9E000) shares sectors with project
slot 4. That was wrong. `st_sector` does map the legacy `OBJ_PROJECT3` there, but those legacy
single-pattern project objects are retired: `st_retired` blocks every load and save, and today's
projects live in `OBJ_BANK0..3` at 0xA0000–0xC7FFF. Upstream's own flash map (in `storage.c`, still
the same on `next`) says "0x9D000..0x9EFFF is free". The Prophet banks overlapping projects 1–3 are
the same retired objects, so that is not a bug either.

The real risk is upstream later using that free pair for something else. Mitigation: a host test
that asserts no two live objects share a sector, and a check of the flash map comment at every
upstream merge.

## 3. Changes to engines and features we plan to remove

- `99f8cbb` DIGITAL engine and its FM4/FM6 conversion removed.
- `5844283` ANALOG retired; track 1 powers on with PROPHET FUNK BASS II.
- `1e47387` every external effect removed (`perform.c`, `delay.c`, `slicer.c`, punch FX), then
  `38d5737` bounded track effects, `cb118c9` shared sends and a lean reverb (filter FX / Echo /
  Room at FS/4, an eight-line reverb at FS/8, 4 KiB per instance, shared DSP on CPU1), and
  `e9a6769` chorus and separate FX sends. Our FX plan (delay, reverb, bitcrusher, wavefolder,
  ring mod, comb) could build on this bounded design.
- Prophet: `17a048c` tone controls, `7cd66fb` DC block, `1bc4555` packed banks.
- `9de525a`, `2dff72d` on-device sound designer (for the engines we remove).
- `1960a6a` CZ-1 panel model trimmed to `cz_panel.h`; `3023205` FM6 tables built at boot.
- `45cced8` synth-only firmware install option.

## Merge difficulty

- **Textual:** small. 10 files conflict (12 hunks). About 24 more are modify/delete: files
  upstream deleted that the fork only touched by a file-mode change (644 → 755), resolved by
  taking the deletion. The same set conflicts against `next`.
- **Semantic:** moderate. The sequencer start/stop and recording paths, the project format
  and the UI pages were all reworked under Tessera.
- **Estimate:** half a day to a day for the merge and test fixes, then a session on the device
  to check playing, saving and loading.

Recommended order:
1. Graft or rebase the fork's history onto `0bc95e8`, so future merges have a real base
   (a decision: see QUESTIONS.md).
2. Merge `v1.3.1` (or `next`, for the CPU1 fix) on its own branch; take ours for README.
3. Build, run the tests (the generator baseline added tonight checks Tessera is unchanged), and
   check the points above.
4. Record the new synced version in ROADMAP.md.
