/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess)
 * RATCH after Felucca's per-step RATCH (Leo Kuroshita, Hügelton Instruments; github.com/hugelton/Felucca, 1.4-1.6.1,
 * GPL-3.0-only): the idea and its x1..x4 in equal parts of a step (x8 ours), our own code */
/* SUB, sub-step timing (Tessera; included by seq.c): a step's notes again within the step. RATCH plays them HITS times
 * (x1..x8) in equal parts of the step, after Felucca's per-step RATCH, each hit its share of the step's gate. A step records
 * the notes it plays on its own voice (seq_sub_note), the sequencer arms the repeats after it (seq_sub_arm) and fires
 * them as the step goes on (seq_sub_tick, from seq_grid_tick). x1, or nothing recorded: nothing armed, the step plays
 * as before. Generator tracks: the track's RATCH (the TIMING page); STEP tracks: each step's (step_ratch, its sheet).
 * Not repeated: notes NOTE LENGTH holds (FIXED, HIT), BRANCHed and LINKed notes (another track's voice), and a
 * step that slides or ties into the next. In MIDI clock mode the positions are its musical units, as seq_pos's */
#define SUB_NOTES (4u + NLANE)
#define SUB_HITS_MAX 8u
enum { SUB_EVEN, SUB_ACCEL, SUB_DECEL, SUB_SHAPES };   /* SHAPE: where the hits fall (tessera.sub_shape) */
#define SUB_GATE_MIN 32u                                /* samples: a repeat always sounds */
#ifndef SUB_NOTE_ON
#define SUB_NOTE_ON trk_note_on                         /* (tests/hostsim.c: logged as a generator's notes) */
#endif
typedef struct {
    uint32_t next, span, gate;      /* seq_pos of the next repeat, the hits' span, each hit's gate at most */
    int32_t start;                  /* the span's start into the step (below 0: before its line, NUDGE) */
    uint8_t left, n;                /* repeats still to come; the notes recorded */
    uint8_t hits, k, shape;         /* the hits, the next one's number (1..), the track's SHAPE */
    uint8_t note[SUB_NOTES], vel[SUB_NOTES];
} seq_sub_t;
static seq_sub_t seq_sub[NTRK] __attribute__((section(".pool")));
static uint8_t seq_sub_rec;         /* bit per track: its step is recording into seq_sub[] (the pool is not cleared at boot) */
static uint8_t seq_sub_armed;       /* bit per track: repeats to come */
/* NUDGE (STEP tracks): a step's notes NUDGE 16ths of a step late (> 0: held on its line, played as the step goes on) or
 * early (< 0: played near the end of the step before; its own line then plays nothing). Not across a pattern's or a
 * song's turn, nor while recording: there the next step is not certain yet, so it plays on its line */
static struct {
    step_t s;                       /* the step held (its change applied, STEP EVOLVE) */
    uint32_t at, span, period, skip; /* held: where it plays, into its step; the step's length; seq_step's period, skip */
    uint32_t lead;                  /* early: how far before its line it played */
    uint8_t hits;                   /* its RATCH */
} seq_nudge[NTRK] __attribute__((section(".pool")));
static uint8_t seq_nudge_due;       /* bit per track: a step held, to play at seq_nudge[].at */
static uint8_t seq_nudge_early;     /* bit per track: the next step played already */
/* HUMAN TIME (TIMING, any track's): its step late by up to TESS_HUMAN_MS ms, as a player a little behind the beat, never
 * early, never more than a quarter of the step; drawn from the track's SEED and the steps since PLAY (its own stream:
 * the notes the generators draw stay as they were), so a piece replays the same. TWEAK's HUMAN on a STEP track: its
 * notes' velocity and length, from a stream of their own (a generator's: tessera.c, with its notes) */
static uint8_t seq_hum_due;         /* bit per track: a generator step held, to play at seq_hum_at */
static uint32_t seq_hum_at[NTRK], seq_hum_span[NTRK], seq_hum_n[NTRK], seq_humv_n[NTRK];
static int32_t cord_tess_get(uint32_t tr, uint32_t x);   /* (cord.c) */
static uint32_t seq_human_late(uint32_t tr, uint32_t span);

/* PLAY, STOP: nothing recorded, armed or held on any track */
static void seq_sub_clear(void)
{
    seq_sub_rec = seq_sub_armed = seq_nudge_due = seq_nudge_early = seq_hum_due = 0;
    for (uint32_t k = 0; k < NTRK; k++) seq_hum_n[k] = seq_humv_n[k] = 0;
}

