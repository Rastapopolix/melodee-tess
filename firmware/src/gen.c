/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* The note generators (gen.h): their tables and pure helpers. Nothing here reads a track or the sequencer. */
static const uint16_t GEN_MORPH_PRESETS[8] = {
    0x8888u, /* 4-on-floor */
    0x2222u, /* Offbeats */
    0xAAAAu, /* Straight 8ths */
    0x8924u, /* Son clave */
    0x8512u, /* Rumba clave */
    0x9249u, /* Syncopated 3-3-2 */
    0xA5A5u, /* Alternating sync */
    0xB6D7u  /* Dense poly */
};

#include "gen_grids.h"

/* Scale degree to semitone intervals (natural minor; the track's scale snaps the notes afterwards) */
static const uint8_t GEN_SCALE_MIN[8] = {0, 2, 3, 5, 7, 8, 10, 12};

/* A track's random stream (xorshift32; never 0): each generator draws only from the stream it is handed, so a
 * track's seed alone decides its random choices. gen_stream: a seed's starting state */
static uint32_t gen_stream(uint32_t seed)
{
    uint32_t x = (seed + 1u) * 2654435761u;
    x ^= x >> 16;
    x *= 2246822519u;
    x ^= x >> 13;
    return x ? x : 0x6D2B79F5u;
}
static uint32_t gen_next(uint32_t *s)
{
    uint32_t x = *s ? *s : gen_stream(0);               /* (0 is xorshift's dead state: it would draw 0 for ever) */
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *s = x;
}
/* 0..n-1 from the stream; n == 0 draws nothing */
static uint32_t gen_rnd(uint32_t *s, uint32_t n) { return n ? gen_next(s) % n : 0u; }

/* 1 = step is a hit of the Euclidean pattern (pulses spread over steps, rotated) */
static int gen_euclid_hit(uint8_t step, uint8_t steps, uint8_t pulses, uint8_t rot)
{
    if (!pulses || !steps) return 0;
    if (pulses >= steps) return 1;
    uint32_t s = (step + rot) % steps;
    return (((s + 1u) * (uint32_t)pulses) / (uint32_t)steps) > ((s * (uint32_t)pulses) / (uint32_t)steps);
}

/* Euclid pitch: semitones 0..11 above C3 for a step. Seed 0 is the original pattern; any other seed
 * gives a different, repeatable one (a hash of seed and step): the track's SEED sets it, EVOLVE may change it. */
static uint32_t gen_euclid_degree(uint8_t seed, uint32_t step)
{
    uint32_t x;
    if (!seed) return (step * 3u) % 12u;
    x = (step + 1u) * 2654435761u ^ ((uint32_t)seed * 40503u);
    x ^= x >> 15;
    x *= 2246822519u;
    x ^= x >> 13;
    return x % 12u;
}

/* Markov: cumulative transition weights (0..100) from state s, derived from tension */
static void gen_markov_row(uint8_t tens, uint32_t s, uint8_t out[8])
{
    uint32_t raw[8], sum = 0, accum = 0;
    for (uint32_t d = 0; d < 8; d++) {
        int32_t dist = (int32_t)d - (int32_t)s;
        if (dist < 0) dist = -dist;
        if      (dist == 0) raw[d] = 15;
        else if (dist == 1) raw[d] = 50 - (tens * 30 / 100);
        else if (dist == 2) raw[d] = 25;
        else                raw[d] = 5 + (tens * 25 / 100);
        if (d == 0 || d == 7) raw[d] += (100 - tens) / 5;
        sum += raw[d];
    }
    if (!sum) sum = 1;
    for (uint32_t d = 0; d < 8; d++) {
        accum += (raw[d] * 100u) / sum;
        out[d] = (accum > 100u || d == 7) ? 100 : (uint8_t)accum;
    }
}

