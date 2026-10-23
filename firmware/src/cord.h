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
    CD_GRD_X, CD_GRD_Y, CD_GRD_DENS,
    CD_BNC_SPEED, CD_BNC_ANGLE, CD_AWK_PROB,
    CD_TW_BRANCH, CD_QD_PROB,
    CD_CS_Y, CD_CS_SIZE, CD_CS_DENS,
    CD_SR_START, CD_RETIRED_CL_RULE,                                /* (CELLS' RULE, cut: a gap, no destination) */
    CD_CS_AUTO,
    CD_CH_PROG,
    CD_GRD_KICK, CD_GRD_SNR, CD_GRD_HAT, CD_GRD_TOMS,
    CD_AN_VARY,
    CD_SUB_HUMAN,                 /* TIMING's HUMAN TIME */
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

/* Source codes. FROZEN as the destinations. A source is a track's (its LFO, its latest note ..): one value per
 * track and block, so a cord crosses tracks (on its own track's voice destinations, ENV VEL KEY RAND: each voice its
 * own, as MOD's routes). Values Q15: bipolar -32768..32767 (LFO, KEY, RAND, S&H, SLEW, NOTE, DRIFT, SLOTH, OCHD), else 0..32767
 * (CV too) */
enum {
    CS_OFF,
    CS_LFO, CS_ENV, CS_VEL, CS_KEY, CS_RAND, CS_MODW, CS_AT, CS_EXPR, CS_SH, CS_SLEW,   /* mod.c's MS_LFO .. MS_SLEW */
    CS_GATE,                      /* the track's generator: 1 while its note sounds (Tessera) */
    CS_NOTE,                      /* the generator's last note, as KEY (C4 = 0, an octave = 0.19) */
    CS_CV,                        /* the generator's CV (gen.c): Turing its register, Euclid and Morph a ramp, Markov the
                                   * degree, Grids the kick's chance, Bounce the ball's height, Awake A's value; 0..1 */
    CS_DRIFT,                     /* a slow wander of the track's own, a new point every 12 s or so */
    CS_TORPOR, CS_APATHY, CS_INERTIA,   /* the track's SLOTH (organic.c): chaos, ~17 min, ~1 min, ~3 min a turn */
    CS_OCHD1, CS_OCHD2, CS_OCHD3, CS_OCHD4, CS_OCHD5, CS_OCHD6, CS_OCHD7, CS_OCHD8,   /* its OCHD: 64 s .. 2.4 s */
    CS_ENERGY,                    /* the project's tide (ARC, energy.c), the same for every track: -1 sparse .. 1 full */
    CS_N
};

typedef struct {                  /* one cord, as saved (6 bytes: uni rides in src_trk's top bit) */
    uint8_t src, src_trk;         /* CS_*, 0..3 */
    uint8_t dst, dst_trk;         /* CD_*, 0..3 */
    int8_t amount;                /* -100..100 %: at a full source, the destination's whole range (pitch: an octave) */
    int8_t offset;                /* -100..100 %: added whatever the source (CS_OFF: a fixed setting) */
    uint8_t uni;                  /* 1: a bipolar source folded to 0..1 (UNI: moves one way from the knob); 0 BI */
} cord_t;

const char *cord_src_name(uint32_t code);           /* <= 7 characters; 0: no such source */
int cord_set(uint32_t i, const cord_t *c);          /* i < CORD_LIVE; out of range: refused (0), nothing changed */
const cord_t *cord_get(uint32_t i);
void cord_clear(void);                              /* every cord off */
uint32_t cord_count(void);                          /* cords doing something (a destination and an amount or offset) */
uint32_t cord_adopt(void);
void cord_tick(void);                               /* an audio block (mix.c mix_block): DRIFT's clock, SLOTH, OCHD */
void cord_defaults(void);                           /* a new project's mods (project.c): the default patch */
void cord_sound_load(uint32_t tr);                  /* a sound loads on track tr (ui.c load_begin) */                          /* MOD's routes into cords (main loop): the routes left, no room */

#endif
