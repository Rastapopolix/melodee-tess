/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Projects: four slots in NOR (storage.c OBJ_PROJ0..3), each a TSP1 record (project_tsp.c, Tessera 2026-10-22) with
 * all 32 pattern banks, the generators and the cords. Hardware retains only slot names/occupancy in RAM; full records
 * use one main-loop staging buffer. Host fixtures also provide a RAM storage backend (proj_bank_slot).
 * project_t below is a project decoded (a record read, the music captured): it is never stored as it is. Older
 * records (FUN, FBKK) are not read: a slot holding one reads as OLD FORMAT, and an archive of one is refused
 * (Theo 2026-10-22: projects are disposable while Tessera's own format is built). */
#define PROJ_MAGIC 0x46554E47u                 /* a decoded project_t's tag (proj_ok) */
#define PROJ_GLOBALS 27u                       /* the globals the template keeps (G_MFXTYPE on: the effects') */
#define PROJ_PHYS 2u                           /* project_t.phys: PHYS without DUST and DRUM (see the top) */
#define PROJ_NAME_LEN 12u                      /* the name: upper-case ASCII 32..126, 0-padded */
typedef struct {                               /* one track */
    int16_t p[P_COUNT];
    uint8_t engine, preset;
    step_t step[NSTEP];
} proj_trk_t;
typedef struct {
    uint32_t magic, size;
    int16_t g[G_COUNT];
    uint8_t sel;                               /* the selected track */
    uint8_t parts;                             /* NPART; 0: track 4 is the old GM drum part (see the top) */
    uint8_t phys;                              /* PROJ_PHYS: PHYS MODEL values as today; 1: MODEL 4 was DRUM;
                                                * 0 (a reserved byte before 1.0): MODEL 2 was DUST */
    uint8_t rsv;
    proj_trk_t t[NTRK];
    chain_config_t chain;
    motion_store_t motion;
    uint8_t fm6[NTRK][FM6_PACKED];             /* each track's FM6 patch, packed (eng_fm6.c) */
    uint8_t fm6_fn[NTRK][FM6_NFN];             /* .. and its function settings (fm6_fn_ok; else Dexed's): not in
                                                * a record of before them) */
    uint8_t fm6_fn_ok, rsv2[3];
    drum_patch_t drum[NTRK];
    recorded_note_t recording[RECORD_MAX];
    uint8_t pattern[NTRK];                     /* each track's current pattern */
    char name[PROJ_NAME_LEN];                  /* the project's name: upper-case ASCII 32..126, 0-padded; "" = none */
    uint32_t sum;
} __attribute__((may_alias)) project_t;
#define PROJ_CZ_BYTES (NTRK * 144u)            /* the template's retired CZ-1 tones (tmpl_t): reserved bytes now */
#define PROJ_P5_BYTES (NTRK * 138u)            /* .. and its Prophet programs */
#define PROJ_REC_DIR_BYTES 44u                 /* the timed notes' directory: 32 eleven-bit counts (0..1024) */
_Static_assert(G_MFXTYPE == PROJ_GLOBALS && G_FXMODE==40u && G_BUDGET==41u && G_COUNT==42u, "old global IDs stay fixed (the template)");
#define PROJ_DEF_SOUND 0xFFu                   /* preset byte: the track's power-on sound, no steps (kept from an
                                                * import of before 1.0) */
#define PROJ_DEF_KEEP 0xFEu                    /* .. the 808 DRUM sound, steps and the rest kept (old drums) */
#define TSP_MAX 32768u                         /* a TSP1 record at most (project_tsp.c): its RAM buffers are in the pool, beside the audio arena */
#if !defined(FM1_IRQ_TARGET)
static uint8_t proj_bank_slot[4][TSP_MAX];    /* host-only: each slot's TSP1 record (MELODEE_FLASH 0: the slots) */
static uint32_t proj_bank_len[4];             /* (0: empty) */
#endif
static struct { uint8_t used, old; char name[PROJ_NAME_LEN + 1u]; } proj_meta[4];   /* old: a record this firmware
                                                     * does not read (written before 1.0): shown, kept, never overwritten unasked */

static uint32_t proj_hash(const void *p, uint32_t n)   /* FNV-1a over n bytes */
{
    const uint8_t *b = (const uint8_t *)p;
    uint32_t i, s = 0x811C9DC5u;
    for (i = 0; i < n; i++)
        s = (s ^ b[i]) * 16777619u;
    return s;
}
static uint32_t proj_sum(const project_t *p) { return proj_hash(p, sizeof *p - 4u); }
static int proj_ok(const project_t *q)
{
    if (q->magic != PROJ_MAGIC || q->size != sizeof *q || q->sum != proj_sum(q) ||
        q->sel >= NTRK || (q->parts != 0u && q->parts != NPART) || q->phys > PROJ_PHYS ||
        !chain_valid(&q->chain) || !motion_valid(&q->motion)) return 0;
    for (uint32_t k = 0; k < NTRK; k++) if (q->t[k].engine >= NENGINES || q->pattern[k] >= NPAT || !drum_patch_valid(&q->drum[k])) return 0;
    for (uint32_t i = 0; i < RECORD_MAX; i++) if (!recording_valid(&q->recording[i])) return 0;
    return 1;
}

/* 12 stored name bytes -> d (PROJ_NAME_LEN + 1): up to the first 0, upper case; a byte outside 32..126 makes it
 * no name (a damaged tail never refuses the project) */
static uint32_t proj_name_get(char *d, const uint8_t *s)   /* its length */
{
    uint32_t i;
    for (i = 0; i < PROJ_NAME_LEN && s[i]; i++) {
        if (s[i] < 32u || s[i] > 126u) { i = 0; break; }
        d[i] = (char)(s[i] >= 'a' && s[i] <= 'z' ? s[i] - 32u : s[i]);
    }
    d[i] = 0;
    return i;
}

/* bits, least significant first (the steps and timed notes of project_tsp.c) */
static void proj_bits_put(uint8_t *b,uint32_t *pos,uint32_t v,uint32_t n)
{ while(n--){b[*pos>>3]|=(uint8_t)((v&1u)<<(*pos&7u));v>>=1;++*pos;} }
static uint32_t proj_bits_get(const uint8_t *b,uint32_t *pos,uint32_t n)
{uint32_t v=0;for(uint32_t i=0;i<n;i++,++*pos)v|=((uint32_t)((b[*pos>>3]>>(*pos&7u))&1u))<<i;return v;}

