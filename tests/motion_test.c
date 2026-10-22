/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Real sequencer/project/UI sources: motion lifecycle and compact migration. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

/* two decoded projects hold the same music (a record keeps no CZ-1 / Prophet tones and compacts the timed notes) */
static int same_music(const project_t *a, const project_t *b)
{
    int ok = !memcmp(a->g, b->g, sizeof a->g) && a->sel == b->sel && !memcmp(a->pattern, b->pattern, sizeof a->pattern) &&
             !memcmp(&a->motion, &b->motion, sizeof a->motion) && !memcmp(a->fm6, b->fm6, sizeof a->fm6) &&
             !memcmp(a->drum, b->drum, sizeof a->drum) && !memcmp(a->name, b->name, sizeof a->name);
    for (uint32_t t = 0; t < NTRK; t++)
        ok &= a->t[t].engine == b->t[t].engine && a->t[t].preset == b->t[t].preset &&
              !memcmp(a->t[t].p, b->t[t].p, sizeof a->t[t].p) && !memcmp(a->t[t].step, b->t[t].step, sizeof a->t[t].step);
    return ok;
}
static int motion_recording(void)
{
    int bad = 0; ui_power_on(); track_t *t = &trk[0];
    t->p[P_LEVEL] = 23; song.sel = 0; song.rec = 1;
    seq_start(); seq_tick(t, CTL); /* first real step */
    t->p[P_LEVEL] = 92;
    bad += check("motion records the selected armed track at its current step", !motion_capture(t, P_LEVEL, 92) &&
        motion.count == 1u && motion.event[0].place == 0u && motion.event[0].value == 92);
    t->p[P_LEVEL] = 110; motion_capture(t, P_LEVEL, 110);
    bad += check("same step/parameter overwrites rather than consuming capacity", motion.count == 1u && motion.event[0].value == 110);
    bad += check("sounding values never replace the original patch base", t->p[P_LEVEL] == 110 && motion_base_value(t, P_LEVEL) == 23);
    project_t q; static uint8_t packed[TSP_MAX];
    project_capture(&q);
    bad += check("project snapshot saves base + independent events while sounding", q.t[0].p[P_LEVEL] == 23 && q.motion.event[0].value == 110 &&
        tsp_pack(packed, &q, 0) != 0u);
    seq_stop();
    bad += check("stop before another step restores the original parameter", t->p[P_LEVEL] == 23);
    seq_start(); seq_tick(t, CTL);
    bad += check("recorded motion plays back at step zero", t->p[P_LEVEL] == 110);
    motion_set_enabled(t, 0);
    bad += check("bypass restores base without deleting automation", t->p[P_LEVEL] == 23 && motion_count(t) == 1u && !motion_enabled(t));
    motion_set_enabled(t, 1); motion_step(t, 0, &motion);
    song.rec = 0; t->p[P_LEVEL] = 37; motion_capture(t, P_LEVEL, 37); seq_stop();
    bad += check("manual edits outside recording become the new base", t->p[P_LEVEL] == 37);
    song.rec = 3; seq_start(); seq_tick(t, CTL);
    trk[1].p[P_LEVEL] = 45; motion_capture(&trk[1], P_LEVEL, 45);
    bad += check("only the selected track captures knob motion", motion_count(&trk[1]) == 0u);
    bad += check("transport and engine-switch parameters cannot be captured", !motion_param(P_SLEN) && !motion_param(P_AMODE) &&
        motion_set_event(t, 0, P_SLEN, 8) == 1);
    seq_stop(); motion_clear(t);
    t->p[P_LEVEL] = 42; song.rec = 1; seq_start(); seq_tick(t, CTL);
    motion_clear(t); t->p[P_LEVEL] = 70; motion_capture(t, P_LEVEL, 70); seq_stop();
    bad += check("clearing then recording mid-play keeps the correct new base", t->p[P_LEVEL] == 42);
    return bad;
}
static int motion_capacity(void)
{
    int bad = 0; ui_power_on();
    for (uint32_t i = 0; i < MOTION_MAX; i++) bad += motion_set_event(&trk[0], i, P_LEVEL, (int16_t)i);
    motion_store_t saved = motion;
    bad += check("full event pool refuses append without overwriting earlier events", motion_set_event(&trk[1], 0, P_LEVEL, 90) == 2 &&
        !memcmp(&motion, &saved, sizeof saved));
    bad += check("full pool still permits a targeted overwrite", !motion_set_event(&trk[0], 12, P_LEVEL, 100) && motion.count == MOTION_MAX);
    motion_delete_event(&trk[0], 12, P_LEVEL);
    bad += check("deleting one event releases one slot", motion.count == MOTION_MAX - 1u && !motion_set_event(&trk[1], 0, P_LEVEL, 90));
    saved = motion; motion_store_t invalid = motion; invalid.count = MOTION_MAX + 1;
    bad += check("invalid restore leaves the pool intact", motion_replace_track(&trk[0], &invalid) != 0 && !memcmp(&motion, &saved, sizeof saved));
    motion_clear(&trk[0]);
    bad += check("track clear keeps every other track's events", motion.count == 1u && motion.event[0].place >> 6 == 1u);
    return bad;
}
static int probability_playback(void)
{
    int bad = 0; ui_power_on(); track_t *t = &trk[0];
    step_t s = {{60, 64, 0, 0}, 2, ST_NOTE, 0, 90, 1u << DV_KICK, 0, 0};
    bad += check("zero-initialized probability remains legacy 100 percent", step_chance(&s) == 100u);
    step_set_chance(&s, 0); seq_step(t, &s, div_samples(2), 0);
    bad += check("zero percent suppresses the whole chord and drum hits", step_chance(&s) == 0u && !t->seq_n);
    step_set_chance(&s, 100); seq_step(t, &s, div_samples(2), 0);
    bad += check("100 percent plays all chord notes and drum hits", t->seq_n == 3u);
    seq_release(t); step_set_chance(&s, 50); uint32_t heard = 0;
    for (uint32_t i = 0; i < 1000u; i++) { seq_step(t, &s, div_samples(2), 0); heard += t->seq_n != 0; seq_release(t); }
    bad += check("chance is evaluated each repeat with one decision per step", heard > 350u && heard < 650u);
    return bad;
}
static int compact_project(void)
{
    int bad = 0; ui_power_on(); track_t *t = &trk[0];
    t->step[3] = (step_t){{60, 67}, 2, ST_NOTE, SF_ACCENT, 110, 0x81, 0x80, 0};
    step_set_chance(&t->step[3], 25); t->p[P_FM1_LEVEL] = 80; t->p[P_ED_FLT] = -50;
    motion_set_event(t, 3, P_LEVEL, 110);
    static project_t before, after; static uint8_t packed[TSP_MAX], corrupt[TSP_MAX];
    uint32_t n;
    project_capture(&before);
    bad += check("a project packs into a record of at most a copy", (n = tsp_pack(packed, &before, 0)) != 0u && n <= TSP_MAX);
    fm6_fn[0][FN_PTIME] = 33;
    project_capture(&before);
    {
        static uint8_t fbk[TSP_MAX];
        bad += check("TSP1 keeps the tracks' FM6 function settings", tsp_pack(fbk, &before, 1) &&
                     tsp_valid(fbk, tsp_len(fbk)) && proj_scratch.fm6_fn_ok && proj_scratch.fm6_fn[0][FN_PTIME] == 33u);
    }
    n = tsp_pack(packed, &before, 0);
    bad += check("TSP1 round trip preserves signed values/FM params/probability/motion", n && tsp_valid(packed, n) &&
        (after = proj_scratch, same_music(&before, &after)));
    memcpy(corrupt, packed, n); corrupt[n / 2u] ^= 1u;
    bad += check("TSP1 torn or corrupted payload is refused", !tsp_valid(corrupt, n));
    memcpy(corrupt, packed, n); tsp_valid(corrupt, n);   /* (track 1's P_LEVEL far out of its range: clamped when loaded) */
    corrupt[tsp_seen.at[TSP_TRACKS] + 3u + 2u * P_LEVEL] = 0xFF; corrupt[tsp_seen.at[TSP_TRACKS] + 4u + 2u * P_LEVEL] = 0x7F;
    tsp_resum(corrupt);
    bad += check("a parameter out of its range reads as it is and is clamped when it loads", tsp_valid(corrupt, n) &&
        proj_scratch.t[0].p[P_LEVEL] == 32767);
    memcpy(corrupt, packed, n); tsp_valid(corrupt, n);
    {   /* (track 1's step 8 in its current pattern: its chance byte 255) */
        uint32_t pos = 57u;
        proj_bits_put(corrupt + tsp_seen.at[TSP_STEPS] + NTRK + ((uint32_t)before.pattern[0] * NSTEP + 7u) * TSP_STEP_BYTES, &pos, 255u, 8u);
    }
    tsp_resum(corrupt);
    bad += check("invalid probability is refused even with a correct hash", !tsp_valid(corrupt, n));
    project_save(1); motion_clear(t); t->p[P_FM1_LEVEL] = 127; step_set_chance(&t->step[3], 100);
    project_load(1);
    bad += check("actual project save/load restores motion, chance and operator settings", motion_count(t) == 1u &&
        step_chance(&t->step[3]) == 25u && t->p[P_FM1_LEVEL] == 80);
    return bad;
}
/* a TSP1 record written by a firmware of np parameters (np < P_COUNT: before parameters were added): its TRACKS part
 * with the first np - 8 values, then the engine's E0..E7; its MOTION with m (its events as that firmware numbered
 * them). The other parts as today's. out holds the record; its length */
