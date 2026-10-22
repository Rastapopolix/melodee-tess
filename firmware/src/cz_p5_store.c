/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess)
 * Portions Copyright (C) 2026 Kerem Kilic (Ellic Studio), Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* What stays of CZ-1 and the Prophet once their engines are gone (tessera-notes docs/design/PROJECT_FORMAT.md,
 * step 1): a project still carries each track's native tone and program, the native user presets and banks keep
 * theirs (cz_bank.c, prophet_user.c, native_presets.c), and a bank never saved defaults to the factory sounds.
 * No track plays them: both engines are retired (engines.c ENG_RETIRED, ENG_BUILT). The own project format
 * removes the rest with their bank objects. */
#include "cz_patch.h"
#include "prophet_patch.c"
#include "melodee_prophet_factory.h"   /* Sequential's 200 v1.03 programs (tools/gen_prophet_factory.py) */
#include "melodee_cz1.h"               /* Casio's 64 CZ-1 preset tones (tools/gen_cz1_factory.py) */

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

/* Casio's tone k (0..CZ_FACTORY_N-1) into raw (CZ_BYTES), unpacked against CZ_DICT (tools/gen_cz1_factory.py) */
static void cz_factory(uint32_t k, uint8_t *raw)
{
    if (!MELODEE_CZ) { memset(raw, 0, CZ_BYTES); return; }   /* (Tessera: left out, core.h; its bank with it) */
    uint8_t b[2u * CZ_BYTES];
    memcpy(b, CZ_DICT, CZ_BYTES);
    lz_unpack(b + CZ_BYTES, CZ_LZ + CZ_LZ_AT[k % CZ_FACTORY_N], CZ_BYTES);
    memcpy(raw, b + CZ_BYTES, CZ_BYTES);
}
/* Sequential's factory program k (0..P5_FACTORY_N-1), unpacked (tools/gen_prophet_factory.py pack) */
static void p5_factory(uint32_t k,p5_patch_t *p)
{
    if(!MELODEE_PROPHET){memset(p,0,sizeof *p);return;}   /* (Tessera: left out, core.h; its bank with it) */
    const uint8_t *b=P5_FPACK+k*P5_FACTORY_BYTES;
    uint32_t pos=0;
    for(uint32_t i=0;i<P5_RAW_MAX;i++){
        uint32_t v=0;
        for(uint32_t j=0;j<P5_FWIDTH[i];j++,pos++)v|=(uint32_t)(b[pos>>3]>>(pos&7u)&1u)<<j;
        p->raw[i]=(uint8_t)(P5_FBASE[i]+v);
    }
    p->size=P5_RAW_MAX;p->model=0x32;p->command=2;p->group=(uint8_t)(k/40u);p->program=(uint8_t)(k%40u);
}
