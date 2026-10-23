/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* Tessera: the note generators of tracks 1-4 (Turing, Euclid, Markov, Morph, Grids, Bounce, Awake,
 * Poly, Quad, Constellation, Series), EVOLVE, FORM and TWEAK, clocked by the
 * sequencer (seq.c) in place of a track's steps; their state is saved with the project (the TESSERA part of its record, project_tsp.c). */
#include "tessera.h"

tessera_state_t tessera;
tessera_sv_t tess_sv __attribute__((section(".pool")));
tessera_an_t tess_an __attribute__((section(".pool")));
static void answer_tick(const track_t *t, uint32_t tr, const gen_clock_t *clk, gen_out_t *out);   /* (answer.c) */
static void answer_restart(uint32_t tr);
/* CHORD's BREATH, while a chord is held: each voice's steps to its next breath, the voices sitting out (bit i) */
#define TESS_BR_VOICES 4u
static struct {
    uint16_t left[TESSERA_TRACKS][TESS_BR_VOICES];
    uint8_t out[TESSERA_TRACKS];
} tess_br __attribute__((section(".pool")));

#include "organic.c"                                 /* SLOTH and OCHD: each track's drifting sources (cords) */
#include "energy.c"                                  /* ENERGY: the project's slow tide (ARC) and its RESTS */

/* The knob handlers (main loop) change several fields the sequencer reads in the audio interrupt (an engine change
 * releases the old note then clears the gate; SEED rewrites a register, pitches, a state): each runs whole, with the interrupt
 * held off, like project.c's loads. The sequencer runs in CPU0's audio interrupt, so this is enough. The host
 * build has no interrupt (hostsim.c defines these empty). */
#ifndef TESS_IRQ_OFF
#define TESS_IRQ_OFF() fm1_irq_off()
#define TESS_IRQ_ON() fm1_irq_on()
#endif
/* Entropy for a fresh seed: the hardware timer (sub-microsecond) at the moment a person first selects or plays a
 * generator, so every power-on differs. The host defines a fixed value: the tests stay repeatable. */
#ifndef TESS_ENTROPY
#define TESS_ENTROPY() (fm1_ticks() ^ fm1_ms * 2654435761u)
#endif

#include "note_len.c"                                /* NOTE LENGTH: the LENGTH page's held notes */

static void tess_param_set_body(uint32_t tr, uint32_t x, int32_t v);

/* SEED (EVOLVE page, ROLL): 1..TESS_SEED_MAX per track, 0 = not picked yet (the defaults). A seed decides the track's
 * random stream and its generators' starting material: Turing's register, Euclid's pitch pattern, Markov's first state.
 * A knob's range, a quick turn across it (ui_input.c accel). Builds 1.1.3 to 1.1.5 picked up to TESS_SEED_LOAD_MAX: a
 * project saved by them keeps its seed and its line, and ROLL from it lands in 1..TESS_SEED_MAX */
#define TESS_SEED_MAX 128u
#define TESS_SEED_LOAD_MAX 9999u
static void tess_seed_apply(uint32_t tr)
{
    uint32_t sd = tessera.seed[tr];
    tessera.rnd[tr] = gen_stream(sd);
    tessera.turing[tr].reg = gen_turing_pick(sd);
    tessera.euclid[tr].pitch_seed = (uint8_t)(1u + gen_stream(sd * 31u + 7u) % 255u);
    tessera.markov[tr].cur_state = 0;
    gen_awake_fill(&tessera.awake[tr], sd);
    gen_poly_fill(&tessera.poly[tr], sd);
    gen_quad_fill(&tessera.quad[tr], sd);
    gen_constel_fill(&tessera.constel[tr], sd);
    gen_series_fill(&tessera.series[tr], sd);
    gen_chord_fill(&tessera.chord[tr], sd);
    gen_bounce_serve(&tessera.bounce[tr], sd);
}
/* A track still without a seed (never played, nor loaded from a project) gets a random one now. Saved projects
 * bring their own seed and material, so their music stays as it was saved */
static void tess_seed_fresh(uint32_t tr)
{
    if (tessera.seed[tr]) return;
    tessera.seed[tr] = (uint16_t)(1u + gen_stream(TESS_ENTROPY() + tr * 0x9E3779B9u) % TESS_SEED_MAX);
    tess_seed_apply(tr);
}
static void tess_realign(uint32_t tr);
static void sd_realign(uint32_t tr);                    /* (step_seed.c) */
static void tess_form_restart(uint32_t tr);
/* The seed ROLL left waiting (tessera_seed_turn): its line from its start. The playheads back on step 1; a FORM
 * begins again at A (its sections were the old line's) */
static void tess_roll_apply(uint32_t tr)
{
    if (!tessera.roll[tr]) return;
    tessera.seed[tr] = tessera.roll[tr];
    tessera.roll[tr] = 0;
    tess_seed_apply(tr);
    if (tessera.evolve[tr].form) tess_form_restart(tr);
    tess_realign(tr);
    tessera.evolve[tr].flash = 4;
}
/* ROLL turned by s (seeds 1..TESS_SEED_MAX, stopping at both ends: no jump from one end to the other). Playing, the seed waits for the track's next bar line
 * (tessera_evolve_tick; the knob shows it blinking): the new line starts in time, and a quick turn plays only the
 * seed it stops on; turned back to the seed playing, nothing waits. Stopped: at once */
void tessera_seed_turn(uint32_t tr, int32_t s)
{
    int32_t n;
    if (tr >= TESSERA_TRACKS || !s) return;
    tess_seed_fresh(tr);
    n = (int32_t)(tessera.roll[tr] ? tessera.roll[tr] : tessera.seed[tr]) + s;
    if (n < 1) n = 1;
    if (n > (int32_t)TESS_SEED_MAX) n = (int32_t)TESS_SEED_MAX;   /* (a seed over it, saved by 1.1.3-1.1.5: here) */
    tessera.roll[tr] = (uint32_t)n == tessera.seed[tr] ? 0u : (uint16_t)n;
    if (!song.playing) tess_roll_apply(tr);
}

void tessera_markov_recalc(uint32_t tr) { (void)tr; }   /* matrix is derived per step now */

/* CHORD's USER before any key: POP's four chords (I V vi IV), so it plays from the start */
static void tess_chord_user_pop(tessera_chord_t *c)
{
    static const uint8_t POP[4] = {0, 4, 5, 3};
    memcpy(c->user, POP, sizeof POP);
    c->ulen = sizeof POP;
}

void tessera_init(void)
{
    uint8_t *p = (uint8_t *)&tessera;
    for (uint32_t b = 0; b < sizeof(tessera); b++) p[b] = 0;
    memset(&tess_sv, 0, sizeof tess_sv);
    memset(&tess_an, 0, sizeof tess_an);
    tessera.key_link = 1;                               /* (a new project: one key for every track) */

    for (uint32_t i = 0; i < TESSERA_TRACKS; i++) {
        tessera.mode[i] = TESS_ENG_OFF;

        /* 1. Turing machine */
        tessera.turing[i].reg = 0;                       /* (the seed picks it: tess_seed_fresh, when first used) */
        tessera.turing[i].len = 16;
        tessera.turing[i].prob = 0;
        tessera.turing[i].oct_span = 2;

        /* 2. Euclidean */
        tessera.euclid[i].steps = 16;
        tessera.euclid[i].pulses = (i == 1) ? 5 : 4;
        tessera.euclid[i].rot = 0;

        /* 3. Markov */
        tessera.markov[i].cur_state = 0;
        tessera.markov[i].tension = 25;
        tessera.markov[i].density = 70;                 /* (a step in three rests: room to breathe) */
        tessera.markov[i].oct_span = 2;
        tessera_markov_recalc(i);

        /* 4. Morph */
        tessera.morph[i].morph_pos = 64;                     /* (halfway between A and B) */
        tessera.morph[i].snap_a = 0x8888u;
        tessera.morph[i].snap_b = 0xA5A5u;
        tessera.morph[i].step_idx = 0;
        tessera.morph[i].oct_span = 2;

        /* 5. Grids: the map's middle, half full, no chaos; KICK, SNR, HAT all of DENS, no TOMS */
        tessera.grids[i].x = 128;
        tessera.grids[i].y = 128;
        tessera.grids[i].dens = 50;

        /* 6. Bounce: one slow ball at 37 degrees; 7. Awake: lengths 8 and 7 (its own); 8. Poly: rhythms of 4 and
         * 3 steps, one octave (their values: the seed's, tess_seed_apply) */
        tessera.bounce[i].speed = 24;
        tessera.bounce[i].angle = 37;
        tessera.bounce[i].balls = 1;
        tessera.awake[i].len_a = 8;
        tessera.awake[i].len_b = 7;
        tessera.awake[i].prob = 100;
        tessera.poly[i].div[0] = 4;
        tessera.poly[i].div[1] = 3;
        tessera.poly[i].span = 1;
        tessera.poly[i].pos = 3;
        tessera.quad[i].prob = 100;                     /* 9. Quad: forward, every cell, no LINK, three octaves; */
        tessera.quad[i].pos = 4;                        /* the columns still, each set to move every 4 steps */
        tessera.qcol[i] = 3u << 2;
        tessera.constel[i].y = 7;                       /* 10. Constellation: the middle of the sky, a star in two */
        tessera.constel[i].dens = 50;                   /* columns caught within 2 rows, the ship steered by hand */
        tessera.constel[i].size = 2;
        tessera.constel[i].len = 16;                    /* (the whole loop; FILL OFF) */
        gen_constel_start(&tessera.constel[i]);
        tessera.series[i].intv = 1;                     /* 11. Series: from its start, a degree a unit, on and on */
        tessera.chord[i].bars = 1;                      /* 12. Chord: POP, a chord a bar, held; USER POP's chords */
        tess_chord_user_pop(&tessera.chord[i]);
        gen_bounce_serve(&tessera.bounce[i], 0u);

        /* 9. Evolve */
        tessera.evolve[i].amount = 30;
        tessera.evolve[i].rec = 2;
        tessera.evolve[i].contrast = 60;
        tessera.evolve[i].cond_first = 1;
        tessera.org_speed[i] = 50;                       /* SLOTH and OCHD at their own speeds */
        tessera.org_rate[i] = 50;

        /* 10. NOTE LENGTH: STEP (the GATE); FIXED's 1/8, four keys */
        tessera.nl_len[i] = 2;
        tessera.nl_keys[i] = NL_KEYS;

        /* 11. STEP EVOLVE: AMOUNT 0, the steps as written */
        tessera.sv_lock[i] = TESS_SV_LOCK0;
        tessera.sv_every[i] = TESS_SV_EVERY0;

        /* 12. SUB: RATCH x1 (as before), every step once it is up */
        tessera.sub_chance[i] = 100;

        /* 13. ANSWER: some change, a few echoes, 6 answers a bar apart; nothing heard yet */
        tessera.answer[i].vary = 30;
        tessera.answer[i].echo = 20;
        tessera.answer[i].fade = 33;
        tessera.answer[i].space = 25;
        tessera.answer[i].shift = 2;
        tessera.answer[i].len = TESS_AN_MID;
    }
}

void tessera_kill(track_t *t)
{
    uint32_t tr = trk_index(t);
    if (tr >= TESSERA_TRACKS) return;
    nl_stop(t);
    if (tessera.active_gate[tr]) {
        trk_note_off(t, tessera.last_note[tr]);
        tessera.active_gate[tr] = 0;
    }
    seq_release(t);                                   /* (it empties seq_n: the step sequencer's notes go too) */
}

static void tess_realign(uint32_t tr);
static void tess_form_reset(uint32_t tr);

void tessera_stop_all(void)
{
    for (uint32_t i = 0; i < TESSERA_TRACKS; i++) {
        tessera_kill(&trk[i]);
        tess_roll_apply(i);                                   /* (a seed still waiting: the next PLAY's) */
        tessera.evolve[i].bar_cnt = 0;
        tessera.evolve[i].step_cnt = 0;
        tessera.evolve[i].flash = 0;
        tessera.evolve[i].cond_bar = 0;                       /* COND counts from PLAY; BRANCH's side back home */
        tessera.evolve[i].cond_first = 1;
        tessera.evolve[i].br_side = 0;
        tessera.hit[i] = 0;
        tessera.br_in[i] = 0;
        tess_realign(i);
        gen_chord_start(&tessera.chord[i]);                   /* (not on EVOLVE's boundaries: the progression goes on) */
        tess_form_reset(i);
        answer_restart(i);                                    /* (a phrase kept: its answers from the first) */
        if (tessera.seed[i]) {                                /* the next PLAY: the seed's random choices from the top */
            tessera.rnd[i] = gen_stream(tessera.seed[i]);
            tessera.markov[i].cur_state = 0;
        }
    }
}

static const uint8_t TESS_EVERY_BARS[6] = {0, 1, 2, 4, 8, 16};

uint32_t tessera_every_bars(uint32_t every)
{
    return TESS_EVERY_BARS[every > 5u ? 5u : every];
}

/* Steps in one 4/4 bar at the track's division, derived from seq_div_samples()
 * with division 0 = 1/4 (a quarter note), so it follows the sequencer's own DIV table. */
static uint32_t tess_steps_per_bar(const track_t *t)
{
    uint32_t q = seq_div_samples(0u), d = seq_div_samples((uint32_t)t->p[P_SDIV]);
    uint32_t n = d ? (4u * q + d / 2u) / d : 16u;
    return n ? n : 1u;
}

/* Phrase loop length in steps (<= 32, the buffer size) */
static uint32_t tess_loop_len(uint32_t tr)
{
    uint32_t n = 16u;
    if (tessera.mode[tr] == TESS_ENG_TURING) n = tessera.turing[tr].len ? tessera.turing[tr].len : 16u;
    else if (tessera.mode[tr] == TESS_ENG_EUCLID) n = tessera.euclid[tr].steps ? tessera.euclid[tr].steps : 16u;
    else if (tessera.mode[tr] == TESS_ENG_GRIDS) n = gen_grids_steps(tess_steps_per_bar(&trk[tr]));
    else if (tessera.mode[tr] == TESS_ENG_MARKOV || tessera.mode[tr] == TESS_ENG_MORPH)   /* no length of their own: */
        n = trk[tr].p[P_SLEN] > 0 ? (uint32_t)trk[tr].p[P_SLEN] : 16u;                     /* PATTERN's LEN (issue #3) */
    return n > 32u ? 32u : n;
}

/* Settings changed: record / capture the phrase again from its start */
static void tess_rearm(uint32_t tr)
{
    tessera.evolve[tr].lp_pos = 0;
    tessera.evolve[tr].rec = 2;
    tessera.evolve[tr].valid &= (uint8_t)~(1u << (tessera.evolve[tr].sec % 3u));   /* this section's phrase is being rewritten */
}

/* A phrase boundary (mutation, SEED, stop): rearm AND put the engines' own playheads back on step 1 */
static void tess_realign(uint32_t tr)
{
    tess_rearm(tr);
    sd_realign(tr);
    tessera.euclid[tr].step_idx = 0;
    tessera.grids[tr].step_idx = 0;
    gen_bounce_serve(&tessera.bounce[tr], tessera.seed[tr]);
    tessera.awake[tr].pos_a = tessera.awake[tr].pos_b = 0;
    tessera.poly[tr].pos = 3;                                /* (the first rhythm's hit plays step 1) */
    tessera.poly[tr].tick = 0;
    tessera.quad[tr].pos = 4;
    tessera.quad[tr].back = 0;
    tessera.qcol_cnt[tr] = 0;                                /* (column tr's: PLAY on all tracks restarts them all) */
    gen_constel_start(&tessera.constel[tr]);
    gen_series_start(&tessera.series[tr]);
    tessera.morph[tr].step_idx = (uint8_t)((tessera.morph[tr].step_idx & 0xF0u) | 0x0Fu);  /* engine pre-increments */
}

/* QUAD's mutation: with other QUAD rows, each column at AMOUNT % turns down a row among them (the last one's cell to
 * the first), so the cells pass from track to track; alone, or while any column moves of its own (tess_quad_cols:
 * the columns already pass the cells round), its own cells change (gen_quad_mutate) */
static int tess_quad_moving(void)
{
    for (uint32_t c = 0; c < 4u; c++)
        if (tessera.qcol[c] & 3u) return 1;
    return 0;
}
static void tess_quad_mutate(uint32_t tr, uint32_t amt)
{
    uint32_t q[TESSERA_TRACKS], n = 0, i, c;
    for (i = 0; i < TESSERA_TRACKS; i++)
        if (tessera.mode[i] == TESS_ENG_QUAD) q[n++] = i;
    if (n < 2u || tess_quad_moving()) {
        gen_quad_mutate(&tessera.quad[tr], amt, &tessera.rnd[tr]);
        return;
    }
    for (c = 0; c < 4u; c++) {
        uint8_t last;
        if (gen_rnd(&tessera.rnd[tr], 100u) >= amt) continue;
        last = tessera.quad[q[n - 1u]].cell[c];
        for (i = n - 1u; i > 0u; i--) tessera.quad[q[i]].cell[c] = tessera.quad[q[i - 1u]].cell[c];
        tessera.quad[q[0]].cell[c] = last;
    }
}

/* One mutation, scaled by the track's AMOUNT (EVOLVE's EVERY; FORM's new sections by CONTRAST) */
void tessera_mutate(uint32_t tr)
{
    if (tr >= TESSERA_TRACKS) return;
    uint32_t amt = tessera.evolve[tr].amount;
    if (amt > 100u) amt = 100u;

    switch (tessera.mode[tr]) {
    case TESS_ENG_TURING: gen_turing_mutate(&tessera.turing[tr], amt, &tessera.rnd[tr]); break;
    case TESS_ENG_EUCLID: gen_euclid_mutate(&tessera.euclid[tr], amt, &tessera.rnd[tr]); break;
    case TESS_ENG_MARKOV: gen_markov_mutate(&tessera.markov[tr], amt, &tessera.rnd[tr]); break;
    case TESS_ENG_MORPH:  gen_morph_mutate(&tessera.morph[tr], amt, &tessera.rnd[tr]); break;
    case TESS_ENG_GRIDS:  gen_grids_mutate(&tessera.grids[tr], amt, &tessera.rnd[tr]); break;
    case TESS_ENG_BOUNCE: gen_bounce_mutate(&tessera.bounce[tr], amt, &tessera.rnd[tr]); break;
    case TESS_ENG_AWAKE:  gen_awake_mutate(&tessera.awake[tr], amt, &tessera.rnd[tr]); break;
    case TESS_ENG_POLY:   gen_poly_mutate(&tessera.poly[tr], amt, &tessera.rnd[tr]); break;
    case TESS_ENG_QUAD:   tess_quad_mutate(tr, amt); break;
    case TESS_ENG_CONSTEL: gen_constel_mutate(&tessera.constel[tr], amt, &tessera.rnd[tr]); break;
    case TESS_ENG_SERIES: gen_series_mutate(&tessera.series[tr], amt, &tessera.rnd[tr]); break;
    case TESS_ENG_CHORD:  gen_chord_mutate(&tessera.chord[tr], amt, &tessera.rnd[tr]); break;
    }
    tessera.evolve[tr].flash = 4;
    tess_realign(tr);
}

/* ------------------------------------------------------------------ form (A/B/A/C ...) --- */
static const uint8_t TESS_FORMS[7][4] = {
    {0, 0, 0, 0},
    {0, 1, 0, 0},   /* AB   */
    {0, 1, 0, 2},   /* ABAC */
    {0, 1, 2, 1},   /* ABCB */
    {0, 0, 1, 0},   /* AABA */
    {0, 1, 2, 0},   /* ABCA */
    {0, 0, 1, 2}    /* AABC */
};
static const uint8_t TESS_FORM_LEN[7] = {0, 2, 4, 4, 4, 4, 4};
static const char *const TESS_FORM_NAME[7] = {"OFF", "AB", "ABAC", "ABCB", "AABA", "ABCA", "AABC"};

const char *tessera_form_name(uint32_t form) { return TESS_FORM_NAME[form > 6u ? 6u : form]; }

uint32_t tessera_form_get(uint32_t tr, uint8_t seq[4], uint32_t *pos)
{
    const tessera_evolve_t *ev = &tessera.evolve[tr < TESSERA_TRACKS ? tr : 0u];
    uint32_t f = ev->form > 6u ? 0u : ev->form, i;
    for (i = 0; i < 4u; i++) seq[i] = TESS_FORMS[f][i];
    *pos = ev->form_pos;
    return TESS_FORM_LEN[f];
}

/* Remember the live phrase as the section being left (the Markov / Morph notes are written straight into it) */
static void tess_save_sec(uint32_t tr)
{
    tessera_evolve_t *ev = &tessera.evolve[tr];
    tessera_sec_t *s = &tessera.sec[tr][ev->sec % 3u];
    gen_sec_t g = {s->reg, s->p0, s->p1};
    switch (tessera.mode[tr]) {
    case TESS_ENG_TURING:
        gen_turing_save(&tessera.turing[tr], &g);
        if (!ev->rec) g.reg = ev->base_reg;                  /* the phrase's start, not where it has shifted to */
        break;
    case TESS_ENG_EUCLID: gen_euclid_save(&tessera.euclid[tr], &g); break;
    case TESS_ENG_MARKOV: gen_markov_save(&tessera.markov[tr], &g); break;
    case TESS_ENG_MORPH:  gen_morph_save(&tessera.morph[tr], &g); break;
    case TESS_ENG_GRIDS:  gen_grids_save(&tessera.grids[tr], &g); break;
    case TESS_ENG_BOUNCE: gen_bounce_save(&tessera.bounce[tr], &g); break;
    case TESS_ENG_AWAKE:                                     /* (its values: the phrase buffer, Awake has no LOCK) */
        gen_awake_save(&tessera.awake[tr], &g);
        memcpy(s->note, tessera.awake[tr].a, 8);
        memcpy(s->note + 8, tessera.awake[tr].b, 8);
        break;
    case TESS_ENG_POLY:   gen_poly_save(&tessera.poly[tr], &g); break;
    case TESS_ENG_QUAD:   gen_quad_save(&tessera.quad[tr], &g); break;
    case TESS_ENG_CONSTEL: gen_constel_save(&tessera.constel[tr], &g); break;
    case TESS_ENG_SERIES: gen_series_save(&tessera.series[tr], &g); break;
    case TESS_ENG_CHORD:  gen_chord_save(&tessera.chord[tr], &g); break;
    }
    s->reg = g.reg;
    s->p0 = g.p0;
    s->p1 = g.p1;
}