/* Awake's values: A's i-th (0 a rest, 1..8 a degree), B's (0..7); set */
static uint32_t gen_awake_a(const gen_awake_t *g, uint32_t i) { return (g->a[(i >> 1) & 7u] >> ((i & 1u) * 4u)) & 0xFu; }
static uint32_t gen_awake_b(const gen_awake_t *g, uint32_t i) { return (g->b[(i >> 1) & 7u] >> ((i & 1u) * 4u)) & 0xFu; }
static void gen_awake_put(uint8_t *v, uint32_t i, uint32_t x)
{
    uint32_t sh = (i & 1u) * 4u;
    v[(i >> 1) & 7u] = (uint8_t)((v[(i >> 1) & 7u] & ~(0xFu << sh)) | ((x & 0xFu) << sh));
}
/* Awake's 32 values from a seed (ROLL; a track's first): A a melody with rests (about one in four), B mostly 0, now and
 * then a step up the scale, as Awake's own defaults are */
static void gen_awake_fill(gen_awake_t *g, uint32_t seed)
{
    uint32_t s = gen_stream(seed * 13u + 5u), i;
    for (i = 0; i < 16u; i++) {
        gen_awake_put(g->a, i, gen_rnd(&s, 4u) ? 1u + gen_rnd(&s, 8u) : 0u);
        gen_awake_put(g->b, i, gen_rnd(&s, 4u) ? 0u : gen_rnd(&s, 6u));
    }
}
/* Poly's four pitches from a seed (ROLL; a track's first), over SPAN octaves */
static void gen_poly_fill(gen_poly_t *g, uint32_t seed)
{
    uint32_t s = gen_stream(seed * 29u + 11u), i, n = 7u * (g->span == 2u ? 2u : 1u);
    for (i = 0; i < 4u; i++)
        g->pitch[i] = (uint8_t)gen_rnd(&s, n);
}

/* ------------------------------------------------------------------ the generators, one step each --- */
/* Each fills only what it plays: out arrives as {gate 0, note 60, vel 100, cv 0} (tessera.c), which a rest
 * keeps, so LOCK's recording sees exactly what it did before phase 1. The CV (the cords' source CV, Q15 0..32767)
 * is set on every step, a rest's too, from what the step already worked out: no draw of its own */

/* Turing's register picked from an entropy value (the adapter's: a timer at a human moment): never 0, the "not
 * picked yet" of the defaults, and its low two bits never both 0, so even LENGTH 2 has a hit */
static uint16_t gen_turing_pick(uint32_t entropy)
{
    uint32_t x = entropy * 2654435761u;
    x ^= x >> 15;
    x *= 2246822519u;
    x ^= x >> 13;
    x = (x ^ (x >> 16)) & 0xFFFFu;
    if (!(x & 3u)) x |= 1u;
    return (uint16_t)x;
}

/* Turing: the register shifts by one, its last bit fed back (flipped at PROB%); the low byte picks the note */
static void gen_turing_tick(gen_turing_t *g, const gen_clock_t *clk, uint32_t *rnd, gen_out_t *out)
{
    uint8_t len = g->len ? g->len : 16;
    uint16_t bit0 = (g->reg >> (len - 1)) & 1u;
    (void)clk;
    if (g->prob > 0 && gen_rnd(rnd, 100u) < g->prob)
        bit0 ^= 1u;
    g->reg = ((g->reg << 1) | bit0) & 0xFFFFu;

    uint8_t span = g->oct_span ? g->oct_span : 1;
    uint16_t tmask = (len >= 16) ? 0xFFFFu : (uint16_t)((1u << len) - 1u);
    uint16_t mreg = g->reg & tmask;
    uint8_t raw = mreg & 0xFFu;
    uint8_t idx = raw % 7;
    uint8_t oct = (raw / 7) % span;
    out->note = (uint8_t)(36 + (oct * 12) + GEN_SCALE_MIN[idx]);
    out->vel = (uint8_t)(90 + ((mreg >> 8) & 0x1Fu));
    out->cv = (int16_t)(raw * 32767u / 255u);           /* the register as a number: a Turing machine's voltage */
    if (g->reg & 1u)
        out->gate = 1;
}

/* Euclid: the pattern's next step; a hit plays the step's pitch (seed 0: the original pattern) */
static void gen_euclid_tick(gen_euclid_t *g, const gen_clock_t *clk, uint32_t *rnd, gen_out_t *out)
{
    uint8_t steps = g->steps ? g->steps : 16;
    uint8_t curr_step = g->step_idx;
    (void)clk;
    (void)rnd;
    g->step_idx = (curr_step + 1u) % steps;
    out->cv = (int16_t)(steps > 1u ? curr_step * 32767u / (steps - 1u) : 0u);   /* a ramp over the cycle */
    if (gen_euclid_hit(curr_step, steps, g->pulses, g->rot)) {
        out->note = (uint8_t)(48u + gen_euclid_degree(g->pitch_seed, curr_step));
        out->gate = 1;
        out->vel = 110;
    }
}

