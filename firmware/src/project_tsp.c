/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* TSP1: Tessera's own project record (included by project.c; docs/design/PROJECT_FORMAT.md in tessera-notes). One
 * object a slot (storage.c OBJ_PROJ0..3) with all of the music, the generators and cords too, as parts:
 *
 *   "TSP1", u32 length (the whole record), u16 part count, u16 0; the parts; u32 FNV-1a of everything before it
 *   a part: u8 id, u8 version, u16 length, then its bytes
 *
 * A reader skips a part it does not know, and a part that is missing leaves its defaults, so a new feature adds a part
 * (or a new version of one) without a converter. A record is checked whole (tsp_valid) before anything of the music
 * playing is replaced (project_restore_runtime, then tsp_restore). Older records (FBKK, FUN) are not read: a slot
 * holding one reads as empty (Theo 2026-10-22: projects are disposable while this is built). Little-endian, as both the
 * FM-1 and the host tests are. */
#define TSP_MAGIC 0x31505354u                   /* "TSP1" */
#define TSP_HEAD 12u
#define TSP_STEP_BYTES 10u                      /* step v2: 80 bits */
enum { TSP_META = 1, TSP_TRACKS, TSP_STEPS, TSP_TIMING, TSP_SONG, TSP_MOTION, TSP_FM6, TSP_DRUM, TSP_REC, TSP_NAME,
       TSP_TESSERA, TSP_CORDS, TSP_SUB, TSP_MIDI, TSP_KEY, TSP_SEED, TSP_HUMAN, TSP_PARTS };   /* (TSP_SUB: each track's SHAPE; none: EVEN;
                                              * TSP_MIDI: each track's MIDI OUT channel; none: AUTO; TSP_KEY: KEY LINK; none: OWN;
                                              * TSP_SEED: each track's SOURCE, then each one's KEEP; none: OFF, FREE;
                                              * TSP_HUMAN: each track's HUMAN TIME; none: TWEAK HUMAN's timing, as it was) */
#if MELODEE_FLASH
_Static_assert(TSP_MAX <= ST_PROJ_MAX, "a record fits one project copy");
#endif

/* ---- writing */
typedef struct { uint8_t *b; uint32_t pos, part, n; int ok; } tsp_w_t;
static void tsp_byte(tsp_w_t *w, uint32_t v) { if (w->pos < TSP_MAX - 4u) w->b[w->pos++] = (uint8_t)v; else w->ok = 0; }
static void tsp_u16(tsp_w_t *w, uint32_t v) { tsp_byte(w, v); tsp_byte(w, v >> 8); }
static void tsp_bytes(tsp_w_t *w, const void *p, uint32_t n)
{
    if (w->pos + n > TSP_MAX - 4u) { w->ok = 0; return; }
    memcpy(w->b + w->pos, p, n);
    w->pos += n;
}
static void tsp_begin(tsp_w_t *w, uint32_t id, uint32_t ver)   /* (its length filled in by tsp_end) */
{
    tsp_byte(w, id); tsp_byte(w, ver); tsp_u16(w, 0);
    w->part = w->pos;
}
static void tsp_end(tsp_w_t *w)
{
    uint32_t len = w->pos - w->part;
    if (len > 0xFFFFu) { w->ok = 0; return; }
    w->b[w->part - 2u] = (uint8_t)len; w->b[w->part - 1u] = (uint8_t)(len >> 8);
    w->n++;
}

/* step v2, 80 bits: notes 4 x 7, kind 4 (n 0..4, 5 tie, 6 rest; +8 recorded), accent and slide 2, velocity 7, hits 8,
 * accents / recorded gate 8, chance 8, RATCH 2, NUDGE 4, the pitch accumulator 4 (0: not yet in RAM), RATCH's high bit 1
 * (x5..x8; 0 in a record of before it), 4 spare (0) */
