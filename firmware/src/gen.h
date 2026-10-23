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
 * patterns, blended between the four nearest; TOMS a fourth part, the kick's and the snare's near misses, on a sound
 * of its own (PART4: a tom, the clap, the open hat, the rim, the bell) (OCT+ held on GEN: KICK, SNR, HAT, TOMS; a
 * black key its sound) */
#define GEN_GRIDS_PARTS 4u   /* kick, snare, hat, the fourth */
#define GEN_GRIDS_TOM 3u     /* (the part TOMS plays: no pattern of its own on the map) */
#define GEN_GRIDS_P4_N 5u    /* the fourth part's sounds (gen.c GEN_GRIDS_P4_NOTE) */
typedef struct {
    uint8_t  x, y;        /* 0..255: the point on the map */
    uint8_t  dens;        /* 0..100: how full the parts are (Grids' FILL, one for all) */
    uint8_t  chaos;       /* 0..100: random hits added each bar (Grids' CHAOS; LOCK turned round) */
    uint8_t  step_idx;    /* the next step, 0..31 */
    uint8_t  perturb[3];  /* this bar's random push per part, 0..63 (drawn on the bar's first step) */
    uint8_t  thin[3];     /* 0..100: KICK, SNR, HAT turned round: how much of DENS the part loses (0: all of it, as
                           * before; the knobs show 100 - thin) */
    uint8_t  toms;        /* 0..100: how far under the line a kick or snare may miss and still play the fourth; 0 none */
    uint8_t  part4;       /* the fourth part's sound, 0..GEN_GRIDS_P4_N - 1: TOM, CLAP, HATOP, RIM, BELL */
} gen_grids_t;

/* 6. Bounce (Theo's): balls crossing a 256 x 256 box a fixed distance a step, no gravity; a wall a ball hits plays a
 * note (which wall, and where along it). BALLS 1..4: each one more from a place and an angle of its own (the seed's).
 * BALLS past 4 is FALL (after norns' Fall, ambalek): eight leaves fall at speeds of their own, swaying; a leaf landing
 * plays a note from where it lands, rests, fades and falls again from the top. The WIND (ANGLE turned round its middle)
 * blows them towards one side, so the melody leans up or down, and in gusts that hurry them down together */
