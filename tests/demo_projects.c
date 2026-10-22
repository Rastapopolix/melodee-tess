/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* Tessera 2.0's demo projects (demos/), made by the firmware's own code: each one set up through the functions the
 * pages call (a new project, the sounds, tessera_param_set, cords), saved into a slot of simulated NOR flash by
 * project_save_as, read back as the backup page reads a slot (editor_backup.c, object 2..5: the TSP1 record), loaded
 * into a fresh project and played. Built as the device builds it (tests/run_tests.sh: -DMELODEE_SID=0
 * -DMELODEE_PHYS=0, the power-on of TRK_START_TESS).
 *   build/host/demo_projects make DIR [SECS]   the records (DIR/<name>.tsp) and SECS s of each (DIR/<name>.wav)
 *   build/host/demo_projects check DIR         every demo record in DIR (demos/) restores through the backup page's PUT, loads
 *                                              and plays: a format change that breaks the shipped demos fails here
 *   build/host/demo_projects archive OUT.json  a new device's whole backup, as the editor's Complete backup saves it
 *   build/host/demo_projects restore IN.json   such a backup restored as the editor restores it, then each slot loaded
 *                                              and played (tools/demo_backup.py's test: the demos put into a backup)
 *   build/host/demo_projects levels [SECS]     each track alone: its level, to balance a demo by
 *   build/host/demo_projects log N SECS        demo N's notes as CSV (time, track, note, velocity)
 * Checks per demo: the record saves, reads back the same, restores and loads with every generator, cord and sound;
 * each track plays notes; no sample at full scale; the voices stay inside the budget. */
#define TESSERA_NOTE_HOOK dm_note
#define main hostsim_main
#include "hostsim.c"
#undef main

static int32_t fm1_enc_take(uint32_t e) { (void)e; return 0; }
static void fm1_irq_off(void) {}
static void fm1_irq_on(void) {}
static void lcd_sync(void) {}
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c) { (void)x; (void)y; (void)w; (void)h; (void)c; }
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{ (void)x; (void)y; (void)w; (void)h; (void)p; }
#define MELODEE_FLASH 1
#include "../firmware/src/gfx.c"
#include "../firmware/src/panel.c"
#include "../firmware/src/ui.c"
static void panel_setup(void) {}

static uint8_t nor[0x100000], flash_ok = 1;
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n) { memcpy(nor + off, src, n); return 0; }
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0; }
static void fl_plain_window_init(void) {}
#define FL_FAR(fn) (fn)
#include "../firmware/src/storage.c"
#include "../firmware/src/upreset.c"
#include "../firmware/src/project.c"

#define ED_BK_FLASH_PTR(off) (nor + (off))
enum { ED_BACKUP_LIST = 65, ED_BACKUP_GET, ED_BACKUP_PUT };
static uint8_t rep[4096];
static uint32_t rep_n;
static void ed_b(uint32_t v) { if (rep_n < sizeof rep) rep[rep_n++] = (uint8_t)(v & 127u); }
static uint32_t ed_unpack7(const uint8_t *a, uint32_t na, uint8_t *out, uint32_t max)
{
    uint32_t n = 0;
    while (na && n < max) {
        uint32_t m = *a++, j;
        na--;
        uint32_t k = na > 7u ? 7u : na;
        if (!k || n + k > max || (m >> k))
            return 0;
        for (j = 0; j < k; j++, na--)
            out[n++] = (uint8_t)(*a++ | ((m >> j) & 1u) << 7);
    }
    return na ? 0u : n;
}
static uint8_t ed_smp_buf[512] __attribute__((aligned(4)));
static void fm1_wdt_feed(void) {}
static int ed_flash_stop(void) { return transport_busy(); }
#include "../firmware/src/editor_backup.c"

#define ENGI_TRIO 6u

static int check(const char *what, int ok)
{
    printf("demos: %-80s %s\n", what, ok ? "ok" : "FAIL");
    return !ok;
}

/* ------------------------------------------------------------- the device --- */
static uint32_t dm_notes[NTRK];
static FILE *dm_log;                                  /* (DM_LOG: every note, for a look at what they play) */
static uint32_t dm_f;
static void dm_note(track_t *t, uint32_t note, uint32_t vel)
{
    uint32_t tr = trk_index(t);
    if (tr < NTRK) dm_notes[tr]++;
    if (dm_log && tr < NTRK) fprintf(dm_log, "%.2f,%u,%u,%u\n", (double)dm_f / FS, tr + 1u, note, vel);
}

