/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Melodee UI: the panel (Y_GRAPH .. Y_GRAPH + H_GRAPH) between the cards and the footer: a SURF area
 * (rounded, on BG) holding the page graphs (ADSR, LFO, steps, piano roll, drum grid, scale,
 * MOD, lists: presets, user presets, patterns, project slots, song), the HOME oscilloscope, the
 * instrument diagrams; the MIXER page draws four SURF columns instead. Each graph is redrawn only when
 * graph_signature() changes. The look:
 * curves THEME (2 px), guides and empty marks RAISE, captions MID / DIM, the active thing ACCENT,
 * bars rounded; a list's selected row is a THEME bar with INK text. */
#define PANEL_X0 10                                  /* the graphs' inner area: x 10..230 */
#define PANEL_W 220
#define GOY 11                                       /* graphs drawn on a 100 px scale sit at y 11..111 */

/* Matches voice.c: attack is linear, decay and release are exponential
 * (env += (target - env) * k each tick, ~99 % after the set time). Time
 * axis is the parameter value (the times themselves are exponential). */
/* an ADSR of four values 0..127 between rows top and bot (Stage: the amp envelope while one of its knobs turns) */
static void graph_adsr_v(int32_t va, int32_t vd, int32_t vs, int32_t vr, int32_t top, int32_t bot, uint16_t c)
{
    int32_t a = 4 + va * 50 / 127, d = 6 + vd * 50 / 127, r = 6 + vr * 60 / 127, sus = vs * 1000 / 127;   /* 0..1000 */
    int32_t x0 = 12, x1 = x0 + a, x3 = 226 - r, i, px, py;
    int32_t e = 32768;                                                  /* exp(-4.6 u), Q15 */
#define EGY(lvl) (bot - (lvl) * (bot - top) / 1000)
    cv_rect(PANEL_X0, bot + 2, PANEL_W, 1, T_RAISE);
    cv_line_t(x0, bot, x1, top, c, 2);                                  /* attack: linear */
    px = x1;
    py = top;
    for (i = 1; i <= d; i++) {                                          /* decay: exponential to SUS */
        int32_t lvl;
        e = (e * (32768 - 150733 / d)) >> 15;           /* k^d = exp(-4.6) */
        lvl = sus + ((1000 - sus) * e >> 15);
        cv_line_t(px, py, x1 + i, EGY(lvl), c, 2);
        px = x1 + i;
        py = EGY(lvl);
    }
    cv_line_t(px, py, x3, EGY(sus), c, 2);                               /* sustain */
    px = x3;
    py = EGY(sus);
    e = 32768;
    for (i = 1; i <= r; i++) {                                          /* release: exponential to 0 */
        e = (e * (32768 - 150733 / r)) >> 15;
        cv_line_t(px, py, x3 + i, EGY(sus * e >> 15), c, 2);
        px = x3 + i;
        py = EGY(sus * e >> 15);
    }
#undef EGY
}

/* FM6: a DX7 envelope, four rates (a segment's width: slower is wider) and four levels, held at L3 while the key is
 * down, then R4 to L4; it starts at L4 (as the DX7's). r, l: 0..99 each. pitch: a pitch envelope (50 = the note's
 * own pitch, a line there). The column just turned in ACCENT */
static void graph_dxenv(const uint8_t *r, const uint8_t *l, int pitch, uint16_t c)
{
    const page_t *pg = cur_page();
    int32_t x = PANEL_X0, i, top = 6, bot = 88, y0, y1, hot = ui.hot_t ? (int32_t)(ui.hot_col & 3u) : -1;
#define FMY(v) (bot - (v) * (bot - top) / 99)
    cv_rect(PANEL_X0, bot + 2, PANEL_W, 1, T_RAISE);
    if (pitch)
        cv_rect(PANEL_X0, FMY(50), PANEL_W, 1, T_RAISE);   /* the note's own pitch */
    y0 = FMY(l[3]);
    for (i = 0; i < 4; i++) {
        int32_t w = 8 + (99 - r[i]) * 38 / 99, xe;
        uint16_t sc = i == hot ? T_ACCENT : c;
        if (i == 3) {                                    /* held at L3, then the release */
            cv_line_t(x, y0, x + 30, y0, c, 2);
            x += 30;
        }
        xe = x + w;
        y1 = FMY(l[i]);
        cv_line_t(x, y0, xe, y1, (pg->id[0] == FP_L1 || pg->id[0] == FP_PL1) && i == hot ? T_ACCENT : sc, 2);
        x = xe;
        y0 = y1;
    }
    cv_line_t(x, y0, PANEL_X0 + PANEL_W, y0, T_DIM, 2);
#undef FMY
}

static void graph_lfo(const track_t *t, uint16_t c)
{
    int32_t x, uni = t->p[P_LPOL] != 0, z = uni ? 88 : 50, py = z;   /* the zero line: POL UNI (LFO 2) draws 0..+1 over it */
    uint32_t ph = (uint32_t)t->p[P_LPHASE] << 25;
    cv_rect(PANEL_X0, z, PANEL_W, 1, T_RAISE);
    for (x = 0; x < PANEL_W; x++) {                  /* two cycles (lfo_wave only reads the track) */
        int32_t w = lfo_wave((track_t *)t, ph + (uint32_t)x * (0xFFFFFFFFu / (PANEL_W / 2u))), y;
        if (t->p[P_LWAVE] == 4)
            w = (int32_t)((x / 20 * 2654435761u) >> 16) - 32768;
        y = uni ? z - (w + 32768) * 38 / 32768 : 50 - w * 38 / 32768;
        if (x)
            cv_line_t(PANEL_X0 + x - 1, py, PANEL_X0 + x, y, c, 2);
        py = y;
    }
}

/* step bar x of step i of a 16-step row: 4 groups, as the footer (9 px bars, 13 px apart, 4 px between groups) */
static int32_t bar_x(uint32_t i) { return 13 + (int32_t)(i % 16u) * 13 + (int32_t)(i % 16u / 4u) * 4; }

/* PATTERN page: the 64 steps as 4 rows of 16 bars (an accent: the accent colour, a tie: a lower bar,
 * empty: a stub), the cursor and the playhead under them */

#include "tessera.h"

