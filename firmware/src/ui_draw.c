/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Melodee UI drawing: the header, the four knob cards, the footer (steps + engine / preset / page)
 * and the frame; the panel between the cards and the footer is ui_graph.c. The layout: SURF cards and panels on
 * BG, rounded corners, no rules, colours from the theme tokens only (gfx.c T_*); THEME is the selected track's
 * colour (labels, gauges, curves), values in TEXT. Type: Chakra Petch, S (12 px) labels, M (15 px) values and the header,
 * L (26 px) big numerals. A card value too wide for M is set in S; free text (names, messages) is ellipsised. */
static void draw_menu(void);
static int name_on(void);                              /* NAME (ui_name.c) */
static void name_draw(void);
static int new_on(void);                               /* NEW SONG (ui_new.c) */
static void new_draw(void);

/* --------------------------------------------------------- drawing --- */
#define COL_W CARD_W                                  /* a card: 57 x 44 at x 3 + 59 c, y 28 */
#define COL_H CARD_H

static int32_t batt_level(void)                         /* thresholds 531 / 561 / 591 on ADC ch3 */
{
    return song.batt_raw >= 591 ? 3 : song.batt_raw >= 561 ? 2 : song.batt_raw >= 531 ? 1 : 0;
}
/* A USB host powers us; there is no separate charger status line.
 * 4 means external power (a steady bolt inside the battery), otherwise its level. */
static int32_t batt_shown(void)
{
    if (usb.config && !usb.suspended)
        return 4;
    return batt_level();
}

/* REC: filled, the selected track ready/recording; outlined, waiting or another track armed. */
static void draw_rec_mark(int32_t x, uint16_t bg)
{
    if (song.rec)
        cv_icon_mid(x, H_HEAD / 2, 16, ((song.rec & ~song.rec_wait) >> song.sel) & 1u ? ICON_X_REC : ICON_X_REC_O, T_REC, bg);
}

/* the battery: a 24 px icon (tools/gen_icons.py). The stock thresholds give 0..3 bars of 3; the font has 0..4 bars of 4:
 * each level takes the nearest share, 0 -> battery_0 (empty), 1 (1/3) -> battery_1 (1/4), 2 (2/3) ->
 * battery_3 (3/4), 3 (full) -> battery_4 (battery_2 is not used); USB power: battery_charging.
 * Colours as the drawn battery had them: one bar left the accent, empty MID (its outline), else THEME */
static uint32_t batt_icon(int32_t lvl)
{
    static const uint8_t I[5] = {ICON_X_BAT0, ICON_X_BAT1, ICON_X_BAT3, ICON_X_BAT4, ICON_X_BAT_CHG};
    return I[clamp(lvl, 0, 4)];
}
static void draw_battery(int32_t bx)
{
    int32_t lvl = batt_shown();
    cv_icon_mid(bx, H_HEAD / 2, 24, batt_icon(lvl), lvl == 1 ? T_ACCENT : lvl == 0 ? T_MID : T_THEME, T_BG);   /* 24 px: the glyph is wide and short */
}

/* ---------------------------------------------------- rolling digits --- */
/* A number that changes rolls its changed digits like a slot machine; only the drawing: the value, the gauge,
 * the hot colour and the sound change at once. Up: the old digit leaves upward and the new one comes from below;
 * down: the reverse. ROLL_FRAMES UI frames (ui.frame, ~135 ms) on an integer ease-out (ROLL_EASE: cubic, over
 * half-way in a quarter of the time, no overshoot); the ones digit first, each place one frame later, all ending
 * together on a frame equal to the static render. Clipped to the value strip, which alone is redrawn while it
 * rolls (a card: rows ROLL_Y.., the header: the BPM's columns). Characters other than digits never roll.
 * A change during a roll retargets it: it restarts from the value it was going to. Shown at once (no roll):
 * a change within ROLL_SNAP frames of the last one (a fast turn reads better as plain numbers), another
 * shape (length, sign, a non-digit, a name or an enum, a value set in S), another label or unit, a track /
 * engine / palette change (ui.roll[].sig), ui.force (a page change), a card's first draw. */
#define ROLL_FRAMES 9u
#define ROLL_SNAP 3u                                    /* frames (~45 ms) */
#define ROLL_BPM 4u                                     /* ui.roll[]: the four cards, then the header BPM */
#define ROLL_Y 15                                       /* a card's value strip: rows 15..34 (M) */
#define ROLL_H 19
#define BPM_X 208
#define BPM_W 26                                        /* the header's BPM strip: 26 columns, every row (S) */
static uint16_t roll_bg;                                /* what a card's value lies on (its fill: tinted while hot) */
static const uint8_t ROLL_EASE[17] = {0, 45, 84, 118, 147, 172, 193, 210, 223, 234, 242, 247, 251, 253, 254, 255, 255};

static int roll_digit(char c) { return c >= '0' && c <= '9'; }
/* +1 / -1: b rolls in from a (same length, digits where a has digits, the rest equal and one of + - .),
 * the sign of b - a; 0: b is shown at once */
static int roll_dir(const char *a, const char *b)
{
    uint32_t i;
    int d = 0, neg = 0;
    for (i = 0; a[i] || b[i]; i++) {
        if (i + 1u >= sizeof ui.roll[0].from || roll_digit(a[i]) != roll_digit(b[i]))
            return 0;
        if (!roll_digit(a[i]) && (a[i] != b[i] || (a[i] != '-' && a[i] != '+' && a[i] != '.')))
            return 0;
        neg |= a[i] == '-';
        if (!d && a[i] != b[i])
            d = b[i] > a[i] ? 1 : -1;
    }
    return neg ? -d : d;
}
/* field k now shows b instead of a: roll (a retarget mid-roll), or snap */
static void roll_note(uint32_t k, const char *a, const char *b, int snap)
{
    uint32_t el = (uint8_t)(ui.frame - ui.roll[k].t0);
    int d = roll_dir(a, b);
    ui.roll[k].t0 = (uint8_t)ui.frame;
    ui.roll[k].from[0] = 0;
    if (snap || !d || el < ROLL_SNAP)
        return;
    str_cpy(ui.roll[k].from, a, sizeof ui.roll[k].from);
    ui.roll[k].dir = (int8_t)d;
}
/* s (M) at x, y with field k's roll: each character at its pen in s (the digits are tabular and never kerned,
 * so the old value has the same pens); a rolling digit has moved o of the strip's h rows (ROLL_EASE, one frame
 * later per place from the right), clipped to the strip. The roll's last frame clears it: the static text. */
static int32_t roll_text(uint32_t k, int32_t x, int32_t y, const char *s, uint16_t fg)
{
    const aafont_t *f = k < ROLL_BPM ? &AF_M : &AF_S;
    uint32_t e = (uint8_t)(ui.frame - ui.roll[k].t0) + 1u, i, p = 0;
    const char *a = ui.roll[k].from;
    int32_t cy = k < ROLL_BPM ? y : 0, h = k < ROLL_BPM ? ROLL_H : H_HEAD;   /* (a card's value: at ROLL_Y) */
    uint16_t bg = k < ROLL_BPM ? roll_bg : T_BG;
    char t[8], ch[2] = {0, 0};
    if (e >= ROLL_FRAMES)
        ui.roll[k].from[0] = 0;
    if (!a[0])
        return cv_text_on(x, y, f, s, fg, bg);
    str_cpy(t, s, sizeof t);
    for (i = 0; s[i]; i++)
        p += (uint32_t)roll_digit(s[i]);
    cv_cy0 = (int16_t)(cy + cv_oy);                     /* the strip (the static characters lie inside it) */
    cv_cy1 = (int16_t)(cy + cv_oy + h);
    cv_scroll = 1;                                      /* (the lint: rolling digits are cut on purpose) */
    for (i = 0; s[i]; i++) {
        int32_t px, ny = y;
        t[i] = 0;
        px = x + text_w(f, t);                          /* (its pen: no digit kerns) */
        t[i] = s[i];
        p -= (uint32_t)roll_digit(s[i]);
        if (s[i] != a[i]) {                             /* p: the places right of it */
            int32_t o = 0, dir = ui.roll[k].dir;
            if (e > p)                                  /* (e < ROLL_FRAMES: the LUT index stays in 1..16) */
                o = (int32_t)((ROLL_EASE[((e - p) * 32u / (ROLL_FRAMES - p) + 1u) >> 1] * (uint32_t)h + 128u) >> 8);
            ch[0] = a[i];
            cv_text_on(px, y - dir * o, f, ch, fg, bg);
            ny = y + dir * (h - o);
        }
        ch[0] = s[i];
        cv_text_on(px, ny, f, ch, fg, bg);
    }
    cv_cy0 = 0;
    cv_cy1 = (int16_t)cv_h;
    cv_scroll = 0;
    return x + text_w(f, s);
}

/* the header (docs/design, 18 px): at the left a chip in the track's colour naming where you are (Stage: the engine;
 * a page: its title), then the page's dots, or a message in their place (a layer: its name); at the right the BPM
 * (x 208..234), the transport left of it (a song playing: its disc), then leftward the REC mark (filled: the selected
 * track armed; a ring: another one), the octave shift ("+2"), the song's row and a battery glyph only while it runs low. Which track: Stage's
 * columns, the pages' sound bar (pv_sound: its numeral) */
static uint32_t page_sheet(void);                       /* ui_popup.c */
static void list_words(char *d, const char *label, uint32_t n);   /* ui_list.c */
static int sec_on(void);                                /* ui_sections.c */
static uint32_t sec_of(uint32_t i);
static void sec_chip(uint32_t i, char *b, uint32_t n);
static int sec_map_on(void);
static void sound_name(const track_t *t, char *b);
static int ptc_on(void);                                /* ui_patterns.c */
static void head_chip_text(char *b)
{
    const page_t *pg = cur_page();
    uint32_t e = TSEL->eng_req % NENGINES;
    if (designer_on() && !name_on() && designer_head_chip(b)) {   /* the designer's START, FEATURES, original */
    } else if (new_on()) {                                     /* NEW SONG */
        str_cpy(b, "NEW SONG", 16);
    } else if (ptc_on()) {                              /* PATTERNS' Copy to: "copy 3 to 5" */
        str_cpy(b, "copy 1 to 1", 16);
        b[5] = (char)('1' + ui.ptc_src % NPAT);
        b[10] = (char)('1' + ui.ptc_dst % NPAT);
    } else if (ui.home) {                               /* Stage: the engine (a kit: its name, "909 KIT") */
        if (browse_pending()) { uint32_t kk, s = browse_shown(&kk); e = src_engine(s, kk); }
        if (ENGINES[eng_idx(e)] == &ENG_DRUM && !browse_pending()) sound_name(TSEL, b);
        else str_cpy(b, ENGINES[eng_idx(e)]->name, 16);
    } else if (sec_map_on()) {                          /* the map: the engine */
        str_cpy(b, ENGINES[e]->name, 16);
    } else if (pg->graph == GR_ROLL && grid_on()) {     /* the drum grid */
        str_cpy(b, "STEP", 16);
    } else if (pg->graph == GR_FMEG) {                  /* FM6's EG RATE / LVL: "OP3 EG" */
        str_cpy(b, "OP1 EG", 16);
        b[2] = (char)('1' + fm6_opsel % 6u);
    } else if (pg->scope != SC_ENGINE && sec_on()) {   /* an engine in sections: the page's name in its section */
        sec_chip(ui.page, b, 16);
    } else {
        const char *t = pg->scope == SC_ENGINE ? ENGINES[e]->page_title[pg->id[0] != P_E0] : pg->title;
        str_cpy(b, t, 16);
    }
}
/* the page's place in its family: n pages, the page the k-th (an engine in sections: the section's pages; the
 * designer: its stage's) */
static void page_place(uint32_t *pn, uint32_t *pk)
{
    const page_t *pg = cur_page();
    uint32_t i, n = 0, k = 0, sc = sec_on() ? sec_of(ui.page) + 1u : 0u;
    if (designer_on() && !name_on())
        designer_pages(&n, &k);
    else for (i = 0; i < NPAGES; i++)
        if (PAGES[i].fam == pg->fam && page_visible(i) &&
            (!sc || (PAGES[i].scope == SC_FMOP ? PAGES[ui.page].scope == SC_FMOP : sec_of(i) + 1u == sc))) {
            if (i == ui.page)
                k = n;
            n++;
        }
    *pn = n;
    *pk = k;
}
static int foot_tabs_on(void);                          /* (below) */
/* the page's place in the header: a dot a page, the page's a pill in the track's colour (no room: "3/12"); none
 * where the foot has the page tabs (foot_tabs_on) */
static void head_pages(int32_t x, int32_t x1)          /* (x: after the chip; x1: where the transport's marks, or the
                                                         * battery, start) */
{
    uint32_t i, n, k;
    if (foot_tabs_on())
        return;
    page_place(&n, &k);
    if (n < 2u)
        return;
    if (x + (int32_t)n * 9 + 8 > x1 - 6) {              /* (no room for the dots; none for that either: nothing) */
        char b[8];
        fmt_int(b, (int32_t)k + 1);
        str_cpy(b + str_len(b), "/", 2);
        fmt_int(b + str_len(b), (int32_t)n);
        if (x + text_w(&AF_X, b) <= x1 - 4)
            cv_text_on(x, 4, &AF_X, b, T_MID, T_BG);
        return;
    }
    for (i = 0; i < n; i++) {
        int32_t w = i == k ? 12 : 4;
        cv_rrect(x, 7, w, 4, 2, i == k ? T_THEME : T_LINE, T_BG);
        x += w + 5;
    }
}
/* Tessera's page tabs, right-aligned to x1 at row y: a numbered tile a page, the page's in the track's colour
 * (no room right of x0: "3/12"); returns their left edge (one page: x1, nothing drawn) */
