/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* Help (Theo 2026-10-16; tessera-notes/docs/design/phase2/help.png): ARP held, a knob turned shows what it does, its value left as it
 * is: a card over the page, dimmed, with the parameter's name, its value and its text (pid.c). Letting ARP go closes it
 * (and opens no page: ui_input.c). The FM-1's knobs cannot sense a touch, so a turn it is. Included by ui_pages.c. */

static struct {
    uint8_t on, knob;
    uint16_t pid;                                       /* PID_NONE: name and text below (CORDS, FORM's CLEAR) */
    const char *name, *text;
} help;

/* the parameter knob k reaches on the page shown: Tessera's pages by their knobs, CORDS and CLEAR by words, the others
 * by where the knob's value lives (a track's p, the globals) */
static void help_find(uint32_t k)
{
    static const uint8_t EV[4] = {TX_EV_LOCK, TX_EV_EVERY, TX_EV_AMOUNT, TX_EV_SEED};
    static const uint8_t FM[4] = {TX_FM_FORM, TX_FM_CONTRAST, 0xFF, TX_FM_FLOURISH};
    static const uint8_t TW[4] = {TX_TW_SHIFT, TX_TW_DROP, TX_TW_HUMAN, TX_TW_FOLLOW};
    static const uint8_t ENG[TESS_ENG_COUNT][3] = {{0xFF, 0xFF, 0xFF}, {TX_TUR_LEN, TX_TUR_PROB, TX_TUR_SPAN},
        {TX_EUC_STEPS, TX_EUC_PULSES, TX_EUC_ROT}, {TX_MKV_LEAP, TX_MKV_DENS, TX_MKV_SPAN}, {TX_MRP_POS, TX_MRP_SNPA, TX_MRP_SNPB},
        {TX_GRD_X, TX_GRD_Y, TX_GRD_DENS}, {TX_BNC_SPEED, TX_BNC_ANGLE, TX_BNC_PADDLE}};
    static const char *const CORD_TEXT[4] = {
        "What moves it: a track's LFO, envelope, velocity, key, random, controllers, its generator's gate, note "
        "or CV, or DRIFT, a slow wander. -- leaves the mod a fixed move.",
        "Which of the 16 mods the other knobs edit.",
        "What it moves, on which track: the voices' pitch, cutoff, shape, gain, a sound setting, or a generator's.",
        "How far, and which way: at a full source, 100 % is the destination's whole range (pitch: an octave)."};
    static const char *const CORD_NAME[4] = {"MOD SOURCE", "MOD", "MOD DESTINATION", "MOD AMOUNT"};
    const page_t *pg = ui.home ? stage_page() : cur_page();
    uint32_t tr = song.sel, x = 0xFFu;
    int16_t *vp;
    help.pid = PID_NONE;
    help.name = help.text = 0;
    k &= 3u;
    if (pg->graph == GR_CORDS) {
        help.name = CORD_NAME[k];
        help.text = CORD_TEXT[k];
        return;
    }
    if (pg->graph == GR_EVOLVE) x = EV[k];
    else if (pg->graph == GR_TWEAK) x = TW[k];
    else if (pg->graph == GR_FORM) {
        x = FM[k];
        if (x == 0xFFu) {
            help.name = "FORM CLEAR";
            help.text = "Forgets sections B and C: the phrase playing is A again.";
            return;
        }
    } else if (pg->graph == GR_STEPS && pg->fam == FAM_GEN && tr < TESSERA_TRACKS && (k == 0u || tessera.mode[tr] != TESS_ENG_OFF))
        x = k == 0u ? (uint32_t)TX_MODE : ENG[tessera.mode[tr] % TESS_ENG_COUNT][k - 1u];
    if (x != 0xFFu) {
        help.pid = (uint16_t)PID_TESS(x);
        return;
    }
    if (page_desc(pg, k, &vp) && vp) {
        if (vp >= TSEL->p && vp < TSEL->p + P_COUNT) help.pid = (uint16_t)PID_TRACK(vp - TSEL->p);
        else if (vp >= song.g && vp < song.g + G_COUNT) help.pid = (uint16_t)PID_GLOBAL(vp - song.g);
        else if (vp == &tuning_a4) help.pid = (uint16_t)PID_GLOBAL(G_A4);
    }
    if (help.pid == PID_NONE || !pid_find(help.pid)) {   /* (an engine's own panel: its label alone, for now) */
        const param_desc_t *d = page_desc(pg, k, &vp);
        help.pid = PID_NONE;
        help.name = d && d->label && d->label[0] ? d->label : "KNOB";
        help.text = "No help text for this one yet.";
    }
}