void graph_tessera_viz(uint32_t tr, int is_panel) {
    if (tr >= TESSERA_TRACKS) return;
    uint32_t mode = tessera.mode[tr];
    uint16_t c = T_THEME;
    uint16_t bg = is_panel ? T_PANEL : T_SURF;
    uint16_t dim = is_panel ? T_LINE : T_RAISE;
    int32_t y_base = is_panel ? 38 : 14;

    if (mode == TESS_ENG_TURING) {
        const gen_turing_t *tur = tessera_turing(tr);
        uint32_t len = tur->len ? tur->len : 16u;
        int step_w = is_panel ? 10 : 11;
        int h = is_panel ? 14 : 18;

        for (uint32_t i = 0; i < 16u; i++) {
            int32_t sx = is_panel ? (18 + i * 12 + (i / 4u) * 4) : (13 + i * 13 + (i / 4u) * 4);

            if (i < len) {
                /* Calculate relative pitch height based on historical bit shift */
                uint8_t raw_history = ((tur->reg & (len >= 16u ? 0xFFFFu : ((1u << len) - 1u))) >> (len - 1u - i)) & 0xFFu;
                uint8_t span = tur->oct_span ? tur->oct_span : 1;
                uint32_t step_val = ((raw_history / 7) % span) * 7 + (raw_history % 7);
                uint32_t max_val = (span * 7) - 1;
                int bar_h = 4 + (max_val == 0 ? 0 : (step_val * (h - 4)) / max_val);
                int sy = y_base + (h - bar_h);

                uint32_t bit = (tur->reg >> (len - 1u - i)) & 1u;
                int is_head = (song.playing && i == len - 1u);

                if (bit) {
                    cv_rrect(sx, sy, step_w, bar_h, 1, is_head ? c : T_TEXT, bg);
                } else {
                    cv_rrect(sx, y_base + h - 4, step_w, 4, 1, is_head ? c : dim, bg);
                }

                cv_rect(sx, y_base + h + 2, step_w, 2, c);
            } else {
                cv_rect(sx + (step_w / 2) - 1, y_base + h - 3, 2, 2, dim);
                cv_rect(sx, y_base + h + 2, step_w, 1, dim);
            }
        }
    } else if (mode == TESS_ENG_EUCLID) {
        const gen_euclid_t *euc = tessera_euclid(tr);
        uint32_t len = euc->steps ? euc->steps : 16u;
        uint32_t max_vis = len > 16 ? 32 : 16;
        int h = is_panel ? 14 : 18;

        for (uint32_t i = 0; i < max_vis; i++) {
            int step_w = is_panel ? 10 : 11;
            int32_t sx = 0;

            if (max_vis <= 16) {
                sx = is_panel ? (18 + i * 12 + (i / 4u) * 4) : (13 + i * 13 + (i / 4u) * 4);
            } else {
                step_w = is_panel ? 4 : 5;
                sx = is_panel ? (16 + i * 6) : (13 + i * 6);
            }

            if (i < len) {
                uint32_t active = (uint32_t)gen_euclid_hit((uint8_t)i, (uint8_t)len, euc->pulses, euc->rot);
                int is_head = (song.playing && i == (euc->step_idx + len - 1u) % len);

                if (active) {
                    uint32_t epit = tessera_euclid_pitch(&trk[tr], i);
                    int ebar = 4 + (int)((epit * (uint32_t)(h - 4)) / 14u);
                    cv_rrect(sx, y_base + h - ebar, step_w, ebar, 1, is_head ? c : T_TEXT, bg);
                } else {
                    cv_rrect(sx, y_base + h - 4, step_w, 4, 1, is_head ? c : dim, bg);
                }

                cv_rect(sx, y_base + h + 2, step_w, 2, c);
            } else {
                cv_rect(sx + (step_w / 2) - 1, y_base + h - 3, 2, 2, dim);
                cv_rect(sx, y_base + h + 2, step_w, 1, dim);
            }
        }
    } else if (mode == TESS_ENG_MARKOV) {
        const gen_markov_t *mk = tessera_markov(tr);
        int h = is_panel ? 14 : 18;
        int step_w = is_panel ? 16 : 18;
        int col = is_panel ? 25 : 27;
        int32_t x0 = is_panel ? 18 : 14;
        uint32_t cur = mk->cur_state & 7u;
        uint8_t mrow[8];
        gen_markov_row(mk->tension, cur, mrow);

        for (uint32_t i = 0; i < 8u; i++) {
            int32_t sx = x0 + (int32_t)i * col;
            /* matrix[][] is cumulative: the chance of i is the step up from i-1 */
            uint32_t cum = mrow[i];
            uint32_t prev = i ? mrow[i - 1u] : 0u;
            uint32_t pr = cum > prev ? cum - prev : 0u;
            if (pr > 40u) pr = 40u;
            int bar_h = 3 + (int)(pr * (uint32_t)(h - 3) / 40u);
            int is_cur = (i == cur);

            int rest = is_cur && !mk->last_hit;          /* playhead on a rest: hollow bar, thin dim underline */
            if (rest) {
                cv_rrect(sx, y_base + h - bar_h, step_w, bar_h, 1, c, bg);
                cv_rect(sx + 1, y_base + h - bar_h + 1, step_w - 2, bar_h - 2, bg);
            } else {
                cv_rrect(sx, y_base + h - bar_h, step_w, bar_h, 1, is_cur ? c : T_TEXT, bg);
            }
            cv_rect(sx, y_base + h + 2, step_w, is_cur && !rest ? 2 : 1, is_cur && !rest ? c : dim);
        }
    } else if (mode == TESS_ENG_MORPH) {
        const gen_morph_t *mo = tessera_morph(tr);
        int h = is_panel ? 14 : 18;
        int step_w = is_panel ? 10 : 11;
        int lane = (h - 2) / 2;
        uint32_t cur = mo->step_idx & 0x0Fu;
        int32_t x0 = is_panel ? 18 : 13;
        int32_t sw = is_panel ? 202 : 218;

        for (uint32_t i = 0; i < 16u; i++) {
            int32_t sx = is_panel ? (18 + (int32_t)i * 12 + (int32_t)(i / 4u) * 4)
                                  : (13 + (int32_t)i * 13 + (int32_t)(i / 4u) * 4);
            int ha = (mo->snap_a >> i) & 1u;
            int hb = (mo->snap_b >> i) & 1u;
            int is_head = (song.playing && i == cur);

            /* top lane = snapshot B, bottom lane = snapshot A */
            if (hb) cv_rrect(sx, y_base, step_w, lane, 1, T_ACCENT, bg);
            else cv_rect(sx, y_base + lane - 1, step_w, 1, dim);
            if (ha) cv_rrect(sx, y_base + lane + 2, step_w, lane, 1, c, bg);
            else cv_rect(sx, y_base + h - 1, step_w, 1, dim);

            cv_rect(sx, y_base + h + 2, step_w, is_head ? 2 : 1, is_head ? T_TEXT : dim);
        }
        /* morph position: left = A, right = B */
        cv_rect(x0, y_base + h + 9, sw, 1, dim);
        cv_rect(x0 + ((int32_t)mo->morph_pos * (sw - 4)) / 127, y_base + h + 6, 4, 7, c);    } else if (mode == TESS_ENG_GRIDS) {
        /* three lanes, hat on top, snare, kick at the bottom: the bar's hits at X, Y and DENSITY (without this bar's
         * CHAOS), an accent in the accent colour; the playhead under them */
        const gen_grids_t *g = tessera_grids(tr);
        uint32_t steps = gen_grids_steps(tess_steps_per_bar(&trk[tr]));
        uint32_t cur = (g->step_idx + steps - 1u) % steps;
        int h = is_panel ? 14 : 18;
        int lane = h / 3;
        int step_w = steps > 16u ? (is_panel ? 4 : 5) : (is_panel ? 10 : 11);

        for (uint32_t i = 0; i < steps; i++) {
            int32_t sx = steps > 16u ? (is_panel ? 16 + (int32_t)i * 6 : 13 + (int32_t)i * 6)
                                     : (is_panel ? (18 + (int32_t)i * 12 + (int32_t)(i / 4u) * 4)
                                                 : (13 + (int32_t)i * 13 + (int32_t)(i / 4u) * 4));
            int is_head = song.playing && i == cur;
            for (uint32_t p = 0; p < 3u; p++) {
                int acc;
                int32_t ly = y_base + (int32_t)(2u - p) * lane;
                if (gen_grids_hit(g, i, steps, p, 0u, &acc))
                    cv_rrect(sx, ly, step_w, lane - 1, 1, is_head ? T_TEXT : acc ? T_ACCENT : c, bg);
                else
                    cv_rect(sx, ly + lane - 2, step_w, 1, dim);
            }
            cv_rect(sx, y_base + h + 2, step_w, is_head ? 2 : 1, is_head ? T_TEXT : dim);
        }
    } else if (mode == TESS_ENG_BOUNCE) {
        /* the box (its 256 x 256 the panel's; on the older page squeezed into a strip), the balls (all alike), the walls
         * hit on the last step lit */
        const gen_bounce_t *b = tessera_bounce(tr);
        int h = is_panel ? 14 : 18;
        int32_t x0 = is_panel ? 18 : 13, sw = is_panel ? 202 : 218, top = is_panel ? 8 : y_base - 2, bh = is_panel ? 96 : h + 8;
        uint32_t w = b->hit, n = b->balls >= 1u && b->balls <= GEN_BOUNCE_MAX ? b->balls : 1u;
        cv_rect(x0, top, sw, w & 8u ? 2 : 1, w & 8u ? T_TEXT : dim);                     /* top */
        cv_rect(x0, top + bh - (w & 1u ? 2 : 1), sw, w & 1u ? 2 : 1, w & 1u ? T_TEXT : dim);   /* floor */
        cv_rect(x0, top, w & 2u ? 2 : 1, bh, w & 2u ? T_TEXT : dim);                     /* left */
        cv_rect(x0 + sw - (w & 4u ? 2 : 1), top, w & 4u ? 2 : 1, bh, w & 4u ? T_TEXT : dim);   /* right */
        for (uint32_t k = n; k-- > 0;) {
            int32_t r = 5;
            int32_t bx = x0 + 2 + (int32_t)b->x[k] * (sw - 4 - r) / 255, by = top + bh - 2 - r - (int32_t)b->y[k] * (bh - 4 - r) / 255;
            cv_rrect(bx, by, r, r, 1, T_TEXT, bg);
        }
    } else if (mode == TESS_ENG_AWAKE) {
        /* loop A (the notes) over loop B (their lift), 16 cells each: A's value a bar its height (1..8; 1 two pixels at
         * least), a rest a dash; B's the lift, "+" and a number on the panel, a bar on the strip (0 a dash); past the loop's length dim; the steps playing lit; the step the keys picked framed */
        const gen_awake_t *a = tessera_awake(tr);
        int32_t ha = is_panel ? 44 : 12, hb = is_panel ? 28 : 6, ya = is_panel ? 10 : y_base, yb = ya + ha + (is_panel ? 14 : 4);
        uint32_t la = a->len_a ? a->len_a : 1u, lb = a->len_b ? a->len_b : 1u;
        uint32_t ca = (a->pos_a + la - 1u) % la, cb = (a->pos_b + lb - 1u) % lb;
        int step_w = is_panel ? 10 : 11;
        if (is_panel) cv_text_c(9, yb + (hb - 12) / 2, &AF_S, "+", T_MID, bg);   /* (row B lifts row A) */
        for (uint32_t i = 0; i < 16u; i++) {
            int32_t sx = is_panel ? (18 + (int32_t)i * 12 + (int32_t)(i / 4u) * 4) : (13 + (int32_t)i * 13 + (int32_t)(i / 4u) * 4);
            uint32_t va = gen_awake_a(a, i), vb = gen_awake_b(a, i);
            int ha_on = song.playing && i == ca, hb_on = song.playing && i == cb;
            if (i >= la) cv_rect(sx, ya + ha - 1, step_w, 1, dim);
            else if (!va) cv_rect(sx, ya + ha - 2, step_w, 2, ha_on ? T_TEXT : dim);
            else cv_rrect(sx, ya + ha - (2 + ((int32_t)va - 1) * (ha - 2) / 7), step_w, 2 + ((int32_t)va - 1) * (ha - 2) / 7, 1,
                          ha_on ? T_TEXT : c, bg);
            if (i >= lb) cv_rect(sx, yb + hb - 1, step_w, 1, dim);
            else if (is_panel) {                            /* B's lift as its number (0 dim), after the row's "+" */
                char d[2] = {(char)('0' + (vb > 7u ? 7u : vb)), 0};
                cv_text_c(sx + step_w / 2, yb + (hb - 12) / 2, &AF_S, d, hb_on ? T_TEXT : vb ? c : dim, bg);
            }
            else if (!vb) cv_rect(sx, yb + hb - 2, step_w, 2, hb_on ? T_TEXT : dim);
            else cv_rrect(sx, yb + hb - (2 + ((int32_t)vb - 1) * (hb - 2) / 6), step_w, 2 + ((int32_t)vb - 1) * (hb - 2) / 6, 1,
                          hb_on ? T_TEXT : c, bg);
            if (a->edit && (a->edit & 15u) == i) {         /* the step picked (ui_awake.c): framed, A's and B's */
                cv_rect(sx - 1, ya - 2, step_w + 2, 1, T_TEXT);
                cv_rect(sx - 1, ya + ha + 1, step_w + 2, 1, T_TEXT);
                cv_rect(sx - 1, yb - 2, step_w + 2, 1, T_TEXT);
                cv_rect(sx - 1, yb + hb + 1, step_w + 2, 1, T_TEXT);
            }
        }
    } else if (mode == TESS_ENG_POLY) {
        /* the four rhythms, a lane each: its divider, then the 16 steps of the bar's window, a tick where it lands, the
         * step playing framed, the tick lit when it fires there (OFF: a dim lane; the one RHY picks, its label lit).
         * Under them the four notes, a bar each at its pitch, the one playing lit */
        const gen_poly_t *p = tessera_poly(tr);
        uint32_t n = 7u * (p->span >= 1u && p->span <= 3u ? p->span : 1u), now = (p->tick + GEN_POLY_WRAP - 1u) % GEN_POLY_WRAP, base = now - now % 16u;
        int32_t top = is_panel ? 6 : y_base, lane = is_panel ? 13 : 5, x0 = is_panel ? 46 : 40, cw = is_panel ? 11 : 12;
        int32_t ny = top + 4 * lane + (is_panel ? 6 : 2), nh = is_panel ? 112 - ny - 6 : 8;
        for (uint32_t r = 0; r < 4u; r++) {
            uint32_t d = p->div[r];
            int32_t ly = top + (int32_t)r * lane;
            if (is_panel) {
                char lb[4] = "OFF";
                if (d) lb[0] = '/', fmt_int(lb + 1, d);
                cv_text(14, ly, &AF_X, lb, r == (p->sel & 3u) ? T_TEXT : d ? c : dim);
            }
            for (uint32_t i = 0; i < 16u; i++) {
                int32_t sx = x0 + (int32_t)i * cw + (int32_t)(i / 4u) * 2;
                uint32_t st = base + i, on = d && st % d == 0u, cur = song.playing && st == now;
                if (!d) cv_rect(sx, ly + lane - 3, cw - 2, 1, dim);
                else if (on) cv_rect(sx, ly + 1, cw - 2, lane - 3, cur ? T_TEXT : c);
                else cv_rect(sx, ly + lane - 3, cw - 2, 1, dim);
                if (cur && r == 0u) cv_rect(sx, top + 4 * lane - 1, cw - 2, 2, T_TEXT);   /* (the step playing, under all four) */
            }
        }
        for (uint32_t i = 0; i < 4u; i++) {
            int32_t bw = is_panel ? 44 : 40, bx = (is_panel ? 18 : 13) + (int32_t)i * (bw + 8);
            int32_t bh = 3 + (int32_t)(p->pitch[i] > n - 1u ? n - 1u : p->pitch[i]) * (nh - 3) / (int32_t)(n - 1u);
            cv_rect(bx, ny + nh - 1, bw, 1, dim);
            cv_rrect(bx, ny + nh - bh, bw, bh, 2, i == p->pos ? T_TEXT : c, bg);
        }
    }
}

/* FORM page: one cell per section of the form (tall = A, medium = B, short = C); the playing one is lit
 * and a line under it fills with the bars elapsed */
static void graph_form(const track_t *t, uint16_t c)
{
    uint32_t tr = trk_index(t), pos = 0, bar = 0, of = 0, i, len, w;
    uint8_t seq[4];
    int flash = 0;
    len = tessera_form_get(tr, seq, &pos);
    tessera_phrase(tr, &bar, &of, &flash);
    if (!len) {
        cv_rect(13, 30, 200, 2, T_RAISE);
        return;
    }
    w = (200u - 4u * (len - 1u)) / len;
    for (i = 0; i < len; i++) {
        int32_t x = 13 + (int32_t)(i * (w + 4u)), hh = 18 - 6 * (int32_t)seq[i];
        uint16_t col = i == pos ? (flash ? T_TEXT : c) : T_RAISE;
        cv_rrect(x, 36 - hh, (int32_t)w, hh, 1, col, T_SURF);
        cv_rect(x, 39, (int32_t)w, 1, T_RAISE);
        if (i == pos && of)
            cv_rect(x, 39, (int32_t)(w * (bar + 1u) / of), 2, c);
    }
}

/* EVOLVE page: phrase progress strip, one cell per bar until the next mutation; all cells flash on a mutation */
static void graph_evolve(const track_t *t, uint16_t c)
{
    uint32_t tr = trk_index(t), bar = 0, of = 0, n, i, w;
    int flash = 0;
    tessera_phrase(tr, &bar, &of, &flash);
    n = of ? of : 1u;
    w = (200u - 2u * (n - 1u)) / n;
    for (i = 0; i < n; i++) {
        int32_t x = 13 + (int32_t)(i * (w + 2u));
        uint16_t col = T_RAISE;
        if (of) {
            if (flash) col = c;
            else if (i < bar) col = T_TEXT;
            else if (i == bar && song.playing) col = c;
        }
        cv_rrect(x, 20, (int32_t)w, of ? 16 : 4, 1, col, T_SURF);
    }
}

/* TWEAK page: one small picture under each card. SHIFT: the note moved off its written pitch (the dashed line);
 * DROP: 8 hits, the share it drops hollow; HUMAN: 5 notes' gates and loudness, uneven as it rises; FOLLOW: the
 * four tracks, the one followed filled, this one dashed. At their off positions: level, full, even, none lit */
