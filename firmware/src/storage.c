/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Persistent storage on the SPI NOR.
 *
 * Every object has an A/B pair (ten sectors a copy for a project, one otherwise). A save goes to the copy that is not
 * the current one: erase its sectors, program the payload pages, then the 32-byte header at offset 0 of its first
 * sector LAST. The header is the commit
 * record (magic, seq, length, payload CRC, header CRC); on load the valid
 * copy with the newest seq wins (including wrap), so a write torn at any point leaves the
 * previous copy in charge.
 *
 * Flash access goes through three hooks (also used by the host test):
 *   st_read(off, dst, n)   st_erase(off)   st_prog(off, src, n)
 */
#define ST_MAGIC 0x554C4546u                   /* "FELU" */
#define ST_SECTOR 4096u
#define ST_PAYLOAD_OFF 256u
#define ST_PAYLOAD_MAX (ST_SECTOR - ST_PAYLOAD_OFF)
#define ST_TAIL 256u                           /* a sector's last bytes, kept erased (see the flash map) */
#define ST_PROJ_SECTORS 10u                    /* a project copy (Tessera's own format, docs/design/PROJECT_FORMAT.md) */
#define ST_PROJ_MAX ((ST_SECTOR - ST_TAIL - 32u) + (ST_PROJ_SECTORS - 1u) * (ST_SECTOR - ST_TAIL))   /* 38,368 B */

/* Flash map (Tessera, 2026-10-22: docs/design/PROJECT_FORMAT.md in tessera-notes): app area 0x4000..0x92FFF; project
 * copies (OBJ_PROJ0..3, ten sectors each, ST_PROJ_RUN) in 0x93000..0x9CFFF, 0xA0000..0xD7FFF and 0xEA000..0xF7FFF;
 * the Tessera object 0x9D000..0x9EFFF; FM6 patch bank copy A 0x9F000 (B 0xFE000); native FM6 0xD8000..0xDBFFF;
 * user preset banks 0xDC000..0xDFFFF (upreset.c); OTA staging 0xE0000..0xE4FFF (0xE4000 the update-record sector);
 * preset extensions 0xE5000..0xE8FFF, 0xE9000, 0xFA000; 0xF8000..0xF9FFF and 0xFB000 free; settings 0xFC000..0xFDFFF;
 * 0xFF000 the device's key and MAC (never written).
 * The update loader (loader/ldr_core.c) reads the last 256 bytes of every sector from 0x93000 to 0xFC000 after an
 * update and erases a sector whose tail looks like an update record: a project sector keeps its tail (ST_TAIL) erased,
 * so music can never look like one.
 * Retired, their numbers kept and never loaded, saved or erased: the legacy single-pattern projects (OBJ_PROJECT0..3),
 * Melodee's bank projects (OBJ_BANK0..3: five base sectors at 0xA0000.. and two at 0xEA000..), the CZ-1 banks
 * (OBJ_CZBANK0..7, 0xC8000..) and the Prophet banks (OBJ_P5BANK0..4, 0x93000..): their sectors are the projects' now.
 * The projects have object numbers no firmware used before, so an old commit record left in one of their sectors
 * never validates as a project (st_head checks the type).
 * User samples are disabled: this area must never accept sample uploads. U33..U64 use E5000..E8FFF; owned FM6
 * U01..U32 reuse the old bank pair, U33..U64 use E9000/FA000. E0000..E4FFF remains reserved for OTA loader staging. */
