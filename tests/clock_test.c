/* SPDX-License-Identifier: GPL-3.0-only */
/* Actual clock-switch routine with modeled MMIO and watchdog recovery. */
#include <assert.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
static uint32_t pll, source, ticks, programmed_at, writes, mode, step;
static jmp_buf failed;
static uint32_t read_word(uint32_t a)
{
    if (a == 0x119ACu) return pll;
    if (a == 0x1000Cu) return source;
    if (a == 0x10804u) { ticks += step; return ticks; }
    if (a == 0x40200u) return mode == 3 ? 1u << 31 : 0;
    assert(a == 0x1EEE008u);
    if ((mode == 2 && writes) || (mode == 1 && writes == 1)) return 0;
    return 1u << 14;
}
static void write_word(uint32_t a, uint32_t v)
{
    if (a == 0x119ACu) {
        assert(source == 0x10003u); /* RC16M throughout both programming attempts */
        pll = v; programmed_at = ticks; writes++;
    } else {
        assert(a == 0x1000Cu);
        if (v == 0x10101u) assert(ticks - programmed_at >= 20000u * 24u);
        source = v;
    }
}
#define FM1_CLOCK_HOST_TEST 1
#define FCLK_READ(a) read_word(a)
#define FCLK_WRITE(a,v) write_word(a,v)
#define FCLK_SYNC() ((void)0)
#define FCLK_FAILED() longjmp(failed, 1)
#include "../firmware/hal/fm1_clock.h"
static void reset(uint32_t m)
{
    pll = 0x0750310Cu; source = 0x10101u;
    ticks = UINT32_MAX - 24000u; programmed_at = writes = 0; mode = m; step = 24000u;
}
int main(void)
{
    /* the three OVERCLOCK points: 320 360 396 MHz, from the SPL's 360 and from each other (a soft reset keeps one) */
    assert(fm1_clock_mhz_of(FCLK_MULT_320) == 320u && fm1_clock_mhz_of(FCLK_MULT_360) == 360u &&
           fm1_clock_mhz_of(FCLK_MULT_396) == 396u);
    reset(0); assert(fm1_clock_mult() == FCLK_MULT_360 && fm1_clock_set(FCLK_MULT_320) == 0);
    assert(pll == 0x075030EEu && source == 0x10101u && writes == 1 && fm1_clock_mult() == FCLK_MULT_320);
    reset(0); assert(fm1_clock_set(FCLK_MULT_396) == 0);
    assert(pll == 0x07503127u && source == 0x10101u && writes == 1 && fm1_clock_mult() == FCLK_MULT_396);
    reset(0); assert(fm1_clock_set(FCLK_MULT_360) == 0 && !writes && pll == 0x0750310Cu);   /* already there */
    reset(0); pll = 0x07503127u; assert(fm1_clock_set(FCLK_MULT_320) == 0 && pll == 0x075030EEu && writes == 1);
    reset(0); pll = 0x075030EEu; assert(fm1_clock_set(FCLK_MULT_360) == 0 && pll == 0x0750310Cu && writes == 1);
    reset(0); pll = 0x07503026u; assert(fm1_clock_mult() == 0 && fm1_clock_set(FCLK_MULT_320) == 1);
    assert(pll == 0x07503026u && source == 0x10101u && writes == 0);
    reset(0); source &= ~(1u << 8); assert(fm1_clock_set(FCLK_MULT_320) == 1 && !writes);
    reset(3); assert(fm1_clock_set(FCLK_MULT_396) == 1 && !writes); /* busy SFC: no source switch */
    reset(1); assert(fm1_clock_set(FCLK_MULT_396) == 2);
    assert(pll == 0x0750310Cu && source == 0x10101u && writes == 2);
    reset(2);
    if (!setjmp(failed)) { fm1_clock_set(FCLK_MULT_320); assert(!"rollback must not return"); }
    assert(source == 0x10003u && pll == 0x0750310Cu && writes == 2);
    reset(0); step = 0;
    if (!setjmp(failed)) { fm1_clock_set(FCLK_MULT_396); assert(!"stopped timer must not return"); }
    assert(source == 0x10003u && writes == 2);
    puts("clock: 320/360/396 between each other, already there, wrapping timer, ready-bit settling, unfamiliar boot, rollback wait, failed rollback and stopped timer passed");
}