static void graph_tweak(const track_t *t, uint16_t c)
{
    static const uint8_t DROP_ORDER[8] = {3, 6, 0, 5, 2, 7, 1, 4};   /* which hits go first as DROP rises */
    static const int8_t HUM_LEN[5] = {-2, 5, -5, 1, 4}, HUM_VEL[5] = {3, -4, 1, -2, 5};   /* fixed jitter shapes */
    uint32_t tr = trk_index(t), i;
    const tessera_evolve_t *ev = &tessera.evolve[tr < TESSERA_TRACKS ? tr : 0u];
    int32_t x, y, sh = ev->shift, nd = ((int32_t)ev->drop * 8 + 50) / 100, hu = ev->human;
    /* SHIFT: +-24 over +-40 px around the middle */
    x = CARD_X(0u);
    for (i = 0; i < 9u; i++)
        cv_rect(x + 6 + (int32_t)i * 5, 61, 3, 1, T_MID);
    y = 61 - sh * 40 / 24;
    if (sh) cv_rect(x + 26, y < 61 ? y + 3 : 62, 2, y < 61 ? 61 - y - 3 : y - 62, T_LINE);
    cv_rrect(x + 10, y - 2, 34, 5, 2, sh ? c : T_DIM, T_SURF);
    /* DROP: two rows of four */
    x = CARD_X(1u);
    for (i = 0; i < 8u; i++) {
        int32_t hx = x + 5 + (int32_t)(i % 4u) * 12, hy = 44 + (int32_t)(i / 4u) * 22, gone = 0, k;
        for (k = 0; k < nd; k++)
            if (DROP_ORDER[k] == i) gone = 1;
        if (gone) cv_frame(hx, hy, 9, 14, T_LINE);
        else cv_rrect(hx, hy, 9, 14, 2, c, T_SURF);
    }
    /* HUMAN: gates shortened by up to HUMAN / 2 %, loudness +-HUMAN / 5 as the bar's thickness */
    x = CARD_X(2u);
    for (i = 0; i < 5u; i++) {
        int32_t w = 44 - 44 * (HUM_LEN[i] + 5) * hu / 1000, h = 5 + HUM_VEL[i] * hu / 100;
        cv_rrect(x + 5, 36 + (int32_t)i * 12 + (8 - h) / 2, w, h, 1, hu ? c : T_DIM, T_SURF);
    }
    /* FOLLOW: the tracks as numbered cells */
    x = CARD_X(3u);
    for (i = 0; i < TESSERA_TRACKS; i++) {
        int32_t cx = x + 4 + (int32_t)(i % 2u) * 25, cy = 40 + (int32_t)(i / 2u) * 25;
        char n[2] = {(char)('1' + i), 0};
        if (ev->follow == i + 1u) {
            cv_rrect(cx, cy, 21, 21, 3, c, T_SURF);
            cv_text_c(cx + 11, cy + 5, &AF_X, n, T_BG, c);
        } else {
            if (i == tr) cv_dashed(cx, cy, 21, 21, 3, ev->follow ? T_TEXT : T_MID);
            else cv_rrect(cx, cy, 21, 21, 3, T_RAISE, T_SURF);
            cv_text_c(cx + 11, cy + 5, &AF_X, n, i == tr ? T_TEXT : T_MID, i == tr ? T_SURF : T_RAISE);
        }
    }
}

/* TWEAK 2 page. BRANCH: a stem forking in two, the branch to TO as thick as its chance (none: the stem alone);
 * TO: the four tracks as numbered cells (as FOLLOW), the one hits go to filled, this one dashed; LOGIC: 8 steps of
 * this track (top), of WITH's (middle) and what plays (bottom); COND: 8 bars, those a hit may play in filled */
static void graph_tweak2(const track_t *t, uint16_t c)
{
    static const uint8_t ME = 0xD6u, OTHER = 0x4Du;         /* 8 steps' hits, step 1 the low bit */
    uint32_t tr = trk_index(t), i, res = ME;
    const tessera_evolve_t *ev = &tessera.evolve[tr < TESSERA_TRACKS ? tr : 0u];
    int on = ev->br_to && ev->br_to != tr + 1u, lg = ev->logic && ev->lg_with && ev->lg_with != tr + 1u;
    int32_t x, w;
    /* BRANCH: the stem from the bottom, this track's branch up left, TO's up right */
    x = CARD_X(0u);
    cv_rect(x + 24, 62, 3, 18, T_MID);
    w = on ? 1 + (int32_t)ev->branch * 4 / 100 : 0;
    cv_line_t(x + 25, 62, x + 8, 40, T_MID, 2);
    if (on && w) cv_line_t(x + 25, 62, x + 42, 40, c, w);
    else cv_line(x + 25, 62, x + 42, 40, T_LINE);
    cv_rrect(x + 3, 34, 10, 10, 3, T_RAISE, T_SURF);
    cv_rrect(x + 38, 34, 10, 10, 3, on && ev->branch ? c : T_RAISE, T_SURF);
    /* TO: the tracks as numbered cells */
    x = CARD_X(1u);
    for (i = 0; i < TESSERA_TRACKS; i++) {
        int32_t cx = x + 4 + (int32_t)(i % 2u) * 25, cy = 40 + (int32_t)(i / 2u) * 25;
        char n[2] = {(char)('1' + i), 0};
        if (on && ev->br_to == i + 1u) {
            cv_rrect(cx, cy, 21, 21, 3, c, T_SURF);
            cv_text_c(cx + 11, cy + 5, &AF_X, n, T_BG, c);
        } else {
            if (i == tr) cv_dashed(cx, cy, 21, 21, 3, on ? T_TEXT : T_MID);
            else cv_rrect(cx, cy, 21, 21, 3, T_RAISE, T_SURF);
            cv_text_c(cx + 11, cy + 5, &AF_X, n, i == tr ? T_TEXT : T_MID, i == tr ? T_SURF : T_RAISE);
        }
    }
    /* LOGIC: three rows of 8 steps */
    if (lg) res = ev->logic == TESS_LG_AND ? ME & OTHER : ev->logic == TESS_LG_OR ? ME | OTHER :
                  ev->logic == TESS_LG_XOR ? ME ^ OTHER : ME & ~OTHER & 0xFFu;
    x = CARD_X(2u);
    for (i = 0; i < 8u; i++) {
        int32_t hx = x + 3 + (int32_t)i * 6;
        if ((ME >> i) & 1u) cv_rect(hx, 36, 5, 8, T_MID);
        else cv_rect(hx, 42, 5, 2, T_RAISE);
        if ((OTHER >> i) & 1u) cv_rect(hx, 50, 5, 8, lg ? T_MID : T_DIM);
        else cv_rect(hx, 56, 5, 2, T_RAISE);
        if ((res >> i) & 1u) cv_rrect(hx, 66, 5, 12, 1, lg ? c : T_DIM, T_SURF);
        else cv_rect(hx, 76, 5, 2, T_RAISE);
    }
    /* COND: 8 bars in two rows of four */
    x = CARD_X(3u);
    for (i = 0; i < 8u; i++) {
        static const uint8_t PLAYS[TESS_COND_COUNT] = {0xFF, 0x55, 0xAA, 0x11, 0x22, 0x44, 0x88, 0x01, 0x80, 0x01, 0xFE};
        int32_t hx = x + 5 + (int32_t)(i % 4u) * 12, hy = 40 + (int32_t)(i / 4u) * 22;
        if ((PLAYS[ev->cond % TESS_COND_COUNT] >> i) & 1u) cv_rrect(hx, hy, 9, 14, 2, ev->cond ? c : T_DIM, T_SURF);
        else cv_frame(hx, hy, 9, 14, T_LINE);
    }
}

/* ORGANIC page (LFO family): the track's SLOTH, its three circuits as dots on their lines (TORPOR, APATHY, INERTIA from
 * the top, -1 left, +1 right), and its OCHD, eight bars up or down from the middle, slowest left. Live: graph_sig
 * redraws it as they move */
static void graph_organic(const track_t *t, uint16_t c)
{
    static const char *const NAME[3] = {"TOR", "APA", "INE"};
    uint32_t tr = trk_index(t) % TESSERA_TRACKS, i;
    for (i = 0; i < 3u; i++) {
        int32_t y = 22 + (int32_t)i * 28, v = organic_out(tr, i);
        cv_text(PANEL_X0, y - 5, &AF_X, NAME[i], T_MID);
        cv_rect(PANEL_X0 + 26, y, 84, 1, T_RAISE);
        cv_rect(PANEL_X0 + 67, y - 4, 1, 9, T_RAISE);
        cv_rrect(PANEL_X0 + 64 + v * 38 / 32767, y - 3, 7, 7, 3, c, T_SURF);
    }
    cv_rect(PANEL_X0 + 120, 50, 100, 1, T_RAISE);
    for (i = 0; i < 8u; i++) {
        int32_t x = PANEL_X0 + 122 + (int32_t)i * 12, h = organic_out(tr, 3u + i) * 38 / 32767;
        if (h > 0) cv_rect(x, 50 - h, 9, h, c);
        else if (h < 0) cv_rect(x, 51, 9, -h, c);
    }
}

static void graph_steps(const track_t *t, uint16_t c)
{
    uint32_t tr = trk_index(t);
    if (tr < TESSERA_TRACKS && tessera.mode[tr] != TESS_ENG_OFF && cur_page()->fam == FAM_GEN) {   /* (PATTERN: the steps) */
        graph_tessera_viz(tr, 0);
        return;
    }
    uint32_t i, len = (uint32_t)t->p[P_SLEN];
    for (i = 0; i < NSTEP; i++) {
        int32_t x = bar_x(i), y = 4 + (int32_t)(i / 16u) * 24;
        const step_t *st = &seq_steps(t)[i];
        if (i >= len)
            continue;
        if (step_on(st))
            cv_rrect(x, y, 9, 14, 2, (st->flags & SF_ACCENT) ? T_ACCENT : c, T_SURF);
        else if (st->time == ST_TIE)
            cv_rrect(x, y + 8, 9, 6, 1, T_MID, T_SURF);
        else
            cv_rrect(x, y + 11, 9, 3, 1, T_RAISE, T_SURF);
        if (i == ui.cursor)
            cv_rect(x, y + 16, 9, 2, T_ACCENT);
        else if (song.playing && i == t->seq_idx)
            cv_rect(x, y + 16, 9, 2, T_TEXT);
    }
}

/* STEP page (a melodic track): a piano roll of the cursor's 16-step page. PR_ROWS semitone rows of PR_RH px,
 * the highest on top; on the left a keyboard strip (a C key named on it, 9 px) (white keys RAISE, black keys a
 * shorter DIM; a key held on the track: its row ACCENT). Rows out of the track's scale (SCL ROOT / SCALE; CHR: the
 * black keys) are darker (QUIET), a C row closes with a RAISE line; step lines GRID, every 4th RAISE. A note is a bar in
 * its column (an accent: TEXT, the row's full height; the cursor step's notes: ACCENT), chords stacked, lane hits as
 * their notes; a TIE carries the bars of the note before through its column; a REST is empty; a slide is a TEXT
 * diagonal from the end of its bar into the next note. Notes outside the view: a 1 px mark on the edge. The cursor:
 * a TEXT column frame; the playhead: a 1 px ACCENT line. The view (proll.lo, the bottom row) fits the notes of the
 * page, else centres on the cursor's note, and follows in steps (a third of the way per frame); graph_signature()
 * holds it, so a page that does not change is not drawn again. */
#define PR_TOP 22                                   /* the panel's top on the screen (ui_pages.c: NOTES) */
#define PR_H 174                                    /* .. its rows */
#define PR_ROWS 14                                  /* semitones shown (1.17 octaves) */
#define PR_RH 12                                    /* px per row */
#define PR_Y0 6                                     /* the top row (panel y) */
#define PR_X0 22                                    /* the first step column */
#define PR_CW 13                                    /* px per step */
#define PR_KX 8                                     /* the keyboard strip, 12 px (black keys 7; a C key named on it) */
static struct {
    uint8_t lo, trk, init, moving;                    /* lo: the note of the bottom row; moving: on its way */
    uint32_t frame;
    int32_t play;                                     /* the playhead's column drawn (notes_play_x) */
} proll;
static const uint8_t KEY_BLACK[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};