static int32_t page_tabs(int32_t x0, int32_t x1, int32_t y)
{
    uint32_t i, n, k;
    int32_t w, x;
    char d[8];
    page_place(&n, &k);
    if (n < 2u)
        return x1;
    w = n > 9u ? 14 : 10;                               /* (a tile: 12 rows, its numeral's width and a margin) */
    if (x1 - (int32_t)n * (w + 2) + 2 < x0) {
        fmt_int(d, (int32_t)k + 1);
        str_cpy(d + str_len(d), "/", 2);
        fmt_int(d + str_len(d), (int32_t)n);
        return cv_text_r(x1, y, &AF_X, d, T_MID, T_BG);
    }
    x = x1 - (int32_t)n * (w + 2) + 2;
    for (i = 0; i < n; i++, x += w + 2) {
        uint16_t c = i == k ? T_THEME : T_SURF;
        fmt_int(d, (int32_t)i + 1);
        cv_rrect(x, y, w, 12, 2, c, T_BG);
        cv_text_c(x + w / 2, y, &AF_X, d, i == k ? T_INK : T_MID, c);
    }
    return x1 - (int32_t)n * (w + 2) + 2;
}
static void draw_head(void)
{
    char b[16], chip[16];
    int32_t x;
    uint32_t low = !(usb.config && !usb.suspended) && batt_level() <= 1, msg = ui.msg_t || ui.layer;
    uint32_t sig, moved = 0;
    int16_t bpm = song.g[G_BPM];                        /* (read once: what is noted is what is drawn) */
    head_chip_text(chip);
    sig = (uint32_t)seq_erase_active(TSEL) * 8191u + (uint32_t)song.playing * 3u + song.rec * 5u + song.rec_wait * 65537u + (uint32_t)(song.octave + 8) * 11u +
          song.sel * 13131u + (msg ? str_hash(7u, ui.msg_t ? ui.msg : layer_head()) : str_hash(5u, chip)) +
          (ui.bpm_t != 0) * 31u + low * 7777u + (chain.running ? (chain.row + 1u) * 104729u : 0u) + ux.gen * 977u +
          (ui.home ? 0u : ui.page * 2654435761u) + page_sheet() * 524287u + (uint32_t)gen_layer_on() * 40503u;
    if (bpm != ui.roll_bpm) {
        char a[8];
        fmt_int(a, ui.roll_bpm);
        fmt_int(b, bpm);
        roll_note(ROLL_BPM, a, b, ui.force);
        ui.roll_bpm = bpm;
        moved = 1;
    } else if (ui.force) {
        ui.roll[ROLL_BPM].from[0] = 0;
    }
    fmt_int(b, bpm);
    if (!ui.force && sig == ui.head_sig) {
        if (ui.roll[ROLL_BPM].from[0] || moved) {       /* rolling, or snapped to the new value: the BPM's strip only */
            cv_begin(BPM_W, H_HEAD, T_BG);
            roll_text(ROLL_BPM, BPM_W - text_w(&AF_S, b), 3, b, ui.bpm_t ? T_ACCENT : T_TEXT);
            cv_blit(BPM_X, Y_HEAD);
        }
        return;
    }
    ui.head_sig = sig;
    cv_begin(240, H_HEAD, T_BG);
    roll_text(ROLL_BPM, BPM_X + BPM_W - text_w(&AF_S, b), 3, b, ui.bpm_t ? T_ACCENT : T_TEXT);   /* (right-aligned) */
    cv_icon_mid(BPM_X - 14, H_HEAD / 2, 12, chain.running ? ICON_X_SONG : song.playing ? ICON_X_PLAY : ICON_X_STOP,
                song.playing ? T_TEXT : T_MID, T_BG);
    x = BPM_X - 14;                                     /* (the transport's left edge; the marks go leftward) */
    if (song.rec) {                                     /* (12 px, as the transport) */
        cv_icon_mid(x - 14, H_HEAD / 2, 12, ((song.rec & ~song.rec_wait) >> song.sel) & 1u ? ICON_X_REC : ICON_X_REC_O, T_REC, T_BG);
        x -= 14;
    }
    if (seq_erase_active(TSEL)) {
        cv_text_on(8, 3, &AF_S, "ERASING", T_REC, T_BG);
    } else if (msg) {                                   /* a message, or the layer's name (may hold a keycap) */
        cv_free_hint(8, 3, ui.msg_t ? ui.msg : layer_head(), T_TEXT, T_BG, x - 6 - 8);
    } else {
        int32_t cw = text_w(&AF_X, chip) + 16, l;
        if (cw < (ui.home ? 82 : 46)) cw = ui.home ? 82 : 46;   /* (a page: its name's, the dots beside it) */
        if (song.octave) {                              /* the octave shift, signed, in an outlined pill ("+2") */
            int32_t w;
            b[0] = song.octave > 0 ? '+' : '-';
            fmt_int(b + 1, song.octave < 0 ? -song.octave : song.octave);
            w = text_w(&AF_X, b) + 10;
            x -= w + 4;
            cv_rrect(x, 3, w, 12, 4, T_MID, T_BG);
            cv_rrect(x + 1, 4, w - 2, 10, 3, T_BG, T_MID);
            cv_text_c(x + w / 2, 3, &AF_X, b, T_TEXT, T_BG);
        }
        if (chain.running) {                            /* the song's row */
            str_cpy(b, "ROW ", 8);
            fmt_int(b + 4, (int32_t)chain.row + 1);
            x -= text_w(&AF_X, b) + 6;
            cv_text_on(x, 4, &AF_X, b, T_MID, T_BG);
        }
        if (low) {                                      /* the battery, only when it runs low */
            cv_icon_mid(x - 16, H_HEAD / 2, 12, batt_level() ? ICON_X_BAT1 : ICON_X_BAT0, T_REC, T_BG);
            x -= 18;
        }
        if (!ui.home && cur_page()->graph == GR_PATGRID) {   /* PATTERNS: its name plain (the grid is in colour);
                                                              * Copy to: what goes where, in the theme's colour */
            l = cv_text_on(8, 4, &AF_X, ptc_on() ? chip : "patterns", ptc_on() ? T_THEME : T_MID, T_BG);
        } else {
            cv_rrect(8, 2, cw, 14, 4, T_THEME, T_BG);
            cv_text_c(8 + cw / 2, 3, &AF_X, chip, T_INK, T_THEME);
            l = 8 + cw;
        }
        if (gen_layer_on()) {                           /* OCT+ held, a generator's layer: its key */
            cv_keycap(l + 6, 3, KC_OCTUP, T_THEME, T_INK, T_BG);
            l += 6 + kc_w(KC_OCTUP);
        } else if (page_sheet()) {                      /* the page has a sheet (OCT+ held) */
            cv_text_on(l + 4, 2, &AF_S, "\x85", T_MID, T_BG);
            l += 14;
        }
        if (!ui.home)                                   /* a page: where it is in its family */
            head_pages(l + 8, x);
    }
    cv_blit(0, Y_HEAD);
}
/* full redraw: the strips (header, cards, panel, footer; Stage: its panel and lanes) cover the rest; only the BG
 * between them is filled */
static void stage_frame(void);
static void draw_frame(int stage)
{
    uint32_t i;
    lcd_fill(0, H_HEAD, 240, Y_LABEL - H_HEAD, T_BG);
    lcd_fill(0, Y_SEP_END, 240, Y_GRAPH - Y_SEP_END, T_BG);
    if (stage)
        stage_frame();
    else
        lcd_fill(0, Y_GRAPH + H_GRAPH, 240, Y_FOOT - Y_GRAPH - H_GRAPH, T_BG);
    lcd_fill(0, Y_LABEL, (uint32_t)CARD_X(0), CARD_H, T_BG);
    for (i = 0; i < 4u; i++)                            /* right of each card */
        lcd_fill((uint32_t)(CARD_X(i) + CARD_W), Y_LABEL, i < 3u ? (uint32_t)(CARD_X(i + 1u) - CARD_X(i) - CARD_W) :
                 240u - (uint32_t)(CARD_X(i) + CARD_W), CARD_H, T_BG);
}

/* one card = one knob: LABEL (THEME) / value unit / gauge on a SURF card, redrawn only when it changed.
 * The value is M, or S when M is too wide (engine names, long ENUMs); the unit S after it. The gauge is
 * a 3 px rounded bar: LINE track, THEME fill. The knob just turned (hot): the card outlined in THEME, the label
 * and value in the accent. ratio: 0..1000 for the gauge, -1 = no gauge. icon: ICON_* (icons.c), ICON_AUTO = by label;
 * a label too long to share the card with its icon goes without it.
 * vc: the value's colour (T_THEME, T_DIM inactive, T_ACCENT the knob just turned) */
static uint8_t stage_drop_rows;                         /* Stage's knobs dropping in: the cards' bottom rows only, 0 all */
static void col_old_value(uint32_t c, char *ov)          /* the value card c drew before (in its cache key), 16 bytes */
{
    const char *k = ui.col[c];
    uint32_t i = 0;
    while (*k && *k != '|')
        k++;
    while (*k && k[1] && k[1] != '|' && i + 1u < 16u)
        ov[i++] = *++k;
    ov[i] = 0;
}
enum { CS_CARD, CS_STRIP, CS_RING, CS_FADER, CS_CHIP, CS_CAPTURE };   /* how draw_column draws a knob (ui_pages.c;
                                                                      * CS_CAPTURE: kept for a list's rows, ui_list.c) */
static void list_capture(uint32_t c, const char *label, const char *val, const char *unit, uint16_t vc);
static uint8_t col_style;                               /* (ui_pages.c pv_column) */
static uint8_t col_quiet;                               /* the next card's value is an aside (ROLL's seed): 9 px, mid */
static uint16_t col_pid = PID_NONE;                     /* the next card's parameter, for its live mark: col_mods */
static int16_t pv_live = -1;                            /* (draw_column to the redesign's rings, ui_pages.c pv_ring) */
/* the next card shows parameter pid of the selected track: with mods on it, draw_column marks on its gauge where they
 * move it now (cord.c cord_moves), as it moves */
static void col_mods(uint32_t pid) { col_pid = (uint16_t)pid; }
static void pv_column(uint32_t c, const char *label, const char *val, const char *unit, uint16_t vc, int32_t ratio,
                      int hot);
static void cu_columns(void);                           /* CORDS (ui_cords.c) */
static void mx_columns(void);                           /* MATRIX (ui_matrix.c) */
static void draw_column(uint32_t c, const char *label, const char *val, const char *unit, uint16_t vc,
                        int32_t ratio, uint32_t icon)
{
    char key[48];
    int hot = c == ui.hot_col && ui.hot_t, named = fmt_named, strip, snap;
    uint8_t sig;
    uint16_t lc = vc == T_DIM ? T_DIM : T_THEME, cbg = hot ? ux_mix(T_SURF, T_THEME, 22) : T_SURF;
    int32_t x, lx = 6, uw, room = COL_W - 10;
    const aafont_t *vf = &AF_M;
    uint32_t n, kn;
    int32_t kid = kc_tag(val, &kn), live = -1, sh;
    int quiet = col_quiet;
    col_quiet = 0;
    if (col_pid != PID_NONE && ratio >= 0 && cord_moves(col_pid, song.sel, &sh))   /* the mods on it, now */
        live = clamp(ratio + sh * 1000 / 32768, 0, 1000);
    col_pid = PID_NONE;
    if (col_style == CS_CAPTURE) {                      /* a list's row (ui_list.c) */
        list_capture(c, label, val, unit, vc);
        fmt_named = 0;
        return;
    }
    fmt_named = 0;                                      /* (params.c: a name, for this card only) */
    if (quiet)
        vf = &AF_X, vc = T_MID;
    if (str_eq(unit, label))
        unit = "";                                      /* "BPM 124 BPM", "USB OFF USB": the label says it */
    if (icon == ICON_AUTO)
        icon = icon_for_label(label);
    str_cpy(key, label, 12);                            /* cache key: texts + colour + gauge */
    str_cpy(key + str_len(key), "|", 2);
    str_cpy(key + str_len(key), val, 14);
    str_cpy(key + str_len(key), "|", 2);
    str_cpy(key + str_len(key), unit, 8);
    n = str_len(key);
    /* every RGB565 bit: status colours can change with identical text */
    key[n] = (char)('A' + (vc & 15u));
    key[n + 1] = (char)('A' + ((vc >> 4) & 15u));
    key[n + 2] = (char)('A' + ((vc >> 8) & 15u));
    key[n + 3] = (char)('A' + (vc >> 12));
    key[n + 4] = (char)(' ' + (ratio < 0 ? 0 : 1 + ratio / 20));
    key[n + 5] = (char)(icon == ICON_NONE ? '~' : '!' + icon % 90u);
    key[n + 6] = (char)('0' + hot);
    key[n + 7] = (char)(' ' + (live < 0 ? 0 : 1 + live * 44 / 1000));   /* (the live mark: to the pixel) */
    key[n + 8] = 0;
    uw = unit[0] ? text_w(&AF_X, unit) + 2 : 0;
    if (text_w(vf, val) + uw > room)
        vf = &AF_S;
    if (text_w(vf, val) + uw > room)                    /* ("HARM MIN"): 9 px before it is cut */
        vf = &AF_X;
    if (uw && text_w(vf, val) + uw > room)              /* ("369 /439"): the value keeps its digits, the unit goes */
        unit = "", uw = 0;
    sig = (uint8_t)str_hash(str_hash(song.sel + TSEL->eng_req * 4u + ux.gen * 64u, label), unit);
    snap = ui.force || sig != ui.roll[c].sig;           /* what the value is of: label, unit, track, engine, palette */
    strip = !snap && str_eq(key, ui.col[c]);
    if (strip && !ui.roll[c].from[0])
        return;
    if (col_style != CS_CARD) {                         /* the redesign's pages (ui_pages.c): the cell again, rolling */
        if (!strip) {                                   /* (or not: strips' and chips' values snap) */
            char ov[16];
            col_old_value(c, ov);
            ui.roll[c].sig = sig;
            if (!str_eq(ov, val))
                roll_note(c, ov, val, snap || named || vf != &AF_M || kid >= 0 || col_style == CS_STRIP ||
                          col_style == CS_CHIP);
            else if (snap)
                ui.roll[c].from[0] = 0;
            str_cpy(ui.col[c], key, sizeof ui.col[c]);
        }
        pv_live = (int16_t)live;
        pv_column(c, label, val, unit, vc, ratio, hot);
        pv_live = -1;
        return;
    }
    roll_bg = cbg;
    if (strip) {                                        /* rolling: the value strip only */
        cv_begin(COL_W, ROLL_H, cbg);
        if (hot) {                                      /* (the hot card's outline crosses the strip) */
            cv_rect(0, 0, 1, ROLL_H, T_THEME);
            cv_rect(COL_W - 1, 0, 1, ROLL_H, T_THEME);
        }
        cv_oy = -ROLL_Y;
    } else {
        char ov[16];                                    /* the value drawn before (in the cache key) */
        col_old_value(c, ov);
        ui.roll[c].sig = sig;
        if (!str_eq(ov, val))
            roll_note(c, ov, val, snap || named || vf != &AF_M || kid >= 0);
        else if (snap)
            ui.roll[c].from[0] = 0;
        str_cpy(ui.col[c], key, sizeof ui.col[c]);
        cv_begin(COL_W, COL_H, T_BG);
        if (hot) {                                      /* the knob just turned: tinted, outlined */
            cv_rrect(0, 0, COL_W, COL_H, 6, T_THEME, T_BG);
            cv_rrect(1, 1, COL_W - 2, COL_H - 2, 5, cbg, T_THEME);
        } else {
            cv_rrect(0, 0, COL_W, COL_H, 6, T_SURF, T_BG);
        }
    }
    if (label[0] || val[0]) {
        (void)icon;                                     /* (the design draws no card icons) */
        if (!strip && label[0])                         /* the label: 9 px capitals in the track's colour, baseline 12 */
            cv_text_fit(lx, 3, &AF_X, label, lc, cbg, COL_W - 2 - lx);
        if (kid >= 0)                                   /* "[OCT+]": the keycap (accent: it would act; DIM: it would not) */
            x = cv_keycap(6, 17, (uint32_t)kid, vc == T_ACCENT || vc == T_DIM ? vc : T_KEY, T_INK, cbg);
        else if (vf == &AF_M)                           /* the value: 15 px, baseline 30 */
            x = roll_text(c, 6, 15, val, vc);
        else
            x = cv_text_fit(6, vf == &AF_S ? 19 : 21, vf, val, vc, cbg, room - uw);
        if (unit[0])                                    /* the unit: 9 px, mid, on the value's baseline */
            cv_text_on(x + 2, 21, &AF_X, unit, T_MID, cbg);
        if (!strip && ratio >= 0) {                     /* the gauge: 42 x 3 at y 36 */
            int32_t gw = COL_W - 12, fx = ratio * gw / 1000;
            cv_rrect(6, 36, gw, 3, 1, T_LINE, cbg);
            cv_rrect(6, 36, fx < 3 ? 3 : fx, 3, 1, vc == T_DIM ? T_DIM : T_THEME, T_LINE);
            if (live >= 0) {                            /* the mods: from the set value to where they move it, a mark */
                int32_t lx = live * gw / 1000;
                if (lx != fx)
                    cv_rect(6 + (lx < fx ? lx : fx), 36, lx < fx ? fx - lx : lx - fx, 3, ux_mix(T_LINE, T_TEXT, 60));
                cv_rect(5 + (lx < 1 ? 1 : lx > gw - 1 ? gw - 1 : lx), 33, 2, 9, T_TEXT);
            }
        }
    }
    cv_oy = 0;
    if (stage_drop_rows && !strip)                      /* (rows r0 .. under the header: Y_LABEL - r0 wraps, + r0 back) */
        cv_blit_from((uint32_t)CARD_X(c), Y_LABEL - (uint32_t)(COL_H - stage_drop_rows), COL_H - stage_drop_rows);
    else
        cv_blit((uint32_t)CARD_X(c), Y_LABEL + (strip ? ROLL_Y : 0));
}

