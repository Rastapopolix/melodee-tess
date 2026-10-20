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
    TESS_ENG_GRIDS,       /* Grids' drum map: kick, snare, hat */
    TESS_ENG_BOUNCE,      /* a ball in a box: a wall hit plays */
    TESS_ENG_AWAKE,       /* two loops added (norns' Awake) */
    TESS_ENG_POLY,        /* four rhythms advance four steps (EuroPi's Polyrhythmic Sequencer) */
    TESS_ENG_QUAD,        /* a row each of a 4 x 4 grid the tracks share (norns' initenere) */
    TESS_ENG_CONSTEL,     /* a ship catches the stars of a scrolling sky into a loop (norns' Constellation) */
    TESS_ENG_SERIES,      /* Nørgård's infinity series (norns' nørgård) */
    TESS_ENG_CHORD,       /* the chord pad: a progression's chords in the track's scale (QNT CHRD: the others follow) */
    TESS_ENG_COUNT
};
/* CELLS (an elementary automaton, cut 2026-10-20 as too like the others) had 12 in blocks up to version 11: such a
 * track loads as TURING (tess_v11_track); CHORD has 12 from version 12 */
#define TESS_ENG_RETIRED_CELLS 12u

/* The generators' own state lives in gen.h (the names below kept for the adapter and the UI) */
typedef gen_turing_t tessera_turing_t;
typedef gen_euclid_t tessera_euclid_t;
typedef gen_markov_t tessera_markov_t;
typedef gen_morph_t  tessera_morph_t;
typedef gen_grids_t  tessera_grids_t;
typedef gen_bounce_t tessera_bounce_t;
typedef gen_awake_t  tessera_awake_t;
typedef gen_poly_t   tessera_poly_t;
typedef gen_quad_t   tessera_quad_t;
typedef gen_constel_t tessera_constel_t;
typedef gen_series_t tessera_series_t;
typedef gen_chord_t  tessera_chord_t;

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
    uint8_t  branch;      /* TWEAK 2: 0..100 chance a hit plays on track br_to instead (MI Branches' coin) */
    uint8_t  br_to;       /* TWEAK 2: 0 = off, 1..4 = the track a branched hit plays on (never this one) */
    uint8_t  br_mode;     /* TWEAK 2: TESS_BR_COIN, _TOGGLE, _LATCH (see tessera.c tess_branch) */
    uint8_t  br_side;     /* 1 = hits go to br_to now (TOGGLE, LATCH; not saved) */
    uint8_t  logic;       /* TWEAK 2: TESS_LG_OFF, _AND, _OR, _XOR, _NOT against track lg_with's hits */
    uint8_t  lg_with;     /* TWEAK 2: 0 = none, 1..4 = the track LOGIC reads (never this one) */
    uint8_t  cond;        /* TWEAK 2: TESS_COND_OFF .. TESS_COND_NFIRST, the bars a hit may play in */
    uint8_t  cond_bar;    /* bars since PLAY or the section's start, mod 8 (COND; not saved) */
    uint8_t  cond_first;  /* 1 until that first bar ends (COND FIRST; not saved) */
} tessera_evolve_t;

enum { TESS_BR_COIN, TESS_BR_TOGGLE, TESS_BR_LATCH, TESS_BR_COUNT };
enum { TESS_LG_OFF, TESS_LG_AND, TESS_LG_OR, TESS_LG_XOR, TESS_LG_NOT, TESS_LG_COUNT };
enum { TESS_COND_OFF, TESS_COND_1_2, TESS_COND_2_2, TESS_COND_1_4, TESS_COND_2_4, TESS_COND_3_4, TESS_COND_4_4,
       TESS_COND_1_8, TESS_COND_8_8, TESS_COND_FIRST, TESS_COND_NFIRST, TESS_COND_COUNT };