static uint32_t pr_src(const track_t *t, uint32_t si, uint32_t len)   /* the step whose notes sound at si (NSTEP: none) */
{
    uint32_t k;
    for (k = 0; k < len && seq_steps(t)[si].time == ST_TIE; k++)
        si = (si + len - 1u) % len;
    return step_on(&seq_steps(t)[si]) ? si : NSTEP;
}
/* the rows lit by keys held on the selected track (bit r: row r from the top) */
static uint32_t pr_held(void)
{
    uint32_t k, i, m = 0;
    for (k = 0; k < 27u; k++)
        if (kb_chn[k] && kb_trk[k] == song.sel)
            for (i = 0; i < kb_chn[k]; i++) {
                int32_t r = (int32_t)proll.lo + PR_ROWS - 1 - kb_chord[k][i];
                if (r >= 0 && r < PR_ROWS)
                    m |= 1u << r;
            }
    return m;
}
static int32_t pr_row_y(int32_t n) { return PR_Y0 + ((int32_t)proll.lo + PR_ROWS - 1 - n) * PR_RH; }
/* NOTES draws original sample times on a timeline. Swing changes the grid
 * spacing; finishing a take with QNT updates its stored/displayed onset. */
static void notes_window(const track_t *t, uint32_t period, uint32_t *a, uint32_t *b)
{
    uint32_t base = notes_base(), end = base + notes_span();
    if (end > (uint32_t)t->p[P_SLEN]) end = (uint32_t)t->p[P_SLEN];
    *a = recording_prefix(t, period, base);
    *b = recording_prefix(t, period, end);
}
static int32_t notes_x(uint32_t at, uint32_t a, uint32_t b)
{
    /* Callers clip to the visible interval before converting to pixels. */
    return PR_X0 + (int32_t)((uint64_t)(at - a) * (16 * PR_CW) / (b - a));
}
/* the playhead's column, -1: stopped or out of the view. The step and the samples into it read as one pair (the audio
 * may step on between the two reads: a step back for a frame) */
static int32_t notes_play_x(const track_t *t)
{
    const volatile track_t *v = t;
    uint32_t period = seq_div_samples((uint32_t)t->p[P_SDIV]), a, b, idx = 0, pos = 0, k, at;
    if (!song.playing)
        return -1;
    for (k = 0; k < 4u; k++) {
        idx = v->seq_idx;
        pos = v->seq_pos;
        if (idx == v->seq_idx)
            break;
    }
    if (pos >= 0x7FFFFFFFu)
        return -1;
    notes_window(t, period, &a, &b);
    at = recording_prefix(t, period, idx) + pos;
    return at >= a && at < b ? notes_x(at, a, b) : -1;
}
static void notes_follow(const track_t *t)
{
    uint32_t period = seq_div_samples((uint32_t)t->p[P_SDIV]), a, b, chosen = notes_selected(t);
    notes_window(t, period, &a, &b);
    int32_t lo = 127, hi = -1;
    for (uint32_t i = recording_head[recording_owner(t)]; i < RECORD_MAX; i = recording_next[i]) {
        if (!recording_active(t, i)) continue;
        uint32_t at = recording_raw_on(t, &recording[i], period);
        if (at < a || at >= b) continue;
        int32_t note = recording[i].note;
        if (note < lo) lo = note;
        if (note > hi) hi = note;
    }
    for (uint32_t si = 0; si < step_pattern_len(t); si++) {
        uint32_t source = pr_src(t, si, step_pattern_len(t));
        if (source == NSTEP || (seq_steps(t)[source].flags & SF_RECORDED)) continue;
        uint32_t at = recording_prefix(t, period, si);
        if (at < a || at >= b) continue;
        for (uint32_t j = 0; j < seq_steps(t)[source].n; j++) {
            int32_t note = seq_steps(t)[source].note[j];
            if (note < lo) lo = note;
            if (note > hi) hi = note;
        }
    }
    int32_t center = hi < 0 ? last_note : (lo + hi + 1) / 2;
    if (hi - lo >= PR_ROWS) {
        uint32_t at = notes_manual_start(t);
        if (chosen < RECORD_MAX) center = recording[chosen].note;
        else if (at < NSTEP && t->step[at].n) center = t->step[at].note[notes_manual_slot(&t->step[at])];
    }
    int32_t target = clamp(center - PR_ROWS / 2, 0, 128 - PR_ROWS);
    if (proll.frame == ui.frame && proll.init && !ui.force) return;
    int snap = ui.force || !proll.init || proll.trk != song.sel || ui.frame != proll.frame + 1u;
    int32_t d = target - proll.lo;
    proll.lo = (uint8_t)(snap ? target : proll.lo + (d / 3 ? d / 3 : d > 0 ? 1 : d < 0 ? -1 : 0));
    proll.moving = proll.lo != target; proll.trk = song.sel; proll.frame = ui.frame; proll.init = 1;
}
static void notes_bar(const track_t *t, uint32_t i, uint32_t period, uint32_t a, uint32_t b, uint16_t c, int selected)
{
    recorded_note_t snapshot = recording_snapshot(i);
    const recorded_note_t *r = &snapshot;
    uint32_t loop = recording_loop(t, period), at = recording_raw_on(t, r, period);
    uint64_t gate = (uint64_t)r->duration * (1u << (r->owner >> 5)) * period / RECORD_UNIT;
    if (!gate) gate = 1;
    if (gate > loop) gate = loop;
    int32_t y = pr_row_y(r->note), bottom = PR_Y0 + PR_ROWS * PR_RH;
    for (int32_t wrap = -1; wrap <= 0; wrap++) {
        int64_t start = (int64_t)at + (int64_t)wrap * loop, end = start + gate;
        if (end <= a || start >= b) continue;
        int32_t x = notes_x(start < a ? a : start, a, b), xe = notes_x(end > b ? b : end, a, b);
        if (xe < x + 2) xe = x + 2;
        if (xe > PR_X0 + 16 * PR_CW) xe = PR_X0 + 16 * PR_CW;
        if (y < PR_Y0 || y >= bottom) {
            cv_rect(x, y < PR_Y0 ? PR_Y0 : bottom - 1, xe - x, 1, c);
        } else {
            cv_rect(x, y + 1, xe - x, PR_RH - 2, c);
            if (selected) cv_frame(x, y, xe - x, PR_RH, T_TEXT);
            /* An onset tick keeps two close hits visible when tails overlap. */
            if (start >= a) cv_rect(x, y, 1, PR_RH, selected ? T_TEXT : T_MID);
        }
    }
}
static void graph_recorded_notes(const track_t *t, uint16_t c)
{
    uint32_t period = seq_div_samples((uint32_t)t->p[P_SDIV]), a, b, chosen = notes_selected(t);
    notes_window(t, period, &a, &b);
    notes_follow(t);
    uint32_t held = pr_held();
    int32_t bottom = PR_Y0 + PR_ROWS * PR_RH;
    for (int32_t row = 0; row < PR_ROWS; row++) {
        int32_t note = (int32_t)proll.lo + PR_ROWS - 1 - row, y = PR_Y0 + row * PR_RH;
        if (!(micro_active(t) ? ((note - 60) & 1) == 0 : scale_mask(t) == 0xFFFu ? !KEY_BLACK[note % 12] : (scale_mask(t) >> ((note - t->p[P_ROOT] + 120) % 12)) & 1u)) cv_rect(PR_X0, y, 16 * PR_CW + 1, PR_RH, T_QUIET);   /* (out of the scale: darker) */
        cv_rect(PR_KX, y, !micro_active(t) && KEY_BLACK[note % 12] ? 7 : 12, PR_RH - 1, !micro_active(t) && KEY_BLACK[note % 12] ? T_DIM : T_RAISE);
        if ((held >> row) & 1u) cv_rect(PR_KX, y, 12, PR_RH - 1, T_ACCENT);
        if (micro_active(t) ? ((note - 60) % (int32_t)scale_note_period(t) == 0) : note % 12 == 0) {
            char name[8];
            if (micro_active(t)) str_cpy(name, "D1", sizeof name); else note_name(name, (uint32_t)note);
            cv_text_on(PR_KX, y, &AF_X, name, T_MID, T_RAISE);
        }
    }
    uint32_t base = notes_base(), end = base + notes_span();
    if (end > (uint32_t)t->p[P_SLEN]) end = (uint32_t)t->p[P_SLEN];
    for (uint32_t step = base; step <= end; step++) {
        int32_t x = notes_x(recording_prefix(t, period, step), a, b);
        cv_rect(x, PR_Y0, 1, bottom - PR_Y0, step % 4u ? T_GRID : T_RAISE);
    }
    if (proll.play >= 0) cv_rect(proll.play, PR_Y0, 1, bottom - PR_Y0, T_ACCENT);   /* (set by the caller: notes_play_x) */
    /* Manual notes use the same time axis and selection language. Ties
     * appear as a continuous note, including a tail wrapping into this view. */
    uint32_t selected_start = notes_manual_start(t);
    for (uint32_t si = base; si < end; si++) {
        uint32_t source = pr_src(t, si, step_pattern_len(t));
        if (source == NSTEP || (seq_steps(t)[source].flags & SF_RECORDED)) continue;
        const step_t *st = &seq_steps(t)[source];
        int32_t x = notes_x(recording_prefix(t, period, si), a, b);
        int32_t xe = notes_x(recording_prefix(t, period, si + 1u), a, b);
        for (uint32_t j = 0; j < st->n; j++) {
            int32_t y = pr_row_y(st->note[j]);
            int selected = (bulk.on&&bulk_has(source)) || (chosen >= RECORD_MAX && source == selected_start && (j == notes_manual_slot(st) || (st->n > 1u && ui.hot_t && (ui.hot_col == 2u || ui.hot_col == 3u))));
            /* (a note tied on: no gap at its column's end, one bar) */
            int32_t lead = source == si ? 2 : 0, cont = si + 1u < end && pr_src(t, si + 1u, step_pattern_len(t)) == source;
            if (y < PR_Y0 || y >= bottom) { cv_rect(x + 1, y < PR_Y0 ? PR_Y0 : bottom - 1, xe - x - 1, 1, c); continue; }
            cv_rect(x + lead, y + 1, xe - x - lead - (cont ? 0 : 1), PR_RH - 2, selected ? T_ACCENT : c);
            if (selected) cv_frame(x + (source == si ? 1 : 0), y, xe - x - (source == si ? 2 : 0), PR_RH, T_TEXT);
            uint32_t next = (si + 1u) % step_pattern_len(t);
            const step_t *ns = &seq_steps(t)[next];
            if (!j && (st->flags & SF_SLIDE) && si + 1u < end && ns->time == ST_NOTE && ns->n) {
                int32_t ny = clamp(pr_row_y(ns->note[0]), PR_Y0, bottom - PR_RH);
                cv_line(xe - 2, y + 2, xe + 2, ny + 2, T_TEXT);
            }
        }
    }
    for (uint32_t i = recording_head[recording_owner(t)]; i < RECORD_MAX; i = recording_next[i])
        if (i != chosen && recording_active(t, i)) notes_bar(t, i, period, a, b, bulk.on&&bulk_note_has(i)?T_ACCENT:c, bulk.on&&bulk_note_has(i));
    if (chosen < RECORD_MAX) notes_bar(t, chosen, period, a, b, T_ACCENT, 1);
    if (ui.cursor >= base && ui.cursor < end) {         /* the cursor's column, last: its right side is the next step's
                                                         * line, its left a tied note's bar (both drawn over it before) */
        int32_t x = notes_x(recording_prefix(t, period, ui.cursor), a, b), xe = notes_x(recording_prefix(t, period, ui.cursor + 1u), a, b);
        cv_frame(x, PR_Y0 - 2, xe - x + 1, bottom - PR_Y0 + 4, T_TEXT);
    }
}

/* SEQ > STEP on a DRUM track: the grid, 8 lanes x the 16 steps of the page shown. Lanes by their two-letter
 * names (BD SD CP CH OH TM RS CB; CG CL CY on the other kits). A hit is a rounded square (accented: the
 * accent), an empty step a dot (brighter on the beats and on the selected lane); the selected lane is
 * underlaid RAISE, the cursor framed in TEXT; a TEXT bar over the step playing */