/* an action's column (act_cols): picked, the OCT+ keycap (accent while it would do something); else "--" */
static void draw_act_column(uint32_t c, const char *label, uint16_t vc, uint32_t icon)   /* icon: ICON_AUTO = by label */
{
    int picked = act_col() == c + 1u;
    draw_column(c, label, picked ? "[OCT+]" : "--", "", picked ? (act_ready() ? T_ACCENT : T_DIM) : vc, -1, icon);
}

/* action pages: the footer's first row says what OCT+ and OCT- do ("OCT+ LOAD   OCT- BACK") */
static void foot_hint(char *a, char *b)
{
    uint32_t c = act_col();
    str_cpy(a, "OCT+ ", 8);
    str_cpy(a + 5, c ? act_name(c - 1u) : "--", 8);
    str_cpy(b, ui.act ? "OCT- CANCEL" : "OCT- BACK", 16);
}

/* the sound's name on the track: a user preset or the engine's preset (b holds 16) */
static void sound_name(const track_t *t, char *b)
{
    const engine_t *e = ENGINES[t->eng_req % NENGINES];
    b[0] = 0;
    if(t->user_native && user_of(t)<USER_NONE)native_name(t->eng_req,user_of(t),b);
    else if (user_of(t) < UP_SLOTS)
        up_name(user_of(t), b);
    else if (e->npresets)
        str_cpy(b, eng_preset(e, t->preset % e->npresets)->name, 16);
}

#if MELODEE_UART
#define MIDI_TRS_STATE "ON"                               /* (the input is on; no cable detection) */
#else
#define MIDI_TRS_STATE "OFF"
#endif
/* SYSTEM's MIDI column: input trs (0 USB, 1 TRS) received something in the last 250 ms (both inputs always play;
 * KNOB 1 only picks the one shown). Sampled whenever the column draws */
static int midi_rx_recent(uint32_t trs)
{
    static uint32_t last[2], at[2];
    static uint8_t seen[2];
#if MELODEE_UART
    uint32_t n = trs ? um.bytes : usb.rx_pkts;
#else
    uint32_t n = trs ? 0u : usb.rx_pkts;
#endif
    if (n != last[trs & 1u]) {
        last[trs & 1u] = n;
        at[trs & 1u] = fm1_ms;
        seen[trs & 1u] = 1;
    }
    return seen[trs & 1u] && fm1_ms - at[trs & 1u] < 250u;
}

