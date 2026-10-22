/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The editor's full backup (editor_backup.c: LIST / GET / PUT) against simulated NOR flash:
 * CRC before any write, stale runtime copies, USB resets and timeouts, malformed objects, older
 * project formats, settings values and user preset banks kept byte for byte. */
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

static int check(const char *what, int ok)
{
    printf("backup: %-72s %s\n", what, ok ? "ok" : "FAIL");
    return !ok;
}

static void reset(void)
{
    memset(nor, 0xFF, sizeof nor);
    memset(&song, 0, sizeof song);
    memset(trk, 0, sizeof trk);
    memset(&chain, 0, sizeof chain);
    chain_defaults(&chain_config);
    pattern_init();
    memset(proj_slot, 0, sizeof proj_slot);
    up_cache_reset(); native_cache_reset();
    memset(&persist_saved, 0, sizeof persist_saved);
    memset(&settings, 0, sizeof settings);
    memset(&ui, 0, sizeof ui);
    panel = PANEL_DEFAULT;
    settings_init();
    host_tracks_init();
    fm1_ms = 0;
    transport_req = 0;
    usb.resets = 0;
    erase_error = 0;
    erases = 0;
    ed_bk_valid = ed_bk_put = 0;
    persist_old = tmpl_old = preset_old_write = 0;
    tessera_reset();
}

static uint32_t call(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    rep_n = 0;
    ed_backup_handle(cmd, a, n);
    return rep_n;
}
static void put32(uint8_t *a, uint32_t v) { for (uint32_t i = 0; i < 5u; i++) a[i] = (uint8_t)((v >> (7u * i)) & 127u); }

/* PUT begin: rc */
static uint32_t put_begin(uint32_t id, uint32_t len, uint32_t crc)
{
    uint8_t a[12] = {0, (uint8_t)id};
    put32(a + 2, len);
    put32(a + 7, crc);
    call(ED_BACKUP_PUT, a, sizeof a);
    return rep[2];
}
/* PUT one chunk of data at off: rc */
static uint32_t put_chunk(uint32_t id, uint32_t off, const uint8_t *p, uint32_t n)
{
    uint8_t a[16 + 300];
    uint32_t k = 7;
    a[0] = 1;
    a[1] = (uint8_t)id;
    put32(a + 2, off);
    while (n) {
        uint32_t g = n > 7u ? 7u : n, m = 0, i;
        for (i = 0; i < g; i++) m |= (uint32_t)(p[i] >> 7) << i;
        a[k++] = (uint8_t)m;
        for (i = 0; i < g; i++) a[k++] = p[i] & 127u;
        p += g;
        n -= g;
    }
    call(ED_BACKUP_PUT, a, k);
    return rep[2];
}
static uint32_t put_end(uint32_t id, uint32_t op)   /* op 2 commit, 3 abort */
{
    uint8_t a[2] = {(uint8_t)op, (uint8_t)id};
    call(ED_BACKUP_PUT, a, 2);
    return rep[2];
}
/* a whole object: begin, 256-byte chunks, commit; the first failing rc or the commit's */
static uint32_t put_all(uint32_t id, const void *v, uint32_t len, uint32_t crc)
{
    const uint8_t *p = v;
    uint32_t off, rc = put_begin(id, len, crc);
    for (off = 0; !rc && off < len; off += 256u)
        rc = put_chunk(id, off, p + off, len - off > 256u ? 256u : len - off);
    return rc ? rc : put_end(id, 2);
}
/* LIST: rc; the length and CRC of object id */
static uint32_t list(uint32_t id, uint32_t *len, uint32_t *crc)
{
    uint32_t i;
    call(ED_BACKUP_LIST, 0, 0);
    if (rep[1])
        return rep[1];
    for (i = 0; i < rep[2]; i++) {
        const uint8_t *o = rep + 3 + i * 11u;
        if (o[0] == id) {
            *len = ed_bk_r32(o + 1);
            *crc = ed_bk_r32(o + 6);
        }
    }
    return 0;
}
static uint32_t get(uint32_t id, uint32_t off, uint32_t count)
{
    uint8_t a[8] = {(uint8_t)id};
    put32(a + 1, off);
    a[6] = (uint8_t)(count & 127u);
    a[7] = (uint8_t)(count >> 7);
    call(ED_BACKUP_GET, a, sizeof a);
    return rep[1];
}

