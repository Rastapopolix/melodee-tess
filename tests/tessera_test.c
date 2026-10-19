/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis */
/* Host test of the Tessera generators (firmware/src/tessera.c) through the real sequencer and transport
 * (hostsim.c, mix_block): the baseline for phase 1 (ROADMAP.md), which moves the generators behind a new
 * interface and must play exactly what they play today.
 *   build/host/tessera_test tests/tessera_baseline.txt [LOGDIR]
 * Each scenario runs in its own process from power-on state with a fixed seed of the shared rng(): it sets a
 * generator up through the same knob functions the UI calls (tessera_knob, tessera_evolve_knob, ..), plays for
 * some bars at 120 BPM, and logs every note a generator plays (hostsim.c TESSERA_NOTE_HOOK): the block (32
 * samples) it starts in, the track, the note, the velocity and its length in blocks (until the sequencer has let
 * it go). The log's hash and note count must equal tests/tessera_baseline.txt. LOGDIR gets each log as text, to
 * diff against a log of the old code when a hash changes.
 * Each scenario's whole generator state after the stop is hashed too (NAME.state in the baseline), for what the
 * notes do not show.
 * Also: the same scenario twice gives the same notes; stopping leaves no generator note held; the saved block
 * (tessera_pack) has the same hash, survives a round trip, and a damaged one is refused with nothing changed.
 * TESSERA_UPDATE=1 rewrites the baseline: only after an intended change of what the generators play. */
#define TESSERA_NOTE_HOOK tt_note
#define main hostsim_main
#include "hostsim.c"
#undef main

/* ------------------------------------------------------------------ the log --- */
#define TT_MAX 8192u
typedef struct { uint32_t blk; uint8_t tr, note, vel; uint16_t len; } tt_ev_t;
static tt_ev_t tt_ev[TT_MAX];
static uint32_t tt_n, tt_blk, tt_lost;
static uint32_t tt_bar_blk[64];               /* the block each bar of the scenario started at */
static uint32_t tt_mark[2];                   /* the block an action took effect at (b_seed_back, b_replay) */
static int32_t tt_open[NTRK];                 /* the event of the track's note still held, -1 none */

static void tt_close(uint32_t tr)
{
    if (tt_open[tr] >= 0) {
        tt_ev[tt_open[tr]].len = (uint16_t)(tt_blk - tt_ev[tt_open[tr]].blk);
        tt_open[tr] = -1;
    }
}

static void tt_note(track_t *t, uint32_t note, uint32_t vel)
{
    uint32_t tr = trk_index(t);
    if (tr >= NTRK) return;
    tt_close(tr);
    if (tt_n >= TT_MAX) { tt_lost++; return; }
    tt_ev[tt_n] = (tt_ev_t){tt_blk, (uint8_t)tr, (uint8_t)note, (uint8_t)vel, 0};
    tt_open[tr] = (int32_t)tt_n++;
}

static uint32_t fnv(uint32_t h, uint32_t v)
{
    for (uint32_t i = 0; i < 4u; i++) {
        h ^= (v >> (8u * i)) & 0xFFu;
        h *= 16777619u;
    }
    return h;
}

static uint32_t tt_hash(void)
{
    uint32_t h = 2166136261u;
    for (uint32_t i = 0; i < tt_n; i++) {
        const tt_ev_t *e = &tt_ev[i];
        h = fnv(h, e->blk);
        h = fnv(h, (uint32_t)e->tr | (uint32_t)e->note << 8 | (uint32_t)e->vel << 16);
        h = fnv(h, e->len);
    }
    return h;
}
/* every byte of the generators' state (tessera.h tessera_state_t) after the stop: what the notes do not show
 * (a register, a section's phrase, the record flags of another engine) must stay as it was too */
static uint32_t tt_state_hash(void)
{
    const uint8_t *p = (const uint8_t *)&tessera;
    uint32_t h = 2166136261u;
    for (uint32_t i = 0; i < sizeof tessera; i++) h = fnv(h, p[i]);
    return h;
}

/* ------------------------------------------------------------- the knobs --- */
static void sel(uint32_t tr) { song.sel = (uint8_t)tr; }
static void gen(uint32_t tr, uint32_t mode) { sel(tr); tessera_knob(0, (int32_t)mode); }   /* from OFF */
static void knob(void (*f)(uint32_t, int32_t), uint32_t k, int32_t s) { f(k, s); }
static void turns(void (*f)(uint32_t, int32_t), uint32_t k, int32_t n)    /* EVERY, FORM, FOLLOW: one a turn */
{
    for (; n > 0; n--) f(k, 1);
    for (; n < 0; n++) f(k, -1);
}

/* ---------------------------------------------------------- the scenarios --- */
static void s_turing(void) { gen(0, TESS_ENG_TURING); }
static void s_turing_var(void)
{
    gen(0, TESS_ENG_TURING);
    knob(tessera_knob, 1, -8);                        /* LEN 8 */
    knob(tessera_knob, 2, 15);                        /* PROB 30 */
    knob(tessera_knob, 3, 1);                         /* 3 octaves */
}
static void s_euclid(void) { gen(0, TESS_ENG_EUCLID); }
static void s_euclid_var(void)
{
    gen(0, TESS_ENG_EUCLID);
    knob(tessera_knob, 1, -3);                        /* 13 steps */
    knob(tessera_knob, 2, 1);                         /* 5 pulses */
    knob(tessera_knob, 3, 3);                         /* rotation 3 */
    knob(tessera_evolve_knob, 3, 1);                  /* ROLL: another pitch seed */
}
static void s_markov(void) { gen(0, TESS_ENG_MARKOV); }
static void s_markov_var(void)
{
    gen(0, TESS_ENG_MARKOV);
    knob(tessera_knob, 1, 14);                        /* TENSION 81 */
    knob(tessera_knob, 2, -2);                        /* DENSITY 60 */
    knob(tessera_knob, 3, 1);                         /* 3 octaves */
}
static void s_morph(void) { gen(0, TESS_ENG_MORPH); }
static void s_morph_var(void)
{
    gen(0, TESS_ENG_MORPH);
    knob(tessera_knob, 1, 16);                        /* MORPH 64 */
    knob(tessera_knob, 2, 3);                         /* A: son clave */
    knob(tessera_knob, 3, 3);                         /* B: rumba clave (from the B slot's 2) */
}
static void s_evolve_turing(void)
{
    gen(0, TESS_ENG_TURING);
    knob(tessera_evolve_knob, 0, -5);                 /* LOCK 90: PROB 10 */
    turns(tessera_evolve_knob, 1, 1);                 /* EVERY 1 bar */
    knob(tessera_evolve_knob, 2, 10);                 /* AMOUNT 50 */
}
static void s_evolve_euclid(void)
{
    gen(0, TESS_ENG_EUCLID);
    turns(tessera_evolve_knob, 1, 1);
    knob(tessera_evolve_knob, 2, 25);                 /* AMOUNT 80 */
}
static void s_evolve_markov(void)
{
    gen(0, TESS_ENG_MARKOV);
    knob(tessera_evolve_knob, 0, 30);                 /* LOCK 60 */
    turns(tessera_evolve_knob, 1, 2);                 /* EVERY 2 bars */
}
static void s_evolve_morph(void)
{
    gen(0, TESS_ENG_MORPH);
    knob(tessera_evolve_knob, 0, 50);                 /* LOCK 100 */
    turns(tessera_evolve_knob, 1, 1);
    knob(tessera_evolve_knob, 2, 5);                  /* AMOUNT 40 */
}
static void s_form_turing(void)
{
    gen(0, TESS_ENG_TURING);
    turns(tessera_form_knob, 0, 2);                   /* ABAC (EVERY becomes 4 bars) */
    turns(tessera_evolve_knob, 1, -2);                /* EVERY 1 bar */
    knob(tessera_form_knob, 1, 10);                   /* CONTRAST 80 */
    knob(tessera_form_knob, 3, 5);                    /* FLOURISH +5 */
}
static void s_form_markov(void)
{
    gen(0, TESS_ENG_MARKOV);
    knob(tessera_evolve_knob, 0, 25);                 /* LOCK 50 */
    turns(tessera_form_knob, 0, 4);                   /* AABA */
    turns(tessera_evolve_knob, 1, -1);                /* EVERY 2 bars */
}
static void s_form_euclid_morph(void)
{
    gen(0, TESS_ENG_EUCLID);
    turns(tessera_form_knob, 0, 3);                   /* ABCB */
    turns(tessera_evolve_knob, 1, -2);
    gen(1, TESS_ENG_MORPH);
    knob(tessera_evolve_knob, 0, 40);                 /* LOCK 80 */
    turns(tessera_form_knob, 0, 6);                   /* AABC */
    turns(tessera_evolve_knob, 1, -2);
    knob(tessera_form_knob, 3, -3);                   /* FLOURISH -3 */
}
static void s_tweak(void)
{
    gen(0, TESS_ENG_EUCLID);
    knob(tessera_tweak_knob, 0, 3);                   /* SHIFT +3 */
    knob(tessera_tweak_knob, 1, 15);                  /* DROP 30 */
    knob(tessera_tweak_knob, 2, 30);                  /* HUMAN 60 */
    gen(1, TESS_ENG_MARKOV);
    turns(tessera_tweak_knob, 3, 1);                  /* FOLLOW track 1 */
    knob(tessera_tweak_knob, 0, 7);                   /* a fifth above it */
}
static void s_scale(void)
{
    gen(0, TESS_ENG_TURING);
    knob(tessera_knob, 2, 10);
    trk[0].p[P_SCALE] = 2;                            /* minor */
    trk[0].p[P_ROOT] = 2;
    trk[0].p[P_TRANS] = -5;
    gen(1, TESS_ENG_EUCLID);
    trk[1].p[P_SCALE] = 5;                            /* major pentatonic */
    trk[1].p[P_ROOT] = 9;
}
static void s_div_gate(void)
{
    gen(0, TESS_ENG_MARKOV);
    trk[0].p[P_SDIV] = 1;                             /* 1/8 */
    trk[0].p[P_SGATE] = 100;
    turns(tessera_evolve_knob, 1, 1);
    gen(1, TESS_ENG_MORPH);
    trk[1].p[P_SDIV] = 5;                             /* 16T */
    trk[1].p[P_SLEN] = 12;
    turns(tessera_evolve_knob, 1, 1);
    knob(tessera_evolve_knob, 2, 20);
}
static void s_all(void)
{
    gen(0, TESS_ENG_TURING);
    knob(tessera_knob, 2, 5);
    turns(tessera_evolve_knob, 1, 2);
    gen(1, TESS_ENG_EUCLID);
    turns(tessera_evolve_knob, 1, 2);
    gen(2, TESS_ENG_MARKOV);
    knob(tessera_evolve_knob, 0, 35);
    turns(tessera_form_knob, 0, 2);
    gen(3, TESS_ENG_MORPH);
    turns(tessera_evolve_knob, 1, 3);
    knob(tessera_tweak_knob, 1, 10);
}
static void s_transport(void)
{
    gen(0, TESS_ENG_MARKOV);
    knob(tessera_evolve_knob, 0, 30);
    turns(tessera_form_knob, 0, 3);                   /* ABCB */
    turns(tessera_evolve_knob, 1, -2);
    gen(1, TESS_ENG_TURING);
    turns(tessera_evolve_knob, 1, 1);
}
static void b_transport(uint32_t bar)               /* stop and start again: forms back to A, loops re-recorded */
{
    if (bar == 5u || bar == 9u) {
        int32_t o[2 * CTL];
        transport_req = 2;
        mix_block(o, CTL);
        tt_blk++;
        transport_req = 1;
    }
}
static void s_mute(void)
{
    gen(0, TESS_ENG_EUCLID);
    gen(1, TESS_ENG_TURING);
}
static void b_mute(uint32_t bar)                     /* MUTE as the MIXER sets it (the generator keeps counting) */
{
    if (bar == 3u) trk[0].p[P_MUTE] = 1;
    if (bar == 5u) trk[0].p[P_MUTE] = 0;
}
static void s_edit_live(void) { gen(0, TESS_ENG_TURING); }
static void b_edit_live(uint32_t bar)                /* knobs turned while it plays: engine change, ROLL, LEN */
{
    sel(0);
    if (bar == 2u) knob(tessera_knob, 1, -4);
    if (bar == 3u) knob(tessera_evolve_knob, 3, 1);
    if (bar == 4u) knob(tessera_knob, 0, 2);        /* TURING -> MARKOV */
    if (bar == 6u) knob(tessera_knob, 0, 1);        /* -> MORPH */
}