static void draw_foot(void)
{
    char s[48], pn[16], ti[20];
    const track_t *t = TSEL;
    uint32_t sig;
    const page_t *pg = cur_page();
    const engine_t *e = ENGINES[TSEL->eng_req % NENGINES];
    const char *ename = e->name;
    int32_t x;
    sound_name(t, pn);
    if (browse_pending()) {                            /* browsing: the sound the list shows, not yet loaded */
        uint32_t k, src = browse_shown(&k);
        char tag[6];
        entry_label(src, k, tag, pn);
        e = ENGINES[src_engine(src, k)];
        ename = e->name;
    }
    if (ui.home) {
        str_cpy(ti, "HOME", sizeof ti);
    } else {                                           /* page title + number in its family: "ENV DEST 2/2" */
        uint32_t i, n = 0, k = 0;
        const char *pt = pg->scope == SC_ENGINE ? e->page_title[pg->id[0] != P_E0] : 0;   /* EDIT: the engine's */
        for (i = 0; i < NPAGES; i++)
            if (PAGES[i].fam == pg->fam && page_visible(i)) {
                n++;
                if (i == ui.page)
                    k = n;
            }
        str_cpy(ti, pt ? pt : grid_on() && pg->scope!=SC_DRUMHIT ? "GRID" : pg->title, 12);
        if (pg->scope == SC_FMOP) {                    /* FM6's operator pages: "OP3 FREQ" (PRESETS picks it) */
            ti[0] = 'O'; ti[1] = 'P'; ti[2] = (char)('1' + fm6_opsel % 6u); ti[3] = ' ';
            str_cpy(ti + 4, pg->title, 9);
        }
        if (n > 1 && !foot_tabs_on()) {              /* (the tabs: the page's place, the header its name) */
            str_cpy(ti + str_len(ti), " ", 4);
            fmt_int(ti + str_len(ti), (int32_t)k);
            str_cpy(ti + str_len(ti), "/", 4);
            fmt_int(ti + str_len(ti), (int32_t)n);
        }
        if (pg->graph == GR_ROLL && !drum_track(t)) {  /* the piano roll: its tinted rows' scale ("C MIN") */
            str_cpy(ti, N_NOTE[(uint32_t)t->p[P_ROOT] % 12u], 4);
            str_cpy(ti + str_len(ti), " ", 4);
            str_cpy(ti + str_len(ti), N_SCALE[clamp(t->p[P_SCALE], 0, (int32_t)(sizeof N_SCALE / sizeof N_SCALE[0]) - 1)], 8);
        }
    }
    str_cpy(s, ename, sizeof s);
    s[str_len(s) + 1u] = 0;
    s[str_len(s)] = (char)('1' + song.sel);
    str_cpy(s + str_len(s), pn, 16);
    str_cpy(s + str_len(s), ti, sizeof ti);
    {   /* step markers: the playhead only when it is in the shown bank, the cursor only in SEQ */
        uint32_t ph = song.playing && t->seq_idx / 16u == ui.bank ? t->seq_idx : 0xFFu;
        sig = str_hash(0x9E3779B9u, s) + ph * 97u + (song.seq_mode ? ui.cursor : 0xFFu) * 3001u + steps_hash(t) +
              (ui.home ? 0u : page_icon(pg)) * 7121u +
              ui.bank * 7u + (uint32_t)t->p[P_SLEN] * 13u + (ui.home ? 0u : ui.page * 2246822519u);
    }
    if (act_cols()) {                                  /* the hint, and whether OCT+ would act */
        char ha[16], hb[16];
        foot_hint(ha, hb);
        sig += str_hash(str_hash(act_ready() ? 7u : 3u, ha), hb);
    }
    if (grid_on())
        sig += 0x51EDu + (uint32_t)black_held(GK_ACC) * 977u;
    if (pg->graph == GR_ROLL) sig += (uint32_t)seq_erase_active(t) * 8191u + (uint32_t)live_rec_sel() * 113u + recording_generation * 7919u + ui.note_pick * 40503u + ui.note_zoom * 937u + ui.step_mods * 613u + bulk.on * 521u + ui.note_move * 197u + ui.note_copy * 317u;
    if (!ui.force && sig == ui.foot_sig)
        return;
    ui.foot_sig = sig;
    cv_begin(240, H_FOOT, T_BG);
    if (act_cols()) {                                 /* row 1: the OCT+ / OCT- hint in place of the steps */
        char ha[16], hb[16];
        khint_t kh[3];
        foot_hint(ha, hb);                            /* "OCT+ LOAD", "OCT- BACK": keycaps and their words */
        kh[0] = (khint_t){KC_OCTUP, ha + 5};
        kh[1] = (khint_t){KC_OCTDN, hb + 5};
        cv_key_row(8, 232, 2, kh, 2, act_ready() ? 3u : 2u, T_BG);
    } else if (pg->graph == GR_ROLL && !ui.home && !grid_on()) {
        char detail[24];
        str_cpy(detail, (ui.step_mods & (1u << panel.btn[B_ENV])) ? "ENV LENGTH / SLIDE" : ui.note_copy?"Copying..":bulk.on||ui.note_move?"Moving..":"K1 NOTE  PRE ZOOM", sizeof detail);
        cv_text_on(8, 2, &AF_S, detail, T_THEME, T_BG);
        fmt_int(detail, (int32_t)notes_span());
        str_cpy(detail + str_len(detail), " ST", 4);
        cv_text_r(232, 2, &AF_S, detail, T_MID, T_BG);
    } else if (grid_on()) {                           /* row 1: the page, and what the keys do */
        char b[16];
        uint32_t len = (uint32_t)t->p[P_SLEN];
        str_cpy(b, "PAGE ", sizeof b);
        fmt_int(b + 5, (int32_t)ui.bank + 1);
        str_cpy(b + str_len(b), "/", 4);
        fmt_int(b + str_len(b), (int32_t)((len + 15u) / 16u));
        cv_text(8, 2, &AF_S, b, T_THEME);
        if (black_held(GK_ACC))
            cv_text_r(232, 2, &AF_S, "ACCENT", T_ACCENT, T_BG);
        else if(pg->scope==SC_DRUMHIT)
            cv_text_r(232,2,&AF_S,"K1 HIT / KEYS STEP",T_MID,T_BG);
        else
            cv_key_hint(232 - kh_w(KC_KEYS, "STEPS"), 2, KC_KEYS, "STEPS", 1, T_BG);   /* the keys are the steps */
    } else {
        uint32_t i;
        for (i = 0; i < 16u; i++) {                   /* row 1: the cursor's bank, 16 dots in 4 groups */
            uint32_t si = ui.bank * 16u + i;
            int32_t sx = 10 + (int32_t)i * 13 + (int32_t)(i / 4u) * 4;
            const step_t *st = &seq_steps(t)[si];
            if (si >= (uint32_t)t->p[P_SLEN])
                continue;
            if (step_on(st))                          /* a note: a dot (accented: the accent) */
                cv_rrect(sx + 1, 2, 7, 7, 3, (st->flags & SF_ACCENT) ? T_ACCENT : T_THEME, T_BG);
            else if (st->time == ST_TIE)              /* a tie: a dash from the note before */
                cv_rrect(sx - 2, 4, 11, 3, 1, T_MID, T_BG);
            else                                      /* empty: a small dot */
                cv_rrect(sx + 3, 4, 3, 3, 1, T_RAISE, T_BG);
            if (song.seq_mode && si == ui.cursor)
                cv_rrect(sx + 2, 12, 5, 3, 1, T_ACCENT, T_BG);   /* the step edited */
            else if (song.playing && si == t->seq_idx)
                cv_rrect(sx + 2, 12, 5, 3, 1, T_TEXT, T_BG);     /* the step sounding */
        }
    }
    if (!ui.home && pg->graph == GR_ROLL && live_rec_sel()) {
        cv_key_hint(8, 19, KC_EDIT, seq_erase_active(t) ? "RELEASE TO STOP" : "HOLD TO ERASE", 1, T_BG);
        cv_blit(0, Y_FOOT);
        return;
    }
    if (!ui.home && pg->graph == GR_ROLL && !grid_on()) {
        uint32_t start = notes_manual_start(t);
        int selected = notes_selected(t) < RECORD_MAX || (start < NSTEP && t->step[start].n);
        int moving=bulk.on||ui.note_move||ui.note_copy;
        khint_t hints[2] = {{selected||moving ? KC_OCTUP : KC_KEYS, selected||moving ? "NOTE MENU" : "ADD"}, {moving?KC_OCTDN:KC_EDIT, moving?"DONE":"DELETE"}};
        cv_key_row(8, 232, 19, hints, 2, 3u, T_BG);
        cv_blit(0, Y_FOOT);
        return;
    }
    cv_icon_mid(6, 25, 16, trk_icon(song.sel, 1), T_THEME, T_BG);   /* row 2: the track's numeral, engine icon + */
    x = 26;                                           /* name, sound, page */
    if (MELODEE_ICONS)
        x += cv_icon_on(x, 20, 12, engine_icon(ename), T_MID, T_BG) + 5;
    x = cv_text_fit(x, 19, &AF_S, ename, T_THEME, T_BG, 80);
    {   /* the page title at the right, its icon before it (MIXER, SONG, MOTION); the page tabs: they, the title in
         * the header's chip */
        uint32_t pi = ui.home ? ICON_NONE : page_icon(pg), pn1, pk1;
        int32_t tx;
        if (!ui.home && foot_tabs_on() && (page_place(&pn1, &pk1), pn1 > 1u)) {
            tx = page_tabs(x + 10 + 64, 232, 20);
            cv_free_text(x + 10, 19, &AF_S, pn, T_TEXT, T_BG, tx - 6 - (x + 10));
            cv_blit(0, Y_FOOT);
            return;
        }
        tx = 232 - text_w(&AF_S, ti) - (pi != ICON_NONE ? 16 : 0);
        cv_free_text(x + 10, 19, &AF_S, pn, T_TEXT, T_BG, tx - 12 - (x + 10));
        if (pi != ICON_NONE) cv_icon_on(tx, 20, 12, pi, T_MID, T_BG);
        cv_text_r(232, 19, &AF_S, ti, T_MID, T_BG);
    }
    cv_blit(0, Y_FOOT);
}
#include "ui_sections.c"                                /* the EDIT pages in sections, the map */
#include "ui_stage.c"                                   /* Stage (HOME) */
#include "ui_browser.c"                                 /* SAVE > PRESETS, the sound browser */
#include "ui_patterns.c"                                /* SEQ > PATTERNS, SEQ > SONG */
static void draw_columns(void)
{
    if(cur_page()->scope==SC_DRUM){
        char v[12];uint32_t sound=ui.drum_sound%drum_sound_count(TSEL);const int8_t *c=drum_patch[song.sel].c[sound];
        draw_column(0,"DRUM",drum_sound_name(TSEL,sound),"",VAL(0u),-1,ICON_AUTO);
        if(cur_page()->id[1]==4){fmt_int(v,127-c[3]);draw_column(1,"LEVEL",v,"",VAL(1u),-1,ICON_AUTO);draw_column(2,"","","",T_THEME,-1,ICON_NONE);draw_column(3,"","","",T_THEME,-1,ICON_NONE);return;}
        fmt_int(v,c[0]);draw_column(1,"TUNE",v,"ST",VAL(1u),-1,ICON_PITCH);
        fmt_int(v,c[1]+64);draw_column(2,"DECAY",v,"",VAL(2u),-1,ICON_GATE);
        fmt_int(v,c[2]+64);draw_column(3,(drum_is909(TSEL)?drum_character(sound):drum_issid(TSEL)?"SNAP":sound==D9_SD?"SNAPPY":"TONE"),v,"",VAL(3u),-1,ICON_AUTO);return;
    }
    if(cur_page()->scope==SC_DRUMHIT){
        char v[12];uint32_t i=drum_hit_selected();const recorded_note_t *r=i<RECORD_MAX?&recording[i]:0;
        fmt_int(v,ui.cursor+1);draw_column(0,"STEP",v,"",VAL(0u),-1,ICON_AUTO);
        draw_column(1,"DRUM",drum_sound_name(TSEL,ui.drum_sound%drum_sound_count(TSEL)),"",VAL(1u),-1,ICON_AUTO);
        fmt_int(v,r?r->pitch:0);draw_column(2,"PITCH",v,"ST",VAL(2u),-1,ICON_PITCH);
        if(r && r->length){uint32_t x=((uint32_t)r->duration*(1u<<(r->owner>>5))*100u+RECORD_UNIT/2)/RECORD_UNIT;fmt_int(v,x/100);str_cpy(v+str_len(v),".",2);if(x%100<10)str_cpy(v+str_len(v),"0",2);fmt_int(v+str_len(v),x%100);}
        else str_cpy(v,"FREE",sizeof v);
        draw_column(3,"LENGTH",v,"",VAL(3u),-1,ICON_GATE);return;
    }

    uint32_t c;
    char val[12];
    const char *unit;
    if (ui.home)                                        /* Stage: its knobs drop in when turned (ui_stage.c stage_knobs) */
        return;
    if (cur_page()->scope == SC_TRK) {                 /* LEVEL PAN MUTE of the selected track */
        const track_t *t = TSEL;
        uint32_t lvl = trk_level(song.sel);
        param_format(&TP[P_LEVEL], (int32_t)lvl, val, &unit);   /* (0: OFF) */
        col_mods(PID_TRACK(P_LEVEL));
        draw_column(0, "LEVEL", val, unit, lvl && !t->p[P_MUTE] ? VAL(0u) : T_DIM, (int32_t)lvl * 1000 / 127, ICON_AUTO);
        param_format(&TP[P_PAN], t->p[P_PAN], val, &unit);
        col_mods(PID_TRACK(P_PAN));
        draw_column(1, "PAN", val, unit, VAL(1u), RATIO(&TP[P_PAN], t->p[P_PAN]), param_icon(&TP[P_PAN], t->p[P_PAN]));
        draw_column(2, "", "", "", T_DIM, -1, ICON_NONE);
        draw_column(3, "MUTE", t->p[P_MUTE] ? "ON" : "OFF", "", t->p[P_MUTE] ? T_ACCENT : VAL(3u), -1, ICON_AUTO);
        return;
    }
    if (cur_page()->graph == GR_SCALE_PICKER) {
        uint32_t scale = (uint32_t)clamp(TSEL->p[P_SCALE], 0, SCALE_TOTAL - 1u);
        draw_column(0, "FAMILY", SCALE_FAMILY_SHORT[ui.scale_family], "", VAL(0u), -1, ICON_X_FOLDER);
        draw_column(1, "SCALE", N_SCALE[scale], "", VAL(1u), -1, ICON_NONE);
        param_format(&TP[P_ROOT], TSEL->p[P_ROOT], val, &unit);
        draw_column(2, "ROOT", val, unit, VAL(2u), -1, ICON_AUTO);
        param_format(&TP[P_QUANT], TSEL->p[P_QUANT], val, &unit);
        draw_column(3, "QNT", val, unit, VAL(3u), -1, ICON_AUTO);
        return;
    }
#if MELODEE_SLICE
    if (cur_page()->graph == GR_SLICES) {                /* SLICE POS, then SPLIT JOIN (ui_slice.c) */
        uint32_t n = slice_count(), j = slice_sel(), src, div, ok = slice_src(&src, &div) && src;
        char u[8];
        if (j < n) {
            fmt_int(val, (int32_t)j + 1);
            str_cpy(u, "/", 8);
            fmt_int(u + 1, (int32_t)n);
        } else {
            str_cpy(val, n ? "END" : "--", sizeof val);
            u[0] = 0;
        }
        draw_column(0, "SLICE", val, u, VAL(0u), -1, ICON_SLICE);
        slice_time(val, slice_mark(j));
        draw_column(1, "POS", val, "S", ok ? VAL(1u) : T_DIM, -1, ICON_AUTO);
        draw_act_column(2, "SPLIT", ok ? T_THEME : T_DIM, ICON_AUTO);
        draw_act_column(3, "JOIN", ok ? T_THEME : T_DIM, ICON_AUTO);
        return;
    }
#endif
    if (cur_page()->graph == GR_FORM) {
        uint32_t tr = song.sel;
        const tessera_evolve_t *ev = &tessera.evolve[tr < TESSERA_TRACKS ? tr : 0u];
        char f2[16];
        fmt_int(f2, ev->contrast);
        draw_column(0, "FORM", tessera_form_name(ev->form), "", VAL(0u), (int32_t)ev->form * 1000 / 6, ICON_AUTO);
        col_mods(PID_TESS(TX_FM_CONTRAST));
        draw_column(1, "CONTR", f2, "%", VAL(1u), (int32_t)ev->contrast * 10, ICON_AUTO);
        draw_column(2, "CLEAR", "TURN", "", VAL(2u), -1, ICON_AUTO);
        {
            char w3[16];
            int32_t fl = ev->flourish;
            if (!fl) str_cpy(w3, "OFF", sizeof w3);
            else { w3[0] = fl < 0 ? '-' : '+'; fmt_int(w3 + 1, (uint32_t)(fl < 0 ? -fl : fl)); }
            draw_column(3, "FLRSH", w3, fl ? "st" : "", VAL(3u), (fl + 12) * 1000 / 24, ICON_AUTO);
        }
        return;
    }
    if (cur_page()->graph == GR_TWEAK) {
        uint32_t tr = song.sel;
        const tessera_evolve_t *ev = &tessera.evolve[tr < TESSERA_TRACKS ? tr : 0u];
        char w0[16], w1[16], w2[16], w3[8];
        int32_t sh = ev->shift;
        w0[0] = sh < 0 ? '-' : '+';
        fmt_int(w0 + 1, (uint32_t)(sh < 0 ? -sh : sh));
        fmt_int(w1, ev->drop);
        fmt_int(w2, ev->human);
        if (ev->follow) { w3[0] = 'T'; w3[1] = (char)('0' + ev->follow); w3[2] = 0; }
        else str_cpy(w3, "OFF", sizeof w3);
        col_mods(PID_TESS(TX_TW_SHIFT));
        draw_column(0, "SHIFT", w0, ev->follow ? "deg" : "st", VAL(0u), (sh + 24) * 1000 / 48, ICON_AUTO);   /* (following: scale degrees) */
        col_mods(PID_TESS(TX_TW_DROP));
        draw_column(1, "DROP", w1, "%", VAL(1u), (int32_t)ev->drop * 10, ICON_AUTO);
        col_mods(PID_TESS(TX_TW_HUMAN));
        draw_column(2, "HUMAN", w2, "%", VAL(2u), (int32_t)ev->human * 10, ICON_AUTO);
        draw_column(3, "FOLLOW", w3, "", VAL(3u), (int32_t)ev->follow * 1000 / 4, ICON_AUTO);
        return;
    }
    if (cur_page()->graph == GR_ORGANIC) {
        uint32_t tr = song.sel < TESSERA_TRACKS ? song.sel : 0u;
        char w0[8], w1[8];
        fmt_int(w0, tessera.org_speed[tr]);
        fmt_int(w1, tessera.org_rate[tr]);
        draw_column(0, "SPEED", w0, "", VAL(0u), (int32_t)tessera.org_speed[tr] * 10, ICON_AUTO);   /* SLOTH's */
        draw_column(1, "RATE", w1, "", VAL(1u), (int32_t)tessera.org_rate[tr] * 10, ICON_AUTO);    /* OCHD's */
        draw_column(2, "", "", "", T_DIM, -1, ICON_NONE);
        draw_column(3, "", "", "", T_DIM, -1, ICON_NONE);
        return;
    }
    if (cur_page()->graph == GR_ENERGY) {                /* ENERGY (energy.c), the project's: ARC, RESTS, the tide now, a rest */
        char w0[8], w1[8], w2[8], w3[8];
        int32_t now = energy_now() * 100 / 32767;
        uint32_t left = energy.rest_left / EN_BPS;
        fmt_int(w0, tessera.en_arc);
        fmt_int(w1, tessera.en_rests);
        fmt_int(w2, now);
        fmt_int(w3, (int32_t)left + 1);
        draw_column(0, "ARC", w0, "%", VAL(0u), (int32_t)tessera.en_arc * 10, ICON_AUTO);
        draw_column(1, "RESTS", w1, "%", VAL(1u), (int32_t)tessera.en_rests * 10, ICON_AUTO);
        draw_column(2, "NOW", tessera.en_arc ? w2 : "--", tessera.en_arc ? "%" : "", T_DIM, -1, ICON_NONE);
        draw_column(3, "REST", song.playing && energy_resting() ? w3 : "--", song.playing && energy_resting() ? "s" : "",
                    T_DIM, -1, ICON_NONE);
        return;
    }
    if (cur_page()->graph == GR_TIMING) {                /* SUB (seq_sub.c): RATCH, its CHANCE (dim at x1), SHAPE */
        uint32_t tr = song.sel % TESSERA_TRACKS, r = tessera.sub_ratch[tr], ch = tessera.sub_chance[tr];
        uint32_t sh = tessera.sub_shape[tr] % 3u, step_trk = tessera.mode[tr] == TESS_ENG_OFF;
        static const char *const SHAPE[3] = {"EVEN", "ACCEL", "DECEL"};
        static const char *const RATCH[8] = {"x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8"};
        char w1[8];
        fmt_int(w1, ch);
        if (step_trk) {                                   /* (a STEP track: RATCH a step's, in its sheet) */
            draw_column(0, "RATCH", "STEP", "", T_DIM, -1, ICON_NONE);
            draw_column(1, "", "", "", T_DIM, -1, ICON_NONE);
            draw_column(2, "SHAPE", SHAPE[sh], "", VAL(2u), (int32_t)sh * 500, ICON_AUTO);
            draw_column(3, "", "", "", T_DIM, -1, ICON_NONE);
            return;
        }
        draw_column(0, "RATCH", RATCH[r & 7u], "", VAL(0u), (int32_t)(r & 7u) * 1000 / 7, ICON_AUTO);
        draw_column(1, "CHANCE", w1, "%", r ? VAL(1u) : T_DIM, (int32_t)ch * 10, ICON_AUTO);
        draw_column(2, "SHAPE", SHAPE[sh], "", r ? VAL(2u) : T_DIM, (int32_t)sh * 500, ICON_AUTO);
        draw_column(3, "", "", "", T_DIM, -1, ICON_NONE);
        return;
    }
    if (cur_page()->graph == GR_NOTELEN) {               /* NOTE LENGTH (note_len.c); LEN and KEYS FIXED's alone */
        uint32_t tr = song.sel % TESSERA_TRACKS, m = tessera.nl_mode[tr], fixed = m == NL_FIXED;
        static const char *const MODE[NL_COUNT] = {"STEP", "FIXED", "HIT"};
        static const char *const LEN[6] = {"1/32", "1/16", "1/8", "1/4", "1/2", "1/1"};
        char k2[8];
        int16_t *vp;
        const param_desc_t *d = page_desc(cur_page(), 3u, &vp);
        fmt_int(k2, tessera.nl_keys[tr]);
        draw_column(0, "LENGTH", MODE[m % NL_COUNT], "", VAL(0u), (int32_t)m * 1000 / (NL_COUNT - 1), ICON_AUTO);
        draw_column(1, "LEN", fixed ? LEN[tessera.nl_len[tr] % 6u] : "--", "", fixed ? VAL(1u) : T_DIM,
                    fixed ? (int32_t)tessera.nl_len[tr] * 1000 / 5 : -1, ICON_AUTO);
        draw_column(2, "KEYS", fixed ? k2 : "--", "", fixed ? VAL(2u) : T_DIM,
                    fixed ? ((int32_t)tessera.nl_keys[tr] - 1) * 1000 / 3 : -1, ICON_AUTO);
        if (d && vp) {                                   /* STEP's GATE (PATTERN's): dim while another mode holds */
            param_format(d, *vp, val, &unit);
            draw_column(3, d->label, val, unit, m == NL_STEP ? VAL(3u) : T_DIM, RATIO(d, *vp), param_icon(d, *vp));
        }
        return;
    }
    if (cur_page()->graph == GR_TWEAK2) {
        static const char *const BRM[TESS_BR_COUNT] = {"COIN", "TGL", "LTCH"};
        static const char *const LGN[TESS_LG_COUNT] = {"OFF", "AND", "OR", "XOR", "NOT"};
        uint32_t tr = song.sel;
        const tessera_evolve_t *ev = &tessera.evolve[tr < TESSERA_TRACKS ? tr : 0u];
        char w0[16], w1[8], w3[8];
        int on = ev->br_to && ev->br_to != tr + 1u, lg = ev->logic && ev->lg_with && ev->lg_with != tr + 1u;
        fmt_int(w0, ev->branch);
        if (on) { w1[0] = 'T'; w1[1] = (char)('0' + ev->br_to); w1[2] = 0; }
        else str_cpy(w1, "OFF", sizeof w1);
        if (lg) { w3[0] = 'T'; w3[1] = (char)('0' + ev->lg_with); w3[2] = 0; }
        else w3[0] = 0;
        col_mods(PID_TESS(TX_TW_BRANCH));
        draw_column(0, "BRANCH", w0, "%", on ? VAL(0u) : T_DIM, (int32_t)ev->branch * 10, ICON_AUTO);   /* (no TO: idle) */
        draw_column(1, "TO", w1, on ? BRM[ev->br_mode % TESS_BR_COUNT] : "", VAL(1u),
                    on ? (int32_t)(1u + (ev->br_to - 1u - (ev->br_to - 1u > tr)) * 3u + ev->br_mode) * 1000 / 9 : 0, ICON_AUTO);
        draw_column(2, "LOGIC", LGN[lg ? ev->logic % TESS_LG_COUNT : 0u], w3, VAL(2u), lg ? (int32_t)ev->logic * 1000 / 4 : 0,
                    ICON_AUTO);
        draw_column(3, "COND", tessera_cond_name(ev->cond), "", VAL(3u), (int32_t)ev->cond * 1000 / (TESS_COND_COUNT - 1),
                    ICON_AUTO);
        return;
    }
    if (cur_page()->graph == GR_EVOLVE) {
        uint32_t tr = song.sel;
        const tessera_evolve_t *ev = &tessera.evolve[tr < TESSERA_TRACKS ? tr : 0u];
        char e0[16], e1[16], e2[16], e3[16], e0u[8] = "", e1u[8] = "", e2u[8] = "%";
        int lk = tessera_lock_get(tr);
        if (tessera_step_on(tr)) {                      /* STEP EVOLVE: LOCK, EVERY, AMOUNT, CLEAR (the steps changed) */
            uint32_t every = tessera.sv_every[tr], amount = tessera.sv_amount[tr], n = tessera_step_varied(tr);
            fmt_int(e0, (uint32_t)lk);
            if (!every) str_cpy(e1, "OFF", sizeof e1);
            else { fmt_int(e1, tessera_every_bars(every)); str_cpy(e1u, "BAR", sizeof e1u); }
            if (!amount) { str_cpy(e2, "OFF", sizeof e2); e2u[0] = 0; }
            else fmt_int(e2, amount);
            if (n) fmt_int(e3, n);
            else str_cpy(e3, "--", sizeof e3);
            draw_column(0, "LOCK", e0, "%", VAL(0u), lk * 10, ICON_AUTO);
            draw_column(1, "EVERY", e1, e1u, VAL(1u), (int32_t)every * 1000 / 5, ICON_AUTO);
            draw_column(2, "AMOUNT", e2, e2u, VAL(2u), (int32_t)amount * 10, ICON_AUTO);
            draw_column(3, "CLEAR", e3, n ? (n == 1u ? "STEP" : "STEPS") : "", n ? VAL(3u) : T_DIM, -1, ICON_AUTO);
            return;
        }
        if (lk < 0) str_cpy(e0, "--", sizeof e0);
        else { fmt_int(e0, (uint32_t)lk); str_cpy(e0u, "%", sizeof e0u); }
        if (!ev->every) str_cpy(e1, "OFF", sizeof e1);
        else { fmt_int(e1, tessera_every_bars(ev->every)); str_cpy(e1u, "BAR", sizeof e1u); }
        fmt_int(e2, ev->amount);
        draw_column(0, "LOCK", e0, e0u, VAL(0u), lk < 0 ? -1 : lk * 10, ICON_AUTO);
        draw_column(1, "EVERY", e1, e1u, VAL(1u), (int32_t)ev->every * 1000 / 5, ICON_AUTO);
        col_mods(PID_TESS(TX_EV_AMOUNT));
        draw_column(2, "AMOUNT", e2, e2u, VAL(2u), (int32_t)ev->amount * 10, ICON_AUTO);
        if (tr >= TESSERA_TRACKS || tessera.mode[tr] == TESS_ENG_OFF || !tessera.seed[tr]) str_cpy(e3, "--", sizeof e3);
        else if (tessera.roll[tr] && ((fm1_ms / 250u) & 1u)) e3[0] = 0;   /* a seed waiting for the bar line: blinking */
        else if (tessera.roll[tr]) fmt_int(e3, tessera.roll[tr]);
        else fmt_int(e3, tessera.seed[tr]);           /* (picked when a generator is first selected or played) */
        col_quiet = 1;                                  /* ROLL: each step a new line; the seed small, to find one again */
        draw_column(3, "ROLL", e3, "", VAL(3u), -1, ICON_AUTO);
        return;
    }
    if (cur_page()->graph == GR_STEPS && cur_page()->fam == FAM_GEN) {   /* (PATTERN: upstream's columns) */
        uint32_t tr = song.sel;
        if (tr < TESSERA_TRACKS) {
            uint32_t m = tessera.mode[tr];
            char v1[16], v2[16], v3[16], u2[8] = "";
            if (m == TESS_ENG_OFF) {
                draw_column(0, "MODE", seqm_get(tr) ? "ARP" : "STEP", "", VAL(0u), -1, ICON_AUTO);
                for (c = 1u; c < 4u; c++) {
                    int16_t *vp;
                    const param_desc_t *d = page_desc(cur_page(), c, &vp);
                    if (d && d->label && d->label[0] != '-') {
                        param_format(d, *vp, val, &unit);
                        draw_column(c, d->label, val, unit, VAL(c),
                                    d->fmt == F_ENUM && d->max < 2 ? -1 : RATIO(d, *vp),
                                    param_icon(d, *vp));
                    }
                }
                return;
            } else if (m == TESS_ENG_TURING) {
                const gen_turing_t *tur = tessera_turing(tr);
                uint8_t tlen = tur->len ? tur->len : 16;
                draw_column(0, "MODE", "TURING", "", VAL(0u), 1 * 1000 / (TESS_ENG_COUNT - 1), ICON_AUTO);
                fmt_int(v1, tlen);
                col_mods(PID_TESS(TX_TUR_LEN));
                draw_column(1, "LEN", v1, "/16", VAL(1u), (int32_t)(tlen - 1u) * 1000 / 15, ICON_AUTO);
                if (tur->prob == 0) {
                    str_cpy(v2, "LOCK", sizeof v2);
                } else {
                    fmt_int(v2, tur->prob);
                    str_cpy(u2, "%", sizeof u2);
                }
                col_mods(PID_TESS(TX_TUR_PROB));
                draw_column(2, "PROB", v2, u2, VAL(2u), (int32_t)tur->prob * 10, ICON_AUTO);
                fmt_int(v3, tur->oct_span);
                draw_column(3, "SPAN", v3, "OCT", VAL(3u), (int32_t)(tur->oct_span > 1 ? tur->oct_span - 1 : 0) * 1000 / 2, ICON_AUTO);
            } else if (m == TESS_ENG_EUCLID) {
                const gen_euclid_t *euc = tessera_euclid(tr);
                uint32_t esteps = euc->steps ? euc->steps : 16;
                draw_column(0, "MODE", "EUCLID", "", VAL(0u), 2 * 1000 / (TESS_ENG_COUNT - 1), ICON_AUTO);
                fmt_int(v1, esteps);
                draw_column(1, "STEPS", v1, "", VAL(1u), (int32_t)(esteps - 1u) * 1000 / 31, ICON_AUTO);
                fmt_int(v2, euc->pulses);
                col_mods(PID_TESS(TX_EUC_PULSES));
                draw_column(2, "PULSE", v2, "", VAL(2u), (int32_t)euc->pulses * 1000 / 32, ICON_AUTO);
                fmt_int(v3, euc->rot);
                col_mods(PID_TESS(TX_EUC_ROT));
                draw_column(3, "ROT", v3, "", VAL(3u), (int32_t)euc->rot * 1000 / 31, ICON_AUTO);
            } else if (m == TESS_ENG_MARKOV) {
                const gen_markov_t *mk = tessera_markov(tr);
                uint32_t moct = mk->oct_span ? mk->oct_span : 1;
                uint32_t dens = (mk->density >= 5 && mk->density <= 100) ? mk->density : 100;
                draw_column(0, "MODE", "MARKOV", "", VAL(0u), 3 * 1000 / (TESS_ENG_COUNT - 1), ICON_AUTO);
                fmt_int(v1, mk->tension);
                col_mods(PID_TESS(TX_MKV_LEAP));
                draw_column(1, "LEAP", v1, "%", VAL(1u), (int32_t)mk->tension * 10, ICON_AUTO);
                fmt_int(v2, dens);
                col_mods(PID_TESS(TX_MKV_DENS));
                draw_column(2, "DENS", v2, "%", VAL(2u), (int32_t)dens * 10, ICON_AUTO);
                fmt_int(v3, moct);
                draw_column(3, "SPAN", v3, "OCT", VAL(3u), (int32_t)(moct - 1u) * 1000 / 2, ICON_AUTO);
            } else if (m == TESS_ENG_BOUNCE) {
                const gen_bounce_t *b = tessera_bounce(tr);
                draw_column(0, "MODE", "BOUNCE", "", VAL(0u), TESS_ENG_BOUNCE * 1000 / (TESS_ENG_COUNT - 1), ICON_AUTO);
                fmt_int(v1, b->speed);
                col_mods(PID_TESS(TX_BNC_SPEED));
                draw_column(1, "SPEED", v1, "", VAL(1u), (int32_t)(b->speed - 1u) * 1000 / 63, ICON_AUTO);
                fmt_int(v2, b->balls == GEN_BOUNCE_FALL ? (int32_t)b->angle - 45 : (int32_t)b->angle);
                col_mods(PID_TESS(TX_BNC_ANGLE));
                if (b->balls == GEN_BOUNCE_FALL)                /* FALL: ANGLE is the WIND, calm in the middle */
                    draw_column(2, "WIND", v2, "", VAL(2u), (int32_t)b->angle * 1000 / 90, ICON_AUTO);
                else
                    draw_column(2, "ANGLE", v2, "DEG", VAL(2u), (int32_t)b->angle * 1000 / 90, ICON_AUTO);
                fmt_int(v3, b->balls);
                draw_column(3, "BALLS", b->balls == GEN_BOUNCE_FALL ? "FALL" : v3, "", VAL(3u),
                            (int32_t)(b->balls - 1u) * 1000 / (GEN_BOUNCE_FALL - 1u), ICON_AUTO);
            } else if (m == TESS_ENG_AWAKE) {
                const gen_awake_t *a = tessera_awake(tr);
                if (a->edit) {                                  /* a step picked (ui_awake.c): KNOB 1 its A, KNOB 2 its B */
                    uint32_t i = a->edit & 15u, va = gen_awake_a(a, i), vb = gen_awake_b(a, i);
                    char l0[8] = {'A', ' ', 0}, l1[8] = {'B', ' ', 0}, v0[8];
                    fmt_int(l0 + 2, i + 1u);
                    fmt_int(l1 + 2, i + 1u);
                    if (va) fmt_int(v0, va);
                    else str_cpy(v0, "REST", sizeof v0);
                    draw_column(0, l0, v0, "", VAL(0u), (int32_t)va * 1000 / 8, ICON_AUTO);
                    v1[0] = '+';
                    fmt_int(v1 + 1, vb);
                    draw_column(1, l1, v1, "", VAL(1u), (int32_t)vb * 1000 / 7, ICON_AUTO);
                } else {
                    draw_column(0, "MODE", "AWAKE", "", VAL(0u), TESS_ENG_AWAKE * 1000 / (TESS_ENG_COUNT - 1), ICON_AUTO);
                    fmt_int(v1, a->len_a);
                    draw_column(1, "LEN A", v1, "", VAL(1u), (int32_t)(a->len_a - 1u) * 1000 / 15, ICON_AUTO);
                }
                fmt_int(v2, a->len_b);
                draw_column(2, "LEN B", v2, "", VAL(2u), (int32_t)(a->len_b - 1u) * 1000 / 15, ICON_AUTO);
                fmt_int(v3, a->prob);
                col_mods(PID_TESS(TX_AWK_PROB));
                draw_column(3, "PROB", v3, "%", VAL(3u), (int32_t)a->prob * 10, ICON_AUTO);
            } else if (m == TESS_ENG_POLY) {
                const gen_poly_t *p = tessera_poly(tr);
                uint32_t d = p->div[p->sel & 3u];
                draw_column(0, "MODE", "POLY", "", VAL(0u), TESS_ENG_POLY * 1000 / (TESS_ENG_COUNT - 1), ICON_AUTO);
                fmt_int(v1, (p->sel & 3u) + 1u);
                draw_column(1, "RHY", v1, "/4", VAL(1u), (int32_t)(p->sel & 3u) * 1000 / 3, ICON_AUTO);
                if (d) v2[0] = '/', fmt_int(v2 + 1, d);    /* (every d steps: "/4", no unit) */
                else str_cpy(v2, "OFF", sizeof v2);
                draw_column(2, "DIV", v2, u2, VAL(2u), (int32_t)d * 1000 / 16, ICON_AUTO);
                fmt_int(v3, p->span);
                draw_column(3, "SPAN", v3, "OCT", VAL(3u), (int32_t)(p->span - 1u) * 500, ICON_AUTO);
            } else if (m == TESS_ENG_QUAD) {
                static const char *const QDIR[GEN_QD_COUNT] = {"FWD", "BACK", "PEND", "RAND", "HOLD"};
                static const char *const QMOVE[GEN_QC_COUNT] = {"STOP", "DOWN", "UP", "RAND"};
                static const char *const QRANGE[GEN_QR_COUNT] = {"3 OCT", "1 OCT", "2 OCT", "+-1"};
                const gen_quad_t *q = tessera_quad(tr);
                if (quad_cols_on()) {                           /* OCT+ held: the columns' and RANGE */
                    uint32_t c = tessera.qcol_sel & 3u, mv = tessera.qcol[c] & 3u, ev = tessera.qcol[c] >> 2;
                    fmt_int(v1, c + 1u);
                    draw_column(0, "COL", v1, "", VAL(0u), (int32_t)c * 1000 / 3, ICON_AUTO);
                    draw_column(1, "MOVE", QMOVE[mv], "", VAL(1u), (int32_t)mv * 1000 / (GEN_QC_COUNT - 1), ICON_AUTO);
                    v2[0] = '/', fmt_int(v2 + 1, tessera_quad_every(c));
                    draw_column(2, "EVERY", v2, "", VAL(2u), (int32_t)ev * 1000 / (GEN_QC_EVERY_N - 1), ICON_AUTO);
                    draw_column(3, "RANGE", QRANGE[q->range % GEN_QR_COUNT], "", VAL(3u),
                                (int32_t)(q->range % GEN_QR_COUNT) * 1000 / (GEN_QR_COUNT - 1), ICON_AUTO);
                    return;
                }
                draw_column(0, "MODE", "QUAD", "", VAL(0u), TESS_ENG_QUAD * 1000 / (TESS_ENG_COUNT - 1), ICON_AUTO);
                draw_column(1, "DIR", QDIR[q->dir % GEN_QD_COUNT], "", VAL(1u), (int32_t)(q->dir % GEN_QD_COUNT) * 1000 / (GEN_QD_COUNT - 1), ICON_AUTO);
                fmt_int(v2, q->prob);
                col_mods(PID_TESS(TX_QD_PROB));
                draw_column(2, "PROB", v2, "%", VAL(2u), (int32_t)q->prob * 10, ICON_AUTO);
                quad_link_text(q->link, v3);
                draw_column(3, "LINK", v3, "", VAL(3u), (int32_t)(q->link & 15u) * 1000 / 15, ICON_AUTO);
            } else if (m == TESS_ENG_SERIES) {
                const gen_series_t *g = tessera_series(tr);
                draw_column(0, "MODE", "SERIES", "", VAL(0u), TESS_ENG_SERIES * 1000 / (TESS_ENG_COUNT - 1), ICON_AUTO);
                fmt_int(v1, g->start);
                col_mods(PID_TESS(TX_SR_START));
                draw_column(1, "START", v1, "", VAL(1u), (int32_t)g->start * 1000 / 127, ICON_AUTO);
                fmt_int(v2, g->intv);
                draw_column(2, "INTV", v2, "", VAL(2u), ((int32_t)g->intv - 1) * 1000 / 6, ICON_AUTO);
                if (g->loop) fmt_int(v3, g->loop);
                else str_cpy(v3, "OFF", sizeof v3);
                draw_column(3, "LOOP", v3, "", VAL(3u), (int32_t)g->loop * 1000 / GEN_SR_LOOP, ICON_AUTO);
            } else if (m == TESS_ENG_CHORD) {
                const gen_chord_t *g = tessera_chord(tr);
                if (ch_layer_on()) {                            /* OCT+ held: OCT, the track's CHRD and VOIC */
                    const param_desc_t *dc = pid_desc(PID_TRACK(P_CHRD), tr), *dv = pid_desc(PID_TRACK(P_VOIC), tr);
                    uint32_t cv = (uint32_t)TSEL->p[P_CHRD], vv = (uint32_t)TSEL->p[P_VOIC];
                    v1[0] = g->oct > 0 ? '+' : g->oct < 0 ? '-' : '0';
                    v1[1] = g->oct ? (char)('0' + (g->oct < 0 ? -g->oct : g->oct)) : 0;
                    v1[2] = 0;
                    draw_column(0, "OCT", v1, "", VAL(0u), ((int32_t)g->oct + 2) * 1000 / 4, ICON_AUTO);
                    col_mods(PID_TRACK(P_CHRD));
                    draw_column(1, "CHRD", cv <= (uint32_t)dc->max ? dc->names[cv] : "?", "", VAL(1u),
                                (int32_t)cv * 1000 / (dc->max ? dc->max : 1), ICON_AUTO);
                    col_mods(PID_TRACK(P_VOIC));
                    draw_column(2, "VOIC", vv <= (uint32_t)dv->max ? dv->names[vv] : "?", "", VAL(2u),
                                (int32_t)vv * 1000 / (dv->max ? dv->max : 1), ICON_AUTO);
                    if (g->prog == GEN_CH_USER) {             /* USER: its LEN (the keys play its chords in) */
                        fmt_int(v3, (int32_t)gen_chord_ulen(g));
                        draw_column(3, "LEN", v3, "", VAL(3u), ((int32_t)gen_chord_ulen(g) - 1) * 1000 / 7, ICON_AUTO);
                    } else {                                  /* the others: BREATH */
                        uint32_t b = tessera.ch_breath[tr];
                        fmt_int(v3, (int32_t)b);
                        col_mods(PID_TESS(TX_CH_BREATH));
                        draw_column(3, "BREATH", b ? v3 : "OFF", b ? "%" : "", VAL(3u), (int32_t)b * 10, ICON_AUTO);
                    }
                    return;
                }
                draw_column(0, "MODE", "CHORD", "", VAL(0u), TESS_ENG_CHORD * 1000 / (TESS_ENG_COUNT - 1), ICON_AUTO);
                col_mods(PID_TESS(TX_CH_PROG));
                draw_column(1, "PROG", pid_desc(PID_TESS(TX_CH_PROG), tr)->names[g->prog % GEN_CH_PROGS], "", VAL(1u),
                            (int32_t)(g->prog % GEN_CH_PROGS) * 1000 / (GEN_CH_PROGS - 1), ICON_AUTO);
                draw_column(2, "BARS", pid_desc(PID_TESS(TX_CH_BARS), tr)->names[g->bars % GEN_CH_BARS], "", VAL(2u),
                            (int32_t)(g->bars % GEN_CH_BARS) * 1000 / (GEN_CH_BARS - 1), ICON_AUTO);
                draw_column(3, "HIT", pid_desc(PID_TESS(TX_CH_HIT), tr)->names[g->hit % GEN_CH_HITS], "", VAL(3u),
                            (int32_t)(g->hit % GEN_CH_HITS) * 1000 / (GEN_CH_HITS - 1), ICON_AUTO);
            } else if (m == TESS_ENG_ANSWER) {
                const tessera_answer_t *a = tessera_answer(tr);
                if (an_layer_on()) {                            /* OCT+ held: ORDER, SHIFT, SPACE, LEN */
                    const param_desc_t *dor = pid_desc(PID_TESS(TX_AN_ORDER), tr), *dl = pid_desc(PID_TESS(TX_AN_LEN), tr);
                    draw_column(0, "ORDER", dor->names[a->order % TESS_AN_ORDERS], "", VAL(0u),
                                (int32_t)(a->order % TESS_AN_ORDERS) * 1000 / (TESS_AN_ORDERS - 1), ICON_AUTO);
                    fmt_int(v1, a->shift);
                    draw_column(1, "SHIFT", a->shift ? v1 : "OFF", "", VAL(1u), (int32_t)a->shift * 1000 / 7, ICON_AUTO);
                    fmt_int(v2, a->space);
                    draw_column(2, "SPACE", v2, "%", VAL(2u), (int32_t)a->space * 10, ICON_AUTO);
                    draw_column(3, "LEN", dl->names[a->len % TESS_AN_LENS], "", VAL(3u),
                                (int32_t)(a->len % TESS_AN_LENS) * 1000 / (TESS_AN_LENS - 1), ICON_AUTO);
                    return;
                }
                draw_column(0, "MODE", "ANSWER", "", VAL(0u), TESS_ENG_ANSWER * 1000 / (TESS_ENG_COUNT - 1), ICON_AUTO);
                fmt_int(v1, a->vary);
                col_mods(PID_TESS(TX_AN_VARY));
                draw_column(1, "VARY", a->vary ? v1 : "OFF", a->vary ? "%" : "", VAL(1u), (int32_t)a->vary * 10, ICON_AUTO);
                fmt_int(v2, a->echo);
                draw_column(2, "ECHO", a->echo ? v2 : "OFF", a->echo ? "%" : "", VAL(2u), (int32_t)a->echo * 10, ICON_AUTO);
                if (tessera_answer_count(tr)) fmt_int(v3, (int32_t)tessera_answer_count(tr));
                else str_cpy(v3, "LOOPS", sizeof v3);
                draw_column(3, "FADE", v3, "", VAL(3u), (int32_t)a->fade * 10, ICON_AUTO);
            } else if (m == TESS_ENG_CONSTEL) {
                const gen_constel_t *c = tessera_constel(tr);
                if (cs_layer_on()) {                            /* OCT+ held: LOCK, LEN, FILL, SIZE */
                    uint32_t lk = tessera.evolve[tr].lock;
                    fmt_int(v1, lk);
                    col_mods(PID_TESS(TX_EV_LOCK));
                    draw_column(0, "LOCK", v1, "%", VAL(0u), (int32_t)lk * 10, ICON_AUTO);
                    fmt_int(v2, c->len);
                    draw_column(1, "LEN", v2, "", VAL(1u), ((int32_t)c->len - 1) * 1000 / 15, ICON_AUTO);
                    draw_column(2, "FILL", c->fill == GEN_CS_FILL_ARP ? "ARP" : "OFF", "", VAL(2u),
                                (int32_t)(c->fill == GEN_CS_FILL_ARP) * 1000, ICON_AUTO);
                    fmt_int(v3, c->size);
                    col_mods(PID_TESS(TX_CS_SIZE));
                    draw_column(3, "SIZE", v3, "", VAL(3u), (int32_t)c->size * 1000 / 7, ICON_AUTO);
                    return;
                }
                draw_column(0, "MODE", "CONSTL", "", VAL(0u), TESS_ENG_CONSTEL * 1000 / (TESS_ENG_COUNT - 1), ICON_AUTO);
                fmt_int(v1, c->y);
                col_mods(PID_TESS(TX_CS_Y));
                draw_column(1, "Y", c->autop ? "AUTO" : v1, "", VAL(1u), (int32_t)c->y * 1000 / (GEN_CS_ROWS - 1u), ICON_AUTO);
                fmt_int(v2, c->dens);
                col_mods(PID_TESS(TX_CS_DENS));
                draw_column(2, "DENS", v2, "%", VAL(2u), (int32_t)c->dens * 10, ICON_AUTO);
                str_cpy(v3, N_DIV[TSEL->p[P_SDIV] % 10], sizeof v3);   /* SPEED: the track's DIV, slow to fast */
                draw_column(3, "SPEED", v3, "", VAL(3u), (int32_t)tessera_cs_speed(tr) * 1000 / (TESS_CS_SPEEDS - 1u), ICON_AUTO);
            } else if (m == TESS_ENG_GRIDS) {
                const gen_grids_t *g = tessera_grids(tr);
                if (gr_layer_on()) {                            /* OCT+ held: KICK, SNR, HAT, the fourth part */
                    const char *p4 = tessera_grids_part4_name(tr);
                    static const char *const GRL[3] = {"KICK", "SNR", "HAT"};
                    for (uint32_t k = 0; k < 3u; k++) {
                        char v[6];
                        int32_t sh = 100 - (int32_t)g->thin[k];
                        fmt_int(v, sh);
                        col_mods(PID_TESS(TX_GRD_KICK + k));
                        draw_column(k, GRL[k], v, "%", VAL(k), sh * 10, ICON_AUTO);
                    }
                    fmt_int(v3, g->toms);
                    col_mods(PID_TESS(TX_GRD_TOMS));
                    if (g->toms) draw_column(3, p4, v3, "%", VAL(3u), (int32_t)g->toms * 10, ICON_AUTO);
                    else draw_column(3, p4, "OFF", "", VAL(3u), 0, ICON_AUTO);
                    return;
                }
                draw_column(0, "MODE", "ATLAS", "", VAL(0u), 5 * 1000 / (TESS_ENG_COUNT - 1), ICON_AUTO);
                fmt_int(v1, g->x);
                col_mods(PID_TESS(TX_GRD_X));
                draw_column(1, "X", v1, "", VAL(1u), (int32_t)g->x * 1000 / 255, ICON_AUTO);
                fmt_int(v2, g->y);
                col_mods(PID_TESS(TX_GRD_Y));
                draw_column(2, "Y", v2, "", VAL(2u), (int32_t)g->y * 1000 / 255, ICON_AUTO);
                fmt_int(v3, g->dens);
                col_mods(PID_TESS(TX_GRD_DENS));
                draw_column(3, "DENS", v3, "%", VAL(3u), (int32_t)g->dens * 10, ICON_AUTO);
            } else {
                const gen_morph_t *mo = tessera_morph(tr);
                int32_t pa = (int32_t)(mo->step_idx >> 4) & 7;
                int32_t pb = (int32_t)mo->oct_span & 7;
                draw_column(0, "MODE", "MORPH", "", VAL(0u), 4 * 1000 / (TESS_ENG_COUNT - 1), ICON_AUTO);
                fmt_int(v1, mo->morph_pos);
                col_mods(PID_TESS(TX_MRP_POS));
                draw_column(1, "POS", v1, "/127", VAL(1u), (int32_t)mo->morph_pos * 1000 / 127, ICON_AUTO);
                fmt_int(v2, pa + 1);
                draw_column(2, "SNP A", v2, "/8", VAL(2u), pa * 1000 / 7, ICON_AUTO);
                fmt_int(v3, pb + 1);
                draw_column(3, "SNP B", v3, "/8", VAL(3u), pb * 1000 / 7, ICON_AUTO);
            }
            return;
        }
    }
    if (cur_page()->graph == GR_CORDS) {                 /* (ui_cords.c) */
        cu_columns();
        return;
    }
    if (cur_page()->graph == GR_MATRIX) {                /* (ui_matrix.c) */
        mx_columns();
        return;
    }
    if (cur_page()->graph == GR_MOD) {                   /* the route's SRC, the route, its DST AMT */
        const track_t *t = TSEL;
        uint32_t id = P_M1SRC + 3u * mod_ui_slot;
        int32_t s = t->p[id], d = t->p[id + 1u], a = t->p[id + 2u];
        list_words(val, N_MSRC[clamp(s, 0, MS_N - 1)], sizeof val);   /* (the chips: words, no labels) */
        draw_column(0, "", val, "", s ? VAL(0u) : T_DIM, -1, mod_src_icon(s));
        fmt_int(val, (int32_t)mod_ui_slot + 1);
        draw_column(1, "SLOT", val, "/4", VAL(1u), (int32_t)mod_ui_slot * 1000 / 3, mod_src_icon(MS_OFF));   /* (the mod icon) */
        list_words(val, mod_dst_name(t, d), sizeof val);
        draw_column(2, "", val, "", d ? VAL(2u) : T_DIM, -1, mod_dst_icon(t, d));
        param_format(&TP[id + 2u], a, val, &unit);
        draw_column(3, "", val, unit[0] ? " %" : "", a ? VAL(3u) : T_DIM, RATIO(&TP[id + 2u], a), mod_src_icon(MS_OFF));
        return;
    }
    if (cur_page()->graph == GR_ROLL && !grid_on()) {
        uint32_t chosen = notes_selected(TSEL), start = notes_manual_start(TSEL);
        const step_t *st = start < NSTEP ? &TSEL->step[start] : 0;
        int selected = chosen < RECORD_MAX || (st && st->n);
        char sn[12], u[12];
        fmt_int(sn, (int32_t)ui.cursor + 1); u[0] = '/'; fmt_int(u + 1, TSEL->p[P_SLEN]);
        draw_column(0, ui.note_copy?"COPY":bulk.on||ui.note_move?"MOVE":"STEP", sn, bulk.on||ui.note_move?"":u, VAL(0u), -1, ICON_AUTO);
        uint32_t pitch = chosen < RECORD_MAX ? recording[chosen].note : st && st->n ? st->note[notes_manual_slot(st)] : last_note;
        note_name(sn, pitch);
        if(chosen<RECORD_MAX && drum_track(TSEL))fmt_int(sn,recording[chosen].pitch);
        draw_column(1, "PITCH", sn, chosen<RECORD_MAX && drum_track(TSEL)?"ST":"", selected ? VAL(1u) : T_DIM, -1, ICON_PITCH);
        if (chosen < RECORD_MAX) {
            uint32_t hundredths = ((uint32_t)recording[chosen].duration * (1u << (recording[chosen].owner >> 5)) * 100u + RECORD_UNIT / 2u) / RECORD_UNIT;
            if (!hundredths) str_cpy(sn, "<.01", sizeof sn);
            else {
                fmt_int(sn, (int32_t)(hundredths / 100u));
                if (hundredths % 100u) {
                    str_cpy(sn + str_len(sn), ".", 2);
                    uint32_t fraction = hundredths % 100u;
                    if (fraction < 10u) str_cpy(sn + str_len(sn), "0", 2);
                    fmt_int(sn + str_len(sn), (int32_t)fraction);
                }
            }
        } else if (selected) fmt_int(sn, (int32_t)step_note_length(TSEL, start));
        else str_cpy(sn, "--", sizeof sn);
        draw_column(2, "LENGTH", sn, chosen >= RECORD_MAX && st && st->n > 1u ? "ALL" : "", selected ? VAL(2u) : T_DIM, -1, ICON_GATE);
        if (chosen >= RECORD_MAX && st && (ui.step_mods & (1u << panel.btn[B_ENV]))) {
            draw_column(3, "SLIDE", st->flags & SF_SLIDE ? "ON" : "OFF", "", VAL(3u), -1, ICON_SLIDE);
            return;
        }
        if (selected) fmt_int(sn, chosen < RECORD_MAX ? recording[chosen].vel : st->flags & SF_ACCENT ? 127 : st->vel ? st->vel : 96);
        else str_cpy(sn, "--", sizeof sn);
        draw_column(3, "VEL", sn, chosen >= RECORD_MAX && st && st->n > 1u ? "ALL" : "", selected ? VAL(3u) : T_DIM, -1, ICON_ACCENT);
        return;
    }
    if (cur_page()->scope == SC_STEP && drum_track(TSEL)) {   /* the grid: STEP LANE HIT ACC */
        const step_t *st = &seq_steps(TSEL)[ui.cursor];
        uint32_t b = 1u << ui.lane, on = (step_lanes(st) & b) != 0u, ac = (step_accents(st) & b) != 0u;
        char sn[8], sl[8];
        fmt_int(sn, (int32_t)ui.cursor + 1);
        str_cpy(sl, "/", 8);
        fmt_int(sl + 1, TSEL->p[P_SLEN]);
        draw_column(0, "STEP", sn, sl, VAL(0u), -1, ICON_AUTO);
        draw_column(1, "", drum_lane_name(TSEL, ui.lane), "", VAL(1u), -1, ICON_AUTO);   /* (its chip: "Snare") */
        draw_column(2, "HIT", on ? "ON" : "--", "", on ? VAL(2u) : T_DIM, -1, ICON_AUTO);
        draw_column(3, "ACC", ac ? "ON" : "--", "", ac ? VAL(3u) : T_DIM, -1, ICON_AUTO);
        return;
    }
    if (cur_page()->scope == SC_STEP) {
        static const char *const TIME_N[3] = {"NOTE", "TIE", "REST"};
        const step_t *st = &seq_steps(TSEL)[ui.cursor];
        char u[8];
        uint32_t cnt = st->n, first = st->note[0], l;
        for (l = NLANE; l-- > 0;)                         /* (lane hits count as their notes) */
            if ((st->hit >> l) & 1u) {
                cnt++;
                if (!st->n)
                    first = DRUM_LANE_NOTE[l];
            }
        if (cnt) {
            note_name(val, first);
            u[0] = 0;
            if (cnt > 1) {
                str_cpy(u, "+", 8);
                fmt_int(u + 1, (int32_t)cnt - 1);
            }
        } else {
            str_cpy(val, "--", 12);
            u[0] = 0;
        }
        {
            static const char *const FLAG_N[4] = {"-", "ACC", "SLD", "A+S"};
            char sn[8], sl[8];
            fmt_int(sn, (int32_t)ui.cursor + 1);
            str_cpy(sl, "/", 8);
            fmt_int(sl + 1, TSEL->p[P_SLEN]);
            draw_column(0, "STEP", sn, sl, VAL(0u), -1, ICON_AUTO);
            draw_column(1, "NOTE", val, u, step_on(st) ? VAL(1u) : T_DIM, -1, ICON_AUTO);
            draw_column(2, "TIME", TIME_N[st->time % 3u], "", VAL(2u), -1, ICON_AUTO);
            draw_column(3, "FLAG", FLAG_N[(st->flags & SF_ACCENT ? 1u : 0u) | (st->flags & SF_SLIDE ? 2u : 0u)], "",
                        VAL(3u), -1, ICON_AUTO);
        }
        return;
    }
    for (c = 0; c < 4u; c++) {
        int16_t *vp;
        if (scale_settings_page(cur_page()) && c == 1u) {
            draw_column(c, "FAV", scale_favorite((uint32_t)TSEL->p[P_SCALE]) ? "ON" : "OFF", "", VAL(c), -1, ICON_X_STAR);
            continue;
        }
        const param_desc_t *d = page_desc(cur_page(), c, &vp);
        if (!d || !d->label || d->label[0] == '-') {
            draw_column(c, "", "", "", T_THEME, -1, ICON_AUTO);
            continue;
        }
        if (cur_page()->id[c] == G_MIDI && cur_page()->scope == SC_GLOBAL) {   /* KNOB 1: which input's status */
            uint32_t trs = song.g[G_MIDI] != 0, rx = midi_rx_recent(trs);
            str_cpy(val, trs ? "TRS" : "USB", 12);
            unit = rx ? "RX" : trs ? MIDI_TRS_STATE :
                   !usb.up ? "OFF" : usb.config ? "ON" : usb.setups ? "ENUM" : usb.sof_seen ? "BUS" : "WAIT";
            draw_column(c, "MIDI", val, unit, rx ? T_ACCENT : T_THEME, -1, ICON_AUTO);
            continue;
        }
        if ((act_cols() >> c) & 1u) {
            draw_act_column(c, d->label, T_THEME, ICON_AUTO);
            continue;
        }
        if (cur_page()->scope == SC_GLOBAL && ui.proj_new && (cur_page()->id[c] == G_SLOT || cur_page()->id[c] == G_SAVE)) {
            draw_column(c, d->label, cur_page()->id[c] == G_SLOT ? "NEW" : "--", "",   /* SLOT past TMPL: NEW (no SAVE) */
                        cur_page()->id[c] == G_SLOT ? VAL(c) : T_DIM, -1, ICON_AUTO);
            continue;
        }
        if (cur_page()->id[c] == G_INFO && cur_page()->scope == SC_GLOBAL) {
            fmt_int(val, (int32_t)(song.cpu_q8 * 100u / 256u));
            unit = "%";
        } else {
            param_format(d, *vp, val, &unit);
            if(d->names==N_FX && (*vp==FX_REVERB || *vp==FX_CHORUS))str_cpy(val,N_FX[*vp],sizeof val);
        }
        if (vp >= TSEL->p && vp < TSEL->p + P_COUNT)     /* a track parameter: the mods on it, live */
            col_mods(PID_TRACK(vp - TSEL->p));
        draw_column(c, d->label, val, unit,
                    vp == &TSEL->p[P_LRATE] && TSEL->p[P_LSYNC] ? T_DIM : VAL(c),   /* LFO 2 SYNC on: RATE does nothing */
                    d->fmt == F_ENUM && d->max < 2 ? -1 : RATIO(d, *vp), param_icon(d, *vp));
    }
}


