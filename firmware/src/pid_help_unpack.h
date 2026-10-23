/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* PIDS entry i's help text (from PID_HELP_N on: ui_help.c HELP_PAGE's) out of build/gen/pid_help.h (tools/gen_help.py): each coded byte expanded on a small
 * stack, a byte from PID_HELP_LO up being two bytes (PID_HELP_PAIR). out takes PID_HELP_MAX + 1 bytes. */
#include "pid_help.h"

static void pid_help_unpack(uint32_t i, char *out)
{
    const uint8_t *p = PID_HELP_DATA + PID_HELP_AT[i % (PID_HELP_N + HELP_PAGE_N)];
    uint8_t st[PID_HELP_DEPTH];
    uint32_t n = 0;
    for (; *p; p++) {
        uint32_t sp = 0;
        st[sp++] = *p;
        while (sp) {
            uint8_t c = st[--sp];
            if (c >= PID_HELP_LO && sp + 2u <= PID_HELP_DEPTH) {
                st[sp++] = PID_HELP_PAIR[c - PID_HELP_LO][1];
                st[sp++] = PID_HELP_PAIR[c - PID_HELP_LO][0];
            } else if (c < PID_HELP_LO && n < PID_HELP_MAX) {
                out[n++] = (char)c;
            }
        }
    }
    out[n] = 0;
}