/* external MIDI clock (GLO CLK: USB), the host at 120 BPM: 24 pulses a beat throughout, Start at 0, Stop after
 * 2 bars, Continue at 2.5, Stop at 4.5, Start (from the top) at 5.5 bars. The transport is the clock's alone. */
static void s_midi_clock(void)
{
    song.g[G_CLOCK] = 1;
    gen(0, TESS_ENG_TURING);
    gen(1, TESS_ENG_EUCLID);
    gen(2, TESS_ENG_MARKOV);
    turns(tessera_evolve_knob, 1, 1);                 /* EVERY 1 bar on track 3 */
}
static void k_midi_clock(uint32_t n)                 /* before block n: the clock's messages due by its time, in order */
{
    static const struct { uint32_t ms; uint8_t st; } EV[] = {{0, 0xFA}, {4000, 0xFC}, {5000, 0xFB}, {9000, 0xFC}, {11000, 0xFA}};
    static uint32_t pulse, ev;
    uint32_t ms = (uint32_t)((uint64_t)n * CTL * 1000u / FS);
    fm1_ms = ms;
    for (;;) {
        uint32_t tp = pulse * 125u / 6u, te = ev < sizeof EV / sizeof EV[0] ? EV[ev].ms : 0xFFFFFFFFu;
        if (tp > ms && te > ms)
            break;
        if (te <= tp)                                 /* (a message before the pulse at its time) */
            midi_enqueue(0xFu | (uint32_t)EV[ev++].st << 8, 1);
        else {
            midi_enqueue(0xFu | 0xF8u << 8, 1);
            pulse++;
        }
    }
}

/* swing: the track's own and the global, together */
static void s_swing(void)
{
    gen(0, TESS_ENG_TURING);
    gen(1, TESS_ENG_EUCLID);
    trk[0].p[P_SSWING] = 40;
    song.g[G_SWING] = 30;
}
/* the track's LEN shorter than the generator's loop (Turing 16, Euclid 16 steps) */
static void s_short_len(void)
{
    gen(0, TESS_ENG_TURING);
    gen(1, TESS_ENG_EUCLID);
    trk[0].p[P_SLEN] = 5;
    trk[1].p[P_SLEN] = 7;
}
/* a pattern bank switch while playing (PATTERNS: the next bank at the loop's end): bank 2 is 8 steps long */
static void s_pattern_switch(void)
{
    gen(0, TESS_ENG_TURING);
    gen(1, TESS_ENG_MORPH);
    pattern_at(0, 1)->timing[0] = 8;
}
static void b_pattern_switch(uint32_t bar)
{
    if (bar == 2u) trk[0].pattern_next = 1;
    if (bar == 5u) trk[0].pattern_next = 0;
}
/* SONG: a chain of bank 1 twice, then bank 2 (8 steps) twice, as project.c chain_prepare arms it; it stops itself */
static void s_song_chain(void)
{
    uint32_t k;
    gen(0, TESS_ENG_MARKOV);
    gen(1, TESS_ENG_EUCLID);
    for (k = 0; k < NTRK; k++) {
        pattern_at(k, 1)->timing[0] = 8;
        chain_patterns[0][k] = 0;
        chain_patterns[1][k] = 1;
        pattern_commit(&trk[k]);
        trk[k].pattern_next = 0xff;
    }
    chain_config.count = 2;
    chain_config.row[0] = (chain_row_t){0, 2};
    chain_config.row[1] = (chain_row_t){1, 2};
    chain.config = chain_config;
    chain.armed = 1;
}
/* GLO held: KNOB / key mute, as ui_layer.c does it (MUTE, then tessera_kill: the generator's note released) */
static void s_glo_mute(void)
{
    gen(0, TESS_ENG_EUCLID);
    gen(1, TESS_ENG_TURING);
}
static void b_glo_mute(uint32_t bar)
{
    if (bar == 3u) { trk[1].p[P_MUTE] = 1; tessera_kill(&trk[1]); }
    if (bar == 5u) trk[1].p[P_MUTE] = 0;
}

/* FOLLOW of a STEP track: a written melody on track 1 (C major, 8 steps, with rests), track 2's Markov following
 * it a third above (SHIFT +2 scale degrees, both tracks in C major) */
static const uint8_t FOLLOW_MELODY[8] = {60, 62, 64, 0, 67, 65, 0, 72};   /* 0: a rest */
static void s_follow_step(void)
{
    uint32_t i;
    for (i = 0; i < 8u; i++) {
        step_t *st = &trk[0].step[i];
        st->n = FOLLOW_MELODY[i] ? 1u : 0u;
        st->note[0] = FOLLOW_MELODY[i];
        st->time = FOLLOW_MELODY[i] ? ST_NOTE : ST_REST;
        st->vel = 100;
    }
    trk[0].p[P_SLEN] = 8;
    trk[0].p[P_SCALE] = 1;                            /* major */
    trk[1].p[P_SCALE] = 1;
    gen(1, TESS_ENG_MARKOV);
    knob(tessera_knob, 2, 6);                         /* DENSITY 100: a note every step (follow_run) */
    turns(tessera_tweak_knob, 3, 1);                  /* FOLLOW track 1 */
    knob(tessera_tweak_knob, 0, 2);                   /* SHIFT +2: a third */
}

typedef struct {
    const char *name;
    uint32_t bars, seed;
    void (*setup)(void);
    void (*bar)(uint32_t bar);
    void (*blk)(uint32_t n);                         /* before each block (n from 0): it runs the transport too */
} scen_t;
/* ROLL: turned away just after bar 2's line and back after bar 4's (track 1 Markov, track 2 Turing at PROB 30):
 * each waits for the next bar line, so bar 2 plays on the first line, bars 3-4 another, bars 5-6 what bars 0-1
 * played, note for note. seed_check and roll_wait_check compare (s_seed_back alone: the line never turned) */
static void s_seed_back(void)
{
    gen(0, TESS_ENG_MARKOV);
    gen(1, TESS_ENG_TURING);
    knob(tessera_knob, 2, 15);                        /* PROB 30: Turing draws too */
}
static void b_seed_back(uint32_t bar)
{
    if (bar == 3u || bar == 5u)                      /* the seed's line from this bar's line (its step: a block or two
                                                      * before, a bar being 2756.25 blocks; a step 172) */
        tt_mark[bar == 3u ? 0 : 1] = tt_blk - 2u;
    if (bar != 2u && bar != 4u) return;
    for (uint32_t tr = 0; tr < 2u; tr++) {
        sel(tr);
        knob(tessera_evolve_knob, 3, bar == 2u ? 1 : -1);
    }
}
/* PLAY again (stopped at bar 2, EVOLVE off): bars 2-3 play what bars 0-1 played */
static void s_replay(void) { gen(0, TESS_ENG_MARKOV); }
static void b_replay(uint32_t bar)
{
    if (bar == 2u) {
        int32_t o[2 * CTL];
        transport_req = 2;
        mix_block(o, CTL);
        tt_blk++;
        transport_req = 1;
        tt_mark[1] = tt_blk;
    }
}

static void s_grids(void) { gen(0, TESS_ENG_GRIDS); }
static void s_grids_var(void)
{
    gen(0, TESS_ENG_GRIDS);
    knob(tessera_knob, 1, -20);                       /* X 48 */
    knob(tessera_knob, 2, 25);                        /* Y 228 */
    knob(tessera_knob, 3, 15);                        /* DENSITY 80 */
    knob(tessera_evolve_knob, 0, -20);                /* LOCK 60: CHAOS 40 */
    knob(tessera_tweak_knob, 1, 10);                  /* DROP 20 */
    knob(tessera_tweak_knob, 2, 15);                  /* HUMAN 30 */
}
static void s_evolve_grids(void)
{
    gen(0, TESS_ENG_GRIDS);
    turns(tessera_evolve_knob, 1, 1);                 /* EVERY 1 bar */
    knob(tessera_evolve_knob, 2, 20);                 /* AMOUNT 70 */
    turns(tessera_form_knob, 0, 2);                   /* FORM ABAC */
}

static void s_bounce(void) { gen(0, TESS_ENG_BOUNCE); }
static void s_bounce_var(void)
{
    gen(0, TESS_ENG_BOUNCE);
    knob(tessera_knob, 1, 16);                        /* SPEED 40 */
    knob(tessera_knob, 2, 23);                        /* ANGLE 60 */
    turns(tessera_knob, 3, 2);                        /* BALLS 3 */
    knob(tessera_tweak_knob, 1, 10);                  /* DROP 20 */
}
static void s_fall(void)
{
    gen(0, TESS_ENG_BOUNCE);
    turns(tessera_knob, 3, 4);                        /* BALLS: FALL */
}
static void s_fall_var(void)
{
    gen(0, TESS_ENG_BOUNCE);
    turns(tessera_knob, 3, 4);                        /* FALL */
    knob(tessera_knob, 1, 16);                        /* SPEED 40 */
    knob(tessera_knob, 2, -30);                       /* WIND -38 */
    turns(tessera_evolve_knob, 1, 1);                 /* EVERY 1 bar */
    knob(tessera_evolve_knob, 2, 15);                 /* AMOUNT 60 */
}
static void s_evolve_bounce(void)
{
    gen(0, TESS_ENG_BOUNCE);
    turns(tessera_evolve_knob, 1, 1);                 /* EVERY 1 bar */
    knob(tessera_evolve_knob, 2, 15);                 /* AMOUNT 60 */
    turns(tessera_form_knob, 0, 1);                   /* FORM AB */
}

static void s_awake(void) { gen(0, TESS_ENG_AWAKE); }
static void s_awake_var(void)
{
    gen(0, TESS_ENG_AWAKE);
    knob(tessera_knob, 1, 4);                         /* LEN A 12 */
    knob(tessera_knob, 2, -2);                        /* LEN B 5 */
    knob(tessera_knob, 3, -15);                       /* PROB 70 */
    knob(tessera_tweak_knob, 0, 2);                   /* SHIFT +2 */
}
static void s_evolve_awake(void)
{
    gen(0, TESS_ENG_AWAKE);
    turns(tessera_evolve_knob, 1, 1);                 /* EVERY 1 bar */
    knob(tessera_evolve_knob, 2, 20);                 /* AMOUNT 70 */
    turns(tessera_form_knob, 0, 2);                   /* FORM ABAC: A's values come back */
}

