/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* Variation: a phrase played with changes that come and go, the phrase itself untouched (STEP EVOLVE: tessera.c
 * tessera_step_vary; ANSWER's phrases to come). A change a place, one byte (0: as written):
 *   bits 0-2  MOVE: -3..+3 scale degrees (two's complement); VARY_DROP (4): a rest
 *   bits 3-4  OCT: 1 an octave up, 2 down
 *   bits 5-6  SRC: whose notes the place plays: its own, the next note's (a SWAP's first half), the last note's (its
 *             second half; on an empty place a GHOST: softer)
 * A round (vary_round), as EVOLVE's LOCK on a generator: each change stays at LOCK % (else its place is as written
 * again), then new ones until AMOUNT's share of the places is changed (100: half of them). LOCK 100 holds a
 * variation, 0 makes a fresh one each round. The caller says what each place holds (VK_*) and plays the changes.
 * Included by tessera.c */
#define VARY_MAX 64u
#define VARY_DROP 4u
enum { VARY_OWN, VARY_NEXT, VARY_PREV };
enum { VK_EMPTY, VK_NOTE, VK_HITS, VK_KEEP };          /* a place: nothing, notes, drum hits only, not to be touched */

static int32_t vary_move(uint32_t m) { uint32_t d = m & 7u; return d == VARY_DROP ? 0 : d > 4u ? (int32_t)d - 8 : (int32_t)d; }
static int vary_dropped(uint32_t m) { return (m & 7u) == VARY_DROP; }
static int32_t vary_oct(uint32_t m) { uint32_t o = (m >> 3) & 3u; return o == 1u ? 1 : o == 2u ? -1 : 0; }
static uint32_t vary_src(uint32_t m) { return (m >> 5) & 3u; }

/* the place with notes (or hits) after i (dir 1) or before it (dir -1), round the phrase; len: none */
static uint32_t vary_find(const uint8_t *kind, uint32_t len, uint32_t i, int dir)
{
    for (uint32_t k = 1; k < len; k++) {
        uint32_t j = dir > 0 ? (i + k) % len : (i + len - k) % len;
        if (kind[j] == VK_NOTE || kind[j] == VK_HITS) return j;
    }
    return len;
}

/* a new change at a place: a note MOVEs (40 %), jumps an OCT (15), DROPs (15) or SWAPs with the next note (30); drum
 * hits DROP or SWAP; an empty place after a note a GHOST of it. 1 = made */
static int vary_new(uint8_t *mut, const uint8_t *kind, uint32_t len, uint32_t i, uint32_t *rnd)
{
    uint32_t r = gen_rnd(rnd, 100u), j;
    if (kind[i] == VK_EMPTY) {
        if (vary_find(kind, len, i, -1) == len) return 0;
        mut[i] = (uint8_t)(VARY_PREV << 5);
        return 1;
    }
    if (kind[i] == VK_NOTE && r < 40u) {
        static const int8_t MV[4] = {-2, -1, 1, 2};
        mut[i] = (uint8_t)((uint32_t)MV[gen_rnd(rnd, 4u)] & 7u);
        return 1;
    }
    if (kind[i] == VK_NOTE && r < 55u) {
        mut[i] = (uint8_t)((gen_rnd(rnd, 2u) ? 1u : 2u) << 3);
        return 1;
    }
    if (r < 70u || (kind[i] == VK_HITS && r < 85u)) {
        mut[i] = VARY_DROP;
        return 1;
    }
    j = vary_find(kind, len, i, 1);
    if (j == len || j == i || kind[j] != kind[i] || mut[j]) return 0;
    mut[i] = (uint8_t)(VARY_NEXT << 5);
    mut[j] = (uint8_t)(VARY_PREV << 5);
    return 1;
}

/* the place's change put back as written, a SWAP's both halves */
static void vary_undo(uint8_t *mut, const uint8_t *kind, uint32_t len, uint32_t i)
{
    uint32_t k;
    if (vary_src(mut[i]) == VARY_NEXT && (k = vary_find(kind, len, i, 1)) < len) mut[k] = 0;
    if (vary_src(mut[i]) == VARY_PREV && kind[i] != VK_EMPTY && (k = vary_find(kind, len, i, -1)) < len) mut[k] = 0;
    mut[i] = 0;
}
/* the changes a round leaves: AMOUNT's share of the places that can change, 100 half of them (0: none) */
static uint32_t vary_target(const uint8_t *kind, uint32_t len, uint32_t amount)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < len; i++) n += kind[i] != VK_KEEP;
    return amount ? (amount * n + 199u) / 200u : 0u;
}
/* one round over a phrase of len places (kind: what each holds): each change kept at LOCK %, then new ones at places
 * no change holds, until AMOUNT's share is changed */
static void vary_round(uint8_t *mut, const uint8_t *kind, uint32_t len, uint32_t amount, uint32_t lock, uint32_t *rnd)
{
    uint32_t want, i, k, changed = 0;
    if (len > VARY_MAX) len = VARY_MAX;
    for (i = 0; i < len; i++)
        if (kind[i] == VK_KEEP) mut[i] = 0;             /* (a place that changed what it holds: as written) */
    for (i = 0; i < len; i++)
        if (mut[i] && gen_rnd(rnd, 100u) >= lock) vary_undo(mut, kind, len, i);
    want = vary_target(kind, len, amount);
    for (i = 0; i < len; i++) changed += mut[i] != 0u;
    for (k = 0; changed < want && k < 4u * len; k++) {  /* (a few tries a place) */
        i = gen_rnd(rnd, len);
        if (kind[i] != VK_KEEP && !mut[i] && vary_new(mut, kind, len, i, rnd))
            for (changed = 0, i = 0; i < len; i++) changed += mut[i] != 0u;
    }
}