/* power-on with empty flash, then NEW: the device's new project */
static void power_on(void)
{
    memset(nor, 0xFF, sizeof nor);
    memset(&song, 0, sizeof song);
    memset(trk, 0, sizeof trk);
    memset(&chain, 0, sizeof chain);
    chain_defaults(&chain_config);
    pattern_init();
    memset(proj_bank_len, 0, sizeof proj_bank_len);
    memset(proj_meta, 0, sizeof proj_meta);
    up_cache_reset(); native_cache_reset();
    memset(&persist_saved, 0, sizeof persist_saved);
    memset(&settings, 0, sizeof settings);
    memset(&ui, 0, sizeof ui);
    panel = PANEL_DEFAULT;
    settings_init();
    host_tracks_init();
    fm6_init();
    fm1_ms = 0;
    transport_req = 0;
    ed_bk_valid = ed_bk_put = 0;
    tessera_reset();
    project_new_blank();
    song.master_q12 = 2048;                           /* (melodee_init's) */
}

/* the backup page's PUT of one object: begin, 256-byte chunks, commit; 0 done */
static void put32(uint8_t *a, uint32_t v) { for (uint32_t i = 0; i < 5u; i++) a[i] = (uint8_t)((v >> (7u * i)) & 127u); }
static uint32_t call(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    rep_n = 0;
    ed_backup_handle(cmd, a, n);
    return rep_n;
}
static uint32_t put_all(uint32_t id, const uint8_t *p, uint32_t len)
{
    uint8_t a[16 + 300];
    uint32_t off, crc = st_crc32(p, len);
    a[0] = 0; a[1] = (uint8_t)id; put32(a + 2, len); put32(a + 7, crc);
    call(ED_BACKUP_PUT, a, 12);
    if (rep[2]) return rep[2];
    for (off = 0; off < len; off += 256u) {
        uint32_t n = len - off > 256u ? 256u : len - off, k = 7, i;
        const uint8_t *q = p + off;
        a[0] = 1; a[1] = (uint8_t)id; put32(a + 2, off);
        while (n) {
            uint32_t g = n > 7u ? 7u : n, m = 0;
            for (i = 0; i < g; i++) m |= (uint32_t)(q[i] >> 7) << i;
            a[k++] = (uint8_t)m;
            for (i = 0; i < g; i++) a[k++] = q[i] & 127u;
            q += g; n -= g;
        }
        call(ED_BACKUP_PUT, a, k);
        if (rep[2]) return rep[2];
    }
    a[0] = 2; a[1] = (uint8_t)id;
    call(ED_BACKUP_PUT, a, 2);
    return rep[2];
}

/* ------------------------------------------------------------- the pages --- */
static void sound(uint32_t tr, uint32_t engine, uint32_t preset)   /* PRESETS: an engine's preset, with its sends */
{
    track_t *t = &trk[tr];
    engine_load_of(t, engine, preset);
    t->engine = t->eng_req;
}
static void tx(uint32_t tr, uint32_t x, int32_t v) { tessera_param_set(tr, x, v); }
static void gen(uint32_t tr, uint32_t mode, uint32_t seed)          /* GEN's MODE, EVOLVE's ROLL */
{
    tx(tr, TX_MODE, (int32_t)mode);
    tx(tr, TX_EV_SEED, (int32_t)seed);
}
static void scale(uint32_t root, uint32_t scl, uint32_t qnt)        /* SCL: the song's scale; ROOT on every track */
{
    for (uint32_t k = 0; k < NTRK; k++) {
        trk[k].p[P_ROOT] = (int16_t)root;
        trk[k].p[P_SCALE] = (int16_t)scl;
        trk[k].p[P_QUANT] = (int16_t)qnt;
    }
}
static void cord(uint32_t i, uint32_t src, uint32_t str, uint32_t dst, uint32_t dtr, int32_t amount, int32_t offset, uint32_t uni)
{
    cord_t c = {(uint8_t)src, (uint8_t)str, (uint8_t)dst, (uint8_t)dtr, (int8_t)amount, (int8_t)offset, (uint8_t)uni};
    cord_set(i, &c);
}
/* the send effects: 1 a reverb, 2 an echo (FX: TYPE MIX RATE CHAR) */
static void sends(int32_t rv_rate, int32_t rv_char, int32_t ec_rate, int32_t ec_char)
{
    song.g[G_MFXTYPE] = 8; song.g[G_MFXMIX] = 127; song.g[G_MFXRATE] = (int16_t)rv_rate; song.g[G_MFXCHAR] = (int16_t)rv_char;
    song.g[G_MFX2TYPE] = 6; song.g[G_MFX2MIX] = 127; song.g[G_MFX2RATE] = (int16_t)ec_rate; song.g[G_MFX2CHAR] = (int16_t)ec_char;
}
static void mixer(uint32_t tr, int32_t level, int32_t pan, int32_t reverb, int32_t echo)
{
    trk[tr].p[P_LEVEL] = (int16_t)level; trk[tr].p[P_PAN] = (int16_t)pan;
    trk[tr].p[P_FXSEND] = (int16_t)reverb; trk[tr].p[P_FXSEND2] = (int16_t)echo;
}
enum { DIV_4, DIV_8, DIV_16, DIV_32, DIV_8T, DIV_16T, DIV_2, DIV_1, DIV_2BAR, DIV_4BAR };   /* (params.c N_DIV) */
enum { SCL_MIN = 2, SCL_DOR = 3, SCL_IN = 70, SCL_YO, SCL_HIRA, SCL_IWATO, SCL_KUMOI, SCL_RYUKYU };   /* (scales.h N_SCALE) */
enum { FX_TINE = 8, FX_KALIMBA = 19, FX_SOFT_PAD = 25, FX_DEEP_PAD = 27, FX_RIN = 31, FX_FURIN = 33, FX_MOKUGYO = 35,
       FX_KOTO_PLUCK = 36, FX_KOTO_RING = 37, FX_KUGO_HARP = 39 };   /* FM6 presets (eng_fm6.c FM6_PRESETS) */