/* ui_input.c: a knob turned with ARP held; 1 = its help shown (the turn does nothing else) */
static int help_knob(uint32_t k)
{
    if (!(fm1_in.buttons & (1u << panel.btn[B_ARP])) || ui.menu || ui.confirm || ui.layer)   /* (a popup: ui_input.c) */
        return 0;
    help_find(k);
    help.knob = (uint8_t)k;
    help.on = 1;
    ui.force = 1;
    return 1;
}

/* the card: the name in the track's colour, the value right, the text wrapped to its width. 220 x 168 at (10, 40): two
 * strips (a canvas holds 240 x 124 at most), each drawing the whole card at cv_oy = -its top */
static void help_strip(int32_t top, int32_t h, const char *name, const char *val, const char *text)
{
    char line[64];
    int32_t y = 34, w = 196;
    uint32_t i, n, start;
    cv_begin(220, (uint32_t)h, T_BG);
    cv_oy = -top;                                       /* (as ui_motion.c: the card drawn whole, this strip of it) */
    cv_rrect(0, 0, 220, 168, 8, T_THEME, T_BG);
    cv_rrect(1, 1, 218, 166, 7, T_SURF, T_THEME);
    cv_free_text(12, 10, &AF_S, name ? name : "", T_THEME, T_SURF, 140);
    if (val[0]) cv_text_r(208, 12, &AF_X, val, T_TEXT, T_SURF);
    for (i = 0, start = 0, n = 0; text && y < 140; ) {   /* words to lines of w px */
        uint32_t end = i;
        while (text[end] && text[end] != ' ') end++;
        if (end - start < sizeof line) {
            memcpy(line, text + start, end - start);
            line[end - start] = 0;
        }
        if ((end - start >= sizeof line || text_w(&AF_X, line) > w) && n) {
            memcpy(line, text + start, n - start);
            line[n - start] = 0;
            cv_text_on(12, y, &AF_X, line, T_TEXT, T_SURF);
            y += 14;
            start = i = n + 1u;
            n = 0;
            continue;
        }
        if (!text[end]) {
            cv_text_on(12, y, &AF_X, line, T_TEXT, T_SURF);
            break;
        }
        n = end;
        i = end + 1u;
    }
    cv_rect(12, 144, 196, 1, T_LINE);
    cv_free_text(12, 149, &AF_X, "ARP held: turn a knob for its help", T_DIM, T_SURF, 196);
    cv_oy = 0;
    cv_blit(10, (uint32_t)(40 + top));
}
/* the card's value: as the knobs' columns show it, a name whole ("TURING": the columns cut one to 5) */
static void help_value(const pid_entry_t *e, char *val, uint32_t size)
{
    const param_desc_t *d = e && !(e->flags & PID_F_ACTION) ? pid_desc(e->pid, song.sel) : 0;
    const char *unit = "";
    int32_t v;
    val[0] = 0;
    if (!d)
        return;
    v = pid_get(e->pid, song.sel);
    param_format(d, v, val, &unit);
    if (d->fmt == F_ENUM && d->names && d->names != N_ROUTE)
        str_cpy(val, d->names[clamp(v, d->min, d->max)], size);
    if (unit[0] && str_len(val) + str_len(unit) + 2u < size) {
        str_cpy(val + str_len(val), " ", 2);
        str_cpy(val + str_len(val), unit, 8);
    }
}
static void help_draw(void)
{
    const pid_entry_t *e = help.pid != PID_NONE ? pid_find(help.pid) : 0;
    char val[24];
    if (!ui.force)
        return;
    help_value(e, val, sizeof val);
    help_strip(0, 112, e ? e->name : help.name, val, e ? e->help : help.text);
    help_strip(112, 56, e ? e->name : help.name, val, e ? e->help : help.text);
}