static void s_poly(void) { gen(0, TESS_ENG_POLY); }
static void s_poly_var(void)
{
    gen(0, TESS_ENG_POLY);
    turns(tessera_knob, 1, 2);                        /* RHY 3 */
    knob(tessera_knob, 2, 5);                         /* its DIV 5 */
    turns(tessera_knob, 1, 1);                        /* RHY 4 */
    knob(tessera_knob, 2, 7);                         /* its DIV 7 */
    knob(tessera_knob, 3, 1);                         /* SPAN 2 */
    knob(tessera_tweak_knob, 2, 20);                  /* HUMAN 40 */
}
static void s_evolve_poly(void)
{
    gen(0, TESS_ENG_POLY);
    turns(tessera_evolve_knob, 1, 1);                 /* EVERY 1 bar */
    knob(tessera_evolve_knob, 2, 35);                 /* AMOUNT 100 */
    turns(tessera_form_knob, 0, 1);                   /* FORM AB */
}

/* TWEAK 2: BRANCH, LOGIC, COND (tessera_tweak2_knob) */
static void s_branch_coin(void)
{
    gen(0, TESS_ENG_MARKOV);
    knob(tessera_tweak2_knob, 0, 25);                 /* BRANCH 50 */
    turns(tessera_tweak2_knob, 1, 1);                 /* TO T2 COIN (track 2: STEP) */
}
static void s_branch_toggle(void)
{
    gen(0, TESS_ENG_EUCLID);
    knob(tessera_knob, 2, 3);                         /* 7 pulses */
    gen(1, TESS_ENG_TURING);
    sel(0);
    knob(tessera_tweak2_knob, 0, 40);                 /* BRANCH 80 */
    turns(tessera_tweak2_knob, 1, 2);                 /* TO T2 TOGGLE */
}
static void s_branch_latch(void)
{
    gen(0, TESS_ENG_BOUNCE);
    knob(tessera_knob, 3, 1);                         /* 2 balls */
    gen(2, TESS_ENG_EUCLID);
    sel(0);
    knob(tessera_tweak2_knob, 0, 30);                 /* BRANCH 60 */
    turns(tessera_tweak2_knob, 1, 6);                 /* TO T3 LATCH */
}
static uint32_t lg_op;                                /* (s_logic: 1 AND .. 4 NOT) */
static void s_logic(void)
{
    gen(0, TESS_ENG_EUCLID);
    knob(tessera_knob, 2, 1);                         /* 5 pulses */
    gen(1, TESS_ENG_MARKOV);                          /* (DENSITY 70: rests of its own) */
    turns(tessera_tweak2_knob, 2, (int32_t)(1u + (lg_op - 1u) * 3u));   /* LOGIC op, T1 */
}
static void s_logic_and(void) { lg_op = TESS_LG_AND; s_logic(); }
static void s_logic_or(void) { lg_op = TESS_LG_OR; s_logic(); }
static void s_logic_xor(void) { lg_op = TESS_LG_XOR; s_logic(); }
static void s_logic_not(void) { lg_op = TESS_LG_NOT; s_logic(); }
static void s_logic_higher(void)                      /* track 1 reads track 2's step before */
{
    gen(1, TESS_ENG_EUCLID);
    gen(0, TESS_ENG_TURING);
    turns(tessera_tweak2_knob, 2, 1);                 /* AND T2 */
}
static void s_cond_form(void)
{
    gen(0, TESS_ENG_TURING);
    turns(tessera_form_knob, 0, 1);                   /* FORM AB, 4 bars a section */
    turns(tessera_tweak2_knob, 3, 4);                 /* COND 2:4 */
    gen(1, TESS_ENG_GRIDS);
    turns(tessera_tweak2_knob, 3, 10);                /* COND !FIRST */
    turns(tessera_tweak2_knob, 2, 10);                /* NOT T1 */
}
static void s_quad(void) { gen(0, TESS_ENG_QUAD); }
static void s_quad_var(void)
{
    gen(0, TESS_ENG_QUAD);
    turns(tessera_knob, 1, GEN_QD_PEND);              /* DIR PEND */
    knob(tessera_knob, 2, -15);                       /* PROB 70 */
}
static void s_quad_four(void)                         /* four rows at four speeds, track 1 pulling a HOLD track 4 */
{
    gen(3, TESS_ENG_QUAD);
    turns(tessera_knob, 1, GEN_QD_HOLD);
    gen(2, TESS_ENG_QUAD);
    turns(tessera_knob, 1, GEN_QD_RAND);
    trk[2].p[P_SDIV] = (int16_t)(trk[2].p[P_SDIV] - 1);
    gen(1, TESS_ENG_QUAD);
    turns(tessera_knob, 1, GEN_QD_BACK);
    trk[1].p[P_SDIV] = (int16_t)(trk[1].p[P_SDIV] - 2);
    gen(0, TESS_ENG_QUAD);
    turns(tessera_knob, 3, 3);                        /* LINK T4 */
}
static void s_evolve_quad(void)
{
    s_quad_four();
    turns(tessera_evolve_knob, 1, 1);                 /* track 1: EVERY 1 bar */
    knob(tessera_evolve_knob, 2, 20);                 /* AMOUNT 70 */
}

static const scen_t SCEN[] = {
    {"turing", 8, 0x1001u, s_turing, 0},
    {"turing_len8_prob30_oct3", 8, 0x1002u, s_turing_var, 0},
    {"euclid", 4, 0x1003u, s_euclid, 0},
    {"euclid_13_5_rot3_roll", 8, 0x1004u, s_euclid_var, 0},
    {"markov", 8, 0x1005u, s_markov, 0},
    {"markov_tension81_density60_oct3", 8, 0x1006u, s_markov_var, 0},
    {"morph", 8, 0x1007u, s_morph, 0},
    {"morph_64_son_rumba", 8, 0x1008u, s_morph_var, 0},
    {"evolve_turing_lock90_every1_amount50", 16, 0x1009u, s_evolve_turing, 0},
    {"evolve_euclid_every1_amount80", 16, 0x100Au, s_evolve_euclid, 0},
    {"evolve_markov_lock60_every2", 16, 0x100Bu, s_evolve_markov, 0},
    {"evolve_morph_lock100_every1", 16, 0x100Cu, s_evolve_morph, 0},
    {"form_turing_abac_contrast80_flourish5", 24, 0x100Du, s_form_turing, 0},
    {"form_markov_aaba_lock50", 24, 0x100Eu, s_form_markov, 0},
    {"form_euclid_abcb_morph_aabc", 24, 0x100Fu, s_form_euclid_morph, 0},
    {"tweak_shift_drop_human_follow", 8, 0x1010u, s_tweak, 0},
    {"scale_root_transpose", 8, 0x1011u, s_scale, 0},
    {"division_gate_length", 8, 0x1012u, s_div_gate, 0},
    {"all_four_tracks", 32, 0x1013u, s_all, 0},
    {"transport_stop_start", 12, 0x1014u, s_transport, b_transport},
    {"mute_and_unmute", 6, 0x1015u, s_mute, b_mute},
    {"knobs_while_playing", 8, 0x1016u, s_edit_live, b_edit_live},
    {"midi_clock_start_stop_continue", 7, 0x1017u, s_midi_clock, 0, k_midi_clock},
    {"swing_track40_global30", 8, 0x1018u, s_swing, 0},
    {"track_len_shorter_than_loop", 8, 0x1019u, s_short_len, 0},
    {"pattern_switch_while_playing", 8, 0x101Au, s_pattern_switch, b_pattern_switch},
    {"song_chain_two_banks", 6, 0x101Bu, s_song_chain, 0},
    {"glo_mute_and_unmute", 6, 0x101Cu, s_glo_mute, b_glo_mute},
    {"follow_step_melody_third", 4, 0x101Du, s_follow_step, 0},
    {"seed_back_markov_turing", 8, 0x101Eu, s_seed_back, b_seed_back},
    {"seed_none_markov_turing", 4, 0x101Eu, s_seed_back, 0},
    {"replay_markov", 4, 0x101Fu, s_replay, b_replay},
    {"grids", 4, 0x1020u, s_grids, 0},
    {"grids_x48_y228_dens80_chaos40_drop_human", 8, 0x1021u, s_grids_var, 0},
    {"evolve_grids_every1_amount70_abac", 16, 0x1022u, s_evolve_grids, 0},
    {"bounce", 8, 0x1023u, s_bounce, 0},
    {"bounce_speed40_angle60_balls3_drop20", 8, 0x1024u, s_bounce_var, 0},
    {"evolve_bounce_every1_amount60_ab", 16, 0x1025u, s_evolve_bounce, 0},
    {"awake", 8, 0x1026u, s_awake, 0},
    {"awake_lena12_lenb5_prob70_shift2", 8, 0x1027u, s_awake_var, 0},
    {"evolve_awake_every1_amount70_abac", 16, 0x1028u, s_evolve_awake, 0},
    {"poly", 4, 0x1029u, s_poly, 0},
    {"poly_div_4_3_5_7_span2_human40", 8, 0x102Au, s_poly_var, 0},
    {"evolve_poly_every1_amount100_ab", 16, 0x102Bu, s_evolve_poly, 0},
    {"branch_markov_coin50_to_step_track", 8, 0x102Cu, s_branch_coin, 0},
    {"branch_euclid_toggle80_to_turing", 8, 0x102Du, s_branch_toggle, 0},
    {"branch_bounce_latch60_to_euclid", 8, 0x102Eu, s_branch_latch, 0},
    {"logic_markov_and_euclid", 4, 0x102Fu, s_logic_and, 0},
    {"logic_markov_or_euclid", 4, 0x102Fu, s_logic_or, 0},
    {"logic_markov_xor_euclid", 4, 0x102Fu, s_logic_xor, 0},
    {"logic_markov_not_euclid", 4, 0x102Fu, s_logic_not, 0},
    {"logic_turing_and_higher_track", 4, 0x1030u, s_logic_higher, 0},
    {"cond_2of4_form_ab_grids_notfirst_not", 16, 0x1031u, s_cond_form, 0},
    {"quad", 4, 0x1032u, s_quad, 0},
    {"quad_pend_prob70", 8, 0x1033u, s_quad_var, 0},
    {"quad_four_rows_link_hold", 8, 0x1034u, s_quad_four, 0},
    {"evolve_quad_every1_amount70", 16, 0x1035u, s_evolve_quad, 0},
    {"fall", 8, 0x1036u, s_fall, 0},
    {"fall_speed40_wind-38_evolve60", 16, 0x1037u, s_fall_var, 0},
};
#define NSCEN (sizeof SCEN / sizeof SCEN[0])

/* one scenario from power-on: "notes hash held" (held: generator notes still sounding after the stop) */
static void scen_run(const scen_t *sc, const char *logdir, char *res, size_t cap)
{
    uint32_t bar_len, f, total, b = 0, tr, held = 0;
    int32_t o[2 * CTL];
    host_tracks_init();
    tessera_reset();
    song.g[G_BPM] = 120;
    for (tr = 0; tr < NTRK; tr++) tt_open[tr] = -1;
    sc->setup();
    rng_state = sc->seed;
    bar_len = 4u * seq_beat_samples();
    total = sc->bars * bar_len;
    if (!sc->blk)
        transport_req = 1;
    for (f = 0; f < total; f += CTL) {
        if (f >= b * bar_len) {
            if (b < 64u) tt_bar_blk[b] = tt_blk;
            if (sc->bar && b) sc->bar(b);
            b++;
        }
        if (sc->blk) sc->blk(f / CTL);
        mix_block(o, CTL);
        tt_blk++;
        for (tr = 0; tr < NTRK; tr++)
            if (tt_open[tr] >= 0 && !trk[tr].seq_n && !(nl_active & (1u << tr))) tt_close(tr);   /* (NOTE LENGTH's) */
    }
    transport_req = 2;
    for (f = 0; f < 4u * CTL; f += CTL) {
        mix_block(o, CTL);
        tt_blk++;
        for (tr = 0; tr < NTRK; tr++)
            if (tt_open[tr] >= 0 && !trk[tr].seq_n && !(nl_active & (1u << tr))) tt_close(tr);   /* (NOTE LENGTH's) */
    }
    for (tr = 0; tr < NTRK; tr++) {
        if (tessera.mode[tr] == TESS_ENG_OFF) continue;
        held += trk[tr].seq_n != 0;
        for (uint32_t v = 0; v < NVOICE; v++) held += trk[tr].v[v].gate != 0;
    }
    if (logdir) {
        char path[512];
        snprintf(path, sizeof path, "%s/%s.txt", logdir, sc->name);
        FILE *fp = fopen(path, "w");
        if (fp) {
            fprintf(fp, "# %s: block track note velocity length (blocks of %u samples)\n", sc->name, (unsigned)CTL);
            for (uint32_t i = 0; i < tt_n; i++)
                fprintf(fp, "%u %u %u %u %u\n", tt_ev[i].blk, tt_ev[i].tr, tt_ev[i].note, tt_ev[i].vel, tt_ev[i].len);
            fclose(fp);
        }
    }
    snprintf(res, cap, "%u 0x%08x %u %u 0x%08x", tt_n, tt_hash(), held, tt_lost, tt_state_hash());
}

