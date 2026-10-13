/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments; Theo Dainis (melodee-tess) */
/* A project saved by an older build of the fork, loaded by this one (after an upstream merge): the music and the
 * Tessera state (OBJ_TESSERA, tagged with the project record's hash) must come back as saved. The same source builds
 * in both trees (the harness of tests/persistence_test.c):
 *   upgrade_test save OUT.nor    the older build: a project with a generator on each track, EVOLVE and FORM, saved
 *                                in slot 1, the whole NOR written out
 *   upgrade_test load IN.nor     this build: boot from that NOR (persist_boot), load slot 1, compare
 *   upgrade_test                 both in this build (a round trip; run_tests.sh)
 * tests/upgrade_check.sh builds the save side from another commit (default tessera-dev) and runs both. */
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
static uint32_t erases;
static int st_erase(uint32_t off) { erases++; memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    for (i = 0; i < n; i++) nor[off + i] &= s[i];
    return 0;
}
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }   /* (persist_boot: the expected part) */
static void fl_plain_window_init(void) {}
#define FL_FAR(fn) (fn)
#include "../firmware/src/storage.c"
#include "../firmware/src/upreset.c"
#include "../firmware/src/project.c"

static int fail;
static void check(const char *what, int ok)
{
    printf("upgrade: %-66s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok) fail = 1;
}

/* power-on, as far as the save flows need it */
static void power_on(void)
{
    memset(&song, 0, sizeof song);
    memset(trk, 0, sizeof trk);
    memset(&chain, 0, sizeof chain);
    chain_defaults(&chain_config);
    memset(proj_slot, 0, sizeof proj_slot);
    up_cache_reset(); native_cache_reset();
    memset(&settings, 0, sizeof settings);
    memset(&ui, 0, sizeof ui);
    panel = PANEL_DEFAULT;
    host_tracks_init();
    tessera_reset();
    fm1_ms = 0;
    transport_req = 0;
    flash_ok = 1;
}

/* the music: a tempo, a step on track 1, a generator on each track, EVOLVE on track 1, a FORM on track 3 */
static void music(void)
{
    static const uint8_t GEN[4] = {TESS_ENG_TURING, TESS_ENG_EUCLID, TESS_ENG_MARKOV, TESS_ENG_MORPH};
    uint32_t tr;
    song.g[G_BPM] = 97;
    trk[0].step[0].n = 1;
    trk[0].step[0].note[0] = 62;
    trk[0].step[0].time = ST_NOTE;
    trk[0].step[0].vel = 100;
    for (tr = 0; tr < 4u; tr++) {
        song.sel = (uint8_t)tr;
        tessera_knob(0, GEN[tr]);
    }
    song.sel = 0;
    tessera_knob(1, -5);                          /* Turing LENGTH 11 */
    tessera_evolve_knob(1, 1);                    /* EVERY 1 bar, then 2 */
    tessera_evolve_knob(1, 1);
    tessera_evolve_knob(2, -25);                  /* AMOUNT */
    song.sel = 2;
    tessera_form_knob(0, 1);                      /* FORM: the first form */
    tessera_form_knob(1, 15);                     /* CONTRAST */
    song.sel = 1;
}

static uint8_t want[TESSERA_PK_BYTES], got[TESSERA_PK_BYTES];

static int save(const char *path)
{
    FILE *f;
    power_on();
    memset(nor, 0xFF, sizeof nor);
    music();
    tessera_pack(want);
    if (project_save_as(0, "UPGRADE")) { printf("upgrade: the save failed\n"); return 1; }
    if (!(f = fopen(path, "wb")) || fwrite(nor, 1, sizeof nor, f) != sizeof nor) { printf("upgrade: cannot write %s\n", path); return 1; }
    fclose(f);
    printf("upgrade: saved slot 1 (a generator on each track) to %s\n", path);
    return 0;
}

static int load(const char *path)
{
    FILE *f;
    uint32_t i, same = 1;
    power_on();                                   /* what the older build saved: the same music, packed here */
    music();
    tessera_pack(want);
    power_on();
    if (path) {
        if (!(f = fopen(path, "rb")) || fread(nor, 1, sizeof nor, f) != sizeof nor) { printf("upgrade: cannot read %s\n", path); return 1; }
        fclose(f);
    }
    memset(proj_slot, 0, sizeof proj_slot);       /* the power-off: only the flash is left */
    persist_boot();
    check("the slot is listed after boot", project_used(0));
    project_load(0);
    check("the project loads (not EMPTY SLOT / OLD FORMAT)", proj_cur == 0u);
    check("its tempo and its step come back", song.g[G_BPM] == 97 && trk[0].step[0].n == 1u && trk[0].step[0].note[0] == 62u);
    check("its name comes back", !memcmp(proj_name, "UPGRADE", 7));
    check("Tessera: a generator on each track again", tessera.mode[0] == TESS_ENG_TURING &&
          tessera.mode[1] == TESS_ENG_EUCLID && tessera.mode[2] == TESS_ENG_MARKOV && tessera.mode[3] == TESS_ENG_MORPH);
    tessera_pack(got);
    for (i = 0; i < TESSERA_PK_BYTES; i++) same &= want[i] == got[i];
    check("Tessera: the saved block equal byte for byte (EVOLVE, FORM, every setting)", same);
    return fail;
}

/* autosave (stopped, untouched): a change of the generators alone must reach the flash too */
static int autosave(void)
{
    uint8_t edited[TESSERA_PK_BYTES];
    uint32_t i, same = 1;
    power_on();
    memset(nor, 0xFF, sizeof nor);
    music();
    song.sel = 0;                                 /* (the selected track is in the record: chosen before the save) */
    if (project_save_as(0, "UPGRADE")) { printf("upgrade: the save failed\n"); return 1; }
    tessera_knob(2, 30);                          /* only Turing's PROB turned: the project record is unchanged */
    tessera_pack(edited);
    project_autosave();
    power_on();                                   /* power-off, power-on, load */
    memset(proj_slot, 0, sizeof proj_slot);
    persist_boot();
    project_load(0);
    tessera_pack(got);
    for (i = 0; i < TESSERA_PK_BYTES; i++) same &= edited[i] == got[i];
    check("autosave keeps a change of the generators alone (PROB turned)", same);
    erases = 0;
    project_autosave();
    check("autosave after a load, nothing changed: no flash written", erases == 0u);
    return fail;
}

int main(int argc, char **argv)
{
    if (argc == 3 && !strcmp(argv[1], "save")) return save(argv[2]);
    if (argc == 3 && !strcmp(argv[1], "load")) return load(argv[2]);
    if (argc == 1) {                              /* a round trip in this build */
        static const char tmp[] = "build/host/upgrade_self.nor";
        return save(tmp) || load(tmp) || autosave();
    }
    printf("usage: upgrade_test [save OUT.nor | load IN.nor]\n");
    return 2;
}
