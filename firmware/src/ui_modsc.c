/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* Mod shortcut (Theo 2026-10-17): FX held, a knob turned sets how far a cord moves that knob's parameter, as the
 * attenuverter beside a Mutable module's knob. The cord runs from the track's shortcut source (its LFO until SELECT
 * picks another while FX is held) to the parameter on the same track. The first turn makes it in a free slot of MODS,
 * the next ones change its amount (-100..100 %); back at 0 the slot is free again. A cord already in MODS that reaches
 * the parameter on this track is the one edited, whatever its source. A parameter no cord reaches (cord.c CORD_DESTS)
 * shows "no mod" and stays as it is. While FX is held after a turn, the four knobs' amounts show over the page;
 * letting FX go closes them and opens no page (as help: ui_input.c). Included by ui_pages.c, after ui_help.c
 * (help_find: the parameter a knob reaches). */
#define MSC_BTN B_FX

static struct {
    uint8_t on;                   /* the map shown (a knob or SELECT turned with FX held) */
    uint8_t knob;                 /* the last knob turned */
    int8_t slot;                  /* the cord it edited, -1: none */
    uint8_t src[NTRK];            /* each track's shortcut source (0: CS_LFO) */
} msc = {0, 0, -1, {0}};

static int msc_held(void)
{
    return (fm1_in.buttons & (1u << panel.btn[MSC_BTN])) && !ui.menu && !ui.confirm && !ui.layer && !help.on && !designer_on();
}
static uint32_t msc_src(uint32_t tr) { return msc.src[tr % NTRK] ? msc.src[tr % NTRK] : (uint32_t)CS_LFO; }

/* the cord destination of knob k's parameter on the page shown; CD_OFF: none. help_find leaves help.pid set */
static uint32_t msc_dest(uint32_t k)
{
    uint32_t code;
    help_find(k);
    if (help.pid == PID_NONE)
        return CD_OFF;
    for (code = CD_OFF + 1u; code < CD_N; code++) {
        const cord_dest_t *d = cord_dest(code);
        if (d && (d->kind == CK_BLOCK || d->kind == CK_STEP) && d->pid == help.pid)
            return code;
    }
    return CD_OFF;
}
static int msc_used(const cord_t *c) { return c->dst != CD_OFF && (c->amount || c->offset); }
/* the cord reaching dst on track tr; -1: none */
static int32_t msc_find(uint32_t dst, uint32_t tr)
{
    uint32_t i;
    for (i = 0; i < CORD_LIVE; i++) {
        const cord_t *c = cord_get(i);
        if (msc_used(c) && c->dst == dst && c->dst_trk == tr)
            return (int32_t)i;
    }
    return -1;
}
static int32_t msc_free(void)
{
    uint32_t i;
    for (i = 0; i < CORD_LIVE; i++)
        if (!msc_used(cord_get(i)))
            return (int32_t)i;
    return -1;
}

/* ui_input.c: KNOB k turned s with FX held; 1 = taken (the parameter's own value left as it is) */
static int msc_knob(uint32_t k, int32_t s)
{
    uint32_t tr = song.sel % NTRK, dst;
    int32_t i;
    cord_t c;
    if (!msc_held())
        return 0;
    msc.on = 1;
    msc.knob = (uint8_t)(k & 3u);
    msc.slot = -1;
    ui.force = 1;
    if ((dst = msc_dest(k)) == CD_OFF)
        return 1;
    if ((i = msc_find(dst, tr)) >= 0) {
        c = *cord_get((uint32_t)i);
    } else if ((i = msc_free()) >= 0) {
        c.src = (uint8_t)msc_src(tr);
        c.src_trk = (uint8_t)tr;
        c.dst = (uint8_t)dst;
        c.dst_trk = (uint8_t)tr;
        c.amount = c.offset = 0;
    } else {
        ui_message("MODS FULL");
        return 1;
    }
    c.amount = (int8_t)clamp((int32_t)c.amount + s * 2, -100, 100);
    if (!c.amount && !c.offset)
        c.dst = CD_OFF;                                 /* (back to 0: the slot free) */
    cord_set((uint32_t)i, &c);
    if (c.dst != CD_OFF)
        msc.slot = (int8_t)i;
    return 1;
}

