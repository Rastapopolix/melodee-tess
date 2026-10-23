# Tessera 2.0 demo projects

Two projects that show what Tessera 2.0 does, each a few minutes of music that keeps changing on its own. Press
PLAY and leave it, then open the pages named below to see what is moving and turn things.

| File | Name on the FM-1 | What it shows |
|---|---|---|
| `still-tide.tsp` | STILL TIDE | Ambient: CHORD DRONE with BREATH, ENERGY's ARC and RESTS, FM6 and TRIO pads, FALL |
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

   That puts STILL TIDE and KOTO GARDEN into slots **C** and **D** and lists what each slot held. `--slots D` puts
   only STILL TIDE into D; `--slots A,B` picks other slots; `--slots D demos/koto-garden.tsp` picks the demo.
3. **Complete backup** > **Restore backup**, and pick `my-backup-with-demos.json`. Then switch the FM-1 off and on.
4. SAVE > PROJECT: pick the slot and load it, then PLAY.

The slots you choose are replaced: save anything you want to keep in them first (or choose other slots). To go back,
restore the backup from step 1.

## STILL TIDE (slot C)

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

## The demo video

The video (about 3 minutes, 1080 x 1080) is made from the firmware's own sound and screens: both demos as `make`
plays them (their first 20 s are the same samples as the WAVs above), then a short tour of the modes the demos leave
out (TURING, EUCLID, MORPH, ATLAS, CONSTELLATION, ANSWER).

```
./build.sh
cc -O2 -w -Ibuild/gen -Ifirmware/src -Itests -DMELODEE_SID=0 -DMELODEE_PHYS=0 -DTRK_START_TESS=1 \
   -o build/dp tests/demo_projects.c -lm
build/dp video build/video                      # each clip's WAV, frames and captions (about 900 MB of frames)
python3 tools/demo_video.py build/video build/tessera-2.0-demo.mp4   # needs Pillow and imageio-ffmpeg (or ffmpeg)
```

The pages shown and the captions are the timelines in `tests/demo_projects.c` (V_STILL, V_GARDEN, the tour).
