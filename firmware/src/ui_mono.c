/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* The Mono look (MELODEE_LOOK 1, a build of its own; tessera-notes/docs/design/MONO_UI.md): Monomachine and Squares &
 * Circles' face. The screen as 120 x 120 cells of 2 x 2 pixels, one ink on the palette's background, a 5 x 7 font. Header 0..9 (the page,
 * the engine, the four tracks as dots), the four knobs as dials 11..49, the picture 51..108, the footer's line 109 and
 * page tabs 111..119 (a clear row between them). Step 1, the prototype: every knob page (draw_page's own branch) gets the header, the dials and the
 * footer; GEN, PATTERN and LFO their pictures, the other pictures a stand-in. Stage, lists, popups, the browser and
 * the other screens of their own keep the detailed look for now. The knobs' labels and values come from
 * draw_columns as ever: draw_column hands them here (mo_capture) while a Mono page is drawn. The page is drawn whole
 * in two 240 x 120 canvases when anything on it changes (mo_sig); cv_blit sends only the rows that differ.
 * Included by ui_draw.c */
#define MO_BG T_BG                                      /* one ink: the palette's (NIGHT: white on black, DAY the reverse) */
#define MO_INK T_TEXT
#define MO_DIM T_MID
#define MO_PY0 52                                       /* the picture's rows: MO_PY0 .. MO_PY1 - 1 */
#define MO_PY1 109

static const uint8_t MO_FONT[64][7] = {   /* ' ' .. '_': 5 x 7, bit 4 the left column (lower case drawn as upper) */
    {0, 0, 0, 0, 0, 0, 0}, /*   */
    {4, 4, 4, 4, 4, 0, 4}, /* ! */
    {10, 10, 0, 0, 0, 0, 0}, /* " */
    {10, 10, 31, 10, 31, 10, 10}, /* # */
    {4, 15, 20, 14, 5, 30, 4}, /* $ */
    {24, 25, 2, 4, 8, 19, 3}, /* % */
    {12, 18, 20, 8, 21, 18, 13}, /* & */
    {12, 4, 8, 0, 0, 0, 0}, /* ' */
    {2, 4, 8, 8, 8, 4, 2}, /* ( */
    {8, 4, 2, 2, 2, 4, 8}, /* ) */
    {0, 4, 21, 14, 21, 4, 0}, /* *  */
    {0, 4, 4, 31, 4, 4, 0}, /* + */
    {0, 0, 0, 0, 12, 4, 8}, /* , */
    {0, 0, 0, 31, 0, 0, 0}, /* - */
    {0, 0, 0, 0, 0, 12, 12}, /* . */
    {0, 1, 2, 4, 8, 16, 0}, /* /  */
    {14, 17, 19, 21, 25, 17, 14}, /* 0 */
    {4, 12, 4, 4, 4, 4, 14}, /* 1 */
    {14, 17, 1, 2, 4, 8, 31}, /* 2 */
    {31, 2, 4, 2, 1, 17, 14}, /* 3 */
    {2, 6, 10, 18, 31, 2, 2}, /* 4 */
    {31, 16, 30, 1, 1, 17, 14}, /* 5 */
    {6, 8, 16, 30, 17, 17, 14}, /* 6 */
    {31, 1, 2, 4, 8, 8, 8}, /* 7 */
    {14, 17, 17, 14, 17, 17, 14}, /* 8 */
    {14, 17, 17, 15, 1, 2, 12}, /* 9 */
    {0, 12, 12, 0, 12, 12, 0}, /* : */
    {0, 12, 12, 0, 12, 4, 8}, /* ; */
    {2, 4, 8, 16, 8, 4, 2}, /* < */
    {0, 0, 31, 0, 31, 0, 0}, /* = */
    {8, 4, 2, 1, 2, 4, 8}, /* > */
    {14, 17, 1, 2, 4, 0, 4}, /* ? */
    {14, 17, 23, 21, 23, 16, 15}, /* @ */
    {14, 17, 17, 31, 17, 17, 17}, /* A */
    {30, 17, 17, 30, 17, 17, 30}, /* B */
    {14, 17, 16, 16, 16, 17, 14}, /* C */
    {28, 18, 17, 17, 17, 18, 28}, /* D */
    {31, 16, 16, 30, 16, 16, 31}, /* E */
    {31, 16, 16, 30, 16, 16, 16}, /* F */
    {14, 17, 16, 23, 17, 17, 15}, /* G */
    {17, 17, 17, 31, 17, 17, 17}, /* H */
    {14, 4, 4, 4, 4, 4, 14}, /* I */
    {7, 2, 2, 2, 2, 18, 12}, /* J */
    {17, 18, 20, 24, 20, 18, 17}, /* K */
    {16, 16, 16, 16, 16, 16, 31}, /* L */
    {17, 27, 21, 21, 17, 17, 17}, /* M */
    {17, 17, 25, 21, 19, 17, 17}, /* N */
    {14, 17, 17, 17, 17, 17, 14}, /* O */
    {30, 17, 17, 30, 16, 16, 16}, /* P */
    {14, 17, 17, 17, 21, 18, 13}, /* Q */
    {30, 17, 17, 30, 20, 18, 17}, /* R */
    {15, 16, 16, 14, 1, 1, 30}, /* S */
    {31, 4, 4, 4, 4, 4, 4}, /* T */
    {17, 17, 17, 17, 17, 17, 14}, /* U */
    {17, 17, 17, 17, 17, 10, 4}, /* V */
    {17, 17, 17, 21, 21, 21, 10}, /* W */
    {17, 17, 10, 4, 10, 17, 17}, /* X */
    {17, 17, 17, 10, 4, 4, 4}, /* Y */
    {31, 1, 2, 4, 8, 16, 31}, /* Z */
    {14, 8, 8, 8, 8, 8, 14}, /* [ */
    {0, 16, 8, 4, 2, 1, 0}, /* \\ */
    {14, 2, 2, 2, 2, 2, 14}, /* ] */
    {4, 10, 17, 0, 0, 0, 0}, /* ^ */
    {0, 0, 0, 0, 0, 0, 31}, /* _ */
};
static const int8_t MO_SIN[64] = {0, 12, 25, 37, 49, 60, 71, 81, 90, 98, 106, 112, 117, 122, 125, 126, 127, 126, 125, 122, 117, 112, 106, 98, 90, 81, 71, 60, 49, 37, 25, 12, 0, -12, -25, -37, -49, -60, -71, -81, -90, -98, -106, -112, -117, -122, -125, -126, -127, -126, -125, -122, -117, -112, -106, -98, -90, -81, -71, -60, -49, -37, -25, -12};

static struct {
    char label[8], val[12], unit[8];
    int32_t ratio;                                      /* 0..1000, -1: a list value (boxed, no dial) */
    uint8_t used, dim, hot;
} mo_col[4];
static uint32_t mo_sig;

static void mo_capture(uint32_t c, const char *label, const char *val, const char *unit, uint16_t vc, int32_t ratio,
                       int hot)
{
    if (c >= 4u || (!label[0] && !val[0]))
        return;
    str_cpy(mo_col[c].label, label, sizeof mo_col[c].label);
    str_cpy(mo_col[c].val, val, sizeof mo_col[c].val);
    str_cpy(mo_col[c].unit, str_eq(unit, label) ? "" : unit, sizeof mo_col[c].unit);
    mo_col[c].ratio = ratio > 1000 ? 1000 : ratio;
    mo_col[c].used = 1;
    mo_col[c].dim = vc == T_DIM;
    mo_col[c].hot = (uint8_t)hot;
}

/* ---- drawing in cells (2 x 2 pixels; cv_oy picks the canvas's half) ---- */
static void mo_fill(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)
{
    if (w > 0 && h > 0)
        cv_rect(x * 2, y * 2, w * 2, h * 2, c);
}
static void mo_px(int32_t x, int32_t y, uint16_t c) { mo_fill(x, y, 1, 1, c); }
static void mo_hl(int32_t x, int32_t y, int32_t w, uint16_t c) { mo_fill(x, y, w, 1, c); }
static void mo_vl(int32_t x, int32_t y, int32_t h, uint16_t c) { mo_fill(x, y, 1, h, c); }
static void mo_box(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)   /* an outline */
{
    mo_hl(x, y, w, c);
    mo_hl(x, y + h - 1, w, c);
    mo_vl(x, y, h, c);
    mo_vl(x + w - 1, y, h, c);
}
static void mo_dash(int32_t x, int32_t y, int32_t w, uint16_t c)
{
    for (int32_t i = 0; i < w; i += 2)
        mo_px(x + i, y, c);
}
static void mo_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c)
{
    int32_t dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y0 - y1 : y1 - y0, sx = x0 < x1 ? 1 : -1,
            sy = y0 < y1 ? 1 : -1, e = dx + dy;
    for (;;) {
        int32_t e2 = 2 * e;
        mo_px(x0, y0, c);
        if (x0 == x1 && y0 == y1)
            break;
        if (e2 >= dy) e += dy, x0 += sx;
        if (e2 <= dx) e += dx, y0 += sy;
    }
}
static void mo_circ(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    int32_t x = r, y = 0, e = 1 - r;
    while (x >= y) {
        mo_px(cx + x, cy + y, c); mo_px(cx + y, cy + x, c); mo_px(cx - y, cy + x, c); mo_px(cx - x, cy + y, c);
        mo_px(cx - x, cy - y, c); mo_px(cx - y, cy - x, c); mo_px(cx + y, cy - x, c); mo_px(cx + x, cy - y, c);
        y++;
        if (e < 0) e += 2 * y + 1;
        else x--, e += 2 * (y - x) + 1;
    }
}
static void mo_disc(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    for (int32_t y = -r; y <= r; y++) {
        int32_t w = 0;
        while ((w + 1) * (w + 1) + y * y <= r * r + r)
            w++;
        mo_hl(cx - w, cy + y, 2 * w + 1, c);
    }
}
/* sine of a (256 a turn), x 127 */
static int32_t mo_sin(int32_t a)
{
    uint32_t i = ((uint32_t)a & 255u) >> 2, f = (uint32_t)a & 3u;
    return (MO_SIN[i] * (int32_t)(4u - f) + MO_SIN[(i + 1u) & 63u] * (int32_t)f) / 4;
}
static int32_t mo_cos(int32_t a) { return mo_sin(a + 64); }

