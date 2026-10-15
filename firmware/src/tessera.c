/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* Tessera: the note generators of tracks 1-4 (Turing, Euclid, Markov, Morph), EVOLVE, FORM and TWEAK, clocked by the
 * sequencer (seq.c) in place of a track's steps; their state is saved with the project (project.c, OBJ_TESSERA). */
#include "tessera.h"

tessera_state_t tessera;

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

void tessera_init(void)
{
    uint8_t *p = (uint8_t *)&tessera;
    for (uint32_t b = 0; b < sizeof(tessera); b++) p[b] = 0;

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
        tessera.morph[i].morph_pos = 0;
        tessera.morph[i].snap_a = 0x8888u;
        tessera.morph[i].snap_b = 0xA5A5u;
        tessera.morph[i].step_idx = 0;
        tessera.morph[i].oct_span = 2;

        /* 5. Evolve */
        tessera.evolve[i].amount = 30;
        tessera.evolve[i].rec = 2;
        tessera.evolve[i].contrast = 60;
    }
}

void tessera_kill(track_t *t)
{
    uint32_t tr = trk_index(t);
    if (tr >= TESSERA_TRACKS) return;
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
        tess_realign(i);
        tess_form_reset(i);
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
    tessera.euclid[tr].step_idx = 0;
    tessera.morph[tr].step_idx = (uint8_t)((tessera.morph[tr].step_idx & 0xF0u) | 0x0Fu);  /* engine pre-increments */
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

/* LOCK = deja vu: 100 frozen loop, 0 fresh every pass. Turing: exactly 100 - prob. */
int tessera_lock_get(uint32_t tr)
{
    if (tr >= TESSERA_TRACKS) return -1;
    if (tessera.mode[tr] == TESS_ENG_TURING) return 100 - (int)tessera.turing[tr].prob;
    if (tessera.mode[tr] == TESS_ENG_MARKOV || tessera.mode[tr] == TESS_ENG_MORPH) return tessera.evolve[tr].lock;
    return -1;                                               /* Euclid is already a fixed loop: no LOCK */
}

void tessera_phrase(uint32_t tr, uint32_t *bar, uint32_t *of, int *flash)
{
    const tessera_evolve_t *ev = &tessera.evolve[tr < TESSERA_TRACKS ? tr : 0u];
    *of = tessera_every_bars(ev->every);
    *bar = ev->bar_cnt;
    *flash = ev->flash != 0;
}

/* EVOLVE page encoders: 0 LOCK, 1 EVERY, 2 AMOUNT, 3 ROLL */
static void tess_evolve_knob_body(uint32_t k, int32_t s)
{
    tessera_ensure_init();
    uint32_t tr = song.sel;
    if (tr >= TESSERA_TRACKS || !s) return;
    tessera_evolve_t *ev = &tessera.evolve[tr];

    if (k == 0u) {
        if (tessera_lock_get(tr) < 0) return;
        if (tessera.mode[tr] == TESS_ENG_TURING) {
            int32_t p = (int32_t)tessera.turing[tr].prob - s * 2;   /* clockwise = more locked */
            tessera.turing[tr].prob = (uint8_t)(p < 0 ? 0 : p > 100 ? 100 : p);
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

static int32_t tess_scale_steps(const track_t *t, int32_t n, int32_t k);
static int32_t tess_follow_source(uint32_t src, uint32_t tr);

void tessera_clock_step(track_t *t)
{
    uint32_t tr = trk_index(t);
    if (tr >= TESSERA_TRACKS) return;

    tess_seed_fresh(tr);                                    /* before EVOLVE captures the phrase's start */
    tessera_evolve_tick(t, tr);

    if (t->p[P_MUTE]) {
        if (tessera.active_gate[tr]) {
            trk_note_off(t, tessera.last_note[tr]);
            tessera.active_gate[tr] = 0;
        }
        seq_release(t);
        return;
    }

    if (tessera.active_gate[tr]) {
        trk_note_off(t, tessera.last_note[tr]);
        tessera.active_gate[tr] = 0;
        seq_release(t);
    }

    gen_out_t out = {0, 60, 100, 0};                       /* a rest keeps note 60, vel 100 (LOCK records it) */
    gen_clock_t clk;
    clk.pos = tessera.evolve[tr].pos_now;
    clk.steps_per_bar = (uint8_t)tess_steps_per_bar(t);
    clk.root = (uint8_t)(t->p[P_ROOT] % 12);

    switch (tessera.mode[tr]) {
    case TESS_ENG_TURING:
        gen_turing_tick(&tessera.turing[tr], &clk, &tessera.rnd[tr], &out);
        break;
    case TESS_ENG_EUCLID:
        gen_euclid_tick(&tessera.euclid[tr], &clk, &tessera.rnd[tr], &out);
        break;
    case TESS_ENG_MARKOV:
        gen_markov_tick(&tessera.markov[tr], &clk, &tessera.rnd[tr], &out);
        break;
    case TESS_ENG_MORPH:
        gen_morph_tick(&tessera.morph[tr], &clk, &tessera.rnd[tr], &out);
        break;
    }
    uint8_t note = out.note, vel = out.vel;
    int should_trigger = out.gate;

    if (tessera.mode[tr] == TESS_ENG_MARKOV || tessera.mode[tr] == TESS_ENG_MORPH)
        tessera_phrase_apply(tr, &note, &vel, &should_trigger);

    {   /* TWEAK: DROP thins any engine's hits; HUMAN varies the velocity (the gate below) */
        const tessera_evolve_t *tw = &tessera.evolve[tr];
        if (should_trigger && tw->drop && gen_rnd(&tessera.rnd[tr], 100u) < tw->drop) should_trigger = 0;
        if (should_trigger && tw->human) {
            int32_t j = (int32_t)(tw->human / 5u);                    /* up to +-20 */
            int32_t v = (int32_t)vel + (int32_t)gen_rnd(&tessera.rnd[tr], (uint32_t)(2 * j + 1)) - j;
            vel = (uint8_t)(v < 1 ? 1 : v > 127 ? 127 : v);
        }
    }
    if (tessera.mode[tr] == TESS_ENG_MARKOV)
        tessera.markov[tr].last_hit = (uint8_t)(should_trigger != 0);   /* after LOCK's replay: what really sounds */

    if (should_trigger) {
        const tessera_evolve_t *tw2 = &tessera.evolve[tr];
        int32_t base_note = (int32_t)note;
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

        trk_note_on(t, (uint32_t)final_note, vel);
        tessera.last_note[tr] = (uint8_t)final_note;
        tessera.active_gate[tr] = 1;
        t->seq_notes[0] = (uint8_t)final_note;
        t->seq_n = 1;

        uint32_t period = seq_div_samples((uint32_t)t->p[P_SDIV]);
        t->seq_off = period * (uint32_t)(t->p[P_SGATE] ? t->p[P_SGATE] : 64) / 128u;
        if (tw2->human)                                    /* shorten by up to HUMAN/2 percent */
            t->seq_off = t->seq_off * (100u - gen_rnd(&tessera.rnd[tr], tw2->human / 2u + 1u)) / 100u;
    }
}

static void tess_knob_body(uint32_t k, int32_t s)
{
    tessera_ensure_init();
    uint32_t tr = song.sel;
    if (tr >= TESSERA_TRACKS || !s) return;

    uint32_t mode = tessera.mode[tr];

    if (k == 0u) {
        int32_t m = (int32_t)mode + s;
        if (m < 0) m = 0;
        if (m >= TESS_ENG_COUNT) m = TESS_ENG_COUNT - 1;
        if ((uint8_t)m != tessera.mode[tr]) {
            seq_release(&trk[tr]);
            tessera.active_gate[tr] = 0;
        }
        tessera.mode[tr] = (uint8_t)m;
        if (m != TESS_ENG_OFF) tess_seed_fresh(tr);             /* (its picture shows the material at once) */
        if (tessera.evolve[tr].form) tess_form_restart(tr);   /* the sections were another engine's */
        tess_rearm(tr);
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
    }
    tess_rearm(tr);
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
    }
    return h;
}

/* Force a note down into the nearest active scale degree of the track */
int32_t tessera_snap_note(const track_t *t, int32_t n)
{
    uint32_t mask = scale_mask(t), guard = 12;
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
    int32_t n = tessera_snap_note(t, 48 + (int32_t)gen_euclid_degree(tessera.euclid[trk_index(t) % TESSERA_TRACKS].pitch_seed, step));
    n -= 45;
    if (n < 0) n = 0;
    if (n > 14) n = 14;
    return (uint32_t)n;
}

/* ------------------------------------------------------------ persistence --- */
/* The project's Tessera block (project.c tessera_store_*): per track the engine, each engine's settings, the
 * EVOLVE / FORM / TWEAK settings and the settings of the remembered sections (A, B, C). Not stored: the recorded
 * Markov / Morph phrases (a loaded project records them again on its first pass: RAM is the limit, see
 * project.c), the playheads and the counters. A block that fails any range check is refused whole. */
#define TESS_PK_TRACK 47u                            /* version 2: version 1's 45 bytes, then SEED (2) */
#define TESS_PK_TRACK_V1 45u
_Static_assert(TESSERA_PK_BYTES == TESSERA_TRACKS * TESS_PK_TRACK, "TESSERA_PK_BYTES (tessera.h)");
_Static_assert(TESSERA_PK_BYTES_V1 == TESSERA_TRACKS * TESS_PK_TRACK_V1, "TESSERA_PK_BYTES_V1 (tessera.h)");

static uint8_t *tess_put16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); return p + 2; }
static uint32_t tess_get16(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8); }

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
    }
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

    if (mode >= TESS_ENG_COUNT || v_seed > TESS_SEED_LOAD_MAX) return 0;
    if (t_len < 2u || t_len > 16u || t_prob > 100u || t_oct > 3u) return 0;
    if (e_steps < 1u || e_steps > 32u || e_pulses > e_steps || e_rot >= e_steps) return 0;
    if (k_tens > 100u || k_dens > 100u || k_oct > 3u) return 0;
    if (m_pos > 127u || m_oct > 7u || m_pa > 7u) return 0;
    if (v_every > 5u || v_amount > 100u || v_lock > 100u || v_form > 6u || v_contrast > 100u) return 0;
    if (v_shift < -24 || v_shift > 24 || v_drop > 100u || v_human > 100u || v_follow > TESSERA_TRACKS) return 0;
    if (v_flour < -12 || v_flour > 12 || v_visited > 7u) return 0;
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
        ev->valid = 0;                                   /* no phrase is stored: every section records its own again */
        ev->rec = 2;
    }
    for (uint32_t s = 0; s < 3u; s++) {
        tessera_sec_t *sc = &tessera.sec[tr][s];
        sc->reg = (uint16_t)tess_get16(in); in += 2; sc->p0 = *in++; sc->p1 = *in++;
    }
    tessera.seed[tr] = (uint16_t)tess_get16(in);
    tessera.rnd[tr] = gen_stream(tessera.seed[tr]);       /* the stream from the top; the material is the block's */
    return 1;
}

