/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess)
 * Held-key timing after Inputsignwave's FM-1 Studio Tess, Performance 6 (tess_features.c, features_duration_*;
 * github.com/jamesusmusic/fm1-studio-tess, GPL-3.0-only) */
/* NOTE LENGTH (the LENGTH page): how long a generator holds its notes, whatever DIV.
 *   STEP   as before: the step's GATE (PATTERN's), HUMAN shortening it
 *   FIXED  1/32 .. 1 bar from the note's start, up to KEYS notes held at once (a fifth: the one nearest its end
 *          goes); the same pitch again ends the one before
 *   HIT    held until the next hit: a rest or a DROP keeps it sounding
 * Held keys only: the release tail is the sound's (its envelope, the voice allocator). FIXED's length is counted from
 * the tempo at the note's start. Grids (drums) keeps STEP. The held notes end on STOP, MUTE, a MODE change, a pattern
 * change and panic. Included by tessera.c */
enum { NL_STEP, NL_FIXED, NL_HIT, NL_COUNT };
#define NL_KEYS 4u
#define NL_HELD 0xFFFFFFFFu                            /* left: HIT's, until the next hit */
static const uint8_t NL_DIV[6] = {3, 2, 1, 0, 6, 7};   /* LEN -> DIV (seq_div_samples): 1/32 1/16 1/8 1/4 1/2 1/1 */

/* the held notes: samples left (NL_HELD: until the next hit), the pitch. In .pool, not cleared at start: a track's
 * four are valid only while its bit in nl_active is set (nl_on clears them first) */
static struct { uint32_t left; uint8_t note, on; } nl_run[TESSERA_TRACKS][NL_KEYS] __attribute__((section(".pool")));
static uint8_t nl_active;

static void nl_release(track_t *t, uint32_t tr, uint32_t k)
{
    uint32_t note = nl_run[tr][k].note;
    if (!nl_run[tr][k].on) return;
    nl_run[tr][k].on = 0;
    if (!(live_held[tr][note >> 5] & (1u << (note & 31u))) && (!t->arp_note || t->arp_note != note) &&
        !recording_owns(t, note))                     /* (arp_note 0: no arp note, so a held note 0 still ends) */
        trk_note_off(t, note);
}

static void nl_stop(track_t *t)
{
    uint32_t tr = trk_index(t);
    if (tr >= TESSERA_TRACKS || !(nl_active & (1u << tr))) return;
    for (uint32_t k = 0; k < NL_KEYS; k++) nl_release(t, tr, k);
    nl_active &= (uint8_t)~(1u << tr);
}
void tessera_len_stop(track_t *t) { nl_stop(t); }

/* each block, before the step (seq.c seq_tick): FIXED's notes end on time; stopped, muted or STEP again: all end */
void tessera_len_tick(track_t *t, uint32_t n)
{
    uint32_t tr = trk_index(t), any = 0;
    if (tr >= TESSERA_TRACKS || !(nl_active & (1u << tr))) return;
    if (tessera.mode[tr] == TESS_ENG_OFF || tessera.nl_mode[tr] == NL_STEP || t->p[P_MUTE] || !song.playing) {
        nl_stop(t);
        return;
    }
    for (uint32_t k = 0; k < NL_KEYS; k++) {
        if (!nl_run[tr][k].on) continue;
        if (nl_run[tr][k].left != NL_HELD) {
            if (nl_run[tr][k].left <= n) { nl_release(t, tr, k); continue; }
            nl_run[tr][k].left -= n;
        }
        any = 1;
    }
    if (!any) nl_active &= (uint8_t)~(1u << tr);
}

/* a hit in FIXED or HIT (tess_clock_step_body): what it ends, then the note */
static void nl_on(track_t *t, uint32_t tr, uint32_t note, uint32_t vel)
{
    uint32_t k, held = 0, slot = 0, hit = tessera.nl_mode[tr] == NL_HIT, keys = tessera.nl_keys[tr];
    if (!(nl_active & (1u << tr)))
        for (k = 0; k < NL_KEYS; k++) nl_run[tr][k].on = 0;
    for (k = 0; k < NL_KEYS; k++) {                   /* HIT and KEYS 1: one note; the same pitch: once */
        if (nl_run[tr][k].on && (hit || keys < 2u || nl_run[tr][k].note == note)) nl_release(t, tr, k);
        held += nl_run[tr][k].on;
    }
    if (held >= keys) {                               /* KEYS held: the one nearest its end goes */
        uint32_t m = NL_KEYS;
        for (k = 0; k < NL_KEYS; k++)
            if (nl_run[tr][k].on && (m == NL_KEYS || nl_run[tr][k].left < nl_run[tr][m].left)) m = k;
        if (m < NL_KEYS) nl_release(t, tr, m);
    }
    while (slot < NL_KEYS - 1u && nl_run[tr][slot].on) slot++;   /* (a free one: KEYS <= NL_KEYS) */
    nl_run[tr][slot].left = hit ? NL_HELD : seq_div_samples(NL_DIV[tessera.nl_len[tr] % 6u]);
    nl_run[tr][slot].note = (uint8_t)note;
    nl_run[tr][slot].on = 1;
    nl_active |= (uint8_t)(1u << tr);
    trk_note_on(t, note, vel);
}

/* LENGTH page encoders: 0 MODE, 1 LEN, 2 KEYS (3, GATE, is PATTERN's own: ui_input.c) */
static void nl_set_body(uint32_t tr, uint32_t x, int32_t v)
{
    switch (x) {
    case TX_NL_MODE:
        v = v < 0 ? 0 : v >= NL_COUNT ? NL_COUNT - 1 : v;
        if ((uint8_t)v == tessera.nl_mode[tr]) return;
        nl_stop(&trk[tr]);                            /* (STEP's gate or the new mode's keys from the next hit) */
        if (tessera.active_gate[tr]) {
            trk_note_off(&trk[tr], tessera.last_note[tr]);
            tessera.active_gate[tr] = 0;
            seq_release(&trk[tr]);
        }
        tessera.nl_mode[tr] = (uint8_t)v;
        return;
    case TX_NL_LEN: tessera.nl_len[tr] = (uint8_t)(v < 0 ? 0 : v > 5 ? 5 : v); return;
    case TX_NL_KEYS: tessera.nl_keys[tr] = (uint8_t)(v < 1 ? 1 : v > (int32_t)NL_KEYS ? (int32_t)NL_KEYS : v); return;
    }
}
void tessera_len_knob(uint32_t k, int32_t s)
{
    static const uint8_t X[3] = {TX_NL_MODE, TX_NL_LEN, TX_NL_KEYS};
    uint32_t tr = song.sel;
    tessera_ensure_init();
    if (tr >= TESSERA_TRACKS || k >= 3u || !s) return;
    TESS_IRQ_OFF();                                   /* (short lists: a step a turn, however quick) */
    nl_set_body(tr, X[k], tessera_param_get(tr, X[k]) + (s < 0 ? -1 : 1));
    TESS_IRQ_ON();
}