static int32_t mo_tw(const char *s) { int32_t n = (int32_t)str_len(s); return n ? n * 6 - 1 : 0; }
static int32_t mo_text_s(const char *s, int32_t x, int32_t y, int32_t sc, uint16_t c)   /* sc: cells a font pixel */
{
    for (; *s; s++, x += 6 * sc) {
        uint32_t ch = (uint8_t)*s, r, b;
        if (ch >= 'a' && ch <= 'z') ch -= 32u;
        if (ch < 32u || ch > 95u) ch = '?';
        for (r = 0; r < 7u; r++)
            for (b = 0; b < 5u; b++)
                if (MO_FONT[ch - 32u][r] & (16u >> b))
                    mo_fill(x + (int32_t)b * sc, y + (int32_t)r * sc, sc, sc, c);
    }
    return x;
}
static int32_t mo_text(const char *s, int32_t x, int32_t y, uint16_t c) { return mo_text_s(s, x, y, 1, c); }
static void mo_textc(const char *s, int32_t cx, int32_t y, uint16_t c) { mo_text(s, cx - mo_tw(s) / 2, y, c); }
static void mo_textr(const char *s, int32_t rx, int32_t y, uint16_t c) { mo_text(s, rx - mo_tw(s), y, c); }
static void mo_inv(const char *s, int32_t x, int32_t y)   /* ink on a filled box */
{
    mo_fill(x - 1, y - 1, mo_tw(s) + 2, 9, MO_INK);
    mo_text(s, x, y, MO_BG);
}
static int mo_vowel(char c)
{
    c = c >= 'a' && c <= 'z' ? (char)(c - 32) : c;
    return c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U';
}
/* four letters at most: the spaces out (SNP A: SNPA); longer, the first letter and the consonants after it (EVERY:
 * EVRY, PULSE: PLS); fewer than three of those, the first four letters */