/* the motion of a record written with np parameters (np < P_COUNT, project_tsp.c TRACKS) -> today's ids:
 * an event names a parameter id, and the ids from that store's P_E0 (np - 8) on moved up with P_E0, as its
 * values did (params_by_count); the ones below kept theirs */
static void proj_motion_ids(motion_store_t *m, uint32_t np)
{
    uint32_t i;
    for (i = 0; i < m->count && i < MOTION_MAX; i++)
        if (!motion_native_event(m->event[i].param,m->event[i].value) && np < P_COUNT && m->event[i].param >= np - 8u && m->event[i].param < np)
            m->event[i].param = (uint8_t)(m->event[i].param + P_COUNT - np);
}

/* Old GM projects used this kit before DRUM existed. Its saved initializer is private now;
 * restore the same sound without passing its index through factory preset browsing. */
static void proj_legacy_perc(track_t *t)
{

    const preset_t *pr = &DRUM_PRESETS[0];
    t->eng_req = ENGI_DRUM;
    uint32_t i;
    t->preset = 0;                              /* display metadata; SET remains the legacy kit */
    for (i = 0; i < P_E0; i++)
        if (!param_kept(i))
            t->p[i] = TP[i].def;
    for (i = 0; i < 8u; i++)
        t->p[P_E0 + i] = pr->e[i];
    t->p[P_ATK] = pr->env[0];
    t->p[P_DEC] = pr->env[1];
    t->p[P_SUS] = pr->env[2];
    t->p[P_REL] = pr->env[3];
    t->p[P_ED_FLT] = pr->fenv;
    t->p[P_VOICE] = pr->mono ? V_LEGATO : V_POLY;
}

#ifdef CV_WORK_BYTES
/* Used synchronously by the main loop, outside drawing. Slot names are
 * cached in proj_meta; drawing never decodes a project into its own canvas.
 * may_alias on project_t covers the canvas's other typed view. */
_Static_assert(sizeof(project_t) <= CV_WORK_BYTES, "decoded project fits canvas scratch");
#define proj_scratch (*(project_t *)cv_work())
#else
static project_t proj_scratch __attribute__((section(".pool"))); /* standalone host fixtures */
#endif
static char proj_name[PROJ_NAME_LEN + 1u]    /* the name of the music as it is now (loaded, saved, the editor's */
    __attribute__((section(".pool")));       /* runtime restore); "" = none. A save takes it unless one is given */
#define PROJ_NO_SLOT 0xFFu
static uint8_t proj_cur = PROJ_NO_SLOT;      /* the slot the music was loaded from or last saved to (a rename of it
                                              * renames the music too); PROJ_NO_SLOT none (the editor's restore) */
static union {                               /* serialized main-loop work, also used for archive transfers */
    uint32_t align;
    uint8_t raw[TSP_MAX];                      /* a whole TSP1 record */
} proj_wire_u __attribute__((section(".pool")));
static uint8_t proj_wire_gen;                /* +1 whenever proj_wire is rewritten (a backup's runtime copy lives there) */
static uint8_t *proj_wire_borrow(void) { proj_wire_gen++; return proj_wire_u.raw; }   /* scratch for another job */

static void proj_steps(step_t *s)            /* a loaded sequence stays inside its fixed fields */
{
    uint32_t i, j;
    for (i = 0; i < NSTEP; i++) {
        if (s[i].n > 4u) s[i].n = 4;
        if (s[i].time > ST_REST) s[i].time = ST_REST;
        for (j = 0; j < 4u; j++) s[i].note[j] &= 127u;
        if (s[i].hit) s[i].acc &= s[i].hit;
        else if (s[i].time == ST_REST) s[i].acc = 0;
    }
}

#include "project_tsp.c"                      /* TSP1: the record a slot holds */

/* FNV of each slot's stored bytes, as last written or read (0: unknown): autosave writes only what differs */
static uint32_t proj_saved_hash[4];
/* slot -> proj_wire_u (its record) and proj_scratch (its project); the record's length, 0 = none / unreadable */
static int proj_read_slot(uint32_t slot)
{
    int n = -1, valid;
    slot &= 3u;
    proj_wire_gen++;
#if MELODEE_FLASH
    if (flash_ok) n = st_load(OBJ_SLOT(slot), proj_wire_u.raw, sizeof proj_wire_u.raw);
#else
    n = (int)proj_bank_len[slot];
    if (n) memcpy(proj_wire_u.raw, proj_bank_slot[slot], (uint32_t)n);
#endif
    valid = n > 0 && tsp_valid(proj_wire_u.raw, (uint32_t)n);
#if MELODEE_FLASH && !defined(FM1_IRQ_TARGET)
    if (valid) { memcpy(proj_bank_slot[slot], proj_wire_u.raw, (uint32_t)n); proj_bank_len[slot] = (uint32_t)n; }   /* (the host tests' view) */
#endif
    proj_meta[slot].used = (uint8_t)valid;
#if MELODEE_FLASH
    proj_meta[slot].old = (uint8_t)(!valid && flash_ok && st_exists(OBJ_SLOT(slot)));
#endif
    if (valid) proj_name_get(proj_meta[slot].name, (const uint8_t *)proj_scratch.name);
    else proj_meta[slot].name[0] = 0;
    proj_saved_hash[slot] = valid ? proj_hash(proj_wire_u.raw, (uint32_t)n) : 0u;
    return valid ? n : 0;
}
static void proj_fetch(uint32_t slot) { (void)proj_read_slot(slot); }
/* a record (len bytes; 0: the slot emptied) -> slot. proj_scratch holds its project. 0 done, 2 the write failed */
static int proj_write_slot(uint32_t slot, const uint8_t *raw, uint32_t len)
{
    slot &= 3u;
#if MELODEE_FLASH
    if (!flash_ok || st_save(OBJ_SLOT(slot), raw, len)) return 2;
#endif
#if !defined(FM1_IRQ_TARGET)
    memset(proj_bank_slot[slot], 0, sizeof proj_bank_slot[slot]);
    proj_bank_len[slot] = len;
    if (len) memcpy(proj_bank_slot[slot], raw, len);
#endif
    proj_meta[slot].used = len != 0;
    proj_meta[slot].old = 0;
    proj_saved_hash[slot] = len ? proj_hash(raw, len) : 0u;
    if (len) proj_name_get(proj_meta[slot].name, (const uint8_t *)proj_scratch.name);
    else proj_meta[slot].name[0] = 0;
    return 0;
}