static int tsp_step_pack(uint8_t *b, const step_t *s)
{
    uint32_t pos = 0, j;
    if (s->n > 4u || s->time > ST_REST || s->vel > 127u || s->probability > 101u || !step_acc_valid(s)) return 0;
    memset(b, 0, TSP_STEP_BYTES);
    for (j = 0; j < 4u; j++) { if (s->note[j] > 127u) return 0; proj_bits_put(b, &pos, s->note[j], 7u); }
    proj_bits_put(b, &pos, (s->time == ST_NOTE ? s->n : s->time == ST_TIE ? 5u : 6u) | (s->flags & SF_RECORDED ? 8u : 0u), 4u);
    proj_bits_put(b, &pos, s->flags & 3u, 2u);
    proj_bits_put(b, &pos, s->vel, 7u);
    proj_bits_put(b, &pos, s->hit, 8u);
    proj_bits_put(b, &pos, s->acc, 8u);
    proj_bits_put(b, &pos, s->probability, 8u);
    proj_bits_put(b, &pos, s->sub & 3u, 2u);
    proj_bits_put(b, &pos, (s->sub >> 2) & 15u, 4u);
    pos += 4u;                                          /* (the pitch accumulator's place: 0) */
    proj_bits_put(b, &pos, (s->sub >> 6) & 1u, 1u);
    return 1;
}
static int tsp_step_unpack(step_t *s, const uint8_t *b)
{
    uint32_t pos = 0, j, meta, r;
    memset(s, 0, sizeof *s);
    for (j = 0; j < 4u; j++) s->note[j] = (uint8_t)proj_bits_get(b, &pos, 7u);
    meta = proj_bits_get(b, &pos, 4u);
    s->n = (meta & 7u) < 5u ? (uint8_t)(meta & 7u) : 0u;
    s->time = (meta & 7u) < 5u ? ST_NOTE : (meta & 7u) == 5u ? ST_TIE : ST_REST;
    s->flags = (uint8_t)(proj_bits_get(b, &pos, 2u) | (meta & 8u ? SF_RECORDED : 0u));
    s->vel = (uint8_t)proj_bits_get(b, &pos, 7u);
    s->hit = (uint8_t)proj_bits_get(b, &pos, 8u);
    s->acc = (uint8_t)proj_bits_get(b, &pos, 8u);
    s->probability = (uint8_t)proj_bits_get(b, &pos, 8u);
    r = proj_bits_get(b, &pos, 2u);
    s->sub = (uint8_t)(r | proj_bits_get(b, &pos, 4u) << 2);
    pos += 4u;
    s->sub |= (uint8_t)(proj_bits_get(b, &pos, 1u) << 6);
    return (meta & 7u) < 7u && s->probability <= 101u && step_acc_valid(s);
}

/* the timed notes: the 32 owners' counts (11 bits each, as FUN's directory), then each note in 8 bytes */
static int tsp_rec_put(tsp_w_t *w, const project_t *q)
{
    uint8_t dir[PROJ_REC_DIR_BYTES];
    uint32_t owner, i, d;
    memset(dir, 0, sizeof dir);
    for (owner = 0; owner < NTRK * NPAT; owner++) {
        uint32_t count = 0;
        for (i = 0; i < RECORD_MAX; i++) count += q->recording[i].vel && (q->recording[i].owner & 31u) == owner;
        d = owner * 11u;
        proj_bits_put(dir, &d, count, 11u);
    }
    tsp_bytes(w, dir, sizeof dir);
    for (owner = 0; owner < NTRK * NPAT; owner++)
        for (i = 0; i < RECORD_MAX; i++) {
            const recorded_note_t *r = &q->recording[i];
            uint8_t v[8];
            uint32_t pos = 0;
            if (!r->vel || (r->owner & 31u) != owner) continue;
            if (!recording_valid(r)) return 0;
            memset(v, 0, sizeof v);
            proj_bits_put(v, &pos, r->on, 16); proj_bits_put(v, &pos, r->duration, 16);
            proj_bits_put(v, &pos, r->note, 7); proj_bits_put(v, &pos, r->vel, 7);
            proj_bits_put(v, &pos, r->step, 8); proj_bits_put(v, &pos, r->owner >> 5, 3);
            proj_bits_put(v, &pos, (uint32_t)(r->pitch + 24), 6); proj_bits_put(v, &pos, r->length, 1);
            tsp_bytes(w, v, sizeof v);
        }
    return 1;
}
static int tsp_rec_get(project_t *q, const uint8_t *b, uint32_t len)
{
    uint32_t owner, i, at = 0, d;
    const uint8_t *v = b + PROJ_REC_DIR_BYTES;
    if (len < PROJ_REC_DIR_BYTES) return 0;
    for (owner = 0; owner < NTRK * NPAT; owner++) {
        d = owner * 11u;
        uint32_t count = proj_bits_get(b, &d, 11u);
        if (count > RECORD_MAX - at || PROJ_REC_DIR_BYTES + (at + count) * 8u > len) return 0;
        for (i = 0; i < count; i++, at++, v += 8) {
            recorded_note_t *r = &q->recording[at];
            uint32_t pos = 0;
            r->on = (uint16_t)proj_bits_get(v, &pos, 16); r->duration = (uint16_t)proj_bits_get(v, &pos, 16);
            r->note = (uint8_t)proj_bits_get(v, &pos, 7); r->vel = (uint8_t)proj_bits_get(v, &pos, 7);
            r->step = (uint8_t)proj_bits_get(v, &pos, 8); r->owner = (uint8_t)(owner | proj_bits_get(v, &pos, 3) << 5);
            r->pitch = (int8_t)((int32_t)proj_bits_get(v, &pos, 6) - 24); r->length = (uint8_t)proj_bits_get(v, &pos, 1);
            if (!r->vel || !recording_valid(r)) return 0;
        }
    }
    return PROJ_REC_DIR_BYTES + at * 8u == len;
}