static uint32_t pack_np(uint8_t *out, const project_t *q, const motion_store_t *m, uint32_t np)
{
    static uint8_t today[TSP_MAX];
    uint32_t n = tsp_pack(today, q, 0), in = 12u, o = 12u, t, i, parts, sum;
    memcpy(out, today, 12);
    parts = today[8] | (uint32_t)today[9] << 8;
    for (uint32_t k = 0; k < parts; k++) {
        uint32_t id = today[in], len = today[in + 2u] | (uint32_t)today[in + 3u] << 8, at = o;
        out[o++] = (uint8_t)id; out[o++] = today[in + 1u]; o += 2u;
        if (id == TSP_TRACKS) {
            out[o++] = (uint8_t)np;
            for (t = 0; t < NTRK; t++) {
                out[o++] = q->t[t].engine; out[o++] = q->t[t].preset;
                for (i = 0; i < np; i++) {
                    uint16_t v = (uint16_t)q->t[t].p[i < np - 8u ? i : i - (np - 8u) + P_E0];
                    out[o++] = (uint8_t)v; out[o++] = (uint8_t)(v >> 8);
                }
            }
        } else if (id == TSP_MOTION) {
            memcpy(out + o, m, sizeof *m); o += sizeof *m;
            memset(out + o, 0, MOTION_MAX); o += MOTION_MAX;
        } else {
            memcpy(out + o, today + in + 4u, len); o += len;
        }
        out[at + 2u] = (uint8_t)(o - at - 4u); out[at + 3u] = (uint8_t)((o - at - 4u) >> 8);
        in += 4u + len;
    }
    (void)n;
    o += 4u;
    memcpy(out + 4, &o, 4);
    sum = proj_hash(out, o - 4u); memcpy(out + o - 4u, &sum, 4);
    return o;
}
static int older_count(void)
{
    int bad = 0, ok; ui_power_on(); track_t *t = &trk[0];
    static project_t before, after; static uint8_t old[TSP_MAX]; motion_store_t m; uint32_t n;
    const uint32_t np = 104u, e0 = np - 8u;
    uint32_t i, k;
    for (k = 0; k < NTRK; k++)
        for (i = 0; i < 8u; i++) trk[k].p[P_E0 + i] = (int16_t)(ENGINES[trk[k].eng_req]->edit[i].min + (int16_t)(k + i) %
            (ENGINES[trk[k].eng_req]->edit[i].max - ENGINES[trk[k].eng_req]->edit[i].min + 1));
    t->p[P_FM1_ATK] = 33; t->p[P_LEVEL] = 20;
    t->step[2] = (step_t){{60, 64, 67}, 3, ST_NOTE, 0, 100};
    project_capture(&before);
    memset(&m, 0, sizeof m);                                       /* as that firmware numbered them */
    m.count = 3; m.on = 1;
    m.event[0] = (motion_event_t){3, P_LEVEL, 90};
    m.event[1] = (motion_event_t){5, (uint8_t)e0, 40};             /* its P_E0 */
    m.event[2] = (motion_event_t){6, P_FM1_ATK, 20};               /* below its P_E0: the same id then and now */
    n = pack_np(old, &before, &m, np);
    ok = tsp_valid(old, n);
    after = proj_scratch;
    for (k = 0; ok && k < NTRK; k++) {
        for (i = 0; i < 8u; i++) ok &= after.t[k].p[P_E0 + i] == before.t[k].p[P_E0 + i];
        for (i = 0; i < e0; i++) ok &= after.t[k].p[i] == before.t[k].p[i];
        for (i = e0; i < P_E0; i++) ok &= after.t[k].p[i] == param_desc_of(after.t[k].engine, i)->def;
    }
    bad += check("a record of P_COUNT 104: E0..E7 in place, FX defaults, the rest kept", ok &&
        after.t[0].p[P_FM1_ATK] == 33 && !memcmp(after.t[0].step, before.t[0].step, sizeof before.t[0].step));
    bad += check("  its motion: its E0 -> P_E0, REV and FM OP1 ATK kept",
        after.motion.count == 3u && after.motion.event[0].param == P_LEVEL && after.motion.event[1].param == P_E0 &&
        after.motion.event[1].value == 40 && after.motion.event[2].param == P_FM1_ATK);
    n = tsp_pack(old, &after, 0);
    bad += check("  written again with today's count: the same project", n && tsp_valid(old, n) &&
        old[tsp_seen.at[TSP_TRACKS]] == P_COUNT && same_music(&proj_scratch, &after));
    n = pack_np(old, &before, &m, np); memcpy(proj_bank_slot[2], old, n); proj_bank_len[2] = n;
    project_load(2);
    bad += check("such a project's motion loads at today's parameter ids", motion.count == 3u &&
        motion.event[1].param == P_E0 && trk[0].pattern == 0u);
    m.event[1].param = (uint8_t)(e0 + 1u);                          /* (any id of its E0 .. E7 moves by 2) */
    n = pack_np(old, &before, &m, np);
    bad += check("  its E1 -> P_E1", tsp_valid(old, n) && proj_scratch.motion.event[1].param == P_E1);
    return bad;
}
static int loads_and_song(void)
{
    int bad = 0; ui_power_on(); track_t *t = &trk[0];
    t->p[P_LEVEL] = 21; motion_set_event(t, 0, P_LEVEL, 100); t->step[0] = (step_t){{60}, 1, ST_NOTE, 0, 100};
    apply_preset_to(t, 1);
    bad += check("sound load clears incompatible motion and keeps pattern", !motion_count(t) && t->step[0].note[0] == 60);
    undo_swap(); bad += check("sound undo restores the original motion pool and base", motion_count(t) == 1u && t->p[P_LEVEL] == 21);
    undo_step(1); bad += check("sound redo restores the loaded motion state", !motion_count(t));
    undo_swap(); pattern_commit(t); pattern_request(t, 1);
    t->p[P_LEVEL] = 43; t->step[0].note[0] = 72; chain_config.count = 1; chain_config.row[0] = (chain_row_t){0, 1};
    bad += check("song preparation imports saved motion alongside steps", chain_prepare() == 0 && motion_count(t) == 0u);
    seq_start(); seq_tick(t, CTL);
    bad += check("song plays saved automation with current instruments", chain.running && t->p[P_LEVEL] == 100 && t->step[0].note[0] == 60);
    seq_stop(); bad += check("song stop restores current base and editable pattern", t->p[P_LEVEL] == 43 && t->step[0].note[0] == 72);
    return bad;
}
static int repeat_mode(void)
{
    int bad = 0; ui_power_on(); track_t *t = &trk[0];
    t->p[P_AMODE] = 6; t->p[P_AOCT] = 4; t->p[P_APROB] = 127;
    arp_add(t, 72); arp_add(t, 60);
    arp_tick(t, CTL);
    bad += check("REPEAT retriggers the last played note without octave traversal", t->arp_note == 60);
    t->arp_pos = 0xFFFFFFF; arp_tick(t, CTL);
    bad += check("REPEAT remains on the same note on the next pulse", t->arp_note == 60);
    arp_remove(t, 60); arp_remove(t, 72); arp_tick(t, CTL);
    bad += check("REPEAT releases after the last held key is released", !t->nheld && !t->arp_note);
    t->p[P_AHOLD] = 1; arp_add(t, 65); arp_remove(t, 65); t->arp_pos = 0xFFFFFFF; arp_tick(t, CTL);
    bad += check("REPEAT supports ARP HOLD", t->nheld == 1u && t->arp_note == 65);
    return bad;
}
static int native_motion_test(void)
{
    int bad = 0;
    const uint32_t engines[] = {ENGI_FM6};          /* (the Prophet's and CZ-1's: retired 2026-10-22) */
    const uint32_t bytes[] = {FP_OL};
    for (uint32_t e = 0; e < 1u; e++) {
        ui_power_on(); track_t *t = TSEL; set_engine_of(t, engines[e]); apply_preset_to(t, 0);
        uint32_t i = bytes[e], tag = motion_native_tag(t), id = MO_NATIVE + i;
        uint8_t old[MO_NATIVE_N], base = *motion_native_at(t, i), changed = base == 80u ? 70u : 80u;
        song.rec = 1; seq_start(); seq_tick(t, CTL);
        motion_native_peek(t, old); *motion_native_at(t, i) = changed; motion_native_edited(t, old);
        bad += check("native edit records tagged motion and keeps its patch base", motion.count == 1u &&
            motion.event[0].param == id && motion.event[0].value == (int16_t)(tag << 8 | changed) && motion_nbase[0][i] == base);
        static project_t q, restored; static uint8_t packed[TSP_MAX]; uint8_t unpacked[FP_SIZE + 1u]; uint32_t pn;
        project_capture(&q); fm6_unpack(q.fm6[0], unpacked);
        bad += check("native project snapshot saves the original patch while motion sounds",
            unpacked[i] == base &&
            (pn = tsp_pack(packed, &q, 0)) && tsp_valid(packed, pn) &&
            (restored = proj_scratch, restored.motion.event[0].value == (int16_t)(tag << 8 | changed)));
        seq_stop(); bad += check("native stop immediately restores the original patch", *motion_native_at(t, i) == base);
        song.rec = 0; seq_start(); seq_tick(t, CTL);
        bad += check("native motion replays from the real sequencer", *motion_native_at(t, i) == changed);
        motion_set_enabled(t, 0);
        bad += check("native bypass restores the patch without losing events", *motion_native_at(t, i) == base && motion.count == 1u);
        motion_set_enabled(t, 1); motion_step(t, 0, &motion);
        motion_native_peek(t, old); *motion_native_at(t, i) = 65; motion_native_edited(t, old); seq_stop();
        bad += check("native manual edit outside REC becomes the new patch base", *motion_native_at(t, i) == 65);
        int16_t lane_value;
        bad += check("native motion has a named lane and the raw value", (mo_name(id|256u, (char *)unpacked, sizeof unpacked), unpacked[0]) &&
            mo_value(id|256u, 0, &lane_value) && lane_value == changed);
        motion_clear_param(t, id|256u);
        bad += check("native lane clear removes its events", !motion.count);
        bad += check("native persisted project restores base then replays its motion", !project_restore_runtime(&restored));
        seq_start(); seq_tick(t, CTL);
        bad += check("native saved events replay after loading", *motion_native_at(TSEL, i) == changed);
        seq_stop(); bad += check("loaded native motion restores the saved patch", *motion_native_at(TSEL, i) == base);
    }
    ui_power_on(); set_engine_of(TSEL, ENGI_FM6); apply_preset_to(TSEL, 0);
    go_home(); song.rec = 1; turn(EN_K1, -1);
    bad += check("armed stopped Stage edit explains how to record motion", !motion.count && msg_is("PLAY TO RECORD MOTION"));
    seq_stop();
    ui_power_on(); set_engine_of(TSEL, ENGI_FM6); apply_preset_to(TSEL, 0);
    for (uint32_t p = 0; p < NPAGES; p++) if (PAGES[p].scope == SC_FMOP) { ui.home = 0; ui.page = p; break; }
    song.rec = 1; seq_start(); seq_tick(TSEL, CTL); turn(EN_K1, -1);
    bad += check("FM6 operator page records its native patch through UI", motion.count && (uint32_t)motion.event[0].value >> 8 == MO_TAG_FM6);
    seq_stop();
    ui_power_on(); set_engine_of(TSEL, ENGI_FM6); uint32_t page = 0;
    for (uint32_t p = 0; p < NPAGES; p++) if (PAGES[p].graph == GR_MOTION) page = p;
    bad += check("empty motion page is hidden", !page_visible(page));
    motion_set_event(TSEL, 0, P_LEVEL, 50);
    bad += check("recorded motion makes its page visible", page_visible(page));
    ui.home = 0; ui.page = page; motion_clear(TSEL);
    bad += check("clearing keeps the current page until leaving it", page_visible(page));
    go_home(); bad += check("empty motion disappears after leaving", !page_visible(page));
    motion_set_event(TSEL, 0, MO_NATIVE + 10u, MO_TAG_P5 << 8 | 60);   /* (a retired Prophet's) */
    bad += check("another engine's native events cannot expose an empty motion page", !page_visible(page));
    motion_clear(TSEL);
    bad += check("invalid native tags, fields and values are refused", !motion_native_ok(MO_NATIVE, 0) &&
        !motion_native_ok(MO_NATIVE + 10u, MO_TAG_P5 << 8 | 60) && !motion_native_ok(MO_NATIVE + 38u, MO_TAG_CZ << 8 | 1) &&
        !motion_native_ok(MO_NATIVE + FP_TRNSP, MO_TAG_FM6 << 8) &&
        !motion_native_ok(MO_NATIVE + FP_DET, MO_TAG_FM6 << 8 | 99));
    ui_power_on(); set_engine_of(TSEL, ENGI_FM6); apply_preset_to(TSEL, 0);
    for (uint32_t j = 0; j < MOTION_MAX - 1u; j++) motion_set_event(TSEL, j, P_LEVEL, 30);
    song.rec = 1; seq_start(); seq_tick(TSEL, CTL); uint8_t old[MO_NATIVE_N]; motion_native_peek(TSEL, old);
    fm6_patch[0][FP_OL] ^= 1; fm6_patch[0][FP_OL + FP_OP] ^= 1; motion_native_edited(TSEL, old);
    bad += check("full pool refuses a whole multi-byte native edit without partial events", motion.count == MOTION_MAX - 1u && motion_full);
    seq_stop();
    return bad;
}
static int overlapping_motion_ids(void)
{
    int bad=0;ui_power_on();track_t *t=TSEL;set_engine_of(t,ENGI_FM6);apply_preset_to(t,0);
    const param_desc_t *d=param_desc_of(ENGI_FM6,P_E1);
    int16_t common=(int16_t)(d->def==d->max?d->min:d->max),native=(int16_t)(MO_TAG_FM6<<8|80u);
    bad+=check("common engine and native motion sharing wire ID 112 coexist", P_E1==MO_NATIVE &&
        !motion_set_event(t,0,P_E1,common) && !motion_set_event(t,0,MO_NATIVE,native) && motion.count==2 && motion_valid(&motion));
    uint16_t ids[MO_MAX];bad+=check("colliding wire IDs produce separate UI lanes",mo_lanes(ids)==2 && ids[0]==P_E1 && ids[1]==(MO_NATIVE|256u));
    motion_step(t,0,&motion);bad+=check("both colliding motion destinations play independently",t->p[P_E1]==common && fm6_patch[0][0]==80);
    motion_delete_event(t,0,P_E1);bad+=check("typed common deletion keeps native motion",motion.count==1 && motion.event[0].value==native);
    motion_set_event(t,0,P_E1,common);motion_delete_event(t,0,MO_NATIVE|256u);
    bad+=check("typed native deletion keeps common motion",motion.count==1 && motion.event[0].value==common);
    motion_set_event(t,0,MO_NATIVE,native);motion_delete_event(t,0,MO_NATIVE|512u);
    bad+=check("legacy untyped deletion clears both kinds",!motion.count);return bad;
}
int main(void)
{
    int bad = native_motion_test() + overlapping_motion_ids() + motion_recording() + motion_capacity() + probability_playback() + compact_project() + older_count() + loads_and_song() + repeat_mode();
    printf("%s\n", bad ? "MOTION TEST FAILED" : "motion/chance/compact storage tests passed"); return bad != 0;
}
