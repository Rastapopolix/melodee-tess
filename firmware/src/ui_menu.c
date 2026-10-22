/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Melodee menu (HOME held): COLOR, HOLD (the layer threshold), LIGHTS (the keys' and buttons'
 * glow: ui_input.c ui_leds), KNOB ACCEL (ui_input.c accel_by), TRACK KNOB (RIGHT: ALGORITHM picks the track, LEFT:
 * PRESETS does; panel.c), USB AUDIO (IN+OUT, IN, OUT or OFF; usb.c ua_off_set),
 * USB INPUT (stereo master or four track stems; usb.c ua_tracks_set),
 * MIDI OUT (KEYS, ALL or OFF: the notes sent over USB) and CLOCK OUT (midi_out.c),
 * AUTOSAVE (automatic project saves after five seconds stopped and untouched),
 * CALIBRATION (the setup screen: HARDWARE CALIBRATION), ABOUT.
 * PRESETS scrolls from ABOUT through all credits. ui.menu: 1 list, 2 information. */
/* ------------------------------------------------------------ menu --- */
#define MENU_HEAD 24                     /* the menu's own title strip (Stage and the pages: H_HEAD) */
enum { MI_COLOR, MI_HOLD, MI_LIGHTS,
#if MELODEE_USB_AUDIO
       MI_USB, MI_USB_INPUT,
#endif
       MI_MIDI_OUT, MI_CLOCK_OUT, MI_CLICK, MI_CLICK_LEVEL, MI_COUNTIN, MI_PREVIEW, MI_ADD, MI_AUTOSAVE, MI_ACCEL, MI_TRACK_KNOB, MI_SCREEN, MI_PANEL, MI_ABOUT, MI_DESIGNER, MI_BACK,
       MI_COUNT };
static const char *const MI_NAME[MI_COUNT] = {"COLOR", "HOLD", "LIGHTS",
#if MELODEE_USB_AUDIO
                                              "USB AUDIO", "USB INPUT",
#endif
                                              "MIDI OUT", "CLOCK OUT", "AUDIO CLICK", "CLICK LEVEL", "COUNT-IN", "NOTE PREVIEW", "CHORD ENTRY",
                                              "AUTOSAVE", "KNOB ACCEL", "TRACK KNOB", "SCREEN OFF", "CALIBRATION", "ABOUT", "SOUND DESIGNER", "BACK"};
#if MELODEE_USB_AUDIO
/* USB AUDIO's four settings, as ua_off_want (UA_OFF_OUT | UA_OFF_IN) */
static const char *const MI_USB_NAME[4] = {"IN+OUT", "IN", "OUT", "OFF"};
#endif

static void hexs(char *b, uint32_t v)                /* v as 8 hex digits (the crash screen, ABOUT) */
{
    uint32_t i;
    for (i = 0; i < 8u; i++)
        b[i] = "0123456789ABCDEF"[(v >> (28u - 4u * i)) & 15u];
    b[8] = 0;
}
/* where a crash was, as the crash screen and ABOUT say it: "IN AUDIO, 812 S IN" or "UI MODS 7, 812 S IN" (the page
 * and the main loop's breadcrumb, main.c / ui_draw.c melodee_dbg.stage: 1 input, 2..6 drawing, 7 a popup, 9 done) */
static void crash_where(char *b, uint32_t audio, uint32_t stage, uint32_t page, uint32_t ms)
{
    char n[12];
    if (audio)
        str_cpy(b, "IN AUDIO", 9);
    else {
        str_cpy(b, "UI ", 4);
        str_cpy(b + 3, page < NPAGES ? PAGES[page].title : "?", 12);
        str_cpy(b + str_len(b), " ", 2);
        fmt_int(n, (int32_t)(stage & 15u));
        str_cpy(b + str_len(b), n, 3);
    }
    str_cpy(b + str_len(b), ", ", 3);
    fmt_int(n, (int32_t)(ms / 1000u > 99999u ? 99999u : ms / 1000u));
    str_cpy(b + str_len(b), n, 6);
    str_cpy(b + str_len(b), " S IN", 6);
}

/* https://github.com/Rastapopolix/melodee-tess, QR version 3, correction L (segno; 44 bytes, over M's 42).
 * One row per word, leftmost module in bit 0; the constant matrix needs no encoder or RAM. */