/* A step starts: what it plays is recorded from here, the last step's repeats are dropped */
static void seq_sub_begin(const track_t *t)
{
    uint32_t tr = trk_index(t);
    seq_sub[tr].n = 0;
    seq_sub[tr].left = 0;
    seq_sub_rec |= (uint8_t)(1u << tr);
    seq_sub_armed &= (uint8_t)~(1u << tr);
}

/* A note the step plays on its own voice: repeated if the step ratchets (a note already recorded is not doubled) */
static void seq_sub_note(const track_t *t, uint32_t note, uint32_t vel)
{
    uint32_t tr = trk_index(t), i;
    seq_sub_t *s = &seq_sub[tr];
    if (!(seq_sub_rec & (1u << tr)) || s->n >= SUB_NOTES) return;
    for (i = 0; i < s->n; i++)
        if (s->note[i] == note) return;
    s->note[s->n] = (uint8_t)note;
    s->vel[s->n] = (uint8_t)(vel < 1u ? 1u : vel > 127u ? 127u : vel);
    s->n++;
}

/* Something to repeat: notes recorded and still sounding (seq_notes, the step's GATE), no slide or tie held over */
static int seq_sub_can(const track_t *t)
{
    uint32_t tr = trk_index(t);
    return (seq_sub_rec & (1u << tr)) && seq_sub[tr].n && t->seq_n && !t->seq_hold;
}

/* where hit k of n falls, samples into span: EVEN k equal parts (as ever), ACCEL 1 - (1 - k/n)^2 (closer and closer),
 * DECEL (k/n)^2 (further apart). 32 bits: span * 64 stays below 2^32 for any step up to 4 bars at 20 BPM */
static uint32_t seq_sub_at(uint32_t span, uint32_t k, uint32_t n, uint32_t shape)
{
    if (shape == SUB_ACCEL) return span * (n * n - (n - k) * (n - k)) / (n * n);
    if (shape == SUB_DECEL) return span * (k * k) / (n * n);
    return span / n * k;
}
static int32_t seq_sub_pos(const seq_sub_t *s, uint32_t k)   /* (hit k: into the step) */
{
    return s->start + (int32_t)seq_sub_at(s->span, k, s->hits, s->shape);
}
static uint32_t seq_sub_gap(const seq_sub_t *s, uint32_t k)   /* (from hit k to the next, or to the span's end) */
{
    return seq_sub_at(s->span, k + 1u, s->hits, s->shape) - seq_sub_at(s->span, k, s->hits, s->shape);
}

/* After the step played: hits (1..8) in equal parts of span (the step's length, swing included, from where its first hit
 * played: start samples into the step, below 0 before its line); the first hit, which sounds already, gets its share of
 * the gate too. Recording ends here */
static void seq_sub_arm(track_t *t, uint32_t hits, uint32_t span, int32_t start)
{
    uint32_t tr = trk_index(t);
    seq_sub_t *s = &seq_sub[tr];
    int can = seq_sub_can(t);
    seq_sub_rec &= (uint8_t)~(1u << tr);
    if (!can || hits < 2u) return;
    if (hits > SUB_HITS_MAX) hits = SUB_HITS_MAX;
    s->span = span;
    s->start = start;
    s->hits = (uint8_t)hits;
    s->shape = tr < TESSERA_TRACKS && tessera.sub_shape[tr] < SUB_SHAPES ? tessera.sub_shape[tr] : (uint8_t)SUB_EVEN;
    s->gate = t->seq_off / hits;                        /* (each hit's share, at most its gap: seq_sub_gap) */
    if (s->gate > span / hits && s->shape == SUB_EVEN) s->gate = span / hits;
    s->k = 1;
    s->next = seq_sub_pos(s, 1) > 0 ? (uint32_t)seq_sub_pos(s, 1) : 0u;   /* (from the step's start: a block late is still
                                                         * in time) */
    s->left = (uint8_t)(hits - 1u);
    t->seq_off = s->gate < seq_sub_gap(s, 0) ? s->gate : seq_sub_gap(s, 0);
    if (t->seq_off < SUB_GATE_MIN) t->seq_off = SUB_GATE_MIN;
    seq_sub_armed |= (uint8_t)(1u << tr);
}

