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
/* Awake's 32 values from a seed (ROLL; a track's first): A a melody with rests (about one in four), B half its steps a
 * lift of 1..5 degrees (the rest 0), so the lower loop is heard at once */
static void gen_awake_fill(gen_awake_t *g, uint32_t seed)
{
    uint32_t s = gen_stream(seed * 13u + 5u), i;
    for (i = 0; i < 16u; i++) {
        gen_awake_put(g->a, i, gen_rnd(&s, 4u) ? 1u + gen_rnd(&s, 8u) : 0u);
        gen_awake_put(g->b, i, gen_rnd(&s, 2u) ? 0u : 1u + gen_rnd(&s, 5u));
    }
}
/* Poly's degrees over SPAN octaves (1..3) */
static uint32_t gen_poly_degs(const gen_poly_t *g) { return 7u * (g->span >= 1u && g->span <= 3u ? g->span : 1u); }
/* Poly's four pitches from a seed (ROLL; a track's first), over SPAN octaves */
static void gen_poly_fill(gen_poly_t *g, uint32_t seed)
{
    uint32_t s = gen_stream(seed * 29u + 11u), i, n = gen_poly_degs(g);
    for (i = 0; i < 4u; i++)
        g->pitch[i] = (uint8_t)gen_rnd(&s, n);
}

/* Quad's row from a seed (ROLL; a track's first): a rest one cell in six, the others over two octaves */
static void gen_quad_fill(gen_quad_t *g, uint32_t seed)
{
    uint32_t s = gen_stream(seed * 37u + 3u), i;
    for (i = 0; i < 4u; i++)
        g->cell[i] = (uint8_t)(gen_rnd(&s, 6u) ? 1u + gen_rnd(&s, 14u) : 0u);
}

/* Constellation: column c's star in layer l (0 the front, the one that plays; 1 and 2 the far ones, the picture's,
 * scrolling slower): 0 none, else bits 0..3 its height + 1, 4..5 its brightness, 6..7 its size. Worked out from the
 * sky's seed, so the sky needs no memory */
