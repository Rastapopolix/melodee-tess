/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* Central parameter IDs (phase 2): one number for every setting, with its name, range, default and help. Cords
 * (the signal bus) name their destinations by it, the online reference is generated from it (pid_dump).
 *
 * An id is FROZEN once released: projects will keep cords by it. The three ranges reuse numbers that are already
 * frozen: a track's P_* and the global G_* (user presets and projects map them by position, core.h), and Tessera's
 * TX_* (tessera.h, append only). */
#ifndef PID_H
#define PID_H
#include <stdint.h>

#define PID_TRACK(p)  ((uint32_t)(p))               /* 0x000..: a track's P_* */
#define PID_GLOBAL(g) (0x100u + (uint32_t)(g))      /* 0x100..: the project's (and the device's) G_* */
#define PID_TESS(x)   (0x200u + (uint32_t)(x))      /* 0x200..: a track's Tessera TX_* */
#define PID_NONE      0xFFFFu

enum {                                              /* pid_entry_t.flags */
    PID_F_TRACK = 1u,                               /* one value per track (pid_get's tr); else one value */
    PID_F_ACTION = 2u,                              /* a button on a page (LOAD, SAVE ..), not a value to set */
    PID_F_DEVICE = 4u                               /* the device's, not the project's (A4, MIDI routing ..) */
};

typedef struct {
    uint16_t pid;
    uint8_t flags;
    const char *name;                               /* "TURING LEN", "LFO RATE", "FX 2 MIX" */
    const char *help;                               /* one or two sentences (the help gesture, the reference) */
} pid_entry_t;

uint32_t pid_count(void);
const pid_entry_t *pid_at(uint32_t i);              /* i < pid_count(), in id order */
const pid_entry_t *pid_find(uint32_t pid);          /* 0: no such parameter */
const param_desc_t *pid_desc(uint32_t pid, uint32_t tr);   /* range, default, format (tr: a track's engine) */
int32_t pid_get(uint32_t pid, uint32_t tr);
int pid_set(uint32_t pid, uint32_t tr, int32_t v);  /* clamped to the range; 0: no such value (an action, a gap) */

#endif