/* Bring a remembered section back: its settings, and (Markov / Morph) its recorded phrase */
static void tess_load_sec(uint32_t tr)
{
    tessera_evolve_t *ev = &tessera.evolve[tr];
    const tessera_sec_t *s = &tessera.sec[tr][ev->sec % 3u];
    const gen_sec_t g = {s->reg, s->p0, s->p1};
    uint8_t v = ev->valid;
    switch (tessera.mode[tr]) {
    case TESS_ENG_TURING: gen_turing_load(&tessera.turing[tr], &g); break;
    case TESS_ENG_EUCLID: gen_euclid_load(&tessera.euclid[tr], &g); break;
    case TESS_ENG_MARKOV: gen_markov_load(&tessera.markov[tr], &g); break;
    case TESS_ENG_MORPH:  gen_morph_load(&tessera.morph[tr], &g); break;
    case TESS_ENG_GRIDS:  gen_grids_load(&tessera.grids[tr], &g); break;
    case TESS_ENG_BOUNCE: gen_bounce_load(&tessera.bounce[tr], &g); break;
    case TESS_ENG_AWAKE: {                                   /* (a section saved before a project load: no values, kept) */
        uint32_t i, any = 0;
        gen_awake_load(&tessera.awake[tr], &g);
        for (i = 0; i < 8u; i++) any |= s->note[i];
        if (any) {
            memcpy(tessera.awake[tr].a, s->note, 8);
            memcpy(tessera.awake[tr].b, s->note + 8, 8);
        }
        break;
    }
    case TESS_ENG_POLY:   gen_poly_load(&tessera.poly[tr], &g); break;
    case TESS_ENG_QUAD:   gen_quad_load(&tessera.quad[tr], &g); break;
    case TESS_ENG_CONSTEL: gen_constel_load(&tessera.constel[tr], &g); break;
    case TESS_ENG_SERIES: gen_series_load(&tessera.series[tr], &g); break;
    case TESS_ENG_CHORD:                                     /* (a section begins its progression from the top) */
        gen_chord_load(&tessera.chord[tr], &g);
        gen_chord_start(&tessera.chord[tr]);
        break;
    }
    tess_realign(tr);
    ev->valid = v;
    if ((tessera.mode[tr] == TESS_ENG_MARKOV || tessera.mode[tr] == TESS_ENG_MORPH) && ((v >> (ev->sec % 3u)) & 1u))
        ev->rec = 0;                                         /* replay what was recorded, do not record again */
}

/* The live phrase becomes section A; B and C are made fresh when first reached */
static void tess_form_restart(uint32_t tr)
{
    tessera_evolve_t *ev = &tessera.evolve[tr];
    ev->sec = 0;
    ev->form_pos = 0;
    ev->visited = 1;
    ev->valid = 0;
    ev->bar_cnt = 0;
    ev->cond_bar = 0;                                        /* (COND: a section counts its own bars) */
    ev->cond_first = 1;
    tess_rearm(tr);
}

/* On stop the form goes back to its first section */
static void tess_form_reset(uint32_t tr)
{
    tessera_evolve_t *ev = &tessera.evolve[tr];
    if (!ev->form) return;
    if (ev->sec != 0u) {
        tess_save_sec(tr);
        ev->sec = 0;
        if (ev->visited & 1u) tess_load_sec(tr);
    }
    ev->form_pos = 0;
}

/* A section boundary: remember this section, move on. A section seen before is recalled exactly;
 * a new one starts from the old one and is pushed away from it by CONTRAST. */
static void tess_form_advance(uint32_t tr)
{
    tessera_evolve_t *ev = &tessera.evolve[tr];
    uint32_t len = TESS_FORM_LEN[ev->form > 6u ? 0u : ev->form];
    if (!len) return;
    tess_save_sec(tr);
    ev->form_pos = (uint8_t)((ev->form_pos + 1u) % len);
    ev->sec = TESS_FORMS[ev->form][ev->form_pos];
    ev->cond_bar = 0;                                        /* (COND: a section counts its own bars) */
    ev->cond_first = 1;
    if (ev->visited & (1u << ev->sec)) {
        tess_load_sec(tr);
    } else {
        uint8_t keep = ev->amount;
        uint32_t n = 1u + ev->contrast / 50u;
        ev->amount = ev->contrast;
        while (n--) tessera_mutate(tr);
        ev->amount = keep;
        ev->visited |= (uint8_t)(1u << ev->sec);
    }
    ev->flash = 4;
}

/* FORM page encoders: 0 FORM, 1 CONTRAST, 2 CLEAR */
static void tess_form_knob_body(uint32_t k, int32_t s)
{
    tessera_ensure_init();
    uint32_t tr = song.sel;
    if (tr >= TESSERA_TRACKS || !s) return;
    tessera_evolve_t *ev = &tessera.evolve[tr];

    if (k == 0u) {
        int32_t f = (int32_t)ev->form + (s > 0 ? 1 : -1);
        f = f < 0 ? 0 : f > 6 ? 6 : f;
        if ((uint8_t)f != ev->form) {
            ev->form = (uint8_t)f;
            if (f && !ev->every) ev->every = 3;              /* sections need a length: 4 bars */
            tess_form_restart(tr);
        }
    } else if (k == 1u) {
        int32_t v = (int32_t)ev->contrast + s * 2;
        ev->contrast = (uint8_t)(v < 0 ? 0 : v > 100 ? 100 : v);
    } else if (k == 3u) {                                    /* FLOURISH: a semitone a click, -12..+12 */
        int32_t v = (int32_t)ev->flourish + s;
        ev->flourish = (int8_t)(v < -12 ? -12 : v > 12 ? 12 : v);
    } else if (k == 2u) {
        if (ev->form) tess_form_restart(tr);                 /* forget B and C: this phrase is A again */
    }
}
void tessera_form_knob(uint32_t k, int32_t s)
{
    TESS_IRQ_OFF();
    tess_form_knob_body(k, s);
    TESS_IRQ_ON();
}

/* Once per sequencer step, before the engine runs: counts bars, mutates on the boundary, and
 * walks the phrase loop. Turing: at the start of every pass the register goes back to the base
 * captured at the phrase start (when EVERY is on), so LOCK's variations never accumulate.
 * Markov / Morph: the first pass after a boundary is recorded, later passes replay it. */
static void tessera_evolve_tick(const track_t *t, uint32_t tr)
{
    tessera_evolve_t *ev = &tessera.evolve[tr];
    uint32_t L = tess_loop_len(tr), pos;
    if (ev->flash) ev->flash--;
    if (ev->step_cnt >= tess_steps_per_bar(t)) {
        ev->step_cnt = 0;
        ev->cond_bar = (uint8_t)((ev->cond_bar + 1u) & 7u);   /* (before FORM's section change sets it back) */
        ev->cond_first = 0;
        if (tessera.roll[tr]) {
            tess_roll_apply(tr);             /* ROLL's seed on the bar line; EVERY counts from its line */
            ev->bar_cnt = 0;
        } else if (ev->every && ++ev->bar_cnt >= tessera_every_bars(ev->every)) {
            ev->bar_cnt = 0;
            if (ev->form) tess_form_advance(tr);
            else tessera_mutate(tr);         /* also puts the loop back on step 1 and rearms it */
        }
    } else if (!ev->step_cnt && tessera.roll[tr]) {
        tess_roll_apply(tr);                 /* (PLAY's first step: a bar line too) */
    }
    if (!ev->step_cnt && ev->br_to && ev->br_mode == TESS_BR_LATCH)   /* LATCH: one toss a bar, its side held */
        ev->br_side = (uint8_t)(ev->branch && gen_rnd(&tessera.rnd[tr], 100u) < ev->branch);
    ev->step_cnt++;
    pos = ev->lp_pos;
    if (pos >= L) pos = 0;
    if (pos == 0) {
        if (tessera.mode[tr] == TESS_ENG_TURING) {
            tessera_turing_t *tu = &tessera.turing[tr];
            if (ev->rec) { ev->base_reg = tu->reg; ev->rec = 0; }
            else if (ev->every) tu->reg = ev->base_reg;
        } else if (ev->rec) {
            if (--ev->rec == 0)              /* 2 -> 1: record this pass; 1 -> 0: replay from now on */
                ev->valid |= (uint8_t)(1u << (ev->sec % 3u));
        }
    }
    ev->pos_now = (uint8_t)pos;
    ev->lp_pos = (uint8_t)(pos + 1u >= L ? 0u : pos + 1u);
}

/* FLOURISH: the last beat of the last bar of every phrase (EVERY bars; a FORM's sections end on the same
 * boundary) plays shifted by `flourish` semitones. step_cnt counts 1..steps-per-bar, already stepped for this step. */
static int32_t tess_flourish(const track_t *t, uint32_t tr)
{
    const tessera_evolve_t *ev = &tessera.evolve[tr];
    uint32_t spb, win;
    if (!ev->flourish || !ev->every) return 0;
    spb = tess_steps_per_bar(t);
    win = spb / 4u ? spb / 4u : 1u;
    if ((uint32_t)ev->bar_cnt + 1u < tessera_every_bars(ev->every)) return 0;   /* not the phrase's last bar */
    return ev->step_cnt > spb - win ? (int32_t)ev->flourish : 0;
}

/* Markov / Morph: LOCK% of steps replay the section's base phrase, the rest play fresh (not remembered) */
static void tessera_phrase_apply(uint32_t tr, uint8_t *note, uint8_t *vel, int *trig)
{
    tessera_evolve_t *ev = &tessera.evolve[tr];
    tessera_sec_t *sc = &tessera.sec[tr][ev->sec % 3u];
    uint32_t pos = ev->pos_now & 31u, sh = (pos & 1u) * 4u;
    if (!ev->lock) return;                                   /* 0 = fresh every pass (the old behaviour) */
    if (ev->rec) {
        uint32_t q = *vel < 64u ? 0u : (uint32_t)(*vel - 64u) / 4u;
        if (q > 15u) q = 15u;
        sc->note[pos] = (uint8_t)((*trig ? 0x80u : 0u) | (*note & 0x7Fu));
        sc->vel[pos >> 1] = (uint8_t)((sc->vel[pos >> 1] & ~(0xFu << sh)) | (q << sh));
    } else if (gen_rnd(&tessera.rnd[tr], 100u) < ev->lock) {
        uint8_t b = sc->note[pos];
        *trig = (b >> 7) & 1;
        *note = b & 0x7Fu;
        *vel = (uint8_t)(64u + ((sc->vel[pos >> 1] >> sh) & 0xFu) * 4u + 2u);
    }
}

/* LOCK = deja vu: 100 frozen loop, 0 fresh every pass. Turing: exactly 100 - prob; Grids: 100 - chaos. */
int tessera_lock_get(uint32_t tr)
{
    if (tr >= TESSERA_TRACKS) return -1;
    if (tessera_step_on(tr)) return tessera.sv_lock[tr];    /* (STEP EVOLVE's) */
    if (tessera.mode[tr] == TESS_ENG_TURING) return 100 - (int)tessera.turing[tr].prob;
    if (tessera.mode[tr] == TESS_ENG_GRIDS) return 100 - (int)tessera.grids[tr].chaos;
    if (tessera.mode[tr] == TESS_ENG_MARKOV || tessera.mode[tr] == TESS_ENG_MORPH || tessera.mode[tr] == TESS_ENG_CONSTEL)
        return tessera.evolve[tr].lock;                      /* (Constellation: the chance a step plays the loop) */
    return -1;                                               /* Euclid, Bounce, Awake, Poly: fixed loops, no LOCK */
}

void tessera_phrase(uint32_t tr, uint32_t *bar, uint32_t *of, int *flash)
{
    const tessera_evolve_t *ev = &tessera.evolve[tr < TESSERA_TRACKS ? tr : 0u];
    if (tr < TESSERA_TRACKS && tessera_step_on(tr)) {   /* (STEP EVOLVE: bars since its last round) */
        *of = tessera_every_bars(tessera.sv_every[tr]);
        *bar = tess_sv.steps[tr] / tess_steps_per_bar(&trk[tr]);
        if (*of && *bar >= *of) *bar = *of - 1u;
        *flash = tess_sv.flash[tr] != 0;
        return;
    }
    *of = tessera_every_bars(ev->every);
    *bar = ev->bar_cnt;
    *flash = ev->flash != 0;
}

/* EVOLVE page encoders: 0 LOCK, 1 EVERY, 2 AMOUNT, 3 ROLL; a STEP track's (STEP EVOLVE): 3 CLEAR */
static void tess_evolve_knob_body(uint32_t k, int32_t s)
{
    tessera_ensure_init();
    uint32_t tr = song.sel;
    if (tr >= TESSERA_TRACKS || !s) return;
    tessera_evolve_t *ev = &tessera.evolve[tr];

    if (tessera_step_on(tr)) {
        if (k == 0u) tess_param_set_body(tr, TX_SV_LOCK, (int32_t)tessera.sv_lock[tr] + s * 2);
        else if (k == 1u) tess_param_set_body(tr, TX_SV_EVERY, (int32_t)tessera.sv_every[tr] + (s > 0 ? 1 : -1));
        else if (k == 2u) tess_param_set_body(tr, TX_SV_AMOUNT, (int32_t)tessera.sv_amount[tr] + s * 2);
        else tessera_step_clear(tr);
        return;
    }
    if (k == 0u) {
        if (tessera_lock_get(tr) < 0) return;
        if (tessera.mode[tr] == TESS_ENG_TURING) {
            int32_t p = (int32_t)tessera.turing[tr].prob - s * 2;   /* clockwise = more locked */
            tessera.turing[tr].prob = (uint8_t)(p < 0 ? 0 : p > 100 ? 100 : p);
        } else if (tessera.mode[tr] == TESS_ENG_GRIDS) {
            int32_t c = (int32_t)tessera.grids[tr].chaos - s * 2;  /* clockwise = less chaos */
            tessera.grids[tr].chaos = (uint8_t)(c < 0 ? 0 : c > 100 ? 100 : c);
        } else {
            int32_t l = (int32_t)ev->lock + s * 2;
            uint8_t was = ev->lock;
            ev->lock = (uint8_t)(l < 0 ? 0 : l > 100 ? 100 : l);
            if (!was && ev->lock) tess_rearm(tr);                  /* record the phrase that is playing next */
        }
    } else if (k == 1u) {
        int32_t e = (int32_t)ev->every + (s > 0 ? 1 : -1);
        ev->every = (uint8_t)(e < 0 ? 0 : e > 5 ? 5 : e);
        ev->bar_cnt = 0;
    } else if (k == 2u) {
        int32_t a = (int32_t)ev->amount + s * 2;
        ev->amount = (uint8_t)(a < 0 ? 0 : a > 100 ? 100 : a);
    } else if (k == 3u) {
        if (tessera.mode[tr] != TESS_ENG_OFF) tessera_seed_turn(tr, s);   /* ROLL: each step a new line, back = the last */
    }
}
void tessera_evolve_knob(uint32_t k, int32_t s)
{
    TESS_IRQ_OFF();
    tess_evolve_knob_body(k, s);
    TESS_IRQ_ON();
}

/* TWEAK page encoders: 0 SHIFT, 1 DROP, 2 HUMAN, 3 FOLLOW */
static void tess_tweak_knob_body(uint32_t k, int32_t s)
{
    tessera_ensure_init();
    uint32_t tr = song.sel;
    if (tr >= TESSERA_TRACKS || !s) return;
    tessera_evolve_t *ev = &tessera.evolve[tr];

    if (k == 0u) {
        int32_t v = (int32_t)ev->shift + s;
        ev->shift = (int8_t)(v < -24 ? -24 : v > 24 ? 24 : v);
    } else if (k == 1u) {
        int32_t v = (int32_t)ev->drop + s * 2;
        ev->drop = (uint8_t)(v < 0 ? 0 : v > 100 ? 100 : v);
    } else if (k == 2u) {
        int32_t v = (int32_t)ev->human + s * 2;
        ev->human = (uint8_t)(v < 0 ? 0 : v > 100 ? 100 : v);
    } else if (k == 3u) {
        int32_t dir = s > 0 ? 1 : -1, f = (int32_t)ev->follow + dir;
        if (f == (int32_t)tr + 1) f += dir;                  /* not itself */
        if (f < 0 || f == (int32_t)tr + 1) f = 0;
        if (f > (int32_t)TESSERA_TRACKS) f = (int32_t)TESSERA_TRACKS;
        if (f == (int32_t)tr + 1) f = 0;
        ev->follow = (uint8_t)f;
    }
}
void tessera_tweak_knob(uint32_t k, int32_t s)
{
    TESS_IRQ_OFF();
    tess_tweak_knob_body(k, s);
    TESS_IRQ_ON();
}

/* TWEAK 2 page encoders: 0 BRANCH, 1 TO (OFF, then each other track with COIN, TOGGLE, LATCH), 2 LOGIC (OFF, then
 * AND, OR, XOR, NOT, each with the other tracks), 3 COND. Another track as a slot 0..2 (this one left out) */
static uint32_t tess_other(uint32_t tr, uint32_t slot) { return slot + (slot >= tr) + 1u; }   /* -> 1..4 */
static uint32_t tess_slot_of(uint32_t tr, uint32_t t) { return t - 1u - (t - 1u > tr); }
static int32_t tess_step_idx(int32_t i, int32_t s, int32_t n)
{
    i += s > 0 ? 1 : -1;
    return i < 0 ? 0 : i >= n ? n - 1 : i;
}
static void tess_tweak2_knob_body(uint32_t k, int32_t s)
{
    tessera_ensure_init();
    uint32_t tr = song.sel;
    if (tr >= TESSERA_TRACKS || !s) return;
    tessera_evolve_t *ev = &tessera.evolve[tr];
    int32_t i;

    if (k == 0u) {
        int32_t v = (int32_t)ev->branch + s * 2;
        ev->branch = (uint8_t)(v < 0 ? 0 : v > 100 ? 100 : v);
    } else if (k == 1u) {
        i = ev->br_to && ev->br_to != tr + 1u ? 1 + (int32_t)(tess_slot_of(tr, ev->br_to) * TESS_BR_COUNT + ev->br_mode) : 0;
        i = tess_step_idx(i, s, 1 + (TESSERA_TRACKS - 1) * TESS_BR_COUNT);
        if (!i) ev->br_to = 0;
        else {
            ev->br_to = (uint8_t)tess_other(tr, (uint32_t)(i - 1) / TESS_BR_COUNT);
            ev->br_mode = (uint8_t)((uint32_t)(i - 1) % TESS_BR_COUNT);
        }
        ev->br_side = 0;
    } else if (k == 2u) {
        i = ev->logic && ev->lg_with && ev->lg_with != tr + 1u ?
            1 + (int32_t)((ev->logic - 1u) * (TESSERA_TRACKS - 1) + tess_slot_of(tr, ev->lg_with)) : 0;
        i = tess_step_idx(i, s, 1 + (TESS_LG_COUNT - 1) * (TESSERA_TRACKS - 1));
        if (!i) ev->logic = TESS_LG_OFF, ev->lg_with = 0;
        else {
            ev->logic = (uint8_t)(1u + (uint32_t)(i - 1) / (TESSERA_TRACKS - 1));
            ev->lg_with = (uint8_t)tess_other(tr, (uint32_t)(i - 1) % (TESSERA_TRACKS - 1));
        }
    } else if (k == 3u) {
        ev->cond = (uint8_t)tess_step_idx(ev->cond, s, TESS_COND_COUNT);
    }
}
void tessera_tweak2_knob(uint32_t k, int32_t s)
{
    TESS_IRQ_OFF();
    tess_tweak2_knob_body(k, s);
    TESS_IRQ_ON();
}

static int32_t tess_scale_steps(const track_t *t, int32_t n, int32_t k);
static int32_t tess_follow_source(uint32_t src, uint32_t tr);

static void tess_clock_step_body(track_t *t, uint32_t tr);
/* The key a generator's note moves up by before the snap: the generators write C minor (MARKOV adds ROOT itself), so
 * ROOT on another note plays its whole scale, not C minor's notes snapped into it (ANSWER plays the keys' notes) */
static int32_t tess_key(const track_t *t, uint32_t tr)
{
    return tessera.mode[tr] == TESS_ENG_MARKOV || tessera.mode[tr] == TESS_ENG_ANSWER ? 0 : (int32_t)(t->p[P_ROOT] % 12);
}
#include "step_seed.c"                                  /* STEP as seed: TURING and EUCLID from a STEP pattern */
static uint32_t cord_smask[NTRK];                       /* (cord.c: the cords on Tessera settings, per track) */
static void cord_step_begin(uint32_t tr);
static void cord_step_end(uint32_t tr);
static void tess_branch_on(track_t *v, uint32_t note, uint32_t vel, uint32_t off);
/* QUAD's LINK (the four-track hook): this track's step also moves each QUAD track it links (a bit each, in track
 * order) on a cell and plays it on that track's voice for this step's gate (tess_branch_on: its gate count or its next
 * own step lets it go). One hop: a linked step moves no further LINK. The tracks step in index order, so it replays the
 * same */
static void tess_quad_link(const track_t *t, uint32_t tr)
{
    const tessera_quad_t *q = &tessera.quad[tr];
    if (tessera.mode[tr] != TESS_ENG_QUAD || !q->link || t->p[P_MUTE]) return;
    for (uint32_t to = 0; to < TESSERA_TRACKS; to++) {
        gen_out_t out = {0, 60, 100, 0, 0, 0};
        track_t *v = &trk[to];
        int32_t n;
        if (to == tr || !((q->link >> to) & 1u) || tessera.mode[to] != TESS_ENG_QUAD || v->p[P_MUTE]) continue;
        gen_quad_step(&tessera.quad[to], &tessera.rnd[to], &out);
        tessera.cv[to] = out.cv;
        if (!out.gate) continue;
        n = tessera_snap_note(v, out.note + tess_key(v, to)) + (int32_t)v->p[P_TRANS] + tessera.evolve[to].shift;
        n = n < 0 ? 0 : n > 127 ? 127 : n;
        tess_branch_on(v, (uint32_t)n, out.vel,
                       seq_div_samples((uint32_t)t->p[P_SDIV]) * (uint32_t)(t->p[P_SGATE] ? t->p[P_SGATE] : 64) / 128u);
    }
}
/* QUAD's columns, on the first QUAD track's steps (before its row plays): a column whose MOVE is not STOP moves every
 * EVERY steps: DOWN its cells a row down the four tracks (track 4's to track 1), UP a row up, RAND by 1..3 rows (a draw
 * of that track's). All four rows turn, a track not in QUAD too: its row keeps the notes till it comes back */
uint32_t tessera_quad_every(uint32_t c) { return GEN_QC_EVERY[(tessera.qcol[c & 3u] >> 2) % GEN_QC_EVERY_N]; }
static void tess_quad_cols(uint32_t tr)
{
    uint32_t i, c;
    for (i = 0; i < tr; i++)
        if (tessera.mode[i] == TESS_ENG_QUAD) return;
    for (c = 0; c < 4u; c++) {
        uint32_t m = tessera.qcol[c] & 3u, by;
        uint8_t col[TESSERA_TRACKS];
        if (m == GEN_QC_STOP || ++tessera.qcol_cnt[c] < tessera_quad_every(c)) continue;
        tessera.qcol_cnt[c] = 0;
        by = m == GEN_QC_DOWN ? 1u : m == GEN_QC_UP ? TESSERA_TRACKS - 1u : 1u + gen_rnd(&tessera.rnd[tr], TESSERA_TRACKS - 1u);
        for (i = 0; i < TESSERA_TRACKS; i++) col[i] = tessera.quad[i].cell[c];
        for (i = 0; i < TESSERA_TRACKS; i++) tessera.quad[(i + by) % TESSERA_TRACKS].cell[c] = col[i];
    }
}
void tessera_clock_step(track_t *t)
{
    uint32_t tr = trk_index(t);
    if (tr >= TESSERA_TRACKS) return;
    if (!cord_smask[tr]) {
        tess_clock_step_body(t, tr);
        tess_quad_link(t, tr);
        return;
    }
    cord_step_begin(tr);                                /* the cords on its settings, for this step only */
    tess_clock_step_body(t, tr);
    tess_quad_link(t, tr);
    cord_step_end(tr);
}
/* TWEAK 2 (Logic and Branches). LOGIC: this step's hit g against track lg_with's last step (tessera.hit, what it
 * played on its own voice). The tracks step in index order, so a lower track's hit is this step's, a higher one's the
 * step before. OR and XOR can make a rest play: the generator's note of the step. No random draw */