/* the highest Tessera block version this build knows: its own (tessera_pk_bytes: 0 past it) */
static uint32_t tsp_tess_ver(void)
{
    uint32_t v = 1;
    while (tessera_pk_bytes(v + 1u)) v++;
    return v;
}

/* q (and, with runtime, the live patterns, song chain, motion tags, generators and cords) -> raw; its length, 0 when q
 * holds a value no field can take. runtime commits each track's current pattern first, as saving always did */
static uint32_t tsp_pack(uint8_t *raw, const project_t *q, int runtime)
{
    tsp_w_t w = {raw, TSP_HEAD, 0, 0, 1};
    uint32_t t, b, i, f = motion_guard(), sum, magic = TSP_MAGIC;
    for (t = 0; t < NTRK; t++)
        if (q->pattern[t] >= NPAT || q->t[t].engine >= NENGINES || !drum_patch_valid(&q->drum[t])) { motion_unguard(f); return 0; }
    if (!chain_valid(&q->chain) || !motion_valid(&q->motion) || P_COUNT > 255u || G_COUNT > 255u) { motion_unguard(f); return 0; }
    memset(raw, 0, TSP_HEAD);

    tsp_begin(&w, TSP_META, 1);                        /* the selected track, the format bytes, the globals */
    tsp_byte(&w, q->sel); tsp_byte(&w, q->parts); tsp_byte(&w, q->phys); tsp_byte(&w, G_COUNT);
    for (i = 0; i < G_COUNT; i++) tsp_u16(&w, (uint16_t)q->g[i]);
    tsp_end(&w);

    tsp_begin(&w, TSP_TRACKS, 1);                      /* each track's engine, preset and parameters by id */
    tsp_byte(&w, P_COUNT);
    for (t = 0; t < NTRK; t++) {
        tsp_byte(&w, q->t[t].engine); tsp_byte(&w, q->t[t].preset);
        for (i = 0; i < P_COUNT; i++) tsp_u16(&w, (uint16_t)q->t[t].p[i]);
    }
    tsp_end(&w);

    tsp_begin(&w, TSP_STEPS, 1);                       /* the current pattern of each track, then all 32 patterns */
    for (t = 0; t < NTRK; t++) tsp_byte(&w, runtime ? trk[t].pattern : q->pattern[t]);
    for (t = 0; t < NTRK; t++) {
        uint32_t active = runtime ? trk[t].pattern : q->pattern[t];
        if (runtime) pattern_commit(&trk[t]);
        for (b = 0; b < NPAT; b++)
            for (i = 0; i < NSTEP; i++) {
                static const step_t empty = {{0}, 0, ST_REST, 0, 0, 0, 0, 0, 0};
                const step_t *s = b == active ? &q->t[t].step[i] : runtime ? &pattern_at(t, b)->step[i] : &empty;
                uint8_t v[TSP_STEP_BYTES];
                if (!tsp_step_pack(v, s)) { motion_unguard(f); return 0; }
                tsp_bytes(&w, v, sizeof v);
            }
    }
    tsp_end(&w);

    tsp_begin(&w, TSP_TIMING, 1);                      /* each pattern's LEN DIV SWING GATE */
    for (t = 0; t < NTRK; t++)
        for (b = 0; b < NPAT; b++)
            for (i = 0; i < 4u; i++) {
                uint32_t active = runtime ? trk[t].pattern : q->pattern[t];
                int16_t v = runtime ? pattern_at(t, b)->timing[i] : (b != active ? TP[P_SLEN + i].def : q->t[t].p[P_SLEN + i]);
                tsp_byte(&w, (uint8_t)v);
            }
    tsp_end(&w);

    tsp_begin(&w, TSP_SONG, 1);                        /* the song chain, each row's patterns */
    tsp_bytes(&w, &q->chain, sizeof q->chain);
    if (runtime) tsp_bytes(&w, chain_patterns, sizeof chain_patterns);
    else for (i = 0; i < sizeof chain_patterns; i++) tsp_byte(&w, 0);
    tsp_end(&w);

    tsp_begin(&w, TSP_MOTION, 1);                      /* the motion, each event's pattern */
    tsp_bytes(&w, &q->motion, sizeof q->motion);
    if (runtime) tsp_bytes(&w, motion_pattern, sizeof motion_pattern);
    else for (i = 0; i < sizeof motion_pattern; i++) tsp_byte(&w, 0);
    tsp_end(&w);

    tsp_begin(&w, TSP_FM6, 1);                         /* each track's FM6 patch and function settings */
    for (t = 0; t < NTRK; t++)
        for (i = 0; i < FM6_PACKED; i++) { if (q->fm6[t][i] > 127u) w.ok = 0; tsp_byte(&w, q->fm6[t][i]); }
    tsp_byte(&w, q->fm6_fn_ok);
    tsp_bytes(&w, q->fm6_fn, sizeof q->fm6_fn);
    tsp_end(&w);

    tsp_begin(&w, TSP_DRUM, 1);                        /* each track's kit offsets */
    for (t = 0; t < NTRK; t++) {
        uint8_t v[DRUM_WIRE_BYTES];
        drum_patch_pack(v, &q->drum[t]);
        tsp_bytes(&w, v, sizeof v);
    }
    tsp_end(&w);

    tsp_begin(&w, TSP_REC, 1);                         /* the timed notes */
    if (!tsp_rec_put(&w, q)) w.ok = 0;
    tsp_end(&w);

    tsp_begin(&w, TSP_NAME, 1);                        /* the name, 0-padded */
    {
        char n[PROJ_NAME_LEN + 1u];
        uint8_t v[PROJ_NAME_LEN];
        memset(v, 0, sizeof v);
        memcpy(v, n, proj_name_get(n, (const uint8_t *)q->name));
        tsp_bytes(&w, v, sizeof v);
    }
    tsp_end(&w);

    if (runtime) {                                     /* the generators and the cords, as they are now */
        uint8_t blk[TESSERA_PK_BYTES];
        tsp_begin(&w, TSP_TESSERA, 1);
        tsp_byte(&w, tsp_tess_ver());
        tessera_pack(blk);
        tsp_bytes(&w, blk, sizeof blk);
        tsp_end(&w);
        tsp_begin(&w, TSP_CORDS, 1);
        {
            uint8_t c[CORD_PK_BYTES];
            cord_pack(c);
            tsp_bytes(&w, c, sizeof c);
        }
        tsp_end(&w);
        tsp_begin(&w, TSP_SUB, 1);                     /* (SUB: each track's SHAPE, outside the Tessera block) */
        tsp_bytes(&w, tessera.sub_shape, NTRK);
        tsp_end(&w);
        tsp_begin(&w, TSP_MIDI, 1);                    /* (MIDI OUT: each track's channel) */
        tsp_bytes(&w, tessera.midi_ch, NTRK);
        tsp_end(&w);
        tsp_begin(&w, TSP_KEY, 1);                     /* (KEY LINK, the project's) */
        tsp_byte(&w, tessera.key_link);
        tsp_end(&w);
        tsp_begin(&w, TSP_SEED, 1);                    /* (STEP as seed: each track's SOURCE, each one's KEEP) */
        tsp_bytes(&w, tessera.sd_src, NTRK);
        tsp_bytes(&w, tessera.sd_keep, NTRK);
        tsp_end(&w);
        tsp_begin(&w, TSP_HUMAN, 1);                   /* (HUMAN TIME: each track's) */
        tsp_bytes(&w, tessera.sub_human, NTRK);
        tsp_end(&w);
    }
    motion_unguard(f);
    if (!w.ok) return 0;
    memcpy(raw, &magic, 4); memcpy(raw + 4, &(uint32_t){w.pos + 4u}, 4);
    raw[8] = (uint8_t)w.n; raw[9] = (uint8_t)(w.n >> 8);
    sum = proj_hash(raw, w.pos);
    memcpy(raw + w.pos, &sum, 4);
    return w.pos + 4u;
}

