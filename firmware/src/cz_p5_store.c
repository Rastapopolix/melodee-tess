/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess)
 * Portions Copyright (C) 2026 Kerem Kilic (Ellic Studio), Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* What stays of CZ-1 and the Prophet once their engines are gone (tessera-notes docs/design/PROJECT_FORMAT.md,
 * step 1): a project's FUN20 record still carries each track's native tone and program. No track plays them: both
 * engines are retired (engines.c ENG_RETIRED, ENG_BUILT), their user stores and factory sounds removed. Step 3b
 * of the own project format removes the rest with FUN20. */
#include "cz_patch.h"
#include "prophet_patch.c"

static cz_patch_t cz_patch[NTRK] __attribute__((section(".pool")));   /* a project's tracks' CZ-1 tones */
static void cz_init(void) { for (uint32_t k = 0; k < NTRK; k++) cz_patch_init(cz_patch[k].raw); }
static void cz_track_accept(track_t *t) { (void)t; }   /* (a tone restored onto a track: nothing plays it) */

static p5_patch_t p5_patch[NPART] __attribute__((section(".pool")));   /* .. and their Prophet programs */
static uint8_t p5_ready[NPART];
static p5_patch_t *p5_patch_of(const track_t *t)
{
    uint32_t k = (uint32_t)(t - trk) % NPART;
    if (!p5_ready[k]) { p5_patch_init(&p5_patch[k]); p5_ready[k] = 1; }
    return &p5_patch[k];
}
