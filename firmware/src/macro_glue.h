/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* MACRO's C side of Plaits (macro_glue.cc, firmware/third_party/mi): one voice in caller-owned memory */
#ifndef MACRO_GLUE_H
#define MACRO_GLUE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    int32_t model;                  /* Plaits' engine index (eng_macro.c MC_MODEL: the lean set's) */
    float note;                     /* MIDI note, fractional */
    float harmonics, timbre, morph; /* 0..1 */
    float decay, lpg_colour;        /* 0..1: the low-pass gate's */
    uint8_t trigger;                /* 1 for the block a note starts */
    uint8_t gated;                  /* 1: Plaits' own LPG shapes it (struck); 0: it drones, the FM-1's ADSR shapes it */
} mg_params_t;
#define MG_BLOCK 16u                /* samples a render (PLAITS_BLOCK_SIZE) */
uint32_t mg_size(void);             /* bytes of a voice (the Voice and its 16 KB working buffer) */
void mg_init(void *mem);            /* mem: mg_size() bytes, 4-aligned */
void mg_render(void *mem, const mg_params_t *p, int16_t *out, int16_t *aux);   /* MG_BLOCK samples */
#ifdef __cplusplus
}
#endif
#endif
