# Mutable Instruments code (MIT)

Plaits' synthesis (`plaits/dsp`, `plaits/resources.*`) and the parts of stmlib it uses, by Emilie Gillet, under
the MIT licence (`LICENSE`), compatible with this project's GPL-3.0. Melodee-tess's MACRO engine
(`firmware/src/eng_macro.c`, `firmware/src/macro_glue.cc`) is built on it. "Mutable Instruments" and "Plaits"
are Emilie Gillet's names; this is not her product and she does not support it.

Taken from https://github.com/pichenettes/eurorack at f4cfa76 (2026-10-12) and
https://github.com/pichenettes/stmlib at d18def8 (2023-09-03). The Python resource generators, the hardware
drivers, the UI and the bootloader are left out.

Changes (each marked `melodee-tess` in the code):
- `plaits/dsp/dsp.h`: the sample rate is `PLAITS_SAMPLE_RATE` (the FM-1's 44.1 kHz), 48 kHz by default.
- `plaits/dsp/fx/diffuser.h`: its LFO by that rate.
- `plaits/dsp/dsp.h`: `PLAITS_SAMPLE_RATE_EXACT` tunes by the true rate (Plaits' own corrects for its chip's
  47,872 Hz); `PLAITS_BLOCK_SIZE` (16 on the FM-1: two renders a block of 32).
- `plaits/dsp/voice.h`, `voice.cc`: `PLAITS_LEAN` leaves out the wavetable, chords, wave terrain, speech and
  6-OP models (their 85 KB of tables), and since 2026-10-17 also the string machine and chiptune (they play chords),
  the waveshaper, grains, swarm, noise, particles and the three drums; the seven left (VA-VCF, phase distortion, VA,
  FM, additive, string, modal) keep Plaits' own numbers.