static int tess_logic(uint32_t tr, int g)
{
    const tessera_evolve_t *ev = &tessera.evolve[tr];
    int o;
    if (!ev->logic || !ev->lg_with || ev->lg_with > TESSERA_TRACKS || ev->lg_with == tr + 1u) return g;
    o = tessera.hit[ev->lg_with - 1u] != 0;
    switch (ev->logic) {
    case TESS_LG_AND: return g && o;
    case TESS_LG_OR: return g || o;
    case TESS_LG_XOR: return g != o;
    case TESS_LG_NOT: return g && !o;
    }
    return g;
}
/* COND (Elektron's trig conditions, a bar at a time): a:b plays in bar a of every b, counted from PLAY (with FORM on,
 * from the section's start); FIRST only in the first bar, !FIRST in every other. No random draw */
static const uint8_t TESS_COND_AB[TESS_COND_COUNT][2] = {{0, 1}, {1, 2}, {2, 2}, {1, 4}, {2, 4}, {3, 4}, {4, 4}, {1, 8},
                                                         {8, 8}, {0, 1}, {0, 1}};
static const char *const TESS_COND_NAME[TESS_COND_COUNT] = {"OFF", "1:2", "2:2", "1:4", "2:4", "3:4", "4:4", "1:8",
                                                            "8:8", "FIRST", "!FIRST"};
const char *tessera_cond_name(uint32_t cond) { return TESS_COND_NAME[cond < TESS_COND_COUNT ? cond : 0u]; }
static int tess_cond(uint32_t tr)
{
    const tessera_evolve_t *ev = &tessera.evolve[tr];
    uint32_t c = ev->cond;
    if (c == TESS_COND_OFF || c >= TESS_COND_COUNT) return 1;
    if (c == TESS_COND_FIRST) return ev->cond_first;
    if (c == TESS_COND_NFIRST) return !ev->cond_first;
    return ev->cond_bar % TESS_COND_AB[c][1] == TESS_COND_AB[c][0] - 1u;
}
/* BRANCH (Mutable Instruments Branches' coin): the track a hit plays on, this one or br_to. COIN tosses each hit
 * (BRANCH the chance it goes over); TOGGLE: a toss won moves the hits to the other side, where they stay until the
 * next one won (100: they alternate); LATCH: one toss a bar (tessera_evolve_tick), the bar's hits all on its side.
 * The tosses use the track's stream, so they replay with its SEED. Off (no draw): BRANCH 0 or no TO */
static uint32_t tess_branch(uint32_t tr)
{
    tessera_evolve_t *ev = &tessera.evolve[tr];
    if (!ev->branch || !ev->br_to || ev->br_to > TESSERA_TRACKS || ev->br_to == tr + 1u) return tr;
    if (ev->br_mode == TESS_BR_COIN) return gen_rnd(&tessera.rnd[tr], 100u) < ev->branch ? ev->br_to - 1u : tr;
    if (ev->br_mode == TESS_BR_TOGGLE && gen_rnd(&tessera.rnd[tr], 100u) < ev->branch) ev->br_side ^= 1u;
    return ev->br_side ? ev->br_to - 1u : tr;
}
/* A BRANCHed note on track v's voice: kept in its seq_notes with this step's length, so its own gate countdown
 * (seq.c) or its next step lets it go (br_in: a generator's next step) */
static void tess_branch_on(track_t *v, uint32_t note, uint32_t vel, uint32_t off)
{
    uint32_t i;
    for (i = 0; i < v->seq_n && v->seq_notes[i] != note; i++)
        ;
    if (i == v->seq_n && v->seq_n >= sizeof v->seq_notes) return;   /* (full: not played, nothing to hang) */
    trk_note_on(v, note, vel);
    if (i == v->seq_n) v->seq_notes[v->seq_n++] = (uint8_t)note;
    if (v->seq_off < off) v->seq_off = off;
    tessera.br_in[trk_index(v)] = 1;
}

/* Capture (seq.c cap_gen): a note this track's step played on its own voice, off samples long, so REC held can
 * print the generator's last bars into its STEP pattern (ui_input.c capture_take) */
static void tess_cap(const track_t *t, uint32_t note, uint32_t vel, uint32_t off)
{
    uint32_t period = seq_div_samples((uint32_t)t->p[P_SDIV]);
    cap_gen(t, note, vel, period ? (uint32_t)((uint64_t)off * 256u / period) : 256u);
}

/* Grids: every part that hits plays its GM drum note (kick 36, snare 38, closed hat 42, the fourth its PART4: tom 45,
 * clap 39, open hat 46, rim 37, bell 56; a DRUM track's lanes),
 * each thinned by DROP and its velocity varied by HUMAN; the fourth, a near miss, a little softer; no scale, SHIFT, FOLLOW
 * or FLOURISH: they are drums. LOCK's replay is CHAOS turned round (gen_grids_tick). The first part sounding is the
 * track's note (FOLLOW, GATE, NOTE) */
static void tess_grids_play(track_t *t, uint32_t tr, const gen_out_t *out)
{
    const tessera_evolve_t *tw = &tessera.evolve[tr];
    uint32_t p, n = 0, off = seq_div_samples((uint32_t)t->p[P_SDIV]) * (uint32_t)(t->p[P_SGATE] ? t->p[P_SGATE] : 64) / 128u;
    for (p = 0; p < GEN_GRIDS_PARTS; p++) {
        uint32_t vel = (out->accent >> p) & 1u ? 127u : p == GEN_GRIDS_TOM ? 80u : 96u;
        if (!((out->parts >> p) & 1u)) continue;
        if (tw->drop && gen_rnd(&tessera.rnd[tr], 100u) < tw->drop) continue;
        if (tw->human) {
            int32_t j = (int32_t)(tw->human / 5u), v = (int32_t)vel + (int32_t)gen_rnd(&tessera.rnd[tr], (uint32_t)(2 * j + 1)) - j;
            vel = (uint32_t)(v < 1 ? 1 : v > 127 ? 127 : v);
        }
        {   /* ENERGY: a rest, the sparse tide (energy.c) */
            uint8_t v8 = (uint8_t)vel;
            if (!energy_hit(tr, &v8)) continue;
            vel = v8;
        }
        trk_note_on(t, gen_grids_note(&tessera.grids[tr], p), vel);
        seq_sub_note(t, gen_grids_note(&tessera.grids[tr], p), vel);   /* (SUB: RATCH repeats it) */
        tess_cap(t, gen_grids_note(&tessera.grids[tr], p), vel, off);
        t->seq_notes[n++] = (uint8_t)gen_grids_note(&tessera.grids[tr], p);
    }
    if (!n) return;
    tessera.hit[tr] = 1;
    tessera.last_note[tr] = t->seq_notes[0];
    tessera.active_gate[tr] = 1;
    t->seq_n = (uint8_t)n;
    t->seq_off = off;
}

/* a note's length in samples: the step's GATE, or len quarters of it (gen_out_t len; Constellation's star sizes), never
 * past the step */
static uint32_t tess_gate_len(const track_t *t, uint32_t period, uint32_t len)
{
    uint32_t g = (uint32_t)(t->p[P_SGATE] ? t->p[P_SGATE] : 64);
    if (len) g = g * len / 4u > 127u ? 127u : g * len / 4u;
    return period * g / 128u;
}

/* Chord: the chord on degree deg of the track's scale, from an octave below middle C (CHR: ROOT's major scale, as SCL
 * CHORD's DIA chords), as its SCL CHORD makes it (CHRD, DIA3 when OFF; VOIC) -> its notes (before TRN and SHIFT); *rp
 * its root, *maskp its tones above the root */
static uint32_t tess_chord_build(const track_t *t, uint32_t deg, uint8_t *out, int32_t *rp, uint16_t *maskp)
{
    uint32_t m = scale_mask(t) & 0xFFFu, mode = (uint32_t)t->p[P_CHRD], guard = 84;
    int32_t x = 48 + t->p[P_ROOT] % 12;
    if (m == 0xFFFu || !m) m = SCALE_MASK[1];
    while (deg && guard--)
        if ((m >> (uint32_t)((++x - t->p[P_ROOT] + 1200) % 12)) & 1u) deg--;
    return chord_make_as(t, mode && mode <= CH_POW ? mode : CH_DIA3, (uint32_t)x, out, rp, maskp);
}
/* chord i of the track's progression: its name ("Am", 12 bytes) -> its pitch classes (bit 0 C), its root's << 12 */
uint32_t tessera_chord_at(uint32_t tr, uint32_t i, char *name)
{
    const track_t *t = &trk[tr % TESSERA_TRACKS];
    uint8_t out[CHORD_MAX];
    int32_t r;
    uint16_t mask;
    uint32_t pc;
    tr %= TESSERA_TRACKS;
    tess_chord_build(t, gen_chord_degree(&tessera.chord[tr], i), out, &r, &mask);
    pc = (uint32_t)((r + t->p[P_TRANS] + tessera.evolve[tr].shift) % 12 + 12) % 12;
    mask = mask ? mask : 1u;
    chord_name(name, pc, mask);
    return (((uint32_t)mask << pc | (uint32_t)mask >> (12u - pc)) & 0xFFFu) | pc << 12;
}

/* Chord: the chord on the degree the generator gives (tess_chord_build), transposed (TRN, SHIFT's semitones). A new chord lets the last go; HOLD
 * holds it to its end, BEAT to the next beat, STEP for the step's GATE. DROP and HUMAN as on any hit; FOLLOW, FLOURISH,
 * BRANCH and NOTE LENGTH pass it by. Its pitch classes are QNT CHRD's (tessera_chord_pcs) */
/* the chord's notes (tess_chord_build's out, n) as tess_chord_play strikes them are the ones the track holds */
static int tess_chord_same(const track_t *t, const tessera_chord_t *g, const uint8_t *out, uint32_t n)
{
    const tessera_evolve_t *tw = &tessera.evolve[(uint32_t)(t - trk) % TESSERA_TRACKS];
    uint32_t i, k = 0;
    for (i = 0; i < n; i++) {
        int32_t x = (int32_t)out[i] + 12 * g->oct + t->p[P_TRANS] + tw->shift;
        x = x < 0 ? 0 : x > 127 ? 127 : x;
        if (k && t->seq_notes[k - 1u] == (uint8_t)x) continue;
        if (k >= t->seq_n || t->seq_notes[k] != (uint8_t)x) return 0;
        k++;
    }
    return k == t->seq_n;
}
/* BREATH: a voice's next breath, 11..19 s away in steps of the track's DIV (first: a part of it, so the voices start
 * out of step) */
static uint32_t tess_br_cycle(const track_t *t, uint32_t tr, int first)
{
    uint32_t period = seq_div_samples((uint32_t)t->p[P_SDIV]), s = (11u + gen_rnd(&tessera.rnd[tr], 9u)) * FS;
    uint32_t n = period ? s / period : 1u;
    if (first) n = n / 4u + gen_rnd(&tessera.rnd[tr], n * 3u / 4u + 1u);
    return n < 1u ? 1u : n > 65535u ? 65535u : n;
}
/* BREATH, each step of a held chord: a voice whose cycle is up breathes in again (struck anew, softer), or at
 * BREATH's chance sits its next cycle out (released); never more than one out, and never the lowest */
static void tess_chord_breathe(track_t *t, uint32_t tr)
{
    uint32_t i, n = t->seq_n < TESS_BR_VOICES ? t->seq_n : TESS_BR_VOICES, b = tessera.ch_breath[tr];
    if (!b || tessera.chord[tr].hit != GEN_CH_HOLD || !t->seq_n) return;
    for (i = 0; i < n; i++) {
        uint32_t note = t->seq_notes[i], bit = 1u << i;
        if (tess_br.left[tr][i] > 1u) { tess_br.left[tr][i]--; continue; }
        tess_br.left[tr][i] = (uint16_t)tess_br_cycle(t, tr, 0);
        if (tess_br.out[tr] & bit) {                     /* back in */
            tess_br.out[tr] &= (uint8_t)~bit;
            trk_note_on(t, note, 64u + gen_rnd(&tessera.rnd[tr], 25u));
        } else if (i && !tess_br.out[tr] && gen_rnd(&tessera.rnd[tr], 200u) < b) {
            tess_br.out[tr] |= (uint8_t)bit;             /* sits this one out */
            trk_note_off(t, note);
        } else {                                         /* breathes: released and struck again */
            trk_note_off(t, note);
            trk_note_on(t, note, 64u + gen_rnd(&tessera.rnd[tr], 25u));
        }
    }
}

static void tess_chord_play(track_t *t, uint32_t tr, uint32_t gate)
{
    const tessera_chord_t *g = &tessera.chord[tr];
    const tessera_evolve_t *tw = &tessera.evolve[tr];
    uint8_t out[CHORD_MAX];
    int32_t r, x;
    uint16_t mask;
    uint32_t vel = gate, n, i, period, br;
    if (!gate) {
        tess_chord_breathe(t, tr);
        return;
    }
    if (tw->drop && gen_rnd(&tessera.rnd[tr], 100u) < tw->drop) return;
    if (tw->human) {
        int32_t j = (int32_t)(tw->human / 5u), v = (int32_t)vel + (int32_t)gen_rnd(&tessera.rnd[tr], (uint32_t)(2 * j + 1)) - j;
        vel = (uint32_t)(v < 1 ? 1 : v > 127 ? 127 : v);
    }
    n = tess_chord_build(t, g->deg, out, &r, &mask);
    period = seq_div_samples((uint32_t)t->p[P_SDIV]);
    br = tessera.ch_breath[tr] && g->hit == GEN_CH_HOLD;
    if (br && t->seq_n && tess_chord_same(t, g, out, n)) {
        t->seq_off = period * g->hold + period;          /* BREATH: the same chord again is tied over, breathing on */
        tess_chord_breathe(t, tr);
        return;
    }
    seq_release(t);
    t->seq_n = 0;
    tess_br.out[tr] = 0;
    for (i = 0; i < TESS_BR_VOICES; i++)
        tess_br.left[tr][i] = (uint16_t)(br ? tess_br_cycle(t, tr, 1) : 0u);
    for (i = 0; i < n; i++) {
        x = (int32_t)out[i] + 12 * g->oct + t->p[P_TRANS] + tw->shift;
        x = x < 0 ? 0 : x > 127 ? 127 : x;
        if (t->seq_n && t->seq_notes[t->seq_n - 1u] == (uint8_t)x) continue;
        trk_note_on(t, (uint32_t)x, vel);
        seq_sub_note(t, (uint32_t)x, vel);
        t->seq_notes[t->seq_n++] = (uint8_t)x;
    }
    r += t->p[P_TRANS] + tw->shift;
    mask = mask ? mask : 1u;                             /* (a kit: the root alone) */
    tessera.chord_pc[tr] = (uint16_t)(((uint32_t)mask << ((r % 12 + 12) % 12) | (uint32_t)mask >> (12 - (r % 12 + 12) % 12)) & 0xFFFu);
    tessera.hit[tr] = 1;
    tessera.last_note[tr] = t->seq_notes[0];             /* (FOLLOW of it: the chord's lowest note) */
    /* (BREATH: held a step past the next chord, which ties it over or lets it go) */
    t->seq_off = br ? period * g->hold + period : g->hold ? period * g->hold - period / 16u : tess_gate_len(t, period, 0);
    for (i = 0; i < t->seq_n; i++)
        tess_cap(t, t->seq_notes[i], vel, t->seq_off);
}

static void tess_clock_step_body(track_t *t, uint32_t tr)
{
    tess_seed_fresh(tr);                                    /* before EVOLVE captures the phrase's start */
    tessera_evolve_tick(t, tr);

    tessera.hit[tr] = 0;
    if (t->p[P_MUTE]) {
        if (tessera.active_gate[tr]) {
            trk_note_off(t, tessera.last_note[tr]);
            tessera.active_gate[tr] = 0;
        }
        nl_stop(t);
        seq_release(t);
        tessera.br_in[tr] = 0;
        return;
    }

    if (tessera.active_gate[tr]) {
        trk_note_off(t, tessera.last_note[tr]);
        tessera.active_gate[tr] = 0;
        seq_release(t);
    } else if (tessera.br_in[tr] && !(tessera.mode[tr] == TESS_ENG_QUAD && tessera.quad[tr].dir == GEN_QD_HOLD)) {
        seq_release(t);                                    /* (another track's BRANCHed notes, still sounding; a held
                                                            * QUAD's LINKed ones keep their gate) */
    }
    tessera.br_in[tr] = 0;

    gen_out_t out = {0, 60, 100, 0, 0, 0};               /* a rest keeps note 60, vel 100 (LOCK records it) */
    gen_clock_t clk;
    clk.pos = tessera.evolve[tr].pos_now;
    clk.steps_per_bar = (uint8_t)tess_steps_per_bar(t);
    clk.root = (uint8_t)(t->p[P_ROOT] % 12);

    switch (tessera.mode[tr]) {
    case TESS_ENG_TURING:
        if (!sd_turing(t, tr, &out))                    /* (a SOURCE: its loop instead) */
            gen_turing_tick(&tessera.turing[tr], &clk, &tessera.rnd[tr], &out);
        break;
    case TESS_ENG_EUCLID:
        gen_euclid_tick(&tessera.euclid[tr], &clk, &tessera.rnd[tr], &out);
        sd_euclid(t, tr, &out);                         /* (a SOURCE: its notes) */
        break;
    case TESS_ENG_MARKOV:
        gen_markov_tick(&tessera.markov[tr], &clk, &tessera.rnd[tr], &out);
        break;
    case TESS_ENG_MORPH:
        gen_morph_tick(&tessera.morph[tr], &clk, &tessera.rnd[tr], &out);
        break;
    case TESS_ENG_GRIDS:
        gen_grids_tick(&tessera.grids[tr], &clk, &tessera.rnd[tr], &out);
        tessera.cv[tr] = out.cv;
        if (!tess_logic(tr, out.gate) || !tess_cond(tr)) out.parts = 0;   /* (OR, XOR: no part to add) */
        tess_grids_play(t, tr, &out);
        return;
    case TESS_ENG_BOUNCE:
        gen_bounce_tick(&tessera.bounce[tr], &clk, &tessera.rnd[tr], &out);
        break;
    case TESS_ENG_AWAKE:
        gen_awake_tick(&tessera.awake[tr], &clk, &tessera.rnd[tr], &out);
        break;
    case TESS_ENG_POLY:
        gen_poly_tick(&tessera.poly[tr], &clk, &tessera.rnd[tr], &out);
        break;
    case TESS_ENG_QUAD:
        tess_quad_cols(tr);
        gen_quad_tick(&tessera.quad[tr], &clk, &tessera.rnd[tr], &out);
        break;
    case TESS_ENG_CONSTEL:
        tessera.constel[tr].keep = tessera.evolve[tr].lock;
        gen_constel_tick(&tessera.constel[tr], &clk, &tessera.rnd[tr], &out);
        break;
    case TESS_ENG_SERIES:
        gen_series_tick(&tessera.series[tr], &out);
        break;
    case TESS_ENG_ANSWER:
        answer_tick(t, tr, &clk, &out);
        break;
    case TESS_ENG_CHORD:
        gen_chord_tick(&tessera.chord[tr], &clk, &out);
        tessera.cv[tr] = out.cv;
        tess_chord_play(t, tr, tess_logic(tr, out.gate) && tess_cond(tr) ? out.vel : 0u);
        return;
    }
    uint8_t note = out.note, vel = out.vel;
    int should_trigger = out.gate;
    tessera.cv[tr] = out.cv;

    if (tessera.mode[tr] == TESS_ENG_MARKOV || tessera.mode[tr] == TESS_ENG_MORPH)
        tessera_phrase_apply(tr, &note, &vel, &should_trigger);

    should_trigger = tess_logic(tr, should_trigger);       /* TWEAK 2: LOGIC, then COND, beside DROP */
    if (should_trigger && !tess_cond(tr)) should_trigger = 0;
    {   /* TWEAK: DROP thins any engine's hits; HUMAN varies the velocity (the gate below) */
        const tessera_evolve_t *tw = &tessera.evolve[tr];
        if (should_trigger && tw->drop && gen_rnd(&tessera.rnd[tr], 100u) < tw->drop) should_trigger = 0;
        if (should_trigger && tw->human) {
            int32_t j = (int32_t)(tw->human / 5u);                    /* up to +-20 */
            int32_t v = (int32_t)vel + (int32_t)gen_rnd(&tessera.rnd[tr], (uint32_t)(2 * j + 1)) - j;
            vel = (uint8_t)(v < 1 ? 1 : v > 127 ? 127 : v);
        }
        if (should_trigger && !energy_hit(tr, &vel)) should_trigger = 0;   /* ENERGY: a rest, the sparse tide */
    }
    if (tessera.mode[tr] == TESS_ENG_MARKOV)
        tessera.markov[tr].last_hit = (uint8_t)(should_trigger != 0);   /* after LOCK's replay: what really sounds */

    if (should_trigger) {
        const tessera_evolve_t *tw2 = &tessera.evolve[tr];
        int32_t base_note = (int32_t)note + tess_key(t, tr);
        int following = 0;
        if (tw2->follow) {          /* harmony: the other track's pitch in this scale, SHIFT scale degrees away */
            int32_t src_note = tess_follow_source((uint32_t)tw2->follow - 1u, tr);
            if (src_note >= 0) {
                base_note = tess_scale_steps(t, tessera_snap_note(t, src_note), tw2->shift);
                following = 1;
            }
        }
        base_note += tess_flourish(t, tr);                  /* before the snap: the flourish stays in the scale */
        base_note = base_note < 0 ? 0 : base_note > 127 ? 127 : base_note;
        int32_t final_note = tessera_snap_note(t, base_note);

        /* Apply track transpose and clamp to MIDI range */
        final_note += (int32_t)t->p[P_TRANS];
        if (!following) final_note += (int32_t)tw2->shift;
        if (final_note < 0) final_note = 0;
        if (final_note > 127) final_note = 127;

        uint32_t to = tess_branch(tr);
        if (to != tr) {                                    /* BRANCH: the hit plays on track to's voice instead */
            track_t *v = &trk[to];
            uint32_t off = tess_gate_len(t, seq_div_samples((uint32_t)t->p[P_SDIV]), out.len);
            tessera.last_note[tr] = (uint8_t)final_note;   /* (FOLLOW of this track: still its melody) */
            if (v->p[P_MUTE]) return;
            tess_branch_on(v, (uint32_t)final_note, vel, off);
            for (uint32_t i = 0; i < out.more_n && i < 2u; i++) {
                int32_t m = tessera_snap_note(t, (int32_t)out.more[i] + tess_key(t, tr) + tess_flourish(t, tr)) + (int32_t)t->p[P_TRANS] + tw2->shift;
                m = m < 0 ? 0 : m > 127 ? 127 : m;
                if (m != final_note) tess_branch_on(v, (uint32_t)m, vel, off);
            }
            return;
        }
        tessera.hit[tr] = 1;
        if (tessera.nl_mode[tr] != NL_STEP) {             /* NOTE LENGTH: held by note_len.c, not the step's GATE */
            uint32_t step = seq_div_samples((uint32_t)t->p[P_SDIV]);   /* (Capture: a step each, as it goes) */
            nl_on(t, tr, (uint32_t)final_note, vel, 0);
            tess_cap(t, (uint32_t)final_note, vel, step);
            tessera.last_note[tr] = (uint8_t)final_note;
            for (uint32_t i = 0; i < out.more_n && i < 2u; i++) {
                int32_t m = tessera_snap_note(t, (int32_t)out.more[i] + tess_key(t, tr) + tess_flourish(t, tr)) + (int32_t)t->p[P_TRANS] + tw2->shift;
                m = m < 0 ? 0 : m > 127 ? 127 : m;
                if (m != final_note) { nl_on(t, tr, (uint32_t)m, vel, 1); tess_cap(t, (uint32_t)m, vel, step); }
            }
            return;
        }
        trk_note_on(t, (uint32_t)final_note, vel);
        seq_sub_note(t, (uint32_t)final_note, vel);   /* (SUB: RATCH repeats it, seq_sub.c) */
        tessera.last_note[tr] = (uint8_t)final_note;
        tessera.active_gate[tr] = 1;
        t->seq_notes[0] = (uint8_t)final_note;
        t->seq_n = 1;
        for (uint32_t i = 0; i < out.more_n && i < 2u; i++) {   /* (Bounce: balls hitting on the same step) */
            int32_t m = tessera_snap_note(t, (int32_t)out.more[i] + tess_key(t, tr) + tess_flourish(t, tr)) + (int32_t)t->p[P_TRANS] + tw2->shift;
            m = m < 0 ? 0 : m > 127 ? 127 : m;
            if (m == final_note) continue;
            trk_note_on(t, (uint32_t)m, vel);
            seq_sub_note(t, (uint32_t)m, vel);
            t->seq_notes[t->seq_n++] = (uint8_t)m;
        }

        uint32_t period = seq_div_samples((uint32_t)t->p[P_SDIV]);
        t->seq_off = tess_gate_len(t, period, out.len);
        if (tw2->human)                                    /* shorten by up to HUMAN/2 percent */
            t->seq_off = t->seq_off * (100u - gen_rnd(&tessera.rnd[tr], tw2->human / 2u + 1u)) / 100u;
        for (uint32_t i = 0; i < t->seq_n; i++)
            tess_cap(t, t->seq_notes[i], vel, t->seq_off);
    }
}