static int full_pattern_archive(void)
{
    int bad=0, ok=1; reset(); static uint8_t archive[TSP_MAX], saved[TSP_MAX];
    for (uint32_t k=0; k<NTRK; k++) for (uint32_t b=0; b<NPAT; b++) {
        track_t *t=&trk[k]; pattern_request(t,b); t->p[P_SLEN]=(int16_t)(16+b);
        t->step[3]=(step_t){{(uint8_t)(40+k*8+b),60,64,67},4,ST_NOTE,SF_ACCENT,100,0x81,0x80,(uint8_t)(20+b)};
        t->step[4]=(step_t){{0},0,ST_TIE}; motion_set_event(t,3,P_PAN,(int16_t)(k*8+b));
    }
    chain_config.count=1; chain_config.row[0]=(chain_row_t){0,2};
    for (uint32_t k=0; k<NTRK; k++) chain_patterns[0][k]=(uint8_t)(k+1);
    project_save_as(0,"CHORD BANKS");
    uint32_t len,crc; list(0,&len,&crc); memcpy(archive,ED_BK_RAW,len);
    bad+=check("full bank archive contains all runtime banks with its verified CRC",len && len==tsp_len(archive) && crc==st_crc32(archive,len));
    for (uint32_t off=0; off<len; off+=256u) {
        cv_begin(240,124,RGB(off&255u,20,30));cv_blit(0,24);
        uint32_t n=len-off>256u?256u:len-off;
        ok &= get(2,off,n)==0;
        ok &= ed_unpack7(rep+9u,rep_n-9u,saved+off,n)==n;
    }
    /* GET emits pack7; the XIP payload itself remains byte-identical after other object reads. */
    ok &= !memcmp(saved,archive,len) && (st_load(OBJ_SLOT(0),saved,sizeof saved)==(int)len && !memcmp(saved,archive,len)) && get(0,0,64)==0;
    bad+=check("saved-bank GET and interleaved screen draws preserve the frozen runtime snapshot",ok);
    bad+=check("full archive restores a saved slot through the A/B path",put_all(3,archive,len,st_crc32(archive,len))==0);
    pattern_init(); memset(&motion,0,sizeof motion); memset(proj_meta,0,sizeof proj_meta); memset(proj_slot,0,sizeof proj_slot);
    proj_fetch(1); project_load(1); ok=1;
    for (uint32_t k=0; k<NTRK; k++) for (uint32_t b=0; b<NPAT; b++) {
        track_t *t=&trk[k]; pattern_request(t,b);
        ok &= t->step[3].n==4 && t->step[3].note[0]==40+k*8+b && t->step[3].note[3]==67 &&
            t->step[4].time==ST_TIE && t->p[P_SLEN]==16+(int16_t)b && step_chance(&t->step[3])==20+b && motion_count(t)==1;
    }
    bad+=check("cold flash load restores all 32 chords, ties, timing, chance and motion banks",ok && chain_patterns[0][3]==4);
    bad+=check("full archive restores the runtime with its original active banks",put_all(0,archive,len,st_crc32(archive,len))==0 && trk[0].pattern==7 && trk[3].pattern==7);
    return bad;
}
static int native_archive(void)
{
    reset();up_boot();int ok=1;uint8_t fm[FM6_PACKED],cz[CZ_BYTES];
    const uint32_t slots[4]={0,16,32,63},ids[4]={21,22,19,20};static native_fm_t saved[4];static cz_bank_t savedcz;
    for(uint32_t b=0;b<4;b++){fm6_factory(b,fm);ok &= !native_put(ENGI_FM6,slots[b],fm);saved[b]=(*native_fm_bank(b));}
    cz_patch_init(cz);cz[128]='Z';ok &= !native_put(ENGI_CZ,127,cz);savedcz=(*native_cz_bank(7));
    for(uint32_t b=0;b<4;b++){uint32_t len;const uint8_t *raw=ed_bk_object(ids[b],&len);ok &= len==sizeof saved[b] && !memcmp(raw,&saved[b],len);ok &= !native_put(ENGI_FM6,slots[b],0);}
    ok &= !native_put(ENGI_CZ,127,0);
    for(uint32_t b=0;b<4;b++)ok &= !put_all(ids[b],(uint8_t *)&saved[b],sizeof saved[b],st_crc32(&saved[b],sizeof saved[b]));
    ok &= !put_all(16,(uint8_t *)&savedcz,sizeof savedcz,st_crc32(&savedcz,sizeof savedcz));
    native_cache_reset();cz_bank_cached=255;up_boot();
    for(uint32_t b=0;b<4;b++)ok &= !memcmp(native_fm_bank(b),&saved[b],sizeof saved[b]);
    ok &= native_used(ENGI_CZ,127) && !memcmp(native_raw(ENGI_CZ,127),cz,sizeof cz);
    return check("all four native FM6 objects and Z128 restore exactly after reboot",ok);
}
static int prophet_archive(void)
{
    reset();up_boot();int bad=0,ok=1;static p5_bank_t saved[5],readback;p5_patch_t patch;
    p5_patch_init(&patch);patch.raw[97]=255;patch.raw[98]=255;
    for(uint32_t b=0;b<5;b++){uint32_t slot=b==4?127:b*26;patch.raw[84]=(uint8_t)('A'+b);ok &= !native_put(ENGI_PROPHET,slot,(const uint8_t *)&patch);ok &= favorite_set(USER_NATIVE_P5,slot,1);saved[b]=*p5_user_bank(b);}
    for(uint32_t b=0;b<5;b++){uint32_t len;const uint8_t *raw=ed_bk_object(23+b,&len);ok &= len==sizeof readback&&!memcmp(raw,&saved[b],len);ok &= !put_all(23+b,0,0,0);ok &= !put_all(23+b,&saved[b],sizeof readback,st_crc32(&saved[b],sizeof readback));}
    native_cache_reset();for(uint32_t b=0;b<5;b++)ok &= !memcmp(p5_user_bank(b),&saved[b],sizeof readback);
    bad+=check("all five Prophet backup objects restore patches, names, metadata and stars exactly",ok);
    uint32_t before=erases;readback=saved[4];readback.used|=1u<<24;
    bad+=check("out-of-range Prophet slot masks are rejected before an erase",put_all(27,&readback,sizeof readback,st_crc32(&readback,sizeof readback))==2&&erases==before);
    readback=saved[4];readback.patch[23].model=99;
    bad+=check("invalid Prophet wire metadata is rejected before an erase",put_all(27,&readback,sizeof readback,st_crc32(&readback,sizeof readback))==2&&erases==before);
    readback=saved[4];erase_error=1;
    bad+=check("failed Prophet backup commit keeps the previous cached and flash bank",put_all(27,&readback,sizeof readback,st_crc32(&readback,sizeof readback))==4&&!memcmp(p5_user_bank(4),&saved[4],sizeof readback));erase_error=0;
    return bad;
}
static int expanded_recording_archive(void)
{
    reset();
    for(uint32_t k=0;k<NTRK;k++)for(uint32_t b=0;b<NPAT;b++) {
        pattern_request(&trk[k],b);
        trk[k].step[3]=(step_t){{60},1,ST_NOTE,SF_RECORDED,100,0,0,0};
    }
    for(uint32_t i=0;i<RECORD_MAX;i++)recording[i]=(recorded_note_t){(uint16_t)(i*61u),1234,60,100,(uint8_t)(i%32u),3};
    recording_reindex();
    static recorded_note_t saved[RECORD_MAX];uint32_t at=0;for(uint32_t owner=0;owner<32;owner++)for(uint32_t i=0;i<RECORD_MAX;i++)if((recording[i].owner&31u)==owner)saved[at++]=recording[i];
    int ok=!project_save_as(0,"FULL TAKE");
    uint32_t len,crc;ok &= !list(0,&len,&crc);
    static uint8_t archive[TSP_MAX], readback[TSP_MAX];memcpy(archive,ED_BK_RAW,len);
    for(uint32_t off=0;off<len;off+=256u){uint32_t n=len-off>256u?256u:len-off;ok &= !get(2,off,n) && ed_unpack7(rep+9u,rep_n-9u,readback+off,n)==n;}
    ok &= len && !memcmp(archive,readback,len);
    ok &= !put_all(3,archive,len,st_crc32(archive,len));
    pattern_init();memset(proj_meta,0,sizeof proj_meta);project_load(1);
    ok &= !memcmp(saved,recording,sizeof saved);
    int bad=check("all 1024 timed notes across 32 banks survive flash, backup GET and saved-slot restore",ok);
    ok=!put_all(0,archive,len,st_crc32(archive,len)) && !memcmp(saved,recording,sizeof saved);
    bad+=check("expanded runtime archive restores every original timed note",ok);
    return bad;
}
/* melodee-tess: the generators. A slot's TSP1 record carries them and its cords; 28 (the Tessera object) is retired,
 * always empty; 29 is the live state */