/* the scenario in a child process (power-on state each time): its result line */
static int scen_fork(const scen_t *sc, const char *logdir, char *res, size_t cap)
{
    int fd[2];
    pid_t pid;
    ssize_t n;
    if (pipe(fd)) return 1;
    pid = fork();
    if (pid < 0) return 1;
    if (!pid) {
        char r[96];
        close(fd[0]);
        scen_run(sc, logdir, r, sizeof r);
        if (write(fd[1], r, strlen(r) + 1u) < 0) _exit(1);
        _exit(0);
    }
    close(fd[1]);
    n = read(fd[0], res, cap - 1u);
    close(fd[0]);
    waitpid(pid, 0, 0);
    if (n <= 0) return 1;
    res[n] = 0;
    return 0;
}

/* FOLLOW of a STEP track, checked note by note: each note track 2 plays is two scale degrees above (a third, in C
 * major) the melody note track 1 sounds then, or last sounded across a rest. "notes wrong" */
static int major_step(int n, int k)                    /* n (in C major) moved k notes of C major */
{
    static const uint16_t MAJ = 0xab5u;
    int d = k < 0 ? -1 : 1;
    for (; k; k -= d)
        do n += d; while (!((MAJ >> ((n + 1200) % 12)) & 1u));
    return n;
}
static void follow_run(char *res, size_t cap)
{
    const scen_t *sc = 0;
    uint32_t i, j, n2 = 0, bad = 0;
    for (i = 0; i < NSCEN; i++) if (!strcmp(SCEN[i].name, "follow_step_melody_third")) sc = &SCEN[i];
    scen_run(sc, 0, res, cap);                        /* (the log has the generators' notes only: track 2's) */
    for (i = 0; i < tt_n; i++) {                      /* Markov at DENSITY 100: one note a step, step k = note k */
        int src = 0;
        if (tt_ev[i].tr != 1u) continue;
        for (j = 0; j < 8u && !src; j++)              /* the melody note sounding at step n2, or the last before */
            src = FOLLOW_MELODY[(n2 + 8u - j) % 8u];
        bad += (int)tt_ev[i].note != major_step(src, 2);
        n2++;
    }
    snprintf(res, cap, "%u %u", n2, bad);
}

/* track tr's notes from block blk on, at most cap (note | vel << 8) into out: the count */
static uint32_t tt_from(uint32_t tr, uint32_t blk, uint32_t *out, uint32_t cap)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < tt_n && n < cap; i++)
        if (tt_ev[i].tr == tr && tt_ev[i].blk >= blk) out[n++] = tt_ev[i].note | (uint32_t)tt_ev[i].vel << 8;
    return n;
}
/* the scenario name, tracks 0..ntr-1: their notes of bars 0-1 (k of them) against the k from tt_mark[1] on: "same
 * differ" (same: equal and not empty on every track; differ (mid): some track's k from tt_mark[0] on unlike them) */
static void seed_check(const char *name, uint32_t ntr, int mid, char *res, size_t cap)
{
    static uint32_t a[512], b[512], c[512];
    const scen_t *sc = 0;
    uint32_t i, same = 1, differ = mid ? 0u : 1u;
    for (i = 0; i < NSCEN; i++) if (!strcmp(SCEN[i].name, name)) sc = &SCEN[i];
    scen_run(sc, 0, res, cap);
    for (uint32_t tr = 0; tr < ntr; tr++) {
        uint32_t k = 0;
        for (i = 0; i < tt_n; i++)                   /* (the step on bar 2's line falls in the block before it) */
            k += tt_ev[i].tr == tr && tt_ev[i].blk + 1u < tt_bar_blk[2];
        if (k > 512u) k = 512u;
        tt_from(tr, 0, a, k);
        same &= k > 0 && tt_from(tr, tt_mark[1], b, k) == k && !memcmp(a, b, k * sizeof a[0]);
        if (mid) differ |= tt_from(tr, tt_mark[0], c, k) != k || memcmp(a, c, k * sizeof a[0]) != 0;
    }
    snprintf(res, cap, "%u %u", same, differ);
}

/* ROLL waits for the bar line: tracks 0..1's notes of bar 2 (from its line to bar 3's) as they are with the line
 * never turned: "same n" */
static void roll_wait_check(char *res, size_t cap)
{
    static uint32_t a[2][256], b[256];
    uint32_t i, n[2], same = 1, lo, hi, tr;
    const scen_t *none = 0, *back = 0;
    for (i = 0; i < NSCEN; i++) {
        if (!strcmp(SCEN[i].name, "seed_none_markov_turing")) none = &SCEN[i];
        if (!strcmp(SCEN[i].name, "seed_back_markov_turing")) back = &SCEN[i];
    }
    for (int run = 0; run < 2; run++) {
        tt_n = tt_blk = 0;
        scen_run(run ? back : none, 0, res, cap);
        lo = tt_bar_blk[2] - 2u, hi = tt_bar_blk[3] - 2u;   /* (a bar line's step: a block or two before the bar's) */
        for (tr = 0; tr < 2u; tr++) {
            uint32_t k = 0, *o = run ? b : a[tr];
            for (i = 0; i < tt_n && k < 256u; i++)
                if (tt_ev[i].tr == tr && tt_ev[i].blk >= lo && tt_ev[i].blk < hi) o[k++] = tt_ev[i].note | (uint32_t)tt_ev[i].vel << 8;
            if (!run) n[tr] = k;
            else same &= k == n[tr] && k > 0 && !memcmp(a[tr], b, k * sizeof b[0]);
        }
    }
    snprintf(res, cap, "%u %u", same, n[0] + n[1]);
}

/* ------------------------------------------------------------ NOTE LENGTH --- */
/* note_len.c on Euclid 4 of 16 at 1/16, 120 BPM: a hit every quarter (689 blocks). "ok": each note's length (the
 * median of the second bar's) as its mode says: STEP the GATE (half a 16th, 86 blocks), FIXED 1/8 an eighth (345)
 * whatever DIV, HIT up to the next hit (689), FIXED 1/1 with KEYS 1 the next hit ends it (689); FIXED 1/1 holds KEYS
 * notes at once (4, 2); STOP, MUTE and STEP again leave no note held */
static uint32_t nl_held(uint32_t tr)
{
    uint32_t n = 0;
    if (nl_active & (1u << tr))
        for (uint32_t k = 0; k < NL_KEYS; k++) n += nl_run[tr][k].on;
    return n;
}
static uint32_t nl_gates(uint32_t tr)
{
    uint32_t n = 0;
    for (uint32_t v = 0; v < NVOICE; v++) n += trk[tr].v[v].gate != 0;
    return n;
}
/* track 0 Euclid in mode / len / keys for `bars` bars: the median length of the notes starting in the last bar (0
 * none); *most: the most notes held at once */
static uint32_t nl_play(uint32_t mode, uint32_t len, uint32_t keys, uint32_t bars, uint32_t *most)
{
    uint32_t f, total, bar_len, i, k = 0, l[64];
    int32_t o[2 * CTL];
    host_tracks_init();
    tessera_reset();
    song.g[G_BPM] = 120;
    for (i = 0; i < NTRK; i++) tt_open[i] = -1;
    tt_n = tt_blk = 0;
    gen(0, TESS_ENG_EUCLID);
    tessera_param_set(0, TX_NL_MODE, (int32_t)mode);
    tessera_param_set(0, TX_NL_LEN, (int32_t)len);
    tessera_param_set(0, TX_NL_KEYS, (int32_t)keys);
    rng_state = 0x4E4Cu;
    bar_len = 4u * seq_beat_samples();
    total = bars * bar_len;
    transport_req = 1;
    *most = 0;
    for (f = 0; f < total + 4u * CTL; f += CTL) {
        if (f >= total) transport_req = 2;
        mix_block(o, CTL);
        tt_blk++;
        if (tt_open[0] >= 0 && !trk[0].seq_n && !nl_held(0)) tt_close(0);
        if (nl_held(0) > *most) *most = nl_held(0);
    }
    for (i = 0; i < tt_n; i++)                          /* (the last bar's, closed before the end) */
        if (tt_ev[i].tr == 0 && tt_ev[i].blk >= (bars - 1u) * bar_len / CTL && tt_ev[i].len && k < 64u)
            l[k++] = tt_ev[i].len;
    for (i = 1; i < k; i++)
        for (uint32_t j = i; j > 0 && l[j - 1] > l[j]; j--) { uint32_t x = l[j]; l[j] = l[j - 1]; l[j - 1] = x; }
    return k ? l[k / 2u] : 0;
}
static int nl_near(uint32_t got, uint32_t want) { return got + 2u >= want && got <= want + 2u; }
static void nl_check(char *res, size_t cap)
{
    uint32_t ok = 1, most, len;
    int32_t o[2 * CTL];
    len = nl_play(NL_STEP, 2, 4, 3, &most);
    ok &= nl_near(len, 86) && !most;
    printf("note length: STEP %u blocks, ", (unsigned)len);
    len = nl_play(NL_FIXED, 2, 4, 3, &most);
    ok &= nl_near(len, 345) && most == 1u;
    printf("FIXED 1/8 %u, ", (unsigned)len);
    len = nl_play(NL_HIT, 2, 4, 3, &most);
    ok &= nl_near(len, 689) && most == 1u;
    printf("HIT %u, ", (unsigned)len);
    len = nl_play(NL_FIXED, 5, 1, 3, &most);
    ok &= nl_near(len, 689) && most == 1u;
    printf("FIXED 1/1 KEYS 1 %u, ", (unsigned)len);
    nl_play(NL_FIXED, 5, 4, 3, &most);
    ok &= most == 4u;
    printf("KEYS 4 holds %u, ", (unsigned)most);
    nl_play(NL_FIXED, 5, 2, 3, &most);
    ok &= most == 2u;
    printf("KEYS 2 holds %u\n", (unsigned)most);
    printf("note length: after STOP %u held, %u gates\n", (unsigned)nl_held(0), (unsigned)nl_gates(0));
    ok &= !nl_held(0) && !nl_gates(0);                 /* (nl_play ends with STOP) */
    for (int how = 0; how < 2; how++) {                 /* MUTE, STEP again: nothing held from the next block */
        nl_play(NL_HIT, 2, 4, 1, &most);
        transport_req = 1;
        for (uint32_t f = 0; f < 800u; f++) mix_block(o, CTL);   /* (over a quarter: a hit) */
        printf("note length: %s: held %u (%u notes), ", how ? "STEP again" : "MUTE", (unsigned)nl_held(0), (unsigned)tt_n);
        ok &= nl_held(0) == 1u;
        if (how) tessera_param_set(0, TX_NL_MODE, NL_STEP);
        else trk[0].p[P_MUTE] = 1;
        mix_block(o, CTL);
        mix_block(o, CTL);
        printf("then %u, %u gates\n", (unsigned)nl_held(0), (unsigned)nl_gates(0));
        ok &= !nl_held(0) && !nl_gates(0);
        trk[0].p[P_MUTE] = 0;
    }
    snprintf(res, cap, "%u", ok);
}

