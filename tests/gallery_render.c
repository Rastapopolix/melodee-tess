/* SPDX-License-Identifier: GPL-3.0-only */
/* The README's Visual Reference Gallery and the manual's pictures: the real drawing code (tests/ui_render.c's scenes)
 * in the device build's engine set (FM6, TRIO, DRUM: no SID, PHYS, CZ-1 or Prophet), the default palette (SMALTI),
 * as PPM in OUTDIR/ppm (240 x 240; the PNGs in assets/gallery are these 2x, nearest: tools/gallery_png.py).
 *   cc -O1 -w -DMELODEE_USB_AUDIO=1 -DMELODEE_SID=0 -DMELODEE_PHYS=0 '-DMELODEE_VERSION="v2.0"' \
 *      -Ibuild/gen -Ifirmware/src -Itests -o build/gallery_render tests/gallery_render.c -lm
 *   mkdir -p build/gallery/ppm && build/gallery_render build/gallery
 * (README.md, MANUAL.md and the project page show these as assets/gallery/<name>.png) */
#define UI_RENDER_NO_MAIN 1
#include "ui_render.c"

enum { G_SCENE, G_TRIO, G_MODE_STEP, G_MODE_ARP, G_FORM, G_TIMING, G_SHEET, G_MIDIOUT };   /* (ARP: the same page, MODE
                                                     * Arp; TIMING: x4 ACCEL; SHEET: x3, +2/16; MIDI: GLO > MIDI OUT) */
typedef struct { const char *name; int kind; int scene; } gshot_t;
static const gshot_t SHOTS[] = {
    {"home", G_SCENE, S_HOME}, {"seq_mode", G_MODE_STEP, 0},
    {"gen_turing", G_SCENE, S_GEN_TURING}, {"gen_euclid", G_SCENE, S_GEN_EUCLID},
    {"gen_markov", G_SCENE, S_GEN_MARKOV}, {"gen_morph", G_SCENE, S_GEN_MORPH},
    {"gen_grids", G_SCENE, S_GEN_GRIDS}, {"gen_grids_oct", G_SCENE, S_GEN_GRIDS_OCT},
    {"gen_bounce", G_SCENE, S_GEN_BOUNCE}, {"gen_fall", G_SCENE, S_GEN_FALL},
    {"gen_awake", G_SCENE, S_GEN_AWAKE}, {"gen_poly", G_SCENE, S_GEN_POLY},
    {"gen_quad", G_SCENE, S_GEN_QUAD}, {"gen_constel", G_SCENE, S_GEN_CONSTEL},
    {"gen_series", G_SCENE, S_GEN_SERIES}, {"gen_chord", G_SCENE, S_GEN_CHORD},
    {"gen_chord_oct", G_SCENE, S_GEN_CHORD_OCT}, {"gen_chord_user", G_SCENE, S_GEN_CHORD_USER},
    {"gen_evolve", G_SCENE, S_GEN_EVOLVE}, {"step_evolve", G_SCENE, S_GEN_STEP_EVOLVE},
    {"form", G_FORM, 0}, {"gen_tweak", G_SCENE, S_GEN_TWEAK}, {"gen_tweak2", G_SCENE, S_GEN_TWEAK2},
    {"gen_length", G_SCENE, S_GEN_LENGTH},
    {"cords", G_SCENE, S_CORDS}, {"mods_live", G_SCENE, S_MODS_LIVE_GEN}, {"matrix", G_SCENE, S_MATRIX},
    {"organic", G_SCENE, S_ORGANIC}, {"mod_shortcut", G_SCENE, S_MOD_SHORTCUT},
    {"help", G_SCENE, S_HELP},
    {"edit_fm6", G_SCENE, S_FM6_ALG5}, {"fm6_eg", G_SCENE, S_FM6_EG}, {"edit_trio", G_TRIO, 0},
    {"drum", G_SCENE, S_DRUM}, {"patterns", G_SCENE, S_PATGRID}, {"mixer", G_SCENE, S_MIXER},
    {"gen_timing", G_TIMING, 0}, {"gen_energy", G_SCENE, S_GEN_ENERGY}, {"gen_answer", G_SCENE, S_GEN_ANSWER},
    {"sheet_note", G_SHEET, 0}, {"midi_out", G_MIDIOUT, 0}, {"about", G_SCENE, S_ABOUT},
};

int main(int argc, char **argv)
{
    const char *out = argc > 1 ? argv[1] : "build/gallery";
    uint32_t i;
    rep = fopen("/dev/null", "w");
    for (i = 0; i < NELEM(SHOTS); i++) {
        const gshot_t *g = &SHOTS[i];
        cur_name = g->name;
        if (g->kind == G_SCENE) setup(g->scene);
        else if (g->kind == G_MODE_STEP || g->kind == G_MODE_ARP) {   /* GEN: KNOB 1 is the track's SEQ MODE */
            setup(S_GEN_TURING);
            seqm_set(song.sel, g->kind == G_MODE_ARP);   /* 0 STEP, 1 ARP, then the generators */
            go_title("GEN");
        } else if (g->kind == G_FORM) {                 /* FORM: TURING's track, its sections */
            setup(S_GEN_TURING);
            go_title("FORM");
        } else if (g->kind == G_SHEET) {                /* a note's sheet: its step ratcheted x3, nudged +2/16, Ratchet's row */
            setup(S_REF_SHEET_NOTE);
            step_set_ratch(&TSEL->step[4], 3);
            step_set_nudge(&TSEL->step[4], 2);
            pop.sel = 3;
        } else if (g->kind == G_TIMING) {               /* TIMING: RATCH x4, every step, SHAPE ACCEL */
            setup(S_GEN_TIMING);
            tessera_param_set(0, TX_SUB_RATCH, 3);
            tessera_param_set(0, TX_SUB_CHANCE, 100);
            tessera_param_set(0, TX_SUB_SHAPE, SUB_ACCEL);
        } else if (g->kind == G_MIDIOUT) {                 /* MIDI OUT: track 1 on 1, 2 AUTO, 3 on 10, 4 OFF; ALL sent */
            setup(S_HOME);
            settings_midi_out = MIDI_OUT_ALL;
            tessera_param_set(0, TX_MIDI_CH, 1);
            tessera_param_set(2, TX_MIDI_CH, 10);
            tessera_param_set(3, TX_MIDI_CH, (int32_t)TESS_MIDI_OFF);
            go_title("MIDI OUT");
        } else { setup(S_HOME); eng(6u); go_title("EDIT 1"); }   /* TRIO */
        pal(UI_DEFAULT_INDEX);
        draw(g->kind == G_SCENE ? g->scene : -1);
        write_ppm(out, UI_PALETTES[UI_DEFAULT_INDEX].name, g->name);
    }
    printf("gallery_render: %u screens in %s\n", (unsigned)NELEM(SHOTS), out);
    return 0;
}
