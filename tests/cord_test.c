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
#include <sys/wait.h>
#include <unistd.h>
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

    /* polarity: track 1's LFO (POL BI) at its bottom, +50 % on track 3's LEVEL; BI pulls it down, UNI (0) leaves it */
    {
        uint32_t lo, uni, top;
        uint8_t pk[CORD_PK_BYTES];
        cord_clear();
        trk[0].p[P_LPOL] = 0;
        trk[0].lfo_val = -32767;
        trk[2].p[P_LEVEL] = 50;
        c = C(CS_LFO, 0, CD_LEVEL, 2, 50, 0);
        cord_set(0, &c);
        mod_begin(&trk[2]); lo = (uint32_t)trk[2].p[P_LEVEL]; mod_end(&trk[2]);
        c.uni = 1;
        cord_set(0, &c);
        mod_begin(&trk[2]); uni = (uint32_t)trk[2].p[P_LEVEL]; mod_end(&trk[2]);
        trk[0].lfo_val = 32767;
        mod_begin(&trk[2]); top = (uint32_t)trk[2].p[P_LEVEL]; mod_end(&trk[2]);
        check("UNI: a bipolar source at its bottom moves nothing (BI: down), at its top up as BI",
              lo < 50u && uni == 50u && top > 50u);
        trk[0].lfo_fade = 0;                       /* (FADE at its start: the LFO not in yet) */
        mod_begin(&trk[2]); uni = (uint32_t)trk[2].p[P_LEVEL]; mod_end(&trk[2]);
        trk[0].lfo_fade = 16384;
        trk[0].lfo_val = -32767;
        mod_begin(&trk[2]); lo = (uint32_t)trk[2].p[P_LEVEL]; mod_end(&trk[2]);
        trk[0].lfo_fade = 32767;
        check("...UNI with the LFO's FADE at 0: nothing moved; half faded in, at its bottom: nothing (folded before the "
              "FADE, as POL UNI)", uni == 50u && lo == 50u);
        cord_pack(pk);
        cord_clear();
        check("...saved and loaded: UNI kept (src_trk's top bit), the track as it was",
              cord_unpack(pk) && cord_get(0)->uni == 1u && cord_get(0)->src_trk == 0u);
        pk[1] &= 0x7Fu;
        check("...a mod saved before UNI (the bit 0): BI", cord_unpack(pk) && cord_get(0)->uni == 0u);
        trk[0].p[P_LPOL] = 1;                         /* the LFO's own POL UNI: the mod's BI/UNI still decides */
        trk[0].lfo_val = 0;                           /* (POL UNI's bottom) */
        c.uni = 0;
        cord_set(0, &c);
        mod_begin(&trk[2]); lo = (uint32_t)trk[2].p[P_LEVEL]; mod_end(&trk[2]);
        c.uni = 1;
        cord_set(0, &c);
        mod_begin(&trk[2]); uni = (uint32_t)trk[2].p[P_LEVEL]; mod_end(&trk[2]);
        trk[0].lfo_val = 32767;
        mod_begin(&trk[2]); top = (uint32_t)trk[2].p[P_LEVEL]; mod_end(&trk[2]);
        check("...the LFO's POL UNI: BI still pulls down at its bottom, UNI leaves it, its top up",
              lo < 50u && uni == 50u && top > 50u);
        trk[0].p[P_LPOL] = 0;
        c.uni = 2;
        check("...a polarity past UNI: refused", !cord_set(1, &c));
        cord_clear();
        trk[2].p[P_LEVEL] = 100;
    }

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

    /* the generators' CV: Euclid's ramp over its cycle, as gen.c makes it */
    host_tracks_init();
    tessera_reset();
    tessera_param_set(0, TX_MODE, TESS_ENG_EUCLID);
    tessera_param_set(0, TX_EUC_STEPS, 4);
    {
        int32_t seen[4];
        for (i = 0; i < 4u; i++) {
            tessera_clock_step(&trk[0]);
            seen[i] = cord_src(CS_CV, 0);
        }
        check("CV: Euclid's ramp over its 4 steps (0, 1/3, 2/3, all), as the cords read it",
              seen[0] == 0 && seen[1] == 10922 && seen[2] == 21844 && seen[3] == 32767);
    }
    check("CV, GATE, NOTE of a track without a generator: 0", !cord_src(CS_CV, 1) && !cord_src(CS_GATE, 1) &&
          !cord_src(CS_NOTE, 1));
    tessera_stop_all();

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

