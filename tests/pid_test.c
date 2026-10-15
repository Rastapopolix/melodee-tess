/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* The central parameter ids (firmware/src/pid.c):
 *   build/host/pid_test tests/pid_ids.txt
 * Checks: ids in order and each a real parameter (no reserved or retired one); every live P_*, G_* and TX_* listed;
 * names unique; each range sane, its default inside it; get and set reach both ends and clamp past them, with the
 * knobs' side effects (STEPS keeps PULSES, FOLLOW never itself, a MODE change releases); actions and the device's
 * settings refused. Then the list against tests/pid_ids.txt: ids are FROZEN (cords and projects will keep them), so
 * any change fails. Rewrite it (PID_UPDATE=1) only to append new ids, or for a range or default meant to change. */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int bad;
static void check(const char *what, int ok)
{
    printf("pid: %-96s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

static int retired(uint32_t p)                    /* DIGITAL's OP ENV / OP LEVEL (params.c: inert, on no page) */
{
    return p >= P_FM1_ATK && p <= P_FM4_LEVEL;
}

static void line_of(const pid_entry_t *e, char *out, size_t cap)
{
    const param_desc_t *d = pid_desc(e->pid, 0);
    snprintf(out, cap, "0x%03x %-24s %s%s%s %d..%d def %d", e->pid, e->name,
             e->flags & PID_F_TRACK ? "track" : "global", e->flags & PID_F_ACTION ? " action" : "",
             e->flags & PID_F_DEVICE ? " device" : "", d ? d->min : 0, d ? d->max : 0, d ? d->def : 0);
}

int main(int argc, char **argv)
{
    const char *golden = argc > 1 ? argv[1] : "tests/pid_ids.txt";
    int update = getenv("PID_UPDATE") && atoi(getenv("PID_UPDATE"));
    uint32_t i, j, n = pid_count(), ordered = 1, real = 1, unique = 1, sane = 1, helped = 0, helped_x = 0;
    uint32_t missing = 0, reach = 1, clamps = 1;
    char what[160];

    host_tracks_init();
    tessera_reset();
    for (i = 0; i < n; i++) {
        const pid_entry_t *e = pid_at(i);
        const param_desc_t *d = pid_desc(e->pid, 0);
        uint32_t id = e->pid;
        if (i && pid_at(i - 1)->pid >= id) ordered = 0;
        if (id >= PID_TESS(0)) real &= id < PID_TESS(TX_COUNT);
        else if (id >= PID_GLOBAL(0)) real &= id < PID_GLOBAL(G_COUNT) && !global_reserved(id - PID_GLOBAL(0));
        else real &= id < P_COUNT && !param_reserved(id) && !retired(id);
        for (j = 0; j < i; j++)
            if (!strcmp(pid_at(j)->name, e->name)) unique = 0;
        if (!e->name[0] || !d || d->min > d->max || d->def < d->min || d->def > d->max) {
            sane = 0;
            printf("    not sane: 0x%03x %s\n", id, e->name);
        }
        helped += e->help[0] != 0;
        helped_x += id >= PID_TESS(0) && e->help[0];
    }
    check("ids in order (pid_find's search)", ordered);
    check("each id a real parameter: no reserved or retired one", real);
    check("names unique", unique);
    check("every range sane, its default inside it", sane);
    for (i = 0; i < P_COUNT; i++)
        missing += !param_reserved(i) && !retired(i) && !pid_find(PID_TRACK(i));
    for (i = 0; i < G_COUNT; i++)
        missing += !global_reserved(i) && i != G_INFO && !pid_find(PID_GLOBAL(i));   /* (G_INFO: the CPU meter) */
    for (i = 0; i < TX_COUNT; i++)
        missing += !pid_find(PID_TESS(i));
    check("every live P_*, G_* (but the CPU meter) and TX_* has an id", !missing);
    check("pid_find: nothing for a gap (a reserved id, past the end)",
          !pid_find(PID_TRACK(P_RESERVED33)) && !pid_find(PID_GLOBAL(G_COUNT)) && !pid_find(PID_TESS(TX_COUNT)));
    snprintf(what, sizeof what, "help: every one of the %u has its text (Tessera's %u among them)", n, helped_x);
    check(what, helped == n && helped_x == TX_COUNT);

    /* get and set: both ends, and past them clamped (every track: PULSES and ROT need STEPS 32 first) */
    pid_set(PID_TESS(TX_MODE), 1, TESS_ENG_MARKOV);           /* (LOCK: Markov's own) */
    for (i = 0; i < n; i++) {
        const pid_entry_t *e = pid_at(i);
        const param_desc_t *d;
        int32_t lo, hi;
        if (e->flags & (PID_F_ACTION | PID_F_DEVICE)) continue;
        if (e->pid == PID_TESS(TX_MODE)) continue;              /* (below, with its release) */
        if (e->pid == PID_TESS(TX_EUC_PULSES) || e->pid == PID_TESS(TX_EUC_ROT)) pid_set(PID_TESS(TX_EUC_STEPS), 1, 32);
        d = pid_desc(e->pid, 1);
        lo = d->min; hi = d->max;
        if (d->fmt == F_ENUM && d->names && (!d->names[lo][0] || !d->names[hi][0])) continue;   /* (an alias) */
        if (e->pid == PID_TESS(TX_TW_FOLLOW)) lo = 0;
        pid_set(e->pid, 1, lo);
        if (pid_get(e->pid, 1) != lo) { reach = 0; printf("    min: 0x%03x %s %d\n", e->pid, e->name, (int)pid_get(e->pid, 1)); }
        pid_set(e->pid, 1, hi);
        if (pid_get(e->pid, 1) != hi) { reach = 0; printf("    max: 0x%03x %s %d\n", e->pid, e->name, (int)pid_get(e->pid, 1)); }
        pid_set(e->pid, 1, hi + 1000);
        if (pid_get(e->pid, 1) != hi) clamps = 0;
        pid_set(e->pid, 1, lo - 1000);
        if (pid_get(e->pid, 1) != lo) clamps = 0;
        pid_set(e->pid, 1, d->def);
    }
    check("set then get: every settable value reaches both ends of its range", reach);
    check("set past either end: clamped to it", clamps);

    host_tracks_init();
    tessera_reset();
    pid_set(PID_TESS(TX_EUC_STEPS), 0, 16);
    pid_set(PID_TESS(TX_EUC_PULSES), 0, 12);
    pid_set(PID_TESS(TX_EUC_ROT), 0, 15);
    pid_set(PID_TESS(TX_EUC_STEPS), 0, 8);
    check("EUCLID STEPS 16 -> 8: PULSES 12 -> 8, ROTATION 15 -> 7 (as the knob)",
          pid_get(PID_TESS(TX_EUC_PULSES), 0) == 8 && pid_get(PID_TESS(TX_EUC_ROT), 0) == 7);
    pid_set(PID_TESS(TX_TW_FOLLOW), 2, 3);
    check("TWEAK FOLLOW: never its own track (T3 on track 3 is OFF)", pid_get(PID_TESS(TX_TW_FOLLOW), 2) == 0);
    pid_set(PID_TESS(TX_MODE), 0, TESS_ENG_TURING);
    pid_set(PID_TESS(TX_EV_LOCK), 0, 80);
    check("EVOLVE LOCK on Turing is its PROB turned round (LOCK 80: PROB 20)", pid_get(PID_TESS(TX_TUR_PROB), 0) == 20);
    tessera.active_gate[0] = 1;
    pid_set(PID_TESS(TX_MODE), 0, TESS_ENG_EUCLID);
    check("GENERATOR MODE changed: its note released (as the knob)", !tessera.active_gate[0] &&
          pid_get(PID_TESS(TX_MODE), 0) == TESS_ENG_EUCLID);
    pid_set(PID_TESS(TX_EV_SEED), 0, 77);
    check("EVOLVE ROLL stopped: the seed at once", tessera.seed[0] == 77 && !tessera.roll[0]);
    i = (uint32_t)song.g[G_SAVE];
    check("an action and a device setting refused (PROJECT SAVE, A4 TUNING), nothing changed",
          !pid_set(PID_GLOBAL(G_SAVE), 0, 1) && !pid_set(PID_GLOBAL(G_A4), 0, 432) && song.g[G_SAVE] == (int16_t)i &&
          tuning_a4 == A4_DEFAULT);
    pid_set(PID_GLOBAL(G_BPM), 0, 85);
    pid_set(PID_TRACK(P_LRATE), 3, 99);
    check("a global and a track value: where Melodee keeps them (song.g, the track's p)",
          song.g[G_BPM] == 85 && trk[3].p[P_LRATE] == 99 && trk[0].p[P_LRATE] != 99);

    /* the frozen list */
    {
        FILE *f = fopen(golden, update ? "w" : "r");
        char want[192], got[192];
        uint32_t diffs = 0, lines = 0;
        if (!f) {
            check("tests/pid_ids.txt opens", 0);
            return 1;
        }
        if (update)
            fprintf(f, "# The central parameter ids (firmware/src/pid.c): id, name, scope, range, default. FROZEN: an id\n"
                       "# never changes meaning. Rewrite (PID_UPDATE=1) only to append ids or for an intended range change.\n");
        else
            while (fgets(want, sizeof want, f) && want[0] == '#') {}
        for (i = 0; i < n; i++) {
            line_of(pid_at(i), got, sizeof got);
            if (update) { fprintf(f, "%s\n", got); continue; }
            if (i) { if (!fgets(want, sizeof want, f)) want[0] = 0; }
            want[strcspn(want, "\n")] = 0;
            lines++;
            if (strcmp(want, got)) {
                if (diffs++ < 8) printf("    want: %s\n    got:  %s\n", want, got);
            }
        }
        if (!update && fgets(want, sizeof want, f)) diffs++;   /* (an id gone from the end) */
        fclose(f);
        if (update) printf("pid: wrote %u ids to %s\n", n, golden);
        else {
            snprintf(what, sizeof what, "the %u ids as tests/pid_ids.txt froze them", lines);
            check(what, !diffs);
        }
    }
    printf("pid: %u ids\n", n);
    if (bad) printf("pid: %d FAILED\n", bad);
    else printf("pid: all passed\n");
    return bad != 0;
}