static void graph_grid(const track_t *t, uint16_t c)
{
    uint32_t l, i, len = (uint32_t)t->p[P_SLEN], base = ui.bank * 16u;
    int32_t y0 = 6;
    for (l = 0; l < NLANE; l++) {
        int32_t y = y0 + 4 + (int32_t)l * 14;
        int sel = l == ui.lane;
        uint16_t row = sel ? T_RAISE : T_SURF;
        if (sel)
            cv_rrect(6, y, 226, 14, 3, T_RAISE, T_SURF);
        cv_text_on(10, y - 1, &AF_S, drum_lane_abbr(t, l), sel ? T_ACCENT : T_MID, row);   /* ink rows 2 .. 11, as the hits */
        for (i = 0; i < 16u && base + i < len; i++) {
            const step_t *st = &seq_steps(t)[base + i];
            uint32_t b = 1u << l;
            int32_t x = 44 + (int32_t)i * 12;
            if (step_lanes(st) & b)
                cv_rrect(x, y + 2, 10, 10, 2, (step_accents(st) & b) ? T_ACCENT : c, row);
            else
                cv_rect(x + 4, y + 6, 2, 2, i % 4u == 0u || sel ? T_MID : T_DIM);
            if (sel && base + i == ui.cursor)          /* the cursor: a 1 px frame */
                cv_frame(x - 1, y + 1, 12, 12, T_TEXT);
        }
    }
    if (song.playing && t->seq_idx < len && t->seq_idx / 16u == ui.bank)
        cv_rect(44 + (int32_t)(t->seq_idx % 16u) * 12, y0 - 1, 10, 3, T_TEXT);
}
/* SCL: the 12 keys as rounded bars (black keys high, white keys low): in the scale THEME, the root the
 * accent, out of the scale RAISE */
