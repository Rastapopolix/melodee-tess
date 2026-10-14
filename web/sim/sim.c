/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments; Theo Dainis (melodee-tess) */
/* Web simulator: the firmware (sound, sequencer, Tessera, UI) on tests/ui_test.c's HAL stubs, built to
 * WebAssembly by web/sim/build.sh. It exports Felucca's browser interface (web/emu/felucca_web.c of
 * hugelton/Felucca: web_render, web_buttons, web_keys, web_enc, web_master, web_midi, the LEDs, the screen), so
 * Felucca's page and worklet (web/sim/index.html, worklet.js) drive it unchanged: the same FM-1 drawing as the
 * other FM-1 browser builds. Audio time drives the clock: each millisecond of the device renders the audio due and
 * runs main.c's loop (a frame every 15 ms, ui_input between).
 * Not simulated: flash (saves are lost on reload; web_flash_writes_count stays 0), USB, the CPU budget. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

/* the real matrix (hal/fm1_input.h; ui_test.c leaves it empty): the LED places of the keys and buttons */
static const int8_t FM1_KEYMAP[6][FM1_NCOL] = {
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 5, 11,  4, 10,  3,  9,  2,  8, -1, -1, -1},
    {34, 35, 36, 37, 38, 40, 39, 13,  7,  6, 12},
    {23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33},
    { 0,  1, 15, 14, 17, 16, 19, 18, 20, 21, 22},
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
};

#undef __attribute__                         /* (tests/hostsim.c defines it away) */
#define EXPORT __attribute__((used, visibility("default")))

static uint32_t web_booted, web_last_frame, web_samples_due;

/* power-on (main.c melodee_init via ui_test.c's ui_power_on), in the default palette */
static void web_power_on(void)
{
    ui_power_on();
    settings.palette = UI_DEFAULT_INDEX;     /* (ui_power_on: the tests' GRAY) */
    palette_set(settings.palette);
    song.master_q12 = 2048;                  /* until the page sends MASTER */
    screen_clear();
    ui.force = 1;
    web_last_frame = fm1_ms - 15u;
    web_booted = 1;
}

/* one pass of main.c's loop: what the browser has (no USB, editor, UBOOT, flash) */
static void web_frame(void)
{
    ui_input();
    ui_leds();
    ui_draw();
}

/* output ring, stereo float: filled a block of CTL at a time as the device's milliseconds pass, drained by
 * web_render. The HOME scope is fed as audio.c audio_block feeds it. */
#define RING 4096u
static float ring[RING * 2];
static uint32_t ring_w, ring_r;

static void web_ms(void)
{
    fm1_ms++;
    host_ticks += 1000u;
    web_samples_due += FS;
    while (web_samples_due >= 1000u * CTL) {
        static int32_t o[2u * CTL];
        uint32_t k;
        web_samples_due -= 1000u * CTL;
        mix_block(o, CTL);
        for (k = 0; k < CTL; k++) {
            uint32_t i = (ring_w++ % RING) * 2u;
            if (k & 1u)
                scope_buf[scope_w++ & (SCOPE_N - 1u)] = (int16_t)o[2u * k];
            ring[i] = (float)o[2u * k] * (1.0f / 32768.0f);
            ring[i + 1u] = (float)o[2u * k + 1u] * (1.0f / 32768.0f);
        }
    }
    if (!web_booted)
        return;
    if (fm1_ms - web_last_frame >= 15u) {        /* main.c: a frame, then ui_input until 15 ms are over */
        web_last_frame = fm1_ms;
        web_frame();
    } else {
        ui_input();
    }
}

static float out_l[1024], out_r[1024];
EXPORT float *web_out_l(void) { return out_l; }
EXPORT float *web_out_r(void) { return out_r; }
/* n frames (<= 1024) into out_l / out_r: the device runs until they exist */
EXPORT void web_render(uint32_t n)
{
    uint32_t k;
    if (n > 1024u)
        n = 1024u;
    while (ring_w - ring_r < n)
        web_ms();
    for (k = 0; k < n; k++) {
        uint32_t i = (ring_r++ % RING) * 2u;
        out_l[k] = ring[i];
        out_r[k] = ring[i + 1u];
    }
}

/* no flash here: the page's saved sectors are ignored, nothing is ever written */
static uint8_t nor[4096];
EXPORT uint8_t *web_nor(void) { return nor; }
EXPORT uint32_t web_nor_size(void) { return 0; }
EXPORT void web_nor_erase(void) { memset(nor, 0xFF, sizeof nor); }
EXPORT uint32_t web_flash_writes_count(void) { return 0; }
EXPORT void web_boot(void)
{
    if (!web_booted)
        web_power_on();
}

/* input, by the printed labels: buttons bit b = B_FX .. B_OCTUP (panel.c), keys bit k = key k (F3 .. G5),
 * encoders by role (EN_SELECT .. EN_K4), MASTER 0..1023 (main.c: the pot, squared) */