static uint32_t gen_cs_star(uint32_t sky, uint32_t c, uint32_t layer, uint32_t dens)
{
    uint32_t h = gen_stream(sky * 2654435761u ^ (c & 0xFFFFu) * 40503u ^ layer * 0x9E3779B9u);
    if (h % 100u >= dens) return 0;
    h >>= 8;
    return (h % GEN_CS_ROWS + 1u) | ((h >> 4) & 3u) << 4 | ((h >> 6) & 3u) << 6;
}
/* Constellation from a seed (ROLL; a track's first): a new sky, the loop empty (the ship catches it again) */
static void gen_constel_fill(gen_constel_t *g, uint32_t seed)
{
    g->sky = (uint16_t)(gen_stream(seed * 53u + 17u) >> 8);
    for (uint32_t i = 0; i < 16u; i++) g->loop[i] = 0;
}
/* Constellation back to the sky's start (PLAY, a new section): the first column, the ship at Y, still */
static void gen_constel_start(gen_constel_t *g)
{
    g->col = 0;
    g->ship = (uint16_t)(g->y * 256u + 128u);
    g->drift = 0;
    g->caught = 0;
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

/* Bounce: sin of 0..90 degrees, x 127 (every 6 degrees, the rest between) */
static const uint8_t GEN_SIN90[16] = {0, 13, 26, 39, 52, 64, 75, 85, 94, 103, 110, 116, 121, 124, 126, 127};
static uint32_t gen_sin90(uint32_t a)
{
    uint32_t i, f;
    a = a > 90u ? 90u : a;
    i = a / 6u, f = a % 6u;
    return (GEN_SIN90[i] * (6u - f) + GEN_SIN90[i < 15u ? i + 1u : 15u] * f) / 6u;
}
/* Bounce's balls back to their start: ball 1 the floor's middle, going up and right; each other one from a place,
 * a direction and an angle offset (8..30 degrees, either way) of the seed's, so they never move as one */
static void gen_bounce_serve(gen_bounce_t *g, uint32_t seed)
{
    uint32_t s = gen_stream(seed * 37u + 3u), k;
    g->x[0] = 128, g->y[0] = 0, g->dir = 0, g->off[0] = 0, g->hit = 0;
    for (k = 1; k < GEN_BOUNCE_MAX; k++) {
        int32_t o = 8 + (int32_t)gen_rnd(&s, 23u);
        g->x[k] = (uint8_t)(16u + gen_rnd(&s, 224u));
        g->y[k] = (uint8_t)(16u + gen_rnd(&s, 224u));
        g->dir |= (uint8_t)(gen_rnd(&s, 4u) << (2u * k));
        g->off[k] = (int8_t)(gen_rnd(&s, 2u) ? o : -o);
    }
    for (k = 0; k < GEN_FALL_LEAVES; k++) {         /* FALL's leaves: at heights of their own, so they land apart */
        g->lx[k] = (uint8_t)gen_rnd(&s, 256u);
        g->ly[k] = (uint8_t)(64u + gen_rnd(&s, 192u));
        g->lv[k] = (uint8_t)((4u + gen_rnd(&s, 16u)) | (gen_rnd(&s, 7u) ? 0u : 0x80u));
        g->lt[k] = 0;
    }
}
/* FALL: a leaf from the top again, at a place, a speed and (one in seven) a bass of the track's stream's */
static void gen_fall_spawn(gen_bounce_t *g, uint32_t k, uint32_t *rnd)
{
    g->lx[k] = (uint8_t)gen_rnd(rnd, 256u);
    g->ly[k] = 255;
    g->lv[k] = (uint8_t)((4u + gen_rnd(rnd, 16u)) | (gen_rnd(rnd, 7u) ? 0u : 0x80u));
}
/* FALL: each falling leaf drops its speed x SPEED / 16 (one at least) and drifts with the WIND (ANGLE - 45, its share of
 * the drop) and a sway of its own, round the box's sides. A leaf reaching the floor plays: where it lands two octaves of
 * the scale left to right, a bass leaf an octave down, faster ones louder; it rests 2..8 steps (the picture fades it),
 * then falls again from the top. Leaves landing on the same step play together, three notes at most, the lowest first */
static void gen_fall_tick(gen_bounce_t *g, uint32_t *rnd, gen_out_t *out)
{
    int32_t wind = (int32_t)(g->angle > 90u ? 90u : g->angle) - 45, sp = g->speed ? g->speed : 1;
    uint32_t k, got = 0, w;
    uint8_t note[GEN_FALL_LEAVES], vel[GEN_FALL_LEAVES];
    g->hit = 0;
    for (k = 0; k < GEN_FALL_LEAVES; k++) {
        int32_t v = g->lv[k] & 31u, dy, ph, sw, x, y;
        if (g->lt[k]) {
            if (!--g->lt[k]) gen_fall_spawn(g, k, rnd);
            continue;
        }
        if (v < 4) v = 4;
        dy = (v * sp + 15) / 16;
        ph = ((int32_t)g->ly[k] + (int32_t)k * 40) & 63;
        sw = ph < 32 ? ph - 16 : 48 - ph;               /* (-16..16: the sway, a triangle down the box) */
        x = ((int32_t)g->lx[k] + wind * dy / 30 + sw / 4) & 255;
        y = (int32_t)g->ly[k] - dy;
        g->lx[k] = (uint8_t)x;
        if (y > 0) {
            g->ly[k] = (uint8_t)y;
            continue;
        }
        {
            uint32_t deg = (uint32_t)x * 14u / 256u, n = 48u + 12u * (deg / 7u) + GEN_SCALE_MIN[deg % 7u], i;
            if (g->lv[k] & 0x80u) n -= 12u;
            for (i = got; i > 0 && note[i - 1u] > n; i--)
                note[i] = note[i - 1u], vel[i] = vel[i - 1u];
            note[i] = (uint8_t)n;
            vel[i] = (uint8_t)(70 + v * 3);
            got++;
        }
        g->ly[k] = 0;
        g->lt[k] = (uint8_t)(2u + gen_rnd(rnd, 7u));
        g->hit = 1;                                     /* (the floor) */
    }
    out->cv = (int16_t)(g->ly[0] * 32767 / 255);       /* leaf 1's height */
    for (k = 0, w = 0; k < got && w < 3u; k++) {
        if (k && note[k] == note[k - 1u]) continue;
        if (!w) {
            out->note = note[k];
            out->vel = vel[k];
            out->gate = 1;
        } else {
            out->more[out->more_n++] = note[k];
        }
        w++;
    }
}
/* Bounce: each ball moves SPEED at its angle (ANGLE and its offset, folded into 0..90), off the walls as a mirror.
 * A wall hit plays its note: the wall (floor 0, left 1, right 2, top 3) two scale degrees apart, where along it eight
 * more (two octaves in all; the top's accented); a ball in a corner plays the floor's or the top's. Hits on the same
 * step play together, three at most, the lowest wall first (a note twice: once); no random draw */
static void gen_bounce_tick(gen_bounce_t *g, const gen_clock_t *clk, uint32_t *rnd, gen_out_t *out)
{
    int32_t sp = g->speed ? g->speed : 1;
    uint32_t n = g->balls >= 1u && g->balls <= GEN_BOUNCE_MAX ? g->balls : 1u, k, w, got = 0;
    uint8_t note[GEN_BOUNCE_MAX], wall[GEN_BOUNCE_MAX];
    (void)clk;
    if (g->balls == GEN_BOUNCE_FALL) {
        gen_fall_tick(g, rnd, out);
        return;
    }
    g->hit = 0;
    for (k = 0; k < n; k++) {
        int32_t a = (int32_t)g->angle + g->off[k], hw = -1, at = 0;
        uint32_t d = (g->dir >> (2u * k)) & 3u;
        int32_t dx, dy, x, y;
        if (a < 0) a = -a;
        if (a > 90) a = 180 - a;
        dx = sp * (int32_t)gen_sin90((uint32_t)(90 - a)) / 127, dy = sp * (int32_t)gen_sin90((uint32_t)a) / 127;
        x = (int32_t)g->x[k] + (d & 1u ? -dx : dx), y = (int32_t)g->y[k] + (d & 2u ? -dy : dy);
        if (dx && (d & 1u) && x <= 0) x = -x, d ^= 1u, hw = 1, at = y;   /* (reaching a wall is a hit) */
        else if (dx && !(d & 1u) && x >= 255) x = 510 - x, d ^= 1u, hw = 2, at = y;
        if (dy && (d & 2u) && y <= 0) y = -y, d ^= 2u, hw = 0, at = x;
        else if (dy && !(d & 2u) && y >= 255) y = 510 - y, d ^= 2u, hw = 3, at = x;
        g->x[k] = (uint8_t)(x < 0 ? 0 : x > 255 ? 255 : x);
        g->y[k] = (uint8_t)(y < 0 ? 0 : y > 255 ? 255 : y);
        g->dir = (uint8_t)((g->dir & ~(3u << (2u * k))) | d << (2u * k));
        if (hw >= 0) {
            uint32_t deg = (uint32_t)hw * 2u + (uint32_t)(at < 0 ? 0 : at > 255 ? 255 : at) * 8u / 256u, i;
            g->hit |= (uint8_t)(1u << hw);
            for (i = got; i > 0 && wall[i - 1u] > (uint32_t)hw; i--)   /* (in wall order, ball order within a wall) */
                note[i] = note[i - 1u], wall[i] = wall[i - 1u];
            note[i] = (uint8_t)(48u + 12u * (deg / 7u) + GEN_SCALE_MIN[deg % 7u]);
            wall[i] = (uint8_t)hw;
            got++;
        }
    }
    out->cv = (int16_t)(g->y[0] * 32767 / 255);         /* ball 1's height */
    for (k = 0, w = 0; k < got && w < 3u; k++) {
        uint32_t i, dup = 0;
        for (i = 0; i < k; i++) dup |= note[i] == note[k];
        if (dup) continue;
        if (!w) {
            out->note = note[k];
            out->vel = wall[k] == 3u ? 115 : 100;
            out->gate = 1;
        } else {
            out->more[out->more_n++] = note[k];
        }
        w++;
    }
}

/* Awake: A's step and B's, each on its own loop: A's value a scale degree (0 a rest), B's added to it, two octaves in
 * all; PROB the chance a step plays (a draw only below 100). The CV is A's value */
static void gen_awake_tick(gen_awake_t *g, const gen_clock_t *clk, uint32_t *rnd, gen_out_t *out)
{
    uint32_t la = g->len_a >= 1u && g->len_a <= 16u ? g->len_a : 16u, lb = g->len_b >= 1u && g->len_b <= 16u ? g->len_b : 16u;
    uint32_t a = gen_awake_a(g, g->pos_a % la), b = gen_awake_b(g, g->pos_b % lb);
    (void)clk;
    g->pos_a = (uint8_t)((g->pos_a % la + 1u) % la);
    g->pos_b = (uint8_t)((g->pos_b % lb + 1u) % lb);
    out->cv = (int16_t)((a > 8u ? 8u : a) * 32767u / 8u);
    if (a && (g->prob >= 100u || gen_rnd(rnd, 100u) < g->prob)) {
        uint32_t deg = a - 1u + (b > 7u ? 7u : b);
        out->note = (uint8_t)(48u + 12u * (deg / 7u) + GEN_SCALE_MIN[deg % 7u]);
        out->vel = 100;
        out->gate = 1;
    }
}

/* Poly: the rhythms that land on this step (its count a multiple of their divider) advance the sequence a step and play
 * it, louder for each one more that lands with it; no draw. The CV: the step, a ramp over the four */
#define GEN_POLY_WRAP 720720u                           /* (every divider 1..16 divides it: the count wraps unheard) */
static void gen_poly_tick(gen_poly_t *g, const gen_clock_t *clk, uint32_t *rnd, gen_out_t *out)
{
    uint32_t i, n = 0;
    (void)clk;
    (void)rnd;
    for (i = 0; i < 4u; i++)
        n += g->div[i] && g->div[i] <= 16u && g->tick % g->div[i] == 0u;
    g->tick = (g->tick + 1u) % GEN_POLY_WRAP;
    if (n) {
        uint32_t deg;
        g->pos = (uint8_t)((g->pos + 1u) & 3u);
        deg = g->pitch[g->pos] > 20u ? 20u : g->pitch[g->pos];
        out->note = (uint8_t)(48u + 12u * (deg / 7u) + GEN_SCALE_MIN[deg % 7u]);
        out->vel = (uint8_t)(88u + 13u * (n - 1u));
        out->gate = 1;
    }
    out->cv = (int16_t)((g->pos & 3u) * 32767u / 3u);
}

/* Quad: the playhead moves a cell (DIR; held: forward, for a LINK's move), the cell plays at PROB (a draw only below
 * 100 and on a note; RANDOM draws the cell first), its first cell louder. The CV: the cell's pitch, a rest 0 */
static void gen_quad_move(gen_quad_t *g, uint32_t *rnd)
{
    uint32_t p = g->pos;
    switch (g->dir) {
    case GEN_QD_BACK: p = p > 3u ? 3u : (p + 3u) & 3u; break;
    case GEN_QD_PEND:
        if (p > 3u) p = 0, g->back = 0;
        else if (g->back) { if (p) p--; else p = 1, g->back = 0; }
        else if (p < 3u) p++;
        else p = 2, g->back = 1;
        break;
    case GEN_QD_RAND: p = gen_rnd(rnd, 4u); break;
    default: p = p > 3u ? 0u : (p + 1u) & 3u; break;
    }
    g->pos = (uint8_t)p;
}
static void gen_quad_play(const gen_quad_t *g, uint32_t *rnd, gen_out_t *out)
{
    uint32_t v = g->pos > 3u ? 0u : g->cell[g->pos] > 21u ? 21u : g->cell[g->pos];
    out->cv = (int16_t)(v * 32767u / 21u);
    if (v && (g->prob >= 100u || gen_rnd(rnd, 100u) < g->prob)) {
        uint32_t deg = v - 1u;
        out->note = (uint8_t)(48u + 12u * (deg / 7u) + GEN_SCALE_MIN[deg % 7u]);
        out->vel = g->pos ? 96u : 112u;
        out->gate = 1;
    }
}
/* a step of the track's own clock: held, it rests where it is */
static void gen_quad_tick(gen_quad_t *g, const gen_clock_t *clk, uint32_t *rnd, gen_out_t *out)
{
    (void)clk;
    if (g->dir == GEN_QD_HOLD) {
        out->cv = (int16_t)(g->pos > 3u ? 0u : (g->cell[g->pos] > 21u ? 21u : g->cell[g->pos]) * 32767u / 21u);
        return;
    }
    gen_quad_move(g, rnd);
    gen_quad_play(g, rnd, out);
}
/* a step another track's LINK gives it */
static void gen_quad_step(gen_quad_t *g, uint32_t *rnd, gen_out_t *out)
{
    gen_quad_move(g, rnd);
    gen_quad_play(g, rnd, out);
}

/* Constellation's ship: AUTO OFF at Y. A wander: its drift changes by -1..1 a step (a draw), kept within AUTO x 2,
 * turning back at the sky's edges. SEEK: towards the next star in reach (GEN_CS_AHEAD columns), a row and a half a
 * step at most, no draw */
static void gen_cs_steer(gen_constel_t *g, uint32_t *rnd)
{
    int32_t s = g->ship, top = (int32_t)(GEN_CS_ROWS - 1u) * 256 + 128;
    if (!g->autop) {
        g->ship = (uint16_t)(g->y * 256u + 128u);
        return;
    }
    if (g->autop >= GEN_CS_SEEK) {
        uint32_t k, st = 0;
        for (k = 0; k < GEN_CS_AHEAD && !st; k++) st = gen_cs_star(g->sky, g->col + k, 0, g->dens);
        if (st) {
            int32_t d = (int32_t)((st & 15u) - 1u) * 256 + 128 - s;
            s += d > 384 ? 384 : d < -384 ? -384 : d;
        }
    } else {
        int32_t d = (int32_t)g->drift + (int32_t)gen_rnd(rnd, 3u) - 1, m = (int32_t)g->autop * 2;
        d = d > m ? m : d < -m ? -m : d;
        s += d * 16;
        if (s < 128) s = 256 - s, d = -d;
        if (s > top) s = 2 * top - s, d = -d;
        g->drift = (int8_t)d;
    }
    g->ship = (uint16_t)(s < 128 ? 128 : s > top ? top : s);
}
/* Constellation: the sky moves a column, the ship steers, and the star crossing the crosshair within SIZE rows of it is
 * caught into the loop's slot for this step (LOCK: the chance it leaves the slot as it was; a draw only between 0 and
 * 100); then the slot plays: its height two octaves and a degree of the scale, its brightness the velocity. The CV:
 * the ship's height */
static void gen_constel_tick(gen_constel_t *g, const gen_clock_t *clk, uint32_t *rnd, gen_out_t *out)
{
    uint32_t slot = clk->pos & 15u, st, v;
    int32_t row;
    g->col++;
    gen_cs_steer(g, rnd);
    row = g->ship >> 8;
    g->caught = 0;
    st = gen_cs_star(g->sky, g->col, 0, g->dens);
    if (st) {
        int32_t d = (int32_t)(st & 15u) - 1 - row, w = g->size > 7u ? 7 : (int32_t)g->size;
        if (d >= -w && d <= w && g->keep < 100u && (!g->keep || gen_rnd(rnd, 100u) >= g->keep)) {
            g->loop[slot] = (uint8_t)st;
            g->caught = (uint8_t)(slot + 1u);
        }
    }
    out->cv = (int16_t)(row * 32767 / (int32_t)(GEN_CS_ROWS - 1u));
    v = g->loop[slot];
    if (v & 15u) {
        uint32_t deg = (v & 15u) - 1u;
        out->note = (uint8_t)(48u + 12u * (deg / 7u) + GEN_SCALE_MIN[deg % 7u]);
        out->vel = (uint8_t)(70u + ((v >> 4) & 3u) * 19u);
        out->gate = 1;
    }
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

/* Bounce: the angle (FALL: the WIND) nudged by up to +-AMOUNT/100 of 15 degrees (one at least), turning back at 0 and 90 */
static void gen_bounce_mutate(gen_bounce_t *g, uint32_t amt, uint32_t *rnd)
{
    int32_t r = (int32_t)(amt * 15u / 100u) + 1, v = (int32_t)g->angle + (int32_t)gen_rnd(rnd, (uint32_t)(2 * r + 1)) - r;
    if (v < 0) v = -v;
    if (v > 90) v = 180 - v;
    g->angle = (uint8_t)(v < 0 ? 0 : v > 90 ? 90 : v);
}

/* Awake: 1 + AMOUNT/200 of A's loop's values drawn again (a rest one in four), as many of B's at AMOUNT % each (0 one
 * in two, else a lift of 1..5) */
static void gen_awake_mutate(gen_awake_t *g, uint32_t amt, uint32_t *rnd)
{
    uint32_t la = g->len_a >= 1u && g->len_a <= 16u ? g->len_a : 16u, lb = g->len_b >= 1u && g->len_b <= 16u ? g->len_b : 16u;
    uint32_t n = 1u + amt * la / 200u;
    if (!amt) return;
    while (n--) {
        uint32_t i = gen_rnd(rnd, la);
        gen_awake_put(g->a, i, gen_rnd(rnd, 4u) ? 1u + gen_rnd(rnd, 8u) : 0u);
    }
    n = 1u + amt * lb / 200u;
    while (n--)
        if (gen_rnd(rnd, 100u) < amt) {
            uint32_t i = gen_rnd(rnd, lb);
            gen_awake_put(g->b, i, gen_rnd(rnd, 2u) ? 0u : 1u + gen_rnd(rnd, 5u));
        }
}

/* Poly: each pitch at AMOUNT % moves a degree or two, turning back at the span's ends; the rhythms stay */
static void gen_poly_mutate(gen_poly_t *g, uint32_t amt, uint32_t *rnd)
{
    int32_t top = (int32_t)gen_poly_degs(g) - 1;
    for (uint32_t i = 0; i < 4u; i++) {
        int32_t v;
        if (gen_rnd(rnd, 100u) >= amt) continue;
        v = (int32_t)g->pitch[i] + (int32_t)(1u + gen_rnd(rnd, 2u)) * (gen_rnd(rnd, 2u) ? 1 : -1);
        if (v < 0) v = -v;
        if (v > top) v = 2 * top - v;
        g->pitch[i] = (uint8_t)(v < 0 ? 0 : v > top ? top : v);
    }
}

/* Quad alone (no other QUAD row to trade with): each cell at AMOUNT % moves a degree or two, turning back at the ends;
 * one in eight a note becomes a rest or a rest a note */
static void gen_quad_mutate(gen_quad_t *g, uint32_t amt, uint32_t *rnd)
{
    for (uint32_t i = 0; i < 4u; i++) {
        int32_t v;
        if (gen_rnd(rnd, 100u) >= amt) continue;
        if (!gen_rnd(rnd, 8u)) { g->cell[i] = (uint8_t)(g->cell[i] ? 0u : 1u + gen_rnd(rnd, 14u)); continue; }
        if (!g->cell[i]) continue;
        v = (int32_t)g->cell[i] + (int32_t)(1u + gen_rnd(rnd, 2u)) * (gen_rnd(rnd, 2u) ? 1 : -1);
        if (v < 1) v = 2 - v;
        if (v > 21) v = 42 - v;
        g->cell[i] = (uint8_t)(v < 1 ? 1 : v > 21 ? 21 : v);
    }
}

/* Constellation: each of the loop's stars let go at AMOUNT % (to be caught again) */
static void gen_constel_mutate(gen_constel_t *g, uint32_t amt, uint32_t *rnd)
{
    for (uint32_t i = 0; i < 16u; i++)
        if (g->loop[i] && gen_rnd(rnd, 100u) < amt) g->loop[i] = 0;
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
static void gen_bounce_save(const gen_bounce_t *g, gen_sec_t *s) { s->p0 = g->angle; s->p1 = g->speed; s->reg = g->balls; }
static void gen_bounce_load(gen_bounce_t *g, const gen_sec_t *s)
{
    g->angle = s->p0 > 90u ? 90u : s->p0;
    g->speed = s->p1 < 1u ? 1u : s->p1 > 64u ? 64u : s->p1;
    g->balls = (uint8_t)(s->reg < 1u ? 1u : s->reg > GEN_BOUNCE_FALL ? GEN_BOUNCE_FALL : s->reg);
}
/* Awake: its lengths here; its 32 values in the section's phrase buffer (tessera.c tess_save_sec) */
static void gen_awake_save(const gen_awake_t *g, gen_sec_t *s) { s->p0 = g->len_a; s->p1 = g->len_b; }
static void gen_awake_load(gen_awake_t *g, const gen_sec_t *s)
{
    g->len_a = s->p0 < 1u ? 1u : s->p0 > 16u ? 16u : s->p0;
    g->len_b = s->p1 < 1u ? 1u : s->p1 > 16u ? 16u : s->p1;
}
static void gen_poly_save(const gen_poly_t *g, gen_sec_t *s)
{
    s->reg = (uint16_t)((g->pitch[0] & 15u) | (g->pitch[1] & 15u) << 4 | (g->pitch[2] & 15u) << 8 | (g->pitch[3] & 15u) << 12);
    s->p0 = (uint8_t)(g->span | (g->pitch[0] >> 4) << 4 | (g->pitch[1] >> 4) << 5 | (g->pitch[2] >> 4) << 6 |
                      (g->pitch[3] >> 4) << 7);
}
static void gen_poly_load(gen_poly_t *g, const gen_sec_t *s)
{
    for (uint32_t i = 0; i < 4u; i++) {
        uint32_t v = ((s->reg >> (i * 4u)) & 0xFu) | ((s->p0 >> (4u + i)) & 1u) << 4;
        g->pitch[i] = (uint8_t)(v > 20u ? 20u : v);
    }
    g->span = (s->p0 & 15u) >= 1u && (s->p0 & 15u) <= 3u ? s->p0 & 15u : 1u;
}
static void gen_quad_save(const gen_quad_t *g, gen_sec_t *s)
{
    s->reg = (uint16_t)((g->cell[0] & 31u) | (g->cell[1] & 31u) << 5 | (g->cell[2] & 31u) << 10);
    s->p0 = g->cell[3];
}
static void gen_quad_load(gen_quad_t *g, const gen_sec_t *s)
{
    for (uint32_t i = 0; i < 4u; i++) {
        uint32_t v = i < 3u ? (s->reg >> (i * 5u)) & 31u : s->p0;
        g->cell[i] = (uint8_t)(v > 21u ? 21u : v);
    }
}
/* Constellation: a section's sky, Y, SIZE and DENS (the loop carries on, caught over as it flies) */
static void gen_constel_save(const gen_constel_t *g, gen_sec_t *s)
{
    s->reg = g->sky;
    s->p0 = (uint8_t)((g->y & 15u) | (g->size & 7u) << 4);
    s->p1 = g->dens;
}
static void gen_constel_load(gen_constel_t *g, const gen_sec_t *s)
{
    g->sky = s->reg;
    g->y = (uint8_t)((s->p0 & 15u) < GEN_CS_ROWS ? s->p0 & 15u : GEN_CS_ROWS - 1u);
    g->size = (uint8_t)((s->p0 >> 4) & 7u);
    g->dens = s->p1 > 100u ? 100u : s->p1;
}