/* SUB (seq.c seq_sub.c): the hits the step just played plays, RATCH's 1..4; CHANCE below 100 tosses for them on the
 * track's stream (so they replay with its SEED). RATCH x1, or a STEP track: 1, no draw */
uint32_t tessera_ratchet(uint32_t tr)
{
    if (tr >= TESSERA_TRACKS || tessera.mode[tr] == TESS_ENG_OFF || !tessera.sub_ratch[tr]) return 1u;
    if (tessera.sub_chance[tr] < 100u && gen_rnd(&tessera.rnd[tr], 100u) >= tessera.sub_chance[tr]) return 1u;
    return tessera.sub_ratch[tr] + 1u;
}

/* Constellation's SPEED: the track's straight DIVs (params.c N_DIV) from the slowest to the fastest; a triplet DIV (set
 * on PATTERN) counts as the straight one below it, so a turn leaves it for a straight one */
static const uint8_t TESS_CS_DIV[TESS_CS_SPEEDS] = {9, 8, 7, 6, 0, 1, 2, 3};
uint32_t tessera_cs_speed(uint32_t tr)
{
    uint32_t d = (uint32_t)trk[tr % TESSERA_TRACKS].p[P_SDIV], i;
    if (d == 4u) d = 0u;                                /* (8T: as 1/4; 16T: as 1/8) */
    else if (d == 5u) d = 1u;
    for (i = 0; i < sizeof TESS_CS_DIV; i++)
        if (TESS_CS_DIV[i] == d) return i;
    return 4u;                                          /* (1/4) */
}

/* QUAD's LINK as its knob steps through it: OFF, one of the other tracks (in order), two, all three; a mask of tracks */
static const uint8_t TESS_LINK_SLOTS[8] = {0, 1, 2, 4, 3, 5, 6, 7};   /* (bits: the other tracks, tess_other's order) */
static uint32_t tess_link_mask(uint32_t tr, uint32_t i)
{
    uint32_t m = 0;
    for (uint32_t b = 0; b < 3u; b++)
        if ((TESS_LINK_SLOTS[i & 7u] >> b) & 1u) m |= 1u << (tess_other(tr, b) - 1u);
    return m;
}
static uint32_t tess_link_idx(uint32_t tr, uint32_t mask)
{
    for (uint32_t i = 0; i < 8u; i++)
        if (tess_link_mask(tr, i) == (mask & ~(1u << tr) & 15u)) return i;
    return 0;
}

/* track tr's SEQ MODE: m a generator, or TESS_ENG_OFF its STEP pattern (GEN's KNOB 1; a print: ui_input.c capture_take,
 * its undo: ui.c undo_step); with the IRQs off */
void tessera_mode_set(uint32_t tr, uint32_t m)
{
    if (tr >= TESSERA_TRACKS || m >= TESS_ENG_COUNT) return;
    if ((uint8_t)m != tessera.mode[tr]) {
        nl_stop(&trk[tr]);
        seq_release(&trk[tr]);
        tessera.active_gate[tr] = 0;
    }
    tessera.mode[tr] = (uint8_t)m;
    tessera.awake[tr].edit = 0;                             /* (no step picked under another generator) */
    if (m != TESS_ENG_OFF) tess_seed_fresh(tr);             /* (its picture shows the material at once) */
    if (tessera.evolve[tr].form) tess_form_restart(tr);     /* the sections were another engine's */
    tess_rearm(tr);
    answer_restart(tr);
}

static void tess_param_set_body(uint32_t tr, uint32_t x, int32_t v);
static void tess_knob_body(uint32_t k, int32_t s)
{
    tessera_ensure_init();
    uint32_t tr = song.sel;
    if (tr >= TESSERA_TRACKS || !s) return;

    uint32_t mode = tessera.mode[tr];

    if (k <= 1u && mode == TESS_ENG_AWAKE && tessera.awake[tr].edit) {   /* a step picked: KNOB 1 its A, KNOB 2 its B */
        tessera_awake_t *a = &tessera.awake[tr];
        uint32_t i = a->edit & 15u, b = k;
        int32_t v = (int32_t)(b ? gen_awake_b(a, i) : gen_awake_a(a, i)) + (s > 0 ? 1 : -1);
        v = v < 0 ? 0 : v > (b ? 7 : 8) ? (b ? 7 : 8) : v;
        gen_awake_put(b ? a->b : a->a, i, (uint32_t)v);
        tess_rearm(tr);
        return;
    }
    if (k == 0u) {
        int32_t m = (int32_t)mode + s;
        if (m < 0) m = 0;
        if (m >= TESS_ENG_COUNT) m = TESS_ENG_COUNT - 1;
        tessera_mode_set(tr, (uint32_t)m);
        return;
    }

    if (mode == TESS_ENG_TURING) {
        tessera_turing_t *tur = &tessera.turing[tr];
        if (k == 1u) {
            int32_t len = (int32_t)tur->len + s;
            if (len < 2) len = 2;
            if (len > 16) len = 16;
            tur->len = (uint8_t)len;
            if (tur->step_idx >= tur->len) tur->step_idx = 0;
        } else if (k == 2u) {
            int32_t p = (int32_t)tur->prob + (s * 2);
            if (p < 0) p = 0;
            if (p > 100) p = 100;
            tur->prob = (uint8_t)p;
        } else if (k == 3u) {
            int32_t oct = (int32_t)tur->oct_span + s;
            if (oct < 1) oct = 1;
            if (oct > 3) oct = 3;
            tur->oct_span = (uint8_t)oct;
        }
    } else if (mode == TESS_ENG_EUCLID) {
        tessera_euclid_t *euc = &tessera.euclid[tr];
        if (k == 1u) {
            int32_t st = (int32_t)euc->steps + s;
            if (st < 1) st = 1;
            if (st > 32) st = 32;
            euc->steps = (uint8_t)st;
            if (euc->pulses > euc->steps) euc->pulses = euc->steps;
            if (euc->rot >= euc->steps) euc->rot = euc->steps > 1 ? (euc->steps - 1) : 0;
        } else if (k == 2u) {
            int32_t p = (int32_t)euc->pulses + s;
            if (p < 0) p = 0;
            if (p > (int32_t)euc->steps) p = (int32_t)euc->steps;
            euc->pulses = (uint8_t)p;
        } else if (k == 3u) {
            int32_t max_r = euc->steps > 1 ? (int32_t)euc->steps - 1 : 0;
            int32_t r = (int32_t)euc->rot + s;
            if (r < 0) r = 0;
            if (r > max_r) r = max_r;
            euc->rot = (uint8_t)r;
        }
    } else if (mode == TESS_ENG_MARKOV) {
        tessera_markov_t *mk = &tessera.markov[tr];
        if (k == 1u) {
            int32_t tens = (int32_t)mk->tension + (s * 4);
            if (tens < 0) tens = 0;
            if (tens > 100) tens = 100;
            mk->tension = (uint8_t)tens;
            tessera_markov_recalc(tr);
        } else if (k == 2u) {
            int32_t dens = (int32_t)mk->density;
            if (dens < 5 || dens > 100) dens = 100;
            dens += (s * 5);
            if (dens < 5) dens = 5;
            if (dens > 100) dens = 100;
            mk->density = (uint8_t)dens;
        } else if (k == 3u) {
            int32_t oct = (int32_t)mk->oct_span + s;
            if (oct < 1) oct = 1;
            if (oct > 3) oct = 3;
            mk->oct_span = (uint8_t)oct;
        }
    } else if (mode == TESS_ENG_MORPH) {
        tessera_morph_t *m = &tessera.morph[tr];
        if (k == 1u) {
            int32_t pos = (int32_t)m->morph_pos + (s * 4);
            if (pos < 0) pos = 0;
            if (pos > 127) pos = 127;
            m->morph_pos = (uint8_t)pos;
        } else if (k == 2u) {
            int32_t pa = ((int32_t)(m->step_idx >> 4) & 7) + s;
            if (pa < 0) pa = 0;
            if (pa > 7) pa = 7;
            m->step_idx = (m->step_idx & 0x0Fu) | ((uint8_t)pa << 4);
            m->snap_a = GEN_MORPH_PRESETS[pa];
        } else if (k == 3u) {
            int32_t pb = ((int32_t)m->oct_span & 7) + s;
            if (pb < 0) pb = 0;
            if (pb > 7) pb = 7;
            m->oct_span = (uint8_t)pb;
            m->snap_b = GEN_MORPH_PRESETS[pb];
        }
    } else if (mode == TESS_ENG_BOUNCE) {
        tessera_bounce_t *b = &tessera.bounce[tr];
        if (k == 1u) {
            int32_t v = (int32_t)b->speed + s;
            b->speed = (uint8_t)(v < 1 ? 1 : v > 64 ? 64 : v);
        } else if (k == 2u) {
            int32_t v = (int32_t)b->angle + s;
            b->angle = (uint8_t)(v < 0 ? 0 : v > 90 ? 90 : v);
        } else if (k == 3u) {
            int32_t v = (int32_t)b->balls + (s > 0 ? 1 : -1);   /* (past 4: FALL) */
            b->balls = (uint8_t)(v < 1 ? 1 : v > (int32_t)GEN_BOUNCE_FALL ? (int32_t)GEN_BOUNCE_FALL : v);
        }
    } else if (mode == TESS_ENG_AWAKE) {
        tessera_awake_t *a = &tessera.awake[tr];
        if (k == 1u || k == 2u) {
            uint8_t *l = k == 1u ? &a->len_a : &a->len_b;
            int32_t v = (int32_t)*l + s;
            *l = (uint8_t)(v < 1 ? 1 : v > 16 ? 16 : v);
        } else if (k == 3u) {
            int32_t v = (int32_t)a->prob + s * 2;
            a->prob = (uint8_t)(v < 0 ? 0 : v > 100 ? 100 : v);
        }
    } else if (mode == TESS_ENG_POLY) {
        tessera_poly_t *p = &tessera.poly[tr];
        if (k == 1u) {                                      /* RHY: which rhythm DIV edits (the page's) */
            int32_t v = (int32_t)p->sel + (s > 0 ? 1 : -1);
            p->sel = (uint8_t)(v < 0 ? 0 : v > 3 ? 3 : v);
            return;
        } else if (k == 2u) {
            int32_t v = (int32_t)p->div[p->sel & 3u] + s;
            p->div[p->sel & 3u] = (uint8_t)(v < 0 ? 0 : v > 16 ? 16 : v);
        } else if (k == 3u) {                               /* SPAN: the seed's pitches over 1 to 3 octaves */
            int32_t v = (int32_t)p->span + (s > 0 ? 1 : -1);
            v = v < 1 ? 1 : v > 3 ? 3 : v;
            if ((uint8_t)v != p->span) {
                p->span = (uint8_t)v;
                gen_poly_fill(p, tessera.seed[tr]);
            }
        }
    } else if (mode == TESS_ENG_QUAD) {
        tessera_quad_t *q = &tessera.quad[tr];
        if (k == 1u) {                                      /* DIR: FWD BACK PEND RAND HOLD */
            int32_t v = (int32_t)q->dir + (s > 0 ? 1 : -1);
            q->dir = (uint8_t)(v < 0 ? 0 : v >= GEN_QD_COUNT ? GEN_QD_COUNT - 1 : v);
        } else if (k == 2u) {
            int32_t v = (int32_t)q->prob + s * 2;
            q->prob = (uint8_t)(v < 0 ? 0 : v > 100 ? 100 : v);
        } else if (k == 3u) {                               /* LINK: OFF, each other track, two of them, all three */
            int32_t v = (int32_t)tess_link_idx(tr, q->link) + (s > 0 ? 1 : -1);
            v = v < 0 ? 0 : v > 7 ? 7 : v;
            q->link = (uint8_t)tess_link_mask(tr, (uint32_t)v);
        }
    } else if (mode == TESS_ENG_SERIES) {
        tessera_series_t *g = &tessera.series[tr];
        int32_t d = s > 0 ? 1 : -1, v;
        if (k == 1u) {
            v = (int32_t)g->start + s;
            g->start = (uint8_t)(v < 0 ? 0 : v > 127 ? 127 : v);
        } else if (k == 2u) {
            v = (int32_t)g->intv + d;
            g->intv = (uint8_t)(v < 1 ? 1 : v > 7 ? 7 : v);
        } else if (k == 3u) {
            v = (int32_t)g->loop + d;
            g->loop = (uint8_t)(v < 0 ? 0 : v > (int32_t)GEN_SR_LOOP ? (int32_t)GEN_SR_LOOP : v);
        }
    } else if (mode == TESS_ENG_CHORD) {                     /* PROG, BARS, HIT: a setting a click */
        const tessera_chord_t *c = &tessera.chord[tr];
        int32_t d = s > 0 ? 1 : -1;
        if (k == 1u) tess_param_set_body(tr, TX_CH_PROG, (int32_t)c->prog + d);
        else if (k == 2u) tess_param_set_body(tr, TX_CH_BARS, (int32_t)c->bars + d);
        else if (k == 3u) tess_param_set_body(tr, TX_CH_HIT, (int32_t)c->hit + d);
    } else if (mode == TESS_ENG_ANSWER) {                    /* VARY, ECHO, FADE */
        const tessera_answer_t *a = &tessera.answer[tr];
        if (k == 1u) tess_param_set_body(tr, TX_AN_VARY, (int32_t)a->vary + s * 2);
        else if (k == 2u) tess_param_set_body(tr, TX_AN_ECHO, (int32_t)a->echo + s * 2);
        else if (k == 3u) tess_param_set_body(tr, TX_AN_FADE, (int32_t)a->fade + s);
    } else if (mode == TESS_ENG_CONSTEL) {
        tessera_constel_t *c = &tessera.constel[tr];
        if (k == 1u) {                                      /* Y: a row a click */
            int32_t v = (int32_t)c->y + (s > 0 ? 1 : -1);
            c->y = (uint8_t)(v < 0 ? 0 : v >= (int32_t)GEN_CS_ROWS ? (int32_t)GEN_CS_ROWS - 1 : v);
        } else if (k == 2u) {
            int32_t v = (int32_t)c->dens + s * 2;
            c->dens = (uint8_t)(v < 0 ? 0 : v > 100 ? 100 : v);
        } else if (k == 3u) {                               /* SPEED: the track's DIV, slow to fast (a column a step) */
            uint32_t d = (uint32_t)trk[tr].p[P_SDIV];
            int32_t v = (int32_t)tessera_cs_speed(tr) + (s > 0 ? 1 : -1);
            if (s < 0 && (d == 4u || d == 5u)) v++;         /* (a triplet down: the straight one below it) */
            v = v < 0 ? 0 : v >= (int32_t)sizeof TESS_CS_DIV ? (int32_t)sizeof TESS_CS_DIV - 1 : v;
            trk[tr].p[P_SDIV] = TESS_CS_DIV[v];
        }
    } else if (mode == TESS_ENG_GRIDS) {
        tessera_grids_t *g = &tessera.grids[tr];
        if (k == 1u || k == 2u) {                           /* X, Y: 4 a click, a turn across the map */
            uint8_t *v = k == 1u ? &g->x : &g->y;
            int32_t n = (int32_t)*v + s * 4;
            *v = (uint8_t)(n < 0 ? 0 : n > 255 ? 255 : n);
        } else if (k == 3u) {
            int32_t d = (int32_t)g->dens + s * 2;
            g->dens = (uint8_t)(d < 0 ? 0 : d > 100 ? 100 : d);
        }
    }
    tess_rearm(tr);
}
static void tess_param_set_body(uint32_t tr, uint32_t x, int32_t v);
/* QUAD with OCT+ held on GEN: KNOB 1 the column (1..4), 2 its MOVE (STOP DOWN UP RAND), 3 its EVERY, 4 the track's
 * RANGE */
void tessera_quad_cols_knob(uint32_t k, int32_t s)
{
    uint32_t tr = song.sel, c;
    int32_t d = s > 0 ? 1 : -1, v;
    tessera_ensure_init();
    if (tr >= TESSERA_TRACKS || !s) return;
    TESS_IRQ_OFF();
    c = tessera.qcol_sel & 3u;
    if (k == 0u) {
        v = (int32_t)c + d;
        tessera.qcol_sel = (uint8_t)(v < 0 ? 0 : v > 3 ? 3 : v);
    } else if (k == 1u) {
        tess_param_set_body(tr, TX_QD_MOVE1 + c, (int32_t)(tessera.qcol[c] & 3u) + d);
    } else if (k == 2u) {
        tess_param_set_body(tr, TX_QD_EVERY1 + c, (int32_t)(tessera.qcol[c] >> 2) + d);
    } else if (k == 3u) {
        tess_param_set_body(tr, TX_QD_RANGE, (int32_t)tessera.quad[tr].range + d);
    }
    TESS_IRQ_ON();
}
/* CONSTELLATION with OCT+ held on GEN: KNOB 1 LOCK (EVOLVE's: how much of the loop stays), 2 LEN (the loop's slots), 3
 * FILL (a step with no star: OFF, ARP), 4 SIZE (the crosshair's catch) */
void tessera_constel_layer_knob(uint32_t k, int32_t s)
{
    uint32_t tr = song.sel;
    const tessera_constel_t *c;
    int32_t d = s > 0 ? 1 : -1;
    tessera_ensure_init();
    if (tr >= TESSERA_TRACKS || !s) return;
    c = &tessera.constel[tr];
    TESS_IRQ_OFF();
    if (k == 0u) tess_param_set_body(tr, TX_EV_LOCK, (int32_t)tessera.evolve[tr].lock + s * 2);
    else if (k == 1u) tess_param_set_body(tr, TX_CS_LEN, (int32_t)c->len + d);
    else if (k == 2u) tess_param_set_body(tr, TX_CS_FILL, (int32_t)c->fill + d);
    else if (k == 3u) tess_param_set_body(tr, TX_CS_SIZE, (int32_t)c->size + d);
    TESS_IRQ_ON();
}

/* GRIDS' fourth part: its sound's name; OCT+ held on GEN, a key: the sound of its note picked when it is one of them
 * (the DRUM lanes' black keys: CP OH TM RS CB), 0 else (BD, SD, CH: the other parts') */
const char *tessera_grids_part4_name(uint32_t tr)
{
    static const char *const N[GEN_GRIDS_P4_N] = {"TOM", "CLAP", "HATOP", "RIM", "BELL"};
    return N[tessera.grids[tr % TESSERA_TRACKS].part4 % GEN_GRIDS_P4_N];
}
int tessera_grids_part4_note(uint32_t tr, uint32_t note)
{
    for (uint32_t i = 0; i < GEN_GRIDS_P4_N; i++)
        if (GEN_GRIDS_P4_NOTE[i] == note && tr < TESSERA_TRACKS) {
            tessera_param_set(tr, TX_GRD_PART4, (int32_t)i);
            return 1;
        }
    return 0;
}
/* GRIDS with OCT+ held on GEN: KNOB 1 KICK, 2 SNR, 3 HAT (each part's share of DENS), 4 TOMS; 2 % a click, as DENS */
void tessera_grids_layer_knob(uint32_t k, int32_t s)
{
    uint32_t tr = song.sel;
    const tessera_grids_t *g;
    tessera_ensure_init();
    if (tr >= TESSERA_TRACKS || !s || k > 3u) return;
    g = &tessera.grids[tr];
    TESS_IRQ_OFF();
    if (k < 3u) tess_param_set_body(tr, TX_GRD_KICK + k, 100 - (int32_t)g->thin[k] + s * 2);
    else tess_param_set_body(tr, TX_GRD_TOMS, (int32_t)g->toms + s * 2);
    TESS_IRQ_ON();
}

/* CHORD USER, a key at a time (OCT+ held on GEN, PROG USER: ui_input.c): the key's note, rounded down to the track's
 * scale (CHR: ROOT's major, as tess_chord_build), is the next chord's root; first: a new progression. 0 = full (eight
 * already), else how many it has now */
uint32_t tessera_chord_user_key(uint32_t tr, uint32_t note, int first)
{
    const track_t *t;
    uint32_t m, rel, d = 0, ok;
    tessera_ensure_init();
    if (tr >= TESSERA_TRACKS) return 0;
    t = &trk[tr];
    m = scale_mask(t) & 0xFFFu;
    if (m == 0xFFFu || !m) m = SCALE_MASK[1];
    rel = (note + 1200u - (uint32_t)(t->p[P_ROOT] % 12)) % 12u;
    while (rel && !((m >> rel) & 1u)) rel--;            /* (the root itself is always in the scale) */
    for (uint32_t k = 0; k < rel; k++) d += (m >> k) & 1u;
    TESS_IRQ_OFF();
    ok = (uint32_t)gen_chord_user_add(&tessera.chord[tr], d, first);
    TESS_IRQ_ON();
    return ok ? tessera.chord[tr].ulen : 0u;
}