/* UPDATE MODE countdown (main.c: OCT- + OCT+ held): over everything, the menu and the dialogs too */
static void draw_uboot(void)
{
    static uint8_t shown;
    char d[4] = {(char)('0' + ui.uboot % 10u), 0, 0, 0};
    if (!ui.force && shown == ui.uboot)
        return;
    shown = ui.uboot;
    lcd_fill(0, H_HEAD, 240, 240 - H_HEAD, T_BG);
    ui.head_sig = ~0u;
    draw_text_box(0, 76, 240, &AF_M, "UPDATE MODE IN", T_TEXT, 1);
    draw_text_box(0, 102, 240, &AF_L, d, T_THEME, 1);
    {   /* the two keycaps held, and what letting go does */
        int32_t w = kc_w(KC_OCTDN) + 3 + kh_w(KC_OCTUP, "LET GO TO CANCEL"), x = (240 - w) / 2;
        cv_begin(240, KC_H + 2, T_BG);
        x = cv_keycap(x, 1, KC_OCTDN, T_KEY, T_INK, T_BG) + 3;
        cv_key_hint(x, 1, KC_OCTUP, "LET GO TO CANCEL", 1, T_BG);
        cv_blit(0, 149);
        lcd_sync();
    }
    ui.force = 0;
    scr_shown();
}

