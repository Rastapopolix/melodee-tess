Melodee 1.3.6 backports the stock V15 power startup to the 1.3.5 stable line.
Both Standard and Single core apply the stock analog-register sequence, the
LDO-to-DCDC handover and original settling loops, then select SYSVDD 15
(nominal 1.38 V) and VDC14 7 (nominal 1.60 V). This happens before peripherals
and CPU1 start. Transfer failures or incorrect read-back keep CPU1 off; existing
boot guards and worker retirement remain. The system PLL is not changed.
Matching stock's supply setup does not yet establish that all instability is
resolved: physical rail measurements, cold/soft boot and loaded audio/CPU1
validation remain pending.

The release also retains the prepared stable backports:

- Hold EDIT to undo; EDIT+OCT−/OCT+ performs undo/redo. Hold SAVE saves the
  current project, stopping playback first and opening the project chooser
  when the project has no saved slot.
- MIDI channel selection is clamped to the supported range.

Melodee 1.3.6 resets unsupported user preset banks at startup instead of leaving
them as OLD FORMAT. This intentionally discards unreadable preset-bank contents.
Keep a backup before upgrading if those bytes may be needed by another firmware.

- The four native FM6 and eight native CZ banks initialize to the same empty
  defaults as a fresh installation when their format is unsupported or damaged.
  Factory sounds remain available separately.
- Unsupported general user-preset banks reset to empty. In a readable bank,
  only unsupported entries are cleared; compatible presets survive.
- Startup shows progress before writes. Each reset uses the existing A/B commit:
  failure retains the previous committed copy and an error remains visible;
  restart retries unfinished resets. Successful resets are not repeated.
- Projects retain OLD FORMAT protection. Compatible settings and preset banks
  are kept. The firmware does not change the system PLL.

The 1.3.5 boot stub, early display and first-boot settings cleanup remain included.
On first installation from stock or Melodee through 1.3.4, interruption before
the boot sector is complete can still require hardware recovery. Keep power
connected throughout installation and startup maintenance.

Automated checks cover bank validation, interrupted resets, compatible presets,
project preservation, both firmware variants and the existing regression suite.
Physical stock-to-Melodee startup and recovery validation remains pending.
