/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* STEP EVOLVE: a STEP track's EVOLVE page (LOCK, EVERY, AMOUNT, CLEAR) evolves the steps written by hand. The steps
 * stay as written; each step plays its change (vary.c), made a round at a time: every EVERY bars, at the pattern's
 * first step, each change kept at LOCK %, then new ones up to AMOUNT's share (100: half the steps). A MOVE goes by degrees of the track's scale
 * (CHR: semitones), drum hits never MOVE or jump an OCT. Tied and recorded steps are left alone. AMOUNT 0 or EVERY OFF:
 * the steps as written. REC held keeps the changes: they become the steps (ui_input.c capture_take). Not saved: a
 * loaded project's steps start as written. Included by tessera.c */
_Static_assert(TESS_SV_STEPS == NSTEP && NSTEP <= VARY_MAX, "a change a step");

int tessera_step_on(uint32_t tr)
{
    return tr < TESSERA_TRACKS && tessera.mode[tr] == TESS_ENG_OFF && !trk[tr].p[P_AMODE];
}
static int sv_on(const track_t *t, uint32_t tr)
{
    (void)t;
    return tessera_step_on(tr) && tessera.sv_amount[tr] && tessera.sv_every[tr];
}
/* what step i holds (vary.c VK_*) */
static uint32_t sv_kind(const step_t *s)
{
    if (s->time == ST_TIE || (s->flags & SF_RECORDED)) return VK_KEEP;
    if (s->time == ST_REST || (!s->n && !s->hit)) return VK_EMPTY;
    return s->n ? VK_NOTE : VK_HITS;
}
/* note n moved d degrees of t's scale (CHR or none: semitones), 0..127 */
static uint32_t sv_degrees(const track_t *t, int32_t n, int32_t d)
{
    uint32_t m = scale_mask(t) & 0xFFFu, guard = 84;
    int32_t dir = d < 0 ? -1 : 1;
    if (!m) m = 0xFFFu;
    while (d && guard--) {
        n += dir;
        if ((m >> (uint32_t)((n - t->p[P_ROOT] % 12 + 1200) % 12)) & 1u) d -= dir;
    }
    return (uint32_t)(n < 0 ? 0 : n > 127 ? 127 : n);
}
/* step i of t as its change m plays it, into *out */
static void sv_apply(const track_t *t, uint32_t i, uint32_t m, step_t *out)
{
    const step_t *st = seq_steps(t), *s = &st[i];
    uint32_t len = t->p[P_SLEN] ? (uint32_t)t->p[P_SLEN] : 1u, src = vary_src(m), k;
    uint8_t kind[NSTEP];
    *out = *s;
    if (vary_dropped(m)) {
        out->time = ST_REST, out->n = 0, out->hit = 0, out->acc = 0;
        return;
    }
    if (src != VARY_OWN) {
        for (k = 0; k < len && k < NSTEP; k++) kind[k] = (uint8_t)sv_kind(&st[k]);
        k = vary_find(kind, len, i, src == VARY_NEXT ? 1 : -1);
        if (k < len) {
            const step_t *f = &st[k];
            memcpy(out->note, f->note, sizeof out->note);
            out->n = f->n, out->hit = f->hit, out->acc = f->acc, out->vel = f->vel;
            out->time = ST_NOTE;
            out->flags = (uint8_t)((s->flags & ~SF_ACCENT) | (f->flags & SF_ACCENT));
            if (sv_kind(s) == VK_EMPTY) {               /* a GHOST: softer, no accent */
                uint32_t v = f->vel ? f->vel : 96u;
                out->vel = (uint8_t)(v * 5u / 8u ? v * 5u / 8u : 1u);
                out->flags &= (uint8_t)~(SF_ACCENT | SF_SLIDE);
                out->acc = 0;
                out->probability = s->probability;
            }
        }
    }
    if (out->n && (vary_move(m) || vary_oct(m)))
        for (k = 0; k < out->n && k < 4u; k++)
            out->note[k] = (uint8_t)sv_degrees(t, (int32_t)out->note[k] + 12 * vary_oct(m), vary_move(m));
}

/* seq.c, a STEP track's step about to play (t->seq_idx): a round of changes when it is due, then the step as its
 * change plays it (out), or s itself */
const step_t *tessera_step_vary(track_t *t, const step_t *s, step_t *out)
{
    uint32_t tr = trk_index(t), i = t->seq_idx, len, k, m;
    if (!sv_on(t, tr)) return s;
    len = t->p[P_SLEN] ? (uint32_t)t->p[P_SLEN] : 1u;
    if (tessera.sv_flash[tr]) tessera.sv_flash[tr]--;
    if (!i && tessera.sv_steps[tr] >= tessera_every_bars(tessera.sv_every[tr]) * tess_steps_per_bar(t)) {
        uint8_t kind[NSTEP];
        const step_t *st = seq_steps(t);
        for (k = 0; k < len && k < NSTEP; k++) kind[k] = (uint8_t)sv_kind(&st[k]);
        if (!tessera.sv_rnd[tr]) tessera.sv_rnd[tr] = gen_stream(0x5EEDu + tr);
        vary_round(tessera.sv_mut[tr], kind, len, tessera.sv_amount[tr], tessera.sv_lock[tr], &tessera.sv_rnd[tr]);
        tessera.sv_steps[tr] = 0;
        tessera.sv_flash[tr] = 4;
    }
    tessera.sv_steps[tr]++;
    m = i < NSTEP ? tessera.sv_mut[tr][i] : 0u;
    if (!m || i >= len || sv_kind(s) == VK_KEEP) return s;
    sv_apply(t, i, m, out);
    return out;
}

/* the steps of track tr playing a change now (CLEAR's count; REC held keeps them) */
uint32_t tessera_step_varied(uint32_t tr)
{
    uint32_t n = 0, len;
    if (tr >= TESSERA_TRACKS || !sv_on(&trk[tr], tr)) return 0;
    len = trk[tr].p[P_SLEN] ? (uint32_t)trk[tr].p[P_SLEN] : 1u;
    for (uint32_t i = 0; i < len && i < NSTEP; i++) n += tessera.sv_mut[tr][i] != 0u;
    return n;
}
/* the changes back to as written, from the next round on afresh (CLEAR; a pattern load) */
void tessera_step_clear(uint32_t tr)
{
    if (tr >= TESSERA_TRACKS) return;
    memset(tessera.sv_mut[tr], 0, sizeof tessera.sv_mut[tr]);
    tessera.sv_steps[tr] = 0;
}
/* REC held on a STEP track with changes: they become its steps (the caller: an undo level, the interrupt off, the
 * NOTES view rebuilt); the steps written that way played as before. Returns the steps changed */
uint32_t tessera_step_keep(uint32_t tr)
{
    track_t *t;
    step_t v[NSTEP];
    uint32_t n = 0, len, i;
    if (tr >= TESSERA_TRACKS) return 0;
    t = &trk[tr];
    len = t->p[P_SLEN] ? (uint32_t)t->p[P_SLEN] : 1u;
    for (i = 0; i < len && i < NSTEP; i++)              /* (each from the steps as written: then written over) */
        if (tessera.sv_mut[tr][i] && sv_kind(&t->step[i]) != VK_KEEP) sv_apply(t, i, tessera.sv_mut[tr][i], &v[i]);
    for (i = 0; i < len && i < NSTEP; i++)
        if (tessera.sv_mut[tr][i] && sv_kind(&t->step[i]) != VK_KEEP) {
            t->step[i] = v[i];
            n++;
        }
    tessera_step_clear(tr);
    return n;
}