static void project_capture(project_t *p)
{
    uint32_t i;
    uint8_t native[FP_SIZE + 1u];
    uint32_t f = motion_guard();
    memset(p, 0, sizeof *p);
    p->magic = PROJ_MAGIC;
    p->size = sizeof *p;
    for (i = 0; i < G_COUNT; i++)
        p->g[i] = song.g[i];
    p->sel = song.sel;
    p->parts = NPART;
    p->phys = PROJ_PHYS;
    p->chain = chain_config;
    for (i = 0; i < NTRK; i++) {
        for (uint32_t j = 0; j < P_COUNT; j++) p->t[i].p[j] = motion_base_value(&trk[i], j);
        p->t[i].engine = trk[i].eng_req;
        p->t[i].preset = trk[i].preset;
        p->pattern[i] = trk[i].pattern;
        memcpy(p->t[i].step, trk[i].step, sizeof trk[i].step);
        memcpy(native, fm6_patch[i], sizeof native);
        motion_native_snapshot(&trk[i], native, MO_TAG_FM6, sizeof native);
        fm6_pack(native, p->fm6[i]);
        memcpy(p->fm6_fn[i], fm6_fn[i], FM6_NFN);
    }
    memcpy(p->drum,drum_patch,sizeof drum_patch);
    p->fm6_fn_ok = 1;
    p->motion = motion;
    for (i = 0; i < RECORD_MAX; i++) p->recording[i] = recording_snapshot(i);
    p->motion.rsv[0] = 1;                     /* full container supplies the bank tags */
    motion_unguard(f);
    memcpy(p->name, proj_name, str_len(proj_name));
    p->sum = proj_sum(p);
}

/* the music as it is now -> slot, named `name` (0: the current name; "" none); 0 saved, nonzero refused or failed.
 * The current name becomes the saved one */
static int project_save_as(uint32_t slot, const char *name)
{
    project_t *p = &proj_scratch;
    if (transport_busy()) {                            /* a flash erase silences the audio and stalls the */
        ui_message("STOP TO SAVE");                     /* sequencer (storage_hw.c): only while stopped */
        return 1;
    }
    momentary_restore();
    project_capture(p);
    if (name) {
        memset(p->name, 0, sizeof p->name);
        memcpy(p->name, name, str_len(name) < PROJ_NAME_LEN ? str_len(name) : PROJ_NAME_LEN);
    }
    proj_wire_gen++;
    uint32_t len = tsp_pack(proj_wire_u.raw, p, 1);    /* (the generators and cords in it too) */
    if (!len) { ui_message("SAVE FORMAT ERROR"); return 2; }
    if (proj_write_slot(slot, proj_wire_u.raw, len)) { ui_message("SAVE ERROR"); return 2; }
    proj_name_get(proj_name, (const uint8_t *)p->name);
    proj_cur = (uint8_t)(slot & 3u);
    ui_message(MELODEE_FLASH ? "SAVED" : "SAVED (RAM)");
    return 0;
}
static int project_save(uint32_t slot) { return project_save_as(slot, 0); }

/* Hold SAVE or SAVE + REC (ui_input.c, stopped): the music back to the slot it was loaded from or last saved to, at once ("SAVED
 * B"); a new one (the template, the power-on sounds) has none yet: PROJECT opens on a free slot, SAVE picked */
static uint32_t project_free_slot(void);
static void project_quick_save(void)
{
    uint32_t i;
    if (proj_cur < 4u) {
        uint32_t slot = proj_cur;
        if (!project_save(slot)) {
            char b[12] = "SAVED A";
            b[6] = (char)('A' + slot);
            ui_message(b);
        }
        return;
    }
    song.g[G_SLOT] = (int16_t)(project_free_slot() + 1u);
    for (i = 0; i < NPAGES && PAGES[i].graph != GR_SLOTS; i++)
        ;
    ui.home = 0;
    ui.page = (uint8_t)i;
    page_entered();
    ui.act = 4;                                         /* (SAVE: OCT+ names and writes it) */
    ui.force = 1;
    ui_message("NEW PROJECT: PICK SLOT");
}
/* autosave (ui_input.c autosave_poll: stopped, untouched a while): the music back to the slot it was loaded from or last
 * saved to when it differs from what that slot holds ("SAVED B"); a new project (no slot yet) waits for a save */
static void project_autosave(void)
{
    project_t *p = &proj_scratch;
    uint32_t slot = proj_cur;
    if (slot >= 4u || transport_busy())
        return;
    project_capture(p);
    proj_wire_gen++;                                    /* (the staging RAM: a backup's copy there is gone) */
    uint32_t len = tsp_pack(proj_wire_u.raw, p, 1);
    if (!len || proj_hash(proj_wire_u.raw, len) == proj_saved_hash[slot])
        return;                                         /* as saved (the generators are in the record): nothing to write */
    project_quick_save();
}
static void project_cur_name(char *b) { str_cpy(b, proj_name, PROJ_NAME_LEN + 1u); }   /* b: 13 bytes */

/* slot's name -> b (PROJ_NAME_LEN + 1 bytes); 0 = an empty slot (b ""). Uses proj_scratch */
static int project_old(uint32_t slot) { return !project_used(slot) && proj_meta[slot & 3u].old; }
/* the slot's name ("" none): 1 a project, 0 empty; an old record's: OLD FORMAT (1: the slot is taken) */
static int project_name(uint32_t slot, char *b)
{
    b[0] = 0;
    if (project_old(slot)) { str_cpy(b, "OLD FORMAT", PROJ_NAME_LEN + 1u); return 1; }
    if (!project_used(slot)) return 0;
    str_cpy(b, proj_meta[slot & 3u].name, PROJ_NAME_LEN + 1u);
    return 1;
}

/* a stored project renamed in place (nothing else of it changes; the music playing is not touched, but the slot
 * it was loaded from / saved to: its name is the new one, the next SAVE's prefill): 0 done, 1 refused (playing,
 * an empty slot), 2 failed (the slot as it was) */