static void graph_scale(const track_t *t, uint16_t c)
{
    static const uint8_t BLACK[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
    const micro_scale_t *s = micro_scale(t);
    if (s) {
        char label[24];
        fmt_int(label, s->count);
        str_cpy(label + str_len(label), " DEGREES / PERIOD", 20);
        cv_text_c(120, 8, &AF_S, label, T_MID, T_SURF);
        cv_rect(12, 91, 216, 1, T_RAISE);
        for (uint32_t d = 0; d <= s->count; d++) {
            int32_t x = 12 + s->pitch[d] * 216 / s->pitch[s->count];
            cv_rect(x, d ? 48 : 36, 2, d ? 42 : 54, d ? c : T_ACCENT);
        }
        return;
    }
    uint32_t i, mask = scale_mask(t);
    for (i = 0; i < 12u; i++) {
        uint32_t deg = (i + 12u - (uint32_t)t->p[P_ROOT]) % 12u;
        int32_t x = 13 + (int32_t)i * 18;
        uint16_t col = (mask >> deg) & 1u ? (deg == 0u ? T_ACCENT : c) : T_RAISE;
        cv_rrect(x + 3, BLACK[i] ? 6 : 38, 10, 52, 3, col, T_SURF);
    }
}
/* CHORD (SCL 2): the chord's name ("Cm7": the last one the track played; before any, the one on its ROOT from
 * C4) over the keys from the C below its lowest note (two octaves, three for a wide one): its notes THEME, its
 * root's ACCENT, the others RAISE. OFF or a kit: what to do instead */
static void panel_note(const char *a, const char *b, const char *c);
static void graph_chord(const track_t *t, uint16_t c)
{
    static const uint8_t BLACK[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
    uint8_t nn[CHORD_MAX];
    uint32_t n, i, j, k = trk_index(t), lo, cnt, w;
    int32_t r;
    uint16_t mask;
    char b[12];
    const char *cap = "LAST";
    if (!t->p[P_CHRD]) {
        panel_note("CHORD KEYS OFF", "[K1] DIA3: IN-KEY CHORDS", 0);   /* (the title says what they are) */
        return;
    }
    if (chord_kit(t)) {
        panel_note("NO CHORDS ON KITS", 0, 0);
        return;
    }
    if (micro_active(t)) {
        if (chord_last[k].n) {
            n = chord_last[k].n;
            for (i = 0; i < n; i++) nn[i] = chord_last[k].note[i];
        } else n = chord_make(t, 60u, nn, &r, &mask);
        cv_text_c(120, 10, &AF_S, "SCALE DEGREES", T_MID, T_SURF);
        for (i = 0; i < n; i++) {
            note_name(b, nn[i]);
            cv_text_c(30 + (int32_t)i * 60, 55, &AF_S, b, i ? c : T_ACCENT, T_SURF);
        }
        return;
    }
    if (chord_last[k].n) {
        r = chord_last[k].root;
        mask = chord_last[k].mask;
        n = chord_last[k].n;
        for (i = 0; i < n; i++)
            nn[i] = chord_last[k].note[i];
    } else {
        n = chord_make(t, (uint32_t)(60 + t->p[P_ROOT]), nn, &r, &mask);
        cap = "ON ROOT";
    }
    if (trk_vmode(t) != V_POLY)
        cap = "ROOT ONLY";                              /* MONO / LEGATO / UNISON */
    chord_name(b, (uint32_t)r, mask);
    cv_text_on(14, 8, &AF_M, b, T_TEXT, T_SURF);
    cv_text_r(226, 12, &AF_S, cap, T_MID, T_SURF);
    lo = nn[0] - nn[0] % 12u;
    cnt = nn[n - 1u] - lo < 24u ? 24u : 36u;
    w = 216u / cnt;
    for (i = 0; i < cnt; i++) {
        uint32_t note = lo + i, on = 0;
        int32_t x = 12 + (int32_t)(i * w);
        for (j = 0; j < n; j++)
            on |= nn[j] == note;
        cv_rrect(x, BLACK[note % 12u] ? 36 : 62, (int32_t)w - 2, 48, w > 6u ? 3 : 2,
                 !on ? T_RAISE : note % 12u == (uint32_t)r % 12u ? T_ACCENT : c, T_SURF);
    }
}


static uint32_t steps_hash(const track_t *t)
{
    uint32_t h = 2166136261u, i;
    for (i = 0; i < NSTEP; i++) {
        const step_t *st = &seq_steps(t)[i];
        h = (h ^ (st->note[0] + st->n * 128u + st->time * 1024u + st->flags * 4096u + st->note[1] * 65536u)) *
            16777619u;
        h = (h ^ (st->hit | (uint32_t)st->acc << 8 | (uint32_t)st->note[2] << 16 | (uint32_t)st->note[3] << 24)) * 16777619u;
    }
    return h;
}

static uint32_t str_hash(uint32_t h, const char *s)
{
    while (*s)
        h = (h ^ (uint8_t)*s++) * 16777619u;
    return h;
}

#if MELODEE_SLICE
/* SLICES (ui_slice.c): the waveform from the marker before the selected one to the one after it (the selected
 * slice tinted, its marker the accent, outside the slices DIM), under it the whole material with every marker and
 * the view; then the selected slice's length and the source */
static void graph_slices(void)
{
    uint32_t a, b, len, n = slice_count(), j = slice_sel(), i, src, div, ma, mb;
    char t[20], u[12];
    if (!slice_src(&src, &div) || !n) {
        panel_note("NO SAMPLE", "SRC: BREAK OR USR1-3", 0);
        return;
    }
    slice_view(&a, &b, &len);
    slice_env(a, b);
    ma = slice_mark(0);
    mb = slice_mark(n);
#define SPX(p) (12 + (int32_t)((uint32_t)((p) - a) * SP_COLS / (b - a)))   /* p in [a, b] */
    if (j < n) {                                     /* the selected slice */
        uint32_t s0 = slice_mark(j), s1 = slice_mark(j + 1u);
        int32_t x0 = SPX(s0 > a ? s0 : a), x1 = SPX(s1 < b ? s1 : b);
        if (x1 > x0)
            cv_rect(x0, 6, x1 - x0, 80, T_TINT);
    }
    for (i = 0; i < SP_COLS; i++) {
        uint32_t p = a + (uint32_t)((uint32_t)i * (b - a) / SP_COLS);
        cv_line(12 + (int32_t)i, 46 - sp.hi[i] * 38 / 64, 12 + (int32_t)i, 46 - sp.lo[i] * 38 / 64,
                p >= ma && p < mb ? T_THEME : T_DIM);
    }
    for (i = 0; i <= n; i++) {                       /* the markers in the view (the selected one last: on top) */
        uint32_t p = slice_mark(i);
        if (i != j && p >= a && p <= b)
            cv_rect(SPX(p), 4, 1, 84, T_MID);
    }
    {
        uint32_t p = slice_mark(j);
        if (p >= a && p <= b)
            cv_rect(SPX(p) > 226 ? 226 : SPX(p), 2, 2, 88, T_ACCENT);
    }
#undef SPX
    cv_rrect(12, 96, (int32_t)SP_COLS, 3, 1, T_RAISE, T_SURF);   /* the whole: every marker, the view */
    for (i = 0; i <= n; i++)
        cv_rect(12 + (int32_t)((uint32_t)slice_mark(i) * (SP_COLS - 1u) / len), 94, 1, 7, i == j ? T_ACCENT : T_MID);
    {
        int32_t x0 = 12 + (int32_t)((uint32_t)a * SP_COLS / len), x1 = 12 + (int32_t)((uint32_t)b * SP_COLS / len);
        cv_rect(x0, 103, x1 - x0 < 2 ? 2 : x1 - x0, 2, T_THEME);
    }
    str_cpy(t, "LEN ", sizeof t);
    slice_time(u, j < n ? slice_mark(j + 1u) - slice_mark(j) : len - mb);
    str_cpy(t + 4, u, sizeof t - 4);
    str_cpy(t + str_len(t), " S", sizeof t - str_len(t));
    cv_text(12, 106, &AF_S, t, T_MID);
    cv_text_r(228, 106, &AF_S, src ? N_SLC_SRC[src] : "BREAK", src ? T_THEME : T_DIM, T_SURF);
}
#endif

/* WHEEL: the nine drawbars as rounded bars over RAISE slots (the bars of the knob just turned: the accent),
 * their footages under them */
#if MELODEE_LEGACY_EXTRAS
static void graph_wheel(const track_t *t, uint16_t c)
{
    uint32_t k;
    static const char *const names[9] = {"16", "5.3", "8", "4", "2.7", "2", "1.6", "1.3", "1"};
    for (k = 0; k < 9u; k++) {
        int32_t x = 11 + (int32_t)k * 25, level = drw_level(t->p, k);
        if (t->p[P_E4] && k == 8u) level = 0; /* DSP's percussion cancels the 1-foot bar. */
        int hot = ui.hot_t && cur_page()->id[0] == P_E0 &&
                  (ui.hot_col == 0u || ui.hot_col == (k < 2u ? 1u : k < 4u ? 2u : 3u));
        cv_rrect(x + 7, 2, 4, 73, 2, T_RAISE, T_SURF);
        if (level) cv_rrect(x + 3, 75 - level * 8, 12, level * 8, 3, hot ? T_ACCENT : c, T_SURF);
        else cv_rrect(x + 3, 73, 12, 2, 1, hot ? T_ACCENT : T_DIM, T_SURF);
        cv_text_c(x + 9, 82, &AF_S, names[k], T_MID, T_SURF);
    }
}
#endif
/* The FM charts' parts: an operator box 21 x 17 (rows 23 px apart, columns 24), junction dots, Manhattan routes */
#define FM_BH 17
static void fm_dot(int32_t x, int32_t y, uint16_t c) { cv_rect(x - 1, y - 1, 3, 3, c); }
/* a Manhattan route, 1 px: down from (x0, y0) to the jog row jy, across to x1, down to y1 */
static void fm_route(int32_t x0, int32_t y0, int32_t x1, int32_t jy, int32_t y1, uint16_t c)
{
    cv_rect(x0, y0, 1, jy - y0, c);
    cv_rect(x0 < x1 ? x0 : x1, jy, (x0 < x1 ? x1 - x0 : x0 - x1) + 1, 1, c);
    cv_rect(x1, jy, 1, y1 - jy, c);
}
/* a feedback loop on the operator box at (x, y): out of its right side, up, back in on top with an arrow */
static void fm_loop(int32_t x, int32_t y, uint16_t c)
{
    cv_rect(x + 11, y + 5, 5, 1, c);
    cv_rect(x + 15, y - 4, 1, 9, c);
    cv_rect(x, y - 4, 15, 1, c);
    cv_rect(x, y - 4, 1, 4, c);
    cv_line(x - 2, y - 3, x - 1, y - 2, c); cv_line(x + 2, y - 3, x + 1, y - 2, c);
}
/* A return from a lower operator, outside the right edge of the whole stack. */
static void fm_stack_loop(int32_t x, int32_t from_y, int32_t to_y, uint16_t c)
{
    cv_rect(x + 11, from_y + 5, 5, 1, c);
    cv_rect(x + 15, to_y - 4, 1, from_y + 10 - to_y, c);
    cv_rect(x, to_y - 4, 15, 1, c);
    cv_rect(x, to_y - 4, 1, 4, c);
    cv_line(x - 2, to_y - 3, x - 1, to_y - 2, c); cv_line(x + 2, to_y - 3, x + 1, to_y - 2, c);
}
/* the output bus under the carriers (the leftmost at cl, the rightmost at cr) at y bus, an arrow out on the right */
static void fm_bus(int32_t cl, int32_t cr, int32_t bus)
{
    cv_rect(cl, bus, cr + 24 - cl, 1, T_MID);
    cv_line(cr + 20, bus - 3, cr + 23, bus, T_MID); cv_line(cr + 20, bus + 3, cr + 23, bus, T_MID);
}

#ifndef FM6_CHART_HOOK
#define FM6_CHART_HOOK(kind, a, b) ((void)0)   /* ui_render.c (host tests): the chart's parts one by one (its lint) */
#endif
enum { FMH_BOX, FMH_ROUTE, FMH_CAR, FMH_BUS, FMH_FB, FMH_LABEL, FMH_END };
/* FM6's 32 algorithms as charts. FM6_CELL: per operator 1..6 its cell, the column in 24 px steps (bits 0..2) and
 * the row above the output bus (bits 4..5; row 0 = the carriers). The routes, the carriers and the feedback
 * operator are fm6_core.c's FM6_ALG itself (fm6_routes, fm6_car_ops, fm6_fb_op). ui_test.c checks the cells
 * against them (a modulator one row above what it modulates, the carriers on the bottom row and only they, one
 * operator per cell) and the routes against the 32 algorithms written out; ui_render.c lints every chart as drawn
 * (no route through a box or touching another route, no two boxes overlapping, all inside the panel). The cells:
 * a search for the fewest and shortest bends, the operators numbered left to right. */
static const uint8_t FM6_CELL[32][6] = {
    {0x00, 0x10, 0x01, 0x11, 0x21, 0x31}, {0x00, 0x10, 0x02, 0x12, 0x22, 0x32}, {0x00, 0x10, 0x20, 0x01, 0x11, 0x21},
    {0x00, 0x10, 0x20, 0x01, 0x11, 0x21}, {0x00, 0x10, 0x01, 0x11, 0x02, 0x12}, {0x00, 0x10, 0x01, 0x11, 0x02, 0x12},
    {0x00, 0x10, 0x01, 0x11, 0x12, 0x22}, {0x00, 0x10, 0x01, 0x12, 0x11, 0x21}, {0x00, 0x10, 0x02, 0x12, 0x13, 0x23},
    {0x00, 0x10, 0x20, 0x01, 0x11, 0x12}, {0x00, 0x10, 0x20, 0x01, 0x11, 0x12}, {0x00, 0x10, 0x03, 0x12, 0x13, 0x14},
    {0x00, 0x10, 0x02, 0x11, 0x12, 0x13}, {0x00, 0x10, 0x01, 0x11, 0x20, 0x21}, {0x00, 0x10, 0x02, 0x12, 0x21, 0x22},
    {0x01, 0x10, 0x11, 0x21, 0x12, 0x22}, {0x01, 0x12, 0x10, 0x20, 0x11, 0x21}, {0x01, 0x10, 0x12, 0x11, 0x21, 0x31},
    {0x00, 0x10, 0x20, 0x01, 0x02, 0x11}, {0x00, 0x01, 0x10, 0x02, 0x12, 0x13}, {0x00, 0x01, 0x10, 0x02, 0x03, 0x12},
    {0x00, 0x10, 0x01, 0x02, 0x03, 0x12}, {0x00, 0x01, 0x11, 0x02, 0x03, 0x12}, {0x00, 0x01, 0x02, 0x03, 0x04, 0x13},
    {0x00, 0x01, 0x02, 0x03, 0x04, 0x13}, {0x00, 0x01, 0x11, 0x02, 0x12, 0x13}, {0x00, 0x01, 0x11, 0x03, 0x13, 0x14},
    {0x00, 0x10, 0x01, 0x11, 0x21, 0x02}, {0x00, 0x01, 0x02, 0x12, 0x03, 0x13}, {0x00, 0x01, 0x02, 0x12, 0x22, 0x03},
    {0x00, 0x01, 0x02, 0x03, 0x04, 0x14}, {0x00, 0x01, 0x02, 0x03, 0x04, 0x05}};
/* algorithm a's routes: m[i] bit j: operator j + 1 modulates operator i + 1. FM6_ALG runs the operators sixth
 * first, each reading a bus (or none) and writing one (or adding to it): who wrote a bus modulates its reader */
static void fm6_routes(uint32_t a, uint8_t *m)
{
    const uint8_t *f = FM6_ALG[a & 31u];
    uint8_t bus[4] = {0, 0, 0, 0};
    uint32_t k;
    for (k = 0; k < 6u; k++) {
        uint32_t op = 5u - k, in = (f[k] >> 4) & 3u, o = f[k] & 3u;
        m[op] = in ? bus[in] : 0u;
        bus[o] = (uint8_t)(((f[k] & FM6_OADD) ? bus[o] : 0u) | 1u << op);
    }
}
/* the operator with feedback in algorithm a (0..5: operator 1..6) */
static uint32_t fm6_fb_op(uint32_t a)
{
    uint32_t k;
    for (k = 0; k < 5u && (FM6_ALG[a & 31u][5u - k] & 0xC0u) != 0xC0u; k++)
        ;
    return k;
}
/* MARK I returns OP4 / OP5 in algorithms 4 / 6; MODERN and OPL return OP6 itself. */
static uint32_t fm6_fb_source(uint32_t a, uint32_t eng)
{
    uint32_t k, src = fm6_fb_op(a);
    if (eng == FM6_MARK1)
        for (k = 0; k < 6u; k++)
            if (FM6_ALG[a & 31u][k] & FM6_FBOUT)
                src = 5u - k;
    return src;
}
/* the carriers of algorithm a: bit k operator k + 1 (fm6_carriers counts the sixth first) */
static uint32_t fm6_car_ops(uint32_t a)
{
    uint32_t c = fm6_carriers(a), r = 0, k;
    for (k = 0; k < 6u; k++)
        r |= (c >> (5u - k) & 1u) << k;
    return r;
}
/* the algorithm FM6 plays on track t (0..31): ALG, or the patch's at PAT */
static uint32_t fm6_alg_of(const track_t *t)
{
    return t->p[P_E0] >= 1 && t->p[P_E0] <= 32 ? (uint32_t)t->p[P_E0] - 1u : fm6_patch[(t - trk) % NTRK][FP_ALG] & 31u;
}
/* FM6's EDIT pages: the algorithm, drawn as graph_fm draws DIGITAL's, "ALG 05" top left. Carriers THEME with INK
 * numerals, modulators RAISE with THEME ones; an operator at output level 0: a DIM numeral (a carrier on RAISE).
 * Routes THEME, the feedback loop MID (DIM at feedback 0), the output bus MID. The knob just turned in ACCENT:
 * ALG and PTCH the label, FB the loop, MLVL the routes, MRAT MEG VMOD the modulators, DTUN the carriers. */
static void graph_fm6(const track_t *t, uint16_t c)
{
    uint32_t alg = fm6_alg_of(t), k, j, car = fm6_car_ops(alg), top = 0, hi = 0, fbop = fm6_fb_op(alg), hotset = 0;
    uint32_t fbsrc = fm6_fb_source(alg, fm6_fn[(t - trk) % NTRK][FN_ENGINE]);
    uint32_t hot = ui.hot_t ? (uint32_t)cur_page()->id[ui.hot_col & 3u] : 0u;
    const uint8_t *pt = fm6_patch[(t - trk) % NTRK];
    int32_t x[6], y[6], bus, cl = 240, cr = 0, fb = clamp(pt[FP_FB] + t->p[P_E1], 0, 7);
    uint8_t m[6], fan[6] = {0, 0, 0, 0, 0, 0};
    uint16_t rc = hot == P_E2 ? T_ACCENT : c;
    char b[7] = {'A', 'L', 'G', ' ', (char)('0' + (alg + 1u) / 10u), (char)('0' + (alg + 1u) % 10u), 0};
    if (cur_page()->scope == SC_FMOP)                /* FM6's operator pages: the operator they show */
        hotset = 1u << (fm6_opsel % 6u), hot = 0;
    else if (cur_page()->scope == SC_FM6)
        hot = 0;                                     /* (their columns are no macros) */
    else if (hot == P_E3 || hot == P_E4 || hot == P_E5)
        hotset = 63u & ~car;
    else if (hot == P_E6)
        hotset = car;
    fm6_routes(alg, m);
    for (k = 0; k < 6u; k++) {
        uint32_t col = FM6_CELL[alg][k] & 7u, row = FM6_CELL[alg][k] >> 4;
        hi = col > hi ? col : hi;
        top = row > top ? row : top;
        for (j = 0; j < 6u; j++)
            fan[j] += (uint8_t)(m[k] >> j & 1u);
    }
    bus = 50 + (27 + 23 * (int32_t)top) / 2;         /* the chart centred on the 100 px scale */
    for (k = 0; k < 6u; k++) {
        x[k] = 120 + (int32_t)(FM6_CELL[alg][k] & 7u) * 24 - (int32_t)hi * 12;
        y[k] = bus - 6 - FM_BH - 23 * (int32_t)(FM6_CELL[alg][k] >> 4);
        if (car >> k & 1u) { cl = x[k] < cl ? x[k] : cl; cr = x[k] > cr ? x[k] : cr; }
    }
    for (k = 0; k < 6u; k++) {                       /* into k: its sources down to its jog row, across, down */
        for (j = 0; j < 6u; j++)
            if (m[k] >> j & 1u) {
                FM6_CHART_HOOK(FMH_ROUTE, j, k);
                fm_route(x[j], y[j] + FM_BH, x[k], y[k] - 3, y[k], rc);
                if (fan[j] > 1u) fm_dot(x[j], y[k] - 3, rc);          /* one operator modulating several */
                if (m[k] & (m[k] - 1u)) fm_dot(x[k], y[k] - 3, rc);   /* several modulating one */
            }
        if (car >> k & 1u) {
            FM6_CHART_HOOK(FMH_CAR, k, 0);
            cv_rect(x[k], y[k] + FM_BH, 1, bus - y[k] - FM_BH, T_MID);
            if (x[k] != cl) fm_dot(x[k], bus, T_MID);
        }
    }
    FM6_CHART_HOOK(FMH_BUS, 0, 0);
    fm_bus(cl, cr, bus);
    FM6_CHART_HOOK(FMH_FB, fbop, fbsrc);
    if (fbsrc != fbop)
        fm_stack_loop(x[fbop], y[fbsrc], y[fbop], hot == P_E1 ? T_ACCENT : fb ? T_MID : T_DIM);
    else
        fm_loop(x[fbop], y[fbop], hot == P_E1 ? T_ACCENT : fb ? T_MID : T_DIM);
    for (k = 0; k < 6u; k++) {
        uint32_t on = pt[(5u - k) * FP_OP + FP_OL] != 0, fill_c = (car >> k & 1u) && on;
        uint16_t f = hotset >> k & 1u ? T_ACCENT : c, fill = fill_c ? f : T_RAISE;
        char n[2] = {(char)('1' + k), 0};
        FM6_CHART_HOOK(FMH_BOX, k, 0);
        cv_rrect(x[k] - 10, y[k], 21, FM_BH, 3, fill, T_SURF);
        cv_text_c(x[k] + 1, y[k] + 1, &AF_S, n, fill_c ? T_INK : on ? f : T_DIM, fill);
    }
    FM6_CHART_HOOK(FMH_LABEL, 0, 0);
    cv_text(10, 4 - GOY, &AF_S, b, hot == P_E0 || hot == P_E7 ? T_ACCENT : T_MID);
    FM6_CHART_HOOK(FMH_END, alg, 0);
}

/* Save-bank validity is expensive (CRC/import). Refresh it once per redraw or
 * twice per second, rather than parsing four projects on every UI frame. */
static char graph_pname[4][13];                       /* .. and the slots' names ("" none) */
static uint32_t graph_pname_sig;
static int graph_project_used(uint32_t slot)
{
    static uint32_t ms, frame;
    static uint8_t mask, ready;
    if (!ready || fm1_ms - ms >= 500u || (ui.force && frame != ui.frame)) {
        uint32_t i; mask = 0;
        for (i = 0; i < 4u; i++) mask |= (uint8_t)((project_name(i, graph_pname[i]) != 0) << i);
        graph_pname_sig = fnv(2166136261u, graph_pname, sizeof graph_pname);
        ready = 1; ms = fm1_ms; frame = ui.frame;
    }
    return (mask >> (slot & 3u)) & 1u;
}
static const char *graph_project_name(uint32_t slot)  /* (after graph_project_used) */
{
    return graph_pname[slot & 3u];
}

/* play 0: without the playhead of NOTES and the drum grid (ui_pages.c sends it alone) */
static uint32_t graph_sig(int play)
{
    const page_t *pg = cur_page();
    const track_t *t = TSEL;
    uint32_t h = 2166136261u, i;
    if (ui.home)
        return h ^ (ui.frame / 2u);                  /* scope: redraw every other frame */
    h ^= (uint32_t)pg->graph * 131u + TSEL->eng_req + song.sel * 7777u + ui.page * 1291u;
    if (pg->graph == GR_NONE || pg->graph == GR_ARP || pg->graph == GR_MOTION) h ^= ui.frame / 2u;
    for (i = 0; i < P_COUNT; i++)
        h = (h ^ (uint32_t)t->p[i]) * 16777619u;
    h ^= (uint32_t)TSEL->preset * 7u + (uint32_t)song.g[G_SLOT] * 13u + TSEL->user * 257u + up_gen * 7919u;
    if (pg->graph == GR_ROLL && !grid_on()) {
        uint32_t chosen = notes_selected(t);
        notes_follow(t);
        h ^= bulk.on*37u + bulk.anchor*41u + bulk.end*43u;
        h ^= recording_generation * 7919u + chosen * 40503u + ui.note_zoom * 104729u + ui.cursor * 613u + ui.note_slot * 937u + proll.lo * 3001u + (ui.hot_t && (ui.hot_col == 2u || ui.hot_col == 3u) ? 8191u : 0u);
        if (song.playing && (song.rec & (1u << trk_index(t)))) h ^= ui.frame / 2u;
        h = (h ^ steps_hash(t)) * 16777619u;
        h ^= (uint32_t)song.g[G_SWING] * 65537u + pr_held() * 15331u;
        if (play) h ^= (uint32_t)(notes_play_x(t) + 1) * 31u;
    }
    if (pg->graph == GR_CHORD) {                     /* the last chord played */
        h = (h ^ (chord_last[song.sel].root + 131u * chord_last[song.sel].mask)) * 16777619u;
        for (i = 0; i < CHORD_MAX; i++)
            h = (h ^ chord_last[song.sel].note[i]) * 16777619u;
        h ^= (uint32_t)t->engine * 389u;             /* (MONO and kits follow the sounding engine) */
    }
    if (pg->scope == SC_FM6 || pg->scope == SC_FMOP) {   /* FM6's pages: the patch, switches, functions, bank */
        h ^= fm6_pgen[song.sel % NTRK] * 2654435761u + fm6_on[song.sel % NTRK] * 40503u + fm6_opsel * 131u +
             up_gen * 104729u;
        for (i = 0; i < FM6_NFN; i++)
            h = (h ^ fm6_fn[song.sel % NTRK][i]) * 16777619u;
    }
#if MELODEE_SLICE
    if (pg->graph == GR_SLICES && slice_page_ok()) h ^= slice_sig();
#endif
    if (pg->scope == SC_ENGINE && ((MELODEE_LEGACY_EXTRAS && t->eng_req % NENGINES == 7u) || t->eng_req % NENGINES == ENGI_FM6))
        h ^= (ui.hot_t ? ui.hot_col + 1u : 0u) * 65537u;
    if (pg->scope == SC_ENGINE && t->eng_req % NENGINES == ENGI_FM6)   /* the patch (PAT's algorithm, levels, FB) */
        h ^= (fm6_pgen[(t - trk) % NTRK] + 1u) * 2246822519u + fm6_fn[(t - trk) % NTRK][FN_ENGINE] * 40503u;
    if (pg->graph == GR_STEPS || pg->graph == GR_EVOLVE || pg->graph == GR_FORM || pg->graph == GR_ROLL || pg->graph == GR_DRUMHIT) {
        uint32_t ph = song.playing ? t->seq_idx : 0xFFFFu;
        if (pg->graph != GR_STEPS && (ph / 16u != ui.bank || (!play && pg->graph == GR_ROLL)))
            ph = 0xFFFFu;                            /* the roll shows the cursor's bank only */
        h ^= steps_hash(t) + ph * 31u + ui.cursor * 7919u + ui.lane * 104723u + ui.bank * 613u;
        h ^= tessera_sig(trk_index(t)) * 2654435761u;
    }
    if (pg->graph == GR_TWEAK && trk_index(t) < TESSERA_TRACKS) {   /* (its values live outside the track's p[]) */
        const tessera_evolve_t *ev = &tessera.evolve[trk_index(t)];
        h ^= ((uint32_t)(uint8_t)ev->shift | (uint32_t)ev->drop << 8 | (uint32_t)ev->human << 16 | (uint32_t)ev->follow << 24) * 2246822519u;
    }
    if (pg->graph == GR_ORGANIC)                     /* (it moves: a step a few pixels apart) */
        for (uint32_t i = 0; i < 11u; i++) h = h * 31u + (uint32_t)(organic_out(trk_index(t), i) >> 11);
    if (pg->graph == GR_TWEAK2 && trk_index(t) < TESSERA_TRACKS) {
        const tessera_evolve_t *ev = &tessera.evolve[trk_index(t)];
        h ^= ((uint32_t)ev->branch | (uint32_t)ev->br_to << 8 | (uint32_t)ev->br_mode << 12 | (uint32_t)ev->logic << 16 |
              (uint32_t)ev->lg_with << 20 | (uint32_t)ev->cond << 24) * 2654435761u;
    }
    return h;
}
static uint32_t graph_signature(void) { return graph_sig(1); }
/* 4-letter engine tags of the preset list */
static const char *eng_abbr(const char *name)
{
    static const char *const A[][2] = {{"PROPHET", "P5"},
                                       {"PHASE", "PHAS"}, {"CZ-1", "CZ-1"}, {"SID", "SID"},
                                       {"SAMPLE", "SMPL"}, {"VOICE", "VOCL"}, {"TRIO", "TRIO"}, {"WHEEL", "WHEL"},
                                       {"GRAIN", "GRAN"}, {"PHYS", "PHYS"}, {"DRUM", "DRUM"},
                                       {"NOISE", "NOIS"}, {"FM6", "FM6"}, {"SLICE", "SLCE"}};
    uint32_t i;
    for (i = 0; i < sizeof A / sizeof A[0]; i++)
        if (str_eq(name, A[i][0]))
            return A[i][1];
    return name;
}

/* a list row (17 px): the selected one a THEME bar with INK text; tag at x 14, free text from x 54 to x1 */
#define LIST_Y(k) (3 + 17 * (int32_t)(k))
static void list_row(int32_t y, int sel, const char *tag, uint16_t tc, const char *name, uint16_t nc, int32_t x1)
{
    uint16_t bg = sel ? T_THEME : T_SURF;
    if (sel)
        cv_rrect(6, y, 228, 16, 4, T_THEME, T_SURF);
    cv_text_on(14, y + 1, &AF_S, tag, sel ? T_INK : tc, bg);
    cv_free_text(54, y + 1, &AF_S, name, sel ? T_INK : nc, bg, x1 - 54);
}
/* an empty list: a title and a hint, centred */
static void note_line(int32_t y, const char *s, uint16_t fg)   /* centred S; "[K2] ADD PATTERN": a key hint */
{
    uint32_t n;
    int32_t id = kc_tag(s, &n);
    if (id < 0) {
        cv_text_c(120, y, &AF_S, s, fg, T_SURF);
        return;
    }
    s += n;
    while (*s == ' ')
        s++;
    cv_text_on(cv_keycap(120 - kh_w((uint32_t)id, s) / 2, y + 1, (uint32_t)id, T_KEY, T_INK, T_SURF) + KH_GAP, y,
               &AF_S, s, fg, T_SURF);
}
static void panel_note(const char *a, const char *b, const char *c)
{
    cv_text_c(120, 30, &AF_M, a, T_TEXT, T_SURF);
    if (b) note_line(60, b, T_MID);
    if (c) note_line(82, c, T_DIM);
}

/* preset browser: the global list (every engine), the current one selected; tag DIM, name TEXT,
 * favourites starred (the accent), the selected row's suggested pattern at its right */

/* a list entry's tag and name, as the browser shows them: "P5" "It's a Proph", "F012" (native), "U07" (user preset) */
static void entry_label(uint32_t e, uint32_t k, char *tag, char *nm)
{
    if (e == USER_NATIVE_P5 || e == USER_NATIVE_FM || e == USER_NATIVE_CZ) {
        uint32_t eng = src_engine(e, k);
        tag[0] = eng == ENGI_PROPHET ? 'P' : eng == ENGI_FM6 ? 'F' : 'Z';
        tag[1] = (char)('0' + (k + 1u) / 100u);
        tag[2] = (char)('0' + (k + 1u) / 10u % 10u);
        tag[3] = (char)('0' + (k + 1u) % 10u);
        tag[4] = 0;
        native_name(eng, k, nm);
    } else if (e == USER_GENERAL) {
        up_slot_label(tag, k);
        up_name(k, nm);
    } else {
        str_cpy(tag, eng_abbr(ENGINES[e % NENGINES]->name), 6);
        str_cpy(nm, eng_preset(ENGINES[e % NENGINES], k)->name, 13);
    }
}
static void p5_short_name(const track_t *t,char *out,uint32_t size)
{
    char full[P5_NAME_LEN+1u];p5_patch_name(full,p5_patch_of((track_t *)t));str_cpy(out,full,size);
}
/* MIXER page: four SURF columns, one under each card: the circled numeral (filled and in the accent:
 * the selected track) with a REC / ARM / MUTE badge (P_MUTE, KNOB 1), the sound's short name (a MUTE badge
 * when armed and muted), the LEVEL knob (dB inside) with the output meter beside it, then the PAN
 * knobs with their values. Knobs: a 270 degree ring, the track RAISE, the value arc THEME (the knob just
 * turned: ACCENT; muted: DIM); PAN from 12 o'clock. Each column is its own canvas with its own signature:
 * while the transport runs only the meters move. */
#define TS_MY 39                                     /* meter slot: column rows 39 .. 70 */
#define TS_MH 32
#define KB_X 4                                       /* LEVEL knob: box 4..37 x 37..70, centre (21, 54) */
#define KB_Y 37
#define KS_Y 86                                      /* PAN knob: boxes 86..105, centres x 15 and 42 */
static struct {
    uint32_t col[NTRK];
    uint8_t meter[NTRK];
} ts;

static int32_t meter_px(int32_t a)                   /* |sample| (Q15) -> px: 60 dB over the slot */
{
    int32_t lg = 0, v;
    if (a < 64)
        return 0;
    while ((a >> lg) > 1)
        lg++;
    v = lg * 8 + (((a << 3) >> lg) & 7);             /* 8 log2(a): 48 (-54 dB) .. 128 (+6 dB) */
    return clamp((v - 48) * (TS_MH - 2) / 80, 0, TS_MH - 2);
}

/* a knob's ring: 270 degrees (7:30 .. 4:30 o'clock), 2 px, anti-aliased, from the quadrant mask of its size
 * (tools/gen_aa_keycaps.py: coverage and angle; the other three quadrants mirror it, no trigonometry here).
 * Box top-left (x, y), outer radius r. Angles: 1/1024 turn from 12 o'clock, clockwise, -384 .. 384; the value
 * arc is lo .. hi (in vc), the rest of the ring the track (tr); bg lies under the ring */
#define KA_END 384
static void knob_arc(int32_t x, int32_t y, int32_t r, const uint8_t *cov, const uint8_t *ang, int32_t lo, int32_t hi,
                     uint16_t tr, uint16_t vc, uint16_t bg)
{
    const uint16_t *rt = ramp(tr, bg), *rv = ramp(vc, bg);
    int32_t i, j, m;
    for (j = 0; j < r; j++)
        for (i = 0; i < r; i++) {
            uint32_t k = (uint32_t)(j * r + i), a = (cov[k >> 1] >> ((k & 1u) ? 0 : 4)) & 15u;
            int32_t q = ang[k];
            if (!a)
                continue;
            for (m = 0; m < 4; m++) {                /* top right, bottom right, bottom left, top left */
                int32_t s = m == 0 ? q : m == 1 ? 512 - q : m == 2 ? q - 512 : -q;
                int32_t px = m < 2 ? x + r + i : x + r - 1 - i, py = (m == 0 || m == 3 ? y + r - 1 - j : y + r + j) + cv_oy;
                if (s < -KA_END || s > KA_END || (uint32_t)px >= cv_w || (uint32_t)py >= cv_h)
                    continue;
                cv_px[(uint32_t)py * cv_w + (uint32_t)px] = (s >= lo && s <= hi ? rv : rt)[a];
            }
        }
}
/* value v of lo..hi as an arc: from the start, or (bipolar, lo < 0) from 12 o'clock with a 1 px nub at 0 */
static void knob(int32_t x, int32_t y, int32_t r, const uint8_t *cov, const uint8_t *ang, int32_t v, int32_t lo,
                 int32_t hi, uint16_t vc)
{
    int32_t a0, a1;
    if (lo < 0) {
        int32_t s = v * KA_END / (v < 0 ? -lo : hi);
        a0 = (s < 0 ? s : 0) - 8;
        a1 = (s > 0 ? s : 0) + 8;
    } else {
        a0 = -KA_END;
        a1 = v > lo ? -KA_END + (v - lo) * 2 * KA_END / (hi - lo) : -KA_END - 1;
    }
    knob_arc(x, y, r, cov, ang, a0, a1, T_RAISE, vc, T_SURF);
}

static uint32_t trk_level(uint32_t c) { return (uint32_t)trk[c].p[P_LEVEL] & 127u; }   /* LEVEL 0..127 */

static void trk_short_name(uint32_t c, char *b)      /* the track's sound, b holds 13 */
{
    const track_t *t = &trk[c];
    const engine_t *e = ENGINES[t->eng_req % NENGINES];
    if(t->eng_req==ENGI_PROPHET)p5_short_name(t,b,13);
    else if(user_of(t)<USER_NONE && t->user_native)native_name(t->eng_req,user_of(t),b);
    else if (user_of(t) < UP_SLOTS)up_name(user_of(t), b);
    else if (e->npresets)
        str_cpy(b, eng_preset(e, t->preset % e->npresets)->name, 13);
    else
        str_cpy(b, e->name, 13);
}

/* HOME note/chord names: the same recognizer as next, with the 1.0 fonts and palette. */
static const struct {
    uint16_t iv;                                     /* bit i: i semitones over the root */
    char q[8];
} CHORDS[] = {                                       /* simplest first: a chord off its bass takes the first that fits */
    {0x091, ""}, {0x089, "m"}, {0x049, "dim"}, {0x111, "aug"}, {0x0A1, "sus4"}, {0x085, "sus2"},
    {0x491, "7"}, {0x891, "maj7"}, {0x489, "m7"}, {0x449, "m7b5"}, {0x249, "dim7"}, {0x4A1, "7sus4"},
    {0x291, "6"}, {0x289, "m6"}, {0x095, "add9"}, {0x08D, "madd9"}, {0x0B1, "add11"}, {0x0A9, "madd11"},
    {0x889, "mM7"}, {0x511, "7#5"}, {0x451, "7b5"}, {0x911, "maj7#5"}, {0x849, "dimM7"},
    {0x495, "9"}, {0x895, "maj9"}, {0x48D, "m9"}, {0x4A5, "9sus4"}, {0x295, "6/9"}, {0x28D, "m6/9"},
    {0x493, "7b9"}, {0x499, "7#9"}, {0x4D1, "7#11"}, {0x591, "7b13"}, {0x8D1, "maj7#11"},
    /* 11ths and 13ths, also without the 5th or the 9th */
    {0x4B5, "11"}, {0x4AD, "m11"}, {0x4A9, "m11"}, {0x42D, "m11"},
    {0x695, "13"}, {0x691, "13"}, {0x615, "13"}, {0x611, "13"},
    {0x6AD, "m13"}, {0x68D, "m13"}, {0x689, "m13"}, {0x60D, "m13"},
    {0xA95, "maj13"}, {0xA91, "maj13"}, {0xA15, "maj13"},
    /* 7ths and 9ths without the 5th */
    {0x411, "7"}, {0x811, "maj7"}, {0x409, "m7"}, {0x415, "9"}, {0x815, "maj9"}, {0x40D, "m9"},
    {0x413, "7b9"}, {0x419, "7#9"}, {0x851, "maj7#11"},
};
#define NCHORDS (sizeof CHORDS / sizeof CHORDS[0])

/* the chord of pitch classes pcs, *root its root: on the bass when it makes one,
 * else the simplest on another root (shown "/bass") */
static const char *chord_of(uint32_t pcs, uint32_t bass, uint32_t *root)
{
    uint32_t i, k, best = NCHORDS;
    for (i = 0; i < 12u; i++) {
        uint32_t r = (bass + i) % 12u, iv = ((pcs >> r) | (pcs << (12u - r))) & 0xFFFu;
        if (!((pcs >> r) & 1u))
            continue;
        for (k = 0; k < best; k++)
            if (CHORDS[k].iv == iv) {
                best = k;
                *root = r;
                break;
            }
        if (!i && best < NCHORDS)
            break;                                   /* on the bass: no slash */
    }
    return best < NCHORDS ? CHORDS[best].q : 0;
}

/* the lowest of n notes (note[] holds the first 8) that fit in w px of font f, "C4 E4 G4",
 * " .." when some are left out; returns how many are shown */
static uint32_t notes_fit(char *b, const uint8_t *note, uint32_t n, const aafont_t *f, int32_t w)
{
    uint32_t m, i;
    for (m = n < 8u ? n : 8u; m > 1u; m--) {
        b[0] = 0;
        for (i = 0; i < m; i++) {
            if (i)
                str_cpy(b + str_len(b), " ", 2);
            note_name(b + str_len(b), note[i]);
        }
        if (m < n)
            str_cpy(b + str_len(b), " ..", 4);
        if (text_w(f, b) <= w)
            return m;
    }
    note_name(b, note[0]);
    return 1;
}

/* oscilloscope of the output, triggered on a rising zero crossing: a RAISE centre line, the trace 2 px */
static int16_t scope_snap[SCOPE_N];          /* audio.c scope_buf, held still to draw: one screen at a time (this scope, the
                                              * stage wave) */
static void graph_scope(uint16_t c, int32_t top, int32_t h)
{
    int16_t *snap = scope_snap;
    uint32_t w = scope_w, i, trig = 0;
    int32_t cy = top + h / 2, py = cy, x, peak = 1500;
    for (i = 0; i < SCOPE_N; i++) {
        snap[i] = scope_buf[(w + i) & (SCOPE_N - 1u)];
        if (snap[i] > peak)
            peak = snap[i];
        else if (-snap[i] > peak)
            peak = -snap[i];
    }
    for (i = 1; i < SCOPE_N - 240u; i++)
        if (snap[i - 1] < 0 && snap[i] >= 0) {
            trig = i;
            break;
        }
    cv_rect(PANEL_X0, cy, PANEL_W, 1, T_RAISE);
    for (x = 0; x < PANEL_W; x++) {
        int32_t y = cy - snap[trig + (uint32_t)x] * (h / 2 - 10) / peak;   /* auto-scaled */
        if (x)
            cv_line_t(PANEL_X0 - 1 + x, py, PANEL_X0 + x, y, c, 2);
        py = y;
    }
}

/* Each arrangement row selects one bank per track. graph_y, graph_h: the panel's place (ui_pages.c: under its rings) */
static int32_t graph_y = Y_GRAPH, graph_h = H_GRAPH, graph_oy = GOY;   /* (graph_oy: the 100 px scale's top) */
static void draw_graph(void)
{
    const page_t *pg = cur_page();
    const track_t *t = TSEL;
    uint16_t c = ACC;
    uint32_t sig;
    sig = graph_signature();
    if (!ui.force && sig == ui.graph_sig)
        return;
    ui.graph_sig = sig;
    cv_begin(240, (uint32_t)graph_h, T_BG);
    cv_rrect(3, 0, 234, graph_h, 5, T_SURF, T_BG);   /* the panel */
    cv_bg = T_SURF;                                  /* (text drawn with cv_text lands on it) */
    cv_oy = graph_oy;                                /* graphs on a 100 px scale */
    if (ui.home) {                                   /* (Stage draws its own panel: ui_stage.c) */
        cv_oy = 0;
        graph_scope(c, 0, H_GRAPH);
    } else {
        switch (pg->graph) {
        case GR_LFO:
            graph_lfo(t, c);
            break;
        case GR_FORM:
            graph_form(t, c);
            break;
        case GR_EVOLVE:
            graph_evolve(t, c);
            break;
        case GR_TWEAK:
            cv_oy = 0;
            graph_tweak(t, c);
            break;
        case GR_TWEAK2:
            cv_oy = 0;
            graph_tweak2(t, c);
            break;
        case GR_ORGANIC:
            graph_organic(t, c);
            break;
        case GR_STEPS:
            graph_steps(t, c);
            break;
        case GR_DRUMHIT:
        case GR_ROLL:
            cv_oy = 0;
            if (grid_on()) graph_grid(t, c);
            else { proll.play = notes_play_x(t); graph_recorded_notes(t, c); }
            break;
        case GR_SCALE:
            if (scale_settings_page(pg)) {
                cv_oy = 0;
                cv_text_fit(12, 3, &AF_S, SCALE_TITLE[clamp(t->p[P_SCALE], 0, SCALE_TOTAL - 1u)], T_TEXT, T_SURF, 216);
                cv_oy = 20;
            }
            graph_scale(t, c);
            break;
        case GR_CHORD:
            cv_oy = 0;
            graph_chord(t, c);
            break;
#if MELODEE_SLICE
        case GR_SLICES:
            cv_oy = 0;
            graph_slices();
            break;
#endif
        case GR_FMEG: {                                  /* FM6: operator fm6_opsel's envelope */
            const uint8_t *op = &fm6_patch[song.sel % NTRK][(5u - fm6_opsel % 6u) * FP_OP];
            graph_dxenv(op + FP_R1, op + FP_L1, 0, c);
            break;
        }
        case GR_FMPEG:                                   /* FM6: the pitch envelope */
            graph_dxenv(fm6_patch[song.sel % NTRK] + FP_PR1, fm6_patch[song.sel % NTRK] + FP_PL1, 1, c);
            break;
        default:
#if MELODEE_LEGACY_EXTRAS
            if (pg->scope == SC_ENGINE && t->eng_req % NENGINES == 7u) graph_wheel(t, c);
            else
#endif
            if ((pg->scope == SC_ENGINE || pg->scope == SC_FM6 || pg->scope == SC_FMOP) &&
                     t->eng_req % NENGINES == ENGI_FM6) graph_fm6(t, c);   /* EDIT 1 and 2, FM6's pages */
            else { cv_oy = 0; graph_scope(c, 0, H_GRAPH); }
            break;
        }
    }
    cv_oy = 0;
    cv_blit(0, (uint32_t)graph_y);
}

/* Draw a parameter bar with a live LFO ghost modulation indicator */
static void draw_modulated_param_bar(int32_t x, int32_t y, int32_t w, int32_t h,
                                      int16_t base_val, int16_t live_val, 
                                      int16_t min, int16_t max, uint16_t c)
{
    int32_t range = max - min;
    if (range <= 0) range = 1;

    /* 1. Draw background track frame */
    cv_rrect(x, y, w, h, 1, T_RAISE, T_SURF);

    /* 2. Draw base encoder value (solid anchor bar) */
    int32_t base_w = ((int32_t)(base_val - min) * w) / range;
    base_w = clamp(base_w, 0, w);
    cv_rrect(x, y, base_w, h, 1, T_DIM, T_SURF);

    /* 3. Draw live LFO modulated range (ghost / dynamic fill) */
    int32_t live_w = ((int32_t)(live_val - min) * w) / range;
    live_w = clamp(live_w, 0, w);
    
    if (live_w > base_w) {
        cv_rrect(x + base_w, y + 1, live_w - base_w, h - 2, 1, c, T_SURF);
    } else if (live_w < base_w) {
        cv_rrect(x + live_w, y + 1, base_w - live_w, h - 2, 1, T_TEXT, T_SURF);
    }
}
