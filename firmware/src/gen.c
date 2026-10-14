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

/* Scale degree to semitone intervals (natural minor; the track's scale snaps the notes afterwards) */
static const uint8_t GEN_SCALE_MIN[8] = {0, 2, 3, 5, 7, 8, 10, 12};

/* 0..n-1 from the shared generator; n == 0 draws nothing (phase 1: the draws stay exactly as before) */
static uint32_t gen_rnd(uint32_t n) { return n ? rng() % n : 0u; }

/* 1 = step is a hit of the Euclidean pattern (pulses spread over steps, rotated) */
static int gen_euclid_hit(uint8_t step, uint8_t steps, uint8_t pulses, uint8_t rot)
{
    if (!pulses || !steps) return 0;
    if (pulses >= steps) return 1;
    uint32_t s = (step + rot) % steps;
    return (((s + 1u) * (uint32_t)pulses) / (uint32_t)steps) > ((s * (uint32_t)pulses) / (uint32_t)steps);
}

/* Euclid pitch: semitones 0..11 above C3 for a step. Seed 0 is the original pattern; any other seed
 * gives a different, repeatable one (a hash of seed and step), which ROLL changes. */
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

/* ------------------------------------------------------------------ the generators, one step each --- */
/* Each fills only what it plays: out arrives as {gate 0, note 60, vel 100, cv 0} (tessera.c), which a rest
 * keeps, so LOCK's recording sees exactly what it did before phase 1. */

/* Turing: the register shifts by one, its last bit fed back (flipped at PROB%); the low byte picks the note */
static void gen_turing_tick(gen_turing_t *g, const gen_clock_t *clk, gen_out_t *out)
{
    uint8_t len = g->len ? g->len : 16;
    uint16_t bit0 = (g->reg >> (len - 1)) & 1u;
    (void)clk;
    if (g->prob > 0 && gen_rnd(100u) < g->prob)
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
    if (g->reg & 1u)
        out->gate = 1;
}

/* Euclid: the pattern's next step; a hit plays the step's pitch (seed 0: the original pattern) */
static void gen_euclid_tick(gen_euclid_t *g, const gen_clock_t *clk, gen_out_t *out)
{
    uint8_t steps = g->steps ? g->steps : 16;
    uint8_t curr_step = g->step_idx;
    (void)clk;
    g->step_idx = (curr_step + 1u) % steps;
    if (gen_euclid_hit(curr_step, steps, g->pulses, g->rot)) {
        out->note = (uint8_t)(48u + gen_euclid_degree(g->pitch_seed, curr_step));
        out->gate = 1;
        out->vel = 110;
    }
}
