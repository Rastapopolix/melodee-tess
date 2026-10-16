/* SPDX-License-Identifier: GPL-3.0-only */
/* the FM-1's engines on this computer: one note on track 1, ns per sample over the idle mix (as plaits hosttime) */
#define main hostsim_main
#include "hostsim.c"
#undef main
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e9 + t.tv_nsec; }
static double run(int note)
{
    uint32_t f, k;
    int32_t o[2 * CTL];
    double t0;
    if (note) trk_note_on(&trk[0], 60, 100);
    for (f = 0; f < FS / 4; f += CTL) mix_block(o, CTL);
    t0 = now();
    for (k = 0, f = 0; f < 4u * FS; f += CTL, k++) mix_block(o, CTL);
    t0 = (now() - t0) / (4.0 * FS);
    if (note) trk_note_off(&trk[0], 60);
    for (f = 0; f < FS; f += CTL) mix_block(o, CTL);
    return t0;
}
int main(void)
{
    static const struct { uint32_t e; const char *n; } E[] = {{3, "SID"}, {10, "DRUM"}, {12, "FM6"}, {15, "CZ-1"}, {19, "PROPHET"}};
    uint32_t i, k;
    host_tracks_init();
    for (k = 0; k < 4u; k++) host_power_on_sound(&trk[k], k);
    for (i = 0; i < sizeof E / sizeof E[0]; i++) {
        double idle, one;
        host_preset(&trk[0], E[i].e, 0);
        trk[0].engine = trk[0].eng_req;
        idle = run(0);
        one = run(1);
        printf("fm-1   %-13s %6.1f ns/sample (one note; idle mix %.1f)\n", E[i].n, one - idle, idle);
    }
    return 0;
}