static int project_rename(uint32_t slot, const char *name)
{
    project_t *p = &proj_scratch;
    if (transport_busy()) {
        ui_message("STOP TO SAVE");
        return 1;
    }
    int n = proj_read_slot(slot);
    if (!n) { ui_message("EMPTY SLOT"); return 1; }
    if (!tsp_rename(proj_wire_u.raw, (uint32_t)n, name)) return 2;   /* (the NAME part, in place) */
    if (proj_write_slot(slot, proj_wire_u.raw, (uint32_t)n)) { ui_message("SAVE ERROR"); return 2; }
    if (proj_cur == (slot & 3u))
        proj_name_get(proj_name, (const uint8_t *)p->name);
#if MELODEE_FLASH
    if (flash_ok) { ui_message("RENAMED"); return 0; }
#endif
    ui_message("RENAMED (RAM)");
    return 0;
}

/* slot -> empty (PROJECT's Erase, after its question): an empty record, read as no project; 0 done */
static int project_erase(uint32_t slot)
{
    if (transport_busy()) {
        ui_message("STOP TO SAVE");
        return 1;
    }
    slot &= 3u;
    if (proj_write_slot(slot, proj_wire_u.raw, 0)) { ui_message("SAVE ERROR"); return 2; }
    if (proj_cur == slot)
        proj_cur = PROJ_NO_SLOT;
#if MELODEE_FLASH
    if (flash_ok) { ui_message("ERASED"); return 0; }
#endif
    ui_message("ERASED (RAM)");
    return 0;
}

/* CLK TUNE MIDI ROUT: the device's, not a project's (sync and routing belong to the setup, as on Elektron and Squarp
 * boxes): kept as last used (settings ext.glo), and a project load, a backup or the template leaves them as they are.
 * Projects still carry them (the record's layout is unchanged) */
static const uint8_t GLO_KEPT[4] = {G_CLOCK, G_TUNE, G_MIDI, G_ROUTE};
static int glo_kept(uint32_t i)
{
    for (uint32_t k = 0; k < 4u; k++)
        if (GLO_KEPT[k] == i) return 1;
    return 0;
}

static int project_restore_runtime(const project_t *input)
{
    momentary_restore();
    project_t *p = &proj_scratch;
    uint32_t i, k;
    if (!proj_ok(input)) return 1;
    if(designer_on() && designer_quit)designer_quit(0);   /* a loaded project owns the new sound */
    if (p != input) memcpy(p, input, sizeof *p);
    transport_req = 2;
    panic_req = (1u << NTRK) - 1u;
    fm1_irq_off();                                      /* the audio ISR must not see half a project */
    seq_stop();
    fx_clear_all();
    transport_req = 0;
    pattern_init();
    memcpy(recording, p->recording, sizeof recording);
    memcpy(drum_patch,p->drum,sizeof drum_patch);
    recording_reindex();
    chain_defaults(&chain_config);
    motion = p->motion;
    memset(motion_active, 0, sizeof motion_active);
    memset(motion_nactive, 0, sizeof motion_nactive);   /* (the patches come with the project) */
    motion_base_valid = 0;
    ui.song_row = 0;
    for (i = 0; i < G_COUNT; i++)
        if (i != G_SLOT && i != G_LOAD && i != G_SAVE && !glo_kept(i))
            song.g[i] = global_reserved(i) ? 0 : (int16_t)clamp(p->g[i], GP[i].min, GP[i].max);
    for (k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        const proj_trk_t *s = &p->t[k];
        uint32_t e = s->engine % NENGINES, gone = !ENG_BUILT(e);   /* (gone: an engine left out of the build, */
        if (gone)                                                     /* Tessera: the fallback's first voice (FM's, or */
            e = ENGI_FALLBACK;                                        /* SID's INIT), the common settings kept, core.h) */
        t->eng_req = (uint8_t)e;
        t->user = 0; t->user_native=0;                                    /* (no user preset slot is saved) */
        for (i = 0; i < P_COUNT; i++) {                 /* every value back inside its range */
            const param_desc_t *d = param_desc_of(e, i);
            t->p[i] = param_reserved(i) ? 0 : gone && i >= P_E0 ? d->def : eng_extra_retired(e) && i >= P_E0 ? s->p[i] :
                      (int16_t)clamp(s->p[i], d->min, d->max);
        }
        t->preset = gone ? 0u : eng_extra_retired(e) ? s->preset : (uint8_t)(ENGINES[e]->npresets ? (s->preset >= PROJ_DEF_KEEP ? 0u : s->preset) % ENGINES[e]->npresets : 0u);
        t->pattern = p->pattern[k];
        memcpy(t->step, s->step, sizeof t->step);
        proj_steps(t->step);
        pattern_commit(t);
        {   /* the project's own FM6 patch */
            uint8_t v[FP_SIZE + 1u];
            fm6_unpack(p->fm6[k], v);
            fm6_set_patch(k, v);
            memcpy(fm6_fn[k], p->fm6_fn_ok && fm6_fn_ok(p->fm6_fn[k]) ? p->fm6_fn[k] : FM6_FNDEF, FM6_NFN);
        }
    }
    song.sel = (uint8_t)(p->sel < NTRK ? p->sel : 0u);
    fm1_irq_on();
    proj_name_get(proj_name, (const uint8_t *)p->name);
    proj_cur = PROJ_NO_SLOT;                            /* (project_load: its slot) */
    undo_clear();                                       /* (ui.c) the undo levels belong to the old project */
    undo_depth++;                                       /* and these loads take none */
    for (k = 0; k < NTRK; k++) {                        /* the power-on sounds: format 1 (tracks 2..4), old drums */
        track_t *t = &trk[k];
        int16_t keep[P_COUNT];
        if (p->t[k].preset == PROJ_DEF_SOUND) {
            apply_preset_to(t, TRK_DEF_V1[k]);
            track_defaults_steps(t);
        } else if (p->t[k].preset == PROJ_DEF_KEEP) {   /* the steps, LEVEL PAN MUTE, LEN DIV SWING GATE kept */
            memcpy(keep, t->p, sizeof keep);
            fm1_irq_off();                           /* publish the legacy sound as one bounded parameter batch */
            proj_legacy_perc(t);                      /* track-owned parameters stay */
            for (i = P_AMODE; i <= P_TRANS; i++)        /* the drum part had no arp or scale */
                t->p[i] = TP[i].def;
            fm1_irq_on();
        }
        pat_sig[k] = ~steps_sig(t);                     /* a project's steps are the user's */
    }
    scale_share(TSEL);                                  /* (older projects: one scale per part, the selected wins) */
    undo_depth--;
    sync_reload = 1;
    ui.force = 1;
    ui_message("LOADED");
    return 0;
}
static void project_load(uint32_t slot)
{
    int n = proj_read_slot(slot);
    if (!n) { ui_message(project_old(slot) ? "OLD FORMAT" : "EMPTY SLOT"); return; }
    if (!project_restore_runtime(&proj_scratch)) {
        tsp_restore(proj_wire_u.raw);                  /* (the other patterns, the generators and the cords) */
        proj_cur = (uint8_t)(slot & 3u);
    }
}

