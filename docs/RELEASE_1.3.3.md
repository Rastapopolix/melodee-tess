Melodee 1.3.3 removes the last clock change at 360 MHz.

1.3.2 made 360 MHz the default, but still retuned the system clock once: on the first
start after updating from a 1.3.0 or 1.3.1 that ran at 320 MHz, because the clock survives
the update's restart. On the FM-1s that cannot take a retune (1.3.0 and 1.3.1 left them in
UBOOT behind a black screen), that is the step that fails.

- At 360 MHz Melodee never touches the clock. A 320 MHz left by an update stays until the
  next power-off; the FM-1 starts at 360 MHz after that.
- Switching OVERCLOCK to 360 MHz applies at the next power-on and says
  **360 MHZ AT POWER-ON**. Switching to 320 MHz still offers a restart.
- Everything from 1.3.2 stays: 360 MHz by default, the clock guard (**CPU FAULT** /
  **PLL FAULT: 360 MHZ**, **360 ONLY**), and the retune only with a verified watchdog.

If an FM-1 is stuck in UBOOT, restore it with the recovery tool from these downloads
(`fm1-recover --info`, then a `.fwsc` backup run, then `--write`; see `RECOVERY.md` in the
archive) or with M-VAVE's updater, then install 1.3.3. Running the recovery tool once
without `--write` first saves a backup that shows why the FM-1 stopped; please attach it
to a bug report.

Firmware downloads include Standard (dual-core) and Single core variants.
`BUILD_INFO.json` records the source commit and native executable smoke tests;
`SHA256SUMS` covers all release assets.
