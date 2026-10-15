Melodee 1.3.4 removes the OVERCLOCK menu and its 320 MHz option.

On some FM-1s, often the ones that also had trouble with the second core, changing the
system clock at start-up failed before the screen and USB came up; two such starts in a
row put the FM-1 in UBOOT behind a black screen. 1.3.2 and 1.3.3 made 360 MHz the default
and guarded the option. 1.3.4 takes it away: Melodee runs at the 360 MHz the boot loader
sets and never writes the clock.

- A 320 MHz setting saved by 1.3.x is ignored (left as saved, never read).
- An FM-1 updated from a session that ran at 320 MHz keeps that clock until it is
  switched off, then starts at 360 MHz. The voice budget follows the clock running.
- Nothing else changes: the boot order, the second core and the boot guard are as in
  1.3.3.

If an FM-1 is stuck in UBOOT, restore it with the recovery tool from these downloads
(`fm1-recover --info`, then a `.fwsc` backup run, then `--write`; see `RECOVERY.md` in the
archive) or with M-VAVE's updater, then install 1.3.4. Running the recovery tool once
without `--write` first saves a backup that shows why the FM-1 stopped; please attach it
to a bug report.

Firmware downloads include Standard (dual-core) and Single core variants.
`BUILD_INFO.json` records the source commit and native executable smoke tests;
`SHA256SUMS` covers all release assets.