/* settings + learned panel table: one flash object. The flash copy wins at
 * boot (the .noinit copies are garbage after a power-off). */
#include "settings_persist.c"

/* The template (SAVE > PROJECT, SLOT TMPL): the music without its patterns: the globals, each track's sound
 * (engine, preset, parameters, FM6 patch and function settings) and the selected track. SAVE there keeps it, LOAD makes a new project
 * from it (every pattern empty, LEN / DIV / SWG / GATE their defaults, no name, SLOT on a free slot), and power-on
 * loads it when BOOT is OFF or its slot empty. No flash sector is free: it follows the settings in their record
 * (set_rec, its size and magic last). P_COUNT, G_COUNT or FM6_PACKED changing changes it (the assert): convert. */
#define TMPL_MAGIC 0x4A4C5054u                    /* "TPLJ": selectable send routing; TPLI / TPLH / TPLG / TPLF convert */
typedef struct {
    int16_t g[PROJ_GLOBALS];
    uint8_t sel, rsv;                             /* (G_COUNT odd: the tracks start on a word) */
    struct { uint8_t engine, preset; int16_t p[P_COUNT]; } t[NTRK];
    uint8_t fm6[NTRK][FM6_PACKED];
    uint8_t fm6_fn[NTRK][FM6_NFN];                /* (TPL6) */
    uint8_t tones[PROJ_CZ_BYTES + PROJ_P5_BYTES];   /* (retired CZ-1 tones, Prophet programs: 0, ignored) */
    drum_patch_t drum[NTRK];
    uint8_t master[16];
    uint32_t size, magic;                         /* last: the record's end */
} tmpl_t;
typedef struct {
    int16_t g[PROJ_GLOBALS];
    uint8_t sel, rsv;                             /* (G_COUNT odd: the tracks start on a word) */
    struct { uint8_t engine, preset; int16_t p[108]; } t[NTRK];
    uint8_t fm6[NTRK][FM6_PACKED];
    uint8_t fm6_fn[NTRK][FM6_NFN];                /* (TPL6) */
    uint8_t tones[PROJ_CZ_BYTES + PROJ_P5_BYTES];
    drum_patch_t drum[NTRK];
    uint32_t size, magic;                         /* last: the record's end */
} tmpl_fx_single_t;
typedef struct {
    int16_t g[PROJ_GLOBALS];
    uint8_t sel, rsv;                             /* (G_COUNT odd: the tracks start on a word) */
    struct { uint8_t engine, preset; int16_t p[104]; } t[NTRK];
    uint8_t fm6[NTRK][FM6_PACKED];
    uint8_t fm6_fn[NTRK][FM6_NFN];                /* (TPL6) */
    uint8_t tones[PROJ_CZ_BYTES + PROJ_P5_BYTES];
    drum_patch_t drum[NTRK];
    uint32_t size, magic;                         /* last: the record's end */
} tmpl_fx_legacy_t;
_Static_assert(sizeof(tmpl_t) == 2u * PROJ_GLOBALS + 2u + NTRK * (2u + 2u * P_COUNT) + NTRK * (FM6_PACKED + FM6_NFN) + PROJ_CZ_BYTES + PROJ_P5_BYTES + NTRK*sizeof(drum_patch_t) + 16u + 8u,
               "template layout (P_COUNT, G_COUNT, FM6_PACKED: a conversion)");
static struct { persist_t p; tmpl_t t; } set_rec __attribute__((section(".pool")));   /* the settings record */
#define tmpl set_rec.t                                /* the template lives where the settings record stages it */
_Static_assert(sizeof set_rec == sizeof(persist_t) + sizeof(tmpl_t), "the template follows the settings");
static uint8_t tmpl_dirty;                        /* saved in RAM, not yet in flash (settings_poll) */
static uint8_t persist_old, tmpl_old;
static int template_old(void) { return persist_old || tmpl_old; }
static void settings_old_accept(void) { persist_old = tmpl_old = 0; }

static int template_used(void) { return tmpl.magic == TMPL_MAGIC && tmpl.size == sizeof tmpl; }