/* CHORD with OCT+ held on GEN, PROG USER, KNOB 4: USER's LEN */
void tessera_chord_ulen_knob(int32_t s)
{
    uint32_t tr = song.sel;
    tessera_ensure_init();
    if (tr >= TESSERA_TRACKS || !s) return;
    TESS_IRQ_OFF();
    tess_param_set_body(tr, TX_CH_ULEN, (int32_t)gen_chord_ulen(&tessera.chord[tr]) + (s > 0 ? 1 : -1));
    TESS_IRQ_ON();
}

/* CHORD with OCT+ held on GEN, KNOB 4 (not USER, whose LEN it is): BREATH, 5 % a detent */
void tessera_chord_breath_knob(int32_t s)
{
    uint32_t tr = song.sel;
    tessera_ensure_init();
    if (tr >= TESSERA_TRACKS || !s) return;
    TESS_IRQ_OFF();
    tess_param_set_body(tr, TX_CH_BREATH, (int32_t)tessera.ch_breath[tr] + (s > 0 ? 5 : -5));
    TESS_IRQ_ON();
}

/* CHORD with OCT+ held on GEN, KNOB 1: OCT (KNOB 2 and 3 are the track's CHRD and VOIC: ui_input.c) */
void tessera_chord_oct_knob(int32_t s)
{
    uint32_t tr = song.sel;
    tessera_ensure_init();
    if (tr >= TESSERA_TRACKS || !s) return;
    TESS_IRQ_OFF();
    tess_param_set_body(tr, TX_CH_OCT, (int32_t)tessera.chord[tr].oct + (s > 0 ? 1 : -1));
    TESS_IRQ_ON();
}

void tessera_knob(uint32_t k, int32_t s)
{
    TESS_IRQ_OFF();
    tess_knob_body(k, s);
    TESS_IRQ_ON();
}

void tessera_ensure_init(void)
{
    static uint8_t done;
    if (!done) { done = 1; tessera_init(); }
}

uint32_t tessera_sig(uint32_t tr)
{
    if (tr < TESSERA_TRACKS && tessera_step_on(tr))    /* (STEP EVOLVE: its EVOLVE page and the changes) */
        return tessera.sv_lock[tr] * 13u + tessera.sv_every[tr] * 61u + tessera.sv_amount[tr] * 29u +
               tess_sv.flash[tr] * 4093u + tess_sv.steps[tr] * 977u + tessera_step_varied(tr) * 131u;
    if (tr >= TESSERA_TRACKS || tessera.mode[tr] == TESS_ENG_OFF) return 0;
    uint32_t h = tessera.mode[tr] * 1009u + tessera.last_note[tr] * 53u + tessera.active_gate[tr];
    h ^= tessera.evolve[tr].bar_cnt * 977u + tessera.evolve[tr].flash * 4093u + tessera.evolve[tr].every * 61u + tessera.evolve[tr].amount * 29u + tessera.evolve[tr].lock * 13u + tessera.evolve[tr].form * 7u + tessera.evolve[tr].form_pos * 131u + tessera.evolve[tr].sec * 17u;
    switch (tessera.mode[tr]) {
    case TESS_ENG_TURING: { const tessera_turing_t *a = &tessera.turing[tr];
        h ^= a->reg * 31u + a->len * 7u + a->oct_span * 131u; break; }
    case TESS_ENG_EUCLID: { const tessera_euclid_t *a = &tessera.euclid[tr];
        h ^= a->steps * 31u + a->pulses * 7u + a->rot * 131u + a->step_idx * 17u + a->pitch_seed * 53u; break; }
    case TESS_ENG_MARKOV: { const tessera_markov_t *a = &tessera.markov[tr];
        h ^= a->cur_state * 31u + a->tension * 7u + a->density * 131u + a->last_hit * 977u; break; }
    case TESS_ENG_MORPH:  { const tessera_morph_t *a = &tessera.morph[tr];
        h ^= a->snap_a * 31u + a->snap_b * 7u + a->morph_pos * 131u + a->step_idx * 17u; break; }
    case TESS_ENG_GRIDS:  { const tessera_grids_t *a = &tessera.grids[tr];
        h ^= a->x * 31u + a->y * 7919u + a->dens * 131u + a->chaos * 977u + a->step_idx * 17u;
        h ^= (a->thin[0] | (uint32_t)a->thin[1] << 8 | (uint32_t)a->thin[2] << 16 | (uint32_t)a->toms << 24) * 2654435761u;
        break; }
    case TESS_ENG_BOUNCE: { const tessera_bounce_t *a = &tessera.bounce[tr];
        h ^= a->speed * 131u + a->angle * 977u + a->balls * 17u + a->dir * 3u + a->hit * 104729u;
        for (uint32_t i = 0; i < GEN_BOUNCE_MAX; i++) h = (h ^ a->x[i] ^ (uint32_t)a->y[i] << 8) * 16777619u;
        if (a->balls == GEN_BOUNCE_FALL)                /* (FALL: the leaves) */
            for (uint32_t i = 0; i < GEN_FALL_LEAVES; i++)
                h = (h ^ a->lx[i] ^ (uint32_t)a->ly[i] << 8 ^ (uint32_t)a->lt[i] << 16) * 16777619u;
        break; }
    case TESS_ENG_AWAKE:  { const tessera_awake_t *a = &tessera.awake[tr];
        h ^= a->pos_a * 31u + a->pos_b * 7919u + a->len_a * 131u + a->len_b * 977u + a->prob * 17u;
        for (uint32_t i = 0; i < 8u; i++) h = (h ^ a->a[i] ^ (uint32_t)a->b[i] << 8) * 16777619u;
        break; }
    case TESS_ENG_POLY:   { const tessera_poly_t *a = &tessera.poly[tr];
        h ^= a->pos * 31u + a->sel * 7919u + a->span * 131u + (a->tick % 16u) * 977u;
        for (uint32_t i = 0; i < 4u; i++) h = (h ^ a->div[i] ^ (uint32_t)a->pitch[i] << 8) * 16777619u;
        break; }
    case TESS_ENG_QUAD:   {   /* (the whole grid: another track's EVOLVE or LINK changes this one's picture) */
        for (uint32_t r = 0; r < TESSERA_TRACKS; r++) {
            const tessera_quad_t *a = &tessera.quad[r];
            h ^= a->pos * 31u + a->dir * 7919u + a->prob * 131u + a->link * 977u + tessera.mode[r] * 17u + a->range * 3u +
                 tessera.qcol[r] * 104729u + tessera.qcol_sel * 61u;
            for (uint32_t i = 0; i < 4u; i++) h = (h ^ a->cell[i]) * 16777619u;
        }
        break; }
    case TESS_ENG_CONSTEL: { const tessera_constel_t *a = &tessera.constel[tr];
        h ^= a->y * 31u + a->dens * 7919u + a->size * 131u + a->autop * 977u + a->caught * 17u + a->sky * 104729u +
             a->held * 61u + a->trail * 4093u + (uint32_t)trk[tr].p[P_SDIV] * 3u + a->len * 7u + a->fill * 65537u +
             a->filled * 1031u;
        h = (h ^ a->col ^ (uint32_t)a->ship << 16) * 16777619u;
        for (uint32_t i = 0; i < 16u; i++) h = (h ^ a->loop[i]) * 16777619u;
        break; }
    case TESS_ENG_SERIES: { const tessera_series_t *a = &tessera.series[tr];
        h = (h ^ (a->start | a->intv << 8 | a->loop << 16) ^ (uint32_t)a->n << 7) * 16777619u;
        break; }
    case TESS_ENG_CHORD: { const tessera_chord_t *a = &tessera.chord[tr];
        h = (h ^ (a->prog | a->bars << 4 | a->hit << 8 | a->rot << 12 | (uint32_t)a->deg << 16 | (uint32_t)a->idx << 20 |
                  (uint32_t)(a->oct + 2) << 24)) *
            16777619u;
        h = (h ^ (a->walk[0] | a->walk[1] << 4 | a->walk[2] << 8 | a->walk[3] << 12) ^ (uint32_t)a->n << 16) * 16777619u;
        break; }
    case TESS_ENG_ANSWER: { const tessera_answer_t *a = &tessera.answer[tr];
        h = (h ^ (a->vary | a->echo << 8 | a->fade << 16 | (uint32_t)a->n << 24)) * 16777619u;
        h = (h ^ (a->order | a->shift << 4 | a->space << 8 | a->len << 16)) * 16777619u;
        h = (h ^ (tess_an.phase[tr] | tess_an.ans[tr] << 4 | tess_an.slot[tr] << 12 | (uint32_t)tess_an.hclk[tr] << 16)) *
            16777619u;
        for (uint32_t i = 0; i < a->n && i < TESS_AN_NOTES; i++)
            h = (h ^ (a->note[i] | a->at[i] << 8 | tess_an.mut[tr][i] << 16 | (uint32_t)tess_an.ord[tr][i] << 24)) * 16777619u;
        break; }
    }
    return h;
}

/* QNT CHRD's chord: the one the first CHORD track plays (the merged picker may choose later), 0 none */
uint32_t tessera_chord_pcs(void)
{
    for (uint32_t i = 0; i < TESSERA_TRACKS; i++)
        if (tessera.mode[i] == TESS_ENG_CHORD && tessera.chord_pc[i]) return tessera.chord_pc[i];
    return 0;
}
uint32_t tessera_chord_snap(uint32_t pcs, int32_t n)
{
    n = n < 0 ? 0 : n > 127 ? 127 : n;
    if (pcs & 0xFFFu)
        for (int32_t d = 0; d < 12; d++) {
            if (n - d >= 0 && ((pcs >> ((n - d) % 12)) & 1u)) return (uint32_t)(n - d);
            if (d && n + d <= 127 && ((pcs >> ((n + d) % 12)) & 1u)) return (uint32_t)(n + d);
        }
    return (uint32_t)n;
}

/* Force a note down into the nearest active scale degree of the track; QNT CHRD with a CHORD track playing: to the
 * nearest note of its chord */
int32_t tessera_snap_note(const track_t *t, int32_t n)
{
    uint32_t mask = scale_mask(t), guard = 12, pcs;
    if (t->p[P_QUANT] == Q_CHRD && (pcs = tessera_chord_pcs()) != 0u) return (int32_t)tessera_chord_snap(pcs, n);
    while (guard-- && !((mask >> (uint32_t)((n - t->p[P_ROOT] + 120) % 12)) & 1u))
        n--;
    return n;
}

/* n (a note of t's scale) moved k notes of the scale up (k > 0) or down; a scale with no notes: k semitones */
static int32_t tess_scale_steps(const track_t *t, int32_t n, int32_t k)
{
    uint32_t mask = scale_mask(t) & 0xFFFu;
    int32_t dir = k < 0 ? -1 : 1;
    if (!mask)
        return n + k;
    for (; k; k -= dir) {
        uint32_t guard = 12;
        do n += dir;
        while (guard-- && !((mask >> (uint32_t)((n - t->p[P_ROOT] + 1200) % 12)) & 1u));
    }
    return n;
}

/* FOLLOW: the note track src gives track tr, -1 none. A generator: the last note it played. A STEP track: the highest
 * note it sounds now (a chord's top: the melody), else the last one it sounded (a rest keeps it) */
static int32_t tess_follow_source(uint32_t src, uint32_t tr)
{
    const track_t *s;
    int32_t n = -1;
    uint32_t i;
    if (src >= TESSERA_TRACKS || src == tr)
        return -1;
    if (tessera.mode[src] != TESS_ENG_OFF)
        return (int32_t)tessera.last_note[src];
    s = &trk[src];
    for (i = 0; i < s->seq_n; i++)
        if ((int32_t)s->seq_notes[i] > n)
            n = (int32_t)s->seq_notes[i];
    if (n >= 0)
        tessera.follow_mem[src] = (uint8_t)(n + 1);
    else if (tessera.follow_mem[src])
        n = (int32_t)tessera.follow_mem[src] - 1;
    return n;
}

/* The pitch Euclid plays on a step, as 0..14 (mirrors tessera_clock_step: 48 + (step*3)%12, snapped) */
uint32_t tessera_euclid_pitch(const track_t *t, uint32_t step)
{
    int32_t n = tessera_snap_note(t, 48 + (int32_t)(t->p[P_ROOT] % 12) + (int32_t)gen_euclid_degree(tessera.euclid[trk_index(t) % TESSERA_TRACKS].pitch_seed, step));
    n -= 45 + (int32_t)(t->p[P_ROOT] % 12);   /* (the picture: the same height in every key) */
    if (n < 0) n = 0;
    if (n > 14) n = 14;
    return (uint32_t)n;
}

/* ------------------------------------------------------------ persistence --- */
/* The project's Tessera block (project_tsp.c, the record's TESSERA part): per track the engine, each engine's settings, the
 * EVOLVE / FORM / TWEAK settings and the settings of the remembered sections (A, B, C). Not stored: the recorded
 * Markov / Morph phrases (a loaded project records them again on its first pass: RAM is the limit, see
 * project.c), the playheads and the counters. A block that fails any range check is refused whole. */
#define TESS_PK_TRACK 182u                           /* version 19: version 17's 150 bytes, then ANSWER (32) */
#define TESS_PK_TRACK_V17 150u                       /* version 17: version 16's 149 bytes, then CHORD's BREATH (1) */
#define TESS_PK_TRACK_V16 149u                       /* version 16: version 15's 147 bytes, then SUB's RATCH, CHANCE (2) */
#define TESS_PK_TRACK_V15 147u                       /* version 15: version 14's 146 bytes, then Grids' PART4 (1) */
#define TESS_PK_TRACK_V14 146u                      /* version 14: version 13's 143 bytes, then STEP EVOLVE (3) */
#define TESS_PK_TRACK_V13 143u                       /* version 13: version 12's 135 bytes, then Grids' parts (4), Chord's USER (4) */
#define TESS_PK_TRACK_V12 135u                       /* version 12: version 11's 130 bytes, then Chord (5) */
#define TESS_PK_TRACK_V11 130u                       /* version 11: version 10's 129 bytes, then Constellation's LEN, FILL (1) */
#define TESS_PK_TRACK_V10 129u                       /* version 10: version 9's 127 bytes, then Quad's RANGE, a column (2) */
#define TESS_PK_TRACK_V9 127u                        /* version 9: version 8's 120 bytes, then Series (3), Cells (4) */
#define TESS_PK_TRACK_V8 120u                        /* version 8: version 7's 98 bytes, then Constellation (22) */
#define TESS_PK_TRACK_V7 98u                         /* version 7: version 6's 95 bytes, then NOTE LENGTH (3) */
#define TESS_PK_TRACK_V6 95u                         /* version 6: version 5's 88 bytes, then Quad (7) */
#define TESS_PK_TRACK_V5 88u                         /* version 5: version 4's 80 bytes, then TWEAK 2 (6), SLOTH OCHD (2) */
#define TESS_PK_TRACK_V4 80u                         /* version 4: version 3's 51 bytes, then Bounce (3), Awake (19), Poly (7) */
#define TESS_PK_TRACK_V3 51u                         /* version 3: version 2's 47 bytes, then Grids (4) */
#define TESS_PK_TRACK_V2 47u                         /* version 2: version 1's 45 bytes, then SEED (2) */
#define TESS_PK_TRACK_V1 45u
#define TESS_PK_TAIL 2u                              /* version 18 on: after the tracks, the project's: ENERGY's ARC, RESTS */
_Static_assert(TESSERA_PK_BYTES == TESSERA_TRACKS * TESS_PK_TRACK + TESS_PK_TAIL, "TESSERA_PK_BYTES (tessera.h)");
_Static_assert(TESSERA_PK_BYTES_V18 == TESSERA_TRACKS * TESS_PK_TRACK_V17 + TESS_PK_TAIL, "TESSERA_PK_BYTES_V18 (tessera.h)");
_Static_assert(TESSERA_PK_BYTES_V17 == TESSERA_TRACKS * TESS_PK_TRACK_V17, "TESSERA_PK_BYTES_V17 (tessera.h)");
_Static_assert(TESSERA_PK_BYTES_V16 == TESSERA_TRACKS * TESS_PK_TRACK_V16, "TESSERA_PK_BYTES_V16 (tessera.h)");
_Static_assert(TESSERA_PK_BYTES_V15 == TESSERA_TRACKS * TESS_PK_TRACK_V15, "TESSERA_PK_BYTES_V15 (tessera.h)");
_Static_assert(TESSERA_PK_BYTES_V14== TESSERA_TRACKS * TESS_PK_TRACK_V14, "TESSERA_PK_BYTES_V14 (tessera.h)");
_Static_assert(TESSERA_PK_BYTES_V13 == TESSERA_TRACKS * TESS_PK_TRACK_V13, "TESSERA_PK_BYTES_V13 (tessera.h)");
_Static_assert(TESSERA_PK_BYTES_V12 == TESSERA_TRACKS * TESS_PK_TRACK_V12, "TESSERA_PK_BYTES_V12 (tessera.h)");
_Static_assert(TESSERA_PK_BYTES_V11 == TESSERA_TRACKS * TESS_PK_TRACK_V11, "TESSERA_PK_BYTES_V11 (tessera.h)");
_Static_assert(TESSERA_PK_BYTES_V10 == TESSERA_TRACKS * TESS_PK_TRACK_V10, "TESSERA_PK_BYTES_V10 (tessera.h)");
_Static_assert(TESSERA_PK_BYTES_V9 == TESSERA_TRACKS * TESS_PK_TRACK_V9, "TESSERA_PK_BYTES_V9 (tessera.h)");
_Static_assert(TESSERA_PK_BYTES_V8 == TESSERA_TRACKS * TESS_PK_TRACK_V8, "TESSERA_PK_BYTES_V8 (tessera.h)");
_Static_assert(TESSERA_PK_BYTES_V7 == TESSERA_TRACKS * TESS_PK_TRACK_V7, "TESSERA_PK_BYTES_V7 (tessera.h)");
_Static_assert(TESSERA_PK_BYTES_V6 == TESSERA_TRACKS * TESS_PK_TRACK_V6, "TESSERA_PK_BYTES_V6 (tessera.h)");
_Static_assert(TESSERA_PK_BYTES_V5 == TESSERA_TRACKS * TESS_PK_TRACK_V5, "TESSERA_PK_BYTES_V5 (tessera.h)");
_Static_assert(TESSERA_PK_BYTES_V4 == TESSERA_TRACKS * TESS_PK_TRACK_V4, "TESSERA_PK_BYTES_V4 (tessera.h)");
_Static_assert(TESSERA_PK_BYTES_V3 == TESSERA_TRACKS * TESS_PK_TRACK_V3, "TESSERA_PK_BYTES_V3 (tessera.h)");
_Static_assert(TESSERA_PK_BYTES_V2 == TESSERA_TRACKS * TESS_PK_TRACK_V2, "TESSERA_PK_BYTES_V2 (tessera.h)");
_Static_assert(TESSERA_PK_BYTES_V1 == TESSERA_TRACKS * TESS_PK_TRACK_V1, "TESSERA_PK_BYTES_V1 (tessera.h)");

static uint8_t *tess_put16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); return p + 2; }
static uint32_t tess_get16(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8); }

/* version 4's part (after version 3's 51 bytes): Bounce, Awake (its 32 values as they are held, 4 bits each), Poly
 * (its pitches 4 bits each) */
static uint8_t *tess_put_v4(uint8_t *o, const tessera_bounce_t *b, const tessera_awake_t *a, const tessera_poly_t *p)
{
    uint32_t i;
    *o++ = b->speed; *o++ = b->angle; *o++ = b->balls;
    *o++ = a->len_a; *o++ = a->len_b; *o++ = a->prob;
    for (i = 0; i < 8u; i++) *o++ = a->a[i];
    for (i = 0; i < 8u; i++) *o++ = a->b[i];
    for (i = 0; i < 4u; i++) *o++ = p->div[i];
    *o++ = (uint8_t)((p->pitch[0] & 15u) | (p->pitch[1] & 15u) << 4);
    *o++ = (uint8_t)((p->pitch[2] & 15u) | (p->pitch[3] & 15u) << 4);
    *o++ = (uint8_t)(p->span | (p->pitch[0] >> 4) << 4 | (p->pitch[1] >> 4) << 5 | (p->pitch[2] >> 4) << 6 |
                     (p->pitch[3] >> 4) << 7);   /* (SPAN, and the pitches' fifth bits: 3 octaves are 21 degrees) */
    return o;
}
/* version 6's part (after version 5's 88 bytes): Quad's row, DIR, PROB, LINK */
static uint8_t *tess_put_v6(uint8_t *o, const tessera_quad_t *q)
{
    for (uint32_t i = 0; i < 4u; i++) *o++ = q->cell[i];
    *o++ = q->dir; *o++ = q->prob; *o++ = q->link;
    return o;
}
/* version 8's part (after version 7's 98 bytes): Constellation's Y, DENS, SIZE, AUTO, sky and loop */
static uint8_t *tess_put_v8(uint8_t *o, const tessera_constel_t *c)
{
    *o++ = c->y; *o++ = c->dens; *o++ = c->size; *o++ = c->autop;
    o = tess_put16(o, c->sky);
    memcpy(o, c->loop, 16);
    return o + 16;
}
/* version 9's part (after version 8's 120 bytes): Series' START, INTV, LOOP; then 4 bytes CELLS had (cut: written 0,
 * never read) */
static uint8_t *tess_put_v9(uint8_t *o, const tessera_series_t *s)
{
    *o++ = s->start; *o++ = s->intv; *o++ = s->loop;
    *o++ = 0; *o++ = 0; *o++ = 0; *o++ = 0;
    return o;
}
/* version 10's part (after version 9's 127 bytes): Quad's RANGE, and the grid's column of the track's number (MOVE,
 * EVERY << 2). From version 10 Quad's LINK (version 6's part) is a mask of tracks, not one track */
static uint8_t *tess_put_v10(uint8_t *o, const tessera_quad_t *q, uint8_t col)
{
    *o++ = q->range; *o++ = col;
    return o;
}
/* version 11's part (after version 10's 129 bytes): Constellation's LEN - 1 | FILL << 4 */
static uint8_t *tess_put_v11(uint8_t *o, const tessera_constel_t *c)
{
    *o++ = (uint8_t)(((c->len >= 1u && c->len <= 16u ? c->len : 16u) - 1u) | c->fill << 4);
    return o;
}
/* version 12's part (after version 11's 130 bytes): Chord's PROG, BARS | HIT << 2 | OCT << 4 (3 bits, -2..2 as 6 7 0 1
 * 2), ROT, its walk (3 bits a chord) */
static uint8_t *tess_put_v12(uint8_t *o, const tessera_chord_t *c)
{
    *o++ = c->prog; *o++ = (uint8_t)(c->bars | c->hit << 2 | ((uint32_t)c->oct & 7u) << 4); *o++ = c->rot;
    return tess_put16(o, (uint16_t)(c->walk[0] | c->walk[1] << 3 | c->walk[2] << 6 | c->walk[3] << 9));
}
/* version 13's part (after version 12's 135 bytes): Grids' KICK, SNR, HAT turned round (as held: 0 all of DENS), TOMS;
 * Chord's USER: its eight degrees, 3 bits each (24 bits, low byte first), its length */