/* One remembered section (A, B or C) of a track's form */
typedef struct {
    uint16_t reg;         /* Turing: base register; Morph: snapshot A; Grids: x, y << 8 */
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
    tessera_grids_t  grids[TESSERA_TRACKS];
    tessera_bounce_t bounce[TESSERA_TRACKS];
    tessera_awake_t  awake[TESSERA_TRACKS];
    tessera_poly_t   poly[TESSERA_TRACKS];
    tessera_quad_t   quad[TESSERA_TRACKS];    /* a row each of QUAD's grid (whatever the track's MODE) */
    uint8_t          qcol[4];                 /* QUAD's columns: MOVE (GEN_QC_*) | EVERY (0..6) << 2 (saved a column
                                               * a track record) */
    uint8_t          qcol_cnt[4];             /* the steps since a column last moved (not saved) */
    uint8_t          qcol_sel;                /* the column OCT+ held on GEN edits, 0..3 (the page's, not saved) */
    tessera_constel_t constel[TESSERA_TRACKS];
    tessera_series_t series[TESSERA_TRACKS];
    tessera_chord_t  chord[TESSERA_TRACKS];
    uint16_t         chord_pc[TESSERA_TRACKS]; /* CHORD: the pitch classes of the chord it plays (bit 0 C), 0 none yet
                                               * (QNT CHRD's: tessera_chord_pcs; not saved) */
    tessera_evolve_t evolve[TESSERA_TRACKS];
    tessera_sec_t    sec[TESSERA_TRACKS][3];
    uint8_t          last_note[TESSERA_TRACKS];
    uint8_t          follow_mem[TESSERA_TRACKS];   /* FOLLOW of a STEP track: its last sounding note + 1, 0 none yet */
    uint8_t          active_gate[TESSERA_TRACKS];
    uint8_t          hit[TESSERA_TRACKS];     /* its last step played on its own voice (LOGIC; seq.c sets a STEP track's) */
    uint8_t          br_in[TESSERA_TRACKS];   /* its seq_notes hold notes another track BRANCHed onto it */
    uint8_t          org_speed[TESSERA_TRACKS];   /* SLOTH SPEED 0..100 (organic.c), 50: the module's speeds */
    uint8_t          org_rate[TESSERA_TRACKS];    /* OCHD RATE 0..100, 50: 64 s .. 2.4 s a turn */
    int16_t          cv[TESSERA_TRACKS];      /* the generator's CV of its last step (gen.c; the cords' source CV) */
    uint16_t         seed[TESSERA_TRACKS];    /* SEED 1..128 (saved by 1.1.3-1.1.5: up to 9999), 0 = not picked yet
                                               * (tessera.c tess_seed_fresh) */
    uint32_t         rnd[TESSERA_TRACKS];     /* each track's random stream, from its seed (gen.c gen_stream) */
    uint16_t         roll[TESSERA_TRACKS];    /* ROLL turned while playing: the seed for the next bar line, 0 none
                                               * (tessera.c tessera_seed_turn; not saved) */
    uint8_t          nl_mode[TESSERA_TRACKS]; /* NOTE LENGTH (note_len.c): 0 STEP (the GATE), 1 FIXED, 2 HIT */
    uint8_t          nl_len[TESSERA_TRACKS];  /* FIXED's length: 0..5 = 1/32 1/16 1/8 1/4 1/2 1 bar */
    uint8_t          nl_keys[TESSERA_TRACKS]; /* FIXED: 1..4 notes held at once */
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
void tessera_tweak2_knob(uint32_t k, int32_t s);    /* TWEAK 2 page: BRANCH TO LOGIC COND */
void tessera_len_knob(uint32_t k, int32_t s);       /* LENGTH page: MODE LEN KEYS (KNOB 4: PATTERN's GATE) */
void tessera_len_tick(track_t *t, uint32_t n);      /* seq.c, each block: FIXED's held notes end on time */
void tessera_len_stop(track_t *t);                  /* the held notes end now (panic, a pattern change) */
void organic_play(void);                            /* seq.c, PLAY: SLOTH and OCHD from each track's SEED */
const char *tessera_cond_name(uint32_t cond);
uint32_t tessera_form_get(uint32_t tr, uint8_t seq[4], uint32_t *pos);   /* returns the form's length, 0 = OFF */
const char *tessera_form_name(uint32_t form);
#define TESSERA_PK_BYTES 540u                       /* the saved state, version 12 (tessera.c: TESS_PK_TRACK x TESSERA_TRACKS) */
#define TESSERA_PK_BYTES_V11 520u                   /* version 11 (before CHORD) */
#define TESSERA_PK_BYTES_V10 516u                   /* version 10 (before CONSTELLATION's LEN and FILL) */
#define TESSERA_PK_BYTES_V9 508u                    /* version 9 (before QUAD's columns, RANGE and LINK to several) */
#define TESSERA_PK_BYTES_V8 480u                    /* version 8 (before SERIES and CELLS) */
#define TESSERA_PK_BYTES_V7 392u                    /* version 7 (before CONSTELLATION) */
#define TESSERA_PK_BYTES_V6 380u                    /* version 6 (before NOTE LENGTH) */
#define TESSERA_PK_BYTES_V5 352u                    /* version 5 (before QUAD) */
#define TESSERA_PK_BYTES_V4 320u                    /* version 4 (before BRANCH, LOGIC, COND, SLOTH, OCHD) */
#define TESSERA_PK_BYTES_V3 204u                    /* version 3 (before BOUNCE, AWAKE, POLY): older projects and backups */
#define TESSERA_PK_BYTES_V2 188u                    /* version 2 (before GRIDS) */
#define TESSERA_PK_BYTES_V1 180u                    /* version 1 (before SEED) */
uint32_t tessera_pack(uint8_t *out);                /* live state -> TESSERA_PK_BYTES */
int tessera_unpack(const uint8_t *in);              /* 1 loaded; 0 refused (range check), nothing changed */
int tessera_unpack_ver(uint32_t ver, const uint8_t *in);   /* the same for a block of version ver (1..12) */
uint32_t tessera_pk_bytes(uint32_t ver);            /* a version's block length, 0 none */
void tessera_track_up(uint32_t ver, const uint8_t *in, uint8_t *out);   /* one track's record, version ver -> 12 */
void tessera_reset(void);                           /* every track OFF, defaults */
/* Tessera's parameters by number (pid.h: PID_TESS(x)). FROZEN: cords and saved projects keep these numbers, so an id
 * never changes meaning; new ones go at the end, a retired one stays as a gap. Ranges and help: pid.c TX_DESC */
enum {
    TX_MODE,                                        /* STEP (off) or a generator */
    TX_TUR_LEN, TX_TUR_PROB, TX_TUR_SPAN,
    TX_EUC_STEPS, TX_EUC_PULSES, TX_EUC_ROT,
    TX_MKV_LEAP, TX_MKV_DENS, TX_MKV_SPAN,
    TX_MRP_POS, TX_MRP_SNPA, TX_MRP_SNPB,
    TX_EV_LOCK, TX_EV_EVERY, TX_EV_AMOUNT, TX_EV_SEED,
    TX_FM_FORM, TX_FM_CONTRAST, TX_FM_FLOURISH,
    TX_TW_SHIFT, TX_TW_DROP, TX_TW_HUMAN, TX_TW_FOLLOW,
    TX_GRD_X, TX_GRD_Y, TX_GRD_DENS,
    TX_BNC_SPEED, TX_BNC_ANGLE, TX_BNC_BALLS,
    TX_AWK_LENA, TX_AWK_LENB, TX_AWK_PROB,
    TX_POL_DIV1, TX_POL_DIV2, TX_POL_DIV3, TX_POL_DIV4, TX_POL_SPAN,
    TX_TW_BRANCH, TX_TW_BR_TO, TX_TW_BR_MODE, TX_TW_LOGIC, TX_TW_LG_WITH, TX_TW_COND,
    TX_ORG_SPEED, TX_ORG_RATE,
    TX_QD_DIR, TX_QD_PROB, TX_QD_LINK,
    TX_NL_MODE, TX_NL_LEN, TX_NL_KEYS,
    TX_CS_Y, TX_CS_DENS, TX_CS_SIZE, TX_CS_AUTO,
    TX_SR_START, TX_SR_INTV, TX_SR_LOOP,
    TX_CL_RULE_RETIRED, TX_CL_TAP_RETIRED, TX_CL_SPAN_RETIRED,   /* (CELLS's, cut 2026-10-20: gaps, no parameter) */
    TX_QD_RANGE, TX_QD_MOVE1, TX_QD_MOVE2, TX_QD_MOVE3, TX_QD_MOVE4, TX_QD_EVERY1, TX_QD_EVERY2, TX_QD_EVERY3, TX_QD_EVERY4,
    TX_CS_LEN, TX_CS_FILL,
    TX_CH_PROG, TX_CH_BARS, TX_CH_HIT,
    TX_COUNT
};
int32_t tessera_param_get(uint32_t tr, uint32_t x);           /* the value its page shows */
void tessera_param_poke(uint32_t tr, uint32_t x, int32_t v); /* written as it is (cord.c, around a step) */
void tessera_param_set(uint32_t tr, uint32_t x, int32_t v);  /* clamped, with what its knob does besides (a MODE
                                                               * change releases the notes, STEPS keeps PULSES ..) */
/* the UI's view of a track's generators (read only; tr < TESSERA_TRACKS) */
const gen_turing_t *tessera_turing(uint32_t tr);
const gen_euclid_t *tessera_euclid(uint32_t tr);
const gen_markov_t *tessera_markov(uint32_t tr);
const gen_morph_t *tessera_morph(uint32_t tr);
const gen_grids_t *tessera_grids(uint32_t tr);
const gen_bounce_t *tessera_bounce(uint32_t tr);
const gen_awake_t *tessera_awake(uint32_t tr);
void tessera_awake_select(uint32_t tr, uint32_t step);   /* the step KNOB 1 (A) and KNOB 2 (B) set (the same again, or
                                                        * step 16: none) */
const gen_poly_t *tessera_poly(uint32_t tr);
const gen_quad_t *tessera_quad(uint32_t tr);
void tessera_quad_cols_knob(uint32_t k, int32_t s);  /* OCT+ held on GEN in QUAD: COL, MOVE, EVERY, RANGE */
uint32_t tessera_quad_every(uint32_t c);            /* column c's EVERY in steps */
const gen_constel_t *tessera_constel(uint32_t tr);
uint32_t tessera_cs_speed(uint32_t tr);                 /* Constellation's SPEED: the track's DIV, 0 the slowest .. 7 */
#define TESS_CS_SPEEDS 8u                               /* (4BAR .. 1/32, the straight DIVs) */
void tessera_constel_layer_knob(uint32_t k, int32_t s);   /* OCT+ held on GEN in CONSTELLATION: LOCK, LEN, FILL, SIZE */
const gen_series_t *tessera_series(uint32_t tr);
const gen_chord_t *tessera_chord(uint32_t tr);
uint32_t tessera_chord_pcs(void);     /* QNT CHRD: the chord the first CHORD track plays (pitch classes, bit 0 C), 0 none */
uint32_t tessera_chord_snap(uint32_t pcs, int32_t n);   /* n moved to the nearest note of pcs (a tie: down), 0..127 */
uint32_t tessera_chord_at(uint32_t tr, uint32_t i, char *name);   /* chord i of its progression: name (12 B) -> pitch
                                                                 * classes (bit 0 C) | its root's << 12 */

#endif