static const uint32_t ABOUT_QR[29] = {
    0x1FC26E7Fu, 0x10515841u, 0x174D0E5Du, 0x1743AD5Du, 0x175D155Du, 0x10544641u,
    0x1FD5557Fu, 0x00015200u, 0x030276E3u, 0x0DA8C4A5u, 0x0105DBF4u, 0x022D4C2Eu,
    0x10C9C6C1u, 0x19D7B596u, 0x0733C4E9u, 0x15881788u, 0x06AA70D8u, 0x1DC9C08Fu,
    0x130EB5DBu, 0x00558DADu, 0x1DFA7FE1u, 0x0317D300u, 0x075AD37Fu, 0x01185541u,
    0x03F81E5Du, 0x163FD05Du, 0x0FF9A05Du, 0x16C51941u, 0x05CA617Fu,
};

static void draw_about_qr(int32_t x, int32_t y)
{
    uint32_t row, col;
    cv_rect(x, y, 74, 74, UI_QR_LIGHT);             /* fixed light / dark, independent of the palette */
    for (row = 0; row < 29u; row++)
        for (col = 0; col < 29u; col++)
            if ((ABOUT_QR[row] >> col) & 1u)
                cv_rect(x + 8 + (int32_t)col * 2, y + 8 + (int32_t)row * 2, 2, 2, UI_QR_DARK);
}

/* All notices are in flash. Wrap at the font's actual width, without dropping URL
 * characters; the same layout computes the scroll limit and draws clipped bands.
 * Mixed case here (long text); S throughout, the title in L. */
#define MENU_DOC_H 216                                /* the document: screen rows 24..240 */
enum { MC_TEXT, MC_TITLE, MC_URL };
typedef struct { const char *text; uint8_t kind; } menu_credit_t;
static const menu_credit_t MENU_CREDITS[] = {
    {"MELODEE (TESS) / THIS FORK", MC_TITLE},
    {"TESS fork by Rastapopolix (Theo Dainis): Tessera generators, engines and project save / GPL-3.0", MC_TEXT},
    {"Thanks: Inputsignwave", MC_TEXT},
    {"tess is short for tessera, the small tile of a mosaic: short phrases set side by side into a larger form", MC_TEXT},
    {"https://github.com/Rastapopolix/melodee-tess", MC_URL},
    {"", MC_TEXT},
    {"MELODEE / ORIGINAL", MC_TITLE},                  /* Tess is a modified Melodee */
    {"Kerem Kilic (keremimo), Ellic Studio / GPL-3.0", MC_TEXT},
    {"https://github.com/keremimo/melodee", MC_URL},
    {"", MC_TEXT},
    {"FELUCCA / UPSTREAM", MC_TITLE},                  /* Melodee is a modified Felucca */
    {"H\xFCgelton Instruments, Leo Kuroshita / GPL-3.0", MC_TEXT},
    {"https://github.com/hugelton/Felucca", MC_URL},
    {"", MC_TEXT},
    {"H\xFCgelton Instruments / OWN WORK", MC_TITLE},
    {"Leo Kuroshita", MC_TEXT},
    {"CrispyZebra (PHASE waveforms), synthesized 808: GPL-3.0", MC_TEXT},
    {"https://github.com/hugelton", MC_URL},
    {"", MC_TEXT},
    {"ZEN-1 AND FUMI-1 / FM6 PATCHES", MC_TITLE},
    {"aceyprime (ZEN-1), cartesive (FuMi-1): their own FM6 voices, SOFT PAD .. KUGO HARP / GPL-3.0", MC_TEXT},
    {"https://github.com/aceyprime/zen-1", MC_URL},
    {"https://github.com/cartesive/fumi-1", MC_URL},
    {"", MC_TEXT},
    {"KLATTSCH / VOICE REFERENCE", MC_TITLE},
    {"Tony Gies / MIT", MC_TEXT},
    {"https://github.com/tgies/klattsch", MC_URL},
    {"", MC_TEXT},
    {"DAISYSP / PHYS PORT", MC_TITLE},
    {"Electrosmith, Emilie Gillet", MC_TEXT},
    {"MIT", MC_TEXT},
    {"https://github.com/electro-smith/DaisySP", MC_URL},
    {"", MC_TEXT},
    {"uPD933 / NATIVE CZ", MC_TITLE},
    {"Devin Acker / BSD-3-Clause", MC_TEXT},
    {"https://github.com/mamedev/mame", MC_URL},
    {"", MC_TEXT},
    {"DEXED + MSFA / FM6 PORT", MC_TITLE},
    {"Pascal Gauthier, Google Inc.", MC_TEXT},
    {"msfa: Apache-2.0, Dexed: GPL-3.0", MC_TEXT},
    {"https://github.com/asb2m10/dexed", MC_URL},
    {"", MC_TEXT},
    {"RESID / SID MODEL", MC_TITLE},
    {"Dag Lem / GPL-2.0-or-later", MC_TEXT},
    {"https://github.com/libsidplayfp/resid", MC_URL},
    {"", MC_TEXT},
    {"RINGS / PHYS PORT", MC_TITLE},
    {"Emilie Gillet / MIT", MC_TEXT},
    {"https://github.com/pichenettes/eurorack", MC_URL},
    {"", MC_TEXT},
    {"CHAKRA PETCH / FONT", MC_TITLE},
    {"The Chakra Petch Project Authors", MC_TEXT},
    {"SIL Open Font License 1.1", MC_TEXT},
    {"https://github.com/m4rc1e/Chakra-Petch", MC_URL},
    {"", MC_TEXT},
    {"VSCO-2 CE + VCSL / SAMPLES", MC_TITLE},
    {"Versilian Studios / CC0 1.0", MC_TEXT},
    {"https://github.com/sgossner/VSCO-2-CE", MC_URL},
    {"https://github.com/sgossner/VCSL", MC_URL},
    {"", MC_TEXT},
    {"VOICE / RESEARCH", MC_TITLE},
    {"Dennis H. Klatt (1980)", MC_TEXT},
    {"https://doi.org/10.1121/1.383940", MC_URL},
    {"Hillenbrand, Getty, Clark and Wheeler (1995)", MC_TEXT},
    {"https://doi.org/10.1121/1.411872", MC_URL},
};