static uint8_t *tess_put_v13(uint8_t *o, const tessera_grids_t *g, const tessera_chord_t *c)
{
    uint32_t u = 0;
    *o++ = g->thin[0]; *o++ = g->thin[1]; *o++ = g->thin[2]; *o++ = g->toms;
    for (uint32_t i = 0; i < GEN_CH_USER_N; i++) u |= (uint32_t)(c->user[i] % 7u) << (3u * i);
    *o++ = (uint8_t)u; *o++ = (uint8_t)(u >> 8); *o++ = (uint8_t)(u >> 16);
    *o++ = c->ulen >= 1u && c->ulen <= GEN_CH_USER_N ? c->ulen : 1u;
    return o;
}
/* version 14's part (after version 13's 143 bytes): STEP EVOLVE's LOCK, EVERY, AMOUNT */
static uint8_t *tess_put_v14(uint8_t *o, uint32_t tr)
{
    *o++ = tessera.sv_lock[tr]; *o++ = tessera.sv_every[tr]; *o++ = tessera.sv_amount[tr];
    return o;
}
/* version 15's part (after version 14's 146 bytes): Grids' PART4 */
static uint8_t *tess_put_v15(uint8_t *o, const tessera_grids_t *g)
{
    *o++ = g->part4;
    return o;
}
/* version 16's part (after version 15's 147 bytes): SUB's RATCH and CHANCE */
static uint8_t *tess_put_v16(uint8_t *o, uint32_t tr)
{
    *o++ = tessera.sub_ratch[tr]; *o++ = tessera.sub_chance[tr];
    return o;
}
/* version 17's part (after version 16's 149 bytes): CHORD's BREATH */
static uint8_t *tess_put_v17(uint8_t *o, uint32_t tr)
{
    *o++ = tessera.ch_breath[tr];
    return o;
}
/* version 19's part (after version 17's 150 bytes; 18 added only the project's tail): ANSWER's VARY, ECHO, FADE,
 * ORDER, SHIFT, SPACE, LEN, its phrase's length, then its notes, velocities and steps */
static uint8_t *tess_put_v19(uint8_t *o, const tessera_answer_t *a)
{
    *o++ = a->vary; *o++ = a->echo; *o++ = a->fade; *o++ = a->order; *o++ = a->shift; *o++ = a->space; *o++ = a->len;
    *o++ = a->n;
    memcpy(o, a->note, TESS_AN_NOTES); o += TESS_AN_NOTES;
    memcpy(o, a->vel, TESS_AN_NOTES); o += TESS_AN_NOTES;
    memcpy(o, a->at, TESS_AN_NOTES);
    return o + TESS_AN_NOTES;
}
uint32_t tessera_pack(uint8_t *o)
{
    tessera_ensure_init();
    for (uint32_t tr = 0; tr < TESSERA_TRACKS; tr++) {
        const tessera_turing_t *tu = &tessera.turing[tr];
        const tessera_euclid_t *eu = &tessera.euclid[tr];
        const tessera_markov_t *mk = &tessera.markov[tr];
        const tessera_morph_t *mo = &tessera.morph[tr];
        const tessera_evolve_t *ev = &tessera.evolve[tr];
        *o++ = tessera.mode[tr];
        o = tess_put16(o, tu->reg); *o++ = tu->len; *o++ = tu->prob; *o++ = tu->oct_span;
        *o++ = eu->steps; *o++ = eu->pulses; *o++ = eu->rot; *o++ = eu->pitch_seed;
        *o++ = mk->tension; *o++ = mk->density; *o++ = mk->oct_span;
        *o++ = mo->morph_pos; o = tess_put16(o, mo->snap_a); o = tess_put16(o, mo->snap_b);
        *o++ = mo->oct_span; *o++ = (uint8_t)(mo->step_idx >> 4);     /* (the high nibble: the A preset) */
        *o++ = ev->every; *o++ = ev->amount; *o++ = ev->lock; *o++ = ev->form; *o++ = ev->contrast;
        *o++ = (uint8_t)ev->shift; *o++ = ev->drop; *o++ = ev->human; *o++ = ev->follow; *o++ = (uint8_t)ev->flourish;
        o = tess_put16(o, ev->base_reg); *o++ = ev->visited;
        for (uint32_t s = 0; s < 3u; s++) {
            const tessera_sec_t *sc = &tessera.sec[tr][s];
            o = tess_put16(o, sc->reg); *o++ = sc->p0; *o++ = sc->p1;
        }
        o = tess_put16(o, tessera.seed[tr]);
        *o++ = tessera.grids[tr].x; *o++ = tessera.grids[tr].y; *o++ = tessera.grids[tr].dens;
        *o++ = tessera.grids[tr].chaos;
        o = tess_put_v4(o, &tessera.bounce[tr], &tessera.awake[tr], &tessera.poly[tr]);
        *o++ = ev->branch; *o++ = ev->br_to; *o++ = ev->br_mode; *o++ = ev->logic; *o++ = ev->lg_with; *o++ = ev->cond;
        *o++ = tessera.org_speed[tr]; *o++ = tessera.org_rate[tr];
        o = tess_put_v6(o, &tessera.quad[tr]);
        *o++ = tessera.nl_mode[tr]; *o++ = tessera.nl_len[tr]; *o++ = tessera.nl_keys[tr];
        o = tess_put_v8(o, &tessera.constel[tr]);
        o = tess_put_v9(o, &tessera.series[tr]);
        o = tess_put_v10(o, &tessera.quad[tr], tessera.qcol[tr]);
        o = tess_put_v11(o, &tessera.constel[tr]);
        o = tess_put_v12(o, &tessera.chord[tr]);
        o = tess_put_v13(o, &tessera.grids[tr], &tessera.chord[tr]);
        o = tess_put_v14(o, tr);
        o = tess_put_v15(o, &tessera.grids[tr]);
        o = tess_put_v16(o, tr);
        o = tess_put_v17(o, tr);
        o = tess_put_v19(o, &tessera.answer[tr]);
    }
    *o++ = tessera.en_arc; *o++ = tessera.en_rests;      /* (version 18's tail: the project's ENERGY) */
    return TESSERA_PK_BYTES;
}

/* one track's record: range-checked, and (apply) written into `tessera` */
/* A block without a seed (version 1, before SEED): one from the track's own bytes, so the same project always gets
 * the same; 1..TESS_SEED_MAX */
static uint16_t tess_seed_of(const uint8_t *rec, uint32_t n)
{
    uint32_t h = 2166136261u;
    while (n--) h = (h ^ *rec++) * 16777619u;
    return (uint16_t)(1u + h % TESS_SEED_MAX);
}

static int tess_pk_track(const uint8_t *in, uint32_t tr, int apply)
{
    const uint8_t *rec = in;
    uint32_t mode = *in++;
    uint32_t t_reg = tess_get16(in); in += 2;
    uint32_t t_len = *in++, t_prob = *in++, t_oct = *in++;
    uint32_t e_steps = *in++, e_pulses = *in++, e_rot = *in++, e_seed = *in++;
    uint32_t k_tens = *in++, k_dens = *in++, k_oct = *in++;
    uint32_t m_pos = *in++, m_a = tess_get16(in), m_b = tess_get16(in + 2); in += 4;
    uint32_t m_oct = *in++, m_pa = *in++;
    uint32_t v_every = *in++, v_amount = *in++, v_lock = *in++, v_form = *in++, v_contrast = *in++;
    int32_t  v_shift = (int8_t)*in++;
    uint32_t v_drop = *in++, v_human = *in++, v_follow = *in++;
    int32_t  v_flour = (int8_t)*in++;
    uint32_t v_base = tess_get16(in); in += 2;
    uint32_t v_visited = *in++;
    uint32_t v_seed = tess_get16(rec + TESS_PK_TRACK_V1);
    const uint8_t *grd = rec + TESS_PK_TRACK_V2, *bnc = rec + TESS_PK_TRACK_V3, *awk = bnc + 3, *pol = awk + 19;
    const uint8_t *tw2 = rec + TESS_PK_TRACK_V4, *qd = rec + TESS_PK_TRACK_V5, *nl = rec + TESS_PK_TRACK_V6;
    const uint8_t *cs = rec + TESS_PK_TRACK_V7, *sc = rec + TESS_PK_TRACK_V8, *qc = rec + TESS_PK_TRACK_V9;
    const uint8_t *cl = rec + TESS_PK_TRACK_V10, *ch = rec + TESS_PK_TRACK_V11, *gp = rec + TESS_PK_TRACK_V12;
    const uint8_t *sv = rec + TESS_PK_TRACK_V13, *p4 = rec + TESS_PK_TRACK_V14, *sb = rec + TESS_PK_TRACK_V15;
    const uint8_t *chb = rec + TESS_PK_TRACK_V16, *an = rec + TESS_PK_TRACK_V17;
    uint32_t i, walk = tess_get16(ch + 3), user = gp[4] | (uint32_t)gp[5] << 8 | (uint32_t)gp[6] << 16;

    if (mode >= TESS_ENG_COUNT || v_seed > TESS_SEED_LOAD_MAX) return 0;
    if (t_len < 2u || t_len > 16u || t_prob > 100u || t_oct > 3u) return 0;
    if (e_steps < 1u || e_steps > 32u || e_pulses > e_steps || e_rot >= e_steps) return 0;
    if (k_tens > 100u || k_dens > 100u || k_oct > 3u) return 0;
    if (m_pos > 127u || m_oct > 7u || m_pa > 7u) return 0;
    if (v_every > 5u || v_amount > 100u || v_lock > 100u || v_form > 6u || v_contrast > 100u) return 0;
    if (v_shift < -24 || v_shift > 24 || v_drop > 100u || v_human > 100u || v_follow > TESSERA_TRACKS) return 0;
    if (v_flour < -12 || v_flour > 12 || v_visited > 7u) return 0;
    if (grd[2] > 100u || grd[3] > 100u || gp[0] > 100u || gp[1] > 100u || gp[2] > 100u || gp[3] > 100u) return 0;
    if (bnc[0] < 1u || bnc[0] > 64u || bnc[1] > 90u || bnc[2] < 1u || bnc[2] > GEN_BOUNCE_FALL) return 0;
    if (awk[0] < 1u || awk[0] > 16u || awk[1] < 1u || awk[1] > 16u || awk[2] > 100u) return 0;
    for (i = 0; i < 8u; i++)
        if ((awk[3 + i] & 0xFu) > 8u || awk[3 + i] >> 4 > 8u || (awk[11 + i] & 0xFu) > 7u || awk[11 + i] >> 4 > 7u) return 0;
    for (i = 0; i < 4u; i++)
        if (pol[i] > 16u) return 0;
    for (i = 0; i < 4u; i++)                         /* (a pitch: its nibble and its fifth bit in SPAN's byte) */
        if ((((pol[4 + i / 2u] >> ((i & 1u) * 4u)) & 0xFu) | ((pol[6] >> (4u + i)) & 1u) << 4) > 20u) return 0;
    if ((pol[6] & 15u) < 1u || (pol[6] & 15u) > 3u) return 0;
    if (tw2[0] > 100u || tw2[1] > TESSERA_TRACKS || tw2[2] >= TESS_BR_COUNT || tw2[3] >= TESS_LG_COUNT ||
        tw2[4] > TESSERA_TRACKS || tw2[5] >= TESS_COND_COUNT || tw2[6] > 100u || tw2[7] > 100u) return 0;
    for (i = 0; i < 4u; i++)
        if (qd[i] > 21u) return 0;
    if (qd[4] >= GEN_QD_COUNT || qd[5] > 100u || qd[6] > 15u) return 0;
    if (nl[0] >= NL_COUNT || nl[1] > 5u || nl[2] < 1u || nl[2] > NL_KEYS) return 0;
    if (cs[0] >= GEN_CS_ROWS || cs[1] > 100u || cs[2] > 7u || cs[3] > GEN_CS_SEEK) return 0;
    for (i = 0; i < 16u; i++)
        if (cs[6 + i] && !(cs[6 + i] & 15u)) return 0;      /* (a star has a height) */
    if (sc[0] > 127u || sc[1] < 1u || sc[1] > 7u || sc[2] > GEN_SR_LOOP) return 0;
    if (qc[0] >= GEN_QR_COUNT || (qc[1] >> 2) >= GEN_QC_EVERY_N) return 0;   /* (MOVE: any 2 bits) */
    if ((cl[0] >> 4) >= GEN_CS_FILL_COUNT) return 0;   /* (LEN - 1: any 4 bits) */
    if (ch[0] >= GEN_CH_PROGS || ((ch[1] >> 2) & 3u) >= GEN_CH_HITS || ch[2] > 7u || walk >> 12) return 0;
    if (ch[1] >> 7 || ((ch[1] >> 4) - 3u) < 3u) return 0;   /* (OCT: -2..2) */
    for (i = 0; i < GEN_CH_WALK_N; i++)
        if (((walk >> (3u * i)) & 7u) > 6u) return 0;
    for (i = 0; i < GEN_CH_USER_N; i++)
        if (((user >> (3u * i)) & 7u) > 6u) return 0;
    if (gp[7] < 1u || gp[7] > GEN_CH_USER_N) return 0;
    if (sv[0] > 100u || sv[1] > 5u || sv[2] > 100u || p4[0] >= GEN_GRIDS_P4_N) return 0;
    if (sb[0] >= SUB_HITS_MAX || sb[1] > 100u || chb[0] > 100u) return 0;
    if (an[0] > 100u || an[1] > 100u || an[2] > 100u || an[3] >= TESS_AN_ORDERS || an[4] > 7u || an[5] > 100u ||
        an[6] >= TESS_AN_LENS || an[7] > TESS_AN_NOTES) return 0;
    for (i = 0; i < TESS_AN_NOTES; i++)
        if (an[8 + i] > 127u || an[16 + i] > 127u || an[24 + i] > TESS_AN_AT_MAX) return 0;
    if (!apply) return 1;

    tessera.mode[tr] = (uint8_t)mode;
    tessera.turing[tr].reg = (uint16_t)t_reg; tessera.turing[tr].len = (uint8_t)t_len;
    tessera.turing[tr].prob = (uint8_t)t_prob; tessera.turing[tr].oct_span = (uint8_t)t_oct;
    tessera.euclid[tr].steps = (uint8_t)e_steps; tessera.euclid[tr].pulses = (uint8_t)e_pulses;
    tessera.euclid[tr].rot = (uint8_t)e_rot; tessera.euclid[tr].pitch_seed = (uint8_t)e_seed;
    tessera.markov[tr].tension = (uint8_t)k_tens; tessera.markov[tr].density = (uint8_t)k_dens;
    tessera.markov[tr].oct_span = (uint8_t)k_oct;
    tessera.morph[tr].morph_pos = (uint8_t)m_pos; tessera.morph[tr].snap_a = (uint16_t)m_a;
    tessera.morph[tr].snap_b = (uint16_t)m_b; tessera.morph[tr].oct_span = (uint8_t)m_oct;
    tessera.morph[tr].step_idx = (uint8_t)((m_pa << 4) | 0x0Fu);          /* (the engine pre-increments) */
    {
        tessera_evolve_t *ev = &tessera.evolve[tr];
        ev->every = (uint8_t)v_every; ev->amount = (uint8_t)v_amount; ev->lock = (uint8_t)v_lock;
        ev->form = (uint8_t)v_form; ev->contrast = (uint8_t)v_contrast; ev->shift = (int8_t)v_shift;
        ev->drop = (uint8_t)v_drop; ev->human = (uint8_t)v_human; ev->follow = (uint8_t)v_follow;
        ev->flourish = (int8_t)v_flour; ev->base_reg = (uint16_t)v_base; ev->visited = (uint8_t)v_visited;
        ev->branch = tw2[0]; ev->br_to = tw2[1]; ev->br_mode = tw2[2];
        ev->logic = tw2[3]; ev->lg_with = tw2[4]; ev->cond = tw2[5];
        ev->valid = 0;                                   /* no phrase is stored: every section records its own again */
        ev->rec = 2;
    }
    for (uint32_t s = 0; s < 3u; s++) {
        tessera_sec_t *sc = &tessera.sec[tr][s];
        sc->reg = (uint16_t)tess_get16(in); in += 2; sc->p0 = *in++; sc->p1 = *in++;
    }
    tessera.seed[tr] = (uint16_t)tess_get16(in);
    tessera.rnd[tr] = gen_stream(tessera.seed[tr]);       /* the stream from the top; the material is the block's */
    tessera.grids[tr].x = grd[0]; tessera.grids[tr].y = grd[1];
    tessera.grids[tr].dens = grd[2]; tessera.grids[tr].chaos = grd[3];
    memcpy(tessera.grids[tr].thin, gp, 3); tessera.grids[tr].toms = gp[3];
    tessera.bounce[tr].speed = bnc[0]; tessera.bounce[tr].angle = bnc[1]; tessera.bounce[tr].balls = bnc[2];
    tessera.awake[tr].len_a = awk[0]; tessera.awake[tr].len_b = awk[1]; tessera.awake[tr].prob = awk[2];
    for (i = 0; i < 8u; i++) tessera.awake[tr].a[i] = awk[3 + i], tessera.awake[tr].b[i] = awk[11 + i];
    for (i = 0; i < 4u; i++) tessera.poly[tr].div[i] = pol[i];
    for (i = 0; i < 4u; i++)
        tessera.poly[tr].pitch[i] = (uint8_t)(((pol[4 + i / 2u] >> ((i & 1u) * 4u)) & 0xFu) | ((pol[6] >> (4u + i)) & 1u) << 4);
    tessera.poly[tr].span = pol[6] & 15u;
    tessera.org_speed[tr] = tw2[6]; tessera.org_rate[tr] = tw2[7];
    for (i = 0; i < 4u; i++) tessera.quad[tr].cell[i] = qd[i];
    tessera.quad[tr].dir = qd[4]; tessera.quad[tr].prob = qd[5];
    tessera.quad[tr].link = (uint8_t)(qd[6] & ~(1u << tr));             /* (not itself) */
    tessera.quad[tr].range = qc[0];
    tessera.qcol[tr] = qc[1];
    tessera.nl_mode[tr] = nl[0]; tessera.nl_len[tr] = nl[1]; tessera.nl_keys[tr] = nl[2];
    tessera.constel[tr].y = cs[0]; tessera.constel[tr].dens = cs[1]; tessera.constel[tr].size = cs[2];
    tessera.constel[tr].autop = cs[3]; tessera.constel[tr].sky = (uint16_t)tess_get16(cs + 4);
    memcpy(tessera.constel[tr].loop, cs + 6, 16);
    tessera.constel[tr].len = (uint8_t)((cl[0] & 15u) + 1u); tessera.constel[tr].fill = (uint8_t)(cl[0] >> 4);
    gen_constel_start(&tessera.constel[tr]);
    tessera.series[tr].start = sc[0]; tessera.series[tr].intv = sc[1]; tessera.series[tr].loop = sc[2];
    gen_series_start(&tessera.series[tr]);
    tessera.chord[tr].prog = ch[0]; tessera.chord[tr].bars = ch[1] & 3u; tessera.chord[tr].hit = (uint8_t)((ch[1] >> 2) & 3u);
    tessera.chord[tr].oct = (int8_t)((ch[1] >> 4) >= 6u ? (int32_t)(ch[1] >> 4) - 8 : (int32_t)(ch[1] >> 4));
    tessera.chord[tr].rot = ch[2];
    for (i = 0; i < GEN_CH_WALK_N; i++) tessera.chord[tr].walk[i] = (uint8_t)((walk >> (3u * i)) & 7u);
    for (i = 0; i < GEN_CH_USER_N; i++) tessera.chord[tr].user[i] = (uint8_t)((user >> (3u * i)) & 7u);
    tessera.chord[tr].ulen = gp[7];
    gen_chord_start(&tessera.chord[tr]);
    tessera.sv_lock[tr] = sv[0]; tessera.sv_every[tr] = sv[1]; tessera.sv_amount[tr] = sv[2];
    tessera.grids[tr].part4 = p4[0];
    tessera.sub_ratch[tr] = sb[0]; tessera.sub_chance[tr] = sb[1];
    tessera.ch_breath[tr] = chb[0];
    tessera.chord_pc[tr] = 0;
    {
        tessera_answer_t *a = &tessera.answer[tr];
        a->vary = an[0]; a->echo = an[1]; a->fade = an[2]; a->order = an[3]; a->shift = an[4]; a->space = an[5];
        a->len = an[6]; a->n = an[7];
        memcpy(a->note, an + 8, TESS_AN_NOTES); memcpy(a->vel, an + 16, TESS_AN_NOTES); memcpy(a->at, an + 24, TESS_AN_NOTES);
        answer_restart(tr);
    }
    return 1;
}

/* A version 17 track record (150 bytes, before ANSWER; version 18 the same) -> version 19 (182): ANSWER's defaults,
 * nothing heard. out may not overlap in */
static void tess_v17_track(const uint8_t *in, uint8_t *out)
{
    tessera_answer_t a;
    memset(&a, 0, sizeof a);
    a.vary = 30; a.echo = 20; a.fade = 33; a.space = 25; a.shift = 2; a.len = TESS_AN_MID;   /* (tessera_init's) */
    memcpy(out, in, TESS_PK_TRACK_V17);
    tess_put_v19(out + TESS_PK_TRACK_V17, &a);
}

/* A version 16 track record (149 bytes, before CHORD's BREATH) -> version 17 (150): BREATH 0, as it played.
 * out may not overlap in */
static void tess_v16_track(const uint8_t *in, uint8_t *out)
{
    memcpy(out, in, TESS_PK_TRACK_V16);
    out[TESS_PK_TRACK_V16] = 0;
}
/* A version 15 track record (147 bytes, before SUB) -> version 16 (149): RATCH x1 (as it played), CHANCE 100.
 * out may not overlap in */
static void tess_v15_track(const uint8_t *in, uint8_t *out)
{
    memcpy(out, in, TESS_PK_TRACK_V15);
    out[TESS_PK_TRACK_V15] = 0;
    out[TESS_PK_TRACK_V15 + 1u] = 100;
}
/* A version 14 track record (146 bytes, before Grids' PART4) -> version 15 (147): the fourth part a tom, as it played.
 * out may not overlap in */
static void tess_v14_track(const uint8_t *in, uint8_t *out)
{
    memcpy(out, in, TESS_PK_TRACK_V14);
    out[TESS_PK_TRACK_V14] = 0;
}
/* A version 13 track record (143 bytes, before STEP EVOLVE) -> version 14 (146): STEP EVOLVE's defaults, AMOUNT 0
 * (the steps as written). out may not overlap in */
static void tess_v13_track(const uint8_t *in, uint8_t *out)
{
    memcpy(out, in, TESS_PK_TRACK_V13);
    out[TESS_PK_TRACK_V13] = TESS_SV_LOCK0; out[TESS_PK_TRACK_V13 + 1u] = TESS_SV_EVERY0; out[TESS_PK_TRACK_V13 + 2u] = 0;
}
/* A version 12 track record (135 bytes, before Grids' KICK, SNR, HAT, TOMS and Chord's USER) -> version 13 (143):
 * each part all of DENS, no TOMS, as it played; USER POP's chords. out may not overlap in */
