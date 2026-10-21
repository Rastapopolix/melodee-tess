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
    static const uint8_t TW2[4] = {TX_TW_BRANCH, TX_TW_BR_TO, TX_TW_LOGIC, TX_TW_COND};
    static const uint8_t NL[4] = {TX_NL_MODE, TX_NL_LEN, TX_NL_KEYS, 0xFF};   /* (GATE: PATTERN's, below) */
    static const uint8_t ENG[TESS_ENG_COUNT][3] = {{0xFF, 0xFF, 0xFF}, {TX_TUR_LEN, TX_TUR_PROB, TX_TUR_SPAN},
        {TX_EUC_STEPS, TX_EUC_PULSES, TX_EUC_ROT}, {TX_MKV_LEAP, TX_MKV_DENS, TX_MKV_SPAN}, {TX_MRP_POS, TX_MRP_SNPA, TX_MRP_SNPB},
        {TX_GRD_X, TX_GRD_Y, TX_GRD_DENS}, {TX_BNC_SPEED, TX_BNC_ANGLE, TX_BNC_BALLS},
        {TX_AWK_LENA, TX_AWK_LENB, TX_AWK_PROB}, {0xFF, TX_POL_DIV1, TX_POL_SPAN},
        {TX_QD_DIR, TX_QD_PROB, TX_QD_LINK}, {TX_CS_Y, TX_CS_DENS, TX_CS_AUTO},
        {TX_SR_START, TX_SR_INTV, TX_SR_LOOP}, {TX_CH_PROG, TX_CH_BARS, TX_CH_HIT}};
    static const char *const CORD_TEXT[4] = {
        "What moves it: a track's LFO, envelope, velocity, key, random, controllers, its generator's gate, note "
        "or CV, or DRIFT, a slow wander, or its SLOTH (TORPOR, APATHY, INERTIA) or OCHD 1-8 (LFO > ORGANIC). -- leaves "
        "the mod a fixed move. OCT+: BI swings both ways from the knob, UNI (+) only one way.",
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
    if (pg->graph == GR_MATRIX) {                       /* (ui_matrix.c) */
        static const char *const MX_NAME[4] = {"MATRIX FROM", "MATRIX TO", "MATRIX MOD", "MATRIX OPEN"};
        static const char *const MX_TEXT[4] = {
            "The track the mods come from: a row of the grid. A lit cell: a mod goes from this track to that one, as "
            "bright as it moves its destination now.",
            "The track the mods go to: a column of the grid.",
            "Which of the cell's mods, when it has more than one (its number in MODS).",
            "OCT+ opens that mod in MODS, to change it."};
        help.name = MX_NAME[k];
        help.text = MX_TEXT[k];
        return;
    }
    if (pg->graph == GR_EVOLVE) x = EV[k];
    else if (pg->graph == GR_TWEAK) x = TW[k];
    else if (pg->graph == GR_TWEAK2) x = TW2[k];
    else if (pg->graph == GR_NOTELEN) x = NL[k];
    else if (pg->graph == GR_ORGANIC && k < 2u) x = k ? TX_ORG_RATE : TX_ORG_SPEED;
    else if (pg->graph == GR_FORM) {
        x = FM[k];
        if (x == 0xFFu) {
            help.name = "FORM CLEAR";
            help.text = "Forgets sections B and C: the phrase playing is A again.";
            return;
        }
    } else if (pg->graph == GR_STEPS && pg->fam == FAM_GEN && tr < TESSERA_TRACKS && (k == 0u || tessera.mode[tr] != TESS_ENG_OFF))
    {
        if (!k && quad_cols_on()) {
            help.name = "QUAD COL";
            help.text = "Which of the grid's four columns KNOB 2 (MOVE) and KNOB 3 (EVERY) set. The picture marks it.";
            return;
        }
        x = k == 0u ? (uint32_t)TX_MODE : ENG[tessera.mode[tr] % TESS_ENG_COUNT][k - 1u];
        if (tessera.mode[tr] == TESS_ENG_AWAKE && k <= 1u && tessera_awake(tr)->edit) {   /* (a step picked) */
            help.name = k ? "AWAKE STEP B" : "AWAKE STEP A";
            help.text = k ? "The picked step of loop B: it lifts A's note by 0 to 7 degrees of the scale. The same key "
                            "again lets the step go (KNOB 2 is LEN A again)."
                          : "The picked step of loop A: a rest, or a degree of the scale, 1 to 8. KNOB 2 sets the same "
                            "step of B. The same key again lets the step go (KNOB 1 is MODE again).";
            return;
        }
        if (tessera.mode[tr] == TESS_ENG_POLY && k == 1u) {   /* (RHY: the page's choice, no setting of its own) */
            help.name = "POLY RHY";
            help.text = "Which of the four rhythms DIV sets.";
            return;
        }
        if (k && quad_cols_on()) {                      /* (QUAD, OCT+ held: the columns' and RANGE) */
            uint32_t c = tessera.qcol_sel & 3u;
            help.pid = (uint16_t)PID_TESS(k == 1u ? TX_QD_MOVE1 + c : k == 2u ? TX_QD_EVERY1 + c : TX_QD_RANGE);
            return;
        }
        if (ch_layer_on()) {                            /* (CHORD, OCT+ held: OCT, the track's CHRD and VOIC) */
            if (k < 3u) help.pid = (uint16_t)(k == 0u ? PID_TESS(TX_CH_OCT) : k == 1u ? PID_TRACK(P_CHRD) : PID_TRACK(P_VOIC));
            else if (tessera_chord(tr)->prog == GEN_CH_USER) help.pid = (uint16_t)PID_TESS(TX_CH_ULEN);
            return;
        }
        if (gr_layer_on()) {                            /* (GRIDS, OCT+ held: KICK, SNR, HAT, TOMS) */
            help.pid = (uint16_t)PID_TESS(TX_GRD_KICK + (k & 3u));
            return;
        }
        if (cs_layer_on()) {                            /* (CONSTELLATION, OCT+ held: LOCK, LEN, FILL, SIZE) */
            static const uint8_t CSL[4] = {TX_EV_LOCK, TX_CS_LEN, TX_CS_FILL, TX_CS_SIZE};
            help.pid = (uint16_t)PID_TESS(CSL[k & 3u]);
            return;
        }
        if (tessera.mode[tr] == TESS_ENG_CONSTEL && k == 3u) {   /* (SPEED: the track's DIV, in another order) */
            help.name = "CONSTELLATION SPEED";
            help.text = "How fast the sky flies by: a column a step, so this is the track's DIV (PATTERN's too), from 4 "
                        "bars a step to 1/32, straight divisions only, so the notes land on the beat. Hold OCT+ for "
                        "LOCK, LEN, FILL and SIZE.";
            return;
        }
        if (tessera.mode[tr] == TESS_ENG_POLY && k == 2u) x += tessera_poly(tr)->sel & 3u;
    }
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