/* the OCT- / OCT+ dialog: what it does (ui.confirm, ui.confirm_trk) on two lines, on a surface */
#define DLG_X 16
#define DLG_Y 58
#define DLG_W 208
#define DLG_H 124
static void confirm_text(char *a, char *b)
{
    uint32_t k = ui.confirm_trk;
    b[0] = 0;
    switch (ui.confirm) {
    case CF_CLEAR_TRK:
        str_cpy(a, "CLEAR TRACK 1?", 24);
        a[12] = (char)('1' + k % NTRK);
        break;
    case CF_OVR_PROJ:
        str_cpy(a, "OVERWRITE PROJECT A?", 24);
        a[18] = (char)('A' + (k & 3u));
        if (!project_name(k & 3u, b) || !b[0])          /* the project's name, else what SONG plays from it */
            str_cpy(b, "Its song and patterns change", 32);
        break;
    case CF_ERASE_PROJ:
        str_cpy(a, "ERASE PROJECT A?", 24);
        a[14] = (char)('A' + (k & 3u));
        if (!project_name(k & 3u, b) || !b[0])
            str_cpy(b, "Its song and patterns go", 32);
        break;
    case CF_DEL_ROW:
        str_cpy(a, "DELETE SONG ROW?", 24);
        break;
    case CF_CLEAR_SONG:
        str_cpy(a, "CLEAR SONG ORDER?", 24);
        break;
    case CF_NEW_SONG:                                   /* what is lost */
        str_cpy(a, "START A NEW SONG?", 24);
        str_cpy(b, "Unsaved changes", 24);
        break;
    case CF_PRINT:                                      /* REC held on a generator track with steps (capture_take) */
        str_cpy(a, "REPLACE STEPS?", 24);
        str_cpy(b, "With the generator's bars", 32);
        break;
    case CF_TAKE_JAM:                                   /* the rows it replaces */
        str_cpy(a, "SONG FROM JAM?", 24);
        fmt_int(b, (int32_t)chain_config.count);
        str_cpy(b + str_len(b), " rows replaced", 16);
        break;
    case CF_INIT_SOUND:
        str_cpy(a, "INITIALIZE SOUND?", 24);
        break;
    case CF_DEL_PAT:                                    /* PATTERNS: the track's pattern, its notes and motion */
        str_cpy(a, "DELETE PATTERN 1?", 24);
        a[15] = (char)('1' + trk[k % NTRK].pattern % NPAT);
        str_cpy(b, "Track 1 \xB7 notes and motion", 32);
        b[6] = (char)('1' + k % NTRK);
        break;
    case CF_PASTE_PAT:                                  /* PATTERNS' Copy to over a pattern in use */
        str_cpy(a, "REPLACE PATTERN 1?", 24);
        a[16] = (char)('1' + ui.ptc_dst % NPAT);
        str_cpy(b, "Track 1 \xB7 with pattern 1", 32);
        b[6] = (char)('1' + k % NTRK);
        b[str_len(b) - 1u] = (char)('1' + ui.ptc_src % NPAT);
        break;
    case CF_CLEAR_MOTION:
        str_cpy(a, "CLEAR T1 MOTION?", 24); a[7] = (char)('1' + k % NTRK);
        break;
    case CF_OVR_USER:
        if (user_old_bank(k)) { str_cpy(a, "REPLACE OLD BANK?", 24); str_cpy(b, "All 16 old slots replaced", 32); break; }
        str_cpy(a, "OVERWRITE ", 24);
        user_label(a + str_len(a), k);
        str_cpy(a + str_len(a), "?", 2);
        user_name(k, b);                             /* the sound stored there */
        break;
    case CF_DISCARD_DESIGN:
        str_cpy(a, "DISCARD THE DESIGN?", 24);
        str_cpy(b, "The original sound returns", 32);
        break;
    case CF_IMPORT:
        for (uint32_t slot = sx_import.first; slot < sx_import.first + sx_import.count; slot++) {
            if (sx_import.general ? up_old_bank(slot) : native_old(sx_import.engine, slot)) {
                str_cpy(a, "REPLACE OLD BANKS?", 24); str_cpy(b, "All slots in old banks replaced", 32); return;
            }
        }
        str_cpy(a,"OVERWRITE USER SLOTS?",24);
        fmt_int(b,sx_import.count);str_cpy(b+str_len(b)," incoming patches",20);break;
    case CF_BULK_ERASE:
        str_cpy(a,"ERASE SELECTED PRESETS?",24);
        str_cpy(b,"Selected slots will be empty",32);break;
    case CF_ERASE_USER:
        if (user_old_bank(k)) { str_cpy(a, "ERASE OLD BANK?", 24); str_cpy(b, "All 16 old slots erased", 32); break; }
        str_cpy(a, "ERASE ", 24);
        user_label(a + str_len(a), k);
        str_cpy(a + str_len(a), "?", 2);
        user_name(k, b);
        break;
    case CF_OLD_SETTINGS:
        str_cpy(a, "REPLACE OLD SETTINGS?", 24);
        str_cpy(b, "Old settings/template replaced", 32);
        break;
    case CF_OLD_TEMPLATE:
        str_cpy(a, "REPLACE OLD TEMPLATE?", 24);
        str_cpy(b, "Old settings/template replaced", 32);
        break;
    default:
        str_cpy(a, "CLEAR T1 SEQUENCE?", 24);     /* the header (T1..T4) is hidden */
        a[7] = (char)('1' + k % NTRK);
        break;
    }
}
/* the question (docs/design: mock/r5_pages, ref/dialog): a card over the page dimmed (ui_draw: drawn under cv_dim):
 * the warning in a tinted ring, the question in words, its detail, OCT- as "↶ No" and OCT+ as "✓ Yes" (the track's
 * colour) */
