# Upstream review: Melodee 1.4

Written 2026-10-17 on branch `upstream-1.4-picks`. Upstream `v1.4.1` (2026-10-15) and `0c21568` after it:
100 commits after `v1.3.6` (`a4e6f22`, our last merge), 336 files, about +34k / −5k lines. A trial merge of
the whole release into `phase4-dev` stops on 28 files (about 55 conflict blocks).

**Decision (Theo, 2026-10-17):** take only what still makes sense for Tessera as its own firmware, with its
own feature set, character and look. From 1.4 on, the fork **picks commits** instead of merging whole
releases (ROADMAP > Upstream sync).

## What upstream 1.4 is

- A new look ("Large type"): Rubik and Melodee Pixel fonts, Stage as coloured rows, palettes, a HOME
  circle-of-fifths view, rack views for FX.
- One shared preset bank, U001–U384, for every engine (Prophet, FM6, CZ-1, SID, Drum), spread over all of
  the old preset flash, including 0x9D000..0x9EFFF.
- One to four tracks (`song.ntrk`; a new project starts with one).
- SID: C64 6581/8580 models, POLY chip arpeggios, SID KIT and SID DIGI drum kits.
- Audio: DSP optimizations, a shared voice BUDGET, RAM reclaimed for audio.
- FX: a TONE control and low/high cuts per slot, boom-free time effects.
- Recording: REC stays armed across loops, whole-take undo, an empty pattern arms one pass.
- Platform: dual-core builds only (single-core packages retired; CPU1's fault fallback kept), the 320 MHz
  option removed, one combined USB audio device, OTA recovery of interrupted updates, the stock V15
  supply startup, an installer fix for returning to stock.
- An online user manual.

## Taken (branch `upstream-1.4-picks`, from `phase4-dev`)

| Upstream | What | Why it fits Tessera |
| --- | --- | --- |
| `1f1229d` | CZ-1 knob test: the render's last block stays inside its buffer | fixes the suite's one failure (#47) while CZ-1 is still built |
| `5e538e1` | RAM reclaimed for audio | headroom for MACRO, FX and generators |
| `6da744c` | DSP optimizations, shared voice BUDGET (GLO knob 2) | less CPU per voice; the budget suits four generator tracks |
| `0d1963f` | batching on a single core | CPU1 can still retire itself |
| `18435e8` `aecf908` `b72135b` `7b9891b` | SID: C64 models, POLY arpeggio, envelope fixes | SID stays in Tessera (phase 4 decision) |
| `a627989` `d3a06b8` `05f88c2` `9564073` | SID KIT and SID DIGI drums | percussion on the kept engines; good with GRIDS |

Already in Tessera through 1.3.6: `7a51700` (the stock V15 supply startup, upstream's `5e66df3`).

Adaptations: Tessera's side kept in every conflict (DRIFT's `cord_tick`, the `melodee-tess` names, the engine
switches and `MELODEE_PLAITS`, 1.3.6's settings checks); the saved USB/BUDGET word takes upstream's final
check (1.4.1), or a saved BUDGET reads as damaged settings; BUDGET gets parameter id `0x129 VOICE BUDGET`.
README.md stays Tessera's.

Result: full host suite passes (the first time since the CZ-1 test broke). Audio arena 146,912 → 162,912 B
(MACRO build, three engines out: 176,416 B). App 531,020 → 544,828 B (MACRO build 532,968 B).

## Left for later (worth it, but in risky areas or tangled)

- `229dacd` OTA: recover interrupted firmware updates (boot stub, loader, packaging: 14 files conflict).
  Valuable for any firmware. Its own session, with a power-cut test on the device.
- `569b820` boot storage safeguards (11 files; much of it is the shared bank's). Take the parts that
  protect projects and settings, not the bank's.
- `0c21568`, `007e44e` installer: stock restore keeps boot data. Take when the Tessera installer gets its
  own identity (phase 4 hard fork).
- `365e699` `4182267` `e9f0fa6` FX: TONE, low/high cuts, boom-free repeats. Take the DSP when building
  "FX with caps"; leave the rack UI.
- `94b941d` one combined USB audio device: neutral; take if Tessera keeps USB audio as it is.

## Left out (not Tessera)

- The "Large type" look, fonts, palettes, Stage rows, HOME chord view, Stage light (`17412df` .. `093f1e6`,
  `fafb78b`, `828b532`, `cbba4f5`): Tessera has its own visual language (ROADMAP > UI). Ideas to keep in
  mind: the font table shrink (53.5 → 31.4 KB of flash) and partial redraws.
- The shared preset bank (`c717cf5`, `686ea56`): built around Prophet, FM6 and CZ-1, which Tessera drops;
  its flash use is the collision below. Tessera's "own project format" task decides its sound storage.
- One to four tracks (`7ee8252`): Tessera's generators, FOLLOW, cords and the default patch assume four;
  a track count may come later as Tessera's own (Entries already brings tracks in and out musically).
- Dual-core only (`25a2bef`), the 320 MHz removal (`fbd9528`): Tessera keeps its single-core rule for now.
  Revisit once the V15 supply startup has proved itself on units whose CPU1 misread SRAM.
- Recording changes (`167e9f2`, `92a4594`, `1730675`), EDIT map navigation, the manual: Melodee's own
  workflow; Tessera's performance page and help come in phase 7.
- `ac1954d` copyright attribution: rewrites headers of files Tessera does not have; Tessera keeps every
  existing attribution (over-crediting Felucca in a few files is harmless).

## Flash: Melodee 1.4 and Tessera share 0x9D000

Tessera keeps its generator state (`OBJ_TESSERA`, object 28, 836 B plus the cords) at 0x9D000..0x9EFFF,
the pair that was free in 1.3.x. Melodee 1.4 made the same object number and pair the last part of its
shared preset bank (`OBJ_SHARED_EXT`), and its other preset objects now cover every former bank. There is
no flash a 1.4 unit leaves unused, so moving Tessera's object would not avoid the problem.

**Risk today:** on a unit that ran Melodee 1.4, Tessera finds data it cannot read at 0x9D000, starts empty
and, at the next project save, writes its own object over 1.4's presets there. Going back to Melodee 1.4
then loses those presets. Melodee 1.2.x–1.3.x data is not affected.

**Proposal (to decide):** Tessera owns the flash, as a firmware of its own should, and makes the handover
explicit instead of moving objects around:

1. The Tessera installer takes a full backup before flashing a unit that runs Melodee 1.4 or later (it can
   read the version from the device identity), as upstream's stock-restore flow does.
2. At first boot, Tessera recognises a foreign object in its sectors (a valid store header, not Tessera's
   magic) and keeps it until the player saves; the first save then asks once ("Melodee 1.4 data here will
   be replaced").
3. Tessera's own layout comes with the "own project format" task: with FM6, CZ-1 and Prophet gone, their
   banks (0x93000..0x9CFFF, 0xC8000..0xDBFFF, 0x9F000) are Tessera's to use for projects, generator state
   and its own sound bank.

Until then: back up before installing Tessera on a unit that ran Melodee 1.4.