enum { OBJ_SETTINGS, OBJ_PROJECT0, OBJ_UPRESET0 = OBJ_PROJECT0 + 4, OBJ_FM6BANK = OBJ_UPRESET0 + 2, OBJ_BANK0, OBJ_CZBANK0 = OBJ_BANK0 + 4, OBJ_UPRESET_EXT0 = OBJ_CZBANK0 + 8, OBJ_UPFM6_EXT = OBJ_UPRESET_EXT0 + 2, OBJ_NATIVEFM0, OBJ_P5BANK0 = OBJ_NATIVEFM0 + 2, OBJ_TESSERA = OBJ_P5BANK0 + 5, OBJ_PROJ0, OBJ_COUNT = OBJ_PROJ0 + 4 };
#define OBJ_SLOT(slot) (OBJ_PROJ0 + ((slot) & 3u))   /* a project slot's object */
/* Each project copy (slot * 2 + copy): its ten sectors as up to two runs of consecutive sectors */
static const struct { uint32_t at; uint8_t n; } ST_PROJ_RUN[8][2] = {
    {{0xA0000u, 10}, {0, 0}}, {{0xAA000u, 10}, {0, 0}},   /* slot A: Melodee's bank projects' base sectors */
    {{0xB4000u, 10}, {0, 0}}, {{0xBE000u, 10}, {0, 0}},   /* slot B */
    {{0xC8000u, 10}, {0, 0}}, {{0xEA000u, 10}, {0, 0}},   /* slot C: the CZ-1 banks', the old extensions */
    {{0x93000u, 10}, {0, 0}},                             /* slot D, A copy: the Prophet banks' */
    {{0xD2000u, 6}, {0xF4000u, 4}},                       /* slot D, B copy: the CZ-1 banks' rest, the extensions' */
};

typedef struct {
    uint32_t magic;
    uint16_t type, slot;
    uint32_t seq, len, crc, rsv[2];
    uint32_t hcrc;
} st_hdr_t;
_Static_assert(sizeof(st_hdr_t) == 32u, "storage commit record layout");

static int st_read(uint32_t off, void *dst, uint32_t n);
static int st_erase(uint32_t off);
static int st_prog(uint32_t off, const void *src, uint32_t n);

static uint32_t st_crc32(const void *p, uint32_t n)   /* zlib CRC-32, 4 bits per step */
{
    static const uint32_t T[16] = {
        0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu, 0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
        0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu, 0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu};
    const uint8_t *b = p;
    uint32_t c = 0xFFFFFFFFu;
    while (n--) {
        c ^= *b++;
        c = (c >> 4) ^ T[c & 15u];
        c = (c >> 4) ^ T[c & 15u];
    }
    return ~c;
}

static int st_proj(uint32_t obj) { return obj >= OBJ_PROJ0 && obj < OBJ_PROJ0 + 4u; }
/* sector i (0..ST_PROJ_SECTORS - 1) of a project copy */
static uint32_t st_proj_sector(uint32_t obj, uint32_t copy, uint32_t i)
{
    uint32_t c = (obj - OBJ_PROJ0) * 2u + copy;
    return i < ST_PROJ_RUN[c][0].n ? ST_PROJ_RUN[c][0].at + i * ST_SECTOR : ST_PROJ_RUN[c][1].at + (i - ST_PROJ_RUN[c][0].n) * ST_SECTOR;
}
static uint32_t st_sector(uint32_t obj, uint32_t copy)  /* flash offset of copy A (0) / B (1): its first sector */
{
    if (st_proj(obj)) return st_proj_sector(obj, copy, 0);
    if (obj == OBJ_TESSERA) return 0x9D000u + copy * ST_SECTOR;     /* the free pair below the FM6 patch bank */
    if (obj >= OBJ_P5BANK0) return 0x93000u+(obj-OBJ_P5BANK0)*2u*ST_SECTOR+copy*ST_SECTOR;
    if (obj >= OBJ_NATIVEFM0) return 0xD8000u + (obj - OBJ_NATIVEFM0) * 2u * ST_SECTOR + copy * ST_SECTOR;
    if (obj == OBJ_UPFM6_EXT) return copy ? 0xFA000u : 0xE9000u;
    if (obj >= OBJ_UPRESET_EXT0) return 0xE5000u + (obj - OBJ_UPRESET_EXT0) * 2u * ST_SECTOR + copy * ST_SECTOR;
    if (obj >= OBJ_CZBANK0)
        return 0xC8000u + (obj - OBJ_CZBANK0) * 2u * ST_SECTOR + copy * ST_SECTOR;
    if (obj >= OBJ_BANK0)
        return 0xA0000u + (obj - OBJ_BANK0) * 10u * ST_SECTOR + copy * 5u * ST_SECTOR;
    if (obj == OBJ_SETTINGS)
        return 0xFC000u + copy * ST_SECTOR;
    if (obj == OBJ_FM6BANK)
        return copy ? 0xFE000u : 0x9F000u;
    if (obj >= OBJ_UPRESET0)
        return 0xDC000u + (obj - OBJ_UPRESET0) * 2u * ST_SECTOR + copy * ST_SECTOR;
    return 0x97000u + (obj - OBJ_PROJECT0) * 2u * ST_SECTOR + copy * ST_SECTOR;
}

