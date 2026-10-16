// SPDX-License-Identifier: GPL-3.0-only
// the whole Plaits voice (all 24 models, the LPG, the post-processing): Init and Render reached
#include "plaits/dsp/voice.h"
char shared_buffer[16384];
static plaits::Voice voice;
static plaits::Voice::Frame frames[plaits::kMaxBlockSize];
extern "C" void __cxa_pure_virtual() { for (;;) {} }
extern "C" { void *__dso_handle; int __cxa_atexit(void (*)(void *), void *, void *) { return 0; } }
extern "C" int main() {
  stmlib::BufferAllocator a(shared_buffer, sizeof shared_buffer);
  voice.Init(&a);
  plaits::Patch p = {}; plaits::Modulations m = {};
  for (int e = 0; e < 24; e++) { p.engine = e; voice.Render(p, m, frames, plaits::kMaxBlockSize); }
  return frames[0].out;
}