static int32_t menu_text(const char *s, int32_t y, uint16_t col, int draw)
{
    while (*s) {
        char line[64];
        uint32_t n = 0, space = 0, take, prev = 0;
        int32_t pen = 0;                                 /* 1/16 px, as text_w adds it up */
        while (s[n] && n < sizeof line - 1u) {
            uint32_t c = (uint8_t)s[n];
            line[n] = s[n];
            line[n + 1u] = 0;
            pen += (prev ? kern(&AF_S, prev, c) : 0) + AF_S.g[glyph(&AF_S, c)].adv;
            prev = c;
            if ((pen + 8) >> 4 > 226)
                break;
            if (s[n] == ' ')
                space = n;
            n++;
        }
        take = s[n] && space ? space : n;
        if (!take) take = 1;                         /* progress even for an unusually wide glyph */
        line[take] = 0;
        if (draw && y + AF_S.h > 0 && y < MENU_DOC_H)
            cv_text(6, y, &AF_S, line, col);
        y += 18;
        s += take;
        while (*s == ' ') s++;
    }
    return y;
}

static int32_t menu_document(int32_t y, int draw)
{
    uint32_t i;
    if (draw) {
        int32_t x;                                    /* (docs/design: mock/r6_pages About) */
        cv_text(6, y + 4, &AF_L, "Melodee", T_THEME);
        cv_text(6, y + 36, &AF_M, "(Tess)", T_THEME);
        x = cv_text(6, y + 62, &AF_M, MELODEE_VERSION, T_TEXT);
        cv_text(x + 8, y + 67, &AF_X, __DATE__, T_MID);
        draw_about_qr(158, y + 4);
        cv_text(6, y + 94, &AF_S, "MELODEE by Kerem Kilic (keremimo)", T_TEXT);
        cv_text(6, y + 112, &AF_S, "\xA9 2026 Kerem Kilic, Ellic Studio", T_SEC);
        cv_text(6, y + 130, &AF_S, "TESS fork by Rastapopolix", T_TEXT);
        cv_text(6, y + 148, &AF_S, "Thanks: Inputsignwave", T_SEC);
        cv_text(6, y + 166, &AF_S, "Based on Felucca by Leo Kuroshita,", T_TEXT);
        cv_text(6, y + 184, &AF_S, "H\xFCgelton Instruments", T_SEC);
        cv_text(6, y + 202, &AF_S, "GPL-3.0-only", T_TEXT);
        cv_text(6, y + 220, &AF_S, "github.com/Rastapopolix/melodee-tess", T_MID);
    }
    y += 246;
#ifdef FM1_CRASH_MAGIC
    if (fm1_crash.magic == FM1_CRASH_MAGIC) {         /* the last crash's record (kept over the restart, gone at a
                                                       * power-off): what its screen said, for a report */
        char b[48], h[12];
        y = menu_text("LAST CRASH", y, T_THEME, draw);
        str_cpy(b, "VEC ", 5);
        hexs(h, fm1_crash.vec);
        str_cpy(b + 4, h + 6, 3);
        str_cpy(b + str_len(b), "  PC ", 6);
        hexs(h, fm1_crash.pc);
        str_cpy(b + str_len(b), h, 9);
        str_cpy(b + str_len(b), "  RETS ", 8);
        hexs(h, fm1_crash.rets);
        str_cpy(b + str_len(b), h, 9);
        y = menu_text(b, y, T_TEXT, draw);
        str_cpy(b, "EMU ", 5);
        hexs(h, fm1_crash.emu);
        str_cpy(b + 4, h, 9);
        str_cpy(b + str_len(b), "  DBG ", 7);
        hexs(h, fm1_crash.dbg);
        str_cpy(b + str_len(b), h, 9);
        y = menu_text(b, y, T_TEXT, draw);
        if (melodee_dbg.crash_magic == DBG_CRASH_MAGIC) {
            crash_where(b, melodee_dbg.crash_audio, melodee_dbg.crash_stage, melodee_dbg.crash_page,
                        fm1_crash.uptime_ms);
            y = menu_text(b, y, T_SEC, draw);
        }
        y += 12;
    }
#endif
    y = menu_text("CREDITS", y, T_THEME, draw) + 8;
    for (i = 0; i < NELEM(MENU_CREDITS); i++) {
        const menu_credit_t *c = &MENU_CREDITS[i];
        const char *text = c->text;
        /* Display host/path once; the full source URL stays in the flash table. */
        if (c->kind == MC_URL && str_len(text) >= 8u && !memcmp(text, "https://", 8))
            text += 8;
        if (!text[0]) y += 8;
        else y = menu_text(text, y, c->kind == MC_TITLE ? T_THEME : c->kind == MC_URL ? T_MID : T_TEXT, draw);
    }
    y = menu_text("LICENSES + SOURCE", y + 12, T_THEME, draw);
    y = menu_text("DRUM + own samples: GPL-3.0-only", y, T_TEXT, draw);
    y = menu_text("Fonts and icons: their licences travel with the source and the release files.", y, T_TEXT, draw);
    y = menu_text("NO WARRANTY", y, T_TEXT, draw);
    y = menu_text("Full notices in source:", y + 8, T_TEXT, draw);
    y = menu_text("github.com/Rastapopolix/melodee-tess", y, T_MID, draw);
    return y + 4;
}
static int32_t menu_scroll_max(void)
{
    int32_t h = menu_document(0, 0);
    return h > MENU_DOC_H ? h - MENU_DOC_H : 0;
}