/* ui_input.c: SELECT turned s with FX held: the next source, for the cord last edited and the track's next ones */
static int msc_select(int32_t s)
{
    uint32_t tr = song.sel % NTRK, n = CS_N - 1u, v = msc_src(tr);
    if (!msc_held())
        return 0;
    if (msc.slot >= 0 && msc_used(cord_get((uint32_t)msc.slot)))
        v = cord_get((uint32_t)msc.slot)->src;
    v = (v - 1u + (s > 0 ? 1u : n - 1u)) % n + 1u;      /* CS_LFO .. CS_DRIFT round, never CS_OFF */
    msc.src[tr] = (uint8_t)v;
    if (msc.slot >= 0 && msc_used(cord_get((uint32_t)msc.slot))) {
        cord_t c = *cord_get((uint32_t)msc.slot);
        c.src = (uint8_t)v;
        cord_set((uint32_t)msc.slot, &c);
    }
    msc.on = 1;
    ui.force = 1;
    return 1;
}

/* ui_input.c, every frame: FX let go closes the map (and its release opens no page) */
static void msc_frame(void)
{
    if (!msc.on)
        return;
    ui.pg_down &= (uint16_t)~(1u << panel.btn[MSC_BTN]);
    if (!(fm1_in.buttons & (1u << panel.btn[MSC_BTN]))) {
        msc.on = 0;
        msc.slot = -1;
        ui.force = 1;
    }
}

/* the map: a card over the dimmed page, 220 x 124 at (10, 40): a row per knob, its parameter, the cord's source,
 * a bar from the middle and the amount; the knob last turned in the track's colour */
static void msc_draw(void)
{
    uint32_t tr = song.sel % NTRK, k;
    char head[24], v[12];
    if (!ui.force)
        return;
    cv_begin(220, 124, T_BG);
    cv_rrect(0, 0, 220, 124, 8, T_THEME, T_BG);
    cv_rrect(1, 1, 218, 122, 7, T_SURF, T_THEME);
    cv_text_on(12, 8, &AF_S, "MOD AMOUNT", T_THEME, T_SURF);
    str_cpy(head, "NEW: ", sizeof head);
    str_cpy(head + str_len(head), cord_src_name(msc_src(tr)), sizeof head - 5u);
    cv_text_r(208, 10, &AF_X, head, T_MID, T_SURF);
    for (k = 0; k < 4u; k++) {
        int32_t y = 30 + (int32_t)k * 18, i, w;
        uint32_t dst = msc_dest(k);
        const pid_entry_t *e = help.pid != PID_NONE ? pid_find(help.pid) : 0;
        int16_t *vp;
        const param_desc_t *d = page_desc(ui.home ? stage_page() : cur_page(), k, &vp);
        const char *name = d && d->label && d->label[0] ? d->label : e ? e->name : "--";   /* (the card's own label) */
        uint16_t fg = dst == CD_OFF ? T_DIM : k == msc.knob ? T_THEME : T_TEXT;
        cv_free_text(12, y, &AF_X, name, fg, T_SURF, 92);
        if (dst == CD_OFF) {
            cv_text_r(208, y, &AF_X, "no mod", T_DIM, T_SURF);
            continue;
        }
        cv_rect(150, y + 6, 40, 1, T_LINE);
        cv_rect(170, y + 3, 1, 7, T_LINE);
        if ((i = msc_find(dst, tr)) < 0) {
            cv_text_r(208, y, &AF_X, "0", T_DIM, T_SURF);
            continue;
        }
        cv_text_on(108, y, &AF_X, cord_src_name(cord_get((uint32_t)i)->src), T_MID, T_SURF);
        w = cord_get((uint32_t)i)->amount * 20 / 100;
        if (w > 0) cv_rect(171, y + 4, w, 5, k == msc.knob ? T_THEME : T_TEXT);
        else if (w < 0) cv_rect(170 + w, y + 4, -w, 5, k == msc.knob ? T_THEME : T_TEXT);
        v[0] = '+';
        fmt_int(cord_get((uint32_t)i)->amount < 0 ? v : v + 1, cord_get((uint32_t)i)->amount);
        cv_text_r(208, y, &AF_X, v, T_TEXT, T_SURF);
    }
    cv_rect(12, 102, 196, 1, T_LINE);
    cv_free_text(12, 106, &AF_X, "FX held: knob amount, SELECT source", T_DIM, T_SURF, 196);
    cv_blit(10, 40);
}
