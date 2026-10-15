/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* Cords (firmware/src/cord.c):
 *   build/host/cord_test tests/cord_dests.txt
 * The destination table: each code at its own place; names short (a cord's chip) and unique; a per-voice or depth
 * destination on one of mod.c's; a parameter destination on a real per-track id (pid.c) with a range, never an
 * action or the device's, and none twice. Then the codes against tests/cord_dests.txt: FROZEN (saved cords keep
 * them), any change fails. Rewrite (CORD_UPDATE=1) only to append. */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int bad;
static void check(const char *what, int ok)
{
    printf("cord: %-95s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

static const char *const KIND[] = {"voice", "block", "depth", "step"};

static cord_t C(uint32_t src, uint32_t st, uint32_t dst, uint32_t dt, int32_t amount, int32_t offset)
{
    cord_t c = {(uint8_t)src, (uint8_t)st, (uint8_t)dst, (uint8_t)dt, (int8_t)amount, (int8_t)offset};
    return c;
}

/* a few seconds of all four tracks playing, every block's peak; 0: something not finite or past full scale */
static double render(uint32_t seconds)
{
    uint32_t f, i, k;
    double sum = 0;
    for (k = 0; k < 4u; k++)
        trk_note_on(&trk[k], 48u + 7u * k, 100);
    for (f = 0; f < seconds * FS; f += CTL) {
        int32_t o[2 * CTL];
        mix_block(o, CTL);
        for (i = 0; i < 2u * CTL; i++) {
            if (o[i] > 0x7FFFFFFF / 2 || o[i] < -0x7FFFFFFF / 2) return -1;
            sum += o[i] < 0 ? -(double)o[i] : (double)o[i];
        }
    }
    for (k = 0; k < 4u; k++)
        trk_note_off(&trk[k], 48u + 7u * k);
    return sum;
}

static int behaviour(void)
{
    int fails = bad;
    cord_t c;
    uint32_t i;
    double dry, wet;
    uint64_t t0, t_dry, t_wet;

    host_tracks_init();
    tessera_reset();
    cord_clear();
    check("none set: nothing to do, and mod_begin stays off for a track without MOD slots",
          cord_count() == 0 && !mod_begin(&trk[1]));
    c = C(CS_OFF, 0, CD_PAN, 1, 0, 50);
    check("refused: past CORD_LIVE, an unknown destination, an amount past 100 %, a track past 4",
          !cord_set(CORD_LIVE, &c) && !(c.dst = CD_N, cord_set(0, &c)) && !(c = C(CS_LFO, 0, CD_PAN, 1, 101, 0), cord_set(0, &c)) &&
          !(c = C(CS_LFO, 4, CD_PAN, 1, 50, 0), cord_set(0, &c)) && cord_count() == 0);

    /* a parameter, in place for the block, back after it */
    c = C(CS_OFF, 0, CD_PAN, 1, 0, 50);
    cord_set(0, &c);
    trk[1].p[P_PAN] = 0;
    mod_begin(&trk[1]);
    i = (uint32_t)trk[1].p[P_PAN];
    mod_end(&trk[1]);
    check("PAN +50 % (no source, the offset): half its range up for the block, back after it",
          i == 63u && trk[1].p[P_PAN] == 0 && cord_count() == 1);   /* (+64 of -64..63: at its top) */

    /* across tracks: track 1's LFO on track 3's LEVEL */
    cord_clear();
    c = C(CS_LFO, 0, CD_LEVEL, 2, -100, 0);
    cord_set(3, &c);
    trk[0].lfo_val = 32767; trk[0].lfo_fade = 32767;
    trk[2].p[P_LEVEL] = 100;
    mod_begin(&trk[2]);
    i = (uint32_t)trk[2].p[P_LEVEL];
    mod_end(&trk[2]);
    check("across tracks: track 1's LFO at its top, -100 % on track 3's LEVEL: down to 0, back to 100",
          i == 0u && trk[2].p[P_LEVEL] == 100);
    check("...and only track 3: track 1 and 2 untouched", !mod_begin(&trk[0]) && !mod_begin(&trk[1]));

    /* per voice: the pitch term, as a MOD slot's */
    cord_clear();
    c = C(CS_OFF, 0, CD_PITCH, 0, 0, 100);
    cord_set(0, &c);
    mod_begin(&trk[0]);
    i = (uint32_t)mod.pit;
    mod_end(&trk[0]);
    check("PITCH +100 %: an octave on every voice (mod.c's units, 16 a semitone)", i == 192u);

    cord_clear();
    c = C(CS_OFF, 0, CD_LFO_DEPTH, 0, 0, -100);
    cord_set(0, &c);
    mod_begin(&trk[0]);
    i = (uint32_t)mod.ldep;
    mod_end(&trk[0]);
    check("LFO DEPTH -100 %: the LFO closed (its gain 0)", i == 0u);

    /* a Tessera setting around the step */
    cord_clear();
    tessera_param_set(0, TX_MODE, TESS_ENG_EUCLID);
    tessera_param_set(0, TX_EUC_STEPS, 16);
    tessera_param_set(0, TX_EUC_PULSES, 4);
    c = C(CS_OFF, 0, CD_EUC_PULSES, 0, 0, 25);
    cord_set(0, &c);
    cord_step_begin(0);
    i = tessera.euclid[0].pulses;
    cord_step_end(0);
    check("EUCLID PULSES +25 % (of 0..32): 4 -> 12 for the step, 4 again after it", i == 12u && tessera.euclid[0].pulses == 4u);
    cord_step_begin(0);
    tessera.euclid[0].pulses = 13;                      /* (as an EVOLVE mutation in the step: one more) */
    cord_step_end(0);
    check("...a mutation in the step kept: one more hit stays one more (5)", tessera.euclid[0].pulses == 5u);
    tessera.euclid[0].pulses = 4;
    c = C(CS_OFF, 0, CD_EUC_PULSES, 0, 0, 100);
    cord_set(0, &c);
    cord_step_begin(0);
    i = tessera.euclid[0].pulses;
    cord_step_end(0);
    check("...past the range: PULSES at most STEPS (16), back to 4", i == 16u && tessera.euclid[0].pulses == 4u);

    /* sixteen at once, every kind, all four tracks: it plays, nothing breaks, and it is not the dry sound */
    host_tracks_init();
    tessera_reset();
    cord_clear();
    for (i = 0; i < 4u; i++) host_power_on_sound(&trk[i], i);
    t0 = now_ns();
    dry = render(2);
    t_dry = now_ns() - t0;
    {
        static const uint8_t D[16] = {CD_PITCH, CD_CUTOFF, CD_SHAPE, CD_GAIN, CD_LEVEL, CD_PAN, CD_SPREAD, CD_ATTACK,
                                      CD_ENV_FLT, CD_LFO_RATE, CD_LFO_DEPTH, CD_LFO_FLT, CD_FX_SEND, CD_E1, CD_E2, CD_TW_DROP};
        static const uint8_t S[16] = {CS_LFO, CS_ENV, CS_VEL, CS_KEY, CS_RAND, CS_MODW, CS_AT, CS_EXPR, CS_SH, CS_SLEW,
                                      CS_GATE, CS_NOTE, CS_LFO, CS_ENV, CS_SH, CS_GATE};
        for (i = 0; i < 16u; i++) {
            c = C(S[i], i % 4u, D[i], (i + 1u) % 4u, (int32_t)(i * 13u % 201u) - 100, (int32_t)(i * 7u % 41u) - 20);
            cord_set(i, &c);
        }
    }
    check("sixteen cords set, every kind of destination", cord_count() == 16u);
    host_tracks_init();
    for (i = 0; i < 4u; i++) host_power_on_sound(&trk[i], i);
    t0 = now_ns();
    wet = render(2);
    t_wet = now_ns() - t0;
    printf("cord: host cost, four tracks playing: %.1f %% more with sixteen cords (a report, no limit: the device's "
           "cost is measured there)\n", t_dry ? 100.0 * ((double)t_wet - (double)t_dry) / (double)t_dry : 0.0);
    check("...two seconds of four tracks: plays, never past full scale, and differs from no cords",
          dry > 0 && wet > 0 && wet != dry);
    for (i = 0; i < NTRK; i++)
        if (trk[i].p[P_PAN] != TP[P_PAN].def || trk[i].p[P_LEVEL] < 0) wet = -1;
    check("...after it every stored value as it was (PAN, LEVEL: no block's modulation left in)", wet > 0);
    cord_clear();
    return bad - fails;
}

int main(int argc, char **argv)
{
    const char *golden = argc > 1 ? argv[1] : "tests/cord_dests.txt";
    int update = getenv("CORD_UPDATE") && atoi(getenv("CORD_UPDATE"));
    uint32_t c, k, placed = 1, names = 1, mods = 1, params = 1, twice = 0;
    char what[160];

    host_tracks_init();
    tessera_reset();
    for (c = 0; c < CD_N; c++) {
        const cord_dest_t *d = cord_dest(c);
        if (!d || d->code != c) { placed = 0; continue; }
        if (!d->name[0] || strlen(d->name) > 7u || !d->full[0]) names = 0;
        for (k = 0; k < c; k++)
            if (cord_dest(k) && !strcmp(cord_dest(k)->name, d->name)) names = 0;
        if (c == CD_OFF) continue;
        if (d->kind == CK_VOICE || d->kind == CK_DEPTH) {
            if (d->pid != PID_NONE || !d->md || d->md >= MD_N || (d->md >= MD_RESERVED6 && d->md <= MD_RESERVED9) ||
                (d->kind == CK_VOICE) != (d->md <= MD_AMP) || (d->kind == CK_DEPTH) != (d->md == MD_DEPTH))
                mods = 0;
        } else {
            const pid_entry_t *e = pid_find(d->pid);
            const param_desc_t *pd = pid_desc(d->pid, 0);
            if (!e || !(e->flags & PID_F_TRACK) || (e->flags & (PID_F_ACTION | PID_F_DEVICE)) || !pd || (pd->max <= pd->min && !(d->pid >= PID_TRACK(P_E0) && d->pid <= PID_TRACK(P_E7))) ||   /* (E1..E8: the engine's) */
               
                (d->kind == CK_STEP) != (d->pid >= PID_TESS(0))) {
                params = 0;
                printf("    0x%03x %s\n", d->pid, d->name);
            }
            for (k = 1; k < c; k++)
                twice += cord_dest(k) && cord_dest(k)->pid == d->pid;
        }
    }
    check("each destination at its own code, none missing", placed);
    check("names: at most 7 characters (a cord's chip), unique; every one explained", names);
    check("per voice and LFO depth: on mod.c's own destinations (MD_PITCH .. MD_AMP, MD_DEPTH)", mods);
    check("the others: a per-track parameter with a range (pid.c; E1..E8 the engine's), not an action nor the device's", params);
    check("no parameter reached by two destinations", !twice);
    check("past the end: nothing", !cord_dest(CD_N) && !cord_dest(255));
    snprintf(what, sizeof what, "the limits: room for %u cords, %u in use (Theo 2026-10-16)", CORD_MAX, CORD_LIVE);
    check(what, CORD_MAX == 32u && CORD_LIVE == 16u);

    (void)behaviour();

    {
        FILE *f = fopen(golden, update ? "w" : "r");
        char want[160], got[160];
        uint32_t diffs = 0;
        if (!f) { check("tests/cord_dests.txt opens", 0); return 1; }
        if (update)
            fprintf(f, "# Cord destinations (firmware/src/cord.c): code, name, kind, parameter. FROZEN: saved cords keep the\n"
                       "# codes. Rewrite (CORD_UPDATE=1) only to append.\n");
        else
            while (fgets(want, sizeof want, f) && want[0] == '#') {}
        for (c = 0; c < CD_N; c++) {
            const cord_dest_t *d = cord_dest(c);
            snprintf(got, sizeof got, "%2u %-8s %-5s %s", c, d ? d->name : "?", d ? KIND[d->kind] : "?",
                     d && d->pid != PID_NONE ? pid_find(d->pid)->name : "-");
            if (update) { fprintf(f, "%s\n", got); continue; }
            if (c && !fgets(want, sizeof want, f)) want[0] = 0;
            want[strcspn(want, "\n")] = 0;
            if (strcmp(want, got) && diffs++ < 8) printf("    want: %s\n    got:  %s\n", want, got);
        }
        if (!update && fgets(want, sizeof want, f)) diffs++;
        fclose(f);
        if (update) printf("cord: wrote %u destinations to %s\n", (unsigned)CD_N, golden);
        else check("the destination codes as tests/cord_dests.txt froze them", !diffs);
    }
    printf("cord: %u destinations\n", (unsigned)CD_N);
    if (bad) printf("cord: %d FAILED\n", bad);
    else printf("cord: all passed\n");
    return bad != 0;
}