/* the menu's header (0..24): its title, the REC mark, the way back */
static void menu_head(void)
{
    cv_begin(240, MENU_HEAD, T_BG);
    cv_text(8, 2, &AF_M, ui.menu == 2 ? "About" : "Settings", T_TEXT);
    if (ui.menu >= 2) {                               /* the way back at the right, the REC mark before it (then */
        cv_icon_mid(226, MENU_HEAD / 2, 12, ICON_X_UNDO, T_MID, T_BG);   /* (OCT-) */
        draw_rec_mark(204, T_BG);
    } else {
        draw_rec_mark(178, T_BG);
    }
    cv_blit(0, Y_HEAD);
}

/* the list (docs/design: mock/r5_pages, ref/settings): nine rows, 20 px, 22 apart from y 28: the name (S, secondary)
 * and its value (S, mid) at the right in words ("Night", "0.4 s"); the selected row lifted across the screen with a
 * bar in the track's colour, its name in text and its value in the track's colour; COLOR: the palettes as swatches,
 * the one chosen outlined. Under the rows where they are in the list. Drawn in two bands (the canvas holds 124
 * rows), split between two rows */
#define MENU_VISIBLE 9u
#define MENU_Y0 28
#define MENU_ROW 22
#define MENU_RH 20
#define MENU_SPLIT 137
/* "AUDIO CLICK" -> "Audio click", "P5 OSC A" -> "Osc A" (acronyms and a letter alone stay capitals) */
static void menu_words(char *d, const char *s, uint32_t n)
{
    static const char *const AC[] = {"USB", "FX", "MIDI", "LFO", "EG", "FM", "OP", "PW", "DCW", "DCA", "DCO", "MPC"};
    uint32_t i, w = 0, a;
    str_cpy(d, s, n);
    for (i = 0; d[i]; i++) {
        int first = !i, ac = 0;
        if (d[i] == ' ' || d[i] == '+' || d[i] == '-' || d[i] == '/') { w = i + 1u; continue; }
        if (i == w) {                                   /* a word: an acronym, a letter alone stay */
            for (a = 0; a < NELEM(AC) && !ac; a++) {
                uint32_t l = str_len(AC[a]);
                ac = !memcmp(d + w, AC[a], l) && (!d[w + l] || d[w + l] == ' ' || d[w + l] == '/' || (d[w + l] >= '0' &&
                     d[w + l] <= '9'));
            }
            ac |= !d[i + 1] || d[i + 1] == ' ';
        }
        if (ac) { while (d[i + 1] && d[i + 1] != ' ' && d[i + 1] != '/') i++; continue; }
        if (!first && d[i] >= 'A' && d[i] <= 'Z') d[i] = (char)(d[i] + 32);
    }
}
static void draw_menu(void)
{
    uint32_t i, pass, sig = ui.menu * 7u + ui.menu_sel * 131u + settings.palette * 1009u + settings.lowcut * 7919u +
                            settings_hold * 3511u + settings_lights * 6151u + settings_autosave * 15485863u +
#if MELODEE_USB_AUDIO
                            ua_off_want * 92821u + ua_tracks_want * 324161u +
#endif
                            song.rec * 65537u + song.sel * 13u +
                            (ui.menu == 2 ? ui.menu_scroll * 48611u : 0u);
    if (!ui.force && sig == ui.menu_sig)
        return;
    ui.menu_sig = sig;
    menu_head();
    if (ui.menu >= 2) {
        int32_t scroll = ui.menu_scroll, max = menu_scroll_max();
        int32_t thumb = MENU_DOC_H * MENU_DOC_H / (max + MENU_DOC_H);
        if (thumb < 12) thumb = 12;
        for (pass = 0; pass < 2u; pass++) {
            cv_begin(240, pass ? MENU_DOC_H - 124u : 124u, T_BG);
            cv_oy = pass ? -124 : 0;
            cv_scroll = 1;
            menu_document(-scroll, 1);
            cv_scroll = 0;
            cv_rrect(235, 2, 3, MENU_DOC_H - 4, 1, T_LINE, T_BG);           /* the scroll bar */
            cv_rrect(235, 2 + (max ? scroll * (MENU_DOC_H - 4 - thumb) / max : 0), 3, thumb, 1, T_THEME, T_LINE);
            cv_oy = 0;
            cv_blit(0, MENU_HEAD + pass * 124u);
        }
        return;
    }
    for (pass = 0; pass < 2u; pass++) {
        int32_t top = pass ? MENU_SPLIT : MENU_HEAD;
        cv_begin(240, (uint32_t)(pass ? 240 - MENU_SPLIT : MENU_SPLIT - MENU_HEAD), T_BG);
        cv_oy = -top;                                 /* drawn in screen rows */
        uint32_t first=ui.menu_sel>=MENU_VISIBLE?ui.menu_sel-MENU_VISIBLE+1u:0u;
        for (i = first; i < first+MENU_VISIBLE && i<MI_COUNT; i++) {
            int32_t y = MENU_Y0 + (int32_t)(i-first) * MENU_ROW, yt = y + 3;
            int sel = i == ui.menu_sel;
            uint16_t bg = sel ? T_LIFT : T_BG, val = sel ? T_THEME : T_MID;
            const char *value = 0;
            char b[24], w[24];
            if (y + MENU_RH <= top || y >= top + (int32_t)cv_h)
                continue;
            if (sel) {
                cv_rect(0, y, 240, MENU_RH, T_LIFT);
                cv_rect(0, y, 3, MENU_RH, T_THEME);
            }
            menu_words(w, MI_NAME[i], sizeof w);
            cv_text_on(12, yt, &AF_S, w, sel ? T_TEXT : T_SEC, bg);
            if (i >= MI_CLICK && i <= MI_ADD)
                value = i==MI_CLICK?(const char *const[]){"OFF","REC","ON"}[settings_click%3u]:
                  i==MI_CLICK_LEVEL?(const char *const[]){"LOW","MID","HIGH"}[settings_click_level%3u]:
                  i==MI_COUNTIN?(const char *const[]){"OFF","1 BAR","2 BARS"}[settings_countin%3u]:
                  i==MI_PREVIEW?(settings_preview?"ON":"OFF"):(settings_chord_add?"ADD":"HOLD");
            if (i == MI_ACCEL) value = PREF_BITS & PREF_ACCEL_OFF ? "OFF" : "ON";
            if (i == MI_TRACK_KNOB) value = settings_knob_swap ? "LEFT" : "RIGHT";
            if (i == MI_MIDI_OUT) value = (const char *const[]){"KEYS", "ALL", "OFF"}[settings_midi_out % MIDI_OUT_N];
            if (i == MI_CLOCK_OUT) value = settings_clock_out ? "ON" : "OFF";
            if (i == MI_AUTOSAVE) value = settings_autosave ? "ON" : "OFF";
            if (i == MI_SCREEN) value = (const char *const[]){"NEVER", "5 MIN", "15 MIN", "30 MIN", "60 MIN"}[scr_get()];
            if (i == MI_LIGHTS) value = LIGHTS_NAME[settings_lights % LIGHTS_N];
#if MELODEE_USB_AUDIO
            if (i == MI_USB_INPUT) value = ua_tracks_want ? "4 TRACKS" : "STEREO";
            if (i == MI_USB) value = (const char *const[]){"In + Out", "In", "Out", "Off"}[ua_off_want & 3u];
#endif
            if (i == MI_HOLD) {                         /* "0.4 s" */
                str_cpy(b, "0.4 s", sizeof b);
                b[2] = (char)('0' + HOLD_MS[settings_hold % 4u] / 100u);
                value = b;
            }
            if (i == MI_COLOR) {                        /* the palettes as swatches, the one chosen outlined */
                uint32_t k;
                int32_t x;
                menu_words(w, UI_PALETTES[settings.palette % NPALETTES].name, sizeof w);
                x = cv_text_r(228, yt, &AF_S, w, val, bg) - 10 - 12 * (int32_t)NPALETTES;   /* (cv_text_r: its left) */
                for (k = 0; k < NPALETTES; k++) {
                    int on = k == settings.palette % NPALETTES;
                    cv_rrect(x + 12 * (int32_t)k, y + 5, 10, 10, 2, on ? T_THEME : T_MID, bg);
                    cv_rrect(x + 12 * (int32_t)k + 1, y + 6, 8, 8, 2,   /* (GRAY, the tests': greys) */
                             ux.mono ? ux_mix(T_BG, T_TEXT, 20u + 60u * k / (NPALETTES - 1u)) : UI_PALETTES[k].bg, on ? T_THEME : T_MID);
                }
            } else if (value) {
                menu_words(w, value, sizeof w);
                cv_text_r(228, yt, &AF_S, w, val, bg);
            }
        }
        {   /* where the rows are in the list */
            int32_t tw = 232 * (int32_t)MENU_VISIBLE / (int32_t)MI_COUNT;
            int32_t tx = MI_COUNT > MENU_VISIBLE ? (232 - tw) * (int32_t)first / (int32_t)(MI_COUNT - MENU_VISIBLE) : 0;
            cv_rrect(4, 228, 232, 2, 1, T_LINE, T_BG);
            cv_rrect(4 + tx, 228, tw, 2, 1, T_THEME, T_LINE);
        }
        cv_oy = 0;
        cv_blit(0, (uint32_t)top);
    }
}
static void enc_drop(void)                             /* knob turns nobody takes */
{
    uint32_t k;
    for (k = 0; k < NE; k++)
        panel_enc(k);
}
/* .. but the knobs in keep (a bit a role): a detent of theirs the scan ISR publishes after their take waits for the
 * next frame instead of being dropped (ui_input runs in a tight loop, so that window is most of a frame's time) */
