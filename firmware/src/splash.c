/* SPDX-License-Identifier: GPL-3.0-only */
/* The boot screen: Tessera's mark, a 3 x 3 mosaic of lapis tiles with one gold tessera, over TESSERA and a line.
 * While the firmware starts, the mosaic's bed shows (the nine places in the grout's colour); when it is ready,
 * splash_lay() sets the tiles from the middle outwards, one every SPL_STEP_MS, the gold one last with a press,
 * in the wait the boot already makes before the first frame, then holds the finished mark a moment (SPL_HOLD_MS;
 * a button or key pressed ends it sooner). The mark's colours are fixed (it is the logo),
 * the rest the palette's. Drawn with lcd_fill and draw_text_box only. */

#define SPL_U 22                                /* a tile, px */
#define SPL_G 4                                 /* the grout between two */
#define SPL_CX 120
#define SPL_CY 78                               /* the mosaic's centre */
#define SPL_STEP_MS 60u
#define SPL_HOLD_MS 800u                        /* the finished mark held (a press ends it sooner) */
#define SPL_GOLD 2u                             /* the gold tessera: top right */
static const uint16_t SPL_LAPIS[9] = {RGB(44, 64, 120), RGB(58, 84, 150), 0, RGB(66, 96, 170), RGB(50, 74, 136),
                                      RGB(40, 60, 112), RGB(62, 90, 160), RGB(46, 68, 126), RGB(54, 80, 146)};
#define SPL_GOLD_C RGB(236, 184, 72)
/* a hand-set look without rotation: each tile a pixel off the grid */
static const int8_t SPL_JIT[9][2] = {{0, 1}, {1, -1}, {-1, 0}, {1, 0}, {0, 0}, {-1, 1}, {0, -1}, {1, 1}, {-1, -1}};
static const uint8_t SPL_ORDER[9] = {4, 1, 3, 5, 7, 0, 6, 8, 2};   /* from the middle outwards, the gold one last */

static void spl_tile(uint32_t i, int32_t grow, uint16_t c)
{
    int32_t x = SPL_CX - SPL_U / 2 + ((int32_t)(i % 3u) - 1) * (SPL_U + SPL_G) + SPL_JIT[i][0] - grow;
    int32_t y = SPL_CY - SPL_U / 2 + ((int32_t)(i / 3u) - 1) * (SPL_U + SPL_G) + SPL_JIT[i][1] - grow;
    lcd_fill((uint32_t)x, (uint32_t)y, (uint32_t)(SPL_U + 2 * grow), (uint32_t)(SPL_U + 2 * grow), c);
}

static void splash(const char *line)          /* the boot screen: the mosaic's bed, TESSERA, then a line */
{
    uint32_t i;
    lcd_fill(0, 0, 240, 240, T_BG);
    for (i = 0; i < 9u; i++)
        spl_tile(i, 0, T_LINE);
    draw_text_box(0, 120, 240, &AF_L, "TESSERA", T_TEXT, 1);
    draw_text_box(0, 160, 240, &AF_S, line, T_MID, 1);
}

static void spl_wait(uint32_t ms)              /* (the boot's wait, the watchdog fed) */
{
    while (ms--) {
        fm1_delay_ms(1);
        if (!(ms & 31u)) fm1_wdt_feed();
    }
}

/* the finished mark held ms, the watchdog fed; a button or key pressed since (not one held from power-on) ends it */
static void spl_hold(uint32_t ms)
{
    uint32_t held = fm1_in.buttons | fm1_in.notes;
    while (ms--) {
        fm1_delay_ms(1);
        if (!(ms & 31u)) fm1_wdt_feed();
        if ((fm1_in.buttons | fm1_in.notes) & ~held) return;
    }
}

/* the tiles set, then at least `ms` gone in all (the boot's own wait); anim 0: the wait alone */
static void splash_lay(uint32_t ms, int anim)
{
    uint32_t k, used = 0;
    if (anim) {
        for (k = 0; k < 8u; k++) {
            spl_tile(SPL_ORDER[k], 0, SPL_LAPIS[SPL_ORDER[k]]);
            spl_wait(SPL_STEP_MS);
            used += SPL_STEP_MS;
        }
        spl_tile(SPL_GOLD, 2, SPL_GOLD_C);      /* the gold tessera, pressed in */
        spl_wait(2u * SPL_STEP_MS);
        spl_tile(SPL_GOLD, 2, T_BG);
        spl_tile(SPL_GOLD, 0, SPL_GOLD_C);
        spl_wait(2u * SPL_STEP_MS);
        used += 4u * SPL_STEP_MS;
        spl_hold(SPL_HOLD_MS);
    }
    if (used < ms)
        spl_wait(ms - used);
}