#define GEN_BOUNCE_MAX 4u
#define GEN_BOUNCE_FALL 5u    /* BALLS: FALL */
#define GEN_FALL_LEAVES 8u
typedef struct {
    uint8_t  speed;       /* 1..64: how far a ball moves a step */
    uint8_t  angle;       /* 0..90: degrees from the floor (ball 1's; the others' offset from it) */
    uint8_t  balls;       /* 1..4 */
    uint8_t  x[GEN_BOUNCE_MAX], y[GEN_BOUNCE_MAX];   /* the balls, 0..255 (y 0: the floor) */
    uint8_t  dir;         /* 2 bits a ball: bit 0 moving left, bit 1 moving down */
    uint8_t  hit;         /* the walls hit on the last step, a bit each: 0 floor, 1 left, 2 right, 3 top */
    int8_t   off[GEN_BOUNCE_MAX];   /* each ball's angle from ANGLE (ball 1: 0) */
    uint8_t  lx[GEN_FALL_LEAVES], ly[GEN_FALL_LEAVES];   /* FALL: the leaves, 0..255 (ly 0: landed) */
    uint8_t  lv[GEN_FALL_LEAVES];   /* FALL: a leaf's speed, 4..19; bit 7 a bass leaf (an octave down) */
    uint8_t  lt[GEN_FALL_LEAVES];   /* FALL: the steps a landed leaf still rests (0 falling) */
    uint8_t  gust;        /* FALL: the steps a gust still blows (0 none) */
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

/* 9. Quad (initenere, a norns script): a 4 x 4 grid of notes the four tracks share, a row each.
 * A track in QUAD plays its own row on its own clock (its division), so the polyrhythm comes from the tracks' speeds;
 * LINK: its steps also move other QUAD tracks on (tessera.c tess_quad_link). RANGE folds the row's notes into fewer
 * octaves. The columns can move too, each on its own: its cells turn down or up the four rows, or jump, every so many
 * steps (tessera.c tess_quad_cols; OCT+ held on GEN). The cells come from the seed (ROLL); EVOLVE turns the grid's
 * columns between the QUAD rows while no column moves, else changes notes (tessera.c tess_quad_mutate) */
enum { GEN_QD_FWD, GEN_QD_BACK, GEN_QD_PEND, GEN_QD_RAND, GEN_QD_HOLD, GEN_QD_COUNT };
enum { GEN_QR_FULL, GEN_QR_ONE, GEN_QR_TWO, GEN_QR_LOW, GEN_QR_COUNT };   /* RANGE: 3 octaves, 1, 2, 1 either side */
enum { GEN_QC_STOP, GEN_QC_DOWN, GEN_QC_UP, GEN_QC_RAND, GEN_QC_COUNT };  /* a column's MOVE */
#define GEN_QC_EVERY_N 7u     /* a column's EVERY: 1, 2, 3, 4, 6, 8, 16 steps (gen.c GEN_QC_EVERY) */
typedef struct {
    uint8_t  cell[4];     /* this track's row: 0 a rest, 1..21 a scale degree + 1 (three octaves) */
    uint8_t  dir;         /* GEN_QD_*: forward, back, back and forth, random, held (only a LINK moves it) */
    uint8_t  prob;        /* 0..100: the chance a cell plays */
    uint8_t  link;        /* the tracks this one's steps also move on, a bit each (bit 0 track 1; its own: none) */
    uint8_t  pos;         /* the cell playing, 0..3; 4: none yet (the first step plays cell 1, or 4 going back) */
    uint8_t  back;        /* back and forth: 1 going back (not saved) */
    uint8_t  range;       /* GEN_QR_*: the octaves the row's notes fold into */
} gen_quad_t;

/* 10. Constellation (after norns' Constellation, timothy-taylor: the idea only, no code): stars scroll across a sky, a
 * column a step (the track's DIV its speed), each column's star (or none) worked out from the sky's seed (ROLL): its
 * height a scale degree, its brightness a velocity, its size the note's length. A star crossing the ship's crosshair at
 * height Y, within SIZE rows of it, plays as it crosses; the loop of 16 (the phrase loop's position) keeps what each step
 * caught (LEN slots: the phrase position modulo LEN). LOCK is the chance a step plays the loop it holds instead of the
 * sky (100: the loop frozen). FILL ARP: a step with nothing to play plays the last few stars played instead, low to high
 * in turn, softer and at the GATE.
 * AUTO steers the ship: a wander, faster further up; at the top SEEK (towards the next star) */
#define GEN_CS_ROWS 15u       /* heights 0..14: two octaves of the scale and one more degree */
#define GEN_CS_SEEK 10u       /* AUTO: SEEK */
#define GEN_CS_AHEAD 6u       /* SEEK looks this many columns ahead */
#define GEN_CS_ARP 4u         /* FILL ARP: the stars it cycles through */
enum { GEN_CS_FILL_OFF, GEN_CS_FILL_ARP, GEN_CS_FILL_COUNT };
typedef struct {
    uint8_t  y;           /* 0..14: the crosshair's height (AUTO OFF) */
    uint8_t  dens;        /* 0..100: the chance a column has a star */
    uint8_t  size;        /* 0..7: the rows either side of the crosshair a star is caught in */
    uint8_t  autop;       /* 0 OFF, 1..9 a wander (faster up), 10 SEEK */
    uint8_t  keep;        /* LOCK as the adapter hands it in, 0..100 (not saved here: EVOLVE's) */
    uint8_t  caught;      /* the last step's catch: 0 none, else its loop slot + 1 (the picture's blink) */
    uint8_t  held;        /* the last step played the loop (LOCK), not the sky: its slot + 1, else 0 */
    uint8_t  trail;       /* the stars that sounded crossing, a bit a column: bit 0 the last step's (the picture's lines) */
    int8_t   drift;       /* AUTO's wander: 1/16 rows a step */
    uint16_t sky;         /* the sky's seed */
    uint16_t col;         /* the sky's column at the crosshair */
    uint16_t ship;        /* the ship's height, rows x 256 */
    uint8_t  loop[16];    /* 0 empty, else bits 0..3 the height + 1, 4..5 the brightness, 6..7 the size */
    uint8_t  len;         /* LEN 1..16: the loop's slots (the phrase position modulo LEN) */
    uint8_t  fill;        /* GEN_CS_FILL_*: what a step with no star plays */
    uint8_t  arp[GEN_CS_ARP];   /* FILL ARP: the last stars played, newest first (a loop slot's value; not saved) */
    uint8_t  arp_n, arp_i;      /* how many, and the next to play (not saved) */
    uint8_t  filled;      /* the last step's FILL note (a loop slot's value), 0 none (the picture's) */
} gen_constel_t;

/* 11. Series (after norns' nørgård and Per Nørgård's infinity series: the idea only, no code): the melody that is the
 * same at every scale, worked out from the bits of the step's number. START picks where in the series it begins, INTV
 * how many degrees of the scale its unit is, LOOP how many steps it plays before it begins again (0: on and on) */
#define GEN_SR_LOOP 16u
typedef struct {
    uint8_t  start;       /* 0..127: the series' first number */
    uint8_t  intv;        /* 1..7: degrees of the scale a step of the series */
    uint8_t  loop;        /* 0 on and on, 1..16 steps */
    uint8_t  pad;
    uint16_t n;           /* the steps played since PLAY */
} gen_series_t;

/* 12. Chord pad: one chord at a time on a degree of the track's scale, from a progression, BARS long (tessera.c builds
 * its tones with the track's SCL CHORD: CHRD, VOIC). HIT: held for the chord, struck every beat, or every step. WALK is
 * the seed's own four chords (ROLL a new walk, EVOLVE changes one); USER up to eight played in on the keys (OCT+ held
 * on GEN: tessera_chord_user_key); the others are fixed progressions */
enum { GEN_CH_POP, GEN_CH_50S, GEN_CH_251, GEN_CH_CIRCLE, GEN_CH_TWO, GEN_CH_DRONE, GEN_CH_WALK, GEN_CH_USER, GEN_CH_PROGS };
enum { GEN_CH_HOLD, GEN_CH_BEAT, GEN_CH_STEP, GEN_CH_HITS };
#define GEN_CH_BARS 4u     /* BARS 0..3: 1/2, 1, 2, 4 bars a chord */
#define GEN_CH_WALK_N 4u
#define GEN_CH_USER_N 8u   /* USER's chords at most */
typedef struct {
    uint8_t  prog;        /* GEN_CH_*: the progression */
    uint8_t  bars;        /* 0..3: 1/2, 1, 2, 4 bars a chord */
    uint8_t  hit;         /* GEN_CH_HOLD, _BEAT, _STEP */
    uint8_t  rot;         /* 0..7: the progression begins this many chords in (EVOLVE turns it) */
    int8_t   oct;         /* -2..2: octaves from the chords' home (around C3; tessera.c, not a section's) */
    uint8_t  walk[GEN_CH_WALK_N];   /* WALK's degrees, 0..6 */
    uint8_t  deg;         /* the degree of the chord playing (the picture's) */
    uint8_t  idx;         /* its place in the progression (the picture's) */
    uint16_t hold;        /* the last strike's length in steps, 0: the step's GATE (tessera.c) */
    uint16_t n;           /* the steps played since PLAY or the section's start */
    uint8_t  user[GEN_CH_USER_N];   /* USER's degrees, 0..6 */
    uint8_t  ulen;        /* USER's chords, 1..8 (0: one, the tonic) */
} gen_chord_t;

/* A FORM section's settings for the generator that made it (tessera.h tessera_sec_t keeps them and the phrase).
 * Each generator saves and loads only its own fields; the others keep what they held */
typedef struct {
    uint16_t reg;         /* Turing: base register; Euclid: pitch seed; Morph: snapshot A; Grids: x, y << 8; Bounce:
                           * balls; Poly: the pitches' low 4 bits each; Quad: cells 1..3, 5 bits each; Constellation: the sky;
                           * Series: START; Chord: WALK, 3 bits a chord, | ROT << 12 */
    uint8_t  p0, p1;      /* Euclid: pulses, rot; Markov: tension; Morph: morph_pos; Bounce: angle, speed; Awake: the
                           * lengths; Poly: span, the pitches' fifth bits from bit 4; Quad: cell 4; Constellation: Y | SIZE << 4,
                           * DENS; Series: INTV | LOOP << 3; Chord: PROG | BARS << 3 | HIT << 5 */
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
    uint8_t parts;          /* Grids: the drum parts that hit, bit 0 kick, 1 snare, 2 hat, 3 tom (gate: any); else 0 */
    uint8_t accent;         /* Grids: the parts among them accented */
    uint8_t more[2];        /* Bounce: notes after `note` on the same step (raw, as note), more_n of them */
    uint8_t more_n;
    uint8_t len;            /* the note's length, quarters of the step's GATE (Constellation: a star's size); 0 = GATE */
} gen_out_t;

#endif
