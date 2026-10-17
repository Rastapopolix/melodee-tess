/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* The note generators (Turing, Euclid, Markov, Morph, Grids, Bounce, Awake, Poly) behind one interface: each knows only its own state, the
 * clock and the input phrase, nothing of tracks, scales, the UI or Melodee. tessera.c is the adapter between them
 * and the sequencer (tessera-notes/docs/analysis/phase1-plan.md). Randomness: each draw from the track's own stream
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
    uint8_t  pulses;      /* Active hits (0..steps; EVOLVE keeps them 1..steps-1) */
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

/* 5. Grids (Mutable Instruments Grids' drum map, gen_grids.h): kick, snare and hat from a point on a 5 x 5 map of
 * patterns, blended between the four nearest */
typedef struct {
    uint8_t  x, y;        /* 0..255: the point on the map */
    uint8_t  dens;        /* 0..100: how full the three parts are (Grids' FILL, one for all three) */
    uint8_t  chaos;       /* 0..100: random hits added each bar (Grids' CHAOS; LOCK turned round) */
    uint8_t  step_idx;    /* the next step, 0..31 */
    uint8_t  perturb[3];  /* this bar's random push per part, 0..63 (drawn on the bar's first step) */
} gen_grids_t;

/* 6. Bounce (Theo's): balls crossing a 256 x 256 box a fixed distance a step, no gravity; a wall a ball hits plays a
 * note (which wall, and where along it). BALLS 1..4: each one more from a place and an angle of its own (the seed's) */
#define GEN_BOUNCE_MAX 4u
typedef struct {
    uint8_t  speed;       /* 1..64: how far a ball moves a step */
    uint8_t  angle;       /* 0..90: degrees from the floor (ball 1's; the others' offset from it) */
    uint8_t  balls;       /* 1..4 */
    uint8_t  x[GEN_BOUNCE_MAX], y[GEN_BOUNCE_MAX];   /* the balls, 0..255 (y 0: the floor) */
    uint8_t  dir;         /* 2 bits a ball: bit 0 moving left, bit 1 moving down */
    uint8_t  hit;         /* the walls hit on the last step, a bit each: 0 floor, 1 left, 2 right, 3 top */
    int8_t   off[GEN_BOUNCE_MAX];   /* each ball's angle from ANGLE (ball 1: 0) */
} gen_bounce_t;

/* 7. Awake (norns, tehn): two looping sequences of their own lengths: A (upper) plays, its value a scale degree (0 a
 * rest); B (lower) transposes A by its value. PROB the chance a step plays. The values come from the seed (ROLL) and
 * the step keys (GEN's step entry, ui_awake.c) */
typedef struct {
    uint8_t  len_a, len_b; /* 1..16 */
    uint8_t  prob;        /* 0..100 */
    uint8_t  pos_a, pos_b; /* the steps playing */
    uint8_t  a[8], b[8];  /* 16 values each, 4 bits (low nibble first): A 0..8, B 0..7 */
    uint8_t  edit;        /* the step the keys picked (GEN's KNOB 1 sets its A, KNOB 2 its B): 0 none, else 0x20 | step */
} gen_awake_t;

/* 8. Poly (EuroPi's Polyrhythmic Sequencer, after the Subharmonicon): four rhythms, each a clock divider, advance a
 * four-step pitch sequence; rhythms that land together play once, louder. The pitches come from the seed (ROLL) */
typedef struct {
    uint8_t  div[4];      /* 0 = off, 1..16: a rhythm advances the sequence every div steps */
    uint8_t  pitch[4];    /* scale degrees, 0..20 */
    uint8_t  span;        /* 1..3: octaves the pitches spread over */
    uint8_t  pos;         /* the step playing, 0..3 */
    uint8_t  sel;         /* the rhythm GEN's DIV edits (the page's, not saved) */
    uint32_t tick;        /* steps since PLAY, modulo 720720 (every divider's multiple) */
} gen_poly_t;

/* A FORM section's settings for the generator that made it (tessera.h tessera_sec_t keeps them and the phrase).
 * Each generator saves and loads only its own fields; the others keep what they held */
typedef struct {
    uint16_t reg;         /* Turing: base register; Euclid: pitch seed; Morph: snapshot A; Grids: x, y << 8; Bounce:
                           * balls; Poly: the pitches' low 4 bits each */
    uint8_t  p0, p1;      /* Euclid: pulses, rot; Markov: tension; Morph: morph_pos; Bounce: angle, speed; Awake: the
                           * lengths; Poly: span, the pitches' fifth bits from bit 4 */
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
    uint8_t parts;          /* Grids: the drum parts that hit, bit 0 kick, 1 snare, 2 hat (gate: any); else 0 */
    uint8_t accent;         /* Grids: the parts among them accented */
    uint8_t more[2];        /* Bounce: notes after `note` on the same step (raw, as note), more_n of them */
    uint8_t more_n;
} gen_out_t;

#endif
