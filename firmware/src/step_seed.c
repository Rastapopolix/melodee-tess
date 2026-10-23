/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* STEP as seed (2.1, slice 1; tessera-notes docs/design/STEP_SEED.md): a TURING or EUCLID track plays from a STEP
 * pattern, SOURCE's (OFF, T1..T4: that track's pattern playing now, this track's own too), which it never changes.
 * TURING loads the source's first LEN steps into a loop and drifts it: each step, at PROB %, the step about to play
 * changes and stays so (a Turing machine's flipped bit). KEEP says what may change: FREE the hit or the pitch, RHYTHM
 * only the pitch (another note of the loop's, or a degree away), PITCH only the hit (a rest taking the last note
 * before it). PROB 0 plays the source as written. EUCLID plays the source's notes in order on its hits (FREE, PITCH);
 * KEEP RHYTHM plays the source's own rhythm on Euclid's pitches. The notes keep their written pitch, then this
 * track's scale, TRN and SHIFT. A source with no notes: as SOURCE OFF. Reloaded at PLAY and when SOURCE or LEN turn.
 * Draws from the track's stream, so it replays with its SEED. Included by tessera.c */
#define SD_LOOP 16u                                     /* TURING's LEN at most */
static struct {
    uint8_t loop[TESSERA_TRACKS][SD_LOOP];              /* TURING's loop: bit 7 a hit, bits 0..6 its note */
    uint8_t pos[TESSERA_TRACKS];                        /* the next step of the loop, or of the source (EUCLID RHYTHM) */
    uint16_t n[TESSERA_TRACKS];                         /* EUCLID: the hits played since the phrase began */
    uint8_t load[TESSERA_TRACKS];                       /* 1: TURING's loop to fill from the source first */
    uint8_t len[TESSERA_TRACKS];                        /* the LEN it was filled for (another LEN: filled again) */
} tess_sd __attribute__((section(".pool")));

/* step i of track s's pattern playing: 1 a hit (*note its note, a drum hit's lane note; *vel its velocity) */
static int sd_step(uint32_t s, uint32_t i, uint32_t *note, uint32_t *vel)
{
    const track_t *t = &trk[s];
    const step_t *st = &t->step[i % (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP)];
    if (st->time != ST_NOTE || (!st->n && !st->hit)) return 0;
    *note = st->n ? st->note[0] : DRUM_LANE_NOTE[__builtin_ctz(st->hit) % NLANE];
    *vel = (st->flags & SF_ACCENT) ? 127u : st->vel ? st->vel : 100u;
    return 1;
}
static uint32_t sd_len(uint32_t s) { return (uint32_t)clamp(trk[s].p[P_SLEN], 1, NSTEP); }
/* the source track of tr, + 1; 0: none (SOURCE OFF, not TURING or EUCLID, or a source without notes) */
static uint32_t sd_source(uint32_t tr)
{
    uint32_t s = tessera.sd_src[tr], i, note, vel;
    if (!s || s > TESSERA_TRACKS || (tessera.mode[tr] != TESS_ENG_TURING && tessera.mode[tr] != TESS_ENG_EUCLID))
        return 0;
    for (i = 0; i < sd_len(s - 1u); i++)
        if (sd_step(s - 1u, i, &note, &vel)) return s;
    return 0;
}
void tessera_sd_play(void)                              /* seq.c, PLAY: the loops filled again, from step 1 */
{
    for (uint32_t tr = 0; tr < TESSERA_TRACKS; tr++) {
        tess_sd.load[tr] = 1;
        tess_sd.pos[tr] = tess_sd.n[tr] = 0;
    }
}
static void sd_realign(uint32_t tr)                     /* a phrase boundary (tess_realign): from step 1 again */
{
    tess_sd.pos[tr] = tess_sd.n[tr] = 0;
}
static void sd_fill(uint32_t tr, uint32_t s)
{
    uint32_t i, note, vel, last = 0;
    for (i = 0; i < sd_len(s); i++)                     /* (a rest before the first hit: the pattern's last note) */
        if (sd_step(s, i, &note, &vel)) last = note;
    for (i = 0; i < SD_LOOP; i++) {
        int hit = sd_step(s, i, &note, &vel);
        if (hit) last = note;
        tess_sd.loop[tr][i] = (uint8_t)((hit ? 0x80u : 0u) | (last & 0x7Fu));
    }
    tess_sd.load[tr] = 0;
    tess_sd.len[tr] = tessera.turing[tr].len;
}
/* TURING's change to step p of its loop: KEEP RHYTHM a new pitch, KEEP PITCH the hit flipped, FREE either */
static void sd_drift(const track_t *t, uint32_t tr, uint32_t p, uint32_t len)
{
    uint8_t *e = &tess_sd.loop[tr][p];
    uint32_t keep = tessera.sd_keep[tr], pitch = keep == TESS_SD_RHYTHM ? 1u : keep == TESS_SD_PITCH ? 0u :
                    gen_rnd(&tessera.rnd[tr], 2u);
    if (!pitch) {
        *e ^= 0x80u;
        return;
    }
    if (!(*e & 0x80u)) return;                          /* (a rest has no pitch to change) */
    {
        uint32_t i = gen_rnd(&tessera.rnd[tr], len), k, note = *e & 0x7Fu;
        for (k = 0; k < len; k++, i = (i + 1u) % len)   /* another note of the loop's .. */
            if ((tess_sd.loop[tr][i] & 0x80u) && (tess_sd.loop[tr][i] & 0x7Fu) != note) break;
        if (k < len) note = tess_sd.loop[tr][i] & 0x7Fu;
        else note = (uint32_t)clamp(tess_scale_steps(t, (int32_t)note, gen_rnd(&tessera.rnd[tr], 2u) ? 1 : -1), 0, 127);
        *e = (uint8_t)(0x80u | note);                   /* .. or, the loop all one note, a degree away */
    }
}
/* a generator's raw note playing the source's note n (tessera_clock_step adds the key back, then snaps) */
static uint8_t sd_raw(const track_t *t, uint32_t tr, uint32_t n)
{
    return (uint8_t)clamp((int32_t)n - tess_key(t, tr), 0, 127);
}
/* TURING with a SOURCE: this step from its loop (instead of gen_turing_tick); 0 = no source */
static int sd_turing(const track_t *t, uint32_t tr, gen_out_t *out)
{
    uint32_t s = sd_source(tr), len = tessera.turing[tr].len, p, note, vel;
    if (!s) return 0;
    len = len < 2u ? 2u : len > SD_LOOP ? SD_LOOP : len;
    if (tess_sd.load[tr] || tess_sd.len[tr] != tessera.turing[tr].len) sd_fill(tr, s - 1u);
    p = tess_sd.pos[tr] % len;
    tess_sd.pos[tr] = (uint8_t)((p + 1u) % len);
    if (tessera.turing[tr].prob && gen_rnd(&tessera.rnd[tr], 100u) < tessera.turing[tr].prob) sd_drift(t, tr, p, len);
    note = tess_sd.loop[tr][p] & 0x7Fu;
    out->cv = (int16_t)(note * 32767u / 127u);
    if (!(tess_sd.loop[tr][p] & 0x80u)) return 1;
    out->gate = 1;
    out->note = sd_raw(t, tr, note);
    out->vel = (uint8_t)(sd_step(s - 1u, p, &note, &vel) ? vel : 100u);
    return 1;
}
/* EUCLID with a SOURCE, after gen_euclid_tick: its hit plays the source's next note (FREE, PITCH); RHYTHM: the source's
 * hit plays Euclid's pitch for the step */
static void sd_euclid(const track_t *t, uint32_t tr, gen_out_t *out)
{
    uint32_t s = sd_source(tr), note, vel, i, k, len;
    if (!s) return;
    s--;
    len = sd_len(s);
    if (tessera.sd_keep[tr] == TESS_SD_RHYTHM) {
        uint32_t p = tess_sd.pos[tr] % len;
        tess_sd.pos[tr] = (uint8_t)((p + 1u) % len);
        out->gate = (uint8_t)sd_step(s, p, &note, &vel);
        if (!out->gate) return;
        out->note = (uint8_t)(48u + gen_euclid_degree(tessera.euclid[tr].pitch_seed, p));
        out->vel = (uint8_t)vel;
        return;
    }
    if (!out->gate) return;
    for (i = k = 0; i < len; i++)                       /* (the source's notes, counted) */
        k += (uint32_t)sd_step(s, i, &note, &vel);
    k = tess_sd.n[tr]++ % k;
    for (i = 0; i < len; i++)
        if (sd_step(s, i, &note, &vel) && !k--) break;
    out->note = sd_raw(t, tr, note);
    out->vel = (uint8_t)vel;
}

/* OCT+ held on GEN, TURING or EUCLID: KNOB 1 SOURCE, KNOB 2 KEEP (ui_input.c) */
void tessera_sd_layer_knob(uint32_t k, int32_t s)
{
    uint32_t tr = song.sel;
    int32_t d = s > 0 ? 1 : -1;
    tessera_ensure_init();
    if (tr >= TESSERA_TRACKS || !s) return;
    TESS_IRQ_OFF();
    if (k == 0u) tess_param_set_body(tr, TX_SD_SOURCE, (int32_t)tessera.sd_src[tr] + d);
    else if (k == 1u) tess_param_set_body(tr, TX_SD_KEEP, (int32_t)tessera.sd_keep[tr] + d);
    TESS_IRQ_ON();
}