/* Markov: the next scale degree from the tension's row; the draws in this order: degree, octave, velocity, density */
static void gen_markov_tick(gen_markov_t *g, const gen_clock_t *clk, uint32_t *rnd, gen_out_t *out)
{
    uint8_t row[8];
    gen_markov_row(g->tension, g->cur_state, row);
    uint32_t r = gen_rnd(rnd, 100u);
    uint8_t next_state = 0;
    for (uint8_t i = 0; i < 8; i++) {
        if (r < row[i]) {
            next_state = i;
            break;
        }
    }
    g->cur_state = next_state;
    out->cv = (int16_t)(next_state * 32767u / 7u);      /* the degree it moved to */

    uint8_t m_oct = g->oct_span ? g->oct_span : 1;
    out->note = (uint8_t)(36 + clk->root + (next_state * 2) + (12 * gen_rnd(rnd, m_oct)));
    out->vel = (uint8_t)(95 + gen_rnd(rnd, 20u));
    uint32_t dens = g->density ? g->density : 100;
    if (gen_rnd(rnd, 100u) < dens)
        out->gate = 1;
}

/* Morph: a step of A or B (B at MORPH_POS/128), each with its own degree pattern */
static void gen_morph_tick(gen_morph_t *g, const gen_clock_t *clk, uint32_t *rnd, gen_out_t *out)
{
    uint8_t cur_s = (g->step_idx + 1u) & 0x0Fu;
    (void)clk;
    g->step_idx = (g->step_idx & 0xF0u) | cur_s;
    out->cv = (int16_t)(cur_s * 32767u / 15u);          /* a ramp over its 16 steps */

    uint8_t hit_a = (g->snap_a >> cur_s) & 1u;
    uint8_t hit_b = (g->snap_b >> cur_s) & 1u;

    uint32_t roll = gen_rnd(rnd, 128u);
    int hit = (roll < g->morph_pos) ? hit_b : hit_a;

    if (hit) {
        uint8_t deg_a = (cur_s * 2u) % 8u;
        uint8_t deg_b = (cur_s * 3u + 2u) % 8u;
        uint8_t chosen_deg = (roll < g->morph_pos) ? deg_b : deg_a;
        out->note = (uint8_t)(48 + GEN_SCALE_MIN[chosen_deg]);
        out->vel = (uint8_t)(90 + ((cur_s % 4u == 0) ? 25 : 10));
        out->gate = 1;
    }
}

/* Grids: the chance (0..255) of part `part`'s hit on step `step` (0..31, 32nds) at x, y: the four patterns around the
 * point blended by where it sits between them (Grids' ReadDrumMap and U8Mix) */
static uint32_t gen_grids_mix(uint32_t a, uint32_t b, uint32_t bal) { return (a * (255u - bal) + b * bal) >> 8; }
static uint32_t gen_grids_level(uint32_t step, uint32_t part, uint32_t x, uint32_t y)
{
    uint32_t i = x >> 6, j = y >> 6, fx = (x << 2) & 0xFFu, fy = (y << 2) & 0xFFu, o = part * 32u + (step & 31u);
    uint32_t a = GEN_GRIDS_NODE[i * 5u + j][o], b = GEN_GRIDS_NODE[(i + 1u) * 5u + j][o];
    uint32_t c = GEN_GRIDS_NODE[i * 5u + j + 1u][o], d = GEN_GRIDS_NODE[(i + 1u) * 5u + j + 1u][o];
    return gen_grids_mix(gen_grids_mix(a, b, fx), gen_grids_mix(c, d, fx), fy);
}
/* Grids: steps in its bar: 32 (Grids' own 32nds) when the track's division makes 32 a bar, else 16 (its 16ths) */
static uint32_t gen_grids_steps(uint32_t steps_per_bar) { return steps_per_bar >= 32u ? 32u : 16u; }