static void tess_v12_track(const uint8_t *in, uint8_t *out)
{
    tessera_grids_t g;
    tessera_chord_t c;
    memset(&g, 0, sizeof g);
    memset(&c, 0, sizeof c);
    tess_chord_user_pop(&c);
    memcpy(out, in, TESS_PK_TRACK_V12);
    tess_put_v13(out + TESS_PK_TRACK_V12, &g, &c);
}

/* A version 11 track record (130 bytes, before CHORD) -> version 12 (135): Chord's defaults, its walk from the track's
 * seed (as ROLL would give it). A track of CELLS (cut; its number is CHORD's now) loads as TURING. out may not overlap
 * in */
static void tess_v11_track(const uint8_t *in, uint8_t *out)
{
    tessera_chord_t c;
    memset(&c, 0, sizeof c);
    c.bars = 1;
    gen_chord_fill(&c, tess_get16(in + TESS_PK_TRACK_V1));
    memcpy(out, in, TESS_PK_TRACK_V11);
    if (out[0] == TESS_ENG_RETIRED_CELLS) out[0] = TESS_ENG_TURING;
    tess_put_v12(out + TESS_PK_TRACK_V11, &c);
}

/* A version 10 track record (129 bytes, before CONSTELLATION's LEN and FILL) -> version 11 (130): the whole loop, FILL
 * OFF, as it played. out may not overlap in */
static void tess_v10_track(const uint8_t *in, uint8_t *out)
{
    tessera_constel_t c;
    memset(&c, 0, sizeof c);
    c.len = 16;
    memcpy(out, in, TESS_PK_TRACK_V10);
    tess_put_v11(out + TESS_PK_TRACK_V10, &c);
}

/* A version 9 track record (127 bytes, before QUAD's columns) -> version 10 (129): LINK's track become its bit, RANGE
 * three octaves, the column still (to move every 4 steps). out may not overlap in */
static void tess_v9_track(const uint8_t *in, uint8_t *out)
{
    uint8_t *link = out + TESS_PK_TRACK_V5 + 6u;
    tessera_quad_t q;
    memset(&q, 0, sizeof q);
    memcpy(out, in, TESS_PK_TRACK_V9);
    *link = (uint8_t)(*link >= 1u && *link <= TESSERA_TRACKS ? 1u << (*link - 1u) : 0u);
    tess_put_v10(out + TESS_PK_TRACK_V9, &q, 3u << 2);
}

/* A version 8 track record (120 bytes, before SERIES and CELLS) -> version 9 (127): SERIES' defaults, START from the
 * track's seed (as ROLL would give it); CELLS' 4 bytes 0. out may not overlap in */
static void tess_v8_track(const uint8_t *in, uint8_t *out)
{
    tessera_series_t s;
    memset(&s, 0, sizeof s);
    s.intv = 1;
    gen_series_fill(&s, tess_get16(in + TESS_PK_TRACK_V1));
    memcpy(out, in, TESS_PK_TRACK_V8);
    tess_put_v9(out + TESS_PK_TRACK_V8, &s);
}

/* A version 7 track record (98 bytes, before CONSTELLATION) -> version 8 (120): Constellation's defaults, its sky from
 * the track's seed (as ROLL would give it), the loop empty. out may not overlap in */
static void tess_v7_track(const uint8_t *in, uint8_t *out)
{
    tessera_constel_t c;
    memset(&c, 0, sizeof c);
    c.y = 7; c.dens = 50; c.size = 2;
    gen_constel_fill(&c, tess_get16(in + TESS_PK_TRACK_V1));
    memcpy(out, in, TESS_PK_TRACK_V7);
    tess_put_v8(out + TESS_PK_TRACK_V7, &c);
}

/* A version 6 track record (95 bytes, before NOTE LENGTH) -> version 7 (98): STEP (the GATE, so it plays as it did),
 * FIXED's 1/8, four keys. out may not overlap in */
static void tess_v6_track(const uint8_t *in, uint8_t *out)
{
    memcpy(out, in, TESS_PK_TRACK_V6);
    out[TESS_PK_TRACK_V6] = NL_STEP; out[TESS_PK_TRACK_V6 + 1u] = 2; out[TESS_PK_TRACK_V6 + 2u] = NL_KEYS;
}

/* A version 5 track record (88 bytes, before QUAD) -> version 6 (95): Quad's row from the track's seed (as ROLL would
 * give it), forward, every cell, no LINK. out may not overlap in */
static void tess_v5_track(const uint8_t *in, uint8_t *out)
{
    tessera_quad_t q;
    memset(&q, 0, sizeof q);
    q.prob = 100;
    gen_quad_fill(&q, tess_get16(in + TESS_PK_TRACK_V1));
    memcpy(out, in, TESS_PK_TRACK_V5);
    tess_put_v6(out + TESS_PK_TRACK_V5, &q);
}
/* A version 4 track record (80 bytes, before BRANCH, LOGIC, COND, SLOTH, OCHD) -> version 5 (88): TWEAK 2 off, so it
 * plays as it did, SLOTH and OCHD at their own speeds. out may not overlap in */
static void tess_v4_track(const uint8_t *in, uint8_t *out)
{
    memcpy(out, in, TESS_PK_TRACK_V4);
    memset(out + TESS_PK_TRACK_V4, 0, TESS_PK_TRACK_V5 - TESS_PK_TRACK_V4);
    out[TESS_PK_TRACK_V4 + 6u] = 50; out[TESS_PK_TRACK_V4 + 7u] = 50;
}
/* A version 3 track record (51 bytes, before BOUNCE, AWAKE, POLY) -> version 4 (80): their settings a fresh track's,
 * Awake's values and Poly's pitches from the track's seed (as ROLL would give them). out may not overlap in */
static void tess_v3_track(const uint8_t *in, uint8_t *out)
{
    tessera_bounce_t b;
    tessera_awake_t a;
    tessera_poly_t p;
    uint32_t sd = tess_get16(in + TESS_PK_TRACK_V1);
    memset(&b, 0, sizeof b);
    memset(&a, 0, sizeof a);
    memset(&p, 0, sizeof p);
    b.speed = 24, b.angle = 37, b.balls = 1;
    a.len_a = 8, a.len_b = 7, a.prob = 100;
    p.div[0] = 4, p.div[1] = 3, p.span = 1;
    gen_awake_fill(&a, sd);
    gen_poly_fill(&p, sd);
    memcpy(out, in, TESS_PK_TRACK_V3);
    tess_put_v4(out + TESS_PK_TRACK_V3, &b, &a, &p);
}
/* A version 2 track record (47 bytes, before GRIDS) -> version 3: Grids' settings a fresh track's (the map's middle,
 * DENSITY 50, no CHAOS) */
static void tess_v2_track(const uint8_t *in, uint8_t *out)
{
    memcpy(out, in, TESS_PK_TRACK_V2);
    out[TESS_PK_TRACK_V2] = 128; out[TESS_PK_TRACK_V2 + 1u] = 128;
    out[TESS_PK_TRACK_V2 + 2u] = 50; out[TESS_PK_TRACK_V2 + 3u] = 0;
}
/* A version 1 track record (45 bytes, before SEED) -> version 2: its seed from its own bytes, so the same project
 * always gets the same one; an OFF track 0 (a fresh one when a generator is first used) */
static void tess_v1_track(const uint8_t *in, uint8_t *out)
{
    memcpy(out, in, TESS_PK_TRACK_V1);
    tess_put16(out + TESS_PK_TRACK_V1, in[0] ? tess_seed_of(in, TESS_PK_TRACK_V1) : 0u);
}
uint32_t tessera_pk_bytes(uint32_t ver)
{
    return ver == 19u ? TESSERA_PK_BYTES : ver == 18u ? TESSERA_PK_BYTES_V18 : ver == 17u ? TESSERA_PK_BYTES_V17 : ver == 16u ? TESSERA_PK_BYTES_V16 : ver == 15u ? TESSERA_PK_BYTES_V15 : ver == 14u ? TESSERA_PK_BYTES_V14 : ver == 13u ? TESSERA_PK_BYTES_V13 : ver == 12u ? TESSERA_PK_BYTES_V12 : ver == 11u ? TESSERA_PK_BYTES_V11 : ver == 10u ? TESSERA_PK_BYTES_V10 : ver == 9u ? TESSERA_PK_BYTES_V9 : ver == 8u ? TESSERA_PK_BYTES_V8 : ver == 7u ? TESSERA_PK_BYTES_V7 : ver == 6u ? TESSERA_PK_BYTES_V6 : ver == 5u ? TESSERA_PK_BYTES_V5 : ver == 4u ? TESSERA_PK_BYTES_V4 : ver == 3u ? TESSERA_PK_BYTES_V3 :
           ver == 2u ? TESSERA_PK_BYTES_V2 : ver == 1u ? TESSERA_PK_BYTES_V1 : 0u;
}
/* A track record of version ver (1..19) -> version 19, a version at a time. out may not overlap in */
void tessera_track_up(uint32_t ver, const uint8_t *in, uint8_t *out)
{
    uint8_t v2[TESS_PK_TRACK_V2], v3[TESS_PK_TRACK_V3], v4[TESS_PK_TRACK_V4], v5[TESS_PK_TRACK_V5], v6[TESS_PK_TRACK_V6];
    uint8_t v7[TESS_PK_TRACK_V7], v8[TESS_PK_TRACK_V8], v9[TESS_PK_TRACK_V9], v10[TESS_PK_TRACK_V10];
    uint8_t v11[TESS_PK_TRACK_V11], v12[TESS_PK_TRACK_V12], v13[TESS_PK_TRACK_V13], v14[TESS_PK_TRACK_V14];
    uint8_t v15[TESS_PK_TRACK_V15], v16[TESS_PK_TRACK_V16], v17[TESS_PK_TRACK_V17];
    if (ver >= 19u) {
        memcpy(out, in, TESS_PK_TRACK);
        return;
    }
    if (ver >= 17u) {
        tess_v17_track(in, out);
        return;
    }
    if (ver <= 1u) { tess_v1_track(in, v2); in = v2; }
    if (ver <= 2u) { tess_v2_track(in, v3); in = v3; }
    if (ver <= 3u) { tess_v3_track(in, v4); in = v4; }
    if (ver <= 4u) { tess_v4_track(in, v5); in = v5; }
    if (ver <= 5u) { tess_v5_track(in, v6); in = v6; }
    if (ver <= 6u) { tess_v6_track(in, v7); in = v7; }
    if (ver <= 7u) { tess_v7_track(in, v8); in = v8; }
    if (ver <= 8u) { tess_v8_track(in, v9); in = v9; }
    if (ver <= 9u) { tess_v9_track(in, v10); in = v10; }
    if (ver <= 10u) { tess_v10_track(in, v11); in = v11; }
    if (ver <= 11u) { tess_v11_track(in, v12); in = v12; }
    if (ver <= 12u) { tess_v12_track(in, v13); in = v13; }
    if (ver <= 13u) { tess_v13_track(in, v14); in = v14; }
    if (ver <= 14u) { tess_v14_track(in, v15); in = v15; }
    if (ver <= 15u) { tess_v15_track(in, v16); in = v16; }
    tess_v16_track(in, v17);
    tess_v17_track(v17, out);
}

/* 1 = loaded; 0 = refused, `tessera` untouched */
int tessera_unpack(const uint8_t *in)
{
    const uint8_t *tail = in + TESSERA_TRACKS * TESS_PK_TRACK;
    tessera_ensure_init();
    for (uint32_t tr = 0; tr < TESSERA_TRACKS; tr++)
        if (!tess_pk_track(in + tr * TESS_PK_TRACK, tr, 0)) return 0;
    if (tail[0] > 100u || tail[1] > 100u) return 0;
    tessera_init();                                      /* the playheads and counters: as at power-on */
    for (uint32_t tr = 0; tr < TESSERA_TRACKS; tr++)
        tess_pk_track(in + tr * TESS_PK_TRACK, tr, 1);
    tessera.en_arc = tail[0];
    tessera.en_rests = tail[1];
    return 1;
}
/* a block of version ver (an older project's or backup's): as tessera_unpack, each track converted by
 * tessera_track_up; 0 refused (no such version, a range check) */
int tessera_unpack_ver(uint32_t ver, const uint8_t *in)
{
    uint8_t b[TESSERA_PK_BYTES];                        /* (main loop: on the stack) */
    uint32_t n = tessera_pk_bytes(ver) / TESSERA_TRACKS;   /* (18, 19: their 2-byte tail rounds away) */
    if (!n) return 0;
    if (ver == 19u) return tessera_unpack(in);
    for (uint32_t tr = 0; tr < TESSERA_TRACKS; tr++)
        tessera_track_up(ver, in + tr * n, b + tr * TESS_PK_TRACK);
    b[TESSERA_TRACKS * TESS_PK_TRACK] = ver == 18u ? in[TESSERA_TRACKS * n] : 0u;   /* (before 18: no ENERGY) */
    b[TESSERA_TRACKS * TESS_PK_TRACK + 1u] = ver == 18u ? in[TESSERA_TRACKS * n + 1u] : 0u;
    return tessera_unpack(b);
}
/* the same checks as tessera_unpack_ver, nothing loaded (project.c reads a record before it replaces anything): 1 a
 * block it would load */
int tessera_check(uint32_t ver, const uint8_t *in)
{
    uint8_t b[TESSERA_PK_BYTES];                        /* (main loop: on the stack) */
    uint32_t n = tessera_pk_bytes(ver) / TESSERA_TRACKS, tr;   /* (18, 19: their 2-byte tail rounds away) */
    if (!n) return 0;
    if (ver >= 18u && (in[TESSERA_TRACKS * n] > 100u || in[TESSERA_TRACKS * n + 1u] > 100u)) return 0;
    for (tr = 0; tr < TESSERA_TRACKS; tr++)
        tessera_track_up(ver, in + tr * n, b + tr * TESS_PK_TRACK);
    tessera_ensure_init();
    for (tr = 0; tr < TESSERA_TRACKS; tr++)
        if (!tess_pk_track(b + tr * TESS_PK_TRACK, tr, 0)) return 0;
    return 1;
}

/* every track back to OFF and its settings to their defaults (a project load starts from this) */
static void cord_reset(void);                           /* (cord.c) */
void tessera_reset(void)
{
    tessera_ensure_init();
    tessera_init();
    cord_reset();                                       /* the cords are the project's too: none */
}

/* ------------------------------------------------------------ the UI's view --- */
/* ------------------------------------------------------------ parameters by number (pid.c) --- */
static int32_t tess_clampi(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }

int32_t tessera_param_get(uint32_t tr, uint32_t x)
{
    const tessera_evolve_t *ev;
    if (tr >= TESSERA_TRACKS) return 0;
    ev = &tessera.evolve[tr];
    switch (x) {
    case TX_MODE: return tessera.mode[tr];
    case TX_TUR_LEN: return tessera.turing[tr].len;
    case TX_TUR_PROB: return tessera.turing[tr].prob;
    case TX_TUR_SPAN: return tessera.turing[tr].oct_span;
    case TX_EUC_STEPS: return tessera.euclid[tr].steps;
    case TX_EUC_PULSES: return tessera.euclid[tr].pulses;
    case TX_EUC_ROT: return tessera.euclid[tr].rot;
    case TX_MKV_LEAP: return tessera.markov[tr].tension;
    case TX_MKV_DENS: return tessera.markov[tr].density < 5u ? 100 : tessera.markov[tr].density;   /* (0 = 100) */
    case TX_MKV_SPAN: return tessera.markov[tr].oct_span;
    case TX_MRP_POS: return tessera.morph[tr].morph_pos;
    case TX_MRP_SNPA: return (tessera.morph[tr].step_idx >> 4) & 7u;
    case TX_MRP_SNPB: return tessera.morph[tr].oct_span & 7u;
    case TX_EV_LOCK:
        if (tessera.mode[tr] == TESS_ENG_GRIDS) return 100 - tessera.grids[tr].chaos;
        return tessera.mode[tr] == TESS_ENG_TURING ? 100 - tessera.turing[tr].prob : ev->lock;
    case TX_EV_EVERY: return ev->every;
    case TX_EV_AMOUNT: return ev->amount;
    case TX_EV_SEED: return tessera.roll[tr] ? tessera.roll[tr] : tessera.seed[tr];   /* (ROLL's, while it waits) */
    case TX_FM_FORM: return ev->form;
    case TX_FM_CONTRAST: return ev->contrast;
    case TX_FM_FLOURISH: return ev->flourish;
    case TX_TW_SHIFT: return ev->shift;
    case TX_TW_DROP: return ev->drop;
    case TX_TW_HUMAN: return ev->human;
    case TX_TW_FOLLOW: return ev->follow;
    case TX_GRD_X: return tessera.grids[tr].x;
    case TX_GRD_Y: return tessera.grids[tr].y;
    case TX_GRD_DENS: return tessera.grids[tr].dens;
    case TX_GRD_KICK: case TX_GRD_SNR: case TX_GRD_HAT: return 100 - tessera.grids[tr].thin[x - TX_GRD_KICK];
    case TX_GRD_TOMS: return tessera.grids[tr].toms;
    case TX_CH_ULEN: return gen_chord_ulen(&tessera.chord[tr]);
    case TX_SV_LOCK: return tessera.sv_lock[tr];
    case TX_SV_EVERY: return tessera.sv_every[tr];
    case TX_SV_AMOUNT: return tessera.sv_amount[tr];
    case TX_GRD_PART4: return tessera.grids[tr].part4;
    case TX_SUB_RATCH: return tessera.sub_ratch[tr];
    case TX_SUB_CHANCE: return tessera.sub_chance[tr];
    case TX_SUB_SHAPE: return tessera.sub_shape[tr];
    case TX_SUB_HUMAN: return tessera.sub_human[tr];
    case TX_MIDI_CH: return tessera.midi_ch[tr];
    case TX_KEY_LINK: return tessera.key_link;          /* (the project's, whichever track) */
    case TX_SD_SOURCE: return tessera.sd_src[tr];
    case TX_SD_KEEP: return tessera.sd_keep[tr];
    case TX_CH_BREATH: return tessera.ch_breath[tr];
    case TX_EN_ARC: return tessera.en_arc;              /* (the project's, whichever track) */
    case TX_EN_RESTS: return tessera.en_rests;
    case TX_AN_VARY: return tessera.answer[tr].vary;
    case TX_AN_ECHO: return tessera.answer[tr].echo;
    case TX_AN_FADE: return tessera.answer[tr].fade;
    case TX_AN_ORDER: return tessera.answer[tr].order;
    case TX_AN_SHIFT: return tessera.answer[tr].shift;
    case TX_AN_SPACE: return tessera.answer[tr].space;
    case TX_AN_LEN: return tessera.answer[tr].len;
    case TX_BNC_SPEED: return tessera.bounce[tr].speed;
    case TX_BNC_ANGLE: return tessera.bounce[tr].angle;
    case TX_BNC_BALLS: return tessera.bounce[tr].balls;
    case TX_AWK_LENA: return tessera.awake[tr].len_a;
    case TX_AWK_LENB: return tessera.awake[tr].len_b;
    case TX_AWK_PROB: return tessera.awake[tr].prob;
    case TX_POL_DIV1: case TX_POL_DIV2: case TX_POL_DIV3: case TX_POL_DIV4: return tessera.poly[tr].div[x - TX_POL_DIV1];
    case TX_POL_SPAN: return tessera.poly[tr].span;
    case TX_TW_BRANCH: return ev->branch;
    case TX_TW_BR_TO: return ev->br_to;
    case TX_TW_BR_MODE: return ev->br_mode;
    case TX_TW_LOGIC: return ev->logic;
    case TX_TW_LG_WITH: return ev->lg_with;
    case TX_TW_COND: return ev->cond;
    case TX_ORG_SPEED: return tessera.org_speed[tr];
    case TX_ORG_RATE: return tessera.org_rate[tr];
    case TX_QD_DIR: return tessera.quad[tr].dir;
    case TX_QD_PROB: return tessera.quad[tr].prob;
    case TX_QD_LINK: return tessera.quad[tr].link;
    case TX_QD_RANGE: return tessera.quad[tr].range;
    case TX_QD_MOVE1: case TX_QD_MOVE2: case TX_QD_MOVE3: case TX_QD_MOVE4: return tessera.qcol[x - TX_QD_MOVE1] & 3u;
    case TX_QD_EVERY1: case TX_QD_EVERY2: case TX_QD_EVERY3: case TX_QD_EVERY4: return tessera.qcol[x - TX_QD_EVERY1] >> 2;
    case TX_NL_MODE: return tessera.nl_mode[tr];
    case TX_NL_LEN: return tessera.nl_len[tr];
    case TX_NL_KEYS: return tessera.nl_keys[tr];
    case TX_CS_Y: return tessera.constel[tr].y;
    case TX_CS_DENS: return tessera.constel[tr].dens;
    case TX_CS_SIZE: return tessera.constel[tr].size;
    case TX_CS_AUTO: return tessera.constel[tr].autop;
    case TX_CS_LEN: return tessera.constel[tr].len;
    case TX_CS_FILL: return tessera.constel[tr].fill;
    case TX_SR_START: return tessera.series[tr].start;
    case TX_SR_INTV: return tessera.series[tr].intv;
    case TX_SR_LOOP: return tessera.series[tr].loop;
    case TX_CH_PROG: return tessera.chord[tr].prog;
    case TX_CH_BARS: return tessera.chord[tr].bars;
    case TX_CH_HIT: return tessera.chord[tr].hit;
    case TX_CH_OCT: return tessera.chord[tr].oct;
    }
    return 0;
}

/* the knobs' edits (tess_knob_body and the page bodies), by value instead of by a turn, on any track; a generator's
 * settings apply whatever its MODE, as a project load sets them */