enum { TR_FAT_BASS = 0, TR_WARM_PAD = 5, TR_HOLLOW_PAD = 7, TR_SUB_BASS = 9, TR_BREATH_FLUTE = 11 };   /* TRIO's */
enum { DK_808_KIT, DK_909_KIT };                                   /* DRUM's */

/* ------------------------------------------------------------- the demos --- */
/* STILL TIDE: the ambient one. A DIA7 drone that breathes, a warm pad answering it, wind chimes falling, a sub bass
 * that rarely moves; ENERGY's slow arc thins the generators over minutes and RESTS leaves the drone alone now and
 * then. YO on D, 72 BPM: nothing to clash, so it can run for an hour. */
static void demo_still(void)
{
    song.g[G_BPM] = 72;
    sound(0, ENGI_FM6, FX_SOFT_PAD);
    sound(1, ENGI_TRIO, TR_WARM_PAD);
    sound(2, ENGI_FM6, FX_FURIN);
    sound(3, ENGI_TRIO, TR_SUB_BASS);
    scale(2, SCL_YO, 0);
    sends(110, 90, 70, 60);

    /* T1: CHORD DRONE, held, every voice on its own breath */
    trk[0].p[P_CHRD] = 2;                             /* DIA7 */
    trk[0].p[P_VOIC] = 1;                             /* OPEN */
    gen(0, TESS_ENG_CHORD, 11);
    tx(0, TX_CH_PROG, GEN_CH_DRONE); tx(0, TX_CH_BARS, 3); tx(0, TX_CH_HIT, GEN_CH_HOLD);
    tx(0, TX_CH_OCT, -1); tx(0, TX_CH_BREATH, 80);
    mixer(0, 100, -10, 90, 0);

    /* T2: AWAKE at half notes, ringing to the next (LENGTH HIT), PROB leaving gaps */
    trk[1].p[P_SDIV] = DIV_2;
    gen(1, TESS_ENG_AWAKE, 37);
    tx(1, TX_AWK_LENA, 7); tx(1, TX_AWK_LENB, 5); tx(1, TX_AWK_PROB, 55);
    tx(1, TX_NL_MODE, 2);
    tx(1, TX_EV_EVERY, 4); tx(1, TX_EV_AMOUNT, 25);   /* EVERY 8 bars */
    mixer(1, 84, 20, 100, 30);

    /* T3: FALL (BOUNCE past 4 balls): leaves landing on bells; the wind leans them with TIDE */
    trk[2].p[P_SDIV] = DIV_8;
    gen(2, TESS_ENG_BOUNCE, 5);
    tx(2, TX_BNC_BALLS, 5); tx(2, TX_BNC_SPEED, 10); tx(2, TX_BNC_ANGLE, 45);
    tx(2, TX_TW_DROP, 45); tx(2, TX_TW_HUMAN, 40); tx(2, TX_TW_SHIFT, 12);
    mixer(2, 102, -30, 110, 70);

    /* T4: MARKOV, mostly neighbours, mostly rests, an octave down (SUB BASS adds its own octave below) */
    trk[3].p[P_SDIV] = DIV_2;
    gen(3, TESS_ENG_MARKOV, 64);
    tx(3, TX_MKV_LEAP, 10); tx(3, TX_MKV_DENS, 45); tx(3, TX_MKV_SPAN, 1);
    tx(3, TX_TW_SHIFT, -12); tx(3, TX_NL_MODE, 2);
    mixer(3, 85, 0, 30, 0);

    /* ENERGY: the project's tide and rests */
    tx(0, TX_EN_ARC, 60); tx(0, TX_EN_RESTS, 70);
    /* MODS */
    cord(0, CS_TORPOR, 0, CD_CUTOFF, 0, 30, 0, 0);    /* T1 TIDE 1 -> T1 CUTOFF */
    cord(1, CS_OCHD2, 0, CD_PAN, 0, 25, 0, 0);        /* T1 SWARM 2 -> T1 PAN */
    cord(2, CS_INERTIA, 2, CD_BNC_ANGLE, 2, 40, 0, 0);   /* T3 TIDE 2 -> T3 WIND */
    cord(3, CS_ENERGY, 0, CD_FX_SEND, 1, 30, 0, 0);   /* ENERGY -> T2 FX SEND */
}