static void mo_abbr(char *d, const char *s)
{
    char w[16];
    uint32_t i, k = 0, n = 0;
    for (i = 0; s[i] && n < 15u; i++)
        if (s[i] != ' ' && s[i] != '.')
            w[n++] = s[i];
    w[n] = 0;
    if (n <= 4u) {
        str_cpy(d, w, 5);
        return;
    }
    d[k++] = w[0];
    for (i = 1; i < n && k < 4u; i++)
        if (!mo_vowel(w[i]))
            d[k++] = w[i];
    if (k < 3u)
        k = 4u, str_cpy(d, w, 5);
    d[k] = 0;
}
/* the page's name: an engine's EDIT pages by the engine's names (CHIP, FILTER), as the detailed header */
static const char *mo_title(const page_t *pg)
{
    const char *t = pg->scope == SC_ENGINE ? ENGINES[TSEL->eng_req % NENGINES]->page_title[pg->id[0] != P_E0] : 0;
    return t && t[0] ? t : pg->title;
}
static void mo_cut(char *d, const char *s, uint32_t n)   /* s's first n characters */
{
    uint32_t i;
    for (i = 0; i < n && s[i]; i++)
        d[i] = s[i];
    d[i] = 0;
}
/* a dial: the ring and a pointer, v 0..1000 over 270 degrees from the lower left */
static void mo_dial(int32_t cx, int32_t cy, int32_t r, int32_t v, uint16_t c)
{
    int32_t a = -160 + v * 192 / 1000;                  /* -225 .. +45 degrees, 256 a turn */
    mo_circ(cx, cy, r, c);
    mo_line(cx, cy, cx + mo_cos(a) * (r - 1) / 127, cy + mo_sin(a) * (r - 1) / 127, c);
}

