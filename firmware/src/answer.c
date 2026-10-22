/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* ANSWER (after Zen-1's koi and petals, aceyprime/zen-1: the idea only, no code): the notes played on the track's keys
 * (or MIDI IN), up to 8, are its phrase, each with its velocity and its step; a bar with no key closes it. After a gap
 * (SPACE) it answers: the phrase's rhythm with its notes in ORDER (as played, reversed, shuffled), the answer moved up
 * to SHIFT scale degrees, and changes that come and go (vary.c: a note moved, an octave, dropped, swapped) at VARY.
 * Each answer is softer and more changed, until FADE's count is played and it forgets: it rests until the keys play
 * again. At the top of FADE it never forgets: its notes run as four loops of unequal lengths (LEN; Eno's tape loops),
 * so it never quite repeats. ECHO: a note now and then comes back once, softer, a few steps later, sometimes an octave
 * up. The phrase is saved (a project answers at PLAY); timing snaps to steps (SUB later). Included by tessera.c */

static const uint8_t AN_LOOPS[TESS_AN_LENS][4] = {{5, 7, 11, 13}, {13, 17, 23, 29}, {23, 29, 31, 37}};

/* the answers FADE gives: 1 (0) .. 16 (99); 0 at 100: never forgets */
uint32_t tessera_answer_count(uint32_t tr)
{
    uint32_t f = tessera.answer[tr % TESSERA_TRACKS].fade;
    return f >= 100u ? 0u : 1u + f * 15u / 99u;
}

/* the answers from the first again (PLAY, a mode change, a load): a phrase kept answers after its gap, none rests */
static void answer_restart(uint32_t tr)
{
    tess_an.phase[tr] = tessera.answer[tr].n ? TESS_AN_GAP : TESS_AN_IDLE;
    tess_an.cnt[tr] = 0;
    tess_an.ans[tr] = 0;
    tess_an.slot[tr] = 0;
    tess_an.shift[tr] = 0;
    tess_an.rnd[tr] = gen_stream(tessera.seed[tr] * 31u + 7u);
    memset(tess_an.mut[tr], 0, TESS_AN_NOTES);
    memset(tess_an.e_left[tr], 0, TESS_AN_ECHOES);
    for (uint32_t i = 0; i < TESS_AN_NOTES; i++) tess_an.ord[tr][i] = (uint8_t)i;
}

/* a note on the track's keys: the first after an answer (or a closed phrase) starts a new phrase; up to 8, a step
 * apart at least (keys while stopped: a step each) */
void tessera_answer_hear(track_t *t, uint32_t note, uint32_t vel)
{
    uint32_t tr = trk_index(t), n, at;
    tessera_answer_t *a;
    if (tr >= TESSERA_TRACKS || tessera.mode[tr] != TESS_ENG_ANSWER || !vel || note > 127u) return;
    a = &tessera.answer[tr];
    if (tess_an.phase[tr] != TESS_AN_HEAR) {
        answer_restart(tr);
        a->n = 0;
        tess_an.phase[tr] = TESS_AN_HEAR;
        tess_an.hclk[tr] = 0;
    }
    n = a->n;
    if (n >= TESS_AN_NOTES) return;
    at = tess_an.hclk[tr] > TESS_AN_AT_MAX ? TESS_AN_AT_MAX : tess_an.hclk[tr];
    if (n && at <= a->at[n - 1u]) at = a->at[n - 1u] + 1u;
    if (at > TESS_AN_AT_MAX) return;
    a->note[n] = (uint8_t)note;
    a->vel[n] = (uint8_t)(vel > 127u ? 127u : vel);
    a->at[n] = (uint8_t)at;
    a->n = (uint8_t)(n + 1u);
    tess_an.cnt[tr] = 0;
}

/* a new answer's (or loop round's) choices: the changes (vary.c, LOCK 50: half of them stay), the order, the SHIFT */
static void answer_round(uint32_t tr)
{
    const tessera_answer_t *a = &tessera.answer[tr];
    uint32_t n = a->n, cnt = tessera_answer_count(tr), amount = a->vary, i, *rnd = &tess_an.rnd[tr];
    uint8_t kind[TESS_AN_NOTES], *ord = tess_an.ord[tr];
    if (amount && cnt) amount += (100u - amount) * tess_an.ans[tr] / cnt;   /* (each answer more changed) */
    for (i = 0; i < n; i++) kind[i] = VK_NOTE;
    if (amount) vary_round(tess_an.mut[tr], kind, n, amount, 50u, rnd);
    else memset(tess_an.mut[tr], 0, TESS_AN_NOTES);
    for (i = 0; i < n; i++) ord[i] = (uint8_t)(a->order == TESS_AN_REVERSE ? n - 1u - i : i);
    if (a->order == TESS_AN_SHUFFLE)
        for (i = n; i > 1u; i--) {
            uint32_t j = gen_rnd(rnd, i);
            uint8_t x = ord[i - 1u];
            ord[i - 1u] = ord[j], ord[j] = x;
        }
    tess_an.shift[tr] = (int8_t)(a->shift ? (int32_t)gen_rnd(rnd, 2u * a->shift + 1u) - (int32_t)a->shift : 0);
}

/* a sound into out: the first the step's note, two more besides it */
static void answer_emit(gen_out_t *out, uint32_t note, uint32_t vel)
{
    if (!out->gate) {
        out->note = (uint8_t)note, out->vel = (uint8_t)(vel ? vel : 1u), out->gate = 1;
    } else if (out->more_n < 2u && note != out->note) {
        out->more[out->more_n++] = (uint8_t)note;
    }
}

/* place i of the phrase as this answer plays it: its note (changed, shifted) and velocity; 0 a rest */
static uint32_t answer_place(const track_t *t, uint32_t tr, uint32_t i, uint32_t *vel)
{
    const tessera_answer_t *a = &tessera.answer[tr];
    uint32_t n = a->n, m = tess_an.mut[tr][i], src = vary_src(m), k = i, note;
    uint8_t kind[TESS_AN_NOTES];
    if (vary_dropped(m)) return 0;
    if (src != VARY_OWN) {                              /* (a SWAP's halves: the next or the last place's note) */
        for (uint32_t j = 0; j < n; j++) kind[j] = VK_NOTE;
        k = vary_find(kind, n, i, src == VARY_NEXT ? 1 : -1);
        if (k >= n) k = i;
    }
    note = sv_degrees(t, a->note[tess_an.ord[tr][k] % n], vary_move(m) + tess_an.shift[tr]);
    note = (uint32_t)((int32_t)note + 12 * vary_oct(m));
    *vel = a->vel[tess_an.ord[tr][k] % n];
    return note > 127u ? 0u : note + 1u;
}

/* ECHO: the note back once, at half its velocity, 3 to 8 steps later; one in four an octave up */
static void answer_echo(uint32_t tr, uint32_t note, uint32_t vel)
{
    const tessera_answer_t *a = &tessera.answer[tr];
    uint32_t *rnd = &tess_an.rnd[tr];
    if (!a->echo || gen_rnd(rnd, 100u) >= a->echo) return;
    for (uint32_t e = 0; e < TESS_AN_ECHOES; e++) {
        if (tess_an.e_left[tr][e]) continue;
        if (!gen_rnd(rnd, 4u) && note + 12u <= 127u) note += 12u;
        tess_an.e_note[tr][e] = (uint8_t)note;
        tess_an.e_vel[tr][e] = (uint8_t)(vel / 2u ? vel / 2u : 1u);
        tess_an.e_left[tr][e] = (uint8_t)(3u + gen_rnd(rnd, 6u));
        return;
    }
}

/* place i plays now: its note into out (velocity scaled by sc / 256), maybe an echo */
static void answer_play(const track_t *t, uint32_t tr, uint32_t i, uint32_t sc, gen_out_t *out)
{
    uint32_t vel = 0, n = answer_place(t, tr, i, &vel);
    tess_an.slot[tr] = (uint8_t)(i + 1u);
    if (!n) return;
    vel = vel * sc / 256u;
    answer_emit(out, n - 1u, vel);
    answer_echo(tr, n - 1u, vel);
}

static void answer_tick(const track_t *t, uint32_t tr, const gen_clock_t *clk, gen_out_t *out)
{
    const tessera_answer_t *a = &tessera.answer[tr];
    uint32_t spb = clk->steps_per_bar ? clk->steps_per_bar : 16u, n = a->n, cnt = tessera_answer_count(tr), i;
    uint32_t c = tess_an.cnt[tr];
    for (uint32_t e = 0; e < TESS_AN_ECHOES; e++)       /* the echoes waiting: one step nearer */
        if (tess_an.e_left[tr][e] && !--tess_an.e_left[tr][e])
            answer_emit(out, tess_an.e_note[tr][e], tess_an.e_vel[tr][e]);
    out->cv = (int16_t)(n ? (uint32_t)tess_an.slot[tr] * 32767u / n : 0u);
    switch (tess_an.phase[tr]) {
    case TESS_AN_HEAR:                                  /* listening: a bar with no key closes the phrase */
        if (tess_an.hclk[tr] < 0xFFFFu) tess_an.hclk[tr]++;
        if (++tess_an.cnt[tr] >= spb) {
            tess_an.phase[tr] = n ? TESS_AN_GAP : TESS_AN_IDLE;
            tess_an.cnt[tr] = 0;
            tess_an.ans[tr] = 0;
        }
        return;
    case TESS_AN_GAP:                                   /* SPACE: 0 .. 4 bars before an answer */
        if (!n) { tess_an.phase[tr] = TESS_AN_IDLE; return; }
        if (c < (uint32_t)a->space * spb * 4u / 100u) {
            tess_an.cnt[tr] = c + 1u;
            return;
        }
        tess_an.phase[tr] = cnt ? TESS_AN_PLAY : TESS_AN_LOOP;
        tess_an.cnt[tr] = c = 0;
        answer_round(tr);
        if (!cnt) break;
        /* fall through */
    case TESS_AN_PLAY: {                                /* an answer: the phrase's rhythm, softer each time */
        uint32_t span = n ? (uint32_t)a->at[n - 1u] + 1u : 1u, sc = cnt ? 256u * (cnt - tess_an.ans[tr]) / cnt : 256u;
        if (!n) { tess_an.phase[tr] = TESS_AN_IDLE; return; }
        for (i = 0; i < n; i++)
            if (a->at[i] == c) answer_play(t, tr, i, sc, out);
        tess_an.cnt[tr] = ++c;
        if (c < span) return;
        tess_an.cnt[tr] = 0;
        if (!cnt) {                                     /* (FADE turned to the top: on as the loops) */
            tess_an.phase[tr] = TESS_AN_LOOP;
            return;
        }
        if (++tess_an.ans[tr] >= cnt) {                 /* forgotten: rest until the keys play again */
            tess_an.phase[tr] = TESS_AN_IDLE;
            tess_an.slot[tr] = 0;
            return;
        }
        tess_an.phase[tr] = TESS_AN_GAP;
        return; }
    default:
        break;
    }
    if (tess_an.phase[tr] != TESS_AN_LOOP) return;
    if (!n || cnt) {                                    /* (FADE turned down: answers again, after a gap) */
        tess_an.phase[tr] = n ? TESS_AN_GAP : TESS_AN_IDLE;
        tess_an.cnt[tr] = 0;
        return;
    }
    {   /* the four loops: place i on loop i % 4, sounding every LEN of it from its own step; a new round each turn of
         * the first loop */
        const uint8_t *L = AN_LOOPS[a->len % TESS_AN_LENS];
        if (c && !(c % L[0])) answer_round(tr);
        for (i = 0; i < n; i++)
            if (c >= a->at[i] && !((c - a->at[i]) % L[i & 3u])) answer_play(t, tr, i, 256u, out);
        tess_an.cnt[tr] = c + 1u < 0x7FFFFFFFu ? c + 1u : 0u;
    }
}

/* OCT+ held on GEN in ANSWER: KNOB 1 ORDER, 2 SHIFT, 3 SPACE, 4 LEN */
void tessera_answer_layer_knob(uint32_t k, int32_t s)
{
    uint32_t tr = song.sel;
    const tessera_answer_t *a;
    int32_t d = s > 0 ? 1 : -1;
    tessera_ensure_init();
    if (tr >= TESSERA_TRACKS || !s) return;
    a = &tessera.answer[tr];
    TESS_IRQ_OFF();
    if (k == 0u) tess_param_set_body(tr, TX_AN_ORDER, (int32_t)a->order + d);
    else if (k == 1u) tess_param_set_body(tr, TX_AN_SHIFT, (int32_t)a->shift + d);
    else if (k == 2u) tess_param_set_body(tr, TX_AN_SPACE, (int32_t)a->space + s * 2);
    else if (k == 3u) tess_param_set_body(tr, TX_AN_LEN, (int32_t)a->len + d);
    TESS_IRQ_ON();
}