/* a stored template (len bytes, as tmpl_t is laid out) is today's: its size and magic, valid tones */
static uint32_t tmpl_param_count(uint32_t len)
{
    return len==sizeof(tmpl_t)?P_COUNT:len==sizeof(tmpl_t)-16u?117u:len==sizeof(tmpl_t)-40u?116u:len==sizeof(tmpl_fx_single_t)?108u:len==sizeof(tmpl_fx_legacy_t)?104u:0u;
}
static uint32_t tmpl_blob_schema(const uint8_t *b,uint32_t len)
{
    uint32_t np=tmpl_param_count(len),size,magic;if(!np)return 0;
    memcpy(&size,b+len-8u,4);memcpy(&magic,b+len-4u,4);
    return size==len && magic==(np==P_COUNT?TMPL_MAGIC:np==117u?0x494C5054u:np==116u?0x484C5054u:np==108u?0x474C5054u:0x464C5054u) ? np : 0u;
}
static int tmpl_blob_valid(const uint8_t *b,uint32_t len)
{
    uint32_t np=tmpl_blob_schema(b,len);
    if(!np || b[2u*PROJ_GLOBALS]>=NTRK)return 0;
    uint32_t delta=NTRK*2u*(P_COUNT-np);
    for(uint32_t k=0;k<NTRK;k++){
        if(b[2u*PROJ_GLOBALS+2u+k*(2u+2u*np)]>=NENGINES)return 0;
        if(!drum_patch_valid((const drum_patch_t *)(b+__builtin_offsetof(tmpl_t,drum)-delta+k*sizeof(drum_patch_t))))return 0;
    }
    if(np>=117u){uint32_t off=__builtin_offsetof(tmpl_t,master)-delta;
        for(uint32_t i=0;i<13u;i++)if(b[off+i]>GP[G_MFXTYPE+i].max)return 0;
        if(b[off+13u]>(np==P_COUNT?1u:0u))return 0;
        for(uint32_t i=14u;i<16u;i++)if(b[off+i])return 0;
    }
    return 1;
}
static int tmpl_take(const uint8_t *b,uint32_t len)
{
    memset(&tmpl,0,sizeof tmpl);if(!tmpl_blob_valid(b,len))return 0;
    uint32_t np=tmpl_param_count(len),off=2u*PROJ_GLOBALS+2u;
    memcpy(tmpl.g,b,sizeof tmpl.g);tmpl.sel=b[2u*PROJ_GLOBALS];
    for(uint32_t k=0;k<NTRK;k++){
        int16_t def[P_COUNT],values[P_COUNT];tmpl.t[k].engine=b[off++];tmpl.t[k].preset=b[off++];
        for(uint32_t i=0;i<P_COUNT;i++)def[i]=param_desc_of(tmpl.t[k].engine%NENGINES,i)->def;
        memcpy(values,b+off,np*2u);off+=np*2u;params_by_count(tmpl.t[k].p,values,np,def);
    }
    uint32_t native=__builtin_offsetof(tmpl_t,master)-__builtin_offsetof(tmpl_t,fm6);memcpy(tmpl.fm6,b+off,native);
    for(uint32_t i=0;i<FX_SLOTS*FX_PARAMS;i++){uint32_t id=G_MFXTYPE+i;uint32_t v=np>=117u?b[off+native+i]:(uint32_t)GP[id].def;if(v>GP[id].max)return 0;tmpl.master[i]=(uint8_t)v;}
    tmpl.master[12]=np>=117u?b[off+native+12u]:(uint8_t)GP[G_FXRETURN].def;if(tmpl.master[12]>127u)return 0;
    tmpl.master[13]=np==P_COUNT?b[off+native+13u]:0;
    tmpl.magic=TMPL_MAGIC;tmpl.size=sizeof tmpl;
    for(uint32_t k=0;k<NTRK;k++)if(!fm6_fn_ok(tmpl.fm6_fn[k]))memcpy(tmpl.fm6_fn[k],FM6_FNDEF,FM6_NFN);
    return 1;
}

static void glo_restore(void)                     /* power-on: the device's as last used (GLO_KEPT) */
{
    uint32_t k;
    for (k = 0; k < 4u; k++)
        song.g[GLO_KEPT[k]] = (int16_t)clamp(settings_glo[k], GP[GLO_KEPT[k]].min, GP[GLO_KEPT[k]].max);
}
/* main loop: a kept value changed (a knob, the editor, a project load): saved 1.5 s after the last change, stopped */
static void glo_poll(void)
{
    static uint32_t t;
    static uint8_t dirty;
    uint32_t k;
    for (k = 0; k < 4u; k++)
        if (song.g[GLO_KEPT[k]] != settings_glo[k]) {
            settings_glo[k] = song.g[GLO_KEPT[k]];
            dirty = 1;
            t = fm1_ms;
        }
    if (dirty && fm1_ms - t > 1500u && !song.playing && !seq_counting() && !momentary.active) {
        dirty = 0;
        settings_save();
    }
}

/* the music without its patterns -> the template, saved with the settings */
static void template_save(void)
{
    momentary_restore();
    uint32_t i, k;
    if (transport_busy()) {
        ui_message("STOP TO SAVE");
        return;
    }
    if (template_old()) { confirm_open(CF_OLD_TEMPLATE, 0); return; }
    memset(&tmpl, 0, sizeof tmpl);
    for (i = 0; i < PROJ_GLOBALS; i++)
        tmpl.g[i] = song.g[i];
    for(i=0;i<FX_SLOTS*FX_PARAMS;i++)tmpl.master[i]=(uint8_t)song.g[G_MFXTYPE+i];
    tmpl.master[12]=(uint8_t)song.g[G_FXRETURN];
    tmpl.master[13]=(uint8_t)song.g[G_FXMODE];
    tmpl.sel = song.sel;
    for (k = 0; k < NTRK; k++) {
        tmpl.t[k].engine = trk[k].eng_req;
        tmpl.t[k].preset = trk[k].preset;
        for (i = 0; i < P_COUNT; i++)
            tmpl.t[k].p[i] = motion_base_value(&trk[k], i);
        tmpl.drum[k]=drum_patch[k];
        fm6_pack(fm6_patch[k], tmpl.fm6[k]);
        memcpy(tmpl.fm6_fn[k], fm6_fn[k], FM6_NFN);
    }
    tmpl.size = sizeof tmpl;
    tmpl.magic = TMPL_MAGIC;
    tmpl_dirty = 1;
#if MELODEE_FLASH
    if (flash_ok) {
        settings_save();                              /* (stopped: written now) */
        ui_message(tmpl_dirty ? "SAVE ERROR" : "TEMPLATE SAVED");
        return;
    }
#endif
    ui_message("TEMPLATE SAVED (RAM)");
}

/* the first empty project slot (0..3), else 0 */
static uint32_t project_free_slot(void)
{
    uint32_t i;
    for (i = 0; i < 4u; i++)
        if (!project_used(i) && !project_old(i))
            return i;
    return 0;
}

/* a new project from the template: its sounds and globals, every pattern empty; SLOT on a free slot */
static void template_load(void)
{
    momentary_restore();
    project_t *p = &proj_scratch;
    uint32_t i, k;
    if (!template_used()) {
        ui_message("NO TEMPLATE");
        return;
    }
    memset(p, 0, sizeof *p);
    p->magic = PROJ_MAGIC;
    p->size = sizeof *p;
    for (i = 0; i < PROJ_GLOBALS; i++)
        p->g[i] = tmpl.g[i];
    for(i=0;i<FX_SLOTS*FX_PARAMS;i++)p->g[G_MFXTYPE+i]=tmpl.master[i];
    p->g[G_FXRETURN]=tmpl.master[12];
    p->g[G_FXMODE]=tmpl.master[13];
    p->sel = tmpl.sel;
    p->parts = NPART;
    p->phys = PROJ_PHYS;
    chain_defaults(&p->chain);
    for (k = 0; k < NTRK; k++) {
        p->t[k].engine = tmpl.t[k].engine;
        p->t[k].preset = tmpl.t[k].preset;
        for (i = 0; i < P_COUNT; i++)
            p->t[k].p[i] = tmpl.t[k].p[i];
        for (i = 0; i < 4u; i++)                      /* LEN DIV SWG GATE: as a new track's */
            p->t[k].p[P_SLEN + i] = TP[P_SLEN + i].def;
        p->drum[k]=tmpl.drum[k];
        memcpy(p->fm6[k], tmpl.fm6[k], FM6_PACKED);
        memcpy(p->fm6_fn[k], tmpl.fm6_fn[k], FM6_NFN);
    }
    p->fm6_fn_ok = 1;
    p->sum = proj_sum(p);
    if (project_restore_runtime(p))
        return;
    tessera_reset();                                  /* (the restore stopped the sequencer: no note is held) */
    for (k = 0; k < NTRK; k++) {
        track_defaults_steps(&trk[k]);                /* every pattern empty */
        pat_last[k] = 0;
        pat_sig[k] = steps_sig(&trk[k]);
    }
    proj_cur = PROJ_NO_SLOT;
    song.g[G_SLOT] = (int16_t)(project_free_slot() + 1u);
    ui_message("TEMPLATE LOADED");
}