/* MOSAIC: a groove. ATLAS drums on the 909 with fills, an EUCLID bass, CHORD 2-5-1 a bar each and a TURING
 * line in FORM ABAC; QNT CHRD keeps the bass and the line on the chord, BRANCH throws some of the line to the keys, a
 * RATCH now and then. A dorian, 108 BPM */
static void demo_mosaic(void)
{
    song.g[G_BPM] = 108;
    sound(0, ENGI_TRIO, TR_FAT_BASS);
    sound(1, ENGI_FM6, FX_TINE);
    sound(2, ENGI_FM6, FX_KALIMBA);
    sound(3, ENGI_DRUM, DK_909_KIT);
    scale(9, SCL_DOR, 5);                             /* QNT CHRD: the chord track leads */
    sends(70, 64, 60, 50);

    /* T1: EUCLID 5 of 16, turned by 2 */
    trk[0].p[P_SDIV] = DIV_16;
    gen(0, TESS_ENG_EUCLID, 23);
    tx(0, TX_EUC_STEPS, 16); tx(0, TX_EUC_PULSES, 5); tx(0, TX_EUC_ROT, 2);
    tx(0, TX_TW_SHIFT, -12);
    mixer(0, 109, 0, 0, 0);

    /* T2: CHORD 2-5-1 a bar each, held, sevenths (HIT BEAT would take the others' release tails all the time) */
    trk[1].p[P_CHRD] = 2;                             /* DIA7 */
    trk[1].p[P_VOIC] = 1;                             /* OPEN */
    gen(1, TESS_ENG_CHORD, 3);
    tx(1, TX_CH_PROG, GEN_CH_251); tx(1, TX_CH_BARS, 1); tx(1, TX_CH_HIT, GEN_CH_HOLD);
    tx(1, TX_TW_HUMAN, 30);
    trk[1].p[P_SGATE] = 40;
    mixer(1, 88, -15, 50, 20);

    /* T3: TURING, 8 steps, nearly locked; FORM ABAC of 4 bars; a quarter of its hits branch to T2's sound */
    trk[2].p[P_SDIV] = DIV_16;
    gen(2, TESS_ENG_TURING, 88);
    tx(2, TX_TUR_LEN, 8); tx(2, TX_TUR_PROB, 8); tx(2, TX_TUR_SPAN, 2);
    tx(2, TX_EV_EVERY, 3); tx(2, TX_FM_FORM, 2); tx(2, TX_FM_CONTRAST, 50);
    tx(2, TX_TW_BRANCH, 25); tx(2, TX_TW_BR_TO, 2); tx(2, TX_TW_BR_MODE, TESS_BR_COIN);
    tx(2, TX_TW_SHIFT, 12); tx(2, TX_TW_DROP, 20); tx(2, TX_TW_HUMAN, 25);
    tx(2, TX_SUB_RATCH, 1); tx(2, TX_SUB_CHANCE, 12);
    mixer(2, 108, 25, 40, 50);

    /* T4: ATLAS with toms as fills, EVOLVE walking the map; SWARM sweeps X */
    trk[3].p[P_SDIV] = DIV_16;
    gen(3, TESS_ENG_GRIDS, 41);
    tx(3, TX_GRD_X, 70); tx(3, TX_GRD_Y, 150); tx(3, TX_GRD_DENS, 58);
    tx(3, TX_GRD_TOMS, 35); tx(3, TX_GRD_HAT, 80);
    tx(3, TX_EV_LOCK, 85); tx(3, TX_EV_EVERY, 3); tx(3, TX_EV_AMOUNT, 25);
    mixer(3, 91, 0, 15, 0);

    cord(0, CS_OCHD1, 3, CD_GRD_X, 3, 25, 0, 0);      /* T4 SWARM 1 -> T4 AT X */
    cord(1, CS_OCHD3, 1, CD_CUTOFF, 0, 20, 0, 0);     /* T2 SWARM 3 -> T1 CUTOFF */
    cord(2, CS_GATE, 0, CD_GAIN, 1, -15, 0, 0);       /* T1 GATE -> T2 GAIN: the keys duck under the bass */
}

/* KOTO GARDEN: Zen-1's and FuMi-1's FM voices in HIRA on C. Two koto on QUAD's shared grid, one in eighths and one in
 * triplets, a column turning down so notes travel between them; a breath flute on the infinity series; a mokugyo
 * on POLY's three rhythms. 92 BPM */
