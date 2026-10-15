/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* CORDS (LFO family, after MOD; docs/design/phase2/cords.png): the cords (cord.c) four to a screen, as MOD's rows:
 * "2  [2] ENV -> [4] DECAY  -25 %" over the amount's bar from the middle; the cord KNOB 2 is on: a card. The knobs as
 * MOD's: KNOB 1 the source (--, then T1 LFO .. T4 NOTE), 2 the cord, 3 the destination (--, then T1 PITCH ..), 4 the
 * amount. A cord without a source is a fixed move: KNOB 4 its OFFSET. Included by ui_pages.c. */

static int32_t accel(uint32_t role, int32_t s, int32_t range);                       /* ui_input.c */
static int32_t list_accel(uint32_t role, int32_t s, uint32_t total, uint32_t *fast);
static uint8_t cord_ui_sel;                             /* the cord KNOB 2 is on: 0 .. CORD_LIVE - 1 */

#define CU_NSRC (1u + NTRK * (CS_N - 1u))               /* KNOB 1's list: --, then track by track */
#define CU_NDST (1u + NTRK * (CD_N - 1u))
static uint32_t cu_src_of(const cord_t *c) { return c->src ? 1u + c->src_trk * (CS_N - 1u) + c->src - 1u : 0u; }
static uint32_t cu_dst_of(const cord_t *c) { return c->dst ? 1u + c->dst_trk * (CD_N - 1u) + c->dst - 1u : 0u; }

/* a destination's name: E1..E8 by the destination track's engine (as MOD names them) */
static const char *cu_dst_name(const cord_t *c)
{
    if (c->dst >= CD_E1 && c->dst <= CD_E8)
        return mod_dst_name(&trk[c->dst_trk % NTRK], MD_E1 + (c->dst - CD_E1));
    return cord_dest(c->dst) ? cord_dest(c->dst)->name : "--";
}

/* the move's percentage as "+40 %" (an amount, or a fixed cord's offset) */
static int32_t cu_pct(const cord_t *c) { return c->src ? c->amount : c->offset; }
static void cu_pct_text(char *b, int32_t v)
{
    char n[8];
    fmt_int(n, v < 0 ? -v : v);
    b[0] = v < 0 ? '-' : v > 0 ? '+' : ' ';
    str_cpy(b + 1, n, 8);
    str_cpy(b + str_len(b), " %", 3);
}

/* a chip: the track's number on its colour, then the name */
static void cu_chip(int32_t x, int32_t y, int32_t w, uint32_t tr, const char *s, int on, int has_trk, uint16_t bg)
{
    uint16_t fill = on ? ux_mix(bg, T_THEME, 30) : ux_mix(bg, T_LINE, 50);
    char num[2] = {(char)('1' + (tr & 3u)), 0};
    cv_rrect(x, y, w, 18, 5, fill, bg);
    if (has_trk) {
        cv_rrect(x + 3, y + 3, 12, 12, 3, on ? T_TRK(tr) : T_DIM, fill);
        cv_text_on(x + 6, y + 4, &AF_X, num, T_BG, on ? T_TRK(tr) : T_DIM);
        x += 14;
        w -= 14;
    }
    cv_free_text(x + 4 + (w - 8 - (text_w(&AF_X, s) < w - 8 ? text_w(&AF_X, s) : w - 8)) / 2, y + 4, &AF_X, s,
                 on ? T_TEXT : T_DIM, fill, w - 8);
}

#define CU_Y 22                                         /* the rows: 48 px each from y 22 (as MOD's), the chips at 220 */
#define CU_H 48
static void cu_rows(void)
{
    static uint32_t sig;
    uint32_t k, top = cord_ui_sel / 4u * 4u, sg = cord_ui_sel * 7u + ux.gen * 977u + ux.pal * 31u;
    for (k = 0; k < CORD_LIVE; k++) {
        const uint8_t *b = (const uint8_t *)cord_get(k);
        uint32_t i;
        for (i = 0; i < sizeof(cord_t); i++)
            sg = (sg ^ b[i]) * 16777619u;
    }
    for (k = 0; k < NTRK; k++)
        sg = (sg ^ trk[k].engine) * 16777619u;          /* (E1..E8's names) */
    if (!ui.force && sg == sig)
        return;
    sig = sg;
    for (k = 0; k < 4u; k++) {
        const cord_t *c = cord_get(top + k);
        int sel = top + k == cord_ui_sel, on = c->dst != CD_OFF && (c->amount || c->offset);
        uint16_t bg = sel ? T_LIFT : T_BG;
        int32_t a = cu_pct(c), m = 128, x;
        char b[16];
        cv_begin(240, CU_H, T_BG);
        if (sel) {
            cv_rrect(4, 1, 232, CU_H - 4, 7, T_THEME, T_BG);
            cv_rrect(5, 2, 230, CU_H - 6, 6, bg, T_THEME);
        }
        fmt_int(b, (int32_t)(top + k + 1u));
        cv_text_on(top + k + 1u >= 10u ? 8 : 12, 7, &AF_S, b, sel ? T_THEME : T_MID, bg);
        cu_chip(26, 6, 74, c->src_trk, c->src ? cord_src_name(c->src) : "--", c->src != CS_OFF, c->src != CS_OFF, bg);
        cv_rect(104, 14, 10, 2, on ? T_MID : T_DIM);    /* an arrow */
        cv_line(110, 11, 113, 14, on ? T_MID : T_DIM);
        cv_line(110, 17, 113, 14, on ? T_MID : T_DIM);
        cu_chip(118, 6, 74, c->dst_trk, cu_dst_name(c), c->dst != CD_OFF, c->dst != CD_OFF, bg);
        cu_pct_text(b, a);
        cv_text_r(232, 9, &AF_X, b, on ? (sel ? T_THEME : T_SEC) : T_DIM, bg);
        cv_rect(26, 30, 206, 3, T_LINE);                /* the move, from the middle */
        x = 26 + (a + 100) * 206 / 200;
        if (a)
            cv_rect(x < m ? x : m, 30, x < m ? m - x : x - m, 3, on ? (sel ? T_THEME : ux_mix(bg, T_THEME, 60)) : T_DIM);
        cv_rect(m, 28, 1, 7, T_MID);
        cv_blit(0, (uint32_t)(CU_Y + (int32_t)k * CU_H));
    }
}