/* New objects (presets' extensions, Tessera, the projects) start directly after the commit header, leaving the last
 * 256 bytes erased so the SPL/update loader cannot interpret musical data as OTA. */
static uint32_t st_payload_off(uint32_t obj) { return obj >= OBJ_UPRESET_EXT0 ? sizeof(st_hdr_t) : ST_PAYLOAD_OFF; }
static uint32_t st_capacity(uint32_t obj) { return st_proj(obj) ? ST_PROJ_MAX : obj >= OBJ_UPRESET_EXT0 ? ST_SECTOR - ST_TAIL - sizeof(st_hdr_t) : ST_PAYLOAD_MAX; }
static uint32_t st_sectors(uint32_t obj) { return st_proj(obj) ? ST_PROJ_SECTORS : 1u; }
/* payload byte off of a copy: its flash offset, *span the bytes from there to the end of that sector's payload. A
 * project's payload runs through its ten sectors, each but its tail; the first after the commit record */
static uint32_t st_address(uint32_t obj, uint32_t copy, uint32_t off, uint32_t *span)
{
    if (st_proj(obj)) {
        const uint32_t first = ST_SECTOR - ST_TAIL - sizeof(st_hdr_t), each = ST_SECTOR - ST_TAIL;
        if (off < first) {
            *span = first - off;
            return st_sector(obj, copy) + sizeof(st_hdr_t) + off;
        }
        off -= first;
        *span = each - off % each;
        return st_proj_sector(obj, copy, 1u + off / each) + off % each;
    }
    *span = st_capacity(obj) - off;
    return st_sector(obj, copy) + st_payload_off(obj) + off;
}
static int st_read_range(uint32_t obj, uint32_t copy, uint32_t off, void *dst, uint32_t n)
{
    if (obj >= OBJ_COUNT || copy > 1u || off > st_capacity(obj) || n > st_capacity(obj) - off) return -1;
    uint8_t *d = dst;
    while (n) {
        uint32_t span, addr = st_address(obj, copy, off, &span), k = n < span ? n : span;
        if (st_read(addr, d, k)) return -1;
        d += k; off += k; n -= k;
    }
    return 0;
}

static uint8_t st_buf[256] __attribute__((aligned(4)));

/* retired, their numbers reserved, never loaded, saved or erased: the legacy single-pattern projects, Melodee's bank
 * projects, the CZ-1 and Prophet banks (their sectors are the projects' now: the flash map, above) */
static int st_retired(uint32_t obj)
{
    return (obj >= OBJ_PROJECT0 && obj < OBJ_PROJECT0 + 4u) || (obj >= OBJ_BANK0 && obj < OBJ_CZBANK0 + 8u) ||
           (obj >= OBJ_P5BANK0 && obj < OBJ_P5BANK0 + 5u);
}

static int st_head(uint32_t obj, uint32_t copy, st_hdr_t *h)   /* commit record valid: 0 */
{
    if (obj >= OBJ_COUNT || copy > 1u || st_retired(obj))
        return -1;
    if (st_read(st_sector(obj, copy), h, sizeof *h))
        return -1;
    if (h->magic != ST_MAGIC || h->type != obj || h->slot != copy || h->len > st_capacity(obj) ||
        h->hcrc != st_crc32(h, sizeof *h - 4u))
        return -1;
    return 0;
}

