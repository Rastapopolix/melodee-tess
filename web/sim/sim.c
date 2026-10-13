/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments; Theo Dainis (melodee-tess) */
/* Web simulator: the firmware (sound, sequencer, Tessera, UI) on tests/ui_test.c's HAL stubs, built to
 * WebAssembly by web/sim/build.sh and run inside an AudioWorklet (web/sim/sim-worklet.js). Audio time drives
 * the clock, as on the device; the UI runs a frame every 16 ms of audio.
 * Not simulated: flash (SAVE / projects are not kept), USB, the CPU budget (check cost with the host tests). */
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

#define SIM_BLOCK 128u                       /* an AudioWorklet render quantum */
static float sim_out[2u * SIM_BLOCK];
static uint8_t sim_led[41];                  /* per key id (0..13 buttons by matrix id, 14..40 keys): 0 off, 1 dim, 2 lit */
static uint64_t sim_frames, sim_sent;

__attribute__((used)) void sim_init(void)
{
    ui_power_on();
    settings.palette = UI_DEFAULT_INDEX;     /* (ui_power_on: the tests' GRAY) */
    palette_set(settings.palette);
    song.master_q12 = 2048;                  /* main.c melodee_init; the volume knob on the device */
    screen_clear();
    ui.force = 1;
}

/* the demo: a Tessera generator on each track, as KNOB 1 on SEQ > PATTERN sets it, EVOLVE on track 1, then PLAY */
__attribute__((used)) void sim_demo(void)
{
    static const uint8_t GEN[4] = {TESS_ENG_TURING, TESS_ENG_MORPH, TESS_ENG_MARKOV, TESS_ENG_EUCLID};
    uint32_t tr, sel = song.sel;
    for (tr = 0; tr < 4u; tr++) {
        song.sel = (uint8_t)tr;
        if (tessera.mode[tr] == TESS_ENG_OFF)
            tessera_knob(0, GEN[tr]);
    }
    song.sel = 0;
    tessera_evolve_knob(1, 1);               /* EVERY: one setting a turn, OFF -> 1 -> 2 bars */
    tessera_evolve_knob(1, 1);
    tessera_evolve_knob(2, -30);             /* AMOUNT: 60 -> 30 % */
    song.sel = (uint8_t)sel;
    ui.force = 1;
    if (!song.playing)
        transport_req = 1;
}

/* n frames (at most SIM_BLOCK, a multiple of CTL) into sim_out, interleaved L R */
__attribute__((used)) float *sim_render(uint32_t n)
{
    static int32_t o[2u * CTL];
    uint32_t i, j;
    for (i = 0; i < n; i += CTL) {
        mix_block(o, CTL);
        for (j = 0; j < CTL; j++) {
            if (j & 1u)
                scope_buf[scope_w++ & (SCOPE_N - 1u)] = (int16_t)o[2u * j];   /* the HOME scope (audio.c audio_block) */
            sim_out[2u * (i + j)] = (float)o[2u * j] * (1.0f / 32768.0f);
            sim_out[2u * (i + j) + 1u] = (float)o[2u * j + 1u] * (1.0f / 32768.0f);
        }
    }
    sim_frames += n;
    fm1_ms = (uint32_t)(sim_frames * 1000u / FS);
    host_ticks = (uint32_t)(sim_frames * 1000000u / FS);
    return sim_out;
}

/* one UI frame (main.c's loop body): 1 if the screen changed */
__attribute__((used)) int sim_frame(void)
{
    uint32_t c, r, id;
    ui_input();
    ui_leds();
    ui_draw();
    memset(sim_led, 0, sizeof sim_led);
    for (c = 0; c < FM1_NCOL; c++)
        for (r = 1; r < 5u; r++)
            if ((id = (uint32_t)FM1_KEYMAP[r][c]) < 41u) {
                uint32_t b = 1u << r;
                sim_led[id] = (fm1_led[c] & b) ? 2 :
                              ((fm1_led_dim[0][c] | fm1_led_dim[1][c] | fm1_led_dim[2][c]) & b) ? 1 : 0;
            }
    if (host_sent == sim_sent)
        return 0;
    sim_sent = host_sent;
    return 1;
}

__attribute__((used)) uint16_t *sim_screen(void) { return host_screen; }   /* 240 x 240 RGB565, high byte first */
__attribute__((used)) uint8_t *sim_leds(void) { return sim_led; }
__attribute__((used)) uint32_t sim_button_id(uint32_t label) { return label < NB ? panel.btn[label] : 255u; }

__attribute__((used)) void sim_button(uint32_t label, int down)   /* label: B_FX .. B_OCTUP */
{
    uint32_t b;
    if (label >= NB)
        return;
    b = 1u << panel.btn[label];
    if (down) {
        fm1_in.buttons |= b;
        host_pressed |= b;
    } else {
        fm1_in.buttons &= ~b;
    }
}

__attribute__((used)) void sim_key(uint32_t k, int down)         /* k: 0..26, F (53) .. G (79) */
{
    if (k >= 27u)
        return;
    if (down) {
        fm1_in.notes |= 1u << k;
        host_notes |= 1u << k;
    } else {
        fm1_in.notes &= ~(1u << k);
    }
}

__attribute__((used)) void sim_turn(uint32_t role, int32_t steps)   /* role: EN_SELECT .. EN_K4, + = clockwise */
{
    if (role < NE)
        host_enc[panel.enc[role]] += steps * panel.dir[role];
}

__attribute__((used)) void sim_midi(uint32_t status, uint32_t d1, uint32_t d2)   /* a short message (Web MIDI) */
{
    if (status < 0x80u || status >= 0xF0u)
        return;
    midi_in_q[mi_w % MQ] = (status >> 4) | status << 8 | (d1 & 127u) << 16 | (d2 & 127u) << 24;
    mi_w++;
}
