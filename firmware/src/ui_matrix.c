/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* MATRIX (LFO family, after MODS): the mods (cord.c) at a glance, source track by destination track. A cell is lit when
 * a mod goes from its row's track to its column's, as bright as they move it now (cord.c cord_share), and moves with
 * them. Nothing of its own to save: a view of the mods. KNOB 1 the row (FROM), 2 the column (TO), 3 which of the
 * cell's mods (several: one by one), OCT+ opens that mod in MODS. Included by ui_pages.c. */

static uint8_t mx_from, mx_to, mx_k;                    /* the cell picked, and which of its mods */

/* the cell's mods (doing something: cord_live), in MODS' order; n of them, i: the k-th's index (none: CORD_LIVE) */
static uint32_t mx_cell(uint32_t from, uint32_t to, uint32_t k, uint32_t *i)
{
    uint32_t j, n = 0;
    *i = CORD_LIVE;
    for (j = 0; j < CORD_LIVE; j++) {
        const cord_t *c = cord_get(j);
        if (!cord_live(c) || !c->src || c->src_trk != from || c->dst_trk != to) continue;
        if (n++ == k) *i = j;
    }
    return n;
}
/* how far the cell's mods move their destinations now, 0..8 (0: none linked; 1: linked, still) */
static uint32_t mx_level(uint32_t from, uint32_t to)
{
    uint32_t j, n = 0;
    int32_t a = 0;
    for (j = 0; j < CORD_LIVE; j++) {
        const cord_t *c = cord_get(j);
        int32_t s;
        if (!cord_live(c) || !c->src || c->src_trk != from || c->dst_trk != to) continue;
        s = cord_share(c);
        a += s < 0 ? -s : s;
        n = 1;
    }
    return n ? 1u + (uint32_t)clamp(a * 7 / 32768, 0, 7) : 0u;
}

/* the panel (ui_graph.c, on its 100 px scale): the tracks' numbers along the top (TO) and down the left (FROM), the
 * cells between; the picked one outlined */
#define MX_X 46                                         /* the first column's x; cells 40 x 18, 44 and 21 apart */
#define MX_Y 16
static void mx_graph(uint16_t c)
{
    uint32_t r, k;
    cv_text(PANEL_X0, 2, &AF_X, "FROM", T_MID);
    cv_text_r(PANEL_X0 + 214, 2, &AF_X, "TO", T_MID, T_SURF);
    for (k = 0; k < NTRK; k++) {
        char n[2] = {(char)('1' + k), 0};
        cv_text_c(MX_X + 20 + (int32_t)k * 44, 2, &AF_X, n, T_TRK(k), T_SURF);
        cv_text_c(PANEL_X0 + 22, MX_Y + 4 + (int32_t)k * 21, &AF_X, n, T_TRK(k), T_SURF);
    }
    for (r = 0; r < NTRK; r++)
        for (k = 0; k < NTRK; k++) {
            int32_t x = MX_X + (int32_t)k * 44, y = MX_Y + (int32_t)r * 21;
            uint32_t lv = mx_level(r, k), i, n = mx_cell(r, k, 0, &i);
            uint16_t f = lv ? ux_mix(T_RAISE, T_TRK(r), 30 + (int32_t)(lv - 1u) * 10) : T_SURF;
            if (r == mx_from && k == mx_to) {
                cv_rrect(x - 2, y - 2, 44, 22, 4, c, T_SURF);
                cv_rrect(x, y, 40, 18, 3, f, c);
            } else
                cv_rrect(x, y, 40, 18, 3, lv ? f : T_RAISE, T_SURF);
            if (!lv)
                cv_rrect(x + 1, y + 1, 38, 16, 2, T_SURF, T_RAISE);   /* (no mod: an outline) */
            if (n > 1u) {                               /* several: how many */
                char m[3];
                fmt_int(m, (int32_t)n);
                cv_text_c(x + 20, y + 4, &AF_X, m, T_TEXT, f);
            }
        }
}
/* what the panel shows: the cells' levels and the pick (graph_sig: it redraws as they move) */
static uint32_t mx_sig(void)
{
    uint32_t r, k, h = mx_from * 977u + mx_to * 31u + mx_k;
    for (r = 0; r < NTRK; r++)
        for (k = 0; k < NTRK; k++) {
            uint32_t i;
            h = h * 31u + mx_level(r, k) * 17u + mx_cell(r, k, 0, &i);
        }
    return h;
}

/* the cards: FROM, TO, which mod (its number in MODS, and k of n), OPEN */
static void mx_columns(void)
{
    char v[8], u[8];
    uint32_t i, n = mx_cell(mx_from, mx_to, mx_k, &i);
    v[0] = 'T'; v[1] = (char)('1' + mx_from); v[2] = 0;
    draw_column(0, "FROM", v, "", VAL(0u), (int32_t)mx_from * 1000 / (NTRK - 1u), ICON_NONE);
    v[1] = (char)('1' + mx_to);
    draw_column(1, "TO", v, "", VAL(1u), (int32_t)mx_to * 1000 / (NTRK - 1u), ICON_NONE);
    if (n) {
        fmt_int(v, (int32_t)i + 1);
        fmt_int(u, (int32_t)mx_k + 1);
        str_cpy(u + str_len(u), "/", 2);
        fmt_int(u + str_len(u), (int32_t)n);
        draw_column(2, "MOD", v, n > 1u ? u : "", VAL(2u), -1, ICON_NONE);
    } else
        draw_column(2, "MOD", "--", "", T_DIM, -1, ICON_NONE);
    draw_column(3, "OPEN", n ? "[OCT+]" : "--", "", n ? T_ACCENT : T_DIM, -1, ICON_NONE);
}

static void mx_knob(uint32_t slot, int32_t steps)
{
    uint32_t i, n;
    int32_t d = steps > 0 ? 1 : -1;
    if (slot == 0u) mx_from = (uint8_t)clamp((int32_t)mx_from + d, 0, NTRK - 1), mx_k = 0;
    else if (slot == 1u) mx_to = (uint8_t)clamp((int32_t)mx_to + d, 0, NTRK - 1), mx_k = 0;
    else if (slot == 2u && (n = mx_cell(mx_from, mx_to, 0, &i)) > 1u)
        mx_k = (uint8_t)(((int32_t)mx_k + d + (int32_t)n) % (int32_t)n);   /* (round the cell's mods) */
    ui.force = 1;
}

/* OCT+: the picked mod in MODS (none: nothing) */
static void mx_open(void)
{
    uint32_t i, p;
    if (!mx_cell(mx_from, mx_to, mx_k, &i))
        return;
    for (p = 0; p < NPAGES && PAGES[p].graph != GR_CORDS; p++)
        ;
    if (p == NPAGES)
        return;
    cord_ui_sel = (uint8_t)i;
    ui.page = (uint8_t)p;
    ui.fam_last[fam_grp(PAGES[p].fam)] = ui.page;
    page_entered();
    ui.force = 1;
}