/* As the step goes on (seq_pos moved, cur_len the step's length): each repeat due plays the recorded notes again */
static void seq_sub_tick(track_t *t, uint32_t cur_len)
{
    uint32_t tr = trk_index(t), i;
    seq_sub_t *s = &seq_sub[tr];
    if (!(seq_sub_armed & (1u << tr))) return;
    while (s->left && t->seq_pos >= s->next && s->next < cur_len) {
        seq_release(t);
        for (i = 0; i < s->n; i++) {
            SUB_NOTE_ON(t, s->note[i], s->vel[i]);
            t->seq_notes[i] = s->note[i];
        }
        t->seq_n = s->n;
        t->seq_off = s->gate < seq_sub_gap(s, s->k) ? s->gate : seq_sub_gap(s, s->k);
        if (t->seq_off < SUB_GATE_MIN) t->seq_off = SUB_GATE_MIN;
        s->k++;
        s->next = seq_sub_pos(s, s->k) > 0 ? (uint32_t)seq_sub_pos(s, s->k) : 0u;
        s->left--;
    }
    if (!s->left) seq_sub_armed &= (uint8_t)~(1u << tr);
}

/* ---- NUDGE: a STEP track's steps off their lines */
static void seq_step(track_t *t, const step_t *s, uint32_t period, uint32_t skip);   /* (seq.c) */
/* a STEP track's step s plays now, at samples into its span (0: on its line), its RATCH armed */
static void seq_sub_play(track_t *t, const step_t *s, uint32_t period, uint32_t span, uint32_t skip, uint32_t at)
{
    uint32_t hits = step_ratch(s);
    if (hits > 1u) seq_sub_begin(t);
    seq_step(t, s, period, skip);
    if (hits > 1u) seq_sub_arm(t, seq_sub_can(t) ? hits : 1u, span - at, (int32_t)at);
}
/* the step on its line (seq_grid_tick): played now, or held for its NUDGE (> 0) and HUMAN TIME, or, played early
 * already (< 0), its RATCH armed from where it played */
static void seq_nudge_step(track_t *t, const step_t *s, uint32_t period, uint32_t span, uint32_t skip)
{
    uint32_t tr = trk_index(t), at;
    int32_t ng = step_nudge(s);
    if (seq_nudge_early & (1u << tr)) {
        seq_nudge_early &= (uint8_t)~(1u << tr);
        if (seq_nudge[tr].hits > 1u)
            seq_sub_arm(t, seq_sub_can(t) ? seq_nudge[tr].hits : 1u, span, -(int32_t)seq_nudge[tr].lead);
        return;
    }
    seq_nudge_due &= (uint8_t)~(1u << tr);
    at = (ng > 0 ? span * (uint32_t)ng / 16u : 0u) + (ng < 0 ? 0u : seq_human_late(tr, span));   /* (an early NUDGE:
                                                     * no HUMAN; it plays on its line here) */
    if (at > span * 15u / 16u) at = span * 15u / 16u;   /* (no later than NUDGE's latest) */
    if (!at) {
        seq_sub_play(t, s, period, span, skip, 0);
        return;
    }
    seq_nudge[tr].s = *s;
    seq_nudge[tr].at = at;
    seq_nudge[tr].span = span;
    seq_nudge[tr].period = period;
    seq_nudge[tr].skip = skip;
    seq_nudge_due |= (uint8_t)(1u << tr);
}
/* As a STEP track's step goes on (cur_len its length, len the pattern's): a held step plays when its place comes; the
 * next step plays early when its NUDGE (< 0) brings it into this one */
static void seq_nudge_tick(track_t *t, uint32_t period, uint32_t cur_len, uint32_t len)
{
    uint32_t tr = trk_index(t), ni, next_len, lead;
    const step_t *s;
    if ((seq_nudge_due & (1u << tr)) && t->seq_pos >= seq_nudge[tr].at && t->seq_pos < cur_len) {
        seq_nudge_due &= (uint8_t)~(1u << tr);
        seq_sub_play(t, &seq_nudge[tr].s, seq_nudge[tr].period, seq_nudge[tr].span, seq_nudge[tr].skip, seq_nudge[tr].at);
    }
    if ((seq_nudge_early & (1u << tr)) || t->seq_pos >= cur_len || chain.running || song.rec || seq_erase_active(t))
        return;
    ni = (t->seq_idx + 1u) % (len ? len : 1u);
    if (!ni && t->pattern_next < NPAT) return;         /* (the next pattern's first step: on its line) */
    s = &seq_steps(t)[ni];
    if (step_nudge(s) >= 0) return;
    next_len = step_samples(t, period, ni);
    lead = next_len * (uint32_t)-step_nudge(s) / 16u;
    if (t->seq_pos + lead < cur_len) return;
    {   /* its turn: played as if on its line (seq_idx, STEP EVOLVE's change), its RATCH armed on the line */
        step_t sv;
        uint16_t was = t->seq_idx;
        t->seq_idx = (uint16_t)ni;
        s = tessera_step_vary(t, s, &sv);
        seq_nudge[tr].s = *s;
        seq_nudge[tr].hits = (uint8_t)step_ratch(s);
        seq_nudge[tr].lead = cur_len - t->seq_pos;
        if (seq_nudge[tr].hits > 1u) seq_sub_begin(t);
        seq_step(t, &seq_nudge[tr].s, step_gate(s) ? next_len : period, 0);
        t->seq_idx = was;
        seq_nudge_early |= (uint8_t)(1u << tr);
    }
}