/* ---- header, dials, footer ---- */
static const char *const MO_GEN_NAME[TESS_ENG_COUNT] = {"STEP", "TURING", "EUCLID", "MARKOV", "MORPH", "GRIDS"};
static void mo_head(void)
{
    const page_t *pg = cur_page();
    char t[24], sub[8];
    uint32_t i;
    str_cpy(sub, eng_abbr(ENGINES[TSEL->eng_req % NENGINES]->name), sizeof sub);
    if (ui.msg_t && ui.msg[0]) {                        /* a message: in the title's place, inverted */
        mo_cut(t, ui.msg, 15);
        mo_inv(t, 2, 1);
    } else {
        str_cpy(t, mo_title(pg), sizeof t);
        if (pg->fam == FAM_GEN && pg->graph == GR_STEPS && song.sel < TESSERA_TRACKS) {
            str_cpy(t + str_len(t), "/", 2);
            str_cpy(t + str_len(t), MO_GEN_NAME[tessera.mode[song.sel] % TESS_ENG_COUNT], 8);
        }
        t[(94 - mo_tw(sub) - 4) / 6] = 0;               /* (room before the engine) */
        mo_text(t, 1, 1, MO_INK);
    }
    mo_textr(sub, 94, 1, MO_DIM);
    for (i = 0; i < 4u; i++) {                          /* the tracks: the selected one filled */
        if (i == song.sel) mo_disc(99 + (int32_t)i * 6, 4, 2, MO_INK);
        else mo_circ(99 + (int32_t)i * 6, 4, 2, MO_INK);
    }
    if (song.rec & (1u << song.sel))                    /* REC: a bar under the track's dot */
        mo_hl(97 + (int32_t)song.sel * 6, 8, 5, MO_INK);
    mo_hl(0, 9, 120, MO_INK);
}
/* a value as its column shows it: five characters at most (MORPH, 0.25); longer, a number loses its last decimals
 * (128.00: 128.0) and a word its vowels from the end (MARKOV: MARKV, REVERB: REVRB); still longer, cut (1: the Mono
 * lint reports it) */
static int mo_fit(char *d, const char *s)
{
    uint32_t n, i, j;
    str_cpy(d, s, 16);
    n = str_len(d);
    if (n > 5u && (d[0] == '-' || d[0] == '+' || (d[0] >= '0' && d[0] <= '9'))) {
        for (i = 0; d[i] && d[i] != '.'; i++) ;
        while (d[i] && n > 5u && n > i + 1u)
            d[--n] = 0;
        if (d[i] && n == i + 1u)
            d[--n] = 0;                                 /* (no decimal left: no point) */
    }
    for (i = n; n > 5u && i-- > 1u;)
        if (mo_vowel(d[i])) {
            for (j = i; j < n; j++) d[j] = d[j + 1u];
            n--;
        }
    if (n <= 5u)
        return 0;
    d[5] = 0;
    return 1;
}
/* the four knobs' values as drawn, for the Mono lint (tests/ui_render.c): the frame (a box, a filled box or the
 * inverted block; -1 none), the text, the kind */
