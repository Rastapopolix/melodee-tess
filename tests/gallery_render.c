/* SPDX-License-Identifier: GPL-3.0-only */
/* The README's Visual Reference Gallery: the real drawing code (tests/ui_render.c's scenes) in the release's
 * engine set (no FM6, CZ-1 or Prophet), the default palette (SMALTI), as PPM in OUTDIR/ppm (240 x 240; the README's PNGs are these 2x, nearest).
 *   cc -O1 -w -DMELODEE_USB_AUDIO=1 -DMELODEE_FM6=0 -DMELODEE_CZ=0 -DMELODEE_PROPHET=0 \
 *      -Ibuild/gen -Ifirmware/src -Itests -o build/gallery_render tests/gallery_render.c -lm
 *   mkdir -p build/gallery/ppm && build/gallery_render build/gallery
 * (README.md shows these as assets/gallery/<name>.png) */
#define UI_RENDER_NO_MAIN 1
#include "ui_render.c"

enum { G_SCENE, G_TRIO, G_PHYS, G_MODE_STEP, G_MODE_ARP, G_FORM };   /* (ARP: the same page, MODE Arp) */
typedef struct { const char *name; int kind; int scene; } gshot_t;
static const gshot_t SHOTS[] = {
    {"home", G_SCENE, S_HOME},
    {"seq_mode", G_MODE_STEP, 0},
    {"gen_turing", G_SCENE, S_GEN_TURING}, {"gen_euclid", G_SCENE, S_GEN_EUCLID},
    {"gen_markov", G_SCENE, S_GEN_MARKOV}, {"gen_morph", G_SCENE, S_GEN_MORPH},
    {"gen_grids", G_SCENE, S_GEN_GRIDS}, {"gen_bounce", G_SCENE, S_GEN_BOUNCE},
    {"gen_awake", G_SCENE, S_GEN_AWAKE}, {"gen_poly", G_SCENE, S_GEN_POLY},
    {"gen_evolve", G_SCENE, S_GEN_EVOLVE}, {"gen_tweak", G_SCENE, S_GEN_TWEAK},
    {"gen_tweak2", G_SCENE, S_GEN_TWEAK2}, {"organic", G_SCENE, S_ORGANIC},
    {"mods_live", G_SCENE, S_MODS_LIVE_GEN}, {"matrix", G_SCENE, S_MATRIX},   /* (1.3.3: TWEAK 2, ORGANIC, live mods) */
    {"mod_shortcut", G_SCENE, S_MOD_SHORTCUT}, {"cords", G_SCENE, S_CORDS},
    {"help", G_SCENE, S_HELP}, {"form", G_FORM, 0},
    {"edit_sid", G_SCENE, S_EDIT_SID}, {"edit_trio", G_TRIO, 0}, {"edit_phys", G_PHYS, 0},
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
        } else { setup(S_EDIT_SID); eng(g->kind == G_TRIO ? 6u : ENGI_PHYS); go_title("EDIT 1"); }
        pal(UI_DEFAULT_INDEX);
        draw(g->kind == G_SCENE ? g->scene : -1);
        write_ppm(out, UI_PALETTES[UI_DEFAULT_INDEX].name, g->name);
    }
    printf("gallery_render: %u screens in %s\n", (unsigned)NELEM(SHOTS), out);
    return 0;
}
