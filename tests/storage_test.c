/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Host test of firmware/src/storage.c against a simulated NOR flash:
 * erase -> 0xFF, program can only clear bits, page writes must not wrap. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static uint8_t nor[0x100000];
static int fail_after = -1;            /* torn-write injection: stop after N programs */
static int fail_bytes = -1;            /* power loss after any programmed byte */
static int erase_error, write_protected;
static uint32_t fail_erase_off = 0xFFFFFFFFu;
static uint32_t io_calls;
/* the sector map check: each sector an object erases or programs, while trace_obj is set, is its own */
static int trace_obj = -1;
static int16_t sec_owner[0x100000 / 4096];
static uint32_t sec_clash, sec_reserved;
static void trace(uint32_t off, uint32_t n)
{
    uint32_t s;
    if (trace_obj < 0 || !n) return;
    for (s = off >> 12; s <= (off + n - 1u) >> 12; s++) {
        if (sec_owner[s] >= 0 && sec_owner[s] != trace_obj) {
            printf("  sector %#x: object %d and object %d\n", s << 12, sec_owner[s], trace_obj);
            sec_clash++;
        }
        sec_owner[s] = (int16_t)trace_obj;
        if ((s << 12) < 0x93000u || ((s << 12) >= 0xE0000u && (s << 12) < 0xE5000u)) {
            printf("  object %d writes %#x: the app area or the OTA staging\n", trace_obj, s << 12);
            sec_reserved++;
        }
    }
}

static int st_read(uint32_t off, void *dst, uint32_t n) { io_calls++; memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off)
{
    io_calls++;
    trace(off, 4096);
    if (erase_error || off == fail_erase_off) return -8;
    if (!write_protected) memset(nor + off, 0xFF, 4096);
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    io_calls++;
    if (fail_after == 0)
        return -9;
    if (fail_after > 0)
        fail_after--;
    trace(off, n);
    if ((off & 0xFFu) + n > 256u && trace_obj < 0) {   /* (the sector map: split, as hal/fm1_flash.h fl_write does) */
        printf("page wrap at %#x\n", off);
        exit(1);
    }
    for (i = 0; i < n; i++) {
        if (fail_bytes == 0) return -9;
        if (fail_bytes > 0) fail_bytes--;
        if (!write_protected) nor[off + i] &= s[i];
    }
    return 0;
}
#include "../firmware/src/storage.c"
#define T_OBJ OBJ_UPRESET_EXT0                   /* (four ordinary one-sector objects in a row for the A/B tests) */