/* ------------------------------------------------------------- TWEAK 2 --- */
/* The checks of tw2_run, each a bit of its result when it fails */
static const char *const TW2_CHECK[] = {
    "COND: each value (OFF 1:2 2:2 1:4 2:4 3:4 4:4 1:8 8:8 FIRST !FIRST) plays its bars of 8, every step in them",
    "COND with FORM on: FIRST plays the first bar of each section (AB, EVERY 2: bars 1, 3, 5, 7)",
    "LOGIC AND / OR / XOR / NOT with a lower track: the steps that play, step for step",
    "BRANCH COIN 100: every hit on track TO, the same notes at the same time, none here",
    "BRANCH TOGGLE 100: the hits alternate, the first on TO",
    "BRANCH LATCH 50: each bar's hits all on one side, both sides used, none lost",
    "BRANCH COIN 50: every hit on one side or the other, none lost, both used",
    "BRANCH to a muted track: its share not played, the rest here",
    "BRANCH: no note left sounding on the track TO after the stop",
    "BRANCH 0, LOGIC and COND OFF: the same notes as without TWEAK 2",
    "QUAD FWD: the row in order, a cell a step, its first cell louder",
    "QUAD BACK and PEND: the same cells, backwards and back and forth; HOLD alone: silent",
    "QUAD LINK to a HOLD track: it plays on the linking track's steps only, its row in order",
    "QUAD LINK: no note left sounding on the linked track after the stop",
    "QUAD EVOLVE with two QUAD rows at AMOUNT 100: their cells change places, the other rows kept",
};
#define TW2_N (sizeof TW2_CHECK / sizeof TW2_CHECK[0])
static uint32_t tw2_mode, tw2_a, tw2_b, tw2_c;          /* (tw2_setup's: which set-up, its values) */
static void tw2_setup(void)
{
    if (tw2_mode == 0u) {                               /* COND tw2_a on Euclid 16 of 16 */
        gen(0, TESS_ENG_EUCLID);
        knob(tessera_knob, 2, 12);
        turns(tessera_tweak2_knob, 3, (int32_t)tw2_a);
    } else if (tw2_mode == 1u) {                        /* COND FIRST, FORM AB of 2 bars, CONTRAST 0 */
        gen(0, TESS_ENG_EUCLID);
        knob(tessera_knob, 2, 12);
        turns(tessera_form_knob, 0, 1);
        knob(tessera_form_knob, 1, -30);
        turns(tessera_evolve_knob, 1, -1);
        turns(tessera_tweak2_knob, 3, TESS_COND_FIRST);
    } else if (tw2_mode == 2u) {                        /* LOGIC tw2_a: Euclid 5 (track 1), Euclid 7 rot 3 with T1 */
        gen(0, TESS_ENG_EUCLID);
        knob(tessera_knob, 2, 1);
        gen(1, TESS_ENG_EUCLID);
        knob(tessera_knob, 2, 3);
        knob(tessera_knob, 3, 3);
        if (tw2_a) turns(tessera_tweak2_knob, 2, (int32_t)(1u + (tw2_a - 1u) * 3u));
    } else if (tw2_mode == 4u) {                        /* QUAD row 1 5 3 10 on track 1, DIR tw2_a */
        static const uint8_t ROW[4] = {1, 5, 3, 10};
        gen(0, TESS_ENG_QUAD);
        memcpy(tessera.quad[0].cell, ROW, 4);
        turns(tessera_knob, 1, (int32_t)tw2_a);
    } else if (tw2_mode == 5u) {                        /* QUAD FWD on track 1 at 1/8 LINK T2, track 2 QUAD HOLD */
        static const uint8_t ROW[4] = {1, 5, 3, 10}, ROW2[4] = {8, 12, 2, 6};
        gen(1, TESS_ENG_QUAD);
        memcpy(tessera.quad[1].cell, ROW2, 4);
        turns(tessera_knob, 1, GEN_QD_HOLD);
        gen(0, TESS_ENG_QUAD);
        memcpy(tessera.quad[0].cell, ROW, 4);
        turns(tessera_knob, 3, 1);
        trk[0].p[P_SDIV] = (int16_t)(trk[1].p[P_SDIV] - 1);   /* (a division slower than track 2's own) */
    } else {                                            /* BRANCH tw2_b % to T2 (STEP), mode tw2_a; tw2_c: T2 muted */
        gen(0, TESS_ENG_EUCLID);
        knob(tessera_knob, 2, 3);
        knob(tessera_tweak2_knob, 0, (int32_t)tw2_b / 2);
        if (tw2_b) turns(tessera_tweak2_knob, 1, (int32_t)(1u + tw2_a));
        trk[1].p[P_MUTE] = (int16_t)tw2_c;
    }
}
static const scen_t TW2_SC = {"tweak2", 8, 0x2001u, tw2_setup, 0};
/* the scenario as set: its notes per track (blk << 16 | note << 8 | vel, at most 512 a track) */
static uint32_t tw2_ev[2][512], tw2_n[2], tw2_held;
static void tw2_go(uint32_t mode, uint32_t a, uint32_t b, uint32_t c)
{
    char r[96];
    tw2_mode = mode, tw2_a = a, tw2_b = b, tw2_c = c;
    tt_n = tt_blk = tt_lost = 0;
    scen_run(&TW2_SC, 0, r, sizeof r);
    tw2_n[0] = tw2_n[1] = 0;
    for (uint32_t i = 0; i < tt_n; i++)
        if (tt_ev[i].tr < 2u && tw2_n[tt_ev[i].tr] < 512u)
            tw2_ev[tt_ev[i].tr][tw2_n[tt_ev[i].tr]++] = tt_ev[i].blk << 16 | (uint32_t)tt_ev[i].note << 8 | tt_ev[i].vel;
    tw2_held = trk[1].seq_n;
    for (uint32_t v = 0; v < NVOICE; v++) tw2_held += trk[1].v[v].gate != 0;
}
static uint32_t tw2_bar(uint32_t ev)                    /* the bar an event is in, to the nearest half step (the
                                                         * blocks round the steps: a bar line's step falls early) */
{
    uint32_t b = 0, bar = tt_bar_blk[1] - tt_bar_blk[0], half = bar / 32u;
    while (b + 1u < 64u && (ev >> 16) + half >= (tt_bar_blk[b + 1u] ? tt_bar_blk[b + 1u] : tt_bar_blk[b] + bar)) b++;
    return b;                                           /* (past the scenario's last bar line: the bars after it) */
}
static int tw2_has(uint32_t tr, uint32_t blk)
{
    for (uint32_t i = 0; i < tw2_n[tr]; i++) if (tw2_ev[tr][i] >> 16 == blk) return 1;
    return 0;
}
static void tw2_run(char *res, size_t cap)
{
    static const uint8_t PLAYS[TESS_COND_COUNT] = {0xFF, 0x55, 0xAA, 0x11, 0x22, 0x44, 0x88, 0x01, 0x80, 0x01, 0xFE};
    static uint32_t a[512], b[512], na, nb;
    uint32_t fail = 0, c, i, k;
    for (c = 0; c < TESS_COND_COUNT; c++) {             /* COND */
        uint32_t per[8] = {0};
        tw2_go(0, c, 0, 0);
        for (i = 0; i < tw2_n[0]; i++) if (tw2_bar(tw2_ev[0][i]) < 8u) per[tw2_bar(tw2_ev[0][i])]++;
        for (k = 0; k < 8u; k++) if (per[k] != ((PLAYS[c] >> k) & 1u ? 16u : 0u)) fail |= 1u << 0;
    }
    {
        uint32_t seen = 0;
        tw2_go(1, 0, 0, 0);
        for (i = 0; i < tw2_n[0]; i++) seen |= 1u << tw2_bar(tw2_ev[0][i]);
        if ((seen & 0xFFu) != 0x55u) fail |= 1u << 1;
    }
    tw2_go(2, 0, 0, 0);                                 /* LOGIC: OFF first, then each against it */
    na = tw2_n[0], nb = tw2_n[1];
    for (i = 0; i < na; i++) a[i] = tw2_ev[0][i] >> 16;
    for (i = 0; i < nb; i++) b[i] = tw2_ev[1][i] >> 16;
    if (!na || !nb) fail |= 1u << 2;
    for (uint32_t op = 1; op < TESS_LG_COUNT; op++) {
        uint32_t steps[1024], ns = 0, want = 0;
        tw2_go(2, op, 0, 0);
        for (i = 0; i < na; i++) steps[ns++] = a[i];
        for (i = 0; i < nb; i++) {
            uint32_t j;
            for (j = 0; j < na && a[j] != b[i]; j++) ;
            if (j == na) steps[ns++] = b[i];
        }
        for (i = 0; i < ns; i++) {                      /* every step either track hit: track 2 plays the op of both */
            int ha = 0, hb = 0, o;
            for (k = 0; k < na; k++) ha |= a[k] == steps[i];
            for (k = 0; k < nb; k++) hb |= b[k] == steps[i];
            o = op == TESS_LG_AND ? ha && hb : op == TESS_LG_OR ? ha || hb : op == TESS_LG_XOR ? ha != hb : hb && !ha;
            want += (uint32_t)o;
            if (tw2_has(1, steps[i]) != o) fail |= 1u << 2;
        }
        if (tw2_n[1] != want) fail |= 1u << 2;
    }
    tw2_go(3, 0, 0, 0);                                 /* BRANCH: off first */
    na = tw2_n[0];
    memcpy(a, tw2_ev[0], na * sizeof a[0]);
    if (!na || tw2_n[1]) fail |= 1u << 3;
    if (tw2_held) fail |= 1u << 8;
    tw2_go(3, TESS_BR_COIN, 100, 0);
    if (tw2_n[0] || tw2_n[1] != na || memcmp(a, tw2_ev[1], na * sizeof a[0])) fail |= 1u << 3;
    if (tw2_held) fail |= 1u << 8;
    tw2_go(3, TESS_BR_TOGGLE, 100, 0);
    if (tw2_n[0] + tw2_n[1] != na) fail |= 1u << 4;
    for (i = 0; i < na; i++)                            /* hit i: on track 2 when i is even */
        if (tw2_has(i % 2u ? 0u : 1u, a[i] >> 16) != 1) fail |= 1u << 4;
    if (tw2_held) fail |= 1u << 8;
    {
        uint32_t side[8], used = 0;
        tw2_go(3, TESS_BR_LATCH, 50, 0);
        for (k = 0; k < 8u; k++) side[k] = 0;
        for (uint32_t tr = 0; tr < 2u; tr++)
            for (i = 0; i < tw2_n[tr]; i++) {
                uint32_t bb = tw2_bar(tw2_ev[tr][i]) & 7u;
                side[bb] |= 1u << tr;
                used |= 1u << tr;
            }
        for (k = 0; k < 8u; k++) if (side[k] == 3u) fail |= 1u << 5;
        if (used != 3u || tw2_n[0] + tw2_n[1] != na) fail |= 1u << 5;
        if (tw2_held) fail |= 1u << 8;
    }
    tw2_go(3, TESS_BR_COIN, 50, 0);
    if (tw2_n[0] + tw2_n[1] != na || !tw2_n[0] || !tw2_n[1]) fail |= 1u << 6;
    for (i = 0; i < na; i++)
        if (tw2_has(0, a[i] >> 16) + tw2_has(1, a[i] >> 16) != 1) fail |= 1u << 6;
    if (tw2_held) fail |= 1u << 8;
    nb = tw2_n[0];
    tw2_go(3, TESS_BR_COIN, 50, 1);                     /* the same tosses, track 2 muted */
    if (tw2_n[1] || tw2_n[0] != nb) fail |= 1u << 7;
    tw2_go(3, TESS_BR_COIN, 0, 0);                      /* BRANCH 0 with TO set: as off */
    if (tw2_n[0] != na || tw2_n[1] || memcmp(a, tw2_ev[0], na * sizeof a[0])) fail |= 1u << 9;
    tw2_go(4, GEN_QD_FWD, 0, 0);                        /* QUAD */
    na = tw2_n[0];
    memcpy(a, tw2_ev[0], na * sizeof a[0]);
    if (na < 128u || (a[0] & 0xFFu) <= (a[1] & 0xFFu)) fail |= 1u << 10;   /* (8 bars, and the step on the last line) */
    for (i = 0; i + 4u < na; i++) if (((a[i] ^ a[i + 4u]) & 0xFFFFu) || (i % 4u && (a[i] & 0xFFu) != (a[1] & 0xFFu))) fail |= 1u << 10;
    if (((a[0] ^ a[1]) & 0xFF00u) == 0u && ((a[1] ^ a[2]) & 0xFF00u) == 0u) fail |= 1u << 10;
    tw2_go(4, GEN_QD_BACK, 0, 0);
    if (tw2_n[0] != na) fail |= 1u << 11;
    for (i = 0; i < tw2_n[0]; i++) if (((tw2_ev[0][i] ^ a[3u - i % 4u]) & 0xFF00u)) fail |= 1u << 11;
    tw2_go(4, GEN_QD_PEND, 0, 0);                       /* cells 1 2 3 4 3 2, 1 2 ..: 6 steps a turn */
    if (tw2_n[0] != na) fail |= 1u << 11;
    for (i = 0; i < tw2_n[0]; i++) {
        static const uint8_t PEND[6] = {0, 1, 2, 3, 2, 1};
        if ((tw2_ev[0][i] ^ a[PEND[i % 6u]]) & 0xFF00u) fail |= 1u << 11;
    }
    tw2_go(4, GEN_QD_HOLD, 0, 0);
    if (tw2_n[0]) fail |= 1u << 11;
    tw2_go(5, 0, 0, 0);                                 /* LINK */
    if (!tw2_n[0] || tw2_n[1] != tw2_n[0]) fail |= 1u << 12;
    for (i = 0; i < tw2_n[1]; i++) {
        if (tw2_ev[1][i] >> 16 != tw2_ev[0][i] >> 16) fail |= 1u << 12;
        if (i >= 4u && (tw2_ev[1][i] ^ tw2_ev[1][i - 4u]) & 0xFF00u) fail |= 1u << 12;
    }
    if (!((tw2_ev[1][0] ^ tw2_ev[1][1]) & 0xFF00u)) fail |= 1u << 12;
    if (tw2_held) fail |= 1u << 13;
    {   /* EVOLVE: tracks 1 and 3 in QUAD, AMOUNT 100: every column turns, so the two rows swap; rows 2 and 4 kept */
        static const uint8_t R[4][4] = {{1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 0}, {12, 13, 14, 15}};
        host_tracks_init();
        tessera_reset();
        gen(0, TESS_ENG_QUAD);
        gen(2, TESS_ENG_QUAD);
        for (k = 0; k < 4u; k++) memcpy(tessera.quad[k].cell, R[k], 4);
        tessera.evolve[0].amount = 100;
        tessera_mutate(0);
        if (memcmp(tessera.quad[0].cell, R[2], 4) || memcmp(tessera.quad[2].cell, R[0], 4) ||
            memcmp(tessera.quad[1].cell, R[1], 4) || memcmp(tessera.quad[3].cell, R[3], 4)) fail |= 1u << 14;
    }
    snprintf(res, cap, "%u", fail);
}

