/* SPDX-License-Identifier: GPL-3.0-only */
/* Boot-time SYS PLL retune (PR #27, @johnskiller), with IRQs off and CPU1 held.
 * Live FM-1 readings in PR #27 identify PLL2_CON1 (0x119AC), not PLL_CON1
 * (0x119A4), as the SYS VCO multiplier: FBDS = multiplier - 2. The SPL's
 * 268 means 24 MHz / 12 * 270 = 540 MHz, with CLK_CON2 selecting 2/3 = 360.
 * The OVERCLOCK menu (overclock.c) picks the multiplier: 240 gives VCO 480 /
 * SYS 320 MHz (HSB 160, LSB 53.3, SFC 80), 270 the SPL's 360 (HSB 180, LSB 60,
 * SFC 90). 297 (VCO 594 / SYS 396, an unreleased test) is never chosen, only
 * recognised at boot and switched away from. Any of these may be found at
 * boot (a soft reset can keep the last one). TIMER4 uses the crystal; audio
 * I2S, USB and UART use separate clock domains.
 *
 * CACHE_CON bit 14 is CACHE_WAIT's handshake, NOT a PLL lock indication.
 * No verified PLL lock bit is available here. Stay on RC16M for a fixed
 * 20 ms settling interval plus the cache handshake before selecting the
 * PLL, including after rollback. This is an experimental settling policy;
 * actual clock and stability still require device measurements.
 */
#pragma once
#include <stdint.h>
#ifndef FM1_CLOCK_HOST_TEST
#include "fm1_time.h"
#include "fm1_flash.h"                         /* RAMFN, FL_FAR */
#define FCLK_READ(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define FCLK_WRITE(a,v) (*(volatile uint32_t *)(uintptr_t)(a) = (v))
#define FCLK_SYNC() __asm__ volatile("csync" ::: "memory")
#define FCLK_RAM RAMFN
/* The watchdog armed in fm1_cstart resets us; repeated failures enter
 * UBOOT through the existing bootguard. Never return into XIP on RC16M. */
#define FCLK_FAILED() do { for (;;) FCLK_SYNC(); } while (0)
#else
#define FCLK_RAM
#endif

#define FCLK_PLL2_CON1 0x119ACu
#define FCLK_CLK_CON0  0x1000Cu
#define FCLK_CACHE_CON 0x1EEE008u
#define FCLK_SFC_CON   0x40200u
#define FCLK_TICKS     0x10804u

#define FCLK_MULT_320 240u
#define FCLK_MULT_360 270u
#define FCLK_MULT_396 297u
/* the SYS clock of a multiplier: 2 MHz a step, 2/3 of the VCO */
static inline uint32_t fm1_clock_mhz_of(uint32_t mult) { return mult * 4u / 3u; }

/* SRAM only: TIMER4 keeps counting on RC16M. Both the elapsed-time and
 * spin limits bound a missing handshake or stopped timer. A ready cache
 * bit alone never skips the settling interval. */
static inline __attribute__((always_inline)) int fm1_clock_idle_ram(void)
{
    uint32_t start = FCLK_READ(FCLK_TICKS), i;
    for (i = 0; i < 1000000u; i++) {
        if ((FCLK_READ(FCLK_CACHE_CON) & (1u << 14)) &&
            !(FCLK_READ(FCLK_SFC_CON) & (1u << 31)))
            return 1;
        if ((uint32_t)(FCLK_READ(FCLK_TICKS) - start) >= 20000u * 24u)
            return 0;
    }
    return 0;
}

static inline __attribute__((always_inline)) int fm1_clock_settle_ram(void)
{
    uint32_t start = FCLK_READ(FCLK_TICKS), i;
    for (i = 0; i < 1000000u; i++) {
        uint32_t elapsed = FCLK_READ(FCLK_TICKS) - start;
        if (elapsed >= 40000u * 24u)
            return 0;
        if (elapsed >= 20000u * 24u && (FCLK_READ(FCLK_CACHE_CON) & (1u << 14))) {
            FCLK_SYNC();
            return 1;
        }
    }
    return 0;
}

/* The multiplier running now (FCLK_MULT_*), 0 for one this file does not know */
static inline uint32_t fm1_clock_mult(void)
{
    uint32_t m = (FCLK_READ(FCLK_PLL2_CON1) & 0xFFFu) + 2u;
    return m == FCLK_MULT_320 || m == FCLK_MULT_360 || m == FCLK_MULT_396 ? m : 0u;
}

/* To multiplier mult (FCLK_MULT_*). Returns 0 after retuning (or when it
 * already runs there), 1 for an unfamiliar boot multiplier left untouched, 2
 * after a settled rollback. A failed rollback stays in SRAM on RC16M until
 * watchdog recovery, rather than returning to the caller with an unverified
 * PLL selected. */
static FCLK_RAM int fm1_clock_set_ram(uint32_t mult)
{
    uint32_t original = FCLK_READ(FCLK_PLL2_CON1), now = (original & 0xFFFu) + 2u;
    uint32_t con0 = FCLK_READ(FCLK_CLK_CON0);
    int result = 0;
    if ((now != FCLK_MULT_320 && now != FCLK_MULT_360 && now != FCLK_MULT_396) || !(con0 & (1u << 8)))
        return 1;
    if (now == mult)
        return 0;
    if (!fm1_clock_idle_ram())
        return 1;
    /* CLK_CON0 bit 1 enables RC16M; clearing bit 8 gates the SYS source,
     * following the SDK's sys_clk_source_rc_clk_on sequence. */
    FCLK_WRITE(FCLK_CLK_CON0, (con0 | (1u << 1)) & ~(1u << 8));
    FCLK_SYNC();
    FCLK_WRITE(FCLK_PLL2_CON1, (original & ~0xFFFu) | (mult - 2u));
    if (!fm1_clock_settle_ram()) {
        FCLK_WRITE(FCLK_PLL2_CON1, original);
        FCLK_SYNC();
        if (!fm1_clock_settle_ram())
            FCLK_FAILED();
        result = 2;
    }
    if (!fm1_clock_idle_ram())
        FCLK_FAILED();
    FCLK_WRITE(FCLK_CLK_CON0, con0);
    FCLK_SYNC();
    return result;
}

/* SRAM is beyond XIP's 23-bit call range: force the indirect call. */
#ifndef FM1_CLOCK_HOST_TEST
#define fm1_clock_set(mult) FL_FAR(fm1_clock_set_ram)(mult)
#else
#define fm1_clock_set(mult) fm1_clock_set_ram(mult)
#endif