enum { MV_DIAL, MV_BOX, MV_KEY, MV_INV };
static struct { int16_t fx0, fx1, tx0, tx1; uint8_t kind, cut, used; char s[8]; } mo_vals[4];
static uint8_t mo_flash_col, mo_flash_t;               /* a one-shot knob's value inverted a moment (CLEAR) */
static void mo_flash(uint32_t c)
{
    mo_flash_col = (uint8_t)c;
    mo_flash_t = 13;                                    /* frames: about 200 ms */
}
static uint8_t mo_vals_new;                             /* mo_cols drew them since the lint last looked */
#define MO_BOX_W 27                                     /* a boxed value's outline: 4 characters and a clear cell each side */
static void mo_cols(void)
{
    char v[4][16];
    uint8_t wide[4], kind[4], cut[4];
    uint32_t i;
    for (i = 0; i < 4u; i++) {                          /* what each column shows, first: a wide one narrows its neighbours' margins */
        const char *s = mo_col[i].val;
        uint32_t n;
        wide[i] = kind[i] = cut[i] = 0;
        v[i][0] = 0;
        if (!mo_col[i].used)
            continue;
        if (s[0] == '[') {                              /* "[OCT+]": the key that acts, a filled box */
            str_cpy(v[i], s + 1, sizeof v[i]);
            n = str_len(v[i]);
            if (n && v[i][n - 1u] == ']') v[i][n - 1u] = 0;
            cut[i] = (uint8_t)mo_fit(v[i], v[i]);
            kind[i] = MV_KEY;
        } else {
            char b[24];
            str_cpy(b, s, sizeof b);
            if (mo_col[i].ratio >= 0 && str_len(b) + str_len(mo_col[i].unit) <= 5u)
                str_cpy(b + str_len(b), mo_col[i].unit, sizeof b - str_len(b));
            cut[i] = (uint8_t)mo_fit(v[i], b);
            kind[i] = mo_col[i].ratio < 0 ? MV_BOX : MV_DIAL;
        }
        if (str_len(v[i]) > 4u && kind[i] != MV_KEY)   /* five: no box, inverted at full length */
            wide[i] = 1, kind[i] = MV_INV;
    }
    for (i = 0; i < 4u; i++) {
        int32_t cx = 15 + (int32_t)i * 30, tw = mo_tw(v[i]), tx = cx - tw / 2, ty, fx0 = -1, fx1 = -1;
        uint16_t ink = mo_col[i].dim ? MO_DIM : MO_INK;
        char l[8];
        mo_vals[i].used = mo_col[i].used;
        if (!mo_col[i].used)
            continue;
        mo_abbr(l, mo_col[i].label);
        mo_textc(l, cx, 12, mo_col[i].hot ? MO_INK : MO_DIM);
        ty = mo_col[i].ratio >= 0 && mo_col[i].val[0] != '[' ? 39 : 25;
        if (mo_col[i].ratio >= 0 && mo_col[i].val[0] != '[')
            mo_dial(cx, 28, 7, mo_col[i].ratio, ink);
        if (mo_flash_t && i == mo_flash_col && (kind[i] == MV_BOX || kind[i] == MV_KEY)) {   /* it acted: inverted */
            fx0 = cx - MO_BOX_W / 2, fx1 = cx + MO_BOX_W / 2;
            mo_fill(fx0, 21, MO_BOX_W, 15, MO_INK);
            mo_text(v[i], tx, ty, MO_BG);
        } else if (kind[i] == MV_KEY) {
            fx0 = cx - MO_BOX_W / 2, fx1 = cx + MO_BOX_W / 2;
            mo_fill(fx0, 21, MO_BOX_W, 15, ink);
            mo_text(v[i], tx, ty, MO_BG);
        } else if (kind[i] == MV_BOX) {
            fx0 = cx - MO_BOX_W / 2, fx1 = cx + MO_BOX_W / 2;
            mo_box(fx0, 21, MO_BOX_W, 15, ink);
            mo_text(v[i], tx, ty, ink);
        } else if (kind[i] == MV_INV) {                 /* a cell of margin each side, but none toward another wide one
                                                         * (they keep a cell between them) or past the screen's edge */
            fx0 = tx - (i > 0u && wide[i - 1u] ? 0 : 1);
            fx1 = tx + tw - 1 + (i < 3u && wide[i + 1u] ? 0 : 1);
            fx0 = fx0 < 0 ? 0 : fx0;
            fx1 = fx1 > 119 ? 119 : fx1;
            mo_fill(fx0, ty - 1, fx1 - fx0 + 1, 9, ink);
            mo_text(v[i], tx, ty, MO_BG);
        } else {
            mo_text(v[i], tx, ty, ink);
        }
        mo_vals[i].fx0 = (int16_t)fx0, mo_vals[i].fx1 = (int16_t)fx1;
        mo_vals[i].tx0 = (int16_t)tx, mo_vals[i].tx1 = (int16_t)(tx + tw - 1);
        mo_vals[i].kind = kind[i], mo_vals[i].cut = cut[i];
        str_cpy(mo_vals[i].s, v[i], sizeof mo_vals[i].s);
        if (mo_col[i].hot)
            mo_hl(cx - 12, 48, 25, MO_INK);
    }
    mo_vals_new = 1;
    mo_dash(0, 50, 120, MO_DIM);
}
static void mo_foot(void)
{
    const page_t *pg = cur_page();
    uint32_t i, n = 0, cur = 0, first = 0;
    uint16_t idx[16];
    int32_t x = 1, w = 0;
    mo_hl(0, 109, 120, MO_INK);                         /* (row 110 clear: the selected tab's block below it) */
    for (i = 0; i < NPAGES && n < 16u; i++)             /* the family's pages, as the button turns through them */
        if (PAGES[i].fam == pg->fam && page_visible(i)) {
            if (&PAGES[i] == pg) cur = n;
            idx[n++] = (uint16_t)i;
        }
    for (i = 0; i < n; i++)                             /* how many fit from the first: start late enough to show cur */
        w += 6 * 4 + 5;
    while (first < cur && (int32_t)(cur - first + 1u) * 29 > 118)
        first++;
    for (i = first; i < n; i++) {
        char t[8], p[8] = "";
        const char *s = mo_title(&PAGES[idx[i]]);
        uint32_t k = 0, j;
        for (; *s && k < 4u; s++)                       /* four letters, no spaces: LFO 2 -> LFO2 */
            if (*s != ' ') t[k++] = *s;
        t[k] = 0;
        for (j = 0; j < i && !p[0]; j++) {              /* the same four as a page before: its last letter instead */
            const char *q = mo_title(&PAGES[idx[j]]);
            uint32_t m = 0;
            for (; *q && m < 4u; q++)
                if (*q != ' ') p[m++] = *q;
            p[m] = 0;
            if (!str_eq(p, t)) p[0] = 0;
        }
        if (p[0] && k == 4u) {
            const char *e = mo_title(&PAGES[idx[i]]);
            t[3] = e[str_len(e) - 1u];
        }
        if (x + mo_tw(t) > 119)
            break;
        if (i == cur) mo_inv(t, x + 1, 112);
        else mo_text(t, x + 1, 112, MO_DIM);
        x += mo_tw(t) + 5;
    }
    (void)w;
}

