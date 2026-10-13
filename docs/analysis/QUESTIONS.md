# Questions for Theo

Decisions the overnight run (2026-10-14, branch `overnight-analysis`) could not make alone.

## Upstream

1. **Attach the fork's history to upstream?** The fork does not share git history with upstream,
   so merges need `--merge-base 0bc95e8` every time. Options: `git replace --graft f93496f 0bc95e8`
   (local only, rewrites nothing) or rebase the fork's commits onto `0bc95e8` (rewrites history;
   fine while nothing is pushed). Recommendation: rebase before the first merge, since nothing has
   been pushed yet.
2. **Merge `v1.3.1` or `next`?** `next` adds a CPU1 retirement fix and recovery tooling on top of
   v1.3.1. Recommendation: the release (v1.3.1), then cherry-pick `7025143` if CPU1 misbehaves.
3. **Merge before phase 1?** Recommendation: yes. It frees RAM (608 B of arena headroom today) and
   removes the failing perform test with the code it tests.
