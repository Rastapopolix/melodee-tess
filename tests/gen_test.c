/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* Unit test of the note generators (firmware/src/gen.c) on their own: no sequencer, no tracks, no Melodee; only
 * gen.h, gen.c and a random stream of its own, as tessera.c hands each track's. That it builds this way is part of the test: the
 * generators know nothing of the firmware (phase 1, docs/analysis/phase1-plan.md step 9).
 *   build/host/gen_test
 * Checks, per generator and across its settings: the same seed gives the same steps; notes and velocities stay
 * inside the generator's range; a rest leaves out's {note 60, vel 100} (LOCK records it); the random draws per
 * step are exactly those tests/tessera_baseline.txt depends on; mutation keeps the settings in range. Also Euclid's
 * hits per cycle (= PULSES) and Turing at PROB 0 repeating every LENGTH steps. Prints the host cost per step. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "gen.h"
#include "gen.c"

static uint32_t rng_state = 1u;                      /* the stream handed to every generator (a track's, in tessera.c) */
static uint32_t draws_since(uint32_t before)         /* draws taken from rng_state since it was `before` (up to 16) */
{
    uint32_t n = 0, x = before;
    while (x != rng_state && n < 17u) { gen_next(&x); n++; }
    return n;
}

static int bad;
static void check(const char *what, int ok)
{
    printf("gen: %-90s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

static const gen_clock_t CLK0 = {0, 16, 0};
static gen_out_t step_out(void) { gen_out_t o = {0, 60, 100, 0}; return o; }   /* as tessera.c starts each step */
/* field by field: gen_out_t has a padding byte (before cv), which memcmp would compare too */
static int out_eq(const gen_out_t *a, const gen_out_t *b, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++)
        if (a[i].gate != b[i].gate || a[i].note != b[i].note || a[i].vel != b[i].vel || a[i].cv != b[i].cv) return 0;
    return 1;
}

/* ------------------------------------------------------------------ Turing --- */
static void turing_tests(void)
{
    int range = 1, draws = 1, same = 1, loop = 1;
    for (uint32_t len = 2; len <= 16; len++)
        for (uint32_t span = 1; span <= 3; span++)
            for (uint32_t prob = 0; prob <= 100; prob += 25) {
                gen_turing_t g = {0xACE1u, (uint8_t)len, (uint8_t)prob, 0, 0, (uint8_t)span};
                for (uint32_t i = 0; i < 64; i++) {
                    gen_out_t o = step_out();
                    uint32_t c = rng_state;
                    gen_turing_tick(&g, &CLK0, &rng_state, &o);
                    draws &= draws_since(c) == (prob ? 1u : 0u);
                    range &= o.note >= 36 && o.note <= 36 + 12 * (span - 1) + 10 && o.vel >= 90 && o.vel <= 121;
                    range &= o.gate <= 1 && o.cv == 0;
                }
            }
    {   /* PROB 0: the register only rotates, so every LENGTH steps it plays the same again */
        for (uint32_t len = 2; len <= 16; len++) {
            gen_turing_t g = {0xB5A3u, (uint8_t)len, 0, 0, 0, 3};
            gen_out_t seen[48];
            for (uint32_t i = 0; i < 48; i++) {
                seen[i] = step_out();
                gen_turing_tick(&g, &CLK0, &rng_state, &seen[i]);
                if (i >= len) loop &= out_eq(&seen[i], &seen[i - len], 1);
            }
        }
    }
    {   /* the same seed: the same steps */
        gen_out_t a[200], b[200];
        for (int run = 0; run < 2; run++) {
            gen_turing_t g = {0xACE1u, 16, 30, 0, 0, 2};
            rng_state = 12345u;
            for (uint32_t i = 0; i < 200; i++) {
                gen_out_t *o = run ? &b[i] : &a[i];
                *o = step_out();
                gen_turing_tick(&g, &CLK0, &rng_state, o);
            }
        }
        same = out_eq(a, b, sizeof a / sizeof a[0]);
    }
    {   /* a fresh register: never 0 (the defaults' "not picked yet"), low two bits never both 0, and spread */
        static uint8_t seen[65536 / 8];
        uint32_t distinct = 0;
        int ok = 1;
        memset(seen, 0, sizeof seen);
        for (uint32_t e = 0; e < 4096; e++) {
            uint16_t r = gen_turing_pick(e * 7919u + 13u);
            ok &= r != 0 && (r & 3u) != 0;
            if (!(seen[r >> 3] & (1u << (r & 7u)))) { seen[r >> 3] |= (uint8_t)(1u << (r & 7u)); distinct++; }
        }
        check("turing: a picked register is never 0, its low two bits never both 0", ok);
        check("turing: 4096 entropy values pick at least 3900 different registers", distinct >= 3900u);
    }
    check("turing: notes 36..36+12*(SPAN-1)+10, velocity 90..121, gate 0/1, cv 0 (every LEN, SPAN, PROB)", range);
    check("turing: one draw a step when PROB > 0, none at PROB 0", draws);
    check("turing: PROB 0 repeats every LENGTH steps", loop);
    check("turing: the same seed gives the same steps", same);
    {
        int ok = 1;
        for (uint32_t amt = 0; amt <= 100; amt += 10) {
            gen_turing_t g = {0xACE1u, 8, 0, 0, 0, 2};
            gen_turing_mutate(&g, amt, &rng_state);
            ok &= g.len == 8 && g.prob == 0 && g.oct_span == 2 && g.step_idx == 0;
            if (!amt) ok &= g.reg == 0xACE1u;
        }
        check("turing: mutation changes only the register (AMOUNT 0: nothing)", ok);
    }
    {   /* from all active bits 0 or all 1, and over a long run: never all alike after a mutation */
        int ok = 1;
        for (uint32_t len = 2; len <= 16; len++) {
            uint16_t m = len >= 16u ? 0xFFFFu : (uint16_t)((1u << len) - 1u);
            for (uint32_t amt = 10; amt <= 100; amt += 10)
                for (uint32_t start = 0; start < 2u; start++) {
                    gen_turing_t g = {start ? m : 0u, (uint8_t)len, 0, 0, 0, 2};
                    for (uint32_t n = 0; n < 200; n++) {
                        gen_turing_mutate(&g, amt, &rng_state);
                        ok &= (g.reg & m) != 0u && (g.reg & m) != m;
                    }
                }
        }
        check("turing: after a mutation the active bits are never all 0 (silence) nor all 1 (LEN 2..16)", ok);
    }
}

/* ------------------------------------------------------------------ Euclid --- */
static void euclid_tests(void)
{
    int range = 1, draws = 1, rest = 1, hits_ok = 1, same = 1, mut = 1;
    for (uint32_t steps = 1; steps <= 32; steps++)
        for (uint32_t pulses = 0; pulses <= steps; pulses++)
            for (uint32_t rot = 0; rot < steps; rot += 3) {
                gen_euclid_t g = {(uint8_t)steps, (uint8_t)pulses, (uint8_t)rot, 0, 0, (uint8_t)(rot * 37u)};
                uint32_t hits = 0;
                for (uint32_t i = 0; i < steps; i++) {
                    gen_out_t o = step_out();
                    uint32_t c = rng_state;
                    gen_euclid_tick(&g, &CLK0, &rng_state, &o);
                    draws &= draws_since(c) == 0u;
                    if (o.gate) {
                        hits++;
                        range &= o.note >= 48 && o.note <= 59 && o.vel == 110;
                    } else {
                        rest &= o.note == 60 && o.vel == 100;
                    }
                }
                hits_ok &= hits == pulses && g.step_idx == 0;
            }
    {
        gen_out_t a[64], b[64];
        for (int run = 0; run < 2; run++) {
            gen_euclid_t g = {13, 5, 3, 0, 0, 77};
            for (uint32_t i = 0; i < 64; i++) {
                gen_out_t *o = run ? &b[i] : &a[i];
                *o = step_out();
                gen_euclid_tick(&g, &CLK0, &rng_state, o);
            }
        }
        same = out_eq(a, b, sizeof a / sizeof a[0]);
    }
    for (uint32_t steps = 1; steps <= 32; steps++)
        for (uint32_t amt = 0; amt <= 100; amt += 20) {
            gen_euclid_t g = {(uint8_t)steps, (uint8_t)(steps / 2u), 0, 0, 0, 5};
            for (uint32_t n = 0; n < 50; n++) gen_euclid_mutate(&g, amt, &rng_state);
            mut &= g.steps == steps && g.pulses <= steps && g.rot < steps && g.pitch_seed != 0;
        }
    check("euclid: a hit plays 48..59 at velocity 110; a rest keeps note 60, velocity 100", range && rest);
    check("euclid: PULSES hits in every cycle of STEPS, the playhead back on step 1 (STEPS 1..32, every PULSES)", hits_ok);
    check("euclid: no random draw on a step (its pitches are a hash of the seed)", draws);
    check("euclid: the same settings give the same steps", same);
    check("euclid: mutation keeps PULSES <= STEPS, ROT < STEPS, a seed other than 0", mut);
    {
        int ok = 1;
        for (uint32_t s = 0; s < 256; s++) {
            uint8_t n = gen_new_seed((uint8_t)s, &rng_state);
            ok &= n != 0 && n != s;
        }
        check("euclid: a new seed is never 0 nor the one it replaces", ok);
    }
}

/* ------------------------------------------------------------------ Markov --- */
static void markov_tests(void)
{
    int range = 1, draws = 1, same = 1, mut = 1;
    for (uint32_t root = 0; root < 12; root++)
        for (uint32_t oct = 1; oct <= 3; oct++)
            for (uint32_t tens = 0; tens <= 100; tens += 20)
                for (uint32_t dens = 0; dens <= 100; dens += 25) {
                    gen_markov_t g = {0, 0, (uint8_t)tens, (uint8_t)dens, (uint8_t)oct};
                    gen_clock_t clk = {0, 16, (uint8_t)root};
                    for (uint32_t i = 0; i < 64; i++) {
                        gen_out_t o = step_out();
                        uint32_t c = rng_state;
                        gen_markov_tick(&g, &clk, &rng_state, &o);
                        draws &= draws_since(c) == 4u;
                        range &= g.cur_state < 8 && o.vel >= 95 && o.vel <= 114 && o.gate <= 1;
                        range &= o.note >= 36 + root && o.note <= 36 + root + 14 + 12 * (oct - 1);
                    }
                }
    {
        gen_out_t a[200], b[200];
        for (int run = 0; run < 2; run++) {
            gen_markov_t g = {0, 0, 40, 70, 2};
            gen_clock_t clk = {0, 16, 5};
            rng_state = 777u;
            for (uint32_t i = 0; i < 200; i++) {
                gen_out_t *o = run ? &b[i] : &a[i];
                *o = step_out();
                gen_markov_tick(&g, &clk, &rng_state, o);
            }
        }
        same = out_eq(a, b, sizeof a / sizeof a[0]);
    }
    {
        uint8_t row[8];
        int rows = 1;
        for (uint32_t tens = 0; tens <= 100; tens++)
            for (uint32_t s = 0; s < 8; s++) {
                gen_markov_row((uint8_t)tens, s, row);
                for (uint32_t d = 1; d < 8; d++) rows &= row[d] >= row[d - 1];
                rows &= row[7] == 100;
            }
        check("markov: every transition row rises to 100 (TENSION 0..100, every state)", rows);
    }
    for (uint32_t amt = 0; amt <= 100; amt += 10)
        for (uint32_t t0 = 0; t0 <= 100; t0 += 50) {
            gen_markov_t g = {3, 0, (uint8_t)t0, 80, 2};
            for (uint32_t n = 0; n < 50; n++) gen_markov_mutate(&g, amt, &rng_state);
            mut &= g.tension <= 100 && g.density == 80 && g.oct_span == 2 && g.cur_state == 3;
        }
    check("markov: notes root+36 .. root+50+12*(SPAN-1), velocity 95..114, a state 0..7 (every ROOT, SPAN, TENSION, DENSITY)", range);
    check("markov: four draws a step, in the order degree, octave, velocity, density", draws);
    check("markov: the same seed gives the same steps", same);
    check("markov: mutation keeps TENSION 0..100 and changes nothing else", mut);
}

/* ------------------------------------------------------------------ Morph --- */
static void morph_tests(void)
{
    int range = 1, draws = 1, rest = 1, same = 1, mut = 1, ends = 1;
    for (uint32_t pos = 0; pos <= 127; pos += 9)
        for (uint32_t pa = 0; pa < 8; pa++) {
            gen_morph_t g = {(uint8_t)pos, GEN_MORPH_PRESETS[pa], GEN_MORPH_PRESETS[7 - pa], 0x0F, 2};
            for (uint32_t i = 0; i < 48; i++) {
                gen_out_t o = step_out();
                uint32_t c = rng_state;
                gen_morph_tick(&g, &CLK0, &rng_state, &o);
                draws &= draws_since(c) == 1u;
                if (o.gate) range &= o.note >= 48 && o.note <= 60 && (o.vel == 100 || o.vel == 115);
                else rest &= o.note == 60 && o.vel == 100;
                range &= (g.step_idx & 0xF0u) == 0;
            }
        }
    {   /* POS 0 plays snapshot A exactly */
        gen_morph_t g = {0, 0x8924u, 0xFFFFu, 0x0F, 2};
        for (uint32_t i = 0; i < 32; i++) {
            gen_out_t o = step_out();
            gen_morph_tick(&g, &CLK0, &rng_state, &o);
            ends &= o.gate == ((0x8924u >> (i & 15u)) & 1u);
        }
    }
    {
        gen_out_t a[100], b[100];
        for (int run = 0; run < 2; run++) {
            gen_morph_t g = {64, 0x8888u, 0xA5A5u, 0x0F, 2};
            rng_state = 4242u;
            for (uint32_t i = 0; i < 100; i++) {
                gen_out_t *o = run ? &b[i] : &a[i];
                *o = step_out();
                gen_morph_tick(&g, &CLK0, &rng_state, o);
            }
        }
        same = out_eq(a, b, sizeof a / sizeof a[0]);
    }
    for (uint32_t amt = 0; amt <= 100; amt += 10) {
        gen_morph_t g = {120, 0x8888u, 0xA5A5u, 0x0F, 2};
        for (uint32_t n = 0; n < 50; n++) gen_morph_mutate(&g, amt, &rng_state);
        mut &= g.morph_pos <= 127 && g.snap_b == 0xA5A5u && g.oct_span == 2;
    }
    check("morph: a hit plays 48..60 at velocity 100 or 115; a rest keeps note 60, velocity 100", range && rest);
    check("morph: one draw a step", draws);
    check("morph: POS 0 plays snapshot A step for step", ends);
    check("morph: the same seed gives the same steps", same);
    check("morph: mutation keeps POS 0..127 and leaves snapshot B", mut);
}

/* ------------------------------------------------------------------ sections --- */
static void section_tests(void)
{
    int ok = 1;
    gen_sec_t s = {0x1234u, 7, 9};
    gen_turing_t tu = {0xBEEFu, 16, 0, 0, 0, 2};
    gen_euclid_t eu = {16, 5, 3, 0, 0, 42};
    gen_markov_t mk = {0, 0, 61, 100, 2};
    gen_morph_t mo = {99, 0x2222u, 0xAAAAu, 0x0F, 2};
    gen_turing_save(&tu, &s); ok &= s.reg == 0xBEEFu && s.p0 == 7 && s.p1 == 9;    /* only its own fields */
    gen_markov_save(&mk, &s); ok &= s.reg == 0xBEEFu && s.p0 == 61 && s.p1 == 9;
    gen_euclid_save(&eu, &s); ok &= s.reg == 42 && s.p0 == 5 && s.p1 == 3;
    gen_morph_save(&mo, &s);  ok &= s.reg == 0x2222u && s.p0 == 99 && s.p1 == 3;
    {
        gen_sec_t e = {42, 6, 2}, m = {0x8888u, 10, 0}, k = {0, 33, 0}, t = {0xACE1u, 0, 0};
        gen_euclid_load(&eu, &e); ok &= eu.pulses == 6 && eu.rot == 2 && eu.pitch_seed == 42 && eu.steps == 16;
        gen_morph_load(&mo, &m);  ok &= mo.snap_a == 0x8888u && mo.morph_pos == 10 && mo.snap_b == 0xAAAAu;
        gen_markov_load(&mk, &k); ok &= mk.tension == 33 && mk.density == 100;
        gen_turing_load(&tu, &t); ok &= tu.reg == 0xACE1u && tu.len == 16;
    }
    check("sections: each generator saves and loads only its own fields", ok);
}

/* ------------------------------------------------------------------ cost --- */
static double ns_per_step(int kind)
{
    gen_turing_t tu = {0xACE1u, 16, 20, 0, 0, 2};
    gen_euclid_t eu = {16, 5, 0, 0, 0, 9};
    gen_markov_t mk = {0, 0, 40, 80, 2};
    gen_morph_t mo = {64, 0x8888u, 0xA5A5u, 0x0F, 2};
    const uint32_t N = 2000000u;
    volatile uint32_t sink = 0;
    struct timespec a, b;
    clock_gettime(CLOCK_MONOTONIC, &a);
    for (uint32_t i = 0; i < N; i++) {
        gen_out_t o = step_out();
        if (kind == 0) gen_turing_tick(&tu, &CLK0, &rng_state, &o);
        else if (kind == 1) gen_euclid_tick(&eu, &CLK0, &rng_state, &o);
        else if (kind == 2) gen_markov_tick(&mk, &CLK0, &rng_state, &o);
        else gen_morph_tick(&mo, &CLK0, &rng_state, &o);
        sink += o.note + o.gate;
    }
    clock_gettime(CLOCK_MONOTONIC, &b);
    (void)sink;
    return ((double)(b.tv_sec - a.tv_sec) * 1e9 + (double)(b.tv_nsec - a.tv_nsec)) / N;
}

static void stream_tests(void)
{
    static uint8_t seen[65536 / 8];
    uint32_t distinct = 0;
    int ok = 1;
    memset(seen, 0, sizeof seen);
    for (uint32_t sd = 0; sd <= 9999u; sd++) {
        uint32_t x = gen_stream(sd), h = (x ^ (x >> 16)) & 0xFFFFu;
        ok &= x != 0;
        if (!(seen[h >> 3] & (1u << (h & 7u)))) { seen[h >> 3] |= (uint8_t)(1u << (h & 7u)); distinct++; }
    }
    check("streams: every seed 0..9999 starts a stream other than 0 (xorshift's dead state)", ok);
    check("streams: the 10,000 seeds start at least 9,000 different streams (16-bit fold)", distinct >= 9000u);
    {   /* a stream left at 0 (xorshift's dead state) draws on, rather than 0 for ever (Euclid's new seed would hang) */
        uint32_t z = 0, a = gen_rnd(&z, 1000u), b = gen_rnd(&z, 1000u), c = gen_rnd(&z, 1000u);
        check("streams: a stream at 0 recovers (no endless draw of 0)", z != 0 && !(a == b && b == c));
    }
}

int main(void)
{
    stream_tests();
    turing_tests();
    euclid_tests();
    markov_tests();
    morph_tests();
    section_tests();
    printf("gen: host cost per step: Turing %.1f ns, Euclid %.1f ns, Markov %.1f ns, Morph %.1f ns\n",
           ns_per_step(0), ns_per_step(1), ns_per_step(2), ns_per_step(3));
    printf(bad ? "gen: %d FAILED\n" : "gen: all passed\n", bad);
    return bad != 0;
}