/* MOD's routes into cords (cord_adopt): the same move, the sound's go with it, the project's stay, no room: left */
static void fresh_tracks(void)
{
    uint32_t i;
    memset(trk, 0, sizeof trk);                         /* (as fx_performance: every voice, LFO and effect from rest) */
    memset(&song, 0, sizeof song);
    memset(fx_state, 0, sizeof fx_state);
    spread_on = 0;
    host_tracks_init();
    tessera_reset();
    cord_clear();
    for (i = 0; i < 4u; i++) host_power_on_sound(&trk[i], i);
    mod_seed = 0x2545F491u;
}
static void route(uint32_t tr, uint32_t k, int32_t s, int32_t d, int32_t a)
{
    trk[tr].p[P_M1SRC + 3u * k] = (int16_t)s;
    trk[tr].p[P_M1DST + 3u * k] = (int16_t)d;
    trk[tr].p[P_M1AMT + 3u * k] = (int16_t)a;
}
static int routes_off(uint32_t tr)
{
    uint32_t k;
    for (k = 0; k < 12u; k++)
        if (trk[tr].p[P_M1SRC + k]) return 0;
    return 1;
}
/* track 1 a chord of three velocities (each voice its own VEL, KEY, ENV), the others one note */
static double render_chord(uint32_t seconds)
{
    static const uint8_t N[3] = {48, 55, 64}, V[3] = {30, 80, 127};
    uint32_t f, i;
    double sum = 0;
    for (i = 0; i < 3u; i++) trk_note_on(&trk[1], N[i], V[i]);
    trk_note_on(&trk[0], 60, 100);
    for (f = 0; f < seconds * FS; f += CTL) {
        int32_t o[2 * CTL];
        mix_block(o, CTL);
        for (i = 0; i < 2u * CTL; i++) sum += (double)o[i] * (double)(1u + (f / CTL + i) % 7u);   /* (the order counts) */
    }
    for (i = 0; i < 3u; i++) trk_note_off(&trk[1], N[i]);
    trk_note_off(&trk[0], 60);
    return sum;
}
/* a render from the same start every time: in a child process (the engines keep state of their own across
 * renders); adopt: the routes into cords first. Its sum, or -1 */
static double render_fresh(uint32_t tr, uint32_t s, uint32_t d, int32_t a, int adopt, const cord_t *cord)
{
    int fd[2];
    double r = -1;
    pid_t child;
    if (pipe(fd)) return -1;
    child = fork();
    if (!child) {
        fresh_tracks();
        if (s) route(tr, 2, (int32_t)s, (int32_t)d, a);
        if (adopt && (cord_adopt() || !routes_off(tr) || cord_count() != 1u)) r = -2;
        if (cord) cord_set(0, cord);
        if (r != -2) r = render_chord(1);
        if (write(fd[1], &r, sizeof r) != sizeof r) _exit(1);
        _exit(0);
    }
    close(fd[1]);
    if (read(fd[0], &r, sizeof r) != sizeof r) r = -1;
    close(fd[0]);
    waitpid(child, 0, 0);
    return r;
}
static int adoption(void)
{
    static const struct { uint8_t s, d; int8_t a; const char *what; } R[] = {
        {MS_VEL, MD_CUT, 40, "VEL -> CUTOFF"}, {MS_KEY, MD_PITCH, -23, "KEY -> PITCH"}, {MS_ENV, MD_AMP, 50, "ENV -> GAIN"},
        {MS_RAND, MD_SHP, 63, "RAND -> SHAPE"}, {MS_LFO, MD_CUT, -64, "LFO -> CUTOFF"}, {MS_MODW, MD_DEPTH, 30, "MODW -> DEPTH"},
        {MS_ENV, MD_DEPTH, -40, "ENV -> DEPTH"}, {MS_LFO, MD_PAN, 37, "LFO -> PAN"}, {MS_ENV, MD_RATE, 21, "ENV -> RATE"},
        {MS_LFO, MD_VIB, 45, "LFO -> VIB"}, {MS_VEL, MD_E1 + 4, -50, "VEL -> E5"}};
    int fails = bad, ok;
    int32_t a;
    uint32_t i, n;
    cord_t c;
    double was, now;
    char what[160];

    for (a = -64, ok = 1; a <= 63; a++) {               /* every AMT: the cord's percentage moves by it again */
        if (!a) continue;
        cord_clear();
        route(0, 0, MS_VEL, MD_AMP, a);
        if (cord_adopt() || !routes_off(0) || cord_get(0)->src != MS_VEL || cord_get(0)->dst != CD_GAIN ||
            cord_amt(cord_get(0)->amount) != a || cord_get(0)->src_trk || cord_get(0)->dst_trk) ok = 0;
    }
    check("MOD's route -> a cord on its track, for every AMT (-64..63) the same amount again (cord_amt)", ok);
    was = render_fresh(0, 0, 0, 0, 0, 0);
    check("(two renders from the same start: the same)", was > 0 && was == render_fresh(0, 0, 0, 0, 0, 0));
    for (i = 0; i < sizeof R / sizeof R[0]; i++) {      /* the sound: a route and its cord, sample for sample */
        was = render_fresh(1, R[i].s, R[i].d, R[i].a, 0, 0);
        now = render_fresh(1, R[i].s, R[i].d, R[i].a, 1, 0);
        snprintf(what, sizeof what, "%s on a chord: the cord sounds as the route did, bit for bit", R[i].what);
        check(what, was > 0 && was == now);
    }
    c = C(CS_VEL, 1, CD_CUTOFF, 1, 60, 0);
    was = render_fresh(0, 0, 0, 0, 0, &c);
    c = C(CS_VEL, 0, CD_CUTOFF, 1, 60, 0);
    now = render_fresh(0, 0, 0, 0, 0, &c);
    check("...VEL on its own track's chord: each voice its own (another track's VEL: its latest, not the same)",
          was > 0 && now > 0 && was != now && was != render_fresh(0, 0, 0, 0, 0, 0));
    fresh_tracks();
    route(2, 0, MS_LFO, MD_PAN, 20);
    route(2, 1, MS_LFO, 0, 30);                         /* (no destination: moves nothing) */
    route(2, 3, MS_VEL, MD_RESERVED7, 30);
    n = cord_adopt();
    check("a project's routes (just loaded) -> cords; ones that move nothing just go", !n && routes_off(2) &&
          cord_count() == 1u && cord_get(0)->dst == CD_PAN);
    cord_sound_load(2);
    (void)cord_adopt();
    check("...another sound on the track: the project's cord stays", cord_count() == 1u);
    route(2, 0, MS_VEL, MD_CUT, 20);                    /* a sound with a route, loaded */
    route(2, 1, MS_ENV, MD_E1 + 2, -10);
    (void)cord_adopt();
    check("a sound's routes -> cords (the next free: 2 and 3)", cord_count() == 3u && cord_get(1)->dst == CD_CUTOFF &&
          cord_get(2)->dst == CD_E3 && cord_get(2)->amount == -16 && routes_off(2));
    c = *cord_get(2);
    c.amount = -50;
    cord_set(2, &c);                                    /* (one edited: the project's now) */
    cord_sound_load(2);
    (void)cord_adopt();
    check("...another sound loaded: the sound's cord goes, the one edited stays", cord_count() == 2u &&
          !cord_get(1)->dst && cord_get(2)->dst == CD_E3 && cord_get(2)->amount == -50);
    route(2, 0, MS_LFO, MD_PAN, 20);                    /* (the same as cord 1: an undo, the same sound again) */
    (void)cord_adopt();
    check("a route there is a cord for already: no second one", cord_count() == 2u && routes_off(2));

    cord_clear();
    for (i = 0; i < CORD_LIVE; i++) {
        c = C(CS_LFO, i % 4u, CD_LEVEL, i % 4u, 10 + (int32_t)i, 0);
        cord_set(i, &c);
    }
    route(3, 1, MS_AT, MD_CUT, 33);
    n = cord_adopt();
    check("sixteen cords in use: the route left, and still there (it plays as before)", n == 1u &&
          trk[3].p[P_M2SRC] == MS_AT && trk[3].p[P_M2AMT] == 33);
    c = C(0, 0, 0, 0, 0, 0);
    cord_set(9, &c);
    n = cord_adopt();
    check("...a cord freed: the route takes it", !n && routes_off(3) && cord_get(9)->src == CS_AT &&
          cord_get(9)->src_trk == 3u && cord_get(9)->dst == CD_CUTOFF);
    cord_clear();
    return bad - fails;
}