static void demo_garden(void)
{
    song.g[G_BPM] = 92;
    sound(0, ENGI_FM6, FX_KOTO_PLUCK);
    sound(1, ENGI_FM6, FX_KOTO_RING);
    sound(2, ENGI_TRIO, TR_BREATH_FLUTE);
    sound(3, ENGI_FM6, FX_MOKUGYO);
    scale(0, SCL_HIRA, 0);
    sends(90, 80, 80, 55);

    /* T1, T2: QUAD rows, eighths against triplets; column 1 moves down every 8 steps, column 3 at random */
    trk[0].p[P_SDIV] = DIV_8;
    gen(0, TESS_ENG_QUAD, 19);
    tx(0, TX_QD_DIR, 0); tx(0, TX_QD_PROB, 80); tx(0, TX_QD_RANGE, 2);
    tx(0, TX_QD_MOVE1, 1); tx(0, TX_QD_EVERY1, 5);
    tx(0, TX_QD_MOVE3, 3); tx(0, TX_QD_EVERY3, 6);
    tx(0, TX_TW_HUMAN, 30);
    mixer(0, 106, -25, 60, 20);
    trk[1].p[P_SDIV] = DIV_8T;
    gen(1, TESS_ENG_QUAD, 19);
    tx(1, TX_QD_DIR, 2); tx(1, TX_QD_PROB, 60);
    tx(1, TX_TW_SHIFT, 12); tx(1, TX_TW_HUMAN, 30);
    mixer(1, 108, 25, 70, 40);

    /* T3: SERIES at quarter notes, two degrees a step, ringing on */
    trk[2].p[P_SDIV] = DIV_4;
    gen(2, TESS_ENG_SERIES, 50);
    tx(2, TX_SR_START, 17); tx(2, TX_SR_INTV, 2); tx(2, TX_SR_LOOP, 0);
    tx(2, TX_NL_MODE, 2); tx(2, TX_TW_DROP, 35); tx(2, TX_TW_HUMAN, 35);
    mixer(2, 81, 0, 90, 30);

    /* T4: POLY, rhythms /3 /4 /5 against each other */
    trk[3].p[P_SDIV] = DIV_16;
    gen(3, TESS_ENG_POLY, 7);
    tx(3, TX_POL_DIV1, 3); tx(3, TX_POL_DIV2, 4); tx(3, TX_POL_DIV3, 5); tx(3, TX_POL_DIV4, 0);
    tx(3, TX_POL_SPAN, 1); tx(3, TX_TW_SHIFT, -12);
    mixer(3, 108, 10, 40, 0);

    cord(0, CS_APATHY, 0, CD_QD_PROB, 0, -40, 0, 0);  /* T1 TIDE 3 -> T1 Q PROB: the koto thins and fills */
    cord(1, CS_OCHD4, 2, CD_CUTOFF, 2, 25, 0, 0);     /* T3 SWARM 4 -> T3 CUTOFF: the flute's breath */
    cord(2, CS_DRIFT, 1, CD_PAN, 1, 30, 0, 0);        /* T2 DRIFT -> T2 PAN */
}

static const struct { const char *name, *file; void (*make)(void); } DEMOS[] = {
    {"STILL TIDE", "still-tide", demo_still},
    {"MOSAIC", "mosaic", demo_mosaic},
    {"KOTO GARDEN", "koto-garden", demo_garden},
};
#define NDEMO (sizeof DEMOS / sizeof DEMOS[0])

/* ------------------------------------------------------------- playing --- */
/* PLAY for secs s (wav: written there, 0 none): the notes each track played, the samples near full scale, the most
 * voices sounding; returns the peak */
typedef struct { uint32_t notes[NTRK], clips, voices, taken; int32_t peak; double rms; } dm_play_t;
static dm_play_t play(uint32_t secs, const char *wav)
{
    dm_play_t r;
    FILE *w = 0;
    uint32_t f, i, frames = secs * FS, kills0 = voice_kills;
    double sq = 0;
    memset(&r, 0, sizeof r);
    memset(dm_notes, 0, sizeof dm_notes);
    if (wav && !(w = fopen(wav, "wb"))) fprintf(stderr, "demos: cannot write %s\n", wav);
    if (w) wav_hdr(w, frames);
    transport_req = 1;
    for (f = 0; f < frames; f += CTL) {
        int32_t o[2 * CTL];
        dm_f = f;
        mix_block(o, CTL);
        fm1_ms = f / (FS / 1000u);
        if (busy_now() > r.voices) r.voices = busy_now();
        for (i = 0; i < 2u * CTL; i++) {
            int32_t a = o[i] < 0 ? -o[i] : o[i];
            if (a >= 32700) r.clips++;
            if (a > r.peak) r.peak = a;
            sq += (double)o[i] * o[i];
        }
        if (w) for (i = 0; i < CTL; i++) wav_put(w, o[2 * i], o[2 * i + 1]);
    }
    if (w) fclose(w);
    transport_req = 2;                                /* STOP */
    for (f = 0; f < FS; f += CTL) { int32_t o[2 * CTL]; mix_block(o, CTL); }
    memcpy(r.notes, dm_notes, sizeof r.notes);
    r.taken = voice_kills - kills0;
    r.rms = sqrt(sq / (2.0 * frames)) / 32768.0;
    return r;
}