static uint32_t get_all(uint32_t id, uint8_t *out, uint32_t len)   /* 0 read */
{
    for (uint32_t off = 0; off < len; off += 256u) {
        uint32_t n = len - off > 256u ? 256u : len - off;
        if (get(id, off, n) || ed_unpack7(rep + 9u, rep_n - 9u, out + off, n) != n) return 1;
    }
    return 0;
}
static int tessera_archive(void)
{
    static uint8_t slot[TSP_MAX], bad_rec[TSP_MAX], live[TESSERA_PK_BYTES], now[TESSERA_PK_BYTES];
    uint32_t len28 = 1, crc28 = 0, len29 = 0, crc29 = 0, len2 = 0, crc2 = 0, before;
    int bad = 0, ok;
    reset();
    flash_ok = 1;
    song.sel = 0; tessera_knob(0, TESS_ENG_TURING);
    song.sel = 1; tessera_knob(0, TESS_ENG_EUCLID);
    {
        cord_t c = {CS_LFO, 0, CD_CUTOFF, 1, 40, 0};   /* a cord, saved with the slot (its CORDS part) */
        cord_set(2, &c);
    }
    project_save_as(0, "GENERATORS");                 /* slot 1: its generators and cord in its record */
    song.sel = 2; tessera_knob(0, TESS_ENG_MARKOV);   /* the live state: one more than the saved one */
    tessera_pack(live);
    ok = !list(28, &len28, &crc28) && !list(29, &len29, &crc29) && !list(2, &len2, &crc2) && len28 == 0u &&
         len29 == TESSERA_PK_BYTES && len2 > 0u;
    ok &= !get_all(2, slot, len2) && !get_all(29, now, len29) && st_crc32(slot, len2) == crc2 && !memcmp(now, live, sizeof live);
    bad += check("LIST / GET: 28 retired (empty), 29 the live generators, slot 1 with its own", ok);
    memset(nor, 0xFF, sizeof nor);                    /* a fresh FM-1: nothing stored, nothing live */
    memset(proj_meta, 0, sizeof proj_meta); tessera_reset();
    ok = !put_all(28, 0, 0, 0) && !put_all(2, slot, len2, crc2) && !put_all(29, live, len29, crc29);
    tessera_pack(now);
    bad += check("PUT 2 and 29 restore the slot and the live generators", ok && !memcmp(now, live, sizeof live));
    tessera_reset(); project_load(0);
    bad += check("... and the slot's: loading it brings its generators back, and its cord",
                 tessera.mode[0] == TESS_ENG_TURING && tessera.mode[1] == TESS_ENG_EUCLID && tessera.mode[2] == TESS_ENG_OFF &&
                 cord_get(2)->dst == CD_CUTOFF && cord_get(2)->amount == 40 && cord_count() == 1u);
    memcpy(bad_rec, slot, len2);
    tsp_valid(bad_rec, len2);
    bad_rec[tsp_seen.at[TSP_CORDS] + 2u * 6u + 2u] = CD_N;   /* cord 3's destination out of range */
    tsp_resum(bad_rec);
    before = erases;
    ok = put_all(3, bad_rec, len2, st_crc32(bad_rec, len2)) == 2 && erases == before;
    bad += check("a damaged cord in a slot's record is refused before any erase", ok);
    memcpy(bad_rec, slot, len2);
    tsp_valid(bad_rec, len2);
    bad_rec[tsp_seen.at[TSP_TESSERA] + 1u + 1u * (TESSERA_PK_BYTES / TESSERA_TRACKS)] = TESS_ENG_COUNT + 1u;   /* track 2's engine */
    tsp_resum(bad_rec);
    before = erases; tessera_pack(live);
    ok = put_all(3, bad_rec, len2, st_crc32(bad_rec, len2)) == 2 && erases == before;
    tessera_pack(now);
    bad += check("a damaged generator block in a slot's record is refused before any erase, the live state kept",
                 ok && !memcmp(now, live, sizeof live));
    memcpy(now, live, sizeof now); now[1u * (TESSERA_PK_BYTES / TESSERA_TRACKS)] = TESS_ENG_COUNT + 1u;
    ok = put_all(29, now, sizeof now, st_crc32(now, sizeof now)) == 2;
    tessera_pack(now);
    bad += check("a damaged live state is refused, nothing changed", ok && !memcmp(now, live, sizeof live));
    bad += check("28 takes nothing but empty; 29 a wrong size is refused at the start", put_begin(28, 100, 0) == 1 && put_begin(29, 100, 0) == 1);
    {   /* an archive of an older build: a live block of version 1 (before SEED; each track 2 bytes shorter) */
        static uint8_t live1[TESSERA_PK_BYTES_V1];
        const uint32_t n2 = TESSERA_PK_BYTES / TESSERA_TRACKS, n1 = TESSERA_PK_BYTES_V1 / TESSERA_TRACKS;
        uint32_t tr;
        for (tr = 0; tr < TESSERA_TRACKS; tr++) memcpy(live1 + tr * n1, live + tr * n2, n1);
        tessera_reset();
        ok = !put_all(29, live1, sizeof live1, st_crc32(live1, sizeof live1));
        ok &= tessera.mode[0] == TESS_ENG_TURING && tessera.mode[1] == TESS_ENG_EUCLID && tessera.mode[2] == TESS_ENG_OFF;
        ok &= tessera.seed[0] >= 1 && tessera.seed[0] <= TESS_SEED_MAX && tessera.seed[1] >= 1 && tessera.seed[1] <= TESS_SEED_MAX;
        tessera_pack(now);
        for (tr = 0; tr < TESSERA_TRACKS; tr++) ok &= !memcmp(now + tr * n2, live + tr * n2, n1);   /* every version 1 setting */
        bad += check("an older live block (version 1, 29 of 180 bytes) restores, each track given a seed", ok);
    }
    bad += check("an empty 29 (no live generators in the archive): all OFF",
                 !put_all(29, 0, 0, 0) && tessera.mode[0] == TESS_ENG_OFF && tessera.mode[1] == TESS_ENG_OFF);
    return bad;
}
int main(void)
{
    int expanded_bad = expanded_recording_archive() + tessera_archive();

    int bad = 0;
    uint32_t len = 0, crc = 0, before;
    static project_store_t st;
    static project_store_t v6;
    persist_t ps;

    reset();
    trk[0].step[0] = (step_t){{60}, 1, ST_NOTE, 0, 96, 0, 0};
    bad += check("LIST captures runtime and all 30 archive objects with CRC",
                 list(0, &len, &crc) == 0 && rep[2] == 30u && len == tsp_len(ED_BK_RAW) && len &&
                 crc == st_crc32(ED_BK_RAW, len));
    bad += check("an empty project slot lists as length 0", list(2, &len, &crc) == 0 && len == 0);
    bad += check("GET of the runtime copy", get(0, 0, 64) == 0);
    project_save(0);
    bad += check("a project save after LIST reused the staging RAM: GET of the runtime is stale (5)",
                 get(0, 0, 64) == 5u);
    bad += check("a new LIST makes it readable again", list(0, &len, &crc) == 0 && get(0, 0, 64) == 0);
    usb.resets++;
    bad += check("a USB reset after LIST: GET is stale (5)", get(2, 0, 16) == 5u);

    /* settings */
    {
        static uint8_t old_settings[sizeof(persist_t)+sizeof(tmpl_fx_legacy_t)];
        persist_t defaults={0};settings_export(&defaults);memcpy(old_settings,&defaults,sizeof defaults);
        FILE *f=fopen("tests/fixtures/fx/tplf.bin","rb");
        uint32_t n=f?(uint32_t)fread(old_settings+sizeof defaults,1,sizeof(tmpl_fx_legacy_t),f):0;
        if(f)fclose(f);
        bad+=check("previous TPLF settings archive restores through PUT and remains dry",
            n==sizeof(tmpl_fx_legacy_t) && put_all(1,old_settings,sizeof old_settings,st_crc32(old_settings,sizeof old_settings))==0 &&
            template_used() && tmpl.t[0].p[P_E0]==17 && !tmpl.t[0].p[P_FXTYPE]);
        uint32_t before=erases;old_settings[sizeof defaults+__builtin_offsetof(tmpl_fx_legacy_t,cz)+2u]=255;
        bad+=check("damaged previous template is refused before any flash erase",
            put_all(1,old_settings,sizeof old_settings,st_crc32(old_settings,sizeof old_settings))==2 && erases==before);
    }
    reset();
    memset(&ps, 0, sizeof ps);
    settings_export(&ps);
    ps.lowcut = 2;                                       /* SPEAKER BASS+ */
    ps.palette = palette_to_stored(2);
    bad += check("settings with SPEAKER BASS+ restore", put_all(1, &ps, sizeof ps, st_crc32(&ps, sizeof ps)) == 0 &&
                 settings.lowcut == 0u && settings.palette == 2u);
    before = erases;
    ps.palette = 13;                                     /* an untagged id (before 1.0; 1.2 rewrote them) */
    bad += check("settings with an untagged palette id are refused, nothing written",
                 put_all(1, &ps, sizeof ps, st_crc32(&ps, sizeof ps)) == 2u && erases == before && settings.palette == 2u);
    ps.palette = palette_to_stored(2);
    settings_click = 1; settings_countin = 2;
    settings_export(&ps);                                /* (the record prefs, tagged in the zoom word) */
    settings_click = settings_countin = 0;
    bad += check("settings with a click and a count-in restore",
                 put_all(1, &ps, sizeof ps, st_crc32(&ps, sizeof ps)) == 0 && settings_click == 1u && settings_countin == 2u);
    ps.zoom = 0x12340100u;
    bad += check("settings with an untagged zoom word are refused", put_all(1, &ps, sizeof ps, st_crc32(&ps, sizeof ps)) == 2u);
    ps.zoom = 0;
    before = erases;
    ps.lowcut = 3;
    bad += check("settings with an unknown SPEAKER value are refused, nothing written",
                 put_all(1, &ps, sizeof ps, st_crc32(&ps, sizeof ps)) == 2u && erases == before && settings.lowcut == 0u);
    ps.lowcut = 0;
    ps.bold = hold_to_stored(0, 3);                      /* HOLD 0.6 s */
    bad += check("settings with HOLD 0.6 s restore",
                 put_all(1, &ps, sizeof ps, st_crc32(&ps, sizeof ps)) == 0 && HOLD_MS[settings_hold] == 600u);
    ps.bold = 1;                                         /* an older backup (its font weight): HOLD 0.4 s */
    bad += check("settings of an older backup restore with HOLD 0.4 s",
                 put_all(1, &ps, sizeof ps, st_crc32(&ps, sizeof ps)) == 0 && HOLD_MS[settings_hold] == 400u);
    before = erases;
    ps.bold = HOLD_TAG + 7u;
    bad += check("settings with an unknown HOLD value are refused, nothing written",
                 put_all(1, &ps, sizeof ps, st_crc32(&ps, sizeof ps)) == 2u && erases == before);
    ps.bold = 0;
    bad += check("a wrong CRC is refused before any write",
                 put_all(1, &ps, sizeof ps, st_crc32(&ps, sizeof ps) ^ 1u) == 2u && erases == before);
    {   /* the settings record with the template after it (SLOT TMPL): both back; without one: no template */
        static struct { persist_t p; tmpl_t t; } rec;
        rec.p = ps;
        rec.p.ext.boot = 2;
        memset(&tmpl, 0, sizeof tmpl);
        trk[1].p[P_LEVEL] = 66;
        template_save();
        rec.t = tmpl;
        memset(&tmpl, 0, sizeof tmpl);
        settings_boot = 0;
        bad += check("settings + template restore: BOOT B, the template back",
                     put_all(1, &rec, sizeof rec, st_crc32(&rec, sizeof rec)) == 0 && settings_boot == 2u &&
                     template_used() && tmpl.t[1].p[P_LEVEL] == 66);
        bad += check("  settings without one: no template", put_all(1, &ps, sizeof ps, st_crc32(&ps, sizeof ps)) == 0 &&
                     !template_used() && settings_boot == 0u);
        rec.t.magic ^= 1u;
        before = erases;
        bad += check("  a broken template is refused, nothing written",
                     put_all(1, &rec, sizeof rec, st_crc32(&rec, sizeof rec)) == 2u && erases == before);
    }

    /* malformed requests */
    bad += check("an unknown object id is refused", put_begin(8, sizeof ps, 0) == 1u);
    bad += check("a project longer than a record, or shorter, is refused at once",
                 put_begin(2, TSP_MAX + 1u, 0) == 1u && put_begin(2, TSP_HEAD + 3u, 0) == 1u && put_begin(0, 8u, 0) == 1u);
    bad += check("settings of the wrong length are refused", put_begin(1, sizeof ps + 4u, 0) == 1u);
    bad += check("a chunk without a begin is refused (5)", (ed_bk_put = 0, put_chunk(1, 0, (const uint8_t *)&ps, 16)) == 5u);
    put_begin(1, sizeof ps, st_crc32(&ps, sizeof ps));
    bad += check("a chunk at the wrong offset is refused", put_chunk(1, 16, (const uint8_t *)&ps, 16) == 1u);
    bad += check("a chunk for another object is refused (5)", put_chunk(2, 0, (const uint8_t *)&ps, 16) == 5u);
    {
        uint32_t off = 0, step = sizeof ps - 8u;
        put_begin(1, sizeof ps, st_crc32(&ps, sizeof ps));
        for (; off + 256u <= step; off += 256u)
            put_chunk(1, off, (const uint8_t *)&ps + off, 256);
        if (off < step) put_chunk(1, off, (const uint8_t *)&ps + off, step - off);
        bad += check("a chunk past the length is refused", put_chunk(1, step, (const uint8_t *)&ps, 16) == 1u);
    }
    bad += check("commit before all bytes arrived is refused", put_end(1, 2) == 2u && erases == before);
    put_begin(1, sizeof ps, st_crc32(&ps, sizeof ps));
    put_chunk(1, 0, (const uint8_t *)&ps, 64);
    usb.resets++;
    bad += check("a USB reset during PUT ends it (5)", put_chunk(1, 64, (const uint8_t *)&ps + 64, 64) == 5u);
    put_begin(1, sizeof ps, st_crc32(&ps, sizeof ps));
    fm1_ms += 16000u;
    bad += check("16 s without a chunk ends the PUT (5)", put_chunk(1, 0, (const uint8_t *)&ps, 64) == 5u);
    put_begin(2, sizeof st, 0);
    put_chunk(2, 0, (const uint8_t *)&ps, 64);
    project_save(1);
    bad += check("a project save during PUT (the staging RAM) ends it (5)",
                 put_chunk(2, 64, (const uint8_t *)&ps, 64) == 5u);
    transport_req = 1;
    bad += check("PUT while starting PLAY is refused (3)", put_begin(1, sizeof ps, 0) == 3u);
    transport_req = 0;

    /* projects */
    reset();
    trk[1].step[3] = (step_t){{64}, 1, ST_NOTE, 0, 96, 0, 0};
    project_capture(&proj_scratch);
    proj_pack(&st, &proj_scratch);
    bad += check("a bare FUN16 record restores into slot 3 (flash and RAM) as a bank",
                 put_all(4, &st, sizeof st, st_crc32(&st, sizeof st)) == 0 && project_used(2) &&
                 !memcmp(&proj_slot[2], &st, sizeof st) && st_load(OBJ_SLOT(2), proj_wire_u.raw, sizeof proj_wire_u.raw) > 0 &&
                 tsp_valid(proj_wire_u.raw, tsp_len(proj_wire_u.raw)));
    {   /* a record of an older format: refused before anything is written (archives of before 1.0.0: 1.2) */
        static uint8_t old[3388];
        memset(old, 0, sizeof old);
        ((uint32_t *)old)[0] = 0x46554E36u; ((uint32_t *)old)[1] = sizeof old;
        before = erases;
        bad += check("a FUN6 project archive is refused before anything is written",
                     put_all(5, old, sizeof old, st_crc32(old, sizeof old)) == 2u && erases == before && !project_used(3));
    }
    {   /* the retired FM6 bank (8) and owned-voice archives (19, 20 at 4,240 B) are refused; native voices restore */
        static native_fm_t got;
        static up_bank_t records;
        static uint8_t owned[4240];
        uint8_t pk[FM6_PACKED];
        set_engine_of(TSEL, ENGI_FM6);
        up_store(2, "OLD FM6");
        bad += check("retired bank id 8 refuses a payload", put_begin(8, 3612, 0) == 1u);
        bad += check("retired bank id 8 is listed empty", list(8, &len, &crc) == 0 && !len);
        bad += check("an owned-voice archive (id 19) is refused", put_begin(19, sizeof owned, st_crc32(owned, sizeof owned)) == 1u);
        fm6_factory(5, pk); native_put(ENGI_FM6, 2, pk);
        bad += check("native voice id 21 is listed with its CRC", list(21, &len, &crc) == 0 && len == sizeof(native_fm_t) && crc == st_crc32(native_fm_bank(0), len));
        memcpy(&got, native_fm_bank(0), sizeof got); native_put(ENGI_FM6, 2, 0);
        bad += check("native voices restore through id 21", !native_used(ENGI_FM6, 2) && put_all(21, &got, sizeof got, st_crc32(&got, sizeof got)) == 0 && native_used(ENGI_FM6, 2) && !memcmp(native_raw(ENGI_FM6, 2), pk, FM6_PACKED));
        got.magic ^= 1u;
        bad += check("a damaged native voice bank is refused", put_all(21, &got, sizeof got, st_crc32(&got, sizeof got)) == 2u);
        up_store(63, "LAST FM6");
        bad += check("additional user record bank id 18 retains U64", list(18, &len, &crc) == 0 && len == sizeof(up_bank_t));
        memcpy(&records, up_cache_bank(3), sizeof records); memset(up_cache_bank(3), 0, sizeof (*up_cache_bank(3)));
        bad += check("additional user record bank id 18 restores U64", put_all(18, &records, sizeof records, st_crc32(&records, sizeof records)) == 0 && up_used(63));
        fm6_pack(fm6_patch[0], pk);
        bad += check("U64 keeps its own voice in native bank 3 (id 20)", native_used(ENGI_FM6, 63) && !memcmp(native_raw(ENGI_FM6, 63), pk, FM6_PACKED) && list(20, &len, &crc) == 0 && len == sizeof(native_fm_t));
    }
    memcpy(&st, &proj_slot[2], sizeof st);
    erase_error = 1;
    proj_slot[2].raw[100] ^= 1;                          /* (marks the RAM copy, to see it is kept) */
    memcpy(&v6, &proj_slot[2], sizeof v6);
    bad += check("a flash error on commit (4) keeps the slot's RAM copy",
                 put_all(4, &st, sizeof st, st_crc32(&st, sizeof st)) == 4u && !memcmp(&proj_slot[2], &v6, sizeof v6));
    erase_error = 0;
    bad += check("an empty project object clears the slot", put_all(4, 0, 0, st_crc32(0, 0)) == 0 && !project_used(2));
    st.raw[200] ^= 1;                                    /* hash no longer matches */
    bad += check("a project whose own hash fails is refused", put_all(4, &st, sizeof st, st_crc32(&st, sizeof st)) == 2u);

    /* the runtime */
    reset();
    trk[2].step[5] = (step_t){{67}, 1, ST_NOTE, 0, 96, 0, 0};
    trk[2].p[P_LEVEL] = 77;
    project_capture(&proj_scratch);
    proj_pack(&st, &proj_scratch);
    host_tracks_init();
    bad += check("the runtime object restores the tracks",
                 put_all(0, &st, sizeof st, st_crc32(&st, sizeof st)) == 0 && trk[2].step[5].note[0] == 67u &&
                 trk[2].p[P_LEVEL] == 77);

    /* Unsupported banks and settings export their committed bytes, not empty cache defaults. */
    reset();
    {
        uint8_t old[3080]; memset(old, 0x5a, sizeof old);
        const uint32_t objects[] = {OBJ_SETTINGS, OBJ_UPRESET0, OBJ_NATIVEFM0};   /* (CZ-1 banks: storage retired) */
        const uint32_t ids[] = {1u, 6u, 21u};
        int intact = 1;
        for (uint32_t k = 0; k < NELEM(objects); k++) intact &= st_save(objects[k], old, sizeof old) == 0;
        up_cache_reset(); native_cache_reset(); persist_old = 1;
        for (uint32_t k = 0; k < NELEM(ids); k++) {
            intact &= list(ids[k], &len, &crc) == 0 && len == sizeof old && crc == st_crc32(old, sizeof old);
            const uint8_t *raw = ed_bk_object(ids[k], &len);
            intact &= raw && len == sizeof old && !memcmp(raw, old, sizeof old);
        }
        bad += check("full backup retains unsupported settings and preset bank bytes", intact);
    }
    /* user preset banks: unknown record versions are kept as bytes */
    reset();
    {
        static up_bank_t b;
        memset(&b, 0, sizeof b);
        b.magic = UP_BANK_MAGIC;
        b.rsize = sizeof(up_rec_t);
        b.nslot = UP_PER_BANK;
        b.r[4].used = UP_USED;
        b.r[4].ver = 99;                                 /* a future record */
        memcpy(b.r[4].name, "Future", 7);
        b.r[4].packed[0] = 55;
        bad += check("a bank with a future record version restores byte for byte",
                     put_all(6, &b, sizeof b, st_crc32(&b, sizeof b)) == 0 &&
                     st_load(OBJ_UPRESET0, &proj_wire, sizeof proj_wire) == (int)sizeof b &&
                     !memcmp(&proj_wire, &b, sizeof b));
        b.nslot = 3;
        bad += check("a bank with the wrong slot count is refused", put_all(7, &b, sizeof b, st_crc32(&b, sizeof b)) == 2u);
    }
    {   /* CZ-1 banks: a never-saved bank (BANK A..D: Casio's factory tones) is no object of the backup, so a
         * restore keeps it the default; a saved one is carried byte for byte */
        static cz_bank_t cb;
        reset();
        cz_bank_boot();
        bad += check("CZ-1: a never-saved BANK A (its factory default) lists as length 0",
                     list(9, &len, &crc) == 0 && len == 0 && cz_bank_load(0)->used == 0xFFFFu);
        cz_bank_empty(&cb, 0);
        memcpy(cb.tone[3].raw, cz_fac(7), CZ_BYTES);
        cb.used = 1u << 3;
        bad += check("CZ-1: a saved BANK A restores and lists with its 2332 B",
                     put_all(9, &cb, sizeof cb, st_crc32(&cb, sizeof cb)) == 0 && list(9, &len, &crc) == 0 &&
                     len == sizeof cb && cz_bank_load(0)->used == 1u << 3);
        bad += check("CZ-1: length 0 clears native presets while keeping factory tones separate",
                     put_all(9, &cb, 0, st_crc32(&cb, 0)) == 0 && !native_cz_bank(0)->used &&
                     list(9, &len, &crc) == 0 && len == sizeof(cz_bank_t));
    }
    bad += native_archive();
    bad += prophet_archive();
    bad += full_pattern_archive();
    printf("backup test %s\n", bad ? "FAILED" : "passed");
    return (bad + expanded_bad) != 0;
}
