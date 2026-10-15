/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
#ifndef TESSERA_H
#define TESSERA_H

#include <stdint.h>
#include "gen.h"

#define TESSERA_TRACKS 4

/* Engine types per track */
enum {
    TESS_ENG_OFF = 0,     /* Track native step sequencer */
    TESS_ENG_TURING,      /* 16-bit shift register */
    TESS_ENG_EUCLID,      /* Euclidean rhythm generator */
    TESS_ENG_MARKOV,      /* 1st-order Markov pitch matrix */
    TESS_ENG_MORPH,       /* Vector/Morph interpolation */
    TESS_ENG_COUNT
};

/* The generators' own state lives in gen.h (the names below kept for the adapter and the UI) */
typedef gen_turing_t tessera_turing_t;
typedef gen_euclid_t tessera_euclid_t;
typedef gen_markov_t tessera_markov_t;
typedef gen_morph_t  tessera_morph_t;

/* 5. Evolve (per track, engine-independent): phrase-level mutation.
 * LOCK has no storage of its own for Turing: it is 100 - turing.prob. */
typedef struct {
    uint8_t  every;       /* 0 = OFF, 1..5 -> 1/2/4/8/16 bars between mutations */
    uint8_t  amount;      /* 0..100: how much changes at each mutation */
    uint8_t  bar_cnt;     /* bars elapsed since the last mutation */
    uint16_t step_cnt;    /* steps elapsed inside the current bar */
    uint8_t  flash;       /* steps left to show the "just mutated" flash */
    uint8_t  lock;        /* 0..100 deja vu for Markov / Morph (Turing: 100 - prob; Euclid: n/a) */
    uint8_t  lp_pos;      /* next position in the phrase loop */
    uint8_t  pos_now;     /* position of the step being played */
    uint8_t  rec;         /* 2 = armed, 1 = recording this pass, 0 = replaying (Turing: 2 = capture base) */
    uint16_t base_reg;    /* Turing: the register captured at the start of the phrase */
    uint8_t  form;        /* 0 = OFF, 1..6 = AB ABAC ABCB AABA ABCA AABC (see TESS_FORMS) */
    uint8_t  contrast;    /* 0..100: how different a new section is from the one before it */
    uint8_t  sec;         /* current section: 0 = A, 1 = B, 2 = C */
    uint8_t  form_pos;    /* position inside the form */
    uint8_t  visited;     /* bit s: section s exists */
    uint8_t  valid;       /* bit s: section s's recorded phrase is complete (Markov / Morph) */
    int8_t   shift;       /* TWEAK: semitones added to the track's notes; while following, scale degrees from the
                           * followed note (+2 a third, +4 a fifth, -7 an octave down in a seven-note scale) */
    uint8_t  drop;        /* TWEAK: 0..100 chance a hit is dropped, any engine */
    uint8_t  human;       /* TWEAK: 0..100 velocity and gate variation */
    uint8_t  follow;      /* TWEAK: 0 = off, 1..4 = play that track's pitch (a generator's or a STEP track's sounding
                           * note), SHIFT scale degrees away, in this track's scale */
    int8_t   flourish;    /* FORM: semitones added on the last beat of every phrase (EVERY bars), 0 = off */
} tessera_evolve_t;

/* One remembered section (A, B or C) of a track's form */
typedef struct {
    uint16_t reg;         /* Turing: base register; Morph: snapshot A */
    uint8_t  p0, p1;      /* Euclid: pulses, rot; Markov: tension; Morph: morph_pos */
    uint8_t  note[32];    /* Markov / Morph phrase: bit 7 = hit, bits 0..6 = note */
    uint8_t  vel[16];     /* 4 bits per step */
} tessera_sec_t;

/* Master Tessera Engine State */
typedef struct {
    uint8_t          mode[TESSERA_TRACKS];
    tessera_turing_t turing[TESSERA_TRACKS];
    tessera_euclid_t euclid[TESSERA_TRACKS];
    tessera_markov_t markov[TESSERA_TRACKS];
    tessera_morph_t  morph[TESSERA_TRACKS];
    tessera_evolve_t evolve[TESSERA_TRACKS];
    tessera_sec_t    sec[TESSERA_TRACKS][3];
    uint8_t          last_note[TESSERA_TRACKS];
    uint8_t          follow_mem[TESSERA_TRACKS];   /* FOLLOW of a STEP track: its last sounding note + 1, 0 none yet */
    uint8_t          active_gate[TESSERA_TRACKS];
    uint16_t         seed[TESSERA_TRACKS];    /* SEED 1..9999, 0 = not picked yet (tessera.c tess_seed_fresh) */
    uint32_t         rnd[TESSERA_TRACKS];     /* each track's random stream, from its seed (gen.c gen_stream) */
    uint16_t         roll[TESSERA_TRACKS];    /* ROLL turned while playing: the seed for the next bar line, 0 none
                                               * (tessera.c tessera_seed_turn; not saved) */
} tessera_state_t;

extern tessera_state_t tessera;

void tessera_init(void);
void tessera_clock_step(track_t *t);
void tessera_markov_recalc(uint32_t tr);
void tessera_knob(uint32_t k, int32_t s);
void tessera_ensure_init(void);
int32_t tessera_snap_note(const track_t *t, int32_t n);
uint32_t tessera_euclid_pitch(const track_t *t, uint32_t step);
uint32_t tessera_sig(uint32_t tr);
void tessera_kill(track_t *t);
void tessera_mutate(uint32_t tr);
void tessera_seed_turn(uint32_t tr, int32_t s);    /* ROLL (EVOLVE page): the next seed, on the bar line */
void tessera_evolve_knob(uint32_t k, int32_t s);
uint32_t tessera_every_bars(uint32_t every);
int  tessera_lock_get(uint32_t tr);            /* 0..100, or -1 when LOCK is not available for the mode */
void tessera_phrase(uint32_t tr, uint32_t *bar, uint32_t *of, int *flash); /* of == 0: EVERY is OFF */
void tessera_stop_all(void);
void tessera_tweak_knob(uint32_t k, int32_t s);
void tessera_form_knob(uint32_t k, int32_t s);
uint32_t tessera_form_get(uint32_t tr, uint8_t seq[4], uint32_t *pos);   /* returns the form's length, 0 = OFF */
const char *tessera_form_name(uint32_t form);
#define TESSERA_PK_BYTES 188u                       /* the saved state, version 2 (tessera.c: TESS_PK_TRACK x TESSERA_TRACKS) */
#define TESSERA_PK_BYTES_V1 180u                    /* version 1 (before SEED): older projects and backups */
uint32_t tessera_pack(uint8_t *out);                /* live state -> TESSERA_PK_BYTES */
int tessera_unpack(const uint8_t *in);              /* 1 loaded; 0 refused (range check), nothing changed */
int tessera_unpack_v1(const uint8_t *in);           /* the same for a version 1 block */
void tessera_v1_track(const uint8_t *in, uint8_t *out);   /* one track's record, version 1 -> 2 */
void tessera_reset(void);                           /* every track OFF, defaults */
/* the UI's view of a track's generators (read only; tr < TESSERA_TRACKS) */
const gen_turing_t *tessera_turing(uint32_t tr);
const gen_euclid_t *tessera_euclid(uint32_t tr);
const gen_markov_t *tessera_markov(uint32_t tr);
const gen_morph_t *tessera_morph(uint32_t tr);

#endif
