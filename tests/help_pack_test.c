/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* The packed help texts (build/gen/pid_help.h, tools/gen_help.py; the device build reads them, MELODEE_HELP_PACKED):
 * every PIDS entry's expands to exactly its plain text in pid.c, and none comes out cut. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include "pid_help_unpack.h"

int main(void)
{
    static char text[PID_HELP_MAX + 1];
    uint32_t i, bad = 0, bytes = 0;
    if (pid_count() != PID_HELP_N) {
        printf("help pack: %u entries packed, pid.c has %u (run tools/gen_help.py)\n", PID_HELP_N, pid_count());
        return 1;
    }
    for (i = 0; i < pid_count(); i++) {
        const pid_entry_t *e = pid_at(i);
        memset(text, 0x55, sizeof text);
        pid_help_unpack(i, text);
        bytes += (uint32_t)strlen(e->help) + 1u;
        if (strcmp(text, e->help) != 0 || strlen(e->help) > PID_HELP_MAX) {
            printf("help pack: %s expands to \"%.60s\", not \"%.60s\"\n", e->name, text, e->help);
            bad++;
        }
    }
    printf("help pack: %u texts (%u B plain) expand as written: %s\n", pid_count(), bytes, bad ? "FAIL" : "ok");
    return bad != 0;
}
