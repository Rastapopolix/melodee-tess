/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* The engines left out of the build (phase 4, tessera-notes/docs/design/phase4/PLAN.md): FM6, CZ-1 and Prophet off. Never
 * offered; a project's track of one loads SID's INIT with its common settings; the power-on sounds are built ones;
 * the parts play. Built with -DMELODEE_FM6=0 -DMELODEE_CZ=0 -DMELODEE_PROPHET=0 (run_tests.sh). */
#if MELODEE_FM6 || MELODEE_CZ || MELODEE_PROPHET
#error "engine_switch_test: build it with the three engines off"
#endif
static unsigned char host_samples[3][0x14000];
#define SMP_USER_XIP(k) host_samples[k]
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
static int erase_error;
static uint32_t erases;
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { if (erase_error) return -8; memset(nor + off, 0xFF, 4096); erases++; return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n) { memcpy(nor + off, src, n); return 0; }
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0; }
static void fl_plain_window_init(void) {}
#define FL_FAR(fn) (fn)
#include "../firmware/src/storage.c"
#include "../firmware/src/upreset.c"
#include "../firmware/src/project.c"

static int bad;
static void check(const char *what, int ok)
{
    printf("engines off: %-70s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static project_t p;

int main(void)
{
    static const uint32_t GONE[3] = {12u, 15u, 19u};
    uint32_t k, shown = 0, ok = 1;
    for (k = 0; k < NENGINES; k++) shown += eng_ok(k);
    check("FM6, CZ-1 and Prophet never offered; SID, TRIO, PHYS and DRUM are", !eng_ok(12) && !eng_ok(15) && !eng_ok(19) &&
          eng_ok(ENGI_SID) && eng_ok(6) && eng_ok(ENGI_PHYS) && eng_ok(ENGI_DRUM) && shown == NENG_SHOWN && NENG_SHOWN == 4u);
    for (k = 0; k < NENG_SHOWN; k++) ok &= ENG_BUILT(eng_vis(k));
    for (k = 0; k < NPART; k++) ok &= ENG_BUILT(TRK_DEF[k][0]) && eng_ok(TRK_DEF[k][0]);
    check("the engine list and the power-on sounds: built engines only", ok);

    host_tracks_init();
    for (k = 0; k < NPART; k++) host_power_on_sound(&trk[k], k);
    project_capture(&p);
    for (k = 0; k < 3u; k++) {                    /* (a project saved with them) */
        p.t[k].engine = (uint8_t)GONE[k];
        p.t[k].preset = 7;
        p.t[k].p[P_ATK] = (int16_t)(20 + k);
        p.t[k].p[P_E0] = 99;
        p.t[k].p[P_E0 + 3u] = -40;
    }
    p.sum = proj_sum(&p);
    check("(the project with them taken: a valid record)", !project_restore_runtime(&p));
    for (k = 0, ok = 1; k < 3u; k++) {
        uint32_t i;
        ok &= trk[k].eng_req == ENGI_SID && trk[k].preset == 0 && trk[k].p[P_ATK] == (int16_t)(20 + k);
        for (i = 0; i < 8u; i++) ok &= trk[k].p[P_E0 + i] == ENGINES[ENGI_SID]->edit[i].def;
    }
    check("a project's FM6, CZ-1, Prophet tracks: SID INIT, its common settings kept (ATTACK)", ok);
    check("...the other track as saved (DRUM)", trk[3].eng_req == ENGI_DRUM);
    {
        int32_t o[2 * CTL];
        uint32_t f;
        double sum = 0;
        for (f = 0; f < FS / 10u; f += CTL) mix_block(o, CTL);   /* (the load's panic and engine switch first) */
        for (k = 0; k < 3u; k++) trk_note_on(&trk[k], 60u + 4u * k, 100);
        for (f = 0; f < FS; f += CTL) {
            uint32_t i;
            mix_block(o, CTL);
            for (i = 0; i < 2u * CTL; i++) sum += o[i] < 0 ? -(double)o[i] : (double)o[i];
        }
        check("...they play (a second of three notes: sound, finite)", sum > 0 && sum < 1e15);
    }
    printf(bad ? "engines off: %d FAILED\n" : "engines off: all passed\n", bad);
    return bad != 0;
}