/* ---- the pictures ---- */
static void mo_pic_pattern(const track_t *t)
{
    uint32_t i, len = (uint32_t)t->p[P_SLEN], rows, rh, sh;
    char b[16];
    if (len > NSTEP) len = NSTEP;
    rows = (len + 15u) / 16u;
    rh = rows <= 2u ? 22u : 12u;                        /* a row's height, its steps' */
    sh = rows <= 2u ? 9u : 7u;
    for (i = 0; i < len; i++) {
        const step_t *st = &seq_steps(t)[i];
        int32_t x = 14 + (int32_t)(i % 16u) * 33 / 5, y = 54 + (int32_t)(i / 16u * rh);
        if (i % 16u == 0) {
            fmt_int(b, (int32_t)i + 1);
            mo_text(b, 1, y + (int32_t)sh - 7, MO_DIM);
        }
        if (step_on(st)) {
            mo_fill(x, y, 5, (int32_t)sh, MO_INK);
            if (st->flags & SF_ACCENT) mo_fill(x + 1, y + 1, 3, 2, MO_BG);   /* an accent: a notch */
        } else if (st->time == ST_TIE) {
            mo_box(x, y + (int32_t)sh - 4, 5, 4, MO_INK);
        } else {
            mo_fill(x, y + (int32_t)sh - 2, 5, 2, MO_DIM);
        }
        if (i == ui.cursor)
            mo_box(x - 1, y - 1, 7, (int32_t)sh + 2, MO_INK);
        if (song.playing && i == t->seq_idx)
            mo_hl(x, y + (int32_t)sh + 1, 5, MO_INK);
    }
    str_cpy(b, "LEN ", sizeof b);
    fmt_int(b + 4, (int32_t)len);
    mo_text(b, 1, 101, MO_DIM);
    if (song.playing) {
        str_cpy(b, "STEP ", sizeof b);
        fmt_int(b + 5, (int32_t)t->seq_idx % (int32_t)(len ? len : 1u) + 1);
        mo_textr(b, 118, 101, MO_INK);
    }
}
static void mo_kv(const char *k, uint32_t v, int32_t y, int inv)   /* the GEN pictures' side column */
{
    char b[8];
    fmt_int(b, (int32_t)v);
    if (v < 10u) { b[1] = b[0]; b[0] = '0'; b[2] = 0; }
    mo_text(k, 78, y, MO_DIM);
    if (inv) mo_inv(b, 100, y);
    else mo_text(b, 100, y, MO_INK);
}
static void mo_pic_euclid(const gen_euclid_t *g)
{
    int32_t cx = 42, cy = 80, r = 25;
    uint32_t n = g->steps ? g->steps : 1u, i, head = (g->step_idx + n - 1u) % n;
    char b[8];
    for (i = 0; i < n; i++) {
        int32_t a = -64 + (int32_t)(i * 256u / n), x = cx + mo_cos(a) * r / 127, y = cy + mo_sin(a) * r / 127;
        if (gen_euclid_hit((uint8_t)i, g->steps, g->pulses, g->rot)) mo_fill(x - 2, y - 2, 5, 5, MO_INK);
        else mo_box(x - 2, y - 2, 5, 5, MO_DIM);
        if (song.playing && i == head) mo_circ(x, y, 4, MO_INK);
    }
    mo_box(cx - 7, cy - 7, 15, 15, MO_INK);
    fmt_int(b, g->pulses);
    mo_textc(b, cx + 1, cy - 3, MO_INK);
    mo_kv("LEN", g->steps, 58, 1);
    mo_kv("HIT", g->pulses, 70, 1);
    mo_kv("ROT", g->rot, 82, 1);
    mo_kv("STP", head + 1u, 94, 0);
}
static void mo_pic_turing(const gen_turing_t *g)
{
    uint32_t len = g->len ? g->len : 16u, span = g->oct_span ? g->oct_span : 1u, i;
    for (i = 0; i < 16u; i++) {                         /* the register: its loop's bits, the rest dim */
        int32_t x = 2 + (int32_t)i * 73 / 10;
        uint32_t bit = i < len ? (g->reg >> (len - 1u - i)) & 1u : 0u;
        if (i >= len) mo_box(x, 55, 6, 6, MO_DIM);
        else if (bit) mo_fill(x, 55, 6, 6, MO_INK);
        else mo_box(x, 55, 6, 6, MO_INK);
    }
    mo_hl(2, 63, (int32_t)len * 73 / 10 - 1, MO_INK);    /* the loop's length */
    for (i = 0; i < len; i++) {                         /* each step's note, as a bar (as the detailed picture) */
        uint32_t raw = ((g->reg & (len >= 16u ? 0xFFFFu : ((1u << len) - 1u))) >> (len - 1u - i)) & 0xFFu;
        uint32_t v = ((raw / 7u) % span) * 7u + raw % 7u, mx = span * 7u - 1u;
        int32_t h = 4 + (int32_t)(mx ? v * 32u / mx : 0u), x = 2 + (int32_t)i * 73 / 10;
        if (song.playing && i == len - 1u) mo_fill(x, 104 - h, 6, h, MO_INK);
        else mo_box(x, 104 - h, 6, h, MO_INK);
    }
}
static void mo_pic_grids(const gen_grids_t *g, const track_t *t)
{
    static const char *const LN[3] = {"BD", "SD", "HH"};
    uint32_t steps = gen_grids_steps(tess_steps_per_bar(t)), cur = (g->step_idx + steps - 1u) % steps, p, i;
    int32_t px = 3 + g->x * 37 / 255, py = 55 + g->y * 37 / 255;
    mo_box(2, 54, 40, 40, MO_INK);                      /* the map, its 5 x 5 patterns, the point on it */
    for (i = 1; i < 5u; i++) {
        mo_dash(3, 54 + (int32_t)i * 8, 38, MO_DIM);
        for (int32_t y = 55; y < 93; y += 2) mo_px(2 + (int32_t)i * 8, y, MO_DIM);
    }
    mo_vl(px, 55, 38, MO_INK);
    mo_hl(3, py, 38, MO_INK);
    mo_disc(px, py, 2, MO_INK);
    for (p = 0; p < 3u; p++) {                          /* the three parts: 16 steps as two rows of eight */
        int32_t y = 56 + (int32_t)p * 17;
        mo_text(LN[p], 47, y + 2, MO_DIM);
        for (i = 0; i < 16u; i++) {
            int acc = 0;
            uint32_t s = steps > 16u ? i * 2u : i;
            int32_t x = 61 + (int32_t)(i % 8u) * 7, yy = y + (i < 8u ? 0 : 7);
            if (gen_grids_hit(g, s, steps, p, 0u, &acc)) mo_fill(x, yy, 5, 5, MO_INK);
            else mo_px(x + 2, yy + 2, MO_DIM);
            if (song.playing && (steps > 16u ? cur / 2u : cur) == i) mo_box(x - 1, yy - 1, 7, 7, MO_INK);
        }
    }
}
static void mo_pic_markov(const gen_markov_t *g)
{
    uint32_t d;
    char b[16];
    for (d = 0; d < 8u; d++) {                          /* the scale degrees, the one playing filled */
        int32_t h = 8 + (int32_t)d * 5, x = 6 + (int32_t)d * 14;
        if (d == g->cur_state % 8u) mo_fill(x, 98 - h, 10, h, MO_INK);
        else mo_box(x, 98 - h, 10, h, MO_DIM);
    }
    str_cpy(b, "DEGREE ", sizeof b);
    fmt_int(b + 7, g->cur_state % 8u + 1u);
    mo_text(b, 1, 101, MO_DIM);
}
static void mo_pic_morph(const gen_morph_t *g)
{
    uint32_t i, cur = g->step_idx & 15u;
    for (i = 0; i < 16u; i++) {
        int32_t x = 14 + (int32_t)i * 33 / 5;
        if ((g->snap_a >> i) & 1u) mo_fill(x, 55, 5, 9, MO_INK); else mo_fill(x, 62, 5, 2, MO_DIM);
        if ((g->snap_b >> i) & 1u) mo_fill(x, 73, 5, 9, MO_INK); else mo_fill(x, 80, 5, 2, MO_DIM);
        if (song.playing && i == cur) mo_hl(x, 84, 5, MO_INK);
    }
    mo_text("A", 1, 56, MO_DIM);
    mo_text("B", 1, 74, MO_DIM);
    mo_box(14, 92, 104, 6, MO_INK);                     /* where it sits between A and B */
    mo_fill(15, 93, (int32_t)g->morph_pos * 102 / 127, 4, MO_INK);
}
static void mo_pic_lfo(track_t *t)
{
    int32_t x, py = 0, uni = t->p[P_LPOL] != 0, z = uni ? 98 : 80, amp = uni ? 40 : 18, now;
    uint32_t ph = (uint32_t)t->p[P_LPHASE] << 25, step = 0xFFFFFFFFu / 58u;
    char b[16];
    const char *unit;
    mo_dash(2, z, 116, MO_DIM);
    for (x = 0; x < 116; x++) {                         /* two cycles from PHASE */
        int32_t w = lfo_wave(t, ph + (uint32_t)x * step), y;
        if (t->p[P_LWAVE] == 4)
            w = (int32_t)((x / 10 * 2654435761u) >> 16) - 32768;
        y = uni ? z - (w + 32768) * amp / 65536 : z - w * amp / 32768;
        if (x) mo_line(1 + x, py, 2 + x, y, MO_INK);
        py = y;
    }
    now = (int32_t)((t->lfo_ph - ph) / step);           /* where it is: on the first cycle */
    if (now >= 0 && now < 116) {
        int32_t w = t->lfo_val;
        mo_vl(2 + now, MO_PY0 + 2, 50, MO_DIM);
        mo_disc(2 + now, uni ? z - (w + 32768) * amp / 65536 : z - w * amp / 32768, 2, MO_INK);
    }
    param_format(&TP[P_LSYNC], t->p[P_LSYNC], b + 5, &unit);
    if (t->p[P_LSYNC]) { b[0] = 'S'; b[1] = 'Y'; b[2] = 'N'; b[3] = 'C'; b[4] = ' '; mo_text(b, 2, 102, MO_DIM); }
    else mo_text("FREE", 2, 102, MO_DIM);
    {
        int32_t v = t->lfo_val * 100 / 32767, a = v < 0 ? -v : v;
        b[0] = v < 0 ? '-' : '+';
        b[1] = (char)('0' + a / 100);
        b[2] = '.';
        b[3] = (char)('0' + a / 10 % 10);
        b[4] = (char)('0' + a % 10);
        b[5] = 0;
        mo_textr(b, 118, 102, MO_INK);
    }
}
static void mo_picture(void)
{
    const page_t *pg = cur_page();
    track_t *t = TSEL;
    uint32_t tr = song.sel;
    if (pg->graph == GR_STEPS && pg->fam == FAM_GEN && tr < TESSERA_TRACKS && tessera.mode[tr] != TESS_ENG_OFF) {
        switch (tessera.mode[tr]) {
        case TESS_ENG_TURING: mo_pic_turing(tessera_turing(tr)); return;
        case TESS_ENG_EUCLID: mo_pic_euclid(tessera_euclid(tr)); return;
        case TESS_ENG_MARKOV: mo_pic_markov(tessera_markov(tr)); return;
        case TESS_ENG_MORPH: mo_pic_morph(tessera_morph(tr)); return;
        case TESS_ENG_GRIDS: mo_pic_grids(tessera_grids(tr), t); return;
        }
    }
    if (pg->graph == GR_STEPS) {
        mo_pic_pattern(t);
        return;
    }
    if (pg->graph == GR_LFO) {
        mo_pic_lfo(t);
        return;
    }
    {                                                   /* the rest: a stand-in until its Mono picture is drawn */
        char b[12];
        mo_cut(b, mo_title(pg), 9);
        mo_text_s(b, 60 - (mo_tw(b) * 2 + 1) / 2, 66, 2, MO_INK);
        mo_textc("MONO PICTURE LATER", 60, 92, MO_DIM);
    }
}