/* 1 = part `part` hits on step `step` of its bar (steps of them): the map's chance plus the bar's push over the
 * threshold DENSITY sets (Grids' EvaluateDrums); *acc: 1 when it is an accent */
static int gen_grids_hit(const gen_grids_t *g, uint32_t step, uint32_t steps, uint32_t part, uint32_t push, int *acc)
{
    uint32_t lv = gen_grids_level(step * (32u / steps), part, g->x, g->y);
    uint32_t thr = 255u - (uint32_t)(g->dens > 100u ? 100u : g->dens) * 255u / 100u;
    lv = lv < 255u - push ? lv + push : 255u;
    if (acc) *acc = lv > 192u;
    return lv > thr;
}

/* Grids: kick, snare and hat of this step, from the map at x, y. CHAOS draws each part's push at the bar's start (none
 * drawn at 0: the pattern alone); the note is the first part's (GM 36 kick, 38 snare, 42 closed hat), the adapter
 * plays every part (tessera.c) */
static const uint8_t GEN_GRIDS_NOTE[3] = {36, 38, 42};
static void gen_grids_tick(gen_grids_t *g, const gen_clock_t *clk, uint32_t *rnd, gen_out_t *out)
{
    uint32_t steps = gen_grids_steps(clk->steps_per_bar), cur = g->step_idx % steps, p;
    g->step_idx = (uint8_t)((cur + 1u) % steps);
    if (!cur)
        for (p = 0; p < 3u; p++)
            g->perturb[p] = (uint8_t)(g->chaos ? (gen_rnd(rnd, 256u) * ((uint32_t)g->chaos * 255u / 100u >> 2)) >> 8 : 0u);
    out->cv = (int16_t)(gen_grids_level(cur * (32u / steps), 0u, g->x, g->y) * 32767u / 255u);   /* the kick's chance */
    for (p = 0; p < 3u; p++) {
        int acc;
        if (!gen_grids_hit(g, cur, steps, p, g->perturb[p], &acc)) continue;
        if (!out->parts) {
            out->note = GEN_GRIDS_NOTE[p];
            out->vel = acc ? 127 : 96;
        }
        out->parts |= (uint8_t)(1u << p);
        if (acc) out->accent |= (uint8_t)(1u << p);
    }
    out->gate = out->parts != 0;
}

/* ------------------------------------------------------------------ mutation (EVOLVE, FORM) --- */
/* amt 0..100: how much changes. The draws keep the order they had in tessera.c */

/* a new Euclid pitch seed, never 0 (the original pattern) nor the one it replaces */
static uint8_t gen_new_seed(uint8_t old, uint32_t *rnd)
{
    uint8_t n;
    do n = (uint8_t)(1u + gen_rnd(rnd, 255u)); while (n == old);
    return n;
}

/* Turing: AMOUNT% of the register's active bits flip, at least one. Never all its active bits alike: all 0 is
 * silence, all 1 one note on every step, and locked (PROB 0) it would stay so; one more flips then */
static void gen_turing_mutate(gen_turing_t *g, uint32_t amt, uint32_t *rnd)
{
    uint32_t len = g->len ? g->len : 16u;
    uint32_t n = (amt * len + 99u) / 100u;
    uint16_t m = len >= 16u ? 0xFFFFu : (uint16_t)((1u << len) - 1u);
    while (n--) g->reg ^= (uint16_t)(1u << gen_rnd(rnd, len));
    if (amt && ((g->reg & m) == 0u || (g->reg & m) == m))
        g->reg ^= (uint16_t)(1u << gen_rnd(rnd, len));
}

/* Euclid: the rotation nudged by one, less often a pulse more or fewer (kept 1..STEPS-1: never silent nor every
 * step; at a bound it turns back), and the pitches re-rolled */
