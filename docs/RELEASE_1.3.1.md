The updater no longer scans or relocates legacy Prophet banks at `0x89000`.
That abandoned region is treated as application flash. Current Prophet banks at
`0x93000` remain supported; older banks are no longer recovered automatically on upgrade.

This release also includes standalone command-line UBOOT recovery tools for Windows
x64, Linux x64, and macOS Intel/x64 and Apple Silicon/arm64. Each archive includes
Python, PyUSB, libusb, the verified RAM recovery helper, instructions and licences.

Extract the archive, open a terminal in its directory, and run `fm1-recover --info`
(`.\fm1-recover.exe --info` in Windows PowerShell). It requires a device already in
ROM UBOOT (`4c4a:8057`, `WL82 / UBOOT1.00`). A `.fwsc` argument makes a full backup;
add `--write` to restore application flash. The bootloader and current saved-data
regions are preserved and verified. Read `RECOVERY.md` in the archive for drivers,
permissions and the complete procedure.

Firmware downloads include both Standard/dual-core and Single core variants.
`BUILD_INFO.json` records the source commit and native executable smoke tests;
`SHA256SUMS` covers all release assets. The full firmware host suite and recovery
failure tests passed. Native executable tests do not prove physical USB recovery
on every OS. Removing relocation does not establish the cause of the 1.3.0 reports.
