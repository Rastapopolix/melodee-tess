# Questions for Theo

Decisions the overnight run (2026-10-14, branch `overnight-analysis`) could not make alone.

**Answered 2026-10-14: all as recommended**, with one change to 1: `main` was already on GitHub, so
the history was connected by a no-change "ours" merge of `0bc95e8` (`4621fa4`) instead of a rebase.
v1.3.1 is merged (`1b4c9e9`, branch `merge-upstream-1.3.1`). Still to do: report the CZ-1 test
upstream (5); FORM development (6) in phase 6; Markov's default (7) at the next baseline change.

## Upstream

1. **Attach the fork's history to upstream?** The fork does not share git history with upstream,
   so merges need `--merge-base 0bc95e8` every time. Options: `git replace --graft f93496f 0bc95e8`
   (local only, rewrites nothing) or rebase the fork's commits onto `0bc95e8` (rewrites history;
   fine while nothing is pushed). Recommendation: rebase before the first merge, since nothing has
   been pushed yet.
2. **Merge `v1.3.1` or `next`?** `next` adds a CPU1 retirement fix and recovery tooling on top of
   v1.3.1. Recommendation: the release (v1.3.1), then cherry-pick `7025143` if CPU1 misbehaves.
3. **Merge before phase 1?** Recommendation: yes. It frees RAM (static RAM has 36 B free, the audio
   arena 608 B) and touches the same sequencer hooks the extraction will move.

## Tests and fixes

4. **Keep the stuck-notes fix?** Task 4 asked for a passing suite; two of the three failures were one
   bug in `tessera_kill` (it cleared the sequencer's note list before releasing it, so stopping the
   transport left step-sequencer notes hanging). I fixed it in its own commit (`279f08c`, one line in
   `tessera.c`): regress and the perform test now pass, and the Tessera baseline is unchanged. It is the
   only firmware change tonight. If you would rather review it first: `git revert 279f08c`.
5. **The CZ-1 knob test still fails** (update 2026-10-14: the draft report for upstream is
   `docs/analysis/upstream-cz1-report.md`, **posted as https://github.com/keremimo/melodee/issues/47**: on upstream `next` it does not even build with GCC 14+, the renders differ
   under GCC 15, and with clang four factory tones have dim knobs that change the sound) (it is the only failing suite). It fails the same way on upstream
   v1.1.0 under GCC 15 / Linux. Probed tonight: two identical renders already differ at sample 11, even
   with the shared random generator and the LFO phase reset between them, so some CZ-1 or voice state
   carries over between renders that the test's `reset_all` does not clear. CZ-1 is removed in phase 4.
   Options: report it upstream (with the probe), skip the suite on Linux, or leave it failing until
   phase 4. Recommendation: report upstream and leave it failing (do not hide it).

## Design

6. **FORM never develops.** Once its sections exist, a form replays them exactly forever (measured: a
   Turing ABAC form identical for two hours; `docs/analysis/generators.md`). Is that the intended
   "A is home" behaviour, or should returns develop (recalled section + a little mutation pulled back
   by Gravity)? Recommendation: develop, with Gravity, in phase 6.
7. **Markov's DENSITY defaults to 100** (a note on every step, forever). Lower the default (say 70)
   when the baseline is next rewritten on purpose?

## Found 2026-10-14, checks while waiting for the device test

8. **Backups leave out the generators.** The full backup (web editor > backup, `editor_backup.c`, `web/fm1backup.js`)
   is a fixed list of objects, without `OBJ_TESSERA`. Restored onto the same FM-1 the generators come back (their
   block is still there and still matches); onto a fresh or another FM-1, every project loads with Tessera OFF.
   Recommendation: add `OBJ_TESSERA` to the backup list (firmware, web page and its test) before a release.
   **Answered (Theo: add them), done on `merge-upstream-1.3.1`:** backup objects 28 (each saved slot's generators,
   as stored) and 29 (the live ones). An archive from this firmware does not restore on older firmware (refused
   before any write); older archives restore here, the generators left as they are.
9. **Turing ignores the track's LEN**, Euclid follows it (`track_len_shorter_than_loop` in the baseline). Should a
   track's LEN bound every generator? Recommendation: decide with the phase 1 interface (`gen_clock_t`).
10. **The live simulator's demo** set EVOLVE's AMOUNT to 0 % (track 1 never evolved); fixed on `web-sim` (`73805aa`,
   "the demo's EVOLVE AMOUNT stays at 30 %"). **Answered:** on the live site since 14 October 2026 (`main` `a4533b5`).

