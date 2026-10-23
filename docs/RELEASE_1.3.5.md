Melodee 1.3.5 adds a boot stub and completion marker to recover interrupted
application updates. It builds on 1.3.4 and keeps its clock behavior: Melodee
never writes the system PLL.

The startup display hotfix brings forward the 1.4 development line's early
screen and maintenance progress. The FM-1 shows **STARTING UP** before reading
stored data, then **INITIALIZING PRESETS** with a progress bar when first-boot
banks are created, or **FINISHING UPDATE** during update cleanup. The display
says to keep power connected and wait. The watchdog is serviced between
maintenance writes. A failed operation stays visible with **Restart to retry**;
press **OCT+** to acknowledge and continue. Existing preset banks follow the
same initialization rules as 1.3.4.

First boot validates the complete settings object before using it. Empty,
damaged, stock or incompatible fork settings show **RESETTING SETTINGS** and
save sane defaults once: stereo USB audio enabled, startup project OFF,
440 Hz tuning, default panel/display preferences and AUTOSAVE ON. Retained RAM
preferences are replaced too. In particular, a fork's longer PER5 record is no
longer truncated and mistaken for Melodee settings. The existing A/B save keeps
the previous committed copy if cleanup is interrupted; safe defaults apply in
RAM and cleanup retries. Valid Melodee settings and supported templates are
preserved and marked during a one-time **CHECKING SETTINGS** pass. The marker
uses unused bytes of the retired DIGITAL favorites row, preserving the PER5
layout and compatibility with earlier Melodee builds.

Projects and templates must match a supported schema, including the parameter
count, track selection and engine IDs. Unknown engine IDs cannot wrap into a
Melodee engine. Incompatible projects are kept as **OLD FORMAT**, excluded from
startup loading and protected against automatic overwrite; settings cleanup
does not erase musical data. A marked Melodee record with an unsupported template
or future settings version remains protected for backup and explicit replacement.
Legacy records have no firmware identity, so acceptance establishes supported
layout compatibility rather than proving which firmware wrote them.

- Before starting Melodee, the stub checks the completion marker and the
  application checksum. An incomplete application requests the retained USB MIDI
  update loader. Reconnect USB and install the same package again; matching
  sectors are skipped. ROM UBOOT is the fallback if the loader cannot be used.
- Hold **OCT− alone** while switching on to request update mode even with a
  complete application. Three short key-light flashes signal loader handover;
  one long flash signals a request for ROM UBOOT. Both octave buttons at
  power-on still select calibration.
- Successful boots retain the rescue loader and clear only its separate update
  record. Boot-stub version 3 is pinned by checksum. Builds with the same stub
  and SDK layout keep the application boot sector identical, including between
  Standard and Single core variants, so ordinary updates skip that sector.
- Boot protection does not repair arbitrary startup faults, and installing
  stock firmware or older firmware without a compatible stub removes it.

**First-install limit:** stock V15 and Melodee through 1.3.4 have no compatible
stub. Installing 1.3.5 must write sector `0x4000`. The initial update phase is
unprotected until that sector is complete, including the preceding marker-sector
erase; an interruption in that phase can still require hardware recovery.
The same limit applies to a later stub or directory/SDK layout change. Keep
the FM-1 charged, USB connected and the computer awake throughout installation.

Automated boot-stub, package and interruption checks exercise recovery decisions,
retained-loader handling, write ordering and protected flash regions. Physical
warm/cold boot, OCT− rescue and interrupted-update recovery validation remains
pending; automated results do not establish hardware recovery behavior.

Firmware downloads include Standard (dual-core) and Single core variants.
For a device already in ROM UBOOT, use the platform recovery tool and retain
its backup before restoring firmware; see `RECOVERY.md` in the archive.
`BUILD_INFO.json` records source and native-build provenance, and `SHA256SUMS`
covers the release assets.

The boot protection is adapted from ChoralRoot FM-1 v0.16 under GPL-3.0-only;
its notices and the shared `CRSTUB` / `CRBM` format are retained.