static void enc_drop_but(uint32_t keep)
{
    uint32_t k;
    for (k = 0; k < NE; k++)
        if (!((keep >> k) & 1u)) panel_enc(k);
}

static void menu_close(void)
{
    settings_save();                                   /* palette / panel table, if changed */
    ui.menu = 0;
    ui.force = 1;
    go_home();
}

/* menu: PRESETS moves, OCT+ confirms, OCT- cancels (ABOUT -> list -> close) */
static void menu_input(uint32_t oct)                  /* oct: ui_input.c oct_taps */
{
    int32_t s;
    uint32_t ok = (oct >> 1) & 1u, back = oct & 1u;
    if (back) {
        if (ui.menu >= 2)
            ui.menu = 1, ui.force = 1;
        else
            menu_close();
        return;
    }
    if ((s = panel_enc(EN_K1)) != 0 && ui.menu == 1)  /* KNOB 1 the row, KNOB 2 its value (Tessera's lists) */
        ui.menu_sel = (uint8_t)clamp((int32_t)ui.menu_sel + s, 0, (int32_t)MI_COUNT - 1);
    if ((s = panel_enc(EN_PRESET)) != 0 && ui.menu == 1)
        ui.menu_sel = (uint8_t)((ui.menu_sel + (s > 0 ? 1u : MI_COUNT - 1u)) % MI_COUNT);
    else if (s != 0 && ui.menu >= 2) {             /* bounded document scrolling */
        ui.menu_scroll = (uint16_t)clamp((int32_t)ui.menu_scroll + clamp(s, -128, 128) * 18, 0, menu_scroll_max());
    }
    s = panel_enc(EN_K2);
    if (s != 0 && ui.menu == 1 && ui.menu_sel == MI_COLOR) {
        settings.palette = (settings.palette + (s > 0 ? 1u : NPALETTES - 1u)) % NPALETTES;   /* (wraps) */
        palette_set(settings.palette);              /* (the menu signature redraws) */
        ui.force = 1;
    }

#if MELODEE_USB_AUDIO
    if ((s != 0 || ok) && ui.menu == 1 && ui.menu_sel == MI_USB_INPUT) {
        ua_tracks_set(s > 0 ? 1 : s < 0 ? 0 : !ua_tracks_want, fm1_ms);
        ok = 0;
    }
    if ((s != 0 || ok) && ui.menu == 1 && ui.menu_sel == MI_USB) {
        /* KNOB 2 steps IN+OUT IN OUT OFF, OCT+ steps and wraps; the host gets it once it rests (main.c) */
        uint32_t v = ua_off_want & 3u;
        v = s > 0 ? (v < 3u ? v + 1u : 3u) : s < 0 ? (v ? v - 1u : 0u) : (v + 1u) % 4u;
        ua_off_set(UA_OFF_OUT, !(v & UA_OFF_OUT), fm1_ms);
        ua_off_set(UA_OFF_IN, !(v & UA_OFF_IN), fm1_ms);
        ok = 0;
    }
#endif
    if ((s != 0 || ok) && ui.menu == 1 && ui.menu_sel == MI_LIGHTS) {
        /* KNOB 2 steps OFF .. FULL, OCT+ steps and wraps (saved when the menu closes) */
        uint32_t v = settings_lights % LIGHTS_N;
        settings_lights = (uint8_t)(s > 0 ? (v + 1u < LIGHTS_N ? v + 1u : v) : s < 0 ? (v ? v - 1u : 0u) : (v + 1u) % LIGHTS_N);
        ok = 0;
    }
    if ((s != 0 || ok) && ui.menu == 1 && ui.menu_sel == MI_HOLD) {
        /* KNOB 2 steps 0.3 .. 0.6 s, OCT+ steps and wraps */
        uint32_t v = settings_hold % 4u;
        settings_hold = (uint8_t)(s > 0 ? (v < 3u ? v + 1u : 3u) : s < 0 ? (v ? v - 1u : 0u) : (v + 1u) % 4u);
        ok = 0;
    }
    if((s || ok) && ui.menu==1 && ui.menu_sel>=MI_CLICK && ui.menu_sel<=MI_ADD){
        uint32_t id=ui.menu_sel-MI_CLICK,top=id<3?2:1;
        uint32_t value=id==0?settings_click:id==1?settings_click_level:id==2?settings_countin:id==3?settings_preview:settings_chord_add;
        value=s>0?(value<top?value+1:value):s<0?(value?value-1:0):(value+1)%(top+1);
        if(id==0)settings_click=value;else if(id==1)settings_click_level=value;else if(id==2)settings_countin=value;else if(id==3)settings_preview=value;else settings_chord_add=value;
        ok=0;ui.force=1;
    }
    if ((s || ok) && ui.menu == 1 && ui.menu_sel == MI_AUTOSAVE) {
        settings_autosave = (uint8_t)(s > 0 ? 1u : s < 0 ? 0u : !settings_autosave);
        ui.force = 1; ok = 0;
    }
    if ((s || ok) && ui.menu==1 && ui.menu_sel==MI_ACCEL) {   /* KNOB 2 right ON, left OFF; OCT+ toggles */
        uint32_t on = s > 0 ? 1u : s < 0 ? 0u : !!(PREF_BITS & PREF_ACCEL_OFF);
        PREF_BITS = (uint8_t)(on ? PREF_BITS & ~PREF_ACCEL_OFF : PREF_BITS | PREF_ACCEL_OFF);
        ui.force=1; ok=0;
    }
    if ((s || ok) && ui.menu == 1 && ui.menu_sel == MI_TRACK_KNOB) {   /* KNOB 2 right RIGHT, left LEFT; OCT+ toggles */
        settings_knob_swap = (uint8_t)(s > 0 ? 0u : s < 0 ? 1u : !settings_knob_swap);
        ui.force = 1; ok = 0;                          /* (from the next turn: the menu itself uses KNOB 1 / 2) */
    }
    if ((s || ok) && ui.menu == 1 && ui.menu_sel == MI_MIDI_OUT) {   /* KNOB 2 steps KEYS ALL OFF, OCT+ wraps */
        uint32_t v = settings_midi_out % MIDI_OUT_N;
        settings_midi_out = (uint8_t)(s > 0 ? (v + 1u < MIDI_OUT_N ? v + 1u : v) : s < 0 ? (v ? v - 1u : 0u) : (v + 1u) % MIDI_OUT_N);
        ui.force = 1; ok = 0;                          /* (notes already sent still send their note-offs) */
    }
    if ((s || ok) && ui.menu == 1 && ui.menu_sel == MI_CLOCK_OUT) {  /* KNOB 2 right ON, left OFF; OCT+ toggles */
        settings_clock_out = (uint8_t)(s > 0 ? 1u : s < 0 ? 0u : !settings_clock_out);
        ui.force = 1; ok = 0;                          /* (on: from the next PLAY; off: STOP goes out now) */
    }
    if ((s || ok) && ui.menu==1 && ui.menu_sel==MI_SCREEN) {
        uint32_t value=scr_get(), top=4u;
        value=s>0?(value<top?value+1u:value):s<0?(value?value-1u:0u):(value+1u)%(top+1u);
        scr_put(value);
        ui.force=1; ok=0;
    }
    if (ok && ui.menu == 1) {
        switch (ui.menu_sel) {
        case MI_COLOR:                                 /* OCT+ steps through the palettes too */
            settings.palette = (settings.palette + 1u) % NPALETTES;
            palette_set(settings.palette);
            ui.force = 1;
            break;
        case MI_PANEL:
            panel_setup();
            ui.force = 1;
            break;
        case MI_DESIGNER:
            menu_close();designer_open();
            break;
        case MI_ABOUT:
            ui.menu = 2;
            ui.menu_scroll = 0;
            ui.force = 1;
            break;
        default:
            menu_close();
            break;
        }
    }
    enc_drop();                                        /* swallow the rest while the menu is up */
}