static int check(const char *what, int ok)
{
    printf("%-46s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

int main(void)
{
    char a[600], b[600], got[600];
    int bad = 0, n;
    memset(nor, 0xFF, sizeof nor);
    memset(a, 'A', sizeof a);
    memset(b, 'B', sizeof b);
    bad += check("empty flash loads nothing", st_load(T_OBJ, got, sizeof got) < 0);
    bad += check("save A", st_save(T_OBJ, a, sizeof a) == 0);
    n = st_load(T_OBJ, got, sizeof got);
    bad += check("load returns A", n == (int)sizeof a && !memcmp(got, a, sizeof a));
    bad += check("save B", st_save(T_OBJ, b, sizeof b) == 0);
    n = st_load(T_OBJ, got, sizeof got);
    bad += check("load returns B (newer seq)", n == (int)sizeof b && !memcmp(got, b, sizeof b));
    fail_after = 1;                                  /* payload page 1 written, the rest torn */
    st_save(T_OBJ, a, sizeof a);
    fail_after = -1;
    n = st_load(T_OBJ, got, sizeof got);
    bad += check("torn save keeps B", n == (int)sizeof b && !memcmp(got, b, sizeof b));
    fail_after = 3;                                  /* all payload pages, header torn */
    st_save(T_OBJ, a, sizeof a);
    fail_after = -1;
    n = st_load(T_OBJ, got, sizeof got);
    bad += check("header not written keeps B", n == (int)sizeof b && !memcmp(got, b, sizeof b));
    bad += check("save A again", st_save(T_OBJ, a, sizeof a) == 0);
    n = st_load(T_OBJ, got, sizeof got);
    bad += check("load returns A", n == (int)sizeof a && !memcmp(got, a, sizeof a));
    nor[st_sector(T_OBJ, 0) + ST_PAYLOAD_OFF + 10] ^= 0x01;   /* bit rot in one copy */
    nor[st_sector(T_OBJ, 1) + ST_PAYLOAD_OFF + 10] ^= 0x01;
    n = st_load(T_OBJ, got, sizeof got);
    bad += check("both copies corrupt -> nothing", n < 0);
    bad += check("other objects untouched", st_load(T_OBJ + 1, got, sizeof got) < 0);
    bad += check("settings save/load",
                 st_save(OBJ_SETTINGS, "hello", 5) == 0 && st_load(OBJ_SETTINGS, got, 5) == 5 && !memcmp(got, "hello", 5));
    bad += check("CRC-32 is zlib's (check value 0xCBF43926)", st_crc32("123456789", 9) == 0xCBF43926u);
    memset(nor, 0xFF, sizeof nor);
    st_save(T_OBJ + 2, a, sizeof a);
    st_save(T_OBJ + 2, b, sizeof b);              /* B is newer, in the other copy */
    {
        st_hdr_t h;
        int cur = st_current(T_OBJ + 2, &h);
        nor[st_sector(T_OBJ + 2, (uint32_t)cur) + ST_PAYLOAD_OFF + 300] ^= 0x10;   /* rot in the newer copy */
    }
    n = st_load(T_OBJ + 2, got, sizeof got);
    bad += check("newer copy rotten -> the older one loads", n == (int)sizeof a && !memcmp(got, a, sizeof a));
    bad += check("save over the rotten copy", st_save(T_OBJ + 2, b, sizeof b) == 0);
    n = st_load(T_OBJ + 2, got, sizeof got);
    bad += check("... loads the new data", n == (int)sizeof b && !memcmp(got, b, sizeof b));
    nor[st_sector(T_OBJ + 2, 0) + 8] ^= 0x01;    /* both headers broken */
    nor[st_sector(T_OBJ + 2, 1) + 8] ^= 0x01;
    bad += check("both headers broken -> nothing", st_load(T_OBJ + 2, got, sizeof got) < 0);
    bad += check("data stays in the Melodee regions",
                 st_sector(OBJ_SETTINGS, 1) + 4096 <= 0xFF000 && st_sector(T_OBJ + 3, 1) + 4096 <= 0xE0000 &&
                     st_sector(OBJ_UPRESET0, 0) >= 0xDC000 && st_sector(OBJ_UPRESET0 + 1, 1) + 4096 <= 0xE0000);
    /* the FM6 patch bank: the two free sectors, 0x9F000 (after the projects) and 0xFE000 (after the settings) */
    bad += check("FM6 bank in the free sectors 0x9F000 / 0xFE000",
                 OBJ_FM6BANK + 1 == OBJ_BANK0 && st_sector(OBJ_FM6BANK, 0) == 0x9F000u &&
                     st_sector(OBJ_FM6BANK, 1) == 0xFE000u && st_sector(OBJ_SETTINGS, 1) + 4096 == 0xFE000u);
    {   /* the legacy single-pattern projects: retired, nothing loads or saves them (the Prophet banks' place) */
        uint32_t k, calls = io_calls;
        int ok = 1;
        for (k = 0; k < 4u; k++)
            ok &= st_load(OBJ_PROJECT0 + k, got, sizeof got) < 0 && st_save(OBJ_PROJECT0 + k, a, sizeof a) < 0;
        bad += check("legacy project objects retired: no load, no save, no flash access", ok && io_calls == calls);
    }
    {
        static uint8_t bank[3612], back[3612];                 /* (any record up to a sector) */
        uint32_t i;
        for (i = 0; i < sizeof bank; i++) bank[i] = (uint8_t)(i * 7u);
        bad += check("FM6 bank save / load (A then B)", st_save(OBJ_FM6BANK, bank, sizeof bank) == 0 &&
                     st_load(OBJ_FM6BANK, back, sizeof back) == (int)sizeof back && !memcmp(bank, back, sizeof bank) &&
                     (bank[0] ^= 1, st_save(OBJ_FM6BANK, bank, sizeof bank) == 0) &&
                     st_load(OBJ_FM6BANK, back, sizeof back) == (int)sizeof back && back[0] == bank[0]);
        {
            int erased = 1;
            for (i = 256u + sizeof bank; i < 4096u; i++) erased &= nor[0x9F000u + i] == 0xFFu && nor[0xFE000u + i] == 0xFFu;
            bad += check("FM6 bank: the sector tails stay erased", erased);
        }
    }
    {
        uint8_t copies[2 * ST_SECTOR];
        uint32_t off = st_sector(T_OBJ, 0), cut;
        int ok = 1;
        memset(nor, 0xFF, sizeof nor);
        st_save(T_OBJ, a, sizeof a);
        st_save(T_OBJ, b, sizeof b);
        memcpy(copies, nor + off, sizeof copies);
        for (cut = 0; cut < sizeof a + sizeof(st_hdr_t); cut++) {
            memcpy(nor + off, copies, sizeof copies);
            fail_bytes = (int)cut;
            ok &= st_save(T_OBJ, a, sizeof a) != 0;
            fail_bytes = -1;
            ok &= st_load(T_OBJ, got, sizeof got) == (int)sizeof b && !memcmp(got, b, sizeof b);
        }
        bad += check("every payload/header byte cut keeps old data", ok);
        erase_error = 1;
        ok = st_save(T_OBJ, a, sizeof a) != 0;
        erase_error = 0;
        bad += check("erase failure keeps old data", ok && st_load(T_OBJ, got, sizeof got) == (int)sizeof b &&
                                                      !memcmp(got, b, sizeof b));
        write_protected = 1;
        ok = st_save(T_OBJ, a, sizeof a) != 0;
        write_protected = 0;
        bad += check("write protection is a save error", ok && st_load(T_OBJ, got, sizeof got) == (int)sizeof b &&
                                                            !memcmp(got, b, sizeof b));
    }
    {
        st_hdr_t h;
        uint32_t copy;
        for (copy = 0; copy < 2u; copy++) {
            uint32_t off = st_sector(T_OBJ, copy);
            memcpy(&h, nor + off, sizeof h);
            h.seq = 0xFFFFFFFEu + copy;
            h.hcrc = st_crc32(&h, sizeof h - 4u);
            memcpy(nor + off, &h, sizeof h);
        }
        bad += check("save sequence wraps", st_save(T_OBJ, a, sizeof a) == 0);
        n = st_load(T_OBJ, got, sizeof got);
        bad += check("wrapped sequence loads new data", n == (int)sizeof a && !memcmp(got, a, sizeof a));
        bad += check("save after sequence wrap", st_save(T_OBJ, b, sizeof b) == 0 &&
                                                st_load(T_OBJ, got, sizeof got) == (int)sizeof b &&
                                                !memcmp(got, b, sizeof b));
    }
    {
        uint32_t calls = io_calls;
        bad += check("invalid object load does no flash access", st_load(OBJ_COUNT, got, sizeof got) < 0 && io_calls == calls);
        bad += check("invalid object save does no flash access", st_save(OBJ_COUNT, a, sizeof a) < 0 && io_calls == calls);
        bad += check("wrapped object rejected", st_save(0xFFFFFFFFu, a, sizeof a) < 0 && io_calls == calls);
        memset(got, 'X', sizeof got);
        bad += check("small destination rejects whole object", st_load(T_OBJ, got, sizeof got - 1u) < 0 && got[0] == 'X');
        bad += check("oversized save rejected", st_save(T_OBJ, a, ST_PAYLOAD_MAX + 1u) < 0);
    }
    {   /* Tessera's projects (OBJ_PROJ0..3): ten sectors a copy from the tables, each sector's tail erased (the update
         * loader's record scan), every sector inside the HAL's storage areas (hal/fm1_flash.h FL_STORE_OK) */
        static uint8_t payload[ST_PROJ_MAX], got[ST_PROJ_MAX];
        int ok = 1, inside = 1, tails = 1;
        for (uint32_t i = 0; i < sizeof payload; i++) payload[i] = (uint8_t)(i * 37u + 11u);
        memset(nor, 0xFF, sizeof nor);
        ok &= st_capacity(OBJ_PROJ0) == 38368u && OBJ_SLOT(5u) == OBJ_PROJ0 + 1u;
        for (uint32_t k = 0; k < 4u; k++)
            for (uint32_t copy = 0; copy < 2u; copy++) {
                ok &= st_save(OBJ_PROJ0 + k, payload, sizeof payload) == 0;
                ok &= st_load(OBJ_PROJ0 + k, got, sizeof got) == (int)sizeof got && !memcmp(got, payload, sizeof got);
                for (uint32_t i = 0; i < ST_PROJ_SECTORS; i++) {
                    uint32_t sec = st_proj_sector(OBJ_PROJ0 + k, copy, i);
                    inside &= (sec >= 0x93000u && sec + ST_SECTOR <= 0xE0000u) || (sec >= 0xE5000u && sec + ST_SECTOR <= 0xFB000u);
                    for (uint32_t j = ST_SECTOR - ST_TAIL; j < ST_SECTOR; j++) tails &= nor[sec + j] == 0xFFu;
                }
            }
        bad += check("projects: four slots, both copies, 38,368 B each round trip", ok);
        bad += check("projects: every sector inside the HAL's storage areas", inside);
        bad += check("projects: every sector's last 256 B stay erased (update records)", tails);
        uint8_t cross[600];
        ok = 1;
        for (uint32_t edge = ST_SECTOR - ST_TAIL - sizeof(st_hdr_t); edge < sizeof payload; edge += ST_SECTOR - ST_TAIL) {
            st_hdr_t h;
            int c = st_current(OBJ_PROJ0 + 3u, &h);
            ok &= c >= 0 && st_read_range(OBJ_PROJ0 + 3u, (uint32_t)c, edge - 300u, cross, sizeof cross) == 0 &&
                  !memcmp(cross, payload + edge - 300u, sizeof cross);
        }
        bad += check("projects: range reads cross every sector edge (slot D's split copy too)", ok);
        uint32_t calls = io_calls;
        bad += check("projects: one byte over is refused before any flash access",
                     st_save(OBJ_PROJ0, payload, ST_PROJ_MAX + 1u) < 0 && io_calls == calls);
    }
    {   /* a save cut at any program (each page) or any header byte keeps the old copy; an erase failure on a later
         * sector too; the CRC covers the last sector */
        static uint8_t first[31000], second[31000], back[31000], snap[0x100000];
        int ok = 1;
        memset(first, 0x35, sizeof first); memset(second, 0x79, sizeof second);
        memset(nor, 0xFF, sizeof nor);
        ok &= st_save(OBJ_PROJ0 + 3u, first, sizeof first) == 0 && st_save(OBJ_PROJ0 + 3u, second, sizeof second) == 0;
        memcpy(snap, nor, sizeof snap);
        uint32_t cuts = 0;
        for (uint32_t cut = 0;; cut++) {                    /* (each program call in turn: the last, the header, completes) */
            memcpy(nor, snap, sizeof snap); fail_after = (int)cut;
            int rc = st_save(OBJ_PROJ0 + 3u, first, sizeof first); fail_after = -1;
            if (!rc) break;
            cuts++;
            ok &= st_load(OBJ_PROJ0 + 3u, back, sizeof back) == (int)sizeof back && !memcmp(back, second, sizeof back);
        }
        ok &= cuts > sizeof first / 256u;                    /* (every page was cut once, the header too) */
        for (uint32_t cut = 0; cut < sizeof(st_hdr_t); cut++) {
            memcpy(nor, snap, sizeof snap); fail_bytes = (int)(sizeof first + cut);
            ok &= st_save(OBJ_PROJ0 + 3u, first, sizeof first) != 0; fail_bytes = -1;
            ok &= st_load(OBJ_PROJ0 + 3u, back, sizeof back) == (int)sizeof back && !memcmp(back, second, sizeof back);
        }
        bad += check("projects: every page or header cut keeps the old copy", ok);
        memcpy(nor, snap, sizeof snap);
        {
            st_hdr_t h;
            int c = st_current(OBJ_PROJ0 + 3u, &h);
            fail_erase_off = st_proj_sector(OBJ_PROJ0 + 3u, c == 0 ? 1u : 0u, ST_PROJ_SECTORS - 1u);
        }
        ok = st_save(OBJ_PROJ0 + 3u, first, sizeof first) != 0; fail_erase_off = 0xFFFFFFFFu;
        bad += check("projects: an erase failure on the last sector keeps the old copy",
                     ok && st_load(OBJ_PROJ0 + 3u, back, sizeof back) == (int)sizeof back && !memcmp(back, second, sizeof back));
        memcpy(nor, snap, sizeof snap);
        {
            st_hdr_t h;
            uint32_t span;
            int c = st_current(OBJ_PROJ0 + 3u, &h);
            nor[st_address(OBJ_PROJ0 + 3u, (uint32_t)c, sizeof second - 1u, &span)] ^= 1;
        }
        bad += check("projects: the CRC covers the last sector (the older copy loads)",
                     st_load(OBJ_PROJ0 + 3u, back, sizeof back) == (int)sizeof back && !memcmp(back, first, sizeof back));
    }
    {   /* retired: Melodee's bank projects, the CZ-1 and Prophet banks: no load, no save, no flash access; an old bank
         * project's commit record left in a project's first sector never validates as a project (another type) */
        uint8_t small[64];
        uint32_t calls = io_calls, k;
        int ok = 1;
        memset(small, 7, sizeof small);
        for (k = 0; k < 4u; k++) ok &= st_load(OBJ_BANK0 + k, got, sizeof got) < 0 && st_save(OBJ_BANK0 + k, small, sizeof small) < 0;
        for (k = 0; k < 8u; k++) ok &= st_load(OBJ_CZBANK0 + k, got, sizeof got) < 0 && st_save(OBJ_CZBANK0 + k, small, sizeof small) < 0;
        for (k = 0; k < 5u; k++) ok &= st_load(OBJ_P5BANK0 + k, got, sizeof got) < 0 && st_save(OBJ_P5BANK0 + k, small, sizeof small) < 0;
        bad += check("retired bank objects: no load, no save, no flash access", ok && io_calls == calls);
        memset(nor, 0xFF, sizeof nor);
        st_hdr_t h = {ST_MAGIC, (uint16_t)OBJ_BANK0, 0, 5, sizeof small, st_crc32(small, sizeof small), {0xFFFFFFFFu, 0xFFFFFFFFu}, 0};
        h.hcrc = st_crc32(&h, sizeof h - 4u);
        memcpy(nor + 0xA0000u, &h, sizeof h);                 /* (an old FBKK's place: slot A's first copy now) */
        memcpy(nor + 0xA0000u + ST_PAYLOAD_OFF, small, sizeof small);
        bad += check("an old bank's commit record is no project", st_sector(OBJ_PROJ0, 0) == 0xA0000u &&
                     st_load(OBJ_PROJ0, got, sizeof got) < 0 && !st_exists(OBJ_PROJ0));
        bad += check("... and the slot saves over it", st_save(OBJ_PROJ0, a, sizeof a) == 0 &&
                     st_load(OBJ_PROJ0, got, sizeof got) == (int)sizeof a && !memcmp(got, a, sizeof a));
    }
    {   /* every live object saved twice (both copies) at its full size: no sector shared by two objects, none in the
         * app area or the OTA staging. Guards the free pair OBJ_TESSERA uses (0x9D000) at every upstream merge. */
        static uint8_t full[64u * 1024u];
        uint32_t obj, saved = 1;
        memset(nor, 0xFF, sizeof nor);
        memset(full, 0x5A, sizeof full);
        for (obj = 0; obj < sizeof sec_owner / sizeof sec_owner[0]; obj++) sec_owner[obj] = -1;
        sec_clash = sec_reserved = 0;
        for (obj = 0; obj < OBJ_COUNT; obj++) {
            if (st_retired(obj)) continue;
            trace_obj = (int)obj;
            saved &= st_capacity(obj) <= sizeof full && st_save(obj, full, st_capacity(obj)) == 0 &&
                     st_save(obj, full, st_capacity(obj)) == 0;
        }
        trace_obj = -1;
        bad += check("every live object saves both copies at full size", saved);
        bad += check("no two live objects share a flash sector", !sec_clash);
        bad += check("no object writes the app area or the OTA staging", !sec_reserved);
        bad += check("Tessera's object stays in the free pair 0x9D000..0x9EFFF",
                     st_sector(OBJ_TESSERA, 0) == 0x9D000u && st_sector(OBJ_TESSERA, 1) == 0x9E000u &&
                     sec_owner[0x9D] == OBJ_TESSERA && sec_owner[0x9E] == OBJ_TESSERA);
    }
    printf("%s\n", bad ? "STORAGE TEST FAILED" : "storage test passed");
    bad += check("user presets and native FM6 keep their places",
        st_sector(OBJ_UPRESET0, 0) == 0xDC000u && st_sector(OBJ_NATIVEFM0, 0) == 0xD8000u &&
        st_sector(OBJ_NATIVEFM0 + 1u, 1) + ST_SECTOR == 0xDC000u);
    return bad != 0;
}
