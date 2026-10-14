/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* The note generators (Turing, Euclid, Markov, Morph) behind one interface: each knows only its own state, the
 * clock and the input phrase, nothing of tracks, scales, the UI or Melodee. tessera.c is the adapter between them
 * and the sequencer (docs/analysis/phase1-plan.md). Randomness: each draw from the track's own stream
 * (gen.c gen_rnd), handed in by the adapter, which starts it from the track's SEED. */
#ifndef GEN_H
#define GEN_H

#include <stdint.h>

/* 1. Turing machine */
typedef struct {
    uint16_t reg;         /* 16-bit shift register */
    uint8_t  len;         /* Active cycle length: 2..16 */
    uint8_t  prob;        /* Mutation probability: 0 = locked, 100 = full random */
    uint8_t  step_idx;    /* Playhead position within active length (0..len-1) */
    uint8_t  scale_deg;   /* Current extracted scale degree */
    uint8_t  oct_span;    /* Octave spread (1..3) */
} gen_turing_t;

/* 2. Euclidean rhythm */
typedef struct {
    uint8_t  steps;       /* Total steps (1..32) */
    uint8_t  pulses;      /* Active hits (0..steps) */
    uint8_t  rot;         /* Step rotation offset */
    uint8_t  step_idx;    /* Current step index */
    uint8_t  last_hit;    /* 1 if current step fired */
    uint8_t  pitch_seed;  /* 0 = the original fixed pitch pattern; 1..255 = a random one (ROLL) */
} gen_euclid_t;

/* 3. Markov pitch */
typedef struct {
    uint8_t  cur_state;   /* 0..7 active scale degree */
    uint8_t  last_hit;    /* 1 if the current step sounded, 0 if it was a rest */
    uint8_t  tension;     /* 0 = tight/stepwise, 100 = wild leaps */
    uint8_t  density;     /* 0..100 chance a step plays (0 = 100) */
    uint8_t  oct_span;    /* 1..3 octave spread */
    /* matrix[8][8] / notes[8] removed: the row is derived from tension (gen_markov_row) */
} gen_markov_t;

/* 4. Morph */
typedef struct {
    uint8_t  morph_pos;   /* 0..127 crossfade between Snapshot A and B */
    uint16_t snap_a;      /* Snapshot A rhythm bitmask (16 steps) */
    uint16_t snap_b;      /* Snapshot B rhythm bitmask (16 steps) */
    uint8_t  step_idx;    /* Playhead index (0..15) */
    uint8_t  oct_span;    /* Octave span */
} gen_morph_t;

/* A FORM section's settings for the generator that made it (tessera.h tessera_sec_t keeps them and the phrase).
 * Each generator saves and loads only its own fields; the others keep what they held */
typedef struct {
    uint16_t reg;         /* Turing: base register; Euclid: pitch seed; Morph: snapshot A */
    uint8_t  p0, p1;      /* Euclid: pulses, rot; Markov: tension; Morph: morph_pos */
} gen_sec_t;

/* One step of an input phrase (phase 6: STEP as seed) */
typedef struct { uint8_t hit, note, vel; } gen_step_t;
typedef struct { const gen_step_t *step; uint8_t len; } gen_phrase_t;   /* len 0: empty (phase 1: always) */

/* What a generator may know of the clock and the track */
typedef struct {
    uint8_t pos;            /* the step's position in the phrase loop (tessera.c tess_loop_len) */
    uint8_t steps_per_bar;  /* from the track's division (tessera.c tess_steps_per_bar) */
    uint8_t root;           /* the track's key, 0..11 (Markov) */
} gen_clock_t;

/* One step's result */
typedef struct {
    uint8_t gate;           /* 1 = a note starts on this step */
    uint8_t note, vel;      /* the raw note (before LOCK, the scale snap, transpose, SHIFT) */
    int16_t cv;             /* the generator's CV for the signal bus (phase 2; 0 until then) */
} gen_out_t;

#endif