static int st_body(uint32_t obj, uint32_t copy, const st_hdr_t *h)   /* streaming CRC, bounded 256-byte scratch, valid: 0 */
{
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t off = 0; off < h->len; off += sizeof st_buf) {
        uint32_t n = h->len - off > sizeof st_buf ? sizeof st_buf : h->len - off;
        if (st_read_range(obj, copy, off, st_buf, n)) return -1;
        for (uint32_t i = 0; i < n; i++) {
            crc ^= st_buf[i];
            for (uint32_t j = 0; j < 8u; j++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc == h->crc ? 0 : -1;

}

/* the current copy: the newest valid sequence (A on a tie), -1 when
 * neither is valid. Headers first, so only the winner's payload is read (it
 * is verified); *h gets its header. */
static int st_current(uint32_t obj, st_hdr_t *h)
{
    st_hdr_t a, b;
    int va = st_head(obj, 0, &a) == 0, vb = st_head(obj, 1, &b) == 0;
    if (vb && (!va || (b.seq != a.seq && b.seq - a.seq < 0x80000000u))) {
        if (st_body(obj, 1, &b) == 0) {
            *h = b;
            return 1;
        }
        vb = 0;
    }
    if (va && st_body(obj, 0, &a) == 0) {
        *h = a;
        return 0;
    }
    if (vb && st_body(obj, 1, &b) == 0) {
        *h = b;
        return 1;
    }
    return -1;
}

/* load the whole object into dst; returns its length, or -1 if it does not fit */
static int st_load(uint32_t obj, void *dst, uint32_t max)
{
    st_hdr_t h;
    int cur;
    if (obj >= OBJ_COUNT || (cur = st_current(obj, &h)) < 0 || h.len > max) return -1;
    if (st_read_range(obj, (uint32_t)cur, 0, dst, h.len)) return -1;
    return (int)h.len;
}

/* a committed record is there, whatever it holds (st_load: -1 for one too big for its buffer too) */
static int st_exists(uint32_t obj)
{
    st_hdr_t h;
    return obj < OBJ_COUNT && !st_retired(obj) && st_current(obj, &h) >= 0 && h.len != 0;
}

static int st_save(uint32_t obj, const void *src, uint32_t len)
{
    uint32_t seq, base, off;
    int cur, rc;
    st_hdr_t h;
    if (obj >= OBJ_COUNT || st_retired(obj) || len > st_capacity(obj))
        return -1;
    cur = st_current(obj, &h);
    seq = cur < 0 ? 0u : h.seq;
    base = st_sector(obj, cur == 0 ? 1u : 0u);       /* write the other copy */
    uint32_t crc = st_crc32(src, len);
    for (off = 0; off < st_sectors(obj); off++)         /* the copy's sectors, its first (the commit record's) first */
        if ((rc = st_erase(st_proj(obj) ? st_proj_sector(obj, cur == 0 ? 1u : 0u, off) : base)) != 0) return rc;
    for (off = 0; off < len;) {                         /* (a piece never runs past its sector's payload, span, or a page) */
        uint32_t span, addr = st_address(obj, cur == 0 ? 1u : 0u, off, &span);
        uint32_t n = len - off > sizeof st_buf ? sizeof st_buf : len - off;
        if (n > span) n = span;
        if (n > 256u - (addr & 0xFFu)) n = 256u - (addr & 0xFFu);
        memcpy(st_buf, (const uint8_t *)src + off, n);
        if ((rc = st_prog(addr, st_buf, n)) != 0) return rc;
        off += n;
    }
    h.magic = ST_MAGIC;
    h.type = (uint16_t)obj;
    h.slot = (uint16_t)(cur == 0 ? 1 : 0);
    h.seq = seq + 1u;
    h.len = len;
    h.crc = crc;
    h.rsv[0] = h.rsv[1] = 0xFFFFFFFFu;
    h.hcrc = st_crc32(&h, sizeof h - 4u);
    if ((rc = st_prog(base, &h, sizeof h)) != 0)       /* the commit record, last */
        return rc;
    {   /* read back: a write-protected or failing part must not report SAVED */
        st_hdr_t chk;
        uint32_t c = cur == 0 ? 1u : 0u;
        if (st_head(obj, c, &chk) || memcmp(&chk, &h, sizeof h) || st_body(obj, c, &chk))
            return -7;
    }
    return 0;
}
