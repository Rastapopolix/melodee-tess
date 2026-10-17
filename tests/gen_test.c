/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* Unit test of the note generators (firmware/src/gen.c) on their own: no sequencer, no tracks, no Melodee; only
 * gen.h, gen.c and a random stream of its own, as tessera.c hands each track's. That it builds this way is part of the test: the
 * generators know nothing of the firmware (phase 1, tessera-notes/docs/analysis/phase1-plan.md step 9).
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
static gen_out_t step_out(void) { gen_out_t o = {0, 60, 100, 0, 0, 0, {0, 0}, 0}; return o; }   /* as tessera.c starts each step */
/* field by field: gen_out_t has a padding byte (before cv), which memcmp would compare too */
static int out_eq(const gen_out_t *a, const gen_out_t *b, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++)
        if (a[i].gate != b[i].gate || a[i].note != b[i].note || a[i].vel != b[i].vel || a[i].cv != b[i].cv ||
            a[i].parts != b[i].parts || a[i].accent != b[i].accent || a[i].more_n != b[i].more_n ||
            (a[i].more_n && memcmp(a[i].more, b[i].more, a[i].more_n))) return 0;
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
                    range &= o.gate <= 1 && o.cv >= 0;   /* (the CV: Q15, 0..32767) */
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
    check("turing: notes 36..36+12*(SPAN-1)+10, velocity 90..121, gate 0/1, cv 0..32767 (every LEN, SPAN, PROB)", range);
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
    {   /* PULSES 1..STEPS-1 once a mutation moved them, from 0 and from STEPS too (STEPS 1: 1) */
        int ok = 1;
        for (uint32_t steps = 1; steps <= 32; steps++)
            for (uint32_t start = 0; start < 3u; start++) {
                uint32_t hi = steps > 1u ? steps - 1u : 1u, moved = 0;
                gen_euclid_t g = {(uint8_t)steps, (uint8_t)(start == 0 ? 0u : start == 1 ? steps : steps / 2u), 0, 0, 0, 5};
                for (uint32_t n = 0; n < 300; n++) {
                    uint8_t was = g.pulses;
                    gen_euclid_mutate(&g, 100, &rng_state);
                    moved |= g.pulses != was;
                    if (moved) ok &= g.pulses >= 1u && g.pulses <= hi;
                }
                ok &= moved || (g.pulses >= 1u && g.pulses <= hi);
            }
        check("euclid: mutation keeps PULSES 1..STEPS-1 (never silent, never every step), from 0 and from STEPS", ok);
    }
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
    {   /* turning back at 0 and 100: at AMOUNT 100, 4000 mutations rest on a bound only about 2 % of the time */
        gen_markov_t g = {3, 0, 100, 80, 2};
        uint32_t at = 0;
        for (uint32_t n = 0; n < 4000; n++) {
            gen_markov_mutate(&g, 100, &rng_state);
            at += g.tension == 0 || g.tension == 100;
        }
        check("markov: mutation turns TENSION back at 0 and 100, never sitting there (AMOUNT 100: under 5 % of 4000)",
              at < 200u);
    }
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