static void gen_euclid_mutate(gen_euclid_t *g, uint32_t amt, uint32_t *rnd)
{
    uint32_t steps = g->steps ? g->steps : 16u;
    if (gen_rnd(rnd, 100u) < amt) {
        if (gen_rnd(rnd, 2u)) g->rot = (uint8_t)((g->rot + 1u) % steps);
        else g->rot = (uint8_t)(g->rot ? g->rot - 1u : steps - 1u);
    }
    if (gen_rnd(rnd, 100u) < amt / 2u) {
        uint32_t hi = steps > 1u ? steps - 1u : 1u, up = gen_rnd(rnd, 2u);
        if (g->pulses <= 1u) up = 1;
        if (g->pulses >= hi) up = 0;
        g->pulses = (uint8_t)(g->pulses + (up ? 1 : -1));
        if (g->pulses < 1u) g->pulses = 1;
        if (g->pulses > hi) g->pulses = (uint8_t)hi;
    }
    if (gen_rnd(rnd, 100u) < amt) g->pitch_seed = gen_new_seed(g->pitch_seed, rnd);
}

/* Markov: the tension (LEAP) moves by up to +-AMOUNT/2, turning back at 0 and 100 (clamped, it sat there for long
 * stretches) */
static void gen_markov_mutate(gen_markov_t *g, uint32_t amt, uint32_t *rnd)
{
    int32_t j = (int32_t)gen_rnd(rnd, amt + 1u) - (int32_t)(amt / 2u);
    int32_t v = (int32_t)g->tension + j;
    if (v < 0) v = -v;
    if (v > 100) v = 200 - v;
    g->tension = (uint8_t)(v < 0 ? 0 : v > 100 ? 100 : v);
}

/* Morph: 1..3 bits of snapshot A flip, and the position moves by up to +-AMOUNT/2 */
static void gen_morph_mutate(gen_morph_t *g, uint32_t amt, uint32_t *rnd)
{
    uint32_t n = 1u + amt / 34u;
    while (n--) g->snap_a ^= (uint16_t)(1u << gen_rnd(rnd, 16u));
    int32_t j = (int32_t)gen_rnd(rnd, amt + 1u) - (int32_t)(amt / 2u);
    int32_t v = (int32_t)g->morph_pos + j;
    g->morph_pos = (uint8_t)(v < 0 ? 0 : v > 127 ? 127 : v);
}

/* Grids: the point wanders: X, then Y, by up to +-AMOUNT/100 of a quarter of the map, turning back at the edges */
static uint8_t gen_grids_walk(uint32_t v, uint32_t amt, uint32_t *rnd)
{
    uint32_t r = amt * 64u / 100u;
    int32_t n = (int32_t)v + (int32_t)gen_rnd(rnd, 2u * r + 1u) - (int32_t)r;
    if (n < 0) n = -n;
    if (n > 255) n = 510 - n;
    return (uint8_t)(n < 0 ? 0 : n > 255 ? 255 : n);
}
static void gen_grids_mutate(gen_grids_t *g, uint32_t amt, uint32_t *rnd)
{
    g->x = gen_grids_walk(g->x, amt, rnd);
    g->y = gen_grids_walk(g->y, amt, rnd);
}

/* ------------------------------------------------------------------ FORM sections --- */
static void gen_turing_save(const gen_turing_t *g, gen_sec_t *s) { s->reg = g->reg; }
static void gen_turing_load(gen_turing_t *g, const gen_sec_t *s) { g->reg = s->reg; }
static void gen_euclid_save(const gen_euclid_t *g, gen_sec_t *s) { s->p0 = g->pulses; s->p1 = g->rot; s->reg = g->pitch_seed; }
static void gen_euclid_load(gen_euclid_t *g, const gen_sec_t *s)
{
    g->pulses = s->p0;
    g->rot = s->p1;
    g->pitch_seed = (uint8_t)s->reg;
}
static void gen_markov_save(const gen_markov_t *g, gen_sec_t *s) { s->p0 = g->tension; }
static void gen_markov_load(gen_markov_t *g, const gen_sec_t *s) { g->tension = s->p0; }
static void gen_morph_save(const gen_morph_t *g, gen_sec_t *s) { s->reg = g->snap_a; s->p0 = g->morph_pos; }
static void gen_morph_load(gen_morph_t *g, const gen_sec_t *s) { g->snap_a = s->reg; g->morph_pos = s->p0; }
static void gen_grids_save(const gen_grids_t *g, gen_sec_t *s) { s->reg = (uint16_t)(g->x | g->y << 8); }
static void gen_grids_load(gen_grids_t *g, const gen_sec_t *s) { g->x = (uint8_t)s->reg; g->y = (uint8_t)(s->reg >> 8); }
