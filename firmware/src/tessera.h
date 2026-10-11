#ifndef TESSERA_H
#define TESSERA_H

#include <stdint.h>

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

/* 1. Turing Machine State */
typedef struct {
    uint16_t reg;         /* 16-bit shift register */
    uint8_t  len;         /* Active cycle length: 2..16 */
    uint8_t  prob;        /* Mutation probability: 0 = locked, 100 = full random */
    uint8_t  step_idx;    /* Playhead position within active length (0..len-1) */
    uint8_t  scale_deg;   /* Current extracted scale degree */
    uint8_t  oct_span;    /* Octave spread (1..3) */
} tessera_turing_t;

/* 2. Euclidean Generator State */
typedef struct {
    uint8_t  steps;       /* Total steps (1..32) */
    uint8_t  pulses;      /* Active hits (0..steps) */
    uint8_t  rot;         /* Step rotation offset */
    uint8_t  step_idx;    /* Current step index */
    uint8_t  last_hit;    /* 1 if current step fired */
} tessera_euclid_t;

/* 3. Markov Pitch State */
typedef struct {
    uint8_t  cur_state;   /* 0..7 active scale degree */
    uint8_t  tension;     /* 0 = tight/stepwise, 100 = wild leaps */
    uint8_t  density;  /* 0=Minor, 1=Major, 2=Dorian, 3=Penta */
    uint8_t  oct_span;    /* 1..3 octave spread */
    uint8_t  matrix[8][8];/* Normalized cumulative transition weights (0..100) */
    uint8_t  notes[8];    /* Pitch degree lookup */
} tessera_markov_t;

/* 4. Morph State */
typedef struct {
    uint8_t  morph_pos;   /* 0..127 crossfade between Snapshot A and B */
    uint16_t snap_a;      /* Snapshot A rhythm bitmask (16 steps) */
    uint16_t snap_b;      /* Snapshot B rhythm bitmask (16 steps) */
    uint8_t  step_idx;    /* Playhead index (0..15) */
    uint8_t  oct_span;    /* Octave span */
} tessera_morph_t;

/* Master Tessera Engine State */
typedef struct {
    uint8_t          mode[TESSERA_TRACKS];
    tessera_turing_t turing[TESSERA_TRACKS];
    tessera_euclid_t euclid[TESSERA_TRACKS];
    tessera_markov_t markov[TESSERA_TRACKS];
    tessera_morph_t  morph[TESSERA_TRACKS];
    uint8_t          last_note[TESSERA_TRACKS];
    uint8_t          active_gate[TESSERA_TRACKS];
} tessera_state_t;

extern tessera_state_t tessera;

void tessera_init(void);
void tessera_clock_step(track_t *t);
int  tessera_euclid_hit(uint8_t step, uint8_t steps, uint8_t pulses, uint8_t rot);
void tessera_markov_recalc(uint32_t tr);
void tessera_knob(uint32_t k, int32_t s);

#endif