/* what the page shows: drawn again only when it changes */
static uint32_t mo_signature(void)
{
    const track_t *t = TSEL;
    uint32_t h = 2166136261u, i;
    const uint8_t *p = (const uint8_t *)mo_col;
    for (i = 0; i < sizeof mo_col; i++)
        h = (h ^ p[i]) * 16777619u;
    h = (h ^ (uint32_t)(uintptr_t)cur_page()) * 16777619u;
    h = (h ^ song.sel ^ (uint32_t)song.playing << 4 ^ (uint32_t)song.rec << 8 ^ (uint32_t)TSEL->eng_req << 16) * 16777619u;
    h = (h ^ graph_signature()) * 16777619u;
    h = (h ^ t->seq_idx ^ (uint32_t)ui.cursor << 16) * 16777619u;
    if (song.sel < TESSERA_TRACKS) {
        h = (h ^ tessera.mode[song.sel] ^ (uint32_t)tessera.euclid[song.sel].step_idx << 8 ^
             (uint32_t)tessera.grids[song.sel].step_idx << 16 ^ (uint32_t)tessera.turing[song.sel].reg << 24) * 16777619u;
        h = (h ^ tessera.morph[song.sel].step_idx ^ (uint32_t)tessera.markov[song.sel].cur_state << 8) * 16777619u;
    }
    if (cur_page()->graph == GR_LFO)                    /* the LFO's dot: 58 places a cycle */
        h = (h ^ (t->lfo_ph / (0xFFFFFFFFu / 58u))) * 16777619u;
    if (ui.msg_t)
        h = (h ^ str_hash(1u, ui.msg)) * 16777619u;
    return h;
}

static void mono_page(void)
{
    uint32_t s, sig;
    memset(mo_col, 0, sizeof mo_col);
    mo_on = 1;
    draw_columns();                                     /* (its draw_column calls land in mo_capture) */
    mo_on = 0;
    if (mo_flash_t)
        mo_flash_t--;
    sig = mo_signature() ^ (mo_flash_t ? 0x5A5A5A5Au : 0u);
    if (!ui.force && sig == mo_sig)
        return;
    mo_sig = sig;
    for (s = 0; s < 2u; s++) {                          /* the top half, then the bottom */
        cv_begin(240, 120, MO_BG);
        cv_oy = -(int32_t)s * 120;
        mo_head();
        mo_cols();
        mo_picture();
        mo_foot();
        cv_oy = 0;
        cv_blit(0, s * 120u);
    }
}