/* ------------------------------------------------------------------ Grids --- */
static void grids_tests(void)
{
    /* the map read against Grids itself: values from its resources.cc and ReadDrumMap (U8Mix), off the nodes too */
    static const uint8_t REF[][5] = {{0, 0, 0, 0, 143}, {4, 1, 255, 255, 0}, {8, 2, 48, 228, 187}, {12, 0, 100, 30, 130},
                                     {30, 1, 200, 77, 114}, {16, 2, 128, 128, 74}, {2, 0, 63, 191, 0}};
    int ref = 1, range = 1, rest = 1, draws = 1, silent = 1, full = 1, len = 1, same = 1, mut = 1;
    for (uint32_t i = 0; i < sizeof REF / sizeof REF[0]; i++)
        ref &= gen_grids_level(REF[i][0], REF[i][1], REF[i][2], REF[i][3]) == REF[i][4];
    for (uint32_t x = 0; x <= 255; x += 51)
        for (uint32_t y = 0; y <= 255; y += 85)
            for (uint32_t dens = 0; dens <= 100; dens += 25)
                for (uint32_t chaos = 0; chaos <= 100; chaos += 50) {
                    gen_grids_t g = {(uint8_t)x, (uint8_t)y, (uint8_t)dens, (uint8_t)chaos, 0, {0, 0, 0}};
                    for (uint32_t i = 0; i < 48; i++) {
                        gen_out_t o = step_out();
                        uint32_t c = rng_state, st = g.step_idx;
                        gen_grids_tick(&g, &CLK0, &rng_state, &o);
                        draws &= draws_since(c) == (st == 0u && chaos ? 3u : 0u);
                        range &= o.parts <= 7u && (o.accent & ~o.parts) == 0u && o.gate == (o.parts != 0u) && o.cv >= 0;
                        range &= g.perturb[0] <= 63u && g.perturb[1] <= 63u && g.perturb[2] <= 63u;
                        if (o.gate) range &= (o.note == 36 || o.note == 38 || o.note == 42) && (o.vel == 96 || o.vel == 127);
                        else rest &= o.note == 60 && o.vel == 100;
                        if (!dens) silent &= !o.gate;
                        if (dens == 100u && !chaos)          /* every step the map gives a chance plays */
                            for (uint32_t p = 0; p < 3u; p++)
                                full &= ((o.parts >> p) & 1u) == (gen_grids_level(st * 2u, p, x, y) > 0u);
                    }
                }
    {   /* 16 steps a bar at 16ths (the map's even 32nds), 32 at 32nds */
        gen_grids_t g = {128, 128, 50, 0, 0, {0, 0, 0}};
        const gen_clock_t c32 = {0, 32, 0};
        for (uint32_t i = 0; i < 40; i++) { gen_out_t o = step_out(); gen_grids_tick(&g, &CLK0, &rng_state, &o); }
        len &= g.step_idx == 40u % 16u;
        g.step_idx = 0;
        for (uint32_t i = 0; i < 40; i++) { gen_out_t o = step_out(); gen_grids_tick(&g, &c32, &rng_state, &o); }
        len &= g.step_idx == 40u % 32u;
    }
    {
        gen_out_t a[64], b[64];
        for (int run = 0; run < 2; run++) {
            gen_grids_t g = {77, 190, 70, 60, 0, {0, 0, 0}};
            rng_state = 0xC0FFEEu;
            for (uint32_t i = 0; i < 64; i++) {
                gen_out_t *o = run ? &b[i] : &a[i];
                *o = step_out();
                gen_grids_tick(&g, &CLK0, &rng_state, o);
            }
        }
        same = out_eq(a, b, 64);
    }
    for (uint32_t amt = 0; amt <= 100; amt += 10) {
        gen_grids_t g = {3, 250, 40, 20, 5, {0, 0, 0}};
        for (uint32_t n = 0; n < 200; n++) gen_grids_mutate(&g, amt, &rng_state);
        mut &= g.dens == 40 && g.chaos == 20 && g.step_idx == 5 && (amt || (g.x == 3 && g.y == 250));
    }
    check("grids: the map reads as Grids' ReadDrumMap, on and between its patterns", ref);
    check("grids: hits play 36/38/42 at velocity 96 or 127 (accent); a rest keeps note 60, vel 100", range && rest);
    check("grids: CHAOS draws three a bar (on its first step), none at 0", draws);
    check("grids: DENSITY 0 is silent; 100 plays every step the map gives a chance", silent && full);
    check("grids: a bar of 16 steps at 16ths, 32 at 32nds", len);
    check("grids: the same seed gives the same steps", same);
    check("grids: mutation moves only X and Y, inside the map (AMOUNT 0: not at all)", mut);
}

