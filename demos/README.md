# Tessera 2.0 demo projects

Three projects that show what Tessera 2.0 does, each a few minutes of music that keeps changing on its own. Press
PLAY and leave it, then open the pages named below to see what is moving and turn things.

| File | Name on the FM-1 | What it shows |
|---|---|---|
| `still-tide.tsp` | STILL TIDE | Ambient: CHORD DRONE with BREATH, ENERGY's ARC and RESTS, FM6 and TRIO pads, FALL |
| `mosaic.tsp` | MOSAIC | A kirtan from below: a breathing fifth drone, a chanting choir on TURING, hand bells on POLY, ATLAS pounding on the 808, ENERGY's arc |
| `koto-garden.tsp` | KOTO GARDEN | ZEN-1's and FuMi-1's FM voices in HIRA: QUAD across two tracks, SERIES, POLY |

They are made by the firmware's own code (`tests/demo_projects.c`), which saves each one to a slot, reads it back
as the backup page does, loads it and plays it; `tests/run_tests.sh` checks that these files still load and play.

## Putting them on your FM-1

The demos go into your slots through a backup of your own FM-1, so everything else on it stays as it is.

1. Connect the FM-1 to the web editor and stop playback. **Complete backup** > **Save backup**: keep that file safe,
   it is your way back.
2. Put the demos into a copy of it (Python 3, from this repository):

   ```
   python3 tools/demo_backup.py my-backup.json my-backup-with-demos.json
   ```

   That puts STILL TIDE, MOSAIC and KOTO GARDEN into slots **B**, **C** and **D** and lists what each slot held.
   `--slots D` puts only STILL TIDE into D; `--slots C,D demos/mosaic.tsp demos/koto-garden.tsp` picks the demos.
3. **Complete backup** > **Restore backup**, and pick `my-backup-with-demos.json`. Then switch the FM-1 off and on.
4. SAVE > PROJECT: pick the slot and load it, then PLAY.

The slots you choose are replaced: save anything you want to keep in them first (or choose other slots). To go back,
restore the backup from step 1.

## STILL TIDE (slot B)

YO on D, 72 BPM. Made to run for an hour.

- **T1 FM6 SOFT PAD**, CHORD: PROG **DRONE**, BARS 4, HIT HOLD; hold OCT+ on GEN for CHRD DIA7, VOICING OPEN, OCT −1
  and **BREATH 80**: each voice of the chord swells and fades on its own slow cycle.
- **T2 TRIO WARM PAD**, AWAKE at half notes, PROB 55, LENGTH **HIT** (each note rings to the next); EVOLVE every 8
  bars.
- **T3 FM6 FURIN**, BOUNCE with 5 balls: **FALL**. Leaves land on wind chimes; DROP and HUMAN loosen them.
- **T4 TRIO SUB BASS**, MARKOV, mostly neighbours and rests, an octave down.
- **ENERGY**: **ARC 60** thins the generators over 3 to 6 minutes and fills them again; **RESTS 70** gives them a rest
  of 8 to 18 s every few minutes while the drone holds.
- **MODS**: T1 TIDE 1 → T1 CUTOFF, T1 SWARM 2 → T1 PAN, T3 TIDE 2 → T3 WIND (B ANGLE), ENERGY → T2 FX SEND.
- Send effects: a long REVERB, an ECHO on the chimes and the pad. Try MENU > LIGHTS **AUTO** for a dark panel.

## MOSAIC (slot C)

C phrygian (the Indian Bhairavi), 92 BPM: a chant that goes round and round over a drone, dark and devotional at once.

- **T1 TRIO FIFTH DRONE**, CHORD: PROG **DRONE**, a strike every 4 bars, held; CHRD **POW** (root, fifth, octave),
  VOICING +OCT, and **BREATH 70**, so the drone's voices swell and fade on their own.
- **T2 TRIO CHIP CHOIR**, the chant: TURING LEN 16 in eighths, PROB 4 (nearly locked), one octave, SHIFT +24,
  LENGTH **HIT** and VOICE LEGATO, so one voice sings from note to note. **FORM AB** of 4 bars answers the phrase
  with a second line.
- **T3 FM6 RIN**, hand bells: **POLY** with rhythms /3, /4 and /5, half of them dropped.
- **T4 DRUM 808 KIT**, **ATLAS** X 192, Y 64, with **TOMS 60** as a second drum.
- **ENERGY ARC 50** builds the generators up and pulls them back over minutes.
- **MODS**: T1 TIDE 2 → T1 CUTOFF, T2 SWARM 3 → T2 PAN, ENERGY → T4 AT DENS (the drums swell with the arc).

## KOTO GARDEN (slot D)

HIRA (Hirajoshi) on C, 92 BPM.

- **T1 FM6 KOTO PLUCK** and **T2 FM6 KOTO RING**, both **QUAD**: two rows of one shared grid, T1 in eighths, T2 in
  eighth triplets (pendulum), so they drift against each other. Hold OCT+ on GEN: column 1 moves **DOWN** every 8
  steps and column 3 at random every 16, so notes travel between the two kotos.
- **T3 TRIO BREATH FLUTE**, **SERIES** (the infinity series) at quarter notes, two degrees a step, ringing on.
- **T4 FM6 MOKUGYO**, **POLY** with rhythms /3, /4 and /5.
- **MODS**: T1 TIDE 3 → T1 Q PROB (the koto thins and fills about once a minute), T3 SWARM 4 → T3 CUTOFF, T2 DRIFT →
  T2 PAN.

## Making them again

```
cc -O2 -w -Ibuild/gen -Ifirmware/src -DMELODEE_SID=0 -DMELODEE_PHYS=0 -DTRK_START_TESS=1 \
    -o build/host/demo_projects tests/demo_projects.c -lm      # (after ./build.sh: build/gen)
build/host/demo_projects make build/demos 180                  # the records, and 3 minutes of each as WAV
cp build/demos/*.tsp demos/
```

A change of what the generators play, or of the project format, can change the records: make them again and check
they still sound right (the WAVs) before committing.