static int report(const char *name, const dm_play_t *p)
{
    char what[160];
    int bad = 0;
    snprintf(what, sizeof what, "%s plays: notes T1 %u T2 %u T3 %u T4 %u, peak %.2f, rms %.3f, voices %u", name,
             p->notes[0], p->notes[1], p->notes[2], p->notes[3], p->peak / 32768.0, p->rms, p->voices);
    bad += check(what, p->notes[0] && p->notes[1] && p->notes[2] && p->notes[3]);
    snprintf(what, sizeof what, "%s: no sample at full scale (%u), voices inside the budget of %u (%u taken)", name, p->clips,
             NVOICE, p->taken);
    bad += check(what, !p->clips && p->voices <= NVOICE);
    return bad;
}

/* raw (len) through the backup page's PUT into slot 1, loaded into a new project: 0 done */
static int restore_load(const uint8_t *raw, uint32_t len, char *name)
{
    power_on();
    if (put_all(2, raw, len)) return 1;
    project_new_blank();
    tessera_reset();
    project_load(0);
    if (proj_cur != 0u) return 2;
    project_cur_name(name);
    song.g[G_SLOT] = 1;
    return 0;
}

static uint8_t rec[TSP_MAX], again[TSP_MAX];

static int make(const char *dir, uint32_t secs)
{
    int bad = 0;
    for (uint32_t d = 0; d < NDEMO; d++) {
        char path[512], name[PROJ_NAME_LEN + 1u], what[160];
        uint32_t len, len2;
        int n;
        FILE *f;
        dm_play_t a, b;
        uint8_t tess[TESSERA_PK_BYTES], cords[CORD_PK_BYTES];
        power_on();
        DEMOS[d].make();
        song.g[G_SLOT] = 1;                           /* (SLOT 1 in every record; a load keeps the device's own) */
        tessera_pack(tess); cord_pack(cords);
        snprintf(what, sizeof what, "%s: saved to slot %u", DEMOS[d].name, d + 1u);
        bad += check(what, project_save_as(d, DEMOS[d].name) == 0);
        n = st_load(OBJ_SLOT(d), rec, sizeof rec);      /* (what the backup page's GET of object 2 + d reads) */
        len = n > 0 ? (uint32_t)n : 0u;
        snprintf(what, sizeof what, "%s: the slot is a whole TSP1 record (%u B of %u)", DEMOS[d].name, len, TSP_MAX);
        bad += check(what, len && tsp_valid(rec, len));
        snprintf(path, sizeof path, "%s/%s.tsp", dir, DEMOS[d].file);
        if (!(f = fopen(path, "wb")) || fwrite(rec, 1, len, f) != len) { bad += check(path, 0); if (f) fclose(f); continue; }
        fclose(f);
        /* through a restore and a load: the same record, generators and cords */
        bad += check("restored through the backup page and loaded", restore_load(rec, len, name) == 0);
        bad += check("its name", !strcmp(name, DEMOS[d].name));
        {
            uint8_t t2[TESSERA_PK_BYTES], c2[CORD_PK_BYTES];
            tessera_pack(t2); cord_pack(c2);
            bad += check("the generators and cords as made", !memcmp(t2, tess, sizeof tess) && !memcmp(c2, cords, sizeof cords));
        }
        project_capture(&proj_scratch);
        len2 = tsp_pack(again, &proj_scratch, 1);
        bad += check("saved again, the same record", len2 == len && !memcmp(again, rec, len));
        /* played as a player gets it, loaded (not as made: FALL's leaves, the balls, are not saved, so they start from
         * where a load leaves them); twice, the same notes */
        a = play(20, 0);
        restore_load(rec, len, name);
        b = play(20, 0);
        snprintf(what, sizeof what, "loaded again, the same notes (T1 %u T2 %u T3 %u T4 %u)", b.notes[0], b.notes[1], b.notes[2], b.notes[3]);
        bad += check(what, !memcmp(a.notes, b.notes, sizeof a.notes));
        restore_load(rec, len, name);
        snprintf(path, sizeof path, "%s/%s.wav", dir, DEMOS[d].file);
        a = play(secs, path);
        bad += report(DEMOS[d].name, &a);
    }
    return bad;
}

