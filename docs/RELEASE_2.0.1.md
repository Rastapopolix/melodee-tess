**Tessera 2.0.1** is a small update to [Tessera 2.0](https://github.com/Rastapopolix/tessera/releases/tag/v2.0): a crisper screen, a fix on MORPH, and an
installer and simulator that are easier to live with. Projects, presets and settings carry over from 2.0 as they are.

### 🖥️ The screen
* **MENU > PANEL GAMMA**: the screen's contrast curve. The new default, **TUNED+VCOM**, gives the panel the contrast
  the pictures in the manual have; 2.0 left it at the chip's flatter default. TUNED and the chip's own G2.2, G1.8,
  G2.5 and G1.0 are there too. The screen goes black for a moment as it changes. Saved with your settings.

### 🔧 Fixes
* **MORPH** lights the bars at the playhead, as the other generators do (2.0 lit only a thin line under them).

### 🌐 Installer, simulator and site
* The **simulator** starts with **KOTO GARDEN** loaded (STILL TIDE is in slot B), and listens to **MIDI** only when
  you tick **MIDI in**: an FM-1 or a DAW left plugged in no longer plays it behind your back.
* The **installer** and the **editor** have the project page's look.
* The installer **counts finished installs** of the released builds at GoatCounter: the version and the build only,
  no cookies or personal data. Local test files are never counted. Open the installer once with `#nocount` at the
  end of its address to leave your browser out.
* A new **demo video** (four minutes): the two demos, then the pages that shape any generator (EVOLVE, FORM, TWEAK,
  TWEAK 2, LENGTH and TIMING), then the other modes.

### 🏷️ Good to know
* Tessera now has its **own package identity**, `FM-1_8377` and the version (`FM-1_83772001` for 2.0.1), so tools
  such as the Firmware Hub can tell it from upstream Melodee. Nothing changes for you: install as before. The
  installer still accepts the 2.0 packages.

### 📦 Installation
Open **[rastapopolix.github.io/tessera](https://rastapopolix.github.io/tessera/)** in Chrome or Edge, connect the
FM-1 by USB and press **Install**. The installer can also put the official M-VAVE firmware back. Or take
`tessera-2.0.1.fwsc` (dual core) or `tessera-2.0.1-1core.fwsc` from this release.