EXPORT void web_buttons(uint32_t labels)
{
    uint32_t m = 0, b;
    for (b = 0; b < NB; b++)
        if ((labels >> b) & 1u)
            m |= 1u << panel.btn[b];
    host_pressed |= m & ~fm1_in.buttons;
    fm1_in.buttons = m;
}
EXPORT void web_keys(uint32_t mask)
{
    mask &= (1u << 27) - 1u;
    host_notes |= mask & ~fm1_in.notes;
    fm1_in.notes = mask;
}
EXPORT void web_enc(uint32_t role, int32_t n)
{
    if (role < NE)
        host_enc[panel.enc[role] % 7u] += n * panel.dir[role];
}
EXPORT void web_master(uint32_t v)
{
    uint32_t k10 = v > 1023u ? 1023u : v;
    song.master_q12 = (int32_t)((k10 * k10) >> 8);
}
/* a channel message from Web MIDI, as a USB-MIDI packet arrives: 0 = the queue was full */
EXPORT uint32_t web_midi(uint32_t status, uint32_t d1, uint32_t d2)
{
    if (status < 0x80u || status >= 0xF0u)
        return 1;
    if (mi_w - mi_r >= MQ)
        return 0;
    midi_in_q[mi_w % MQ] = (status >> 4) | status << 8 | (d1 & 127u) << 16 | (d2 & 127u) << 24;
    mi_w++;
    return 1;
}

/* the LEDs: bit b = the button labelled b, keys bit k. Lit: fm1_led; dim: the idle-key and idle-button planes;
 * breath: the third plane (ui_input.c ui_leds) */
static uint32_t led_on(const uint8_t *l, uint32_t id)
{
    uint32_t c, r;
    for (c = 0; c < FM1_NCOL; c++)
        for (r = 1; r < 5u; r++)
            if (FM1_KEYMAP[r][c] == (int8_t)id)
                return (l[c] >> r) & 1u;
    return 0;
}
static uint32_t led_buttons(const uint8_t *l)
{
    uint32_t b, m = 0;
    for (b = 0; b < NB; b++)
        m |= led_on(l, panel.btn[b]) << b;
    return m;
}
static uint32_t led_keys(const uint8_t *l)
{
    uint32_t k, m = 0;
    for (k = 0; k < 27u; k++)
        m |= led_on(l, 14u + k) << k;
    return m;
}
static uint8_t led_dim[FM1_NCOL];
static const uint8_t *web_dim_plane(void)
{
    uint32_t c;
    for (c = 0; c < FM1_NCOL; c++)
        led_dim[c] = (uint8_t)((fm1_led_dim[0][c] | fm1_led_dim[1][c]) & ~fm1_led[c]);
    return led_dim;
}
EXPORT uint32_t web_lit_buttons(void) { return led_buttons(fm1_led); }
EXPORT uint32_t web_lit_keys(void) { return led_keys(fm1_led); }
EXPORT uint32_t web_dim_buttons(void) { return led_buttons(web_dim_plane()); }
EXPORT uint32_t web_dim_keys(void) { return led_keys(web_dim_plane()); }
EXPORT uint32_t web_dim_level(void) { return 0; }
EXPORT uint32_t web_breath_buttons(void) { return led_buttons(fm1_led_dim[2]); }
EXPORT uint32_t web_breath_keys(void) { return led_keys(fm1_led_dim[2]); }

EXPORT uint16_t *web_screen(void) { return host_screen; }        /* 240 x 240 RGB565, the panel's byte order */
EXPORT uint32_t web_screen_draws(void) { return (uint32_t)host_sent; }
EXPORT uint32_t web_now_ms(void) { return fm1_ms; }
EXPORT uint32_t web_playing(void) { return song.playing; }
EXPORT uint32_t web_sample_rate(void) { return FS; }
/* (web/sim/test_sim.mjs) track tr's EVOLVE: EVERY << 8 | AMOUNT */
EXPORT uint32_t web_test_evolve(uint32_t tr)
{
    return tr < TESSERA_TRACKS ? (uint32_t)tessera.evolve[tr].every << 8 | tessera.evolve[tr].amount : 0u;
}

/* melodee-tess: the demo, a Tessera generator on each track (as KNOB 1 on SEQ > PATTERN sets it), EVOLVE on
 * track 1, then PLAY */
EXPORT void web_demo(void)
{
    static const uint8_t GEN[4] = {TESS_ENG_TURING, TESS_ENG_MORPH, TESS_ENG_MARKOV, TESS_ENG_EUCLID};
    uint32_t tr, sel = song.sel;
    for (tr = 0; tr < 4u; tr++) {
        song.sel = (uint8_t)tr;
        if (tessera.mode[tr] == TESS_ENG_OFF)
            tessera_knob(0, GEN[tr]);
    }
    song.sel = 0;
    tessera_evolve_knob(1, 1);               /* EVERY: one setting a turn, OFF -> 1 -> 2 bars; AMOUNT stays 30 % */
    tessera_evolve_knob(1, 1);
    song.sel = (uint8_t)sel;
    ui.force = 1;
    if (!song.playing)
        transport_req = 1;
}
