// SPDX-License-Identifier: GPL-3.0-only
// Plaits on this computer: each model, one voice, ns per sample (48 kHz blocks of 12), note held, mid knobs
#include "plaits/dsp/voice.h"
#include <cstdio>
#include <ctime>
char shared_buffer[16384];
static plaits::Voice voice;
static plaits::Voice::Frame f[12];
static double now() { timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e9 + t.tv_nsec; }
int main() {
  stmlib::BufferAllocator a(shared_buffer, sizeof shared_buffer);
  voice.Init(&a);
  const char *N[24] = {"VA VCF", "PHASE DIST", "6-OP A", "6-OP B", "6-OP C", "WAVE TERRAIN", "STRING MACH", "CHIPTUNE",
                       "VA", "WAVESHAPE", "FM", "GRAIN", "ADDITIVE", "WAVETABLE", "CHORDS", "SPEECH",
                       "SWARM", "NOISE", "PARTICLE", "STRING", "MODAL", "BASS DRUM", "SNARE", "HI-HAT"};
  long sink = 0;
  for (int e = 0; e < 24; e++) {
    plaits::Patch p = {}; plaits::Modulations m = {};
    p.engine = e; p.note = 48; p.harmonics = 0.5f; p.timbre = 0.5f; p.morph = 0.5f; p.decay = 0.5f; p.lpg_colour = 0.5f;
    m.trigger_patched = true;
    for (int i = 0; i < 400; i++) { m.trigger = (i % 200) < 4 ? 1.0f : 0.0f; voice.Render(p, m, f, 12); }
    double t0 = now(); int n = 0;
    for (int i = 0; i < 40000; i++) { m.trigger = (i % 2000) < 4 ? 1.0f : 0.0f; voice.Render(p, m, f, 12); sink += f[0].out; n += 12; }
    printf("plaits %-13s %6.1f ns/sample\n", N[e], (now() - t0) / n);
  }
  return (int)(sink & 1);
}