/* ---- HUMAN: a step a little late (HUMAN TIME), a STEP track's notes a little uneven (TWEAK's HUMAN) */
static uint32_t seq_hum_draw(uint32_t tr, uint32_t n, uint32_t salt)   /* draw n of track tr's stream salt */
{
    uint32_t x = ((uint32_t)tessera.seed[tr] << 16 ^ n ^ salt) * 0x9E3779B1u;
    x ^= x >> 15; x *= 0x2C1B3C6Du; x ^= x >> 12;
    return x;
}
static uint32_t seq_human_late(uint32_t tr, uint32_t span)   /* samples late, 0..: this step's draw */
{
    uint32_t ms = tr < TESSERA_TRACKS ? (uint32_t)cord_tess_get(tr, TX_SUB_HUMAN) : 0u, x, most;
    if (!ms) return 0;
    x = seq_hum_draw(tr, seq_hum_n[tr]++, 0x48554Du);
    most = 441u * ms / 10u;                             /* (44.1 samples a ms) */
    if (most > span / 4u) most = span / 4u;
    return most ? x % (most + 1u) : 0u;
}
/* TWEAK's HUMAN on a STEP track's step: its notes' velocity moved by up to HUMAN / 5 either way (the return), *cut
 * percent left off its gate (up to HUMAN / 2), as a generator's (tessera.c). HUMAN 0, another track: none, no draw */
static int32_t seq_human_vel(uint32_t tr, uint32_t *cut)
{
    uint32_t h, j, x;
    *cut = 0;
    if (tr >= TESSERA_TRACKS || tessera.mode[tr] != TESS_ENG_OFF) return 0;
    h = (uint32_t)cord_tess_get(tr, TX_TW_HUMAN);
    if (!h) return 0;
    x = seq_hum_draw(tr, seq_humv_n[tr]++, 0x56454Cu);
    j = h / 5u;
    *cut = (x >> 16) % (h / 2u + 1u);
    return (int32_t)((x & 0xFFFFu) % (2u * j + 1u)) - (int32_t)j;
}
static void seq_gen_play(track_t *t, uint32_t tr, uint32_t span, uint32_t at)   /* the generator step, at samples in */
{
    seq_sub_begin(t);                                   /* (SUB: the notes it plays recorded, its RATCH armed) */
    tessera_clock_step(t);
    seq_sub_arm(t, seq_sub_can(t) ? tessera_ratchet(tr) : 1u, span > at ? span - at : 1u, (int32_t)at);
}
/* a generator track's step on its line: played now, or held for HUMAN (one still held from the last step plays
 * first: its place did not come, a DIV change) */
static void seq_gen_step(track_t *t, uint32_t tr, uint32_t span)
{
    uint32_t late;
    if (seq_hum_due & (1u << tr)) {
        seq_hum_due &= (uint8_t)~(1u << tr);
        seq_gen_play(t, tr, 1u, 0u);
    }
    late = seq_human_late(tr, span);
    if (!late) { seq_gen_play(t, tr, span, 0u); return; }
    seq_hum_at[tr] = late;
    seq_hum_span[tr] = span;
    seq_hum_due |= (uint8_t)(1u << tr);
}
static void seq_gen_tick(track_t *t, uint32_t cur_len)   /* as the step goes on: a held step when its place comes */
{
    uint32_t tr = trk_index(t);
    if ((seq_hum_due & (1u << tr)) && t->seq_pos >= seq_hum_at[tr] && t->seq_pos < cur_len) {
        seq_hum_due &= (uint8_t)~(1u << tr);
        seq_gen_play(t, tr, seq_hum_span[tr], seq_hum_at[tr]);
    }
}
