// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Theo Dainis (melodee-tess)
// MACRO's Plaits voice (firmware/third_party/mi, MIT): built as C++ apart from melodee.c (build.py), no C++ library
// (no exceptions, no RTTI, no heap: the voice lives in the memory eng_macro.c gives it, the audio arena).
#include <stddef.h>
#include "plaits/dsp/voice.h"
#include "macro_glue.h"

#ifdef MACRO_HOST
#include <new>                                              // (host tests: the C++ library's placement new)
#else
inline void *operator new(size_t, void *p) { return p; }   // (placement new: <new> is not there)
#endif
extern "C" void __cxa_pure_virtual(void) { for (;;) {} }

// the one libm function Plaits calls (the firmware links no libm): a first guess by the exponent's half, then
// Newton's steps (four: full float precision)
extern "C" float sqrtf(float x)
{
    union { float f; uint32_t u; } g;
    if (!(x > 0.0f)) return 0.0f;
    g.f = x;
    g.u = (g.u >> 1) + 0x1FC00000u;
    for (int i = 0; i < 4; i++) g.f = 0.5f * (g.f + x / g.f);
    return g.f;
}

namespace {
struct mg_voice {
    plaits::Voice voice;
    char buffer[16384];                              // Plaits' working memory (its strings, modal, reverbs)
};
}

extern "C" uint32_t mg_size(void) { return sizeof(mg_voice); }

extern "C" void mg_init(void *mem)
{
    mg_voice *v = new (mem) mg_voice;
    stmlib::BufferAllocator a(v->buffer, sizeof v->buffer);
    v->voice.Init(&a);
}

extern "C" void mg_render(void *mem, const mg_params_t *q, int16_t *out, int16_t *aux)
{
    mg_voice *v = static_cast<mg_voice *>(mem);
    plaits::Patch p = {};
    plaits::Modulations m = {};
    plaits::Voice::Frame f[MG_BLOCK];
    p.engine = q->model;
    p.note = q->note;
    p.harmonics = q->harmonics;
    p.timbre = q->timbre;
    p.morph = q->morph;
    p.decay = q->decay;
    p.lpg_colour = q->lpg_colour;
    m.trigger_patched = q->gated != 0;
    m.trigger = q->trigger ? 1.0f : 0.0f;
    v->voice.Render(p, m, f, MG_BLOCK);
    for (uint32_t i = 0; i < MG_BLOCK; i++) {
        out[i] = f[i].out;
        aux[i] = f[i].aux;
    }
}