/* ------------------------------------------------------------ SLOTH, OCHD --- */
static const char *const ORG_CHECK[] = {
    "SLOTH: 6 hours at SPEED 100 (4 days at 50): every circuit in its range, swinging over most of it",
    "SLOTH: APATHY turns about once a minute at SPEED 50, twice as often at 62.5",
    "OCHD: 8 rates, slowest first, 64 s a turn for OCHD 1 at RATE 50; bent triangles within -1..1",
    "PLAY: each track's from its SEED (the same again after PLAY), another SEED elsewhere",
};
#define ORG_N (sizeof ORG_CHECK / sizeof ORG_CHECK[0])
static uint32_t org_turns(uint32_t tr, uint32_t k, uint32_t blocks)   /* upward zero crossings of output k */
{
    uint32_t n = 0;
    int32_t was = organic_out(tr, k);
    for (uint32_t i = 0; i < blocks; i++) {
        int32_t v;
        organic_tick();
        v = organic_out(tr, k);
        n += was < 0 && v >= 0;
        was = v;
    }
    return n;
}
static void org_run(char *res, size_t cap)
{
    uint32_t fail = 0, tr, k, i, n1, n2;
    const uint32_t BPS = 1378u;
    int32_t lo[3] = {32767, 32767, 32767}, hi[3] = {-32767, -32767, -32767}, a[11];
    host_tracks_init();
    tessera_reset();
    for (tr = 0; tr < TESSERA_TRACKS; tr++) { tessera.seed[tr] = (uint16_t)(1u + tr * 37u); tessera.org_speed[tr] = 100; }
    organic_play();
    organic_tick();
    for (i = 0; i < 6u * 3600u * BPS; i++) {
        organic_tick();
        for (k = 0; k < 3u; k++) {
            float x = organic[1].x[k];
            int32_t v = organic_out(1, k);
            if (!(x > -2.6f && x < 1.4f)) fail |= 1u << 0;
            lo[k] = v < lo[k] ? v : lo[k], hi[k] = v > hi[k] ? v : hi[k];
        }
    }
    for (k = 0; k < 3u; k++) if (lo[k] > -25000 || hi[k] < 15000) fail |= 1u << 0;
    tessera.org_speed[0] = 50;
    n1 = org_turns(0, 1, 30u * 60u * BPS);           /* 30 minutes of APATHY */
    tessera.org_speed[0] = 62;                       /* (62.5: 62 is 2^0.96) */
    n2 = org_turns(0, 1, 30u * 60u * BPS);
    if (n1 < 20u || n1 > 45u || n2 * 10u < n1 * 17u || n2 * 10u > n1 * 23u) fail |= 1u << 1;
    tessera.org_rate[0] = 50;
    n1 = org_turns(0, 3, 640u * BPS);                /* OCHD 1: 10 turns in 640 s */
    if (n1 < 9u || n1 > 11u) fail |= 1u << 2;
    for (k = 4; k < 11u; k++) {
        uint32_t m = org_turns(0, k, 64u * BPS);
        if (m <= n1 / 10u) fail |= 1u << 2;
        n1 = m * 10u;
    }
    for (i = 0; i < 100000u; i++) {
        organic_tick();
        for (k = 3; k < 11u; k++) if (organic_out(0, k) < -32767 || organic_out(0, k) > 32767) fail |= 1u << 2;
    }
    organic_play();
    for (i = 0; i < 5000u; i++) organic_tick();
    for (k = 0; k < 11u; k++) a[k] = organic_out(2, k);
    organic_play();
    for (i = 0; i < 5000u; i++) organic_tick();
    for (k = 0; k < 11u; k++) if (organic_out(2, k) != a[k]) fail |= 1u << 3;
    for (k = 0, n1 = 0; k < 11u; k++) n1 += organic_out(3, k) != a[k];
    if (n1 < 9u) fail |= 1u << 3;
    snprintf(res, cap, "%u", fail);
}

