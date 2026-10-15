/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* Cords (phase 2, the signal bus): up to CORD_MAX connections, each from a source on one track to a destination on
 * another (or the same), with an amount and an offset. They extend Melodee's per-track MOD matrix (mod.c), whose four
 * routes stay as they are. This header: the limits and the destinations a cord can reach (cord.c CORD_DESTS). */
#ifndef CORD_H
#define CORD_H
#include <stdint.h>

#define CORD_MAX 32u              /* the fixed maximum: the saved format and the worst-case cost are sized for it */
#define CORD_LIVE 16u             /* how many are in use now (Theo 2026-10-16); raising it changes no format */

/* How a destination is moved, and when (each one checked against where the firmware reads it):
 *   CK_VOICE  per voice, in mod.c mod_voice: the voice's pitch, cutoff, shape, gain (mod.c's MD_PITCH .. MD_AMP)
 *   CK_BLOCK  a track parameter, modulated in place for the part's block (mod.c mod_begin .. mod_end); only those
 *             read inside it: mix_part after mod_begin, track_render_audio, env_tick, track_lfo_tick, the FX send
 *   CK_DEPTH  the track LFO's depth (mod.c's MD_DEPTH: a gain on the LFO wherever it goes)
 *   CK_STEP   a Tessera setting, in place around the track's generator step (tessera_clock_step) */
enum { CK_VOICE, CK_BLOCK, CK_DEPTH, CK_STEP };

/* Destination codes. FROZEN: saved cords keep them; new ones at the end, a retired one stays as a gap */
enum {
    CD_OFF,
    CD_PITCH, CD_CUTOFF, CD_SHAPE, CD_GAIN,                         /* per voice */
    CD_LEVEL, CD_PAN, CD_SPREAD,
    CD_ATTACK, CD_DECAY, CD_SUSTAIN, CD_RELEASE,
    CD_ENV_FLT, CD_ENV_PIT, CD_ENV_SHP,
    CD_LFO_RATE, CD_LFO_DEPTH, CD_LFO_PIT, CD_LFO_FLT, CD_LFO_SHP, CD_LFO_AMP,
    CD_FX_SEND,
    CD_E1, CD_E2, CD_E3, CD_E4, CD_E5, CD_E6, CD_E7, CD_E8,         /* the track engine's own eight */
    CD_TUR_PROB, CD_TUR_LEN,                                        /* Tessera, at the generator's step */
    CD_EUC_PULSES, CD_EUC_ROT,
    CD_MKV_LEAP, CD_MKV_DENS,
    CD_MRP_POS,
    CD_TW_SHIFT, CD_TW_DROP, CD_TW_HUMAN,
    CD_EV_AMOUNT, CD_FM_CONTRAST,
    CD_N
};

typedef struct {
    uint8_t code, kind;
    uint8_t md;                   /* CK_VOICE / CK_DEPTH: mod.c's MD_* it shares; else 0 */
    uint16_t pid;                 /* CK_BLOCK / CK_STEP: the parameter (pid.h); else PID_NONE */
    const char *name;             /* as a cord shows it: <= 7 characters */
    const char *full;             /* for the help and the reference */
} cord_dest_t;

const cord_dest_t *cord_dest(uint32_t code);        /* 0: no such destination (a gap, past the end) */

#endif
