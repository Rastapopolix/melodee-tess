# Housekeeping: backup files and stray branches

Checked 2026-10-14. **Done the same day** (Theo: "do the housekeeping"): the commands below were run.

## Backup files (32, none tracked by git)

Each file was compared with every version of its file in git, on every branch.

- **16 are byte-for-byte copies** of a version already in git. Deleting them loses nothing:
  `console.c.prefork`, `melodee.c.prefork`, `params.c.bak4`, `project.c.prenew`, `project.c.presave`,
  `storage.c.presave`, `tessera.c.bak4`, `tessera.h.bak4`, `ui_draw.c.bak4`, `ui_graph.c.bak4`, `ui_input.c.bak4`,
  `ui_menu.c.prename3` (in `firmware/src/`), `tools/build.py.prename2`, `tools/build.py.prerename`,
  `web/index_pkg.html.prefork`, `web/make_site.py.prename`.
- **14 are older drafts** whose lines all exist in some version in git, apart from a line or two of no value
  (an older `GR_` enum line, a menu text, one `tess_active` condition, two Euclid save lines of an older format):
  `params.c.bak6`, `tessera.c.bak5`, `tessera.c.bak6`, `tessera.c.preram`, `tessera.c.presave`, `tessera.h.bak5`,
  `tessera.h.bak6`, `tessera.h.preram`, `tessera.h.presave`, `ui_draw.c.bak6`, `ui_draw.c.preflourish`,
  `ui_graph.c.preram`, `ui_input.c.bak6`, `ui_menu.c.preabout`.
- **2 hold work that is nowhere in git:** `tessera.c.preflourish` and `tessera.h.preflourish`. They are an
  abandoned, richer save format: 189 bytes a track (756 in all) instead of today's 45 (180). It also saved each
  FORM section's recorded LOCK phrase (32 notes, 16 velocities), which sections were visited and valid, and the
  Turing register a phrase starts from. Today a loaded project re-records its LOCK phrases on the first loop.
  Worth keeping as a reference for phase 1 (the saved state of `gen_t`) before deleting.

## Stray branches

| Branch | Has anything no other branch has? | Recommendation |
| --- | --- | --- |
| `master` (`a8ad7ec`, "before visualizer patch") | No: it is an older point of `main` | Delete |
| `cleanup-claude-md` (`b8f6844`) | No: it is the `main` of 2026-10-12 | Delete |
| `tools-util` (`c9557b7`) | One commit: `tools/util.py` shares a CRC-16, `f32` and the package identity reader among `build.py`, `fm1pkg_make.py`, the table generators, `fm1_install.py` and `make_site.py` | Drop: it rewrites six upstream files (CLAUDE.md: avoid that), and upstream has since reworked `build.py` and `make_site.py`, so it would conflict at every sync. Redo after the hard fork if wanted |

## Commands (not run)

```
# keep the abandoned save format as a reference, then delete every backup file
mkdir -p docs/analysis/attic
mv firmware/src/tessera.c.preflourish firmware/src/tessera.h.preflourish docs/analysis/attic/
git add docs/analysis/attic
find firmware/src tools web -maxdepth 1 -type f \( -name '*.bak*' -o -name '*.pre*' \) -delete

# the branches (only local; none of them is on GitHub)
git branch -D master cleanup-claude-md tools-util
```