static void tess_param_set_body(uint32_t tr, uint32_t x, int32_t v)
{
    tessera_evolve_t *ev = &tessera.evolve[tr];
    tessera_euclid_t *euc = &tessera.euclid[tr];
    tessera_morph_t *m = &tessera.morph[tr];
    uint32_t eng = 0;
    switch (x) {
    case TX_MODE:
        v = tess_clampi(v, 0, TESS_ENG_COUNT - 1);
        if ((uint8_t)v == tessera.mode[tr]) return;
        nl_stop(&trk[tr]);
        seq_release(&trk[tr]);
        tessera.active_gate[tr] = 0;
        tessera.mode[tr] = (uint8_t)v;
        if (v != TESS_ENG_OFF) tess_seed_fresh(tr);
        if (ev->form) tess_form_restart(tr);
        tess_rearm(tr);
        answer_restart(tr);
        return;
    case TX_TUR_LEN:
        tessera.turing[tr].len = (uint8_t)tess_clampi(v, 2, 16);
        if (tessera.turing[tr].step_idx >= tessera.turing[tr].len) tessera.turing[tr].step_idx = 0;
        eng = TESS_ENG_TURING; break;
    case TX_TUR_PROB: tessera.turing[tr].prob = (uint8_t)tess_clampi(v, 0, 100); eng = TESS_ENG_TURING; break;
    case TX_TUR_SPAN: tessera.turing[tr].oct_span = (uint8_t)tess_clampi(v, 1, 3); eng = TESS_ENG_TURING; break;
    case TX_EUC_STEPS:
        euc->steps = (uint8_t)tess_clampi(v, 1, 32);
        if (euc->pulses > euc->steps) euc->pulses = euc->steps;
        if (euc->rot >= euc->steps) euc->rot = euc->steps > 1 ? (euc->steps - 1) : 0;
        eng = TESS_ENG_EUCLID; break;
    case TX_EUC_PULSES: euc->pulses = (uint8_t)tess_clampi(v, 0, euc->steps); eng = TESS_ENG_EUCLID; break;
    case TX_EUC_ROT: euc->rot = (uint8_t)tess_clampi(v, 0, euc->steps > 1 ? euc->steps - 1 : 0); eng = TESS_ENG_EUCLID; break;
    case TX_MKV_LEAP: tessera.markov[tr].tension = (uint8_t)tess_clampi(v, 0, 100); eng = TESS_ENG_MARKOV; break;
    case TX_MKV_DENS: tessera.markov[tr].density = (uint8_t)tess_clampi(v, 5, 100); eng = TESS_ENG_MARKOV; break;
    case TX_MKV_SPAN: tessera.markov[tr].oct_span = (uint8_t)tess_clampi(v, 1, 3); eng = TESS_ENG_MARKOV; break;
    case TX_MRP_POS: m->morph_pos = (uint8_t)tess_clampi(v, 0, 127); eng = TESS_ENG_MORPH; break;
    case TX_MRP_SNPA:
        v = tess_clampi(v, 0, 7);
        m->step_idx = (uint8_t)((m->step_idx & 0x0Fu) | ((uint32_t)v << 4));
        m->snap_a = GEN_MORPH_PRESETS[v];
        eng = TESS_ENG_MORPH; break;
    case TX_MRP_SNPB:
        v = tess_clampi(v, 0, 7);
        m->oct_span = (uint8_t)v;
        m->snap_b = GEN_MORPH_PRESETS[v];
        eng = TESS_ENG_MORPH; break;
    case TX_EV_LOCK:
        v = tess_clampi(v, 0, 100);
        if (tessera.mode[tr] == TESS_ENG_TURING) {
            tessera.turing[tr].prob = (uint8_t)(100 - v);   /* (Turing's LOCK is its PROB turned round) */
        } else if (tessera.mode[tr] == TESS_ENG_GRIDS) {
            tessera.grids[tr].chaos = (uint8_t)(100 - v);   /* (Grids': its CHAOS turned round) */
        } else if (tessera_lock_get(tr) >= 0 || tessera.mode[tr] == TESS_ENG_OFF) {   /* (the others have no LOCK) */
            uint8_t was = ev->lock;
            ev->lock = (uint8_t)v;
            if (!was && ev->lock) tess_rearm(tr);
        }
        return;
    case TX_EV_EVERY: ev->every = (uint8_t)tess_clampi(v, 0, 5); ev->bar_cnt = 0; return;
    case TX_EV_AMOUNT: ev->amount = (uint8_t)tess_clampi(v, 0, 100); return;
    case TX_EV_SEED:
        tess_seed_fresh(tr);
        v = tess_clampi(v, 1, (int32_t)TESS_SEED_MAX);
        tessera.roll[tr] = (uint32_t)v == tessera.seed[tr] ? 0u : (uint16_t)v;   /* (as ROLL: on the bar line) */
        if (!song.playing) tess_roll_apply(tr);
        return;
    case TX_FM_FORM:
        v = tess_clampi(v, 0, 6);
        if ((uint8_t)v == ev->form) return;
        ev->form = (uint8_t)v;
        if (v && !ev->every) ev->every = 3;
        tess_form_restart(tr);
        return;
    case TX_FM_CONTRAST: ev->contrast = (uint8_t)tess_clampi(v, 0, 100); return;
    case TX_FM_FLOURISH: ev->flourish = (int8_t)tess_clampi(v, -12, 12); return;
    case TX_TW_SHIFT: ev->shift = (int8_t)tess_clampi(v, -24, 24); return;
    case TX_TW_DROP: ev->drop = (uint8_t)tess_clampi(v, 0, 100); return;
    case TX_TW_HUMAN: ev->human = (uint8_t)tess_clampi(v, 0, 100); return;
    case TX_TW_FOLLOW:
        v = tess_clampi(v, 0, TESSERA_TRACKS);
        ev->follow = (uint8_t)(v == (int32_t)tr + 1 ? 0 : v);   /* (not itself) */
        return;
    case TX_GRD_X: tessera.grids[tr].x = (uint8_t)tess_clampi(v, 0, 255); eng = TESS_ENG_GRIDS; break;
    case TX_GRD_Y: tessera.grids[tr].y = (uint8_t)tess_clampi(v, 0, 255); eng = TESS_ENG_GRIDS; break;
    case TX_GRD_DENS: tessera.grids[tr].dens = (uint8_t)tess_clampi(v, 0, 100); eng = TESS_ENG_GRIDS; break;
    case TX_GRD_KICK: case TX_GRD_SNR: case TX_GRD_HAT:
        tessera.grids[tr].thin[x - TX_GRD_KICK] = (uint8_t)(100 - tess_clampi(v, 0, 100)); eng = TESS_ENG_GRIDS; break;
    case TX_GRD_TOMS: tessera.grids[tr].toms = (uint8_t)tess_clampi(v, 0, 100); eng = TESS_ENG_GRIDS; break;
    case TX_CH_ULEN: tessera.chord[tr].ulen = (uint8_t)tess_clampi(v, 1, GEN_CH_USER_N); return;
    case TX_SV_LOCK: tessera.sv_lock[tr] = (uint8_t)tess_clampi(v, 0, 100); return;
    case TX_SV_EVERY: tessera.sv_every[tr] = (uint8_t)tess_clampi(v, 0, 5); tess_sv.steps[tr] = 0; return;
    case TX_SV_AMOUNT: tessera.sv_amount[tr] = (uint8_t)tess_clampi(v, 0, 100); return;
    case TX_GRD_PART4: tessera.grids[tr].part4 = (uint8_t)tess_clampi(v, 0, GEN_GRIDS_P4_N - 1); return;
    case TX_SUB_RATCH: tessera.sub_ratch[tr] = (uint8_t)tess_clampi(v, 0, SUB_HITS_MAX - 1); return;
    case TX_SUB_CHANCE: tessera.sub_chance[tr] = (uint8_t)tess_clampi(v, 0, 100); return;
    case TX_SUB_SHAPE: tessera.sub_shape[tr] = (uint8_t)tess_clampi(v, 0, SUB_SHAPES - 1); return;
    case TX_SUB_HUMAN: tessera.sub_human[tr] = (uint8_t)tess_clampi(v, 0, (int32_t)TESS_HUMAN_MS); return;
    case TX_MIDI_CH: tessera.midi_ch[tr] = (uint8_t)tess_clampi(v, 0, (int32_t)TESS_MIDI_OFF); return;
    case TX_KEY_LINK: tessera.key_link = (uint8_t)tess_clampi(v, 0, 1); return;
    case TX_SD_SOURCE:                                  /* (the loop filled again from the new source) */
        tessera.sd_src[tr] = (uint8_t)tess_clampi(v, 0, TESSERA_TRACKS);
        tess_sd.load[tr] = 1;
        return;
    case TX_SD_KEEP: tessera.sd_keep[tr] = (uint8_t)tess_clampi(v, 0, TESS_SD_KEEPS - 1); return;
    case TX_CH_BREATH: tessera.ch_breath[tr] = (uint8_t)tess_clampi(v, 0, 100); return;
    case TX_AN_VARY: tessera.answer[tr].vary = (uint8_t)tess_clampi(v, 0, 100); return;
    case TX_AN_ECHO: tessera.answer[tr].echo = (uint8_t)tess_clampi(v, 0, 100); return;
    case TX_AN_FADE: tessera.answer[tr].fade = (uint8_t)tess_clampi(v, 0, 100); return;
    case TX_AN_ORDER: tessera.answer[tr].order = (uint8_t)tess_clampi(v, 0, TESS_AN_ORDERS - 1); return;
    case TX_AN_SHIFT: tessera.answer[tr].shift = (uint8_t)tess_clampi(v, 0, 7); return;
    case TX_AN_SPACE: tessera.answer[tr].space = (uint8_t)tess_clampi(v, 0, 100); return;
    case TX_AN_LEN: tessera.answer[tr].len = (uint8_t)tess_clampi(v, 0, TESS_AN_LENS - 1); return;
    case TX_EN_ARC: tessera.en_arc = (uint8_t)tess_clampi(v, 0, 100); return;
    case TX_EN_RESTS:
        v = tess_clampi(v, 0, 100);
        if (v && !tessera.en_rests) {
            tessera.en_rests = (uint8_t)v;
            energy_rests_on();                          /* (the next rest counted from now) */
        }
        tessera.en_rests = (uint8_t)v;
        return;
    case TX_BNC_SPEED: tessera.bounce[tr].speed = (uint8_t)tess_clampi(v, 1, 64); eng = TESS_ENG_BOUNCE; break;
    case TX_BNC_ANGLE: tessera.bounce[tr].angle = (uint8_t)tess_clampi(v, 0, 90); eng = TESS_ENG_BOUNCE; break;
    case TX_BNC_BALLS: tessera.bounce[tr].balls = (uint8_t)tess_clampi(v, 1, GEN_BOUNCE_FALL); eng = TESS_ENG_BOUNCE; break;
    case TX_AWK_LENA: tessera.awake[tr].len_a = (uint8_t)tess_clampi(v, 1, 16); eng = TESS_ENG_AWAKE; break;
    case TX_AWK_LENB: tessera.awake[tr].len_b = (uint8_t)tess_clampi(v, 1, 16); eng = TESS_ENG_AWAKE; break;
    case TX_AWK_PROB: tessera.awake[tr].prob = (uint8_t)tess_clampi(v, 0, 100); eng = TESS_ENG_AWAKE; break;
    case TX_POL_DIV1: case TX_POL_DIV2: case TX_POL_DIV3: case TX_POL_DIV4:
        tessera.poly[tr].div[x - TX_POL_DIV1] = (uint8_t)tess_clampi(v, 0, 16); eng = TESS_ENG_POLY; break;
    case TX_POL_SPAN:
        v = tess_clampi(v, 1, 3);
        if ((uint8_t)v != tessera.poly[tr].span) {
            tessera.poly[tr].span = (uint8_t)v;
            gen_poly_fill(&tessera.poly[tr], tessera.seed[tr]);
        }
        eng = TESS_ENG_POLY; break;
    case TX_TW_BRANCH: ev->branch = (uint8_t)tess_clampi(v, 0, 100); return;
    case TX_TW_BR_TO:
        v = tess_clampi(v, 0, TESSERA_TRACKS);
        ev->br_to = (uint8_t)(v == (int32_t)tr + 1 ? 0 : v);   /* (not itself) */
        ev->br_side = 0;
        return;
    case TX_TW_BR_MODE: ev->br_mode = (uint8_t)tess_clampi(v, 0, TESS_BR_COUNT - 1); ev->br_side = 0; return;
    case TX_TW_LOGIC: ev->logic = (uint8_t)tess_clampi(v, 0, TESS_LG_COUNT - 1); return;
    case TX_TW_LG_WITH:
        v = tess_clampi(v, 0, TESSERA_TRACKS);
        ev->lg_with = (uint8_t)(v == (int32_t)tr + 1 ? 0 : v);   /* (not itself) */
        return;
    case TX_TW_COND: ev->cond = (uint8_t)tess_clampi(v, 0, TESS_COND_COUNT - 1); return;
    case TX_ORG_SPEED: tessera.org_speed[tr] = (uint8_t)tess_clampi(v, 0, 100); return;
    case TX_ORG_RATE: tessera.org_rate[tr] = (uint8_t)tess_clampi(v, 0, 100); return;
    case TX_QD_DIR: tessera.quad[tr].dir = (uint8_t)tess_clampi(v, 0, GEN_QD_COUNT - 1); eng = TESS_ENG_QUAD; break;
    case TX_QD_PROB: tessera.quad[tr].prob = (uint8_t)tess_clampi(v, 0, 100); eng = TESS_ENG_QUAD; break;
    case TX_QD_LINK:                                /* (a mask of tracks; its own bit kept, never followed) */
        tessera.quad[tr].link = (uint8_t)tess_clampi(v, 0, 15);
        eng = TESS_ENG_QUAD; break;
    case TX_QD_RANGE: tessera.quad[tr].range = (uint8_t)tess_clampi(v, 0, GEN_QR_COUNT - 1); eng = TESS_ENG_QUAD; break;
    case TX_QD_MOVE1: case TX_QD_MOVE2: case TX_QD_MOVE3: case TX_QD_MOVE4: {   /* (the grid's: whatever track) */
        uint8_t *c = &tessera.qcol[x - TX_QD_MOVE1];
        *c = (uint8_t)((*c & ~3u) | (uint32_t)tess_clampi(v, 0, GEN_QC_COUNT - 1));
        eng = TESS_ENG_QUAD; break; }
    case TX_QD_EVERY1: case TX_QD_EVERY2: case TX_QD_EVERY3: case TX_QD_EVERY4: {
        uint8_t *c = &tessera.qcol[x - TX_QD_EVERY1];
        *c = (uint8_t)((*c & 3u) | (uint32_t)tess_clampi(v, 0, GEN_QC_EVERY_N - 1) << 2);
        eng = TESS_ENG_QUAD; break; }
    case TX_NL_MODE: case TX_NL_LEN: case TX_NL_KEYS: nl_set_body(tr, x, v); return;
    case TX_CS_Y: tessera.constel[tr].y = (uint8_t)tess_clampi(v, 0, GEN_CS_ROWS - 1); return;
    case TX_CS_DENS: tessera.constel[tr].dens = (uint8_t)tess_clampi(v, 0, 100); return;
    case TX_CS_SIZE: tessera.constel[tr].size = (uint8_t)tess_clampi(v, 0, 7); return;
    case TX_CS_AUTO: tessera.constel[tr].autop = (uint8_t)tess_clampi(v, 0, GEN_CS_SEEK); return;
    case TX_CS_LEN: tessera.constel[tr].len = (uint8_t)tess_clampi(v, 1, 16); return;
    case TX_CS_FILL: tessera.constel[tr].fill = (uint8_t)tess_clampi(v, 0, GEN_CS_FILL_COUNT - 1); return;
    case TX_SR_START: tessera.series[tr].start = (uint8_t)tess_clampi(v, 0, 127); return;
    case TX_SR_INTV: tessera.series[tr].intv = (uint8_t)tess_clampi(v, 1, 7); return;
    case TX_SR_LOOP: tessera.series[tr].loop = (uint8_t)tess_clampi(v, 0, GEN_SR_LOOP); return;
    case TX_CH_PROG: tessera.chord[tr].prog = (uint8_t)tess_clampi(v, 0, GEN_CH_PROGS - 1); return;
    case TX_CH_BARS: tessera.chord[tr].bars = (uint8_t)tess_clampi(v, 0, GEN_CH_BARS - 1); return;
    case TX_CH_HIT: tessera.chord[tr].hit = (uint8_t)tess_clampi(v, 0, GEN_CH_HITS - 1); return;
    case TX_CH_OCT: tessera.chord[tr].oct = (int8_t)tess_clampi(v, -2, 2); return;
    default:
        return;
    }
    if (tessera.mode[tr] == eng) tess_rearm(tr);
}
/* a setting written as it is, no side effect (cord.c, around the step): clamped to its range now (PULSES to STEPS,
 * ROT below it), DENSITY's 0 kept as 100 */
void tessera_param_poke(uint32_t tr, uint32_t x, int32_t v)
{
    tessera_evolve_t *ev;
    if (tr >= TESSERA_TRACKS) return;
    ev = &tessera.evolve[tr];
    switch (x) {
    case TX_TUR_PROB: tessera.turing[tr].prob = (uint8_t)tess_clampi(v, 0, 100); break;
    case TX_TUR_LEN: tessera.turing[tr].len = (uint8_t)tess_clampi(v, 2, 16); break;
    case TX_EUC_PULSES: tessera.euclid[tr].pulses = (uint8_t)tess_clampi(v, 0, tessera.euclid[tr].steps); break;
    case TX_EUC_ROT:
        tessera.euclid[tr].rot = (uint8_t)tess_clampi(v, 0, tessera.euclid[tr].steps > 1 ? tessera.euclid[tr].steps - 1 : 0);
        break;
    case TX_MKV_LEAP: tessera.markov[tr].tension = (uint8_t)tess_clampi(v, 0, 100); break;
    case TX_MKV_DENS: tessera.markov[tr].density = (uint8_t)tess_clampi(v, 5, 100); break;
    case TX_MRP_POS: tessera.morph[tr].morph_pos = (uint8_t)tess_clampi(v, 0, 127); break;
    case TX_TW_SHIFT: ev->shift = (int8_t)tess_clampi(v, -24, 24); break;
    case TX_TW_DROP: ev->drop = (uint8_t)tess_clampi(v, 0, 100); break;
    case TX_TW_HUMAN: ev->human = (uint8_t)tess_clampi(v, 0, 100); break;
    case TX_TW_BRANCH: ev->branch = (uint8_t)tess_clampi(v, 0, 100); break;
    case TX_EV_AMOUNT: ev->amount = (uint8_t)tess_clampi(v, 0, 100); break;
    case TX_FM_CONTRAST: ev->contrast = (uint8_t)tess_clampi(v, 0, 100); break;
    case TX_GRD_X: tessera.grids[tr].x = (uint8_t)tess_clampi(v, 0, 255); break;
    case TX_GRD_Y: tessera.grids[tr].y = (uint8_t)tess_clampi(v, 0, 255); break;
    case TX_GRD_DENS: tessera.grids[tr].dens = (uint8_t)tess_clampi(v, 0, 100); break;
    case TX_GRD_KICK: case TX_GRD_SNR: case TX_GRD_HAT:
        tessera.grids[tr].thin[x - TX_GRD_KICK] = (uint8_t)(100 - tess_clampi(v, 0, 100)); break;
    case TX_GRD_TOMS: tessera.grids[tr].toms = (uint8_t)tess_clampi(v, 0, 100); break;
    case TX_CH_ULEN: tessera.chord[tr].ulen = (uint8_t)tess_clampi(v, 1, GEN_CH_USER_N); break;
    case TX_SV_LOCK: tessera.sv_lock[tr] = (uint8_t)tess_clampi(v, 0, 100); break;
    case TX_SV_AMOUNT: tessera.sv_amount[tr] = (uint8_t)tess_clampi(v, 0, 100); break;
    case TX_GRD_PART4: tessera.grids[tr].part4 = (uint8_t)tess_clampi(v, 0, GEN_GRIDS_P4_N - 1); break;
    case TX_SUB_HUMAN: tessera.sub_human[tr] = (uint8_t)tess_clampi(v, 0, (int32_t)TESS_HUMAN_MS); break;
    case TX_BNC_SPEED: tessera.bounce[tr].speed = (uint8_t)tess_clampi(v, 1, 64); break;
    case TX_BNC_ANGLE: tessera.bounce[tr].angle = (uint8_t)tess_clampi(v, 0, 90); break;
    case TX_AWK_PROB: tessera.awake[tr].prob = (uint8_t)tess_clampi(v, 0, 100); break;
    case TX_POL_DIV1: case TX_POL_DIV2: case TX_POL_DIV3: case TX_POL_DIV4:
        tessera.poly[tr].div[x - TX_POL_DIV1] = (uint8_t)tess_clampi(v, 0, 16); break;
    case TX_QD_PROB: tessera.quad[tr].prob = (uint8_t)tess_clampi(v, 0, 100); break;
    case TX_CS_Y: tessera.constel[tr].y = (uint8_t)tess_clampi(v, 0, GEN_CS_ROWS - 1); break;
    case TX_CS_DENS: tessera.constel[tr].dens = (uint8_t)tess_clampi(v, 0, 100); break;
    case TX_CS_SIZE: tessera.constel[tr].size = (uint8_t)tess_clampi(v, 0, 7); break;
    case TX_CS_AUTO: tessera.constel[tr].autop = (uint8_t)tess_clampi(v, 0, GEN_CS_SEEK); break;
    case TX_SR_START: tessera.series[tr].start = (uint8_t)tess_clampi(v, 0, 127); break;
    case TX_CH_PROG: tessera.chord[tr].prog = (uint8_t)tess_clampi(v, 0, GEN_CH_PROGS - 1); break;
    case TX_AN_VARY: tessera.answer[tr].vary = (uint8_t)tess_clampi(v, 0, 100); break;
    default: break;                                 /* (the others are no cord's: cord.c's destinations) */
    }
}

void tessera_param_set(uint32_t tr, uint32_t x, int32_t v)
{
    if (tr >= TESSERA_TRACKS || x >= TX_COUNT) return;
    tessera_ensure_init();
    TESS_IRQ_OFF();
    tess_param_set_body(tr, x, v);
    TESS_IRQ_ON();
}

const gen_turing_t *tessera_turing(uint32_t tr) { return &tessera.turing[tr % TESSERA_TRACKS]; }
const gen_euclid_t *tessera_euclid(uint32_t tr) { return &tessera.euclid[tr % TESSERA_TRACKS]; }
const gen_markov_t *tessera_markov(uint32_t tr) { return &tessera.markov[tr % TESSERA_TRACKS]; }
const gen_morph_t *tessera_morph(uint32_t tr) { return &tessera.morph[tr % TESSERA_TRACKS]; }
const gen_grids_t *tessera_grids(uint32_t tr) { return &tessera.grids[tr % TESSERA_TRACKS]; }
const gen_bounce_t *tessera_bounce(uint32_t tr) { return &tessera.bounce[tr % TESSERA_TRACKS]; }
const gen_awake_t *tessera_awake(uint32_t tr) { return &tessera.awake[tr % TESSERA_TRACKS]; }
const gen_poly_t *tessera_poly(uint32_t tr) { return &tessera.poly[tr % TESSERA_TRACKS]; }
const gen_quad_t *tessera_quad(uint32_t tr) { return &tessera.quad[tr % TESSERA_TRACKS]; }
const gen_constel_t *tessera_constel(uint32_t tr) { return &tessera.constel[tr % TESSERA_TRACKS]; }
const gen_series_t *tessera_series(uint32_t tr) { return &tessera.series[tr % TESSERA_TRACKS]; }
const gen_chord_t *tessera_chord(uint32_t tr) { return &tessera.chord[tr % TESSERA_TRACKS]; }
const tessera_answer_t *tessera_answer(uint32_t tr) { return &tessera.answer[tr % TESSERA_TRACKS]; }
void tessera_awake_select(uint32_t tr, uint32_t step)
{
    uint8_t e;
    if (tr >= TESSERA_TRACKS) return;
    e = step < 16u ? (uint8_t)(0x20u | step) : 0u;
    tessera.awake[tr].edit = tessera.awake[tr].edit == e ? 0u : e;   /* (the same again: none) */
}

#include "vary.c"                                     /* a phrase played with changes (STEP EVOLVE, ANSWER) */
#include "step_evolve.c"                              /* STEP EVOLVE: a STEP track's EVOLVE page */
#include "answer.c"                                   /* ANSWER: the keys' phrase answered */