/* ------------------------------------------------------------------ Bounce --- */
static gen_bounce_t bounce_at(uint32_t speed, uint32_t angle, uint32_t balls, uint32_t seed)
{
    gen_bounce_t g;
    memset(&g, 0, sizeof g);
    g.speed = (uint8_t)speed, g.angle = (uint8_t)angle, g.balls = (uint8_t)balls;
    gen_bounce_serve(&g, seed);
    return g;
}
static void bounce_tests(void)
{
    int range = 1, draws = 1, walls = 1, loop = 1, mut = 1, same = 1, order = 1, apart = 1, one = 1;
    uint32_t chords = 0;
    for (uint32_t sp = 1; sp <= 64; sp += 9)
        for (uint32_t an = 0; an <= 90; an += 15)
            for (uint32_t nb = 1; nb <= 4; nb++) {
                gen_bounce_t g = bounce_at(sp, an, nb, sp * 7u + an);
                for (uint32_t i = 0; i < 400; i++) {
                    gen_out_t o = step_out();
                    uint32_t c = rng_state, k;
                    gen_bounce_tick(&g, &CLK0, &rng_state, &o);
                    draws &= draws_since(c) == 0u;
                    range &= o.gate ? o.note >= 48 && o.note <= 70 && (o.vel == 100 || o.vel == 115) :
                                      o.note == 60 && o.vel == 100 && !o.more_n;
                    range &= o.cv >= 0 && o.more_n <= 2u && o.gate == (g.hit != 0) && (nb > 1u || !o.more_n);
                    for (k = 0; k < o.more_n; k++) range &= o.more[k] >= 48 && o.more[k] <= 70 && o.more[k] != o.note;
                    chords += o.more_n != 0;
                }
            }
    {   /* straight up (90 degrees), 64 a step: floor and top in turn, every 4 steps a hit (0 128 255 127 0 ...) */
        gen_bounce_t g = bounce_at(64, 90, 1, 0);
        uint32_t hits = 0, top = 0;
        for (uint32_t i = 0; i < 16; i++) {
            gen_out_t o = step_out();
            gen_bounce_tick(&g, &CLK0, &rng_state, &o);
            hits += o.gate;
            top += o.gate && o.vel == 115;
            walls &= g.x[0] == 128u;
        }
        walls &= hits == 4u && top == 2u;
    }
    {   /* at 45 degrees and 43 a step (30 each way: 510 / 30 = 17) ball 1's path comes back to its start every 17 steps */
        gen_bounce_t g = bounce_at(43, 45, 1, 5);
        gen_out_t a[17], b[17];
        for (uint32_t i = 0; i < 17; i++) { a[i] = step_out(); gen_bounce_tick(&g, &CLK0, &rng_state, &a[i]); }
        for (uint32_t i = 0; i < 17; i++) { b[i] = step_out(); gen_bounce_tick(&g, &CLK0, &rng_state, &b[i]); }
        loop &= out_eq(a, b, 17) && g.x[0] == 128u && g.y[0] == 0u;
    }
    {   /* the same settings and seed give the same steps */
        gen_bounce_t g1 = bounce_at(37, 61, 3, 9), g2 = bounce_at(37, 61, 3, 9);
        gen_out_t a[100], b[100];
        for (uint32_t i = 0; i < 100; i++) { a[i] = step_out(); gen_bounce_tick(&g1, &CLK0, &rng_state, &a[i]); }
        for (uint32_t i = 0; i < 100; i++) { b[i] = step_out(); gen_bounce_tick(&g2, &CLK0, &rng_state, &b[i]); }
        same &= out_eq(a, b, 100);
    }
    for (uint32_t sd = 0; sd < 200; sd++) {             /* the balls start apart, at angles of their own */
        gen_bounce_t g = bounce_at(20, 40, 4, sd);
        for (uint32_t k = 1; k < 4u; k++) {
            int32_t o = g.off[k] < 0 ? -g.off[k] : g.off[k];
            apart &= o >= 8 && o <= 30 && (g.x[k] != g.x[0] || g.y[k] != g.y[0]);
        }
        apart &= !g.off[0] && g.x[0] == 128u && !g.y[0];
    }
    {   /* ball 1 alone plays as it would with the others there: they add notes, they take none of its */
        gen_bounce_t g1 = bounce_at(29, 52, 1, 3), g4 = bounce_at(29, 52, 4, 3);
        for (uint32_t i = 0; i < 300; i++) {
            gen_out_t a = step_out(), b = step_out();
            gen_bounce_tick(&g1, &CLK0, &rng_state, &a);
            gen_bounce_tick(&g4, &CLK0, &rng_state, &b);
            one &= g1.x[0] == g4.x[0] && g1.y[0] == g4.y[0];
            if (a.gate) {
                uint32_t k, in = b.note == a.note;
                for (k = 0; k < b.more_n; k++) in |= b.more[k] == a.note;
                one &= b.gate && (in || b.more_n == 2u);
            }
        }
    }
    {   /* hits on one step: the lowest wall's note first (a ball on the floor and one on the top, the same step) */
        gen_bounce_t g = bounce_at(64, 90, 2, 0);
        gen_out_t o = step_out();
        g.off[1] = 0, g.x[1] = 40, g.y[1] = 250, g.dir &= (uint8_t)~0xCu;   /* ball 2 going up, ball 1 coming down */
        g.y[0] = 6, g.dir |= 2u;
        gen_bounce_tick(&g, &CLK0, &rng_state, &o);
        order &= o.gate && o.more_n == 1u && g.hit == 9u && o.note < o.more[0] && o.vel == 100;
    }
    for (uint32_t amt = 0; amt <= 100; amt += 25) {
        gen_bounce_t g = bounce_at(20, 3, 2, 1);
        for (uint32_t n = 0; n < 200; n++) {
            gen_bounce_mutate(&g, amt, &rng_state);
            mut &= g.angle <= 90u && g.speed == 20u && g.balls == 2u;
        }
    }
    check("bounce: a wall hit plays 48..70 at 100 (the top 115), up to 3 notes; else a rest keeps note 60, vel 100", range);
    check("bounce: several balls hit on one step now and then (chords)", chords > 0u);
    check("bounce: no random draw a step", draws);
    check("bounce: straight up at 64 a step hits the floor and the top in turn, every 4 steps", walls);
    check("bounce: at 45 degrees the path loops (17 steps); the same settings give the same steps", loop && same);
    check("bounce: the other balls start apart, 8..30 degrees off ANGLE; ball 1 plays as alone", apart && one);
    check("bounce: hits on one step sound the lowest wall's note first", order);
    check("bounce: mutation moves only the angle, inside 0..90", mut);
}

