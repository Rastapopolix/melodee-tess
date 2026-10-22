/* SPDX-License-Identifier: GPL-3.0-only */
/* The README's Visual Reference Gallery and the manual's pictures: the real drawing code (tests/ui_render.c's scenes)
 * in the device build's engine set (FM6, TRIO, DRUM: no SID, PHYS, CZ-1 or Prophet), the default palette (SMALTI),
 * as PPM in OUTDIR/ppm (240 x 240; the PNGs in assets/gallery are these 2x, nearest).
 *   cc -O1 -w -DMELODEE_USB_AUDIO=1 -DMELODEE_SID=0 -DMELODEE_PHYS=0 -DMELODEE_CZ=0 -DMELODEE_PROPHET=0 \
 *      -Ibuild/gen -Ifirmware/src -Itests -o build/gallery_render tests/gallery_render.c -lm
 *   mkdir -p build/gallery/ppm && build/gallery_render build/gallery
 * (README.md, MANUAL.md and the project page show these as assets/gallery/<name>.png) */
#define UI_RENDER_NO_MAIN 1
#include "ui_render.c"

enum { G_SCENE, G_TRIO, G_MODE_STEP, G_MODE_ARP, G_FORM };   /* (ARP: the same page, MODE Arp) */
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
    {"form", G_FORM, 0}, {"gen_tweak", G_SCENE, S_GEN_TWEAK}, {"gen_tweak2", G_SCENE, S_GEN_TWEAK2_COIN},
    {"gen_length", G_SCENE, S_GEN_LENGTH},
    {"cords", G_SCENE, S_CORDS}, {"mods_live", G_SCENE, S_MODS_LIVE_GEN}, {"matrix", G_SCENE, S_MATRIX},
    {"organic", G_SCENE, S_ORGANIC}, {"mod_shortcut", G_SCENE, S_MOD_SHORTCUT},
    {"help", G_SCENE, S_HELP_ZOOM},
    {"edit_fm6", G_SCENE, S_FM6_ALG5}, {"fm6_eg", G_SCENE, S_FM6_EG}, {"edit_trio", G_TRIO, 0},
    {"drum", G_SCENE, S_DRUM}, {"patterns", G_SCENE, S_PATGRID}, {"mixer", G_SCENE, S_MIXER},
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
        } else { setup(S_HOME); eng(6u); go_title("EDIT 1"); }   /* TRIO */
        pal(UI_DEFAULT_INDEX);
        draw(g->kind == G_SCENE ? g->scene : -1);
        write_ppm(out, UI_PALETTES[UI_DEFAULT_INDEX].name, g->name);
    }
    printf("gallery_render: %u screens in %s\n", (unsigned)NELEM(SHOTS), out);
    return 0;
}
