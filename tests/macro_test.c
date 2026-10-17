/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* MACRO (eng_macro.c, Plaits' synthesis): built with -DMELODEE_PLAITS=1 and linked with Plaits compiled for this
 * computer (run_tests.sh macro_tests, g++). Every model plays, finite and within full scale; a held VA note at its
 * pitch; HIT ON strikes and decays, OFF holds; MODEL, HARM .. AUX reach the sound; its memory from the audio arena;
 * the cost against SID's on this computer (a report). */
#if !MELODEE_PLAITS
#error "macro_test: build it with -DMELODEE_PLAITS=1"
#endif
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <time.h>

static int bad;
static void check(const char *what, int ok)
{
    printf("macro: %-76s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static double now_s(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }

/* track 0 alone (the others silent): seconds of it, note held for hold of them; peak, rms, zero crossings a second
 * in the held part, the rms of its last quarter; 0: not finite / past full scale */
static int play(uint32_t note, double secs, double hold, double *peak, double *rms, double *zc, double *tail)
{
    int32_t o[2 * CTL];
    uint32_t f, i, n = (uint32_t)(secs * FS), nh = (uint32_t)(hold * FS), cross = 0, ncount = 0;
    double sum = 0, tsum = 0, last = 0;
    uint32_t tn = 0;
    *peak = 0;
    trk_note_on(&trk[0], note, 100);
    for (f = 0; f < n; f += CTL) {
        if (f >= nh && f < nh + CTL) trk_note_off(&trk[0], note);
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++) {
            double x = o[2 * i];
            if (x != x || x > 2147483647.0 || x < -2147483647.0) return 0;
            if (x > *peak) *peak = x;
            if (-x > *peak) *peak = -x;
            sum += x * x;
            if (f > FS / 4u && f + i < nh) {          /* (after the start, while held) */
                if ((x >= 0) != (last >= 0)) cross++;
                ncount++;
            }
            if (f + i >= n - n / 4u) { tsum += x * x; tn++; }
            last = x;
        }
    }
    trk_note_off(&trk[0], note);
    *rms = sqrt(sum / n);
    *zc = ncount ? cross * (double)FS / ncount / 2.0 : 0;
    *tail = tn ? sqrt(tsum / tn) : 0;
    return 1;
}

static void sound(uint32_t preset)
{
    uint32_t k;
    host_tracks_init();
    for (k = 1; k < NPART; k++) trk[k].p[P_LEVEL] = 0;
    host_preset(&trk[0], ENGI_MACRO, preset);
    trk[0].engine = trk[0].eng_req;
}

int main(void)
{
    uint32_t k, ok = 1, models;
    double peak, rms, zc, tail, base;
    check("MACRO offered, at engine 21", eng_ok(ENGI_MACRO) && ENGINES[ENGI_MACRO]->npresets > 0);
    check("its memory: the voice and its 16 KB working buffer, from the audio arena", mc_bytes() > 16384u &&
          mc_bytes() < 40000u);

    models = (uint32_t)ENGINES[ENGI_MACRO]->edit[0].max + 1u;
    for (k = 0, ok = 1; k < sizeof MC_MODEL; k++)      /* (Plaits' 6, 7, 14: string machine, chiptune, chords) */
        ok &= MC_MODEL[k] != 6u && MC_MODEL[k] != 7u && MC_MODEL[k] != 14u;
    check("no chord model: each note plays one pitch (2026-10-17)", ok && models == sizeof MC_MODEL);
    for (k = 0; k < models; k++) {                    /* every model, from INIT VA's settings, struck and held */
        sound(0);
        trk[0].p[P_E0] = (int16_t)k;
        trk[0].p[P_E6] = 1;
        if (!play(48, 1.0, 0.5, &peak, &rms, &zc, &tail) || rms < 50.0 || peak > 0.95 * 2147483647.0) {
            ok = 0;
            printf("    model %u (%s): peak %.0f rms %.0f\n", k, ENGINES[ENGI_MACRO]->edit[0].names[k], peak, rms);
        }
    }
    check("every model plays: sound, finite, within full scale", ok);

    sound(0);                                         /* INIT VA: HIT OFF, held: a tone at its pitch */
    ok = play(57, 1.5, 1.4, &peak, &rms, &zc, &tail);
    printf("    INIT VA, A3 held: %.1f crossings a second (220 Hz wanted)\n", zc);
    check("a held VA note at its pitch (A3: 220 Hz within 3 %)", ok && zc > 213.4 && zc < 226.6);
    sound(0);
    ok = play(69, 1.5, 1.4, &peak, &rms, &zc, &tail);
    check("...an octave and more up (A4: 440 Hz within 3 %)", ok && zc > 426.8 && zc < 453.2);

    sound(8);                                         /* PLUCK: HIT ON, struck: it dies away though held */
    trk[0].p[P_E4] = 30;                              /* (a short DECAY) */
    ok = play(60, 2.0, 1.9, &peak, &rms, &zc, &tail);
    check("HIT ON (PLUCK, DECAY short): struck, it dies away though the key is held", ok && rms > 50.0 && tail < 0.05 * peak);
    sound(0);
    ok = play(60, 2.0, 1.9, &peak, &rms, &zc, &tail);
    check("HIT OFF (INIT VA): it sounds as long as the key is held", ok && tail > 0.2 * rms);

    sound(0);                                         /* the settings reach the sound: each moved, the sound differs */
    play(60, 0.5, 0.45, &peak, &base, &zc, &tail);
    for (k = 0, ok = 1; k < 8u; k++) {
        double r1, r2;
        if (k == 6u) continue;                        /* (HIT: above) */
        sound(0);
        trk[0].p[P_E0 + k] = (int16_t)(k == 0u ? 6 : k == 7u ? 127 : 10);
        play(60, 0.5, 0.45, &peak, &r1, &zc, &tail);
        sound(0);
        trk[0].p[P_E0 + k] = (int16_t)(k == 0u ? 4 : 120);
        play(60, 0.5, 0.45, &peak, &r2, &zc, &tail);
        if (r1 == r2) { ok = 0; printf("    E%u: no change\n", k + 1u); }
    }
    check("MODEL, HARM, TIMBR, MORPH, DECAY, COLOR, AUX: each moves the sound", ok);

    {                                                 /* the cost against SID's, one note, on this computer */
        double t0, mc, sid, idle;
        int32_t o[2 * CTL];
        uint32_t f;
        host_tracks_init();
        for (k = 0; k < NPART; k++) trk[k].p[P_LEVEL] = 0;
        t0 = now_s();
        for (f = 0; f < 4u * FS; f += CTL) mix_block(o, CTL);
        idle = now_s() - t0;
        sound(0);
        trk_note_on(&trk[0], 60, 100);
        t0 = now_s();
        for (f = 0; f < 4u * FS; f += CTL) mix_block(o, CTL);
        mc = now_s() - t0 - idle;
        host_tracks_init();
        host_preset(&trk[0], ENGI_SID, 0);
        trk[0].engine = trk[0].eng_req;
        trk_note_on(&trk[0], 60, 100);
        t0 = now_s();
        for (f = 0; f < 4u * FS; f += CTL) mix_block(o, CTL);
        sid = now_s() - t0 - idle;
        printf("macro: cost on this computer, one note: MACRO (VA) %.0f ns a sample, SID %.0f (%.1f x; a report)\n",
               mc * 1e9 / (4.0 * FS), sid * 1e9 / (4.0 * FS), sid > 0 ? mc / sid : 0);
    }
    printf(bad ? "macro: %d FAILED\n" : "macro: all passed\n", bad);
    return bad != 0;
}