/* ---- reading */
static struct { uint32_t at[TSP_PARTS], len[TSP_PARTS]; uint8_t ver[TSP_PARTS]; } tsp_seen;   /* the record tsp_valid read */
static uint32_t tsp_u16_at(const uint8_t *b) { return b[0] | (uint32_t)b[1] << 8; }

/* raw (len bytes) is a whole TSP1 record whose every part checks out: 1, and proj_scratch holds its project (the
 * patterns other than the current ones, the song's and motion's pattern tags, the generators and the cords stay in raw
 * for tsp_restore). 0: not one (proj_scratch then undefined) */
static int tsp_valid(const uint8_t *raw, uint32_t len)
{
    project_t *q = &proj_scratch;
    uint32_t magic, size, sum, n, pos = TSP_HEAD, t, b, i, np = P_COUNT;
    if (len < TSP_HEAD + 4u || len > TSP_MAX) return 0;
    memcpy(&magic, raw, 4); memcpy(&size, raw + 4, 4); memcpy(&sum, raw + len - 4u, 4);
    if (magic != TSP_MAGIC || size != len || sum != proj_hash(raw, len - 4u)) return 0;
    n = tsp_u16_at(raw + 8);
    memset(&tsp_seen, 0, sizeof tsp_seen);
    for (i = 0; i < n; i++) {                          /* the part table: every part inside the record, once */
        uint32_t id, plen;
        if (pos + 4u > len - 4u) return 0;
        id = raw[pos]; plen = tsp_u16_at(raw + pos + 2u);
        if (pos + 4u + plen > len - 4u) return 0;
        if (id && id < TSP_PARTS) {
            if (tsp_seen.at[id]) return 0;
            tsp_seen.at[id] = pos + 4u; tsp_seen.len[id] = plen; tsp_seen.ver[id] = raw[pos + 1u];
        }
        pos += 4u + plen;
    }
    if (pos != len - 4u) return 0;

    memset(q, 0, sizeof *q);                           /* defaults for what a record leaves out */
    q->magic = PROJ_MAGIC; q->size = sizeof *q; q->parts = NPART; q->phys = PROJ_PHYS;
    for (i = 0; i < G_COUNT; i++) q->g[i] = GP[i].def;
    for (t = 0; t < NTRK; t++) {
        q->t[t].engine = 0;
        for (i = 0; i < P_COUNT; i++) q->t[t].p[i] = param_desc_of(0, i)->def;
        for (i = 0; i < NSTEP; i++) q->t[t].step[i].time = ST_REST;
    }
    chain_defaults(&q->chain);

    if (tsp_seen.at[TSP_META] && tsp_seen.ver[TSP_META] == 1u) {
        const uint8_t *p = raw + tsp_seen.at[TSP_META];
        uint32_t ng = p[3];
        if (tsp_seen.len[TSP_META] != 4u + 2u * ng || p[0] >= NTRK || (p[1] != 0u && p[1] != NPART) || p[2] > PROJ_PHYS) return 0;
        q->sel = p[0]; q->parts = p[1]; q->phys = p[2];
        for (i = 0; i < ng && i < G_COUNT; i++) q->g[i] = (int16_t)tsp_u16_at(p + 4u + 2u * i);
    }
    if (tsp_seen.at[TSP_TRACKS] && tsp_seen.ver[TSP_TRACKS] == 1u) {
        const uint8_t *p = raw + tsp_seen.at[TSP_TRACKS];
        np = p[0];
        if (np < 8u || np > P_COUNT || tsp_seen.len[TSP_TRACKS] != 1u + NTRK * (2u + 2u * np)) return 0;
        for (t = 0, p++; t < NTRK; t++) {
            int16_t values[P_COUNT], def[P_COUNT];
            q->t[t].engine = p[0]; q->t[t].preset = p[1];
            if (q->t[t].engine >= NENGINES) return 0;
            for (i = 0; i < np; i++) values[i] = (int16_t)tsp_u16_at(p + 2u + 2u * i);
            for (i = 0; i < P_COUNT; i++) def[i] = param_desc_of(q->t[t].engine, i)->def;
            params_by_count(q->t[t].p, values, np, def);
            p += 2u + 2u * np;
        }
    }
    if (tsp_seen.at[TSP_STEPS]) {
        const uint8_t *p = raw + tsp_seen.at[TSP_STEPS];
        if (tsp_seen.ver[TSP_STEPS] != 1u || tsp_seen.len[TSP_STEPS] != NTRK + NTRK * NPAT * NSTEP * TSP_STEP_BYTES) return 0;
        for (t = 0; t < NTRK; t++) { if (p[t] >= NPAT) return 0; q->pattern[t] = p[t]; }
        for (t = 0, p += NTRK; t < NTRK; t++)
            for (b = 0; b < NPAT; b++)
                for (i = 0; i < NSTEP; i++, p += TSP_STEP_BYTES) {
                    step_t s;
                    if (!tsp_step_unpack(&s, p)) return 0;
                    if (b == q->pattern[t]) q->t[t].step[i] = s;
                }
    }
    if (tsp_seen.at[TSP_TIMING]) {
        const uint8_t *p = raw + tsp_seen.at[TSP_TIMING];
        if (tsp_seen.ver[TSP_TIMING] != 1u || tsp_seen.len[TSP_TIMING] != NTRK * NPAT * 4u) return 0;
        for (t = 0; t < NTRK; t++)
            for (b = 0; b < NPAT; b++)
                for (i = 0; i < 4u; i++) {
                    int16_t v = p[(t * NPAT + b) * 4u + i];
                    if (v < TP[P_SLEN + i].min || v > TP[P_SLEN + i].max) return 0;
                    if (b == q->pattern[t]) q->t[t].p[P_SLEN + i] = v;   /* (the current pattern's: the track's) */
                }
    }
    if (tsp_seen.at[TSP_SONG]) {
        const uint8_t *p = raw + tsp_seen.at[TSP_SONG];
        if (tsp_seen.ver[TSP_SONG] != 1u || tsp_seen.len[TSP_SONG] != sizeof q->chain + sizeof chain_patterns) return 0;
        memcpy(&q->chain, p, sizeof q->chain);
        if (!chain_valid(&q->chain)) return 0;
        for (i = 0; i < sizeof chain_patterns; i++) if (p[sizeof q->chain + i] >= NPAT) return 0;
    }
    if (tsp_seen.at[TSP_MOTION]) {
        const uint8_t *p = raw + tsp_seen.at[TSP_MOTION], *tag;
        if (tsp_seen.ver[TSP_MOTION] != 1u || tsp_seen.len[TSP_MOTION] != sizeof q->motion + sizeof motion_pattern) return 0;
        memcpy(&q->motion, p, sizeof q->motion);
        proj_motion_ids(&q->motion, np);
        if (!motion_valid(&q->motion)) return 0;
        tag = p + sizeof q->motion;
        for (i = 0; i < q->motion.count; i++) {          /* (an event a pattern: no two alike in one) */
            if (tag[i] >= NPAT) return 0;
            for (uint32_t j = 0; j < i; j++)
                if (tag[i] == tag[j] && q->motion.event[i].place == q->motion.event[j].place &&
                    q->motion.event[i].param == q->motion.event[j].param) return 0;
        }
    }
    if (tsp_seen.at[TSP_FM6]) {
        const uint8_t *p = raw + tsp_seen.at[TSP_FM6];
        if (tsp_seen.ver[TSP_FM6] != 1u || tsp_seen.len[TSP_FM6] != sizeof q->fm6 + 1u + sizeof q->fm6_fn) return 0;
        for (t = 0; t < NTRK; t++)
            for (i = 0; i < FM6_PACKED; i++) { if (p[t * FM6_PACKED + i] > 127u) return 0; q->fm6[t][i] = p[t * FM6_PACKED + i]; }
        p += sizeof q->fm6;
        q->fm6_fn_ok = p[0] ? 1u : 0u;
        memcpy(q->fm6_fn, p + 1, sizeof q->fm6_fn);
        if (q->fm6_fn_ok) for (t = 0; t < NTRK; t++) if (!fm6_fn_ok(q->fm6_fn[t])) return 0;
    }
    if (tsp_seen.at[TSP_DRUM]) {
        const uint8_t *p = raw + tsp_seen.at[TSP_DRUM];
        if (tsp_seen.ver[TSP_DRUM] != 1u || tsp_seen.len[TSP_DRUM] != NTRK * DRUM_WIRE_BYTES) return 0;
        for (t = 0; t < NTRK; t++) if (!drum_patch_unpack(&q->drum[t], p + t * DRUM_WIRE_BYTES)) return 0;
    }
    if (tsp_seen.at[TSP_REC]) {
        if (tsp_seen.ver[TSP_REC] != 1u || !tsp_rec_get(q, raw + tsp_seen.at[TSP_REC], tsp_seen.len[TSP_REC])) return 0;
    }
    if (tsp_seen.at[TSP_NAME]) {
        char nm[PROJ_NAME_LEN + 1u];
        if (tsp_seen.ver[TSP_NAME] != 1u || tsp_seen.len[TSP_NAME] != PROJ_NAME_LEN) return 0;
        memcpy(q->name, nm, proj_name_get(nm, raw + tsp_seen.at[TSP_NAME]));
    }
    if (tsp_seen.at[TSP_TESSERA]) {
        const uint8_t *p = raw + tsp_seen.at[TSP_TESSERA];
        if (tsp_seen.ver[TSP_TESSERA] != 1u || tsp_seen.len[TSP_TESSERA] < 1u ||
            tsp_seen.len[TSP_TESSERA] != 1u + tessera_pk_bytes(p[0]) || !tessera_check(p[0], p + 1)) return 0;
    }
    if (tsp_seen.at[TSP_SUB]) {
        if (tsp_seen.ver[TSP_SUB] != 1u || tsp_seen.len[TSP_SUB] != NTRK) return 0;
        for (uint32_t k = 0; k < NTRK; k++) if (raw[tsp_seen.at[TSP_SUB] + k] >= SUB_SHAPES) return 0;
    }
    if (tsp_seen.at[TSP_MIDI]) {
        if (tsp_seen.ver[TSP_MIDI] != 1u || tsp_seen.len[TSP_MIDI] != NTRK) return 0;
        for (uint32_t k = 0; k < NTRK; k++) if (raw[tsp_seen.at[TSP_MIDI] + k] > TESS_MIDI_OFF) return 0;
    }
    if (tsp_seen.at[TSP_KEY]) {
        if (tsp_seen.ver[TSP_KEY] != 1u || tsp_seen.len[TSP_KEY] != 1u || raw[tsp_seen.at[TSP_KEY]] > 1u) return 0;
    }
    if (tsp_seen.at[TSP_SEED]) {
        const uint8_t *p = raw + tsp_seen.at[TSP_SEED];
        if (tsp_seen.ver[TSP_SEED] != 1u || tsp_seen.len[TSP_SEED] != 2u * NTRK) return 0;
        for (uint32_t k = 0; k < NTRK; k++) if (p[k] > TESSERA_TRACKS || p[NTRK + k] >= TESS_SD_KEEPS) return 0;
    }
    if (tsp_seen.at[TSP_HUMAN]) {
        if (tsp_seen.ver[TSP_HUMAN] != 1u || tsp_seen.len[TSP_HUMAN] != NTRK) return 0;
        for (uint32_t k = 0; k < NTRK; k++) if (raw[tsp_seen.at[TSP_HUMAN] + k] > TESS_HUMAN_MS) return 0;
    }
    if (tsp_seen.at[TSP_CORDS]) {
        if (tsp_seen.ver[TSP_CORDS] != 1u || tsp_seen.len[TSP_CORDS] != CORD_PK_BYTES ||
            !cord_check(raw + tsp_seen.at[TSP_CORDS])) return 0;
    }
    q->sum = proj_sum(q);
    return proj_ok(q);
}