/* NEW SONG (ui_new.c): the template's key and tempo (track 1's ROOT, the song's SCALE, BPM), when there is one */
static void template_key(uint32_t *root, uint32_t *scale, int32_t *bpm)
{
    if (!template_used())
        return;
    *root = (uint32_t)tmpl.t[0].p[P_ROOT];
    *scale = (uint32_t)tmpl.t[0].p[P_SCALE];
    *bpm = tmpl.g[G_BPM];
}
/* .. no template: the power-on sounds (melodee_init's), every pattern, the song and the motion empty, the globals at
 * their defaults but CLK TUNE MIDI ROUT (the device's); a new project: no slot, no name, SLOT on a free one */
static void project_new_blank(void)
{
    uint32_t i, k;
    int16_t kept[4];
    momentary_restore();
    tessera_stop_all();                               /* release what a Tessera track holds, then OFF */
    tessera_reset();
    for (k = 0; k < 4u; k++)
        kept[k] = song.g[GLO_KEPT[k]];
    fm1_irq_off();
    chain_defaults(&chain_config);
    pattern_init();
    fm1_irq_on();
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    for (k = 0; k < 4u; k++)
        song.g[GLO_KEPT[k]] = kept[k];
    song.g[G_BPM] = TRK_DEF_BPM;
    undo_depth++;
    for (k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        track_defaults(t);
        set_engine_of(t, TRK_DEF[k][0]);
        apply_preset_to(t, TRK_DEF[k][1]);
        if (TRK_DEF[k][0] != ENGI_DRUM)
            t->p[P_SCALE] = TRK_DEF_SCALE;
        track_defaults_steps(t);
        pat_last[k] = 0;
        pat_sig[k] = steps_sig(t);
    }
    undo_depth--;
    undo_clear();
    proj_cur = PROJ_NO_SLOT;
    proj_name[0] = 0;
    song.sel = 0;
    song.g[G_SLOT] = (int16_t)(project_free_slot() + 1u);
    cord_defaults();                                  /* (Tessera: a new project's mods: none, cord.c) */
}
/* the music differs from its slot (none: it holds notes): a new song asks first. Packs the project (proj_wire_u) */
static int project_dirty(void)
{
    uint32_t k;
    if (proj_cur < 4u) {
        project_capture(&proj_scratch);
        proj_wire_gen++;
        uint32_t len = tsp_pack(proj_wire_u.raw, &proj_scratch, 1);
        return !len || proj_hash(proj_wire_u.raw, len) != proj_saved_hash[proj_cur];
    }
    for (k = 0; k < NTRK; k++)
        if (!seq_is_empty(&trk[k]) || notes_have_recording(&trk[k]))
            return 1;
    return 0;
}

/* power-on, after melodee_init: the BOOT project (SAVE > PROJECT KNOB 2) in place of the default sounds, SLOT on it
 * so SAVE writes back there. BOOT OFF or an empty BOOT slot: a new project, from the template if one is saved, SLOT
 * on a free slot. A boot that crashed or hung within 30 s (bootguard) skips the project or the template: one that
 * cannot load does not lock the FM-1 out */
static void project_boot(void)
{
    if (settings_boot && project_used(settings_boot - 1u)) {
        song.g[G_SLOT] = (int16_t)settings_boot;
        if (bootguard.failed)
            ui_message("BOOT PROJECT SKIPPED");
        else
            project_load(settings_boot - 1u);
        return;
    }
    if (template_used() && !bootguard.failed)
        template_load();
    else {
        if (template_used())
            ui_message("TEMPLATE SKIPPED");
        cord_defaults();                              /* (Tessera: a new project's mods: none) */
    }
    song.g[G_SLOT] = (int16_t)(project_free_slot() + 1u);
}
#if MELODEE_FLASH && MELODEE_SLICE
#include "slice_store.c"                          /* SLICE's MAN slices, kept in the user slots */
#endif
#if MELODEE_FLASH
static persist_t persist_saved;
static uint8_t persist_pending;                 /* 1 requested, 2 waiting after a flash error */
static uint8_t persist_repair;                  /* boot repair must retry even when RAM equals defaults */
static uint32_t persist_retry_ms;
#endif