/* A version 1 track record (45 bytes, before SEED) -> version 2 (47): its seed from its own bytes, so the same
 * project always gets the same one; an OFF track 0 (a fresh one when a generator is first used). out may not
 * overlap in */
void tessera_v1_track(const uint8_t *in, uint8_t *out)
{
    memcpy(out, in, TESS_PK_TRACK_V1);
    tess_put16(out + TESS_PK_TRACK_V1, in[0] ? tess_seed_of(in, TESS_PK_TRACK_V1) : 0u);
}

/* 1 = loaded; 0 = refused, `tessera` untouched */
int tessera_unpack(const uint8_t *in)
{
    tessera_ensure_init();
    for (uint32_t tr = 0; tr < TESSERA_TRACKS; tr++)
        if (!tess_pk_track(in + tr * TESS_PK_TRACK, tr, 0)) return 0;
    tessera_init();                                      /* the playheads and counters: as at power-on */
    for (uint32_t tr = 0; tr < TESSERA_TRACKS; tr++)
        tess_pk_track(in + tr * TESS_PK_TRACK, tr, 1);
    return 1;
}
/* a version 1 block (TESSERA_PK_BYTES_V1: an older project's, an older backup's): as tessera_unpack, each track
 * given its seed by tessera_v1_track */
int tessera_unpack_v1(const uint8_t *in)
{
    uint8_t b[TESSERA_PK_BYTES];                        /* (main loop: on the stack) */
    for (uint32_t tr = 0; tr < TESSERA_TRACKS; tr++)
        tessera_v1_track(in + tr * TESS_PK_TRACK_V1, b + tr * TESS_PK_TRACK);
    return tessera_unpack(b);
}

/* every track back to OFF and its settings to their defaults (a project load starts from this) */
void tessera_reset(void)
{
    tessera_ensure_init();
    tessera_init();
}

/* ------------------------------------------------------------ the UI's view --- */
const gen_turing_t *tessera_turing(uint32_t tr) { return &tessera.turing[tr % TESSERA_TRACKS]; }
const gen_euclid_t *tessera_euclid(uint32_t tr) { return &tessera.euclid[tr % TESSERA_TRACKS]; }
const gen_markov_t *tessera_markov(uint32_t tr) { return &tessera.markov[tr % TESSERA_TRACKS]; }
const gen_morph_t *tessera_morph(uint32_t tr) { return &tessera.morph[tr % TESSERA_TRACKS]; }