static void confirm_words(char *s)                      /* "OVERWRITE PROJECT A?" -> "Overwrite project A?" */
{
    uint32_t i;
    for (i = 1; s[i]; i++) {
        int single = s[i - 1] == ' ' && (s[i + 1] == '?' || !s[i + 1]);   /* (a slot's letter at the end) */
        if (s[i] >= 'A' && s[i] <= 'Z' && !single)
            s[i] = (char)(s[i] + 32);
    }
}
static void draw_confirm(void)
{
    char a[24], b[32];
    const aafont_t *tf = &AF_M;
    uint16_t ring = ux_mix(T_SURF, T_THEME, 20);
    confirm_text(a, b);
    confirm_words(a);
    cv_begin(DLG_W, DLG_H, T_BG);
    cv_rrect(0, 0, DLG_W, DLG_H, 10, T_LINE, T_BG);
    cv_rrect(1, 1, DLG_W - 2, DLG_H - 2, 9, T_SURF, T_LINE);
    cv_disc(DLG_W / 2, 26, 22, ring, 0);
    cv_icon_mid(DLG_W / 2 - 6, 26, 12, ui.confirm == CF_CLEAR_MOTION ? ICON_X_MOTION_DEL : ICON_X_WARN, T_THEME, ring);
    if (text_w(tf, a) > DLG_W - 16)
        tf = &AF_S;
    cv_text_c(DLG_W / 2, 45, tf, a, T_TEXT, T_SURF);
    if (b[0]) {
        char f[32];
        text_fit(f, sizeof f, b, &AF_S, DLG_W - 16);    /* a name: free text */
        cv_text_flags(DLG_W / 2 - text_w(&AF_S, f) / 2, 67, &AF_S, f, T_MID, T_SURF, 8u | (f[str_len(f) - 1u] == ELLIPSIS));
    }
    cv_rrect(16, 92, 80, 22, 6, T_LINE, T_SURF);       /* OCT-: no */
    cv_rrect(17, 93, 78, 20, 5, T_PANEL, T_LINE);
    cv_icon_mid(56 - 6 - text_w(&AF_S, "No") / 2 - 4, 103, 12, ICON_X_UNDO, T_MID, T_PANEL);
    cv_text_on(56 - text_w(&AF_S, "No") / 2 + 6, 96, &AF_S, "No", T_MID, T_PANEL);
    cv_rrect(112, 92, 80, 22, 6, T_THEME, T_SURF);     /* OCT+: yes */
    cv_icon_mid(152 - 6 - text_w(&AF_S, "Yes") / 2 - 4, 103, 12, ICON_X_CHECK, T_INK, T_THEME);
    cv_text_on(152 - text_w(&AF_S, "Yes") / 2 + 6, 96, &AF_S, "Yes", T_INK, T_THEME);
    cv_blit(DLG_X, DLG_Y);
}

