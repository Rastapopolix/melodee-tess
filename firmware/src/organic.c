/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* SLOTH and OCHD: slow, never quite repeating motion, each track's own, as cord sources (cord.c CS_TORPOR ..
 * CS_OCHD8). The ideas of Nonlinear Circuits' Triple Sloth and DivKid's ochd; no code of either.
 *   SLOTH: three chaotic circuits, each a jerk system x''' = -0.6 x'' - x' + |x| - 1 (J. C. Sprott's piecewise linear
 *          one, 1997), at three speeds: APATHY about a minute a turn, INERTIA about three, TORPOR about seventeen
 *          (SPEED 50; each 12.5 more twice as fast). x leaves -2.45..1.25 never; out of -4..4 (it cannot, but a float
 *          is a float) a circuit starts again.
 *   OCHD:  eight free-running LFOs, the slowest a turn in 64 s, each 1.6 times faster than the one before (no two in
 *          step), a triangle bent towards a sine and leaning forward (RATE 50; each 12.5 more twice as fast).
 * They run every audio block (cord_tick), stopped or playing; PLAY starts every track's from the place its SEED gives,
 * so a piece replays. Settings: tessera.org_speed / org_rate (the Tessera block, TSR5). Included by tessera.c. */

#define ORG_BPS (44100.0f / 32.0f)                    /* audio blocks a second (32 samples each: CTL) */
#define ORG_LOOP 6.45f                                /* the jerk system's time units for one turn, about */

typedef struct {
    float x[3], y[3], z[3];                           /* SLOTH: TORPOR, APATHY, INERTIA */
    float due[3];                                     /* their time not stepped yet */
    uint32_t ph[8];                                   /* OCHD 1..8: their phases */
} organic_t;
static organic_t organic[TESSERA_TRACKS];

static const float ORG_SLOTH_S[3] = {1020.0f, 62.0f, 185.0f};   /* seconds a turn at SPEED 50 */
static const float ORG_OCHD_S[8] = {64.0f, 40.0f, 25.0f, 15.6f, 9.77f, 6.1f, 3.81f, 2.38f};

static float org_rate_k(uint32_t v)                   /* 0..100 -> 1/16 .. 16, 50: 1 (2^((v-50)/12.5)) */
{
    static const float OCT[9] = {1.0f / 16, 1.0f / 8, 1.0f / 4, 1.0f / 2, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f};
    uint32_t i = v > 100u ? 100u : v, o = i * 2u / 25u, f = i * 2u % 25u;   /* (octave o, f/25 of the next) */
    float u = (float)f * (1.0f / 25.0f);
    return o >= 8u ? OCT[8] : OCT[o] * (1.0f + u * (0.6565f + 0.3435f * u));   /* (2^u within 0.3 %) */
}
static uint32_t org_hash(uint32_t k)
{
    k = (k ^ (k >> 16)) * 0x7FEB352Du;
    k = (k ^ (k >> 15)) * 0x846CA68Bu;
    return k ^ (k >> 16);
}
static float org_jit(uint32_t k) { return (float)(int32_t)(org_hash(k) & 0xFFFFu) * (0.1f / 65536.0f) - 0.05f; }
/* circuit c of track tr where its seed starts it (near the attractor: every such start stays on it) */
static void org_circuit(uint32_t tr, uint32_t c)
{
    uint32_t s = (uint32_t)tessera.seed[tr] * 0x9E3779B9u + tr * 977u + c * 131u;
    organic[tr].x[c] = -0.5f + org_jit(s);
    organic[tr].y[c] = org_jit(s + 1u);
    organic[tr].z[c] = org_jit(s + 2u);
    organic[tr].due[c] = 0.0f;
}
static void org_start(uint32_t tr)
{
    uint32_t c;
    for (c = 0; c < 3u; c++) org_circuit(tr, c);
    for (c = 0; c < 8u; c++) organic[tr].ph[c] = org_hash((uint32_t)tessera.seed[tr] * 31u + tr * 8u + c);
}
void organic_play(void)                               /* seq.c seq_start: every track's from its SEED */
{
    for (uint32_t tr = 0; tr < TESSERA_TRACKS; tr++) org_start(tr);
}

static void organic_tick(void)                        /* an audio block (cord_tick) */
{
    static uint8_t ready;
    uint32_t tr, c;
    if (!ready) { ready = 1; organic_play(); }
    for (tr = 0; tr < TESSERA_TRACKS; tr++) {
        organic_t *o = &organic[tr];
        float ks = org_rate_k(tessera.org_speed[tr]), kr = org_rate_k(tessera.org_rate[tr]);
        for (c = 0; c < 3u; c++) {                    /* semi-implicit Euler, a step once 0.004 is due (a block's
                                                       * share is too small for a float at the slow speeds) */
            float dt = (o->due[c] += ks * (ORG_LOOP / ORG_BPS) / ORG_SLOTH_S[c]), x = o->x[c];
            if (dt < 0.004f) continue;
            o->due[c] = 0.0f;
            o->z[c] += (-0.6f * o->z[c] - o->y[c] + (x < 0.0f ? -x : x) - 1.0f) * dt;
            o->y[c] += o->z[c] * dt;
            o->x[c] = x + o->y[c] * dt;
            if (!(o->x[c] > -4.0f && o->x[c] < 4.0f)) org_circuit(tr, c);   /* (NaN too) */
        }
        for (c = 0; c < 8u; c++)
            o->ph[c] += (uint32_t)(kr * (4294967296.0f / ORG_BPS) / ORG_OCHD_S[c]);
    }
}

/* output k of track tr's (0 TORPOR, 1 APATHY, 2 INERTIA, 3..10 OCHD 1..8), Q15 -32767..32767 */
static int32_t organic_out(uint32_t tr, uint32_t k)
{
    const organic_t *o = &organic[tr % TESSERA_TRACKS];
    if (k < 3u) {                                     /* x: -2.45..1.25 around -0.6 */
        float v = (o->x[k] + 0.6f) * (1.0f / 1.85f);
        v = v < -1.0f ? -1.0f : v > 1.0f ? 1.0f : v;
        return (int32_t)(v * 32767.0f);
    }
    if (k < 11u) {                                    /* a triangle rising 55 % of the turn, then bent: t (1.5 - t^2 / 2) */
        uint32_t p = o->ph[k - 3u];
        float t = p < 0x8CCCCCCDu ? (float)p * (2.0f / 2362232013.0f) - 1.0f
                                  : 1.0f - (float)(p - 0x8CCCCCCDu) * (2.0f / 1932735283.0f);
        return (int32_t)(t * (1.5f - 0.5f * t * t) * 32767.0f);
    }
    return 0;
}