static int check_dir(const char *dir)
{
    int bad = 0;
    for (uint32_t d = 0; d < NDEMO; d++) {
        char path[512], name[PROJ_NAME_LEN + 1u], what[600];
        FILE *f;
        uint32_t len;
        snprintf(path, sizeof path, "%s/%s.tsp", dir, DEMOS[d].file);
        if (!(f = fopen(path, "rb"))) { bad += check(path, 0); continue; }
        len = (uint32_t)fread(rec, 1, sizeof rec, f);
        fclose(f);
        snprintf(what, sizeof what, "%s: restores through the backup page and loads", path);
        bad += check(what, restore_load(rec, len, name) == 0 && !strcmp(name, DEMOS[d].name));
        bad += report(DEMOS[d].name, (dm_play_t[]){play(20, 0)});
    }
    return bad;
}

/* ------------------------------------------------- the backup page's file --- */
/* A whole archive as the backup page writes it (web/fm1backup.js captureBackup: LIST, then GET of every object),
 * from this simulated device; and one read back and restored as restoreBackup does (each object's PUT begun and
 * aborted first, then the objects from 2 on, the settings, the music playing last). For tools/demo_backup.py's test */
static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static uint8_t obj_buf[40000];
static int archive_write(const char *path)
{
    FILE *f = fopen(path, "w");
    uint8_t list[3 + 30 * 11];
    uint32_t n, i, off;
    if (!f) return 1;
    call(ED_BACKUP_LIST, 0, 0);
    if (rep[1] || rep[2] != ED_BK_N) { fclose(f); return 2; }
    memcpy(list, rep, sizeof list);
    n = list[2];
    fprintf(f, "{\"format\":\"felucca-backup\",\"version\":1,\"firmware\":\"host\",\"created\":\"2026-10-22T00:00:00.000Z\",\"objects\":[");
    for (i = 0; i < n; i++) {
        const uint8_t *o = list + 3 + i * 11u;
        uint32_t id = o[0], len = ed_bk_r32(o + 1), crc = ed_bk_r32(o + 6), k;
        for (off = 0; off < len; off += 256u) {
            uint32_t c = len - off > 256u ? 256u : len - off;
            uint8_t a[8] = {(uint8_t)id};
            put32(a + 1, off); a[6] = (uint8_t)(c & 127u); a[7] = (uint8_t)(c >> 7);
            call(ED_BACKUP_GET, a, sizeof a);
            if (rep[1] || ed_unpack7(rep + 9u, rep_n - 9u, obj_buf + off, c) != c) { fclose(f); return 3; }
        }
        fprintf(f, "%s{\"id\":%u,\"size\":%u,\"crc\":%u,\"data\":\"", i ? "," : "", id, len, crc);
        for (k = 0; k < len; k += 3u) {
            uint32_t v = (uint32_t)obj_buf[k] << 16 | (k + 1u < len ? (uint32_t)obj_buf[k + 1u] << 8 : 0u) | (k + 2u < len ? obj_buf[k + 2u] : 0u);
            fputc(B64[v >> 18 & 63u], f); fputc(B64[v >> 12 & 63u], f);
            fputc(k + 1u < len ? B64[v >> 6 & 63u] : '=', f); fputc(k + 2u < len ? B64[v & 63u] : '=', f);
        }
        fprintf(f, "\"}");
    }
    fprintf(f, "]}\n");
    fclose(f);
    return 0;
}
static uint32_t b64_val(int c)
{
    const char *p = c ? strchr(B64, c) : 0;
    return p ? (uint32_t)(p - B64) : 0u;
}
static char *json;
/* object id's bytes from the archive in json -> obj_buf: its length, -1 not there */
static int32_t archive_obj(uint32_t id)
{
    char key[24];
    const char *p, *d;
    uint32_t n = 0;
    snprintf(key, sizeof key, "{\"id\":%u,", id);
    if (!(p = strstr(json, key)) || !(d = strstr(p, "\"data\":\""))) return -1;
    for (d += 8; d[0] && d[0] != '"'; d += 4) {
        uint32_t v = b64_val(d[0]) << 18 | b64_val(d[1]) << 12 | b64_val(d[2]) << 6 | b64_val(d[3]);
        obj_buf[n++] = (uint8_t)(v >> 16);
        if (d[2] != '=') obj_buf[n++] = (uint8_t)(v >> 8);
        if (d[3] != '=') obj_buf[n++] = (uint8_t)v;
    }
    return (int32_t)n;
}
static int archive_restore(const char *path)
{
    static const uint8_t ORDER[30] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
                                      24, 25, 26, 27, 28, 29, 1, 0};
    FILE *f = fopen(path, "rb");
    long sz;
    int bad = 0;
    if (!f) return check(path, 0);
    fseek(f, 0, SEEK_END); sz = ftell(f); fseek(f, 0, SEEK_SET);
    json = malloc((size_t)sz + 1u);
    if (fread(json, 1, (size_t)sz, f) != (size_t)sz) sz = 0;
    json[sz] = 0;
    fclose(f);
    power_on();
    for (uint32_t i = 0; i < 30u; i++) {                /* (sizes accepted before the first write) */
        int32_t n = archive_obj(i);
        uint8_t a[12] = {0, (uint8_t)i};
        if (n <= 0) continue;
        put32(a + 2, (uint32_t)n); put32(a + 7, st_crc32(obj_buf, (uint32_t)n));
        call(ED_BACKUP_PUT, a, sizeof a);
        if (rep[2]) { char w[80]; snprintf(w, sizeof w, "object %u accepted before any write (rc %u)", i, rep[2]); bad += check(w, 0); }
        a[0] = 3; call(ED_BACKUP_PUT, a, 2);
    }
    for (uint32_t i = 0; i < 30u; i++) {
        int32_t n = archive_obj(ORDER[i]);
        char w[80];
        if (n < 0) { snprintf(w, sizeof w, "object %u in the archive", ORDER[i]); bad += check(w, 0); continue; }
        snprintf(w, sizeof w, "object %u restored (%d B)", ORDER[i], n);
        bad += check(w, put_all(ORDER[i], obj_buf, (uint32_t)n) == 0);
    }
    memset(proj_meta, 0, sizeof proj_meta);              /* switched off and on: the slot list read from flash */
    for (uint32_t slot = 0; slot < 4u; slot++) proj_fetch(slot);
    for (uint32_t slot = 0; slot < 4u; slot++) {         /* what each slot holds now, loaded and played */
        char name[PROJ_NAME_LEN + 1u], w[120];
        if (!project_used(slot)) { printf("demos: slot %c empty\n", 'A' + slot); continue; }
        project_new_blank();
        project_load(slot);
        project_cur_name(name);
        snprintf(w, sizeof w, "slot %c: %s loads", 'A' + slot, name);
        bad += check(w, proj_cur == slot);
        bad += report(name, (dm_play_t[]){play(10, 0)});
    }
    free(json);
    return bad;
}