/* ------------------------------------------------------------------ Awake --- */
static gen_awake_t awake_at(uint32_t la, uint32_t lb, uint32_t prob, uint32_t seed)
{
    gen_awake_t g;
    memset(&g, 0, sizeof g);
    g.len_a = (uint8_t)la, g.len_b = (uint8_t)lb, g.prob = (uint8_t)prob;
    gen_awake_fill(&g, seed);
    return g;
}
static void awake_tests(void)
{
    int range = 1, draws = 1, loop = 1, silent = 1, fill = 1, mut = 1, same = 1;
    for (uint32_t sd = 1; sd <= 128; sd += 7)
        for (uint32_t la = 1; la <= 16; la += 5)
            for (uint32_t prob = 0; prob <= 100; prob += 50) {
                gen_awake_t g = awake_at(la, 17u - la, prob, sd);
                for (uint32_t i = 0; i < 64; i++) {
                    gen_out_t o = step_out();
                    uint32_t c = rng_state, a = gen_awake_a(&g, g.pos_a);
                    gen_awake_tick(&g, &CLK0, &rng_state, &o);
                    draws &= draws_since(c) == (a && prob < 100u ? 1u : 0u);
                    range &= o.gate ? o.note >= 48 && o.note <= 72 && o.vel == 100 : o.note == 60 && o.vel == 100;
                    silent &= prob || !o.gate;
                    range &= g.pos_a < la && g.pos_b < 17u - la && o.cv >= 0;
                }
            }
    for (uint32_t sd = 0; sd < 200; sd++) {                 /* the values in their ranges, A with rests and notes */
        gen_awake_t g = awake_at(16, 16, 100, sd);
        uint32_t rests = 0;
        for (uint32_t i = 0; i < 16; i++) {
            fill &= gen_awake_a(&g, i) <= 8u && gen_awake_b(&g, i) <= 7u;
            rests += !gen_awake_a(&g, i);
        }
        fill &= rests < 16u;
    }
    {   /* lengths 4 and 3 at PROB 100: the steps repeat every 12 */
        gen_awake_t g = awake_at(4, 3, 100, 5);
        gen_out_t a[12], b[12];
        for (uint32_t i = 0; i < 12; i++) { a[i] = step_out(); gen_awake_tick(&g, &CLK0, &rng_state, &a[i]); }
        for (uint32_t i = 0; i < 12; i++) { b[i] = step_out(); gen_awake_tick(&g, &CLK0, &rng_state, &b[i]); }
        loop &= out_eq(a, b, 12);
    }
    {
        gen_awake_t g1 = awake_at(9, 5, 60, 77), g2 = awake_at(9, 5, 60, 77);
        uint32_t r1 = 9, r2 = 9;
        gen_out_t a[60], b[60];
        for (uint32_t i = 0; i < 60; i++) { a[i] = step_out(); gen_awake_tick(&g1, &CLK0, &r1, &a[i]); }
        for (uint32_t i = 0; i < 60; i++) { b[i] = step_out(); gen_awake_tick(&g2, &CLK0, &r2, &b[i]); }
        same &= out_eq(a, b, 60);
    }
    for (uint32_t amt = 0; amt <= 100; amt += 25) {
        gen_awake_t g = awake_at(6, 11, 80, 3), g0 = g;
        for (uint32_t n = 0; n < 200; n++) {
            gen_awake_mutate(&g, amt, &rng_state);
            for (uint32_t i = 0; i < 16; i++) mut &= gen_awake_a(&g, i) <= 8u && gen_awake_b(&g, i) <= 7u;
            for (uint32_t i = 6; i < 16; i++) mut &= gen_awake_a(&g, i) == gen_awake_a(&g0, i);   /* (past LEN A: kept) */
            mut &= g.len_a == 6u && g.len_b == 11u && g.prob == 80u;
        }
        if (!amt) mut &= !memcmp(g.a, g0.a, 8) && !memcmp(g.b, g0.b, 8);
    }
    check("awake: notes 48..72 at 100 (A's degree plus B's); a rest keeps note 60, vel 100; PROB 0 silent", range && silent);
    check("awake: a draw a step only for one of A's notes below PROB 100", draws);
    check("awake: the seed's values in range (A 0..8 with notes, B 0..7)", fill);
    check("awake: lengths 4 and 3 repeat every 12 steps; the same seed and stream, the same steps", loop && same);
    check("awake: mutation changes values inside A's and B's loops only (AMOUNT 0: none)", mut);
}

