/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Firmware save flows against NOR flash: deferred settings, retries, and failed-save rollback. */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int32_t fm1_enc_take(uint32_t e) { (void)e; return 0; }
static uint32_t irq_start_race, irq_races;
static void fm1_irq_off(void)
{
    if (irq_start_race) {                         /* audio serviced PLAY immediately before main masks it */
        irq_start_race = 0;
        events_block(CTL);
        irq_races++;
    }
}
static void fm1_irq_on(void) {}
static uint32_t boot_syncs, boot_trace, boot_trace_bad;
static void lcd_sync(void) { boot_syncs++; }
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c) { (void)x; (void)y; (void)w; (void)h; (void)c; }
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{ (void)x; (void)y; (void)w; (void)h; (void)p; }
#define MELODEE_FLASH 1
#include "../firmware/src/gfx.c"
#include "../firmware/src/panel.c"
#include "../firmware/src/ui.c"
static void panel_setup(void) {}

static uint8_t nor[0x100000], flash_ok = 1;
static int erase_error, fail_after = -1;
static uint32_t erases, preset_reads;
static int st_read(uint32_t off, void *dst, uint32_t n) { preset_reads++; memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off)
{
    erases++;
    if (boot_trace && (!boot_syncs || boot_work.state != 1u || boot_work.done >= boot_work.total)) boot_trace_bad++;
    if (erase_error) return -8;
    memset(nor + off, 0xFF, 4096);
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    if (fail_after == 0) return -9;
    if (fail_after > 0) fail_after--;
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

enum { ED_UI_STATE = 34, ED_UI_SET, ED_UI_PALETTES, ED_FAV_GET, ED_FAV_SET };
static void ed_b(uint32_t n) { (void)n; }
static void ed_v(int32_t n) { (void)n; }
static int32_t ed_rv(const uint8_t *a) { return (int32_t)(a[0] | (uint32_t)a[1] << 7) - 8192; }
static void ed_str(const char *s, uint32_t n) { (void)s; (void)n; }
#include "../firmware/src/editor_preferences.c"

static int check(const char *what, int ok)
{
    printf("persist: %-66s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

static void reset(void)
{
    memset(nor, 0xFF, sizeof nor);
    memset(&song, 0, sizeof song);
    memset(trk, 0, sizeof trk);
    memset(&chain, 0, sizeof chain);
    chain_defaults(&chain_config);
    memset(proj_bank_len, 0, sizeof proj_bank_len);
    up_cache_reset(); native_cache_reset();
    memset(&persist_saved, 0, sizeof persist_saved);
    memset(&settings, 0, sizeof settings);
    memset(&ui, 0, sizeof ui);
    memset(&boot_work, 0, sizeof boot_work);
    memset(&favorites, 0, sizeof favorites);
    panel = PANEL_DEFAULT;
    host_tracks_init();
    fm1_ms = 0;
    transport_req = 0;
    persist_pending = persist_repair = 0;
    persist_old = tmpl_old = preset_old_write = 0;
    persist_retry_ms = 0;
    flash_ok = 1;
    erase_error = 0;
    fail_after = -1;
    up_gen = erases = 0;
    boot_syncs = boot_trace = boot_trace_bad = 0;
    irq_start_race = irq_races = 0;
}

#ifndef PERSISTENCE_TEST_NO_MAIN
static int staged_import_persistence(void)
{
    int bad=0;reset();memset(&sx_import,0,sizeof sx_import);fm6_rx_ready=0;
    fail_after=100000;native_put(ENGI_FM6,4,FM6_INIT);uint32_t writes=100000u-(uint32_t)fail_after;
    reset();for(uint32_t i=0;i<32;i++){memcpy(fm6_rx+6+i*FM6_PACKED,FM6_INIT,FM6_PACKED);fm6_rx[6+i*FM6_PACKED+118]=(uint8_t)('A'+i%26);}
    sysex_stage(ENGI_FM6,fm6_rx+6,32,FM6_PACKED);ui.uslot=4;fail_after=(int)writes;sysex_choose();
    bad+=check("partial SysEx write retains only unsaved tones for retry",sx_import.count==31&&sx_import.first==5&&sx_import.data==fm6_rx+6+FM6_PACKED&&fm6_rx_ready==2&&native_used(ENGI_FM6,4)&&!native_used(ENGI_FM6,5));
    native_cache_reset();bad+=check("committed import prefix survives cache reset and torn next slot",native_used(ENGI_FM6,4)&&native_raw(ENGI_FM6,4)[118]=='A'&&!native_used(ENGI_FM6,5));
    fail_after=-1;sysex_choose();
    bad+=check("retry starts after committed prefix and unlocks the receive buffer",!sx_import.count&&!fm6_rx_ready&&native_used(ENGI_FM6,35)&&native_raw(ENGI_FM6,5)[118]=='B'&&native_raw(ENGI_FM6,4)[118]=='A');
    return bad;
}
static int test_boot_maintenance_writes(void)
{
    int bad = 0;
    reset(); boot_trace = 1;
    persist_boot();
    bad += check("first boot flushes warning before settings and 4 FM6 bank erases",   /* (CZ-1 banks: retired) */
                 erases == 5u && !boot_trace_bad && boot_syncs >= 9u &&
                 !boot_work.state && boot_work.total == 4u && boot_work.done == 4u);
    uint32_t before = erases, flushed = boot_syncs;
    persist_boot();
    bad += check("completed first boot needs no more bank writes or progress screen",
                 erases == before && boot_syncs == flushed && !boot_work.state);
    reset(); persist_t prefs = {0}; settings_init(); settings_export(&prefs);
    st_save(OBJ_SETTINGS, &prefs, sizeof prefs); erases = 0;
    boot_trace = 1; fail_after = 30;               /* first three FM6 banks commit, the fourth fails */
    persist_boot();
    bad += check("failed first-boot write preserves completed banks and visible failure",
                 boot_work.state == 2u && boot_work.done == 3u && !boot_trace_bad &&
                 native_fm_valid(native_fm_bank(0)) && native_fm_valid(native_fm_bank(2)));
    fail_after = -1; before = erases; boot_syncs = 0;
    persist_boot();
    bad += check("restart retries only the unfinished bank",
                 !boot_work.state && boot_work.total == 1u && boot_work.done == 1u &&
                 erases == before + 1u && !boot_trace_bad && boot_syncs >= 3u);
    reset();
    return bad;
}

/* TSP1 (project_tsp.c): a step's RATCH and NUDGE, the generators and the cords come back with a slot; the largest
 * project fits a record; a part this build does not know is skipped and a missing one leaves its defaults; a damaged
 * part or checksum refuses the record; a rename keeps the music */
static int test_tsp_record(void)
{
    static uint8_t raw[TSP_MAX];
    char name[PROJ_NAME_LEN + 1u];
    int bad = 0, ok;
    uint32_t len, i, k, b;
    reset();
    trk[0].step[5] = (step_t){{62}, 1, ST_NOTE, 0, 90, 0, 0, 0, 0};
    step_set_ratch(&trk[0].step[5], 3); step_set_nudge(&trk[0].step[5], -6);
    trk[0].step[6] = trk[0].step[5]; step_set_ratch(&trk[0].step[6], 8);   /* (x8: RATCH's high bit) */
    song.sel = 1; tessera_knob(0, TESS_ENG_EUCLID);
    tessera_param_set(1, TX_SUB_SHAPE, SUB_DECEL); tessera_param_set(0, TX_SUB_SHAPE, SUB_ACCEL);   /* (the record's SUB part) */
    tessera_param_set(2, TX_MIDI_CH, 9); tessera_param_set(3, TX_MIDI_CH, (int32_t)TESS_MIDI_OFF);   /* (its MIDI part) */
    tessera_param_set(0, TX_KEY_LINK, 0);                                    /* (its KEY part: OWN, not the default) */
    tessera_param_set(1, TX_SD_SOURCE, 1); tessera_param_set(1, TX_SD_KEEP, TESS_SD_PITCH);   /* (its SEED part) */
    tessera.en_arc = 40u; tessera.en_rests = 60u;     /* (ENERGY: the block's project tail, TS18) */
    {
        cord_t c = {CS_LFO, 0, CD_CUTOFF, 1, 30, 0};
        cord_set(0, &c);
    }
    ok = !project_save_as(0, "TSP");
    memset(trk[0].step, 0, sizeof trk[0].step); tessera_reset();
    project_load(0);
    bad += check("TSP1: a step's RATCH and NUDGE, the generators and a cord come back with the slot", ok &&
                 step_ratch(&trk[0].step[5]) == 3u && step_nudge(&trk[0].step[5]) == -6 && trk[0].step[5].note[0] == 62u &&
                 step_ratch(&trk[0].step[6]) == 8u && step_nudge(&trk[0].step[6]) == -6 &&
                 tessera.mode[1] == TESS_ENG_EUCLID && cord_count() == 1u && cord_get(0)->amount == 30 &&
                 tessera.en_arc == 40u && tessera.en_rests == 60u &&
                 tessera.sub_shape[1] == SUB_DECEL && tessera.sub_shape[0] == SUB_ACCEL && tessera.sub_shape[2] == SUB_EVEN &&
                 tessera.midi_ch[2] == 9u && tessera.midi_ch[3] == TESS_MIDI_OFF && tessera.midi_ch[0] == 0u &&
                 tessera.key_link == 0u && tessera.sd_src[1] == 1u && tessera.sd_keep[1] == TESS_SD_PITCH &&
                 tessera.sd_src[0] == 0u && tessera.sd_keep[0] == TESS_SD_FREE);
    ok = project_rename(0, "TSP TWO") == 0;
    memset(trk[0].step, 0, sizeof trk[0].step);
    project_load(0);
    bad += check("TSP1: a rename keeps the music and the generators", ok && project_name(0, name) && !strcmp(name, "TSP TWO") &&
                 step_ratch(&trk[0].step[5]) == 3u && tessera.mode[1] == TESS_ENG_EUCLID);
    project_capture(&proj_scratch);
    len = tsp_pack(raw, &proj_scratch, 1);             /* (with the generators: the TESSERA part) */
    ok = len && tsp_valid(raw, len) && tsp_seen.at[TSP_TESSERA];
    raw[tsp_seen.at[TSP_TESSERA] + 1u + TESSERA_PK_BYTES - 2u] = 101u;   /* (ARC past 100) */
    tsp_resum(raw);
    bad += check("TSP1: a Tessera block whose ENERGY tail is out of range is refused", ok && !tsp_valid(raw, len));
    len = tsp_pack(raw, &proj_scratch, 1);
    ok = len && tsp_valid(raw, len) && tsp_seen.at[TSP_SUB];
    raw[tsp_seen.at[TSP_SUB] + 2u] = SUB_SHAPES;        /* (track 3's SHAPE past DECEL) */
    tsp_resum(raw);
    bad += check("TSP1: a SUB part with a SHAPE out of range is refused", ok && !tsp_valid(raw, len));
    len = tsp_pack(raw, &proj_scratch, 1);
    ok = len && tsp_valid(raw, len) && tsp_seen.at[TSP_MIDI];
    raw[tsp_seen.at[TSP_MIDI] + 1u] = (uint8_t)(TESS_MIDI_OFF + 1u);   /* (track 2's channel past OFF) */
    tsp_resum(raw);
    bad += check("TSP1: a MIDI part with a channel out of range is refused", ok && !tsp_valid(raw, len));
    len = tsp_pack(raw, &proj_scratch, 1);
    ok = len && tsp_valid(raw, len) && tsp_seen.at[TSP_KEY];
    raw[tsp_seen.at[TSP_KEY]] = 2u;                     /* (KEY LINK past LINKED) */
    tsp_resum(raw);
    bad += check("TSP1: a KEY part out of range is refused", ok && !tsp_valid(raw, len));
    len = tsp_pack(raw, &proj_scratch, 1);
    ok = len && tsp_valid(raw, len) && tsp_seen.at[TSP_SEED];
    raw[tsp_seen.at[TSP_SEED] + 2u] = (uint8_t)(TESSERA_TRACKS + 1u);   /* (track 3's SOURCE past T4) */
    tsp_resum(raw);
    ok &= !tsp_valid(raw, len);
    len = tsp_pack(raw, &proj_scratch, 1);
    ok &= len && tsp_valid(raw, len);
    raw[tsp_seen.at[TSP_SEED] + NTRK] = TESS_SD_KEEPS;   /* (track 1's KEEP past PITCH) */
    tsp_resum(raw);
    bad += check("TSP1: a SEED part with a SOURCE or a KEEP out of range is refused", ok && !tsp_valid(raw, len));

    reset();                                           /* HUMAN TIME: the record's HUMAN part; a record without it */
    song.sel = 1; tessera_knob(0, TESS_ENG_EUCLID);
    tessera_param_set(1, TX_TW_HUMAN, 60); tessera_param_set(0, TX_TW_HUMAN, 40);   /* (track 0: a STEP track) */
    tessera_param_set(2, TX_SUB_HUMAN, 45);
    project_capture(&proj_scratch);
    len = tsp_pack(raw, &proj_scratch, 1);
    ok = len && tsp_valid(raw, len) && tsp_seen.at[TSP_HUMAN];
    tessera_reset();
    ok = ok && !project_restore_runtime(&proj_scratch);
    tsp_restore(raw);
    bad += check("TSP1: HUMAN TIME comes back from its part, TWEAK HUMAN as set", ok && tessera.sub_human[2] == 45u &&
                 tessera.sub_human[1] == 0u && tessera.evolve[1].human == 60u && tessera.evolve[0].human == 40u);
    raw[tsp_seen.at[TSP_HUMAN] - 4u] = (uint8_t)(TSP_PARTS + 1u);   /* (its id one this build does not know: skipped) */
    tsp_resum(raw);
    tessera_reset();
    ok = tsp_valid(raw, len) && !tsp_seen.at[TSP_HUMAN] && !project_restore_runtime(&proj_scratch);
    tsp_restore(raw);
    bad += check("TSP1: a record without the HUMAN part (saved before it, or a part unknown) loads: a generator's TWEAK "
                 "HUMAN 60 times it 12 ms, a STEP track's HUMAN cleared",
                 ok && tessera.mode[1] != TESS_ENG_OFF && tessera.sub_human[1] == 12u && tessera.evolve[1].human == 60u &&
                 tessera.sub_human[0] == 0u && tessera.evolve[0].human == 0u && tessera.sub_human[2] == 0u);
    tessera_param_set(2, TX_SUB_HUMAN, 45);
    project_capture(&proj_scratch);
    len = tsp_pack(raw, &proj_scratch, 1);
    ok = len && tsp_valid(raw, len) && tsp_seen.at[TSP_HUMAN];
    raw[tsp_seen.at[TSP_HUMAN] + 3u] = (uint8_t)(TESS_HUMAN_MS + 1u);   /* (track 4's past 60 ms) */
    tsp_resum(raw);
    bad += check("TSP1: a HUMAN part past 60 ms is refused", ok && !tsp_valid(raw, len));

    reset();                                           /* the largest: every step of every pattern full, 1,024 notes */
    for (k = 0; k < NTRK; k++) {
        for (b = 0; b < NPAT; b++)
            for (i = 0; i < NSTEP; i++) {
                step_t s = {{(uint8_t)(40u + i), 60, 64, 67}, 4, ST_NOTE, SF_ACCENT, 100, 0, 0, 20, 0};
                step_set_ratch(&s, 4); step_set_nudge(&s, 7);
                if (b == trk[k].pattern) trk[k].step[i] = s; else pattern_at(k, b)->step[i] = s;
            }
        song.sel = (uint8_t)k; tessera_knob(0, TESS_ENG_TURING);
    }
    for (i = 0; i < RECORD_MAX; i++) recording[i] = (recorded_note_t){(uint16_t)(i * 61u), 1234, 60, 100, (uint8_t)(i % 32u), 3};
    recording_reindex();
    project_capture(&proj_scratch);
    len = tsp_pack(raw, &proj_scratch, 1);
    printf("  (the largest project: %u B of %u)\n", (unsigned)len, (unsigned)TSP_MAX);
    bad += check("TSP1: the largest project packs into a record and reads back", len && len <= TSP_MAX && tsp_valid(raw, len));

    reset();
    project_capture(&proj_scratch);
    len = tsp_pack(raw, &proj_scratch, 1);
    ok = tsp_valid(raw, len);
    raw[tsp_seen.at[TSP_NAME] - 4u] = 200u;            /* (the NAME part: an id this build does not know) */
    memcpy(raw + tsp_seen.at[TSP_NAME], "SKIPPED", 7);
    tsp_resum(raw);
    bad += check("TSP1: an unknown part is skipped, a missing one leaves its default", ok && tsp_valid(raw, len) &&
                 !proj_scratch.name[0] && !tsp_seen.at[TSP_NAME]);
    len = tsp_pack(raw, &proj_scratch, 1); tsp_valid(raw, len);
    raw[tsp_seen.at[TSP_TRACKS]] = 200u;               /* (more parameters than this build has) */
    tsp_resum(raw);
    ok = !tsp_valid(raw, len);
    len = tsp_pack(raw, &proj_scratch, 1);
    raw[len / 2u] ^= 1u;                               /* (a bit flipped: the checksum) */
    ok &= !tsp_valid(raw, len);
    len = tsp_pack(raw, &proj_scratch, 1);
    ok &= !tsp_valid(raw, len - 1u) && tsp_len(raw) == len;
    bad += check("TSP1: a damaged part, a flipped bit or a wrong length refuses the record", ok);
    reset();
    return bad;
}

int main(void)
{
    int bad = test_boot_maintenance_writes() + test_tsp_record(), ok;
    uint32_t before;
    persist_t p;
    static uint8_t old[TSP_MAX], keep[TSP_MAX];   /* (a slot's record as it was) */
    uint32_t oldlen, keeplen;
    static uint8_t got[TSP_MAX];
    up_bank_t bank;
    up_rec_t r;
    reset();
    settings_init();
    {
        const uint8_t amber[] = {0, 2};
        song.playing = 1;
        bad += check("editor display change while playing is applied and reports deferred saving",
                      ed_ui_set(amber, sizeof amber) == 4 && settings.palette == 2u && persist_pending == 1 && !erases);
        song.playing = 0;
        settings_poll();
        bad += check("STOP commits the deferred palette setting",
                      !persist_pending && erases == 1 && persist_saved.palette == palette_to_stored(2));
        before = erases;
        bad += check("repeating an unchanged editor preference succeeds without erasing",
                      ed_ui_set(amber, sizeof amber) == 0 && erases == before);
        erase_error = 1;
        const uint8_t mono[] = {0, 0};
        bad += check("editor reports failed saving while preserving the previous flash setting",
                      ed_ui_set(mono, sizeof mono) == 3 && settings.palette == 0u && persist_pending == 2 &&
                      persist_saved.palette == palette_to_stored(2));
        erase_error = 0;
        fm1_ms += 1000;
        settings_poll();
        bad += check("failed editor preference retries through the existing persistence path",
                      !persist_pending && persist_saved.palette == palette_to_stored(0));
        const uint8_t bold[] = {1, 1};
        bad += check("the retired font weight is refused without a write", ed_ui_set(bold, sizeof bold) == 2);
    }
    reset();
    settings.palette = 1;
    settings.lowcut = 2;
    settings.zoom = 1;
    persist_saved.bold = 1;                       /* (an earlier firmware's font weight: kept as saved) */
    favorites.factory[8][0] = 1;
    favorites.user = 1u << 31;
    song.playing = 1;
    settings_save();
    bad += check("playing: settings queued, no erase, runtime kept", persist_pending && !erases &&
                  settings.palette == 1u && settings.lowcut == 2u && settings.zoom == 1u);
    song.playing = 0;
    transport_req = 1;
    settings_poll();
    bad += check("pending PLAY: settings still queued, no erase", persist_pending && !erases);
    transport_req = 0;
    chain.armed = 1;
    settings_poll();
    bad += check("pending SONG: settings still queued, no erase", persist_pending && !erases);
    chain.armed = 0;
    settings.palette = 2;
    settings_poll();
    bad += check("STOP: latest settings saved and request cleared", !persist_pending && erases == 1u &&
                  st_load(OBJ_SETTINGS, &p, sizeof p) == (int)sizeof p && p.palette == palette_to_stored(2) &&
                  p.lowcut == 0u && p.zoom == 1u && p.bold == 1u &&
                  p.favorites.factory[8][0] == 1 && p.favorites.user == (1u << 31) &&
                  !memcmp(&p.panel, &panel, sizeof panel));
    settings_save();
    bad += check("unchanged settings do not erase", !persist_pending && erases == 1u);
    settings.palette = 1;
    erase_error = 1;
    fm1_ms = 0xFFFFFF00u;
    settings_save();
    before = erases;
    settings_poll();
    fm1_ms += 999u;
    settings_poll();
    bad += check("failed settings retry waits through clock wrap", persist_pending && erases == before &&
                  persist_saved.palette == palette_to_stored(2) && settings.palette == 1u);
    erase_error = 0;
    fm1_ms++;
    settings_poll();
    bad += check("settings retry succeeds after 1000 ms", !persist_pending && erases == before + 1u &&
                  st_load(OBJ_SETTINGS, &p, sizeof p) == (int)sizeof p && p.palette == palette_to_stored(1));

    reset();
    trk[0].step[0] = (step_t){{60}, 1, ST_NOTE, 0, 96, 0, 0};
    project_save(0);
    oldlen = proj_bank_len[0]; memcpy(old, proj_bank_slot[0], oldlen);
    trk[0].step[0].note[0] = 72;
    fail_after = 1;
    project_save(0);
    fail_after = -1;
    bad += check("failed project save keeps RAM and old flash", !strcmp(ui.msg, "SAVE ERROR") &&
                  proj_bank_len[0] == oldlen && !memcmp(proj_bank_slot[0], old, oldlen) &&
                  st_load(OBJ_SLOT(0), got, sizeof got) == (int)oldlen && !memcmp(got, old, oldlen));
    project_save(0);
    bad += check("successful project save publishes new RAM", !strcmp(ui.msg, "SAVED") &&
                  tsp_valid(proj_bank_slot[0], proj_bank_len[0]) &&
                  proj_scratch.t[0].step[0].note[0] == 72u && proj_ok(&proj_scratch));
    before = erases;
    transport_req = 1;
    project_save(1);
    bad += check("pending PLAY: project save leaves slot and flash", !project_used(1) && erases == before &&
                  !strcmp(ui.msg, "STOP TO SAVE"));
    transport_req = 0;
    erase_error = 1;
    project_save(2);
    bad += check("failed first project save leaves slot empty", !project_used(2));
    erase_error = 0;
    {   /* names: a project's through flash; a failed rename keeps the old one in RAM and flash */
        char n[16];
        step_t st0 = trk[0].step[0];
        project_save_as(0, "LOFI JAM");
        memset(proj_bank_slot[0], 0, 8);                       /* (the RAM copy lost: read it back from flash) */
        proj_fetch(0);
        bad += check("a project's name goes to flash and comes back", project_name(0, n) && !strcmp(n, "LOFI JAM") &&
                      !strcmp(proj_name, "LOFI JAM"));
        keeplen = proj_bank_len[0]; memcpy(keep, proj_bank_slot[0], keeplen);
        fail_after = 1;
        bad += check("failed project rename: SAVE ERROR, the slot and flash as they were",
                      project_rename(0, "DUB") == 2 && !strcmp(ui.msg, "SAVE ERROR") &&
                      st_load(OBJ_SLOT(0), got, sizeof got) == (int)keeplen && !memcmp(got, keep, keeplen));
        fail_after = -1;
        before = erases;
        transport_req = 1;
        bad += check("pending PLAY: no rename, no erase", project_rename(0, "DUB") == 1 && erases == before &&
                      !strcmp(ui.msg, "STOP TO SAVE"));
        transport_req = 0;
        bad += check("project rename: flash has the new name, the music as it was", project_rename(0, "DUB") == 0 &&
                      !strcmp(ui.msg, "RENAMED") && (memset(proj_bank_slot[0], 0, 8), proj_fetch(0), project_name(0, n)) &&
                      !strcmp(n, "DUB") && proj_scratch.t[0].step[0].note[0] == st0.note[0]);
    }
    {   /* CLK TUNE MIDI ROUT are the device's: a project saved with others loads without changing them; its BPM loads */
        song.g[G_CLOCK] = 1; song.g[G_TUNE] = 9; song.g[G_MIDI] = (int16_t)GP[G_MIDI].max; song.g[G_ROUTE] = 2;
        song.g[G_BPM] = 133;
        project_save_as(3, "SYNC");
        song.g[G_CLOCK] = 0; song.g[G_TUNE] = -4; song.g[G_MIDI] = (int16_t)GP[G_MIDI].min; song.g[G_ROUTE] = 0;
        song.g[G_BPM] = 90;
        project_load(3);
        bad += check("project load keeps the device's CLK TUNE MIDI ROUT, takes the project's BPM",
                     song.g[G_CLOCK] == 0 && song.g[G_TUNE] == -4 && song.g[G_MIDI] == GP[G_MIDI].min &&
                     song.g[G_ROUTE] == 0 && song.g[G_BPM] == 133);
    }
    {   /* All inactive banks are validated before replacing the runtime. */
        step_t keep = trk[0].step[0];
        project_capture(&proj_scratch);
        uint32_t len = tsp_pack(got, &proj_scratch, 1);
        tsp_valid(got, len);                          /* (track 1's second pattern, its first step: kind 15) */
        got[tsp_seen.at[TSP_STEPS] + NTRK + NSTEP * TSP_STEP_BYTES + 3u] |= 0xF0; tsp_resum(got);
        st_save(OBJ_SLOT(0), got, len);
        project_load(0);
        bad += check("damaged inactive-bank chord is refused with runtime unchanged", !strcmp(ui.msg,"OLD FORMAT") &&
            !memcmp(&trk[0].step[0],&keep,sizeof keep));
    }
    {   /* every parameter range fits the record's byte (FUN16: value + 64, 0..191) */
        uint32_t i;
        for (i = 0; i < P_COUNT; i++) {
            uint32_t e;
            for (e = 0; e < NENGINES; e++)
                if (param_desc_of(e, i)->min < -64 || param_desc_of(e, i)->max > 127) {
                    printf("param %u engine %u: range %d..%d does not fit FUN7's byte\n", i, e,
                           param_desc_of(e, i)->min, param_desc_of(e, i)->max);
                    bad++;
                }
        }
    }
    chain_config.count = 1;
    chain_config.row[0] = (chain_row_t){3, 1};
    bad += check("SONG sources use the same step bounds", chain_prepare() == 0 &&
                  !memcmp(&pattern_at(0, 0)->step[0], &trk[0].step[0], sizeof(step_t)));
    seq_stop();
    transport_req = 0;
    memset(&r, 0, sizeof r);
    r.used = UP_USED;
    r.ver = UP_VER;
    r.np = P_COUNT;
    memcpy(r.name, "First", 6);
    erase_error = 1;
    bank = (*up_cache_bank(1));
    ok = up_put(17, &r) == 2;
    erase_error = 0;
    bad += check("failed first preset save restores empty bank", ok && !memcmp(up_cache_bank(1), &bank, sizeof bank));
    bad += check("preset initial save", up_put(3, &r) == 0 && up_used(3));
    bank = (*up_cache_bank(0));
    before = up_gen;
    r.packed[0] = 77;
    fail_after = 1;
    ok = up_put(3, &r) == 2;
    fail_after = -1;
    bad += check("failed preset save rolls back bank and generation", ok && up_gen == before &&
                  !memcmp(up_cache_bank(0), &bank, sizeof bank));
    favorite_set(USER_GENERAL, 3, 1);
    fail_after = 1;
    ok = up_put(3, 0) == 2;
    fail_after = -1;
    bad += check("failed preset erasure retains both the sound and its favorite", ok && up_used(3) &&
                  favorite_has(USER_GENERAL, 3) && !memcmp(up_cache_bank(0), &bank, sizeof bank));
    memset(up_cache_bank(0), 0, sizeof (*up_cache_bank(0)));
    up_boot();
    bad += check("failed preset save keeps old flash", !memcmp(up_cache_bank(0), &bank, sizeof bank));
    trk[0].user = 4;
    erase_error = 1;
    ok = up_put(3, 0) == 2;
    erase_error = 0;
    bad += check("failed preset erase keeps record and loaded label", ok && trk[0].user == 4u && up_gen == before &&
                  !memcmp(up_cache_bank(0), &bank, sizeof bank));
    bad += check("successful preset erase clears loaded label", up_put(3, 0) == 0 && !up_used(3) && !trk[0].user);
    before = erases;
    transport_req = 1;
    bad += check("pending PLAY: preset save blocked", up_put(3, &r) == 2 && erases == before && !up_used(3));
    transport_req = 0;
    chain.armed = 1;
    bad += check("pending SONG: preset save blocked", up_put(3, &r) == 2 && erases == before && !up_used(3));
    chain.armed = 0;
    bad += check("preset invalid slot blocked before access", up_put(UP_SLOTS, &r) == 1 && erases == before);
    song.playing = 0; transport_req = 1; irq_start_race = 1;
    bad += check("PLAY consumed before the save snapshot still blocks a project write",
                  project_save(1) != 0 && irq_races == 1u && song.playing && !transport_req && erases == before);
    song.playing = 0; transport_req = 1; irq_start_race = 1;
    bad += check("PLAY consumed before the preset snapshot still blocks a bank write",
                  up_put(3, &r) == 2 && irq_races == 2u && song.playing && !transport_req && erases == before);
    song.playing = 0; transport_req = 1; irq_start_race = 1;
    settings.palette = 1;
    settings_save();
    bad += check("PLAY consumed before the settings snapshot defers the write",
                  irq_races == 3u && song.playing && !transport_req && persist_pending && erases == before);
    bad += staged_import_persistence();
    printf("%s\n", bad ? "PERSISTENCE TEST FAILED" : "persistence test passed");
    return bad != 0;
}

#endif