/* the chips under the rows (ui_draw.c draw_columns): KNOB 1 the source, 2 the cord, 3 the destination, 4 the move */
static void cu_columns(void)
{
    const cord_t *c = cord_get(cord_ui_sel);
    char val[24], src[16];
    if (c->src) {
        src[0] = 'T'; src[1] = (char)('1' + c->src_trk); src[2] = ' ';
        str_cpy(src + 3, cord_src_name(c->src), sizeof src - 3);
    } else
        str_cpy(src, "--", sizeof src);
    draw_column(0, "", src, "", c->src ? VAL(0u) : T_DIM, -1, ICON_NONE);
    fmt_int(val, (int32_t)cord_ui_sel + 1);
    draw_column(1, "CORD", val, "/16", VAL(1u), (int32_t)cord_ui_sel * 1000 / (CORD_LIVE - 1u), ICON_NONE);
    if (c->dst) {
        val[0] = 'T'; val[1] = (char)('1' + c->dst_trk); val[2] = ' ';
        str_cpy(val + 3, cu_dst_name(c), sizeof val - 3);
    } else
        str_cpy(val, "--", sizeof val);
    draw_column(2, "", val, "", c->dst ? VAL(2u) : T_DIM, -1, ICON_NONE);
    cu_pct_text(val, cu_pct(c));
    draw_column(3, c->src ? "" : "OFS", val, "", cu_pct(c) ? VAL(3u) : T_DIM, (cu_pct(c) + 100) * 1000 / 200, ICON_NONE);
}

/* a knob on CORDS (ui_input.c edit_param) */
static void cu_knob(uint32_t slot, int32_t steps)
{
    cord_t c = *cord_get(cord_ui_sel);
    int32_t v;
    if (slot == 1u) {
        cord_ui_sel = (uint8_t)clamp((int32_t)cord_ui_sel + (steps > 0 ? 1 : -1), 0, CORD_LIVE - 1);
        ui.force = 1;
        return;
    }
    if (slot == 0u) {
        v = clamp((int32_t)cu_src_of(&c) + list_accel(EN_K1, steps, CU_NSRC, 0), 0, CU_NSRC - 1);
        c.src = v ? (uint8_t)((v - 1) % (CS_N - 1u) + 1u) : CS_OFF;
        c.src_trk = v ? (uint8_t)((v - 1) / (CS_N - 1u)) : 0u;
        if (c.src && !c.amount) { c.amount = c.offset; c.offset = 0; }   /* (one move shown: a fixed move becomes */
        if (!c.src && c.amount) { c.offset = c.amount; c.amount = 0; }   /* the source's, and back) */
    } else if (slot == 2u) {
        int was = c.dst != CD_OFF;
        v = clamp((int32_t)cu_dst_of(&c) + list_accel(EN_K1 + 2u, steps, CU_NDST, 0), 0, CU_NDST - 1);
        c.dst = v ? (uint8_t)((v - 1) % (CD_N - 1u) + 1u) : CD_OFF;
        c.dst_trk = v ? (uint8_t)((v - 1) / (CD_N - 1u)) : 0u;
        if (!was && c.dst && !c.amount && !c.offset) {  /* (connected: a move to hear at once) */
            if (c.src) c.amount = 50;
            else c.offset = 25;
        }
    } else {
        v = clamp(cu_pct(&c) + accel(EN_K1 + 3u, steps, 200), -100, 100);
        if (c.src) c.amount = (int8_t)v;
        else c.offset = (int8_t)v;
    }
    (void)cord_set(cord_ui_sel, &c);
    ui.force = 1;
}