/* after tsp_valid of raw and project_restore_runtime of its project: the other patterns, the song's and motion's
 * pattern tags, the generators and the cords (none in the record: OFF, none) */
static void tsp_restore(const uint8_t *raw)
{
    uint32_t t, b, i, f = motion_guard();
    chain_config = proj_scratch.chain;
    memset(chain_patterns, 0, sizeof chain_patterns);
    memset(motion_pattern, 0, sizeof motion_pattern);
    if (tsp_seen.at[TSP_SONG]) memcpy(chain_patterns, raw + tsp_seen.at[TSP_SONG] + sizeof proj_scratch.chain, sizeof chain_patterns);
    if (tsp_seen.at[TSP_MOTION]) memcpy(motion_pattern, raw + tsp_seen.at[TSP_MOTION] + sizeof proj_scratch.motion, sizeof motion_pattern);
    for (t = 0; t < NTRK; t++) {
        trk[t].pattern = proj_scratch.pattern[t]; trk[t].pattern_next = 0xff; trk[t].pattern_gen++;
        for (b = 0; b < NPAT; b++) {
            pattern_t *p = pattern_at(t, b);
            for (i = 0; i < 4u; i++)
                p->timing[i] = tsp_seen.at[TSP_TIMING] ? raw[tsp_seen.at[TSP_TIMING] + (t * NPAT + b) * 4u + i] : TP[P_SLEN + i].def;
            if (b == trk[t].pattern) memcpy(p->step, trk[t].step, sizeof p->step);
            else if (tsp_seen.at[TSP_STEPS])
                for (i = 0; i < NSTEP; i++)
                    tsp_step_unpack(&p->step[i], raw + tsp_seen.at[TSP_STEPS] + NTRK + ((t * NPAT + b) * NSTEP + i) * TSP_STEP_BYTES);
        }
    }
    motion_unguard(f);
    tessera_reset();                                   /* (generators OFF, no cords: a record without them) */
    if (tsp_seen.at[TSP_TESSERA]) tessera_unpack_ver(raw[tsp_seen.at[TSP_TESSERA]], raw + tsp_seen.at[TSP_TESSERA] + 1u);
    if (tsp_seen.at[TSP_CORDS]) (void)cord_unpack(raw + tsp_seen.at[TSP_CORDS]);
    if (tsp_seen.at[TSP_SUB]) memcpy(tessera.sub_shape, raw + tsp_seen.at[TSP_SUB], NTRK);
    if (tsp_seen.at[TSP_MIDI]) memcpy(tessera.midi_ch, raw + tsp_seen.at[TSP_MIDI], NTRK);
    tessera.key_link = tsp_seen.at[TSP_KEY] ? raw[tsp_seen.at[TSP_KEY]] : 0u;   /* (saved before KEY LINK: OWN) */
    if (tsp_seen.at[TSP_SEED]) {                       /* (saved before STEP as seed: tessera_reset's OFF, FREE) */
        memcpy(tessera.sd_src, raw + tsp_seen.at[TSP_SEED], NTRK);
        memcpy(tessera.sd_keep, raw + tsp_seen.at[TSP_SEED] + NTRK, NTRK);
    }
    if (tsp_seen.at[TSP_HUMAN])
        memcpy(tessera.sub_human, raw + tsp_seen.at[TSP_HUMAN], NTRK);
    else                                               /* (saved before HUMAN TIME: TWEAK HUMAN timed a generator's
                                                        * steps, up to 20 ms at 100; a STEP track's HUMAN did nothing) */
        for (t = 0; t < TESSERA_TRACKS; t++) {
            if (tessera.mode[t] == TESS_ENG_OFF) tessera.evolve[t].human = 0;
            else tessera.sub_human[t] = (uint8_t)((tessera.evolve[t].human + 2u) / 5u);
        }
}