/* ------------------------------------------------------------------ Poly --- */
static gen_poly_t poly_at(uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3, uint32_t span, uint32_t seed)
{
    gen_poly_t g;
    memset(&g, 0, sizeof g);
    g.div[0] = (uint8_t)d0, g.div[1] = (uint8_t)d1, g.div[2] = (uint8_t)d2, g.div[3] = (uint8_t)d3, g.span = (uint8_t)span;
    g.pos = 3;
    gen_poly_fill(&g, seed);
    return g;
}
static void poly_tests(void)
{
    int range = 1, draws = 1, hits = 1, fill = 1, mut = 1, loud = 1, off = 1, wrap = 1;
    for (uint32_t d = 0; d <= 16; d++)
        for (uint32_t span = 1; span <= 3; span++) {
            gen_poly_t g = poly_at(d, 16u - d, d / 2u, 7, span, d * 7u + 1u);
            for (uint32_t i = 0; i < 200; i++) {
                gen_out_t o = step_out();
                uint32_t c = rng_state;
                gen_poly_tick(&g, &CLK0, &rng_state, &o);
                draws &= draws_since(c) == 0u;
                range &= o.gate ? o.note >= 48 && o.note <= 82 && o.vel >= 88 && o.vel <= 127 : o.note == 60 && o.vel == 100;
                range &= g.pos < 4u && o.cv >= 0;
            }
        }
    {   /* 4 against 3: a note on steps 0 3 4 6 8 9 (twelve a cycle: 6 notes), step 0 louder (both land) */
        gen_poly_t g = poly_at(4, 3, 0, 0, 1, 1);
        static const uint8_t WANT[12] = {1, 0, 0, 1, 1, 0, 1, 0, 1, 1, 0, 0};
        for (uint32_t i = 0; i < 24; i++) {
            gen_out_t o = step_out();
            gen_poly_tick(&g, &CLK0, &rng_state, &o);
            hits &= o.gate == WANT[i % 12u];
            if (i % 12u == 0u) loud &= o.vel == 101u;
            else if (o.gate) loud &= o.vel == 88u;
        }
    }
    {   /* every rhythm OFF: silent */
        gen_poly_t g = poly_at(0, 0, 0, 0, 2, 3);
        for (uint32_t i = 0; i < 64; i++) { gen_out_t o = step_out(); gen_poly_tick(&g, &CLK0, &rng_state, &o); off &= !o.gate; }
    }
    {   /* the count wraps where every divider lands: no step lost or doubled across it */
        gen_poly_t g = poly_at(7, 11, 13, 16, 1, 9);
        uint32_t n = 0;
        g.tick = GEN_POLY_WRAP - 2u;
        for (uint32_t i = 0; i < 4; i++) { gen_out_t o = step_out(); gen_poly_tick(&g, &CLK0, &rng_state, &o); n += o.gate; }
        wrap &= n == 1u && g.tick == 2u;
    }
    for (uint32_t sd = 0; sd < 300; sd++)
        for (uint32_t span = 1; span <= 3; span++) {
            gen_poly_t g = poly_at(4, 3, 0, 0, span, sd);
            for (uint32_t i = 0; i < 4; i++) fill &= g.pitch[i] < 7u * span;
        }
    for (uint32_t amt = 0; amt <= 100; amt += 25)
        for (uint32_t span = 1; span <= 3; span++) {
            gen_poly_t g = poly_at(5, 2, 9, 0, span, 4), g0 = g;
            for (uint32_t n = 0; n < 200; n++) {
                gen_poly_mutate(&g, amt, &rng_state);
                for (uint32_t i = 0; i < 4; i++) mut &= g.pitch[i] < 7u * span && g.div[i] == g0.div[i];
            }
            if (!amt) mut &= !memcmp(g.pitch, g0.pitch, 4);
        }
    check("poly: notes 48..82, louder (88 + 13 a rhythm more) where rhythms land together; else a rest as ever", range);
    check("poly: no random draw a step", draws);
    check("poly: 4 against 3 plays steps 0 3 4 6 8 9 of every 12, step 0 louder; all OFF silent", hits && loud && off);
    check("poly: its count wraps at 720720 with no step lost or doubled", wrap);
    {   /* a FORM section keeps pitches above 15 (their fifth bits beside SPAN) */
        gen_poly_t g = poly_at(4, 3, 0, 0, 3, 1), h;
        gen_sec_t s;
        g.pitch[0] = 20, g.pitch[1] = 16, g.pitch[2] = 5, g.pitch[3] = 15;
        gen_poly_save(&g, &s);
        memset(&h, 0, sizeof h);
        gen_poly_load(&h, &s);
        mut &= h.span == 3u && !memcmp(h.pitch, g.pitch, 4);
    }
    check("poly: the seed's pitches and mutation's stay inside SPAN (1..3); a section keeps them; the rhythms stay", fill && mut);
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
    gen_grids_t gr = {40, 200, 50, 10, 3, {0, 0, 0}};
    gen_turing_save(&tu, &s); ok &= s.reg == 0xBEEFu && s.p0 == 7 && s.p1 == 9;    /* only its own fields */
    gen_markov_save(&mk, &s); ok &= s.reg == 0xBEEFu && s.p0 == 61 && s.p1 == 9;
    gen_euclid_save(&eu, &s); ok &= s.reg == 42 && s.p0 == 5 && s.p1 == 3;
    gen_morph_save(&mo, &s);  ok &= s.reg == 0x2222u && s.p0 == 99 && s.p1 == 3;
    gen_grids_save(&gr, &s);  ok &= s.reg == (40u | 200u << 8) && s.p0 == 99 && s.p1 == 3;
    {
        gen_bounce_t bo = bounce_at(33, 61, 3, 1);
        gen_sec_t b2 = {2, 80, 5};
        gen_bounce_save(&bo, &s); ok &= s.reg == 3u && s.p0 == 61 && s.p1 == 33;
        gen_bounce_load(&bo, &b2); ok &= bo.balls == 2u && bo.angle == 80u && bo.speed == 5u;
    }
    {
        gen_awake_t aw = awake_at(9, 4, 50, 1);
        gen_sec_t a2 = {0x5555u, 3, 12};
        gen_awake_save(&aw, &s); ok &= s.reg == 3u && s.p0 == 9 && s.p1 == 4;
        gen_awake_load(&aw, &a2); ok &= aw.len_a == 3u && aw.len_b == 12u && aw.prob == 50u;
    }
    {
        gen_poly_t po = poly_at(4, 3, 0, 0, 2, 1);
        gen_sec_t p2 = {0xD210u, 1, 77};
        po.pitch[0] = 1, po.pitch[1] = 2, po.pitch[2] = 3, po.pitch[3] = 13;
        gen_poly_save(&po, &s); ok &= s.reg == 0xD321u && s.p0 == 2 && s.p1 == 4;
        gen_poly_load(&po, &p2); ok &= po.pitch[0] == 0 && po.pitch[1] == 1 && po.pitch[2] == 2 && po.pitch[3] == 13 &&
                                       po.span == 1 && po.div[0] == 4;
    }
    {
        gen_sec_t e = {42, 6, 2}, m = {0x8888u, 10, 0}, k = {0, 33, 0}, t = {0xACE1u, 0, 0};
        gen_euclid_load(&eu, &e); ok &= eu.pulses == 6 && eu.rot == 2 && eu.pitch_seed == 42 && eu.steps == 16;
        gen_morph_load(&mo, &m);  ok &= mo.snap_a == 0x8888u && mo.morph_pos == 10 && mo.snap_b == 0xAAAAu;
        gen_markov_load(&mk, &k); ok &= mk.tension == 33 && mk.density == 100;
        gen_turing_load(&tu, &t); ok &= tu.reg == 0xACE1u && tu.len == 16;
        gen_grids_load(&gr, &t);  ok &= gr.x == 0xE1u && gr.y == 0xACu && gr.dens == 50 && gr.chaos == 10;
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
    gen_grids_t gr = {77, 190, 70, 60, 0, {0, 0, 0}};
    gen_bounce_t bo = bounce_at(37, 61, 3, 4);
    gen_awake_t aw = awake_at(9, 5, 70, 3);
    gen_poly_t po = poly_at(4, 3, 5, 0, 2, 3);
    const uint32_t N = 2000000u;
    volatile uint32_t sink = 0;
    struct timespec a, b;
    clock_gettime(CLOCK_MONOTONIC, &a);
    for (uint32_t i = 0; i < N; i++) {
        gen_out_t o = step_out();
        if (kind == 0) gen_turing_tick(&tu, &CLK0, &rng_state, &o);
        else if (kind == 1) gen_euclid_tick(&eu, &CLK0, &rng_state, &o);
        else if (kind == 2) gen_markov_tick(&mk, &CLK0, &rng_state, &o);
        else if (kind == 3) gen_morph_tick(&mo, &CLK0, &rng_state, &o);
        else if (kind == 4) gen_grids_tick(&gr, &CLK0, &rng_state, &o);
        else if (kind == 5) gen_bounce_tick(&bo, &CLK0, &rng_state, &o);
        else if (kind == 6) gen_awake_tick(&aw, &CLK0, &rng_state, &o);
        else gen_poly_tick(&po, &CLK0, &rng_state, &o);
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
    grids_tests();
    bounce_tests();
    awake_tests();
    poly_tests();
    section_tests();
    printf("gen: host cost per step: Turing %.1f ns, Euclid %.1f ns, Markov %.1f ns, Morph %.1f ns, Grids %.1f ns, "
           "Bounce %.1f ns, Awake %.1f ns, Poly %.1f ns\n", ns_per_step(0), ns_per_step(1), ns_per_step(2), ns_per_step(3),
           ns_per_step(4), ns_per_step(5), ns_per_step(6), ns_per_step(7));
    printf(bad ? "gen: %d FAILED\n" : "gen: all passed\n", bad);
    return bad != 0;
}