/* DRIFT and the default patch (phase 3) */
static int drift_and_defaults(void)
{
    int fails = bad, smooth = 1, bounded = 1;
    int32_t lo[NTRK], hi[NTRK], prev[NTRK];
    uint32_t b, tr, apart = 0, saved = cord_blocks;
    double d;
    for (tr = 0; tr < NTRK; tr++) lo[tr] = 32767, hi[tr] = -32767, prev[tr] = cord_drift(tr);
    for (b = 0; b < 1378u * 120u; b++) {            /* two minutes of blocks */
        cord_tick();
        for (tr = 0; tr < NTRK; tr++) {
            int32_t x = cord_drift(tr);
            if (x < -32767 || x > 32767) bounded = 0;
            if ((x > prev[tr] ? x - prev[tr] : prev[tr] - x) > 16) smooth = 0;   /* (a point 12 s on: a few a block) */
            if (x < lo[tr]) lo[tr] = x;
            if (x > hi[tr]) hi[tr] = x;
            prev[tr] = x;
        }
        if (cord_drift(0) != cord_drift(1)) apart++;
    }
    check("DRIFT: within -1..1, and smooth (at most 16 / 32768 a block: no step to hear)", bounded && smooth);
    for (tr = 0, b = 1; tr < NTRK; tr++)
        if (hi[tr] - lo[tr] < 16384) b = 0;
    check("...it wanders: over two minutes each track's covers at least half its range", b);
    check("...each track its own", apart > 1378u * 100u);
    cord_blocks = saved;

    fresh_tracks();
    {
        cord_t c = {CS_DRIFT, 0, CD_CUTOFF, 0, 12, 0};
        cord_set(0, &c);
    }
    cord_defaults();
    check("a new project's mods: none (automation is opt-in), all 16 free", cord_count() == 0u);
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
    (void)adoption();
    (void)drift_and_defaults();

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
