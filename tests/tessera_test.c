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
            if (tt_open[tr] >= 0 && !trk[tr].seq_n) tt_close(tr);
    }
    transport_req = 2;
    for (f = 0; f < 4u * CTL; f += CTL) {
        mix_block(o, CTL);
        tt_blk++;
        for (tr = 0; tr < NTRK; tr++)
            if (tt_open[tr] >= 0 && !trk[tr].seq_n) tt_close(tr);
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

/* "hash ok": the block of a project set up as s_all + s_tweak (track 2 FOLLOW) + FORM's FLOURISH; ok: it loads
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
        static const uint8_t BAD[][2] = {{0, 0}, {0, 65}, {1, 91}, {2, 0}, {2, 5}, {3, 0}, {4, 17}, {5, 101}, {6, 0x09},
                                         {13, 0x90}, {14, 0x08}, {21, 0x80}, {22, 17},
                                         {28, 0}, {28, 4}, {28, 0x10}};   /* (Bounce 0..2, Awake 3..21, Poly 22..28) */
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
