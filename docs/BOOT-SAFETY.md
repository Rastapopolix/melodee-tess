# Interrupted firmware updates

Melodee's boot protection is adapted from [ChoralRoot FM-1 v0.16](https://github.com/Quixotic7/ChoralRootFM1/tree/v0.16),
commit `9c7445cdd7b767c779741d28654fb59cca3acd97`, under GPL-3.0-only.
The shared `CRSTUB` entry signature and `CRBM` marker format are retained.
Melodee 1.3.5 introduces this protection on the stable release line. Stock V15 and
Melodee through 1.3.4 do not gain protection until a build containing the stub is installed.

## Recovery

If an application update is interrupted, switch the FM-1 on again. Three short
key-light flashes mean the USB MIDI update loader is starting. Reconnect the USB
cable, open the installer and install the same package again. Sectors that already
match the package are skipped; this restarts the transfer and finishes the flash
write rather than continuing a saved transfer position.

Hold **OCT− alone** while switching on to request update mode even with a complete
application. OCT− and OCT+ together remain the application's calibration gesture.
One long flash means the loader was unavailable or the handover failed, and the
stub requested ROM UBOOT instead. Follow [USB recovery](../BUILDING.md#recover-a-black-screen-fm-1).

## Flash layout and commit

| Flash range | Purpose |
| --- | --- |
| `0x00000..0x03FFF` | SDK SPL and chip configuration; never written by our updater |
| `0x04000..0x0411F` | Application directory |
| `0x04120..0x04FFF` | Boot stub; entered before the application |
| `0x05000..0x90FFF` | Application body, linked at XIP `0x02001000` |
| `0x91000..0x91FFF` | Completion-marker sector; marker at `0x91FE0` |
| `0x92000..0x92FFF` | Configuration files and application-slot padding |
| `0xE0000..0xE3FFF` | Retained USB update loader |
| `0xE4000..0xE4FFF` | Separate update-record sector |

The 20-byte marker stores magic, format, aligned body length, body CRC-32 and
marker CRC-32. At each boot the stub checks the marker and recomputes the body
checksum before entering application code. Its key scan powers the panel rail
(PA2 low) and samples OCT−, OCT+ and an idle row over approximately 48 ms, using
the final 12 samples to allow settling and reject bounce or a stuck row.

Before the first changed application sector is erased, the loader erases the
marker sector and verifies that it is blank. It then writes sector `0x4000` only
if different, verifies all remaining sectors, and writes the marker sector last,
with the marker in its last programmed page. A malformed marker in a stub image
is rejected before any erase. Legacy packages keep their original write order.

Four compensation bytes following the marker fix the application directory's
CRC-16 at `0x4352`. The directory and the boot stub therefore remain identical
across builds with the same stub and SDK layout, despite different body sizes,
checksums and build options. Ordinary updates skip the entire boot sector.

The stub verifies staged-loader headers, ownership signature, sizes and compressed
block-chain structure before writing the RAM `UPDATA_PARM` record and resetting
into the SPL. Compressed payloads are not decompressed/checksummed by the stub;
the application's staging path checks the package loader CRC and verifies its
flash writes. A retained rescue word permits one loader handover; another arrival
without a successful boot requests ROM UBOOT instead of looping indefinitely.
Cold-boot recovery does **not** depend on the flash copy of `UPDATA_PARM`: ChoralRoot's
hardware investigation found that copy did not recover an interrupted application.

Successful application boots keep a complete Melodee loader. Cleanup erases only
the separate record sector, retaining existing progress/error/retry handling.
Damaged or foreign staging is erased. Presets, projects, samples and settings
outside the update windows are untouched. The stub reserves 32 bytes at RAM
`0x01C7C000`; application retained state begins at `0x01C7C020`.

## Stored data at first boot

Settings are checked before USB starts or a startup project/template loads.
Empty, invalid or incompatible foreign settings use defaults immediately,
including retained RAM preferences, then commit one owned PER5 record through
the existing A/B save. The display is flushed before this write. A failed write
keeps the previous committed object, shows an error and queues a retry; retry
also shows progress even if the RAM defaults have not changed.

Legacy PER5 has no firmware identity. Its tagged palette, settings schema and
complete optional template layout must match Melodee before it is accepted.
The longer ChoralRoot PER5 layout is rejected rather than truncated into our
USB/startup/global settings. A versioned `MDS` marker in the retired DIGITAL
favorites row records validation without changing the PER5 layout. This is a
compatibility marker, not authentication of the firmware that last ran.
Unknown future marker versions and recognizable but unreadable Melodee
templates stay protected for backup and explicit replacement.

Projects require known magic, size, hashes, parameter count and field bounds;
templates also require known layout, selection and engine IDs. A fork's unknown
engine must not wrap into an existing engine. Rejected projects remain
**OLD FORMAT**, occupied and excluded from automatic startup loading. Settings
cleanup does not format these slots or any existing preset bank.

## Preset reset policy from 1.3.6

Startup treats unsupported preset formats as damaged data to reset. Invalid
native FM6/CZ banks get the same empty defaults as missing banks. Unsupported
general user-preset banks become empty, while readable banks keep compatible
records and clear only unreadable entries. This deliberately discards unsupported
preset contents; back up before updating if another firmware may need them.
Projects and unsupported settings/template ownership retain their separate guards.

Progress is flushed before each reset. A/B commits keep the prior copy until
the default/repaired bank is fully written and verified. A failed write remains
visible and prevents later preset initialization from hiding it; restarting retries
the unfinished bank. A successfully reset bank is not rewritten on later boots.

Validation: Standard and Single core 1.3.6 builds pass; both packages retain the
boot sector of published 1.3.5. The full host suite passes. Simulated NOR tests
check unsupported general/FM6/CZ banks, preservation of good entries and project
bytes, visible writes, interruption before four different program calls, retry,
and no repeated writes after success. Physical device validation remains pending.

## Limits and verification

The first installation of the stub, a stub change, or a directory/SDK layout change
must rewrite the boot sector. First installations remain unprotected until that
sector is complete, including the preceding marker-sector erase. A power cut
during that phase or a boot-sector rewrite can still require hardware recovery. Keep the battery charged, connect
USB directly, and keep the computer awake. Installing older Melodee, stock firmware
or another package without a compatible stub removes this protection.

`firmware/stub/stub.lock` pins the stub version and SHA-256. A release build refuses
an unexpected stub; an intentional change requires a version bump, rebuilding with
`FM1_STUB_LOCK=update`, and release notes explaining the boot-sector rewrite.
The stub is compiled independently of application options, with no RAM data or
BSS and no application/HAL dependency. The build caps compressed loader size below
the separate record sector and caps the body below the marker sector.

Automated coverage:

- `tests/boot_stub_test.c`: CRCs against independent references; torn/invalid markers;
  loader-header and block-chain damage; rescue-key bounce, calibration and stuck rows;
  RAM requests, retry fallback, and exact SPL handover record.
- `tests/boot_stub_pkg_test.py`: varied body lengths, maximum size, overflow rejection,
  independent directory CRCs, and identical boot sectors across different bodies.
- `tests/loader_powercut_test.c` with `tests/boot_fixtures.py`: power cut after every
  erase and programmed page; boot decision from each state; bit-exact reinstall;
  first install, same-stub update, changed-stub update and mixed legacy firmware.
- `tests/ota_test.c`, `tests/ldr_test.c`, Python/JavaScript installer tests: retained
  loader, cleanup failures/retry, chip-key refusal, bounded writes and data preservation.

Synthetic fixtures live under `build/host/boot-fixtures` and must never be installed.
The host model does not prove physical boot behavior, USB enumeration after a cold
start, or power loss during a NOR operation. Hardware validation remains required:
normal warm/cold boots, OCT− rescue, both-key calibration, interrupted ordinary
updates followed by cold-boot recovery and reinstall, and preservation of musical
data. Do interruption testing only with an independent recovery route available.

Original next-branch validation on 2026-10-14: default, resident-audio dual-core and resident-audio
single-core target builds passed; both release configurations produced identical
sector `0x4000`. The host suite with `SOUND=0` passed. The four synthetic update
scenarios exercised 8,555 cut points, including 2,126 with an unchanged stub and
no unprotected state. An additional single-core to dual-core update exercised
2,126 cut points with no unprotected state. Stub tests passed 16,143 checks,
package tests passed 265 checks, and the two staged-loader checkers agreed on
20,480 damaged copies. These are automated results; no device was flashed or
physically power-cycled for this change.

1.3.5 backport validation on 2026-10-14: Standard and Single core release builds
passed, used the pinned version-3 stub, and produced identical sector `0x4000`.
The full host suite passed against the Standard build (including sound and target
render-cost checks). Boot-stub tests passed 16,143 checks and package tests passed
265 checks. The four synthetic scenarios exercised 8,215 cut points. A separate
simulation using the genuine, hash-verified official V15 package exercised 2,432
cut points: 2,414 selected rescue, one was the completed boot, and 17 were in the
initial unprotected phase before the stub sector was complete. Single core to
Standard exercised 2,041 cut points with no unprotected state. Across these six
scenarios, 12,688 interruption positions were checked. Genuine V15 recovery-package
validation also passed. No device was flashed or physically power-cycled.

Startup-display hotfix validation on 2026-10-14: both release variants passed
build checks and kept the pinned boot sector identical. The full host suite,
including sound and target render-cost checks, passed again. Simulated NOR tests
verify that the warning is flushed before the twelve first-boot bank erases,
that completed banks are preserved after a failure, and that retry initializes
only the nine unfinished banks in the injected partial-failure case. UI tests
verify that working screens consume input and that errors remain visible above
menus/dialogs, do not sleep, and require OCT+ acknowledgment. Progress and error
renders were visually checked. Physical first-boot/display validation remains
pending.

First-boot data hotfix validation on 2026-10-14: the full host suite passed in
57 seconds, including sound, target cost, backup/restore and storage safety.
Both release variants built successfully and retained the same pinned stub and
boot sector. Simulated NOR tests cover the CRC-valid 764-byte ChoralRoot PER5
collision, erased storage, 31 random settings-sector fills, CRC-valid malformed
settings, retained RAM, compatible legacy settings/templates, intentional USB
OFF, future ownership versions, and all four settings-program interruption points.
Successful retries persist defaults even when RAM has not changed, and subsequent
boots do not rewrite them. Checksums outside the two settings sectors remain
unchanged during cleanup. CRC-valid incompatible bank/project layouts and unknown
engine IDs cannot load at boot and keep their raw bytes; incompatible template
engines stay inert while compatible settings survive. Settings progress/error
renders were visually checked. Physical stock-to-Melodee first-boot validation
remains pending.
