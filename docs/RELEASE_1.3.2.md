Melodee 1.3.2 fixes FM-1s that stayed on a black screen and came back as
**WL82 UBOOT1.00** after installing 1.3.0 or 1.3.1.

1.3.0 made 320 MHz the default, which retunes the FM-1's system clock at every start,
before the screen and USB come up. On some units, often the ones that had trouble with
the second core, that retune fails; the watchdog restarts the FM-1, the next start fails
the same way, and after two such starts the boot guard enters UBOOT.

- Melodee starts at the boot loader's **360 MHz** again and does not touch the clock, as
  1.2.1 did. 320 MHz remains available in the HOME-held menu's **OVERCLOCK**.
- A 320 MHz session that crashes or hangs is followed by a start that leaves the clock
  untouched and saves 360 MHz (**CPU FAULT: 360 MHZ**). If the retune itself fails, the
  FM-1 says **PLL FAULT: 360 MHZ** once and OVERCLOCK reads **360 ONLY** from then on.
- The retune runs only once the watchdog reads back as armed (after johnskiller's
  follow-ups to #27).
- A crash or a lost second core at 360 MHz no longer moves the FM-1 to 320 MHz.
- The 320 MHz default saved by 1.3.0 and 1.3.1 reads as 360 MHz; choose 320 MHz again in
  OVERCLOCK if you want it.
- A retired second core stays off until power-off from the moment it is retired.
- The recovery downloads accept the unmodified official M-VAVE V15 `FM-1.fwsc`, verified
  by whole-file SHA-256, as the corrected 1.3.1 archives did.

If an FM-1 is stuck in UBOOT, restore it with the recovery tool from these downloads
(`fm1-recover --info`, then a `.fwsc` backup run, then `--write`; see `RECOVERY.md` in the
archive) or with M-VAVE's updater, then install 1.3.2. An FM-1 still running 1.3.0 or
1.3.1 can avoid the risk by setting OVERCLOCK to 360 MHz before updating.

Firmware downloads include Standard (dual-core) and Single core variants.
`BUILD_INFO.json` records the source commit and native executable smoke tests;
`SHA256SUMS` covers all release assets.