/* a record's length as its header gives it (0: not a TSP1 record or one longer than TSP_MAX) */
static uint32_t tsp_len(const uint8_t *raw)
{
    uint32_t magic, size;
    memcpy(&magic, raw, 4); memcpy(&size, raw + 4, 4);
    return magic == TSP_MAGIC && size >= TSP_HEAD + 4u && size <= TSP_MAX ? size : 0u;
}
/* after a record was changed in place: its checksum again */
static void tsp_resum(uint8_t *raw)
{
    uint32_t len = tsp_len(raw), sum;
    if (!len) return;
    sum = proj_hash(raw, len - 4u);
    memcpy(raw + len - 4u, &sum, 4);
}

/* raw (a valid record, len bytes) renamed in place: its NAME part rewritten and its checksum; 1 done */
static int tsp_rename(uint8_t *raw, uint32_t len, const char *name)
{
    uint32_t n = str_len(name);
    uint8_t given[PROJ_NAME_LEN];
    char nm[PROJ_NAME_LEN + 1u];
    if (!tsp_valid(raw, len) || !tsp_seen.at[TSP_NAME]) return 0;
    memset(given, 0, sizeof given);
    memcpy(given, name, n < PROJ_NAME_LEN ? n : PROJ_NAME_LEN);
    memset(raw + tsp_seen.at[TSP_NAME], 0, PROJ_NAME_LEN);
    memcpy(raw + tsp_seen.at[TSP_NAME], nm, proj_name_get(nm, given));   /* (upper case, as a save stores it) */
    tsp_resum(raw);
    return tsp_valid(raw, len);
}
