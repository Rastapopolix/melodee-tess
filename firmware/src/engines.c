/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Engine table (order = PRESETS browsing order and the engine numbers of the editor protocol) and the parts'
 * sounds at power-on. */
#include "dsp.c"
#include "audio_batch.h"
#include "eng_prophet_test.c"   /* (always: what the rest names; MELODEE_PROPHET 0: nothing reaches it, core.h) */
#include "eng_phase.c"
#include "eng_cz.c"
#include "eng_sid.c"
#include "eng_formant.c"
#include "eng_trio.c"
#include "eng_wheel.c"
#include "eng_phys.c"           /* PHYS: DaisySP physical models (phys_dsp.c, MIT) */
#include "eng_drum.c"           /* DRUM: synthesized 808 circuits (drum_808.c), the 909, a C64's SID (drum_sid.c) */
#include "eng_noise.c"
#include "eng_fm6.c"            /* FM6: 6-operator FM rendered as Dexed renders it (fm6_core.c) */
#if MELODEE_SLICE
#include "eng_slice.c"
#else
static void slice_gone_note_on(struct track *t, voice_t *v) { (void)t; (void)v; }
static void slice_gone_render(struct track *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    (void)t; (void)v; (void)out; (void)n; (void)m;
}
static const engine_t ENG_RETIRED = {         /* 13 without MELODEE_SLICE: reserved, never offered (eng_ok) */
    .name = "-",
    .page_title = {"-", "-"},
    .edit = {{"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0},
             {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0},
             {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0}},
    .note_on = slice_gone_note_on,
    .render = slice_gone_render,
    .knob = {P_E4, P_E5, P_E6, P_REL},
};
#endif

/* Each track owns only its selected engine's working state. Simple engines
 * use voice_t alone. State is released after the old engine's switch fade. */
#include "resources.c"
#include "eng_macro.c"          /* MACRO: Plaits' synthesis (MELODEE_PLAITS; macro_glue.cc, third_party/mi) */
static uint32_t eng_state_size(uint32_t e)
{
    switch (e) {
#if MELODEE_PROPHET
    case ENGI_PROPHET: return sizeof(p5_part_t);
#if MELODEE_PROPHET_PROTOTYPE
    case 0: return sizeof(p5_part_t);
#endif
#endif
#if PHYS_ON
    case ENGI_PHYS: return sizeof(phys_slot_t) * PHYS_POLY;
#endif
#if MELODEE_LEGACY_EXTRAS
    case 7: return sizeof(drw_part_t);
#endif
#if MELODEE_LEGACY_EXTRAS
    case 2:                                         /* (PHASE: CZ-1's core) */
#endif
    case ENGI_CZ: return sizeof(cz_part_t);
    case 3: return sizeof(sid_part_t);              /* SID (eng_sid.c) */
#if MELODEE_PLAITS
    case ENGI_MACRO: return mc_bytes();             /* MACRO: Plaits' voice and its working memory (eng_macro.c) */
#endif
    case ENGI_DRUM: return sizeof(drum909_part_t);
    case ENGI_FM6: return sizeof(fm6_part_t);
#if MELODEE_SLICE
    case ENGI_SLICE: return sizeof(slc_rb_t);
#endif
    default: return 0;
    }
}
static void eng_state_clear(uint32_t part)
{
    if (part < NPART) resource_release(RES_ENGINE0 + part);
}
static void eng_state_reset(void) { for (uint32_t k = 0; k < NPART; k++) eng_state_clear(k); }
static int eng_state_prepare(const track_t *t)
{
    uint32_t size = t->engine==ENGI_DRUM?drum_bytes(t):eng_state_size(t->engine);
    return !size || resource_get(RES_ENGINE0 + (uint32_t)(t - trk), size) != 0;
}
static phys_slot_t *phys_slots(uint32_t part) { return resource_get(RES_ENGINE0 + part % NPART, sizeof(phys_slot_t) * PHYS_POLY); }
static drum909_part_t *drum909_part(uint32_t part) { return resource_get(RES_ENGINE0 + part % NPART, sizeof(drum909_part_t)); }
static drum_lane_t *drum_kit_part(uint32_t part) { return resource_get(RES_ENGINE0 + part % NPART, sizeof(drum_lane_t) * DV_NLANE); }
static dsk_part_t *dsk_part(uint32_t part) { return resource_get(RES_ENGINE0 + part % NPART, sizeof(dsk_part_t)); }
static drw_part_t *drw_of(const track_t *t) { return resource_get(RES_ENGINE0 + (uint32_t)(t - trk), sizeof(drw_part_t)); }
static fm6_part_t *fm6_part(uint32_t part) { return resource_get(RES_ENGINE0 + part % NPART, sizeof(fm6_part_t)); }
static cz_part_t *cz_part(uint32_t part) { return resource_get(RES_ENGINE0 + part % NPART, sizeof(cz_part_t)); }
static sid_part_t *sid_part(uint32_t part) { return resource_get(RES_ENGINE0 + part % NPART, sizeof(sid_part_t)); }
static p5_part_t *p5_part(uint32_t part) { return resource_get(RES_ENGINE0 + part % NPART, sizeof(p5_part_t)); }
#if MELODEE_SLICE
static int16_t (*slc_rbuf(uint32_t part))[SLC_RB] { return resource_get(RES_ENGINE0 + part % NPART, sizeof(slc_rb_t)); }
#endif

/* the editor protocol, user presets and projects store these indices: append, never reorder */
static const engine_t *const ENGINES[NENGINES] = {
#if MELODEE_PROPHET_PROTOTYPE
    &ENG_P5_TEST,                /* test firmware only: RAM patches, NEVER persist */
#else
    &ENG_RETIRED,                /* 0: retired ANALOG; stored ID reserved */
#endif
    &ENG_RETIRED,                /* 1: retired DIGITAL; stored ID reserved */
#if MELODEE_LEGACY_EXTRAS
    &ENG_PHASE,                  /* 2 */
#else
    &ENG_RETIRED,                /* 2: retired PHASE; stored ID reserved (its core: CZ-1's) */
#endif
#if MELODEE_SID
    &ENG_SID,                    /* 3: SID replaces LOFI without reusing a stored ID */
#else
    &ENG_RETIRED,                /* 3: SID left out (core.h); its tracks load FM's INIT */
#endif
    &ENG_RETIRED,                /* 4: retired SAMPLE; reserved to preserve stored indices */
#if MELODEE_LEGACY_EXTRAS
    &ENG_FORMANT,                /* 5 VOICE (eng_formant.c: "voice" is a sounding note in voice.c) */
#else
    &ENG_RETIRED,                /* 5: retired VOICE; stored ID reserved */
#endif
#if TRIO_ON
    &ENG_TRIO,
#else
    &ENG_RETIRED,                /* 6: retired TRIO; stored ID reserved */
#endif
#if MELODEE_LEGACY_EXTRAS
    &ENG_WHEEL,
#else
    &ENG_RETIRED,                /* 7: retired WHEEL; stored ID reserved */
#endif
    &ENG_RETIRED,                /* 8: retired GRAIN; stored index reserved */
#if PHYS_ON
    &ENG_PHYS,
#else
    &ENG_RETIRED,                /* 9: retired PHYS; stored ID reserved */
#endif
    &ENG_DRUM,                   /* 10 (ENGI_DRUM) */
#if MELODEE_LEGACY_EXTRAS
    &ENG_NOISE,                  /* 11 */
#else
    &ENG_RETIRED,                /* 11: retired NOISE; stored ID reserved */
#endif
#if MELODEE_FM6
    &ENG_FM6,                    /* 12 (ENGI_FM6) */
#else
    &ENG_RETIRED,                /* 12: FM6 left out of the build; stored ID reserved */
#endif
#if MELODEE_SLICE
    &ENG_SLICE,                  /* 13 (ENGI_SLICE) */
#else
    &ENG_RETIRED,             /* 13: reserved (MELODEE_SLICE=0 builds without SLICE) */
#endif
    &ENG_RETIRED,                /* 14: retired OBXF; stored index reserved */
#if MELODEE_CZ
    &ENG_CZ,                     /* 15: native Casio CZ-1 */
#else
    &ENG_RETIRED,                /* 15: CZ-1 left out of the build; stored ID reserved */
#endif
    &ENG_RETIRED, &ENG_RETIRED, &ENG_RETIRED, /* 16..18 reserved */
#if MELODEE_PROPHET
    &ENG_P5_TEST,                /* 19: Prophet (ANALOG's ID 0 stays reserved) */
#else
    &ENG_RETIRED,                /* 19: Prophet left out of the build; stored ID reserved */
#endif
    &ENG_RETIRED,                /* 20: reserved (USER_NATIVE_P5, the browser's Prophet user bank) */
#if MELODEE_PLAITS
    &ENG_MACRO,                  /* 21: MACRO, Plaits' synthesis (Tessera) */
#else
    &ENG_RETIRED,                /* 21: MACRO left out of the build; stored ID reserved */
#endif
};

/* a track's engine number as an index (the audio paths: a compare, cheaper than % NENGINES; a bad number: 0) */
static inline uint32_t eng_idx(uint32_t e) { return e < NENGINES ? e : 0u; }

/* preset k (< npresets) of e: its row, or one made on demand (Prophet, CZ-1: their names live in the native data).
 * Made presets take turns in two slots: two can be held at once (main loop only) */
static const preset_t *eng_preset(const engine_t *e, uint32_t k)
{
    static preset_t made[2];
    static char made_name[2][21];
    static uint8_t turn;
    if (e->presets)
        return &e->presets[k];
    turn ^= 1u;
    memset(&made[turn], 0, sizeof made[turn]);
    made_name[turn][0] = 0;
    made[turn].name = made_name[turn];
    e->preset_make(k, &made[turn], made_name[turn]);
    return &made[turn];
}

/* the order the engines are shown in (PRESETS browsing and its ENG knob, the editor's list):
 * engine indices, never a retired one. The indices stay as they are (the
 * stores and the protocol hold them); only this table orders them */
static const uint8_t ENGINE_ORDER[NENG_SHOWN] = {
#if MELODEE_PROPHET
    ENGI_PROPHET,                /* Prophet replaces ANALOG in new-sound browsing */
#endif
#if MELODEE_FM6
    12,                          /* FM6 */
#endif
#if MELODEE_LEGACY_EXTRAS
    2, ENGI_CZ, 3, 5,             /* PHASE CZ-1 SID VOICE */
    6, 7, 9,
    11,                          /* NOISE */
#else
#if MELODEE_CZ
    ENGI_CZ,                     /* CZ-1 */
#endif
#if MELODEE_SID
    3,                           /* SID */
#endif
#if MELODEE_TRIO
    6,                           /* TRIO */
#endif
#if MELODEE_PHYS
    9,                           /* PHYS */
#endif
#endif
#if MELODEE_PLAITS
    ENGI_MACRO,                  /* MACRO */
#endif
#if MELODEE_SLICE
    13,                          /* SLICE */
#endif
    10,                          /* DRUM */
};

/* Retired synth records (ANALOG and DIGITAL since 2.0 too) retain their stored identity and render silence.
 * Never relabel retired records FM6. */
static int eng_extra_retired(uint32_t e)
{
#if !MELODEE_PROPHET_PROTOTYPE
    if (e == ENGI_ANALOG) return 1;
#endif
    return e == ENGI_DIGITAL || (!MELODEE_LEGACY_EXTRAS && (e == 2u || e == 5u || (e == 6u && !MELODEE_TRIO) || e == 7u || e == 11u));
}
/* the engines one can pick, in ENGINE_ORDER: eng_ok(e), the n-th of them
 * eng_vis(n), e's place among them eng_rank(e), the next / previous one eng_step(e, dir) (wraps) */
static int eng_ok(uint32_t e)
{
    return e < NENGINES && !(e >= 16u && e <= 18u) && !eng_extra_retired(e) && e != 4u && e != 8u && e != 14u && e != 20u && (MELODEE_SLICE || e != ENGI_SLICE) &&
           ENG_BUILT(e);
}
static uint32_t eng_sound_idx(uint32_t e)
{
    return eng_extra_retired(e) || eng_ok(e) ? e : ENGI_FALLBACK;
}
static uint32_t eng_vis(uint32_t n) { return ENGINE_ORDER[n % NENG_SHOWN]; }
static uint32_t eng_rank(uint32_t e)
{
    uint32_t n;
    if (!eng_ok(e))
        e = ENGI_FALLBACK;                       /* (a retired or reserved engine: FM6's place, or SID's) */
    for (n = 0; n < NENG_SHOWN && ENGINE_ORDER[n] != e; n++)
        ;
    return n < NENG_SHOWN ? n : 0u;
}
static uint32_t eng_step(uint32_t e, int32_t dir)
{
    return eng_vis((eng_rank(e % NENGINES) + (dir > 0 ? 1u : NENG_SHOWN - 1u)) % NENG_SHOWN);
}

/* The start for testing the generators on the device (Theo 2026-10-15): track 1 PHYS DRONE STRING (2026-10-19; was
 * CZ-1 VIBRAPHONE), and the project at power-on and NEW (melodee_init, project_new_blank) at 85 BPM, the melodic parts
 * in minor pentatonic. 0: Melodee's
 * (track 1 PROPHET FUNK BASS II, the parameters' defaults), which the host tests are written for (hostsim.c) */
#ifndef TRK_START_TESS
#define TRK_START_TESS 1
#endif
/* the parts at power-on (engine, preset; the steps empty): bass (TRK_START_TESS: a drone), pad, lead, drums */
static const uint8_t TRK_DEF[NPART][2] = {
#if TRK_START_TESS && MELODEE_PHYS
    {ENGI_PHYS, 7},                                 /* PHYS DRONE STRING (Theo 2026-10-19) */
#elif TRK_START_TESS && MELODEE_CZ
    {ENGI_CZ, 42},                                  /* CZ-1 VIBRAPHONE */
#elif !TRK_START_TESS && MELODEE_PROPHET
    {ENGI_PROPHET, 4},                              /* PROPHET FUNK BASS II (was ANALOG ACID) */
#elif MELODEE_SID
    {ENGI_SID, 26},                                 /* (without it: SID BELL RING) */
#else
    {ENGI_FM6, 16},                                 /* FM6 GLASS PAD (FM back, PHYS and SID out: 2026-10-21) */
#endif
#if MELODEE_FM6
    {ENGI_FM6, 4},                                  /* FM6 PAD (was DIGITAL PAD) */
#else
    {ENGI_SID, 19},                                 /* (without it: SID 8580 PAD) */
#endif
#if MELODEE_SID
    {3, 0},                                         /* SID INIT SID */
#else
    {ENGI_FM6, 19},                                 /* FM6 KALIMBA (without SID) */
#endif
    {ENGI_DRUM, 0}};                                /* DRUM KIT */
/* the power-on presets when project format 1 was written (project.c PROJ_DEF_SOUND): fixed, whatever TRK_DEF becomes */
static const uint8_t TRK_DEF_V1[NPART] = {4, 4, 0, 0};
#if TRK_START_TESS
#define TRK_DEF_BPM 85
#define TRK_DEF_SCALE 6                             /* MPEN (scales.h N_SCALE) */
#else
#define TRK_DEF_BPM GP[G_BPM].def
#define TRK_DEF_SCALE TP[P_SCALE].def
#endif
static uint32_t trk_def_engine(uint32_t i) { return TRK_DEF[i % NPART][0]; }