static void persist_boot(void)                    /* before settings_init / panel_init */
{
#if MELODEE_FLASH
    persist_t p;
    boot_work.state = 0;
    uint32_t f = irq_save();
    flash_ok = FL_FAR(fl_jedec_ram)() == 0x856014u;       /* the expected 1 MiB part, else stay RAM-only */
    irq_restore(f);
    if (!flash_ok)
        return;
    fl_plain_window_init();                        /* flash above 0x93000 reads as plaintext through XIP
                                                    * (user sample sets are played from there) */
#if MELODEE_SLICE
    slc_store_boot();                              /* (the scans read each slot's stored slices) */
#endif
    {   /* the settings, then a template if one follows them */
        int n = st_load(OBJ_SETTINGS, &proj_wire_u, sizeof proj_wire_u);
        int tail_ok = n == (int)sizeof p || (n > (int)sizeof p &&
            tmpl_blob_schema((const uint8_t *)&proj_wire_u + sizeof p, (uint32_t)n - sizeof p));
        int owned = 0, foreign, future_owner;
        persist_t original;
        memset(&p, 0, sizeof p);
        if (n >= (int)sizeof p) memcpy(&p, &proj_wire_u, sizeof p);
        original = p;
        owned = p.magic == PERSIST_MAGIC && n >= (int)sizeof p && settings_owned(&p);
        future_owner = p.magic == PERSIST_MAGIC && n >= (int)sizeof p &&
            !memcmp(p.favorites.factory[1] + 28, SETTINGS_OWNER, 3) && !owned;
        /* Never truncate a foreign extension into our ext block. Legacy PER5
         * is shared with forks, so both settings and the whole tail must match. */
        foreign = future_owner || (!owned && !(settings_legacy_valid(&p) && tail_ok));
        persist_old = tmpl_old = 0;
        persist_pending = persist_repair = 0;
        memset(&tmpl, 0, sizeof tmpl);
        if (!foreign && n > (int)sizeof set_rec.p) {
            tmpl_old = (uint8_t)!tmpl_take((const uint8_t *)&proj_wire_u + sizeof set_rec.p, (uint32_t)n - sizeof set_rec.p);
        }
        if (foreign) {
            memset(&p, 0, sizeof p);
            p.magic = PERSIST_MAGIC;
            p.palette = palette_to_stored(UI_DEFAULT_INDEX);
            p.panel = PANEL_DEFAULT;
            for (uint32_t k = 0; k < 4u; k++) p.ext.glo[k] = GP[GLO_KEPT[k]].def;
        }
        settings_import(&p, sizeof p);            /* replaces retained RAM too, before USB enumeration */
        panel_init(); settings_init();
        for (uint32_t k = 0; k < 4u; k++) {
            const param_desc_t *d = &GP[GLO_KEPT[k]];
            if (settings_glo[k] < d->min || settings_glo[k] > d->max) settings_glo[k] = d->def;
        }
        settings_export(&p);
        persist_saved = p;
        persist_old = (uint8_t)future_owner;       /* an unknown Melodee schema is protected, not foreign */
        /* Unknown owned templates stay protected. Otherwise commit defaults
         * or the ownership/normalization repair once through settings' A/B save. */
        if (!template_old() && (foreign || memcmp(&p, &original, sizeof p))) {
            BOOT_WORK_BEGIN(foreign ? "RESETTING SETTINGS" : "CHECKING SETTINGS", 1);
            set_rec.p = p;
            if (st_save(OBJ_SETTINGS, &set_rec, sizeof p + (template_used() ? sizeof tmpl : 0u))) {
                persist_repair = 1;
                persist_pending = 2;
                persist_retry_ms = fm1_ms;
                BOOT_WORK_END(1);
            } else {
                tmpl_dirty = 0;
                BOOT_WORK_PROGRESS(1);
                BOOT_WORK_END(0);
            }
        }
    }
    {   /* projects: fill empty RAM slots from flash, so the slot list is right after power-on */
        uint32_t i;
        for (i = 0; i < 4u; i++)
            if (!project_used(i))
                proj_fetch(i);
    }
    settings_init();                               /* maintenance uses the saved palette */
    up_boot();                                     /* user presets */
#endif
}

static int project_used(uint32_t slot)
{
#if !MELODEE_FLASH
    slot &= 3u;
    int valid = proj_bank_len[slot] && tsp_valid(proj_bank_slot[slot], proj_bank_len[slot]);
    proj_meta[slot].used = (uint8_t)valid;
    if (valid) proj_name_get(proj_meta[slot].name, (const uint8_t *)proj_scratch.name);
#endif
    return proj_meta[slot & 3u].used;
}

/* Main loop only: no flash access or copies when the ISR changes rows. */
static uint32_t chain_prepare(void)
{
    if (transport_busy()) return 2;
    if (!chain_valid(&chain_config) || !chain_config.count) return 1;
    for (uint32_t i = 0; i < chain_config.count; i++)
        for (uint32_t k = 0; k < NTRK; k++) if (chain_patterns[i][k] >= NPAT) return 1;
    uint32_t f = motion_guard();
    if (transport_busy()) { motion_unguard(f); return 2; }
    for (uint32_t k = 0; k < NTRK; k++) { pattern_commit(&trk[k]); trk[k].pattern_next = 0xff; }
    chain.config = chain_config;
    RING_PUBLISH(); chain.armed = 1; transport_req = 1;
    motion_unguard(f);
    return 0;
}

static void settings_poll(void)
{
#if MELODEE_FLASH
    persist_t p;
#if MELODEE_SLICE
    slc_store_poll();                              /* SLICE's slices edited on the SLICES page */
#endif
    if (!persist_pending || !flash_ok || transport_busy() ||
        (persist_pending == 2u && (uint32_t)(fm1_ms - persist_retry_ms) < 1000u))
        return;
#ifdef FM1_IRQ_TARGET
    /* Live preference edits queue their flash save until audio is idle. */
    for(uint32_t k=0;k<NPART;k++)for(uint32_t i=0;i<NVOICE;i++)if(trk[k].v[i].active)return;
#if MELODEE_USB_AUDIO
    if(ua.play_alt || ua.cap_alt)return;
#endif
#endif
    if (template_old()) {
        if (!ui.confirm && !ui.name_active) {
            persist_pending = 0;
            confirm_open(CF_OLD_SETTINGS, 0);
        }
        return;
    }
    p = persist_saved;
    settings_export(&p);
    if (!persist_repair && !memcmp(&p, &persist_saved, sizeof p) && !tmpl_dirty) {
        persist_pending = 0;
        return;                                    /* unchanged: no erase cycle */
    }
    set_rec.p = p;                                 /* the record: the settings, the template (tmpl) after them */
    uint8_t repairing = persist_repair;
    if (repairing) BOOT_WORK_BEGIN("RESETTING SETTINGS", 1);
    if (st_save(OBJ_SETTINGS, &set_rec, sizeof set_rec.p + (template_used() ? sizeof set_rec.t : 0u)) == 0) {
        persist_saved = p;
        persist_pending = 0;
        persist_repair = 0;
        tmpl_dirty = 0;
        if (repairing) { BOOT_WORK_PROGRESS(1); BOOT_WORK_END(0); }
    } else {
        persist_pending = 2;
        persist_retry_ms = fm1_ms;
        if (repairing) BOOT_WORK_END(1);
    }
#endif
}

static void settings_save(void)
{
#if MELODEE_FLASH
    persist_pending = 1;
#endif
    settings_poll();
}

#if MELODEE_FLASH
_Static_assert(sizeof(persist_t) <= ST_PAYLOAD_MAX, "settings do not fit one flash sector");
#endif