#include "ui_pages.c"                                   /* the redesign's other pages (R5) */
#include "ui_list.c"                                    /* list pages */
#include "ui_popup.c"                                   /* action sheets, pickers */
#include "ui_slots.c"                                   /* PROJECT, USER, the STOREs: their slots */
#include "ui_motion.c"                                  /* MOTION: its lanes */
#include "ui_scales.c"                                  /* SCALES: a screen of its own */
#include "ui_designer.c"                                /* guided native sound creation */
#include "ui_awake.c"                                   /* AWAKE's step entry on GEN */
static int own_screen(void)                             /* a page drawn whole by its own code, no cards or footer */
{
    uint32_t g = cur_page()->graph;
    return !ui.home && (g == GR_BROWSE || g == GR_PATGRID || g == GR_SONG || g == GR_SCALE_PICKER);
}
/* the foot has the page tabs, the header then no dots: the sound's strip (ui_pages.c pv_sound) on the pages pv_draw
 * draws with it, draw_foot's second row on the rest (not the notes', which end before it); not the designer's (its
 * stages are its tabs), MIXER's, MOD's or CORDS' (no foot) */
static int foot_tabs_on(void)
{
    uint32_t k = pv_kind(), g = cur_page()->graph;
    if (ui.home || designer_on() || own_screen() || slot_kind() || motion_page() || list_on())
        return 0;
    if (k == PV_NONE)
        return !(g == GR_ROLL && (!grid_on() || live_rec_sel()));
    return k == PV_CURVE || k == PV_RINGS || k == PV_FADERS || k == PV_NOTES || k == PV_GRID;
}
/* the page (or Stage): its own screen, the redesign's, else its cards, panel and footer */
static void draw_page(void)
{
    if (!ui.home && !page_visible(ui.page)) {          /* an OP page (DIGITAL's, retired): EDIT 1 */
        ui.page = (uint8_t)page_first(FAM_EDIT);
        page_entered();
    }
    cursor_fix();
    if (designer_screen()) {                            /* the sound designer's START, FEATURES (ui_designer.c) */
        designer_draw();
    } else if (!own_screen() && slot_kind()) {          /* a slot page (ui_slots.c) */
        slot_draw();
    } else if (!own_screen() && motion_page()) {        /* MOTION's lanes (ui_motion.c) */
        motion_draw();
    } else if (!own_screen() && list_on()) {            /* a list page (ui_list.c) */
        list_draw();
    } else if (!own_screen() && pv_kind()) {            /* the redesign's other pages (ui_pages.c) */
        pv_draw();
    } else if (own_screen()) {                          /* the browser, PATTERNS, SONG: screens of their own */
        if (cur_page()->graph == GR_BROWSE)
            browser_draw();
        else if (cur_page()->graph == GR_SCALE_PICKER)
            scales_draw();
        else if (cur_page()->graph == GR_SONG)
            song_draw();
        else {
            draw_head();
            patterns_draw();
        }
    } else {
        if (ui.force)
            draw_frame(ui.home);
        melodee_dbg.stage = 3;
        draw_head();
        melodee_dbg.stage = 4;
        draw_columns();
        melodee_dbg.stage = 5;
        if (ui.home)                                    /* Stage: its panel and the lanes (ui_stage.c), no footer */
            stage_draw();
        else
            draw_graph();
    }
    melodee_dbg.stage = 6;
    if (!ui.home && !own_screen() && !pv_kind() && !list_on() && !slot_kind() && !motion_page())
        draw_foot();
}
static void ui_draw_frame(void);
static void ui_draw(void)                               /* (gfx.c: the fills wait for the frame's canvases) */
{
    gfx_frame(1);
    ui_draw_frame();
    gfx_frame(0);
}
static void ui_draw_frame(void)
{
    if (boot_work.state) {
        scrn.idle = fm1_ms;                          /* maintenance/errors never sleep or get covered */
        if (ui.force) { boot_work_draw(); ui.force = 0; }
        return;
    }
    if (!scr_frame()) return;
    ui.frame++;
    if (palette_track(song.sel))                       /* THEME is the selected track's colour */
        ui.force = 1;
    if(seq_counting()) { char text[24]="COUNT IN ";fmt_int(text+9,(int32_t)cin_left);
        ui_message(text); }
    else if(!memcmp(ui.msg,"COUNT IN ",9))ui.msg_t=0;

    if (ui.uboot) {
        draw_uboot();
        draw_head();
        return;
    }
    if (ui.menu) {
        draw_menu();
        ui.force = 0;
        return;
    }
    if (ui.confirm) {                                   /* the OCT- / OCT+ dialog */
        if (ui.force) {                                 /* over the page, dimmed (each canvas darkened as it goes) */
            uint8_t cf = ui.confirm;
            ui.confirm = 0;
            cv_dim = cv_under = 1;
            draw_page();
            cv_dim = cv_under = 0;
            ui.confirm = cf;
            ui.force = 1;
            draw_confirm();
            ui.force = 0;
        }
        draw_head();                                    /* recording remains visible over confirmations */
        return;
    }
    if (help.on) {                                      /* ARP held + a knob: its help over the page, dimmed (ui_help.c) */
        if (ui.force) {
            cv_dim = cv_under = 1;
            draw_page();
            cv_dim = cv_under = 0;
            ui.force = 1;
            help_draw();
            ui.force = 0;
        }
        draw_head();
        return;
    }
    if (msc.on) {                                       /* FX held + a knob: the four knobs' mod amounts (ui_modsc.c) */
        if (ui.force) {
            cv_dim = cv_under = 1;
            draw_page();
            cv_dim = cv_under = 0;
            ui.force = 1;
            msc_draw();
            ui.force = 0;
        }
        draw_head();
        return;
    }
    if (smap.on && (ui.home || cur_page()->fam != FAM_EDIT || ui.layer))
        smap.on = 0;
    if (smap.on) {                                      /* an engine's map (ui_sections.c) */
        smap_draw();
        ui.force = 0;
        return;
    }
    pick_poll();
    if (pop.on) {                                       /* a popup over the page, dimmed (ui_popup.c); a knob's picker
                                                         * over it as it is (it comes with every turn of a list knob:
                                                         * dimming sent the whole screen twice, ~150 ms on the SPI) */
        if (ui.force) {
            cv_dim = pop.on != POP_PICK;
            cv_under = 1;
            draw_page();
            cv_dim = cv_under = 0;
            ui.force = 1;
        } else if (pop.on == POP_PICK)
            pv_mod_chips();                             /* (the values under it, the page left as it is: no flash) */
        melodee_dbg.stage = 7;
        pop_draw();
        ui.force = 0;
        return;
    }
    if (name_on()) {                                    /* NAME: a user preset's or a project's (ui_name.c) */
        name_draw();
        return;
    }
    if (new_on()) {                                     /* NEW SONG (ui_new.c) */
        new_draw();
        return;
    }
    if (ui.layer) {                                     /* a layer's map over the page (ui_layer.c) */
        if (ui.force)
            draw_frame(0);
        draw_head();
        draw_layer();
        if (ui.msg_t && !--ui.msg_t && ui.msg2[0]) {
            str_cpy(ui.msg, ui.msg2, sizeof ui.msg);
            ui.msg2[0] = 0;
            ui.msg_t = 60;
        }
        if (ui.hot_t)
            ui.hot_t--;
        if (ui.bpm_t)
            ui.bpm_t--;
        ui.force = 0;
        return;
    }
    draw_page();
    if (ui.msg_t && !--ui.msg_t && ui.msg2[0]) {     /* the second message (ui_notices) */
        str_cpy(ui.msg, ui.msg2, sizeof ui.msg);
        ui.msg2[0] = 0;
        ui.msg_t = 60;
    }
    if (ui.bpm_t)
        ui.bpm_t--;
    if (ui.hot_t)
        ui.hot_t--;
    ui.force = 0;
    scr_shown();
}