/* --------------------------------------------------------- saved block --- */
static uint32_t pack_hash(const uint8_t *p)
{
    uint32_t h = 2166136261u;
    for (uint32_t i = 0; i < TESSERA_PK_BYTES; i++) {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

/* "hash ok": the block of a project set up as s_all + s_tweak (track 2 FOLLOW) + FORM's FLOURISH + TWEAK 2 (track 3); ok: it loads
 * back to the same block, and a block with any value out of range is refused, the live state untouched */
static void pack_run(char *res, size_t cap)
{
    static uint8_t a[TESSERA_PK_BYTES], b[TESSERA_PK_BYTES], c[TESSERA_PK_BYTES];
    uint32_t ha, ok = 1;
    host_tracks_init();
    tessera_reset();
    s_all();
    sel(1);
    turns(tessera_tweak_knob, 3, 1);
    knob(tessera_tweak_knob, 0, -4);
    sel(2);
    knob(tessera_form_knob, 3, 2);
    knob(tessera_form_knob, 1, -5);
    knob(tessera_tweak2_knob, 0, 20);                 /* track 3: BRANCH 40 TO T4 LATCH, LOGIC XOR T2, COND 3:4 */
    turns(tessera_tweak2_knob, 1, 9);
    turns(tessera_tweak2_knob, 2, 8);
    turns(tessera_tweak2_knob, 3, 5);
    tessera_param_set(3, TX_QD_DIR, GEN_QD_PEND);     /* track 4: QUAD PEND, PROB 70, LINK T1 */
    tessera_param_set(3, TX_QD_PROB, 70);
    tessera_param_set(3, TX_QD_LINK, 1);
    tessera_pack(a);
    ha = pack_hash(a);
    tessera_reset();
    ok &= tessera_unpack(a) == 1;
    tessera_pack(b);
    ok &= !memcmp(a, b, sizeof a);
    memcpy(c, a, sizeof c);
    c[1u * (TESSERA_PK_BYTES / TESSERA_TRACKS) + 0u] = TESS_ENG_COUNT;   /* track 2's engine out of range */
    ok &= tessera_unpack(c) == 0;
    tessera_pack(b);
    ok &= !memcmp(a, b, sizeof a);                   /* refused whole: nothing changed */
    {   /* a seed of 1.1.3 to 1.1.5 (up to 9999) loads as it was, ROLL from it lands in 1..128; over 9999 refused */
        const uint32_t at = 0u * (TESSERA_PK_BYTES / TESSERA_TRACKS) + TESSERA_PK_BYTES_V1 / TESSERA_TRACKS;   /* (after version 1's) */
        memcpy(c, a, sizeof c);
        c[at] = 9999u & 0xFFu, c[at + 1u] = 9999u >> 8;
        ok &= tessera_unpack(c) == 1 && tessera.seed[0] == 9999u;
        sel(0);
        song.playing = 0;
        tessera_seed_turn(0, 1);
        ok &= tessera.seed[0] >= 1u && tessera.seed[0] <= TESS_SEED_MAX;
        tessera_seed_turn(0, -500);                  /* ROLL stops at its ends, no wrapping */
        tessera_seed_turn(0, -1);
        ok &= tessera.seed[0] == 1u;
        tessera_seed_turn(0, 500);
        tessera_seed_turn(0, 1);
        ok &= tessera.seed[0] == TESS_SEED_MAX;
        c[at] = 10000u & 0xFFu, c[at + 1u] = 10000u >> 8;
        ok &= tessera_unpack(c) == 0;
        tessera_unpack(a);
    }
    {   /* a version 1 block (before SEED: each track 2 bytes shorter) loads every setting, with a seed of its own */
        static uint8_t v1[TESSERA_PK_BYTES_V1], v1b[TESSERA_PK_BYTES_V1];
        const uint32_t n2 = TESSERA_PK_BYTES / TESSERA_TRACKS, n1 = TESSERA_PK_BYTES_V1 / TESSERA_TRACKS;
        for (uint32_t tr = 0; tr < TESSERA_TRACKS; tr++) memcpy(v1 + tr * n1, a + tr * n2, n1);
        tessera_reset();
        ok &= tessera_unpack_ver(1u, v1) == 1;
        tessera_pack(b);
        for (uint32_t tr = 0; tr < TESSERA_TRACKS; tr++) {
            memcpy(v1b + tr * n1, b + tr * n2, n1);
            ok &= tessera.mode[tr] == TESS_ENG_OFF ? tessera.seed[tr] == 0 : tessera.seed[tr] >= 1 && tessera.seed[tr] <= TESS_SEED_MAX;
        }
        ok &= !memcmp(v1, v1b, sizeof v1);            /* every version 1 setting as it was */
        tessera_reset();
        ok &= tessera_unpack_ver(1u, v1) == 1;
        tessera_pack(c);
        ok &= !memcmp(b, c, sizeof c);                /* the same block: the same seeds */
    }
    {   /* a version 2 block (before GRIDS: each track 4 bytes shorter) loads every setting, Grids' a fresh track's;
         * Grids' DENSITY or CHAOS out of range is refused */
        static uint8_t v2[TESSERA_PK_BYTES_V2], v2b[TESSERA_PK_BYTES_V2];
        const uint32_t n3 = TESSERA_PK_BYTES / TESSERA_TRACKS, n2 = TESSERA_PK_BYTES_V2 / TESSERA_TRACKS;
        for (uint32_t tr = 0; tr < TESSERA_TRACKS; tr++) memcpy(v2 + tr * n2, a + tr * n3, n2);
        tessera_reset();
        ok &= tessera_unpack_ver(2u, v2) == 1;
        tessera_pack(b);
        for (uint32_t tr = 0; tr < TESSERA_TRACKS; tr++) {
            memcpy(v2b + tr * n2, b + tr * n3, n2);
            ok &= tessera.grids[tr].x == 128u && tessera.grids[tr].y == 128u && tessera.grids[tr].dens == 50u &&
                  !tessera.grids[tr].chaos;
        }
        ok &= !memcmp(v2, v2b, sizeof v2);            /* every version 2 setting as it was */
        memcpy(c, a, sizeof c);
        c[2u * n3 + n2 + 2u] = 101u;                  /* track 3's Grids DENSITY */
        ok &= tessera_unpack(c) == 0;
        memcpy(c, a, sizeof c);
        c[n2 + 3u] = 101u;                            /* track 1's Grids CHAOS */
        ok &= tessera_unpack(c) == 0;
        tessera_unpack(a);
    }
    {   /* a version 3 block (before BOUNCE, AWAKE, POLY: each track 29 bytes shorter) loads every setting, theirs a fresh
         * track's with Awake's values and Poly's pitches from the track's seed (as ROLL gives them); each new setting
         * out of range is refused */
        static uint8_t v3[TESSERA_PK_BYTES_V3], v3b[TESSERA_PK_BYTES_V3];
        static const uint8_t BAD[][2] = {{0, 0}, {0, 65}, {1, 91}, {2, 0}, {2, 6}, {3, 0}, {4, 17}, {5, 101}, {6, 0x09},
                                         {13, 0x90}, {14, 0x08}, {21, 0x80}, {22, 17},
                                         {28, 0}, {28, 4}, {28, 0x10}};   /* (Bounce 0..2: BALLS 6 past FALL; Awake 3..21, Poly 22..28) */
        const uint32_t n4 = TESSERA_PK_BYTES / TESSERA_TRACKS, n3 = TESSERA_PK_BYTES_V3 / TESSERA_TRACKS;
        for (uint32_t tr = 0; tr < TESSERA_TRACKS; tr++) memcpy(v3 + tr * n3, a + tr * n4, n3);
        tessera_reset();
        ok &= tessera_unpack_ver(3u, v3) == 1;
        tessera_pack(b);
        for (uint32_t tr = 0; tr < TESSERA_TRACKS; tr++) {
            gen_awake_t aw;
            gen_poly_t po;
            memset(&aw, 0, sizeof aw);
            memset(&po, 0, sizeof po);
            po.span = 1;
            gen_awake_fill(&aw, tessera.seed[tr]);
            gen_poly_fill(&po, tessera.seed[tr]);
            memcpy(v3b + tr * n3, b + tr * n4, n3);
            ok &= tessera.bounce[tr].speed == 24u && tessera.bounce[tr].angle == 37u && tessera.bounce[tr].balls == 1u;
            ok &= tessera.awake[tr].len_a == 8u && tessera.awake[tr].len_b == 7u && tessera.awake[tr].prob == 100u &&
                  !memcmp(aw.a, tessera.awake[tr].a, 8) && !memcmp(aw.b, tessera.awake[tr].b, 8);
            ok &= tessera.poly[tr].div[0] == 4u && tessera.poly[tr].div[1] == 3u && !tessera.poly[tr].div[2] &&
                  !tessera.poly[tr].div[3] && tessera.poly[tr].span == 1u && !memcmp(po.pitch, tessera.poly[tr].pitch, 4);
        }
        ok &= !memcmp(v3, v3b, sizeof v3);            /* every version 3 setting as it was */
        for (uint32_t k = 0; k < sizeof BAD / sizeof BAD[0]; k++) {
            memcpy(c, a, sizeof c);
            c[3u * n4 + n3 + BAD[k][0]] = BAD[k][1];  /* (track 4's) */
            ok &= tessera_unpack(c) == 0;
        }
        tessera_unpack(a);
    }
    {   /* BOUNCE's FALL (BALLS 5): its round trip, no format step */
        tessera_param_set(2, TX_BNC_BALLS, GEN_BOUNCE_FALL);
        tessera_pack(b);
        tessera_reset();
        ok &= tessera_unpack(b) == 1 && tessera.bounce[2].balls == GEN_BOUNCE_FALL;
        tessera_unpack(a);
    }
    {   /* SLOTH SPEED and OCHD RATE: their round trip */
        tessera_param_set(3, TX_ORG_SPEED, 80);
        tessera_param_set(3, TX_ORG_RATE, 7);
        tessera_pack(b);
        tessera_reset();
        ok &= tessera_unpack(b) == 1 && tessera.org_speed[3] == 80u && tessera.org_rate[3] == 7u;
        tessera_unpack(a);
    }
    {   /* a version 4 block (before BRANCH, LOGIC, COND, SLOTH, OCHD: each track 8 bytes shorter) loads every setting, theirs off;
         * each new setting out of range is refused */
        static uint8_t v4[TESSERA_PK_BYTES_V4], v4b[TESSERA_PK_BYTES_V4];
        static const uint8_t BAD[][2] = {{0, 101}, {1, 5}, {2, 3}, {3, 5}, {4, 5}, {5, 11}, {6, 101}, {7, 101}};
        const uint32_t n5 = TESSERA_PK_BYTES / TESSERA_TRACKS, n4 = TESSERA_PK_BYTES_V4 / TESSERA_TRACKS;
        for (uint32_t tr = 0; tr < TESSERA_TRACKS; tr++) memcpy(v4 + tr * n4, a + tr * n5, n4);
        ok &= tessera.evolve[2].branch == 40u && tessera.evolve[2].br_to == 4u && tessera.evolve[2].br_mode == TESS_BR_LATCH &&
              tessera.evolve[2].logic == TESS_LG_XOR && tessera.evolve[2].lg_with == 2u && tessera.evolve[2].cond == TESS_COND_3_4;
        tessera_reset();
        ok &= tessera_unpack_ver(4u, v4) == 1;
        tessera_pack(b);
        for (uint32_t tr = 0; tr < TESSERA_TRACKS; tr++) {
            const tessera_evolve_t *ev = &tessera.evolve[tr];
            memcpy(v4b + tr * n4, b + tr * n5, n4);
            ok &= !ev->branch && !ev->br_to && !ev->br_mode && !ev->logic && !ev->lg_with && !ev->cond;
            ok &= tessera.org_speed[tr] == 50u && tessera.org_rate[tr] == 50u;
        }
        ok &= !memcmp(v4, v4b, sizeof v4);            /* every version 4 setting as it was */
        for (uint32_t k = 0; k < sizeof BAD / sizeof BAD[0]; k++) {
            memcpy(c, a, sizeof c);
            c[1u * n5 + n4 + BAD[k][0]] = BAD[k][1];  /* (track 2's) */
            ok &= tessera_unpack(c) == 0;
        }
        tessera_unpack(a);
    }
    {   /* a version 5 block (before QUAD: each track 7 bytes shorter) loads every setting; QUAD's row the seed's, FWD,
         * every cell, no LINK; each QUAD setting out of range is refused */
        static uint8_t v5[TESSERA_PK_BYTES_V5], v5b[TESSERA_PK_BYTES_V5];
        static const uint8_t BAD[][2] = {{0, 22}, {3, 22}, {4, GEN_QD_COUNT}, {5, 101}, {6, 5}};
        const uint32_t n6 = TESSERA_PK_BYTES / TESSERA_TRACKS, n5 = TESSERA_PK_BYTES_V5 / TESSERA_TRACKS;
        for (uint32_t tr = 0; tr < TESSERA_TRACKS; tr++) memcpy(v5 + tr * n5, a + tr * n6, n5);
        ok &= tessera.quad[3].dir == GEN_QD_PEND && tessera.quad[3].prob == 70u && tessera.quad[3].link == 1u;
        tessera_reset();
        ok &= tessera_unpack_ver(5u, v5) == 1;
        tessera_pack(b);
        for (uint32_t tr = 0; tr < TESSERA_TRACKS; tr++) {
            tessera_quad_t q;
            memset(&q, 0, sizeof q);
            gen_quad_fill(&q, tessera.seed[tr]);
            memcpy(v5b + tr * n5, b + tr * n6, n5);
            ok &= !tessera.quad[tr].dir && tessera.quad[tr].prob == 100u && !tessera.quad[tr].link;
            ok &= !memcmp(q.cell, tessera.quad[tr].cell, 4);
        }
        ok &= !memcmp(v5, v5b, sizeof v5);            /* every version 5 setting as it was */
        for (uint32_t k = 0; k < sizeof BAD / sizeof BAD[0]; k++) {
            memcpy(c, a, sizeof c);
            c[1u * n6 + n5 + BAD[k][0]] = BAD[k][1];  /* (track 2's) */
            ok &= tessera_unpack(c) == 0;
        }
        memcpy(c, a, sizeof c);
        c[1u * n6 + n5 + 6u] = 2;                     /* track 2 LINKed to itself: loads as no LINK */
        ok &= tessera_unpack(c) == 1 && !tessera.quad[1].link;
        tessera_unpack(a);
    }
    {   /* a version 6 block (before NOTE LENGTH: each track 3 bytes shorter) loads every setting, NOTE LENGTH STEP;
         * a NOTE LENGTH out of range is refused; FIXED 1/2 KEYS 3 round trips */
        static uint8_t v6[TESSERA_PK_BYTES_V6], v6b[TESSERA_PK_BYTES_V6];
        static const uint8_t BAD[3] = {NL_COUNT, 6, 0};
        const uint32_t n7 = TESSERA_PK_BYTES / TESSERA_TRACKS, n6 = TESSERA_PK_BYTES_V6 / TESSERA_TRACKS;
        for (uint32_t tr = 0; tr < TESSERA_TRACKS; tr++) memcpy(v6 + tr * n6, a + tr * n7, n6);
        tessera_reset();
        ok &= tessera_unpack_ver(6u, v6) == 1;
        tessera_pack(b);
        for (uint32_t tr = 0; tr < TESSERA_TRACKS; tr++) {
            memcpy(v6b + tr * n6, b + tr * n7, n6);
            ok &= tessera.nl_mode[tr] == NL_STEP && tessera.nl_len[tr] == 2u && tessera.nl_keys[tr] == NL_KEYS;
        }
        ok &= !memcmp(v6, v6b, sizeof v6);            /* every version 6 setting as it was */
        for (uint32_t k = 0; k < 3u; k++) {
            memcpy(c, a, sizeof c);
            c[3u * n7 + n6 + k] = BAD[k];             /* track 4's MODE, LEN, KEYS */
            ok &= tessera_unpack(c) == 0;
        }
        tessera_unpack(a);
        tessera_param_set(2, TX_NL_MODE, NL_FIXED);
        tessera_param_set(2, TX_NL_LEN, 4);
        tessera_param_set(2, TX_NL_KEYS, 3);
        tessera_pack(b);
        tessera_reset();
        ok &= tessera_unpack(b) == 1 && tessera.nl_mode[2] == NL_FIXED && tessera.nl_len[2] == 4u &&
              tessera.nl_keys[2] == 3u;
        tessera_unpack(a);
    }
    {   /* POLY over 3 octaves: pitches above 15 survive the round trip (their fifth bits in SPAN's byte) */
        static const uint8_t P[4] = {20, 16, 5, 15};
        tessera.poly[3].span = 3;
        memcpy(tessera.poly[3].pitch, P, 4);
        tessera_pack(b);
        tessera_reset();
        ok &= tessera_unpack(b) == 1 && tessera.poly[3].span == 3u && !memcmp(tessera.poly[3].pitch, P, 4);
        tessera_unpack(a);
    }
    snprintf(res, cap, "0x%08x %u", ha, ok);
}

/* ---------------------------------------------------------------- main --- */
static int check(const char *what, int ok)
{
    printf("tessera: %-86s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

/* the baseline's line for name: "<hash> <notes>" into want, 0 = not there */
static int base_get(const char *file, const char *name, char *want, size_t cap)
{
    char line[256], key[128];
    FILE *fp = fopen(file, "r");
    if (!fp) return 0;
    while (fgets(line, sizeof line, fp)) {
        char rest[128];
        if (line[0] == '#' || sscanf(line, "%127s %127[^\n]", key, rest) != 2) continue;
        if (!strcmp(key, name)) {
            snprintf(want, cap, "%s", rest);
            fclose(fp);
            return 1;
        }
    }
    fclose(fp);
    return 0;
}

int main(int argc, char **argv)
{
    const char *base = argc > 1 ? argv[1] : "tests/tessera_baseline.txt", *logdir = argc > 2 ? argv[2] : 0;
    int update = getenv("TESSERA_UPDATE") && atoi(getenv("TESSERA_UPDATE")), bad = 0;
    char res[96], again[96], line[160], want[128], got[128];
    FILE *out = 0;
    uint32_t i;
    if (update && !(out = fopen(base, "w"))) {
        printf("tessera: cannot write %s\n", base);
        return 1;
    }
    if (out)
        fprintf(out, "# Tessera generators: what they play, per scenario of tests/tessera_test.c: hash of the note log, "
                     "notes.\n# Rewrite (TESSERA_UPDATE=1) only after an intended change of what they play.\n");
    for (i = 0; i < NSCEN; i++) {
        unsigned n = 0, h = 0, held = 1, lost = 1, st = 0;
        char key[96], stg[16];
        if (scen_fork(&SCEN[i], logdir, res, sizeof res) || sscanf(res, "%u 0x%x %u %u 0x%x", &n, &h, &held, &lost, &st) != 5) {
            snprintf(line, sizeof line, "%s: runs", SCEN[i].name);
            bad += check(line, 0);
            continue;
        }
        snprintf(got, sizeof got, "0x%08x %u", h, n);
        snprintf(line, sizeof line, "%s: no note held after the stop, log complete", SCEN[i].name);
        bad += check(line, !held && !lost && n > 0);
        snprintf(key, sizeof key, "%s.state", SCEN[i].name);
        snprintf(stg, sizeof stg, "0x%08x", st);
        if (out) {
            fprintf(out, "%s %s\n", SCEN[i].name, got);
            fprintf(out, "%s %s\n", key, stg);
        } else {
            int have = base_get(base, SCEN[i].name, want, sizeof want);
            snprintf(line, sizeof line, "%s: %u notes, %s", SCEN[i].name, n,
                     !have ? "not in the baseline (TESSERA_UPDATE=1)" : strcmp(want, got) ? "CHANGED" : "as the baseline");
            bad += check(line, have && !strcmp(want, got));
            if (have && strcmp(want, got))
                printf("tessera:   baseline %s, now %s\n", want, got);
            have = base_get(base, key, want, sizeof want);
            snprintf(line, sizeof line, "%s: the state after the stop %s", SCEN[i].name,
                     !have ? "not in the baseline (TESSERA_UPDATE=1)" : strcmp(want, stg) ? "CHANGED" : "as the baseline");
            bad += check(line, have && !strcmp(want, stg));
            if (have && strcmp(want, stg))
                printf("tessera:   baseline %s, now %s\n", want, stg);
        }
    }
    for (i = 0; i < NSCEN && strcmp(SCEN[i].name, "transport_stop_start"); i++)
        ;
    if (i < NSCEN && !scen_fork(&SCEN[i], 0, res, sizeof res) && !scen_fork(&SCEN[i], 0, again, sizeof again))
        bad += check("the same scenario twice: the same notes", !strcmp(res, again));
    else
        bad += check("the same scenario twice: the same notes", 0);
    {   /* FOLLOW of a STEP melody, note by note */
        unsigned n2 = 0, wrong = 1;
        pid_t pid;
        int fd[2];
        if (!pipe(fd) && (pid = fork()) >= 0) {
            if (!pid) {
                close(fd[0]);
                follow_run(res, sizeof res);
                if (write(fd[1], res, strlen(res) + 1u) < 0) _exit(1);
                _exit(0);
            }
            close(fd[1]);
            if (read(fd[0], res, sizeof res - 1u) > 0) sscanf(res, "%u %u", &n2, &wrong);
            close(fd[0]);
            waitpid(pid, 0, 0);
        }
        snprintf(line, sizeof line, "FOLLOW a STEP melody: all %u notes a third above it (in C major)", n2);
        bad += check(line, n2 > 16u && !wrong);
    }
    for (int which = 0; which < 2; which++) {   /* SEED back, PLAY again: in a child, as the scenarios */
        unsigned same = 0, differ = 0;
        pid_t pid;
        int fd[2];
        if (!pipe(fd) && (pid = fork()) >= 0) {
            if (!pid) {
                close(fd[0]);
                if (which == 0) seed_check("seed_back_markov_turing", 2, 1, res, sizeof res);
                else seed_check("replay_markov", 1, 0, res, sizeof res);
                if (write(fd[1], res, strlen(res) + 1u) < 0) _exit(1);
                _exit(0);
            }
            close(fd[1]);
            if (read(fd[0], res, sizeof res - 1u) > 0) sscanf(res, "%u %u", &same, &differ);
            close(fd[0]);
            waitpid(pid, 0, 0);
        }
        bad += check(which == 0 ? "ROLL turned away and back: the same line again, note for note (Markov, Turing PROB 30)"
                                : "PLAY again (EVOLVE off): the same line from the top, note for note (Markov)", same && differ);
        if (which == 0) bad += check("... and the other seed played another line meanwhile", differ);
    }
    {   /* TWEAK 2: COND, LOGIC, BRANCH */
        unsigned fail = ~0u;
        pid_t pid;
        int fd[2];
        if (!pipe(fd) && (pid = fork()) >= 0) {
            if (!pid) {
                close(fd[0]);
                tw2_run(res, sizeof res);
                if (write(fd[1], res, strlen(res) + 1u) < 0) _exit(1);
                _exit(0);
            }
            close(fd[1]);
            if (read(fd[0], res, sizeof res - 1u) > 0) sscanf(res, "%u", &fail);
            close(fd[0]);
            waitpid(pid, 0, 0);
        }
        for (i = 0; i < TW2_N; i++) bad += check(TW2_CHECK[i], !((fail >> i) & 1u));
    }
    {   /* NOTE LENGTH (note_len.c) */
        unsigned ok = 0;
        pid_t pid;
        int fd[2];
        fflush(stdout);
        if (!pipe(fd) && (pid = fork()) >= 0) {
            if (!pid) {
                close(fd[0]);
                nl_check(res, sizeof res);
                fflush(stdout);
                if (write(fd[1], res, strlen(res) + 1u) < 0) _exit(1);
                _exit(0);
            }
            close(fd[1]);
            if (read(fd[0], res, sizeof res - 1u) > 0) sscanf(res, "%u", &ok);
            close(fd[0]);
            waitpid(pid, 0, 0);
        }
        bad += check("NOTE LENGTH: STEP the GATE, FIXED its LEN, HIT to the next hit, KEYS held at once; STOP, MUTE, "
                     "STEP again release", ok);
    }
    {   /* SLOTH, OCHD */
        unsigned fail = ~0u;
        pid_t pid;
        int fd[2];
        if (!pipe(fd) && (pid = fork()) >= 0) {
            if (!pid) {
                close(fd[0]);
                org_run(res, sizeof res);
                if (write(fd[1], res, strlen(res) + 1u) < 0) _exit(1);
                _exit(0);
            }
            close(fd[1]);
            if (read(fd[0], res, sizeof res - 1u) > 0) sscanf(res, "%u", &fail);
            close(fd[0]);
            waitpid(pid, 0, 0);
        }
        for (i = 0; i < ORG_N; i++) bad += check(ORG_CHECK[i], !((fail >> i) & 1u));
    }
    {   /* ROLL mid-bar: the line playing until the bar line */
        unsigned same = 0, n = 0;
        pid_t pid;
        int fd[2];
        if (!pipe(fd) && (pid = fork()) >= 0) {
            if (!pid) {
                close(fd[0]);
                roll_wait_check(res, sizeof res);
                if (write(fd[1], res, strlen(res) + 1u) < 0) _exit(1);
                _exit(0);
            }
            close(fd[1]);
            if (read(fd[0], res, sizeof res - 1u) > 0) sscanf(res, "%u %u", &same, &n);
            close(fd[0]);
            waitpid(pid, 0, 0);
        }
        bad += check("ROLL turned mid-bar: the line playing goes on to the bar line, note for note", same && n > 0u);
    }
    {
        unsigned h = 0, ok = 0;
        pid_t pid;
        int fd[2];
        if (!pipe(fd) && (pid = fork()) >= 0) {
            if (!pid) {
                close(fd[0]);
                pack_run(res, sizeof res);
                if (write(fd[1], res, strlen(res) + 1u) < 0) _exit(1);
                _exit(0);
            }
            close(fd[1]);
            ssize_t r = read(fd[0], res, sizeof res - 1u);
            close(fd[0]);
            waitpid(pid, 0, 0);
            res[r > 0 ? r : 0] = 0;
            sscanf(res, "0x%x %u", &h, &ok);
        }
        bad += check("saved block: round trip, out-of-range block refused whole", ok == 1);
        snprintf(got, sizeof got, "0x%08x %u", h, (unsigned)TESSERA_PK_BYTES);
        if (out) {
            fprintf(out, "saved_block %s\n", got);
        } else {
            int have = base_get(base, "saved_block", want, sizeof want);
            bad += check(have && !strcmp(want, got) ? "saved block: the same bytes as the baseline"
                                                    : "saved block: CHANGED or not in the baseline", have && !strcmp(want, got));
        }
    }
    if (out) {
        fclose(out);
        printf("tessera: baseline written to %s\n", base);
    }
    return bad ? 1 : 0;
}
