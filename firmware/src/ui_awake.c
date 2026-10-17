/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* AWAKE's step entry (as norns' Awake: A, the upper loop, plays; B, the lower, transposes it). On GEN with AWAKE the
 * keys are its steps, silent, as the DRUM grid's: a white key picks that step, the same key again lets it go. While a
 * step is picked KNOB 1 (MODE otherwise) sets its A, a rest or a degree 1..8, and KNOB 2 (LEN A otherwise) its B, a
 * lift of 0..7 (tessera.c). The white keys' LEDs show A's notes, the step playing inverted, the step picked lit.
 * Included by ui_draw.c */
static int awake_entry_on(void)
{
    return !ui.home && !ui.menu && !ui.confirm && song.sel < TESSERA_TRACKS && cur_page()->fam == FAM_GEN &&
           cur_page()->graph == GR_STEPS && tessera.mode[song.sel] == TESS_ENG_AWAKE;
}
static void awake_keys(uint32_t pressed)
{
    uint32_t k;
    for (k = 0; k < 27u; k++)
        if (((pressed >> k) & 1u) && !key_black(k) && key_place(k) < 16u) {
            tessera_awake_select(song.sel, key_place(k));
            ui.force = 1;
        }
}
static uint32_t awake_leds(void)
{
    const gen_awake_t *a = tessera_awake(song.sel);
    uint32_t k, m = 0, len = a->len_a;
    uint32_t ph = song.playing && len ? (a->pos_a + len - 1u) % len : 0xFFu;
    for (k = 0; k < 27u; k++) {
        uint32_t p = key_place(k), on;
        if (key_black(k)) continue;
        on = p < len && gen_awake_a(a, p) != 0u;
        on ^= (uint32_t)(p == ph);
        if (a->edit && (a->edit & 15u) == p) on = 1;
        m |= on << k;
    }
    return m;
}