/* each track alone (the others' LEVEL 0) for secs s: its rms and peak, to balance the mix by */
static void levels(uint32_t secs)
{
    for (uint32_t d = 0; d < NDEMO; d++) {
        printf("%-12s", DEMOS[d].name);
        for (uint32_t k = 0; k < NTRK; k++) {
            power_on(); DEMOS[d].make();
            for (uint32_t j = 0; j < NTRK; j++) if (j != k) trk[j].p[P_LEVEL] = 0;
            dm_play_t p = play(secs, 0);
            printf("  T%u rms %.3f pk %.2f", k + 1u, p.rms, p.peak / 32768.0);
        }
        printf("\n");
        power_on(); DEMOS[d].make();
        memset(voice_kills_part, 0, sizeof voice_kills_part);
        dm_play_t p = play(secs, 0);
        printf("%-12s  taken %u: T1 %u T2 %u T3 %u T4 %u\n", "", p.taken, voice_kills_part[0], voice_kills_part[1], voice_kills_part[2], voice_kills_part[3]);
    }
}

int main(int argc, char **argv)
{
    int bad;
    if (argc >= 3 && !strcmp(argv[1], "make")) bad = make(argv[2], argc > 3 ? (uint32_t)atoi(argv[3]) : 60u);
    else if (argc >= 3 && !strcmp(argv[1], "check")) bad = check_dir(argv[2]);
    else if (argc >= 4 && !strcmp(argv[1], "log")) {     /* log DEMO SECS: its notes as CSV on stdout */
        dm_log = stdout; power_on(); DEMOS[atoi(argv[2])].make();
        if (getenv("DM_DEBUG")) for (uint32_t k = 0; k < NTRK; k++) fprintf(stderr, "T%u scl %d root %d qnt %d eng %d\n", k + 1u, trk[k].p[P_SCALE], trk[k].p[P_ROOT], trk[k].p[P_QUANT], trk[k].engine);
        play((uint32_t)atoi(argv[3]), 0); return 0;
    }
    else if (argc >= 3 && !strcmp(argv[1], "archive")) {   /* archive OUT.json: a new device's whole backup */
        power_on();
        bad = check("a whole archive written as the backup page writes it", archive_write(argv[2]) == 0);
    }
    else if (argc >= 3 && !strcmp(argv[1], "restore")) bad = archive_restore(argv[2]);
    else if (argc >= 2 && !strcmp(argv[1], "levels")) { levels(argc > 2 ? (uint32_t)atoi(argv[2]) : 60u); return 0; }
    else { fprintf(stderr, "demo_projects make DIR [SECS] | check DIR\n"); return 2; }
    printf(bad ? "demos: %d FAILED\n" : "demos passed\n", bad);
    return bad != 0;
}
