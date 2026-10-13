/* SPDX-License-Identifier: GPL-3.0-only */
/* OVERCLOCK (HOME held, ui_menu.c): the SYS clock the FM-1 boots at (main.c oc_boot; the retune is PR #27's by
 * @johnskiller, hal/fm1_clock.h):
 *   320 MHz   the default: JieLi's own clock table
 *   360 MHz   MEDIUM: M-VAVE's boot clock (the SPL's), what Melodee ran at before
 * The supply rails stay where the boot loader sets them.
 * Kept in favorites.factory[15][30] bits 2..3 (favorites.c; older firmware leaves them alone). A new clock takes a
 * restart: closing the menu asks for one (oc_restart: main.c restarts once the settings are saved). A session above
 * 320 that crashes, hangs or loses CPU1 is set back to 320 (main.c oc_boot, oc_poll; core.h oc_guard). */
enum { OC_OFF, OC_MEDIUM, OC_N };            /* (a stored 2, an unreleased 396 MHz, reads as OFF) */
static const char *const OC_NAME[OC_N] = {"320 MHz", "360 MHz"};
#define PREF_OC_SHIFT 2u
#define PREF_OC_MASK (3u << PREF_OC_SHIFT)
static uint8_t oc_boot_mode;                    /* the mode this session booted with (main.c oc_boot) */
static uint8_t oc_restart;                      /* the restart question answered Yes (main.c oc_poll) */
static uint8_t oc_reverted;                     /* 1: set back to 320 MHz, to save and say (main.c oc_poll) */

static uint32_t oc_get(void)
{
    uint32_t m = (PREF_BITS & PREF_OC_MASK) >> PREF_OC_SHIFT;
    return m < OC_N ? m : OC_OFF;
}
static void oc_put(uint32_t m)
{
    PREF_BITS = (uint8_t)((PREF_BITS & ~PREF_OC_MASK) | (m < OC_N ? m : OC_OFF) << PREF_OC_SHIFT);
}

/* main.c oc_boot: the mode to boot with, the saved one; 320 MHz after a session above it that ended without an
 * intended reset (failed: core.h oc_guard.live) */
static uint32_t oc_boot_choose(int failed)
{
    uint32_t mode = oc_get();
    if (failed && mode != OC_OFF) {
        oc_put(OC_OFF);
        oc_reverted = 1;
        mode = OC_OFF;
    }
    oc_boot_mode = (uint8_t)mode;
    return mode;
}

/* main.c oc_poll: CPU1 retired in a session above 320 MHz: 320 from the next start */
static void oc_core1_lost(void)
{
    if (oc_boot_mode != OC_OFF && oc_get() != OC_OFF) {
        oc_put(OC_OFF);
        oc_reverted = 1;
    }
}
