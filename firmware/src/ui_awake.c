/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* AWAKE's step entry (as norns' Awake: A, the upper loop, plays; B, the lower, transposes it). On GEN with AWAKE the
 * keys are its steps, silent, as the DRUM grid's: a white key picks that step of A, with black key 9 (the grid's ACC)
 * held, of B; the same key again lets it go. KNOB 1 (MODE otherwise) sets the step picked: A a rest or a degree 1..8,
 * B a transposition 0..7 (tessera.c). The white keys' LEDs show A's notes (B's values while black key 9 is held), the
 * step playing inverted, the step picked lit. Included by ui_draw.c */
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
            tessera_awake_select(song.sel, (uint32_t)black_held(GK_ACC), key_place(k));
            ui.force = 1;
        }
}
static uint32_t awake_leds(void)
{
    const gen_awake_t *a = tessera_awake(song.sel);
    uint32_t k, m = 0, b = (uint32_t)black_held(GK_ACC), len = b ? a->len_b : a->len_a;
    uint32_t ph = song.playing && len ? ((b ? a->pos_b : a->pos_a) + len - 1u) % len : 0xFFu;
    for (k = 0; k < 27u; k++) {
        uint32_t p = key_place(k), on;
        if (!key_black(k)) {
            on = p < len && (b ? gen_awake_b(a, p) : gen_awake_a(a, p)) != 0u;
            on ^= (uint32_t)(p == ph);
            if (a->edit && (a->edit & 15u) == p && ((a->edit >> 4) & 1u) == b) on = 1;
        } else {
            on = p == GK_ACC && b;
        }
        m |= on << k;
    }
    return m;
}
