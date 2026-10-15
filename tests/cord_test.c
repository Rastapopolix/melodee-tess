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
