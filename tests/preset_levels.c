/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* Preset levels (a tool, not in the suite): every factory preset of the engines a device build has (TRIO, DRUM, FM6),
 * one note (C4, velocity 100) on track 1 alone, through the real mix:
 *   held  the RMS of the first second with the note held, and its peak (16-bit units, dBFS)
 *   short the RMS of the 250 ms after a note released at a 1/16 step's GATE 50 % (at 120 BPM: 62 ms): how much
 *         of the sound is left when a step is short (a release that cuts off reads low against held)
 * Each line: engine, preset, name, held dB, short dB, short - held, peak dB; then each engine's median and the presets
 * more than 6 dB below it.
 *   cc -O1 -w -Ibuild/gen -Ifirmware/src tests/preset_levels.c -lm -o build/host/preset_levels && build/host/preset_levels */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <math.h>

typedef struct { uint32_t e, k; char name[24]; double held, shrt, peak; } lv_t;
static lv_t lv[256];
static uint32_t nlv;

static double db(double rms) { return rms > 0.5 ? 20.0 * log10(rms / 32768.0) : -99.0; }

/* one note on track 1: held for hold_ms (then released), the RMS of [from_ms, to_ms) from the note-on; *peak */
static double play(uint32_t e, uint32_t k, uint32_t hold_ms, uint32_t from_ms, uint32_t to_ms, double *peak)
{
    int32_t o[2 * CTL];
    uint32_t f, tr, total = to_ms * 44100u / 1000u, on = 1, from = from_ms * 44100u / 1000u;
    double sum = 0, pk = 0;
    uint32_t n = 0;
    host_tracks_init();
    tessera_reset();
    for (tr = 1; tr < NTRK; tr++) trk[tr].p[P_MUTE] = 1;
    host_preset(&trk[0], e, k);
    for (f = 0; f < 8u * CTL; f += CTL) mix_block(o, CTL);   /* (settle) */
    trk_note_on(&trk[0], 60, 100);
    for (f = 0; f < total; f += CTL) {
        if (on && f >= hold_ms * 44100u / 1000u) { trk_note_off(&trk[0], 60); on = 0; }
        mix_block(o, CTL);
        for (uint32_t i = 0; i < CTL; i++) {
            double l = o[2 * i], r = o[2 * i + 1], m = (l + r) / 2;
            if (fabs(l) > pk) pk = fabs(l);
            if (fabs(r) > pk) pk = fabs(r);
            if (f + i >= from) { sum += m * m; n++; }
        }
    }
    if (on) trk_note_off(&trk[0], 60);
    *peak = pk;
    return n ? sqrt(sum / n) : 0;
}
/* a note held hold_ms: the ms from its note-off until a 5 ms window is 30 dB below the one ending at the note-off (-1:
 * not within 2 s); the release a short GATE ends on */
static int32_t release_ms(uint32_t e, uint32_t k, uint32_t hold_ms)
{
    int32_t o[2 * CTL];
    uint32_t f, tr, off = hold_ms * 44100u / 1000u, win = 220u, n = 0;
    double sum = 0, ref = -1;
    host_tracks_init();
    tessera_reset();
    for (tr = 1; tr < NTRK; tr++) trk[tr].p[P_MUTE] = 1;
    host_preset(&trk[0], e, k);
    for (f = 0; f < 8u * CTL; f += CTL) mix_block(o, CTL);
    trk_note_on(&trk[0], 60, 100);
    for (f = 0; f < off + 2u * 44100u; f += CTL) {
        if (f >= off && f < off + CTL) trk_note_off(&trk[0], 60);
        mix_block(o, CTL);
        for (uint32_t i = 0; i < CTL; i++) {
            double m = ((double)o[2 * i] + o[2 * i + 1]) / 2;
            sum += m * m;
            if (++n == win) {
                double r = sqrt(sum / win);
                uint32_t at = f + i + 1u;
                if (at <= off + CTL) ref = r;
                else if (ref > 0 && r < ref * 0.0316) return (int32_t)((at - off) * 1000u / 44100u);
                sum = 0; n = 0;
            }
        }
    }
    return -1;
}
static int by_held(const void *a, const void *b) { double x = ((const lv_t *)a)->held, y = ((const lv_t *)b)->held; return x < y ? -1 : x > y; }

int main(void)
{
    static const uint32_t ENG[3] = {6, 10, 12};             /* TRIO, DRUM, FM6 */
    for (uint32_t ei = 0; ei < 3u; ei++) {
        const engine_t *en = ENGINES[ENG[ei]];
        for (uint32_t k = 0; k < en->npresets && nlv < 256u; k++) {
            lv_t *v = &lv[nlv++];
            const preset_t *p = eng_preset(en, k);
            double pk, pk2;
            v->e = ENG[ei]; v->k = k;
            snprintf(v->name, sizeof v->name, "%s", p && p->name ? p->name : "?");
            v->held = db(play(ENG[ei], k, 1000, 0, 1000, &pk));
            v->shrt = db(play(ENG[ei], k, 62, 0, 250, &pk2));
            v->peak = db(pk);
            printf("%-5s %2u %-20s held %6.1f dB  short %6.1f dB (%+5.1f)  peak %6.1f dB  release %4d ms\n", en->name,
                   (unsigned)k, v->name, v->held, v->shrt, v->shrt - v->held, v->peak, (int)release_ms(ENG[ei], k, 250));
        }
    }
    for (uint32_t ei = 0; ei < 3u; ei++) {
        lv_t s[128];
        uint32_t m = 0;
        for (uint32_t i = 0; i < nlv; i++) if (lv[i].e == ENG[ei] && m < 128u) s[m++] = lv[i];
        if (!m) continue;
        qsort(s, m, sizeof *s, by_held);
        double med = s[m / 2].held;
        printf("\n%s: %u presets, median held %.1f dB; more than 6 dB below it:\n", ENGINES[ENG[ei]]->name, (unsigned)m, med);
        for (uint32_t i = 0; i < m; i++)
            if (s[i].held < med - 6.0) printf("  %2u %-20s %6.1f dB (%+.1f)\n", (unsigned)s[i].k, s[i].name, s[i].held, s[i].held - med);
    }
    return 0;
}
