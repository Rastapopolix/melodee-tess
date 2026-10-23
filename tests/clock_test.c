/* SPDX-License-Identifier: GPL-3.0-only */
/* The SYS clock, read only (hal/fm1_clock.h): the multiplier the SPL or a kept retune left, never written. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
static uint32_t pll, reads;
static uint32_t read_word(uint32_t a)
{
    assert(a == 0x119ACu);                          /* PLL2_CON1, nothing else */
    reads++;
    return pll;
}
#define FM1_CLOCK_HOST_TEST 1
#define FCLK_READ(a) read_word(a)
#include "../firmware/hal/fm1_clock.h"

int main(void)
{
    assert(fm1_clock_mhz_of(FCLK_MULT_320) == 320u && fm1_clock_mhz_of(FCLK_MULT_360) == 360u &&
           fm1_clock_mhz_of(FCLK_MULT_396) == 396u);
    pll = 0x0750310Cu; assert(fm1_clock_mult() == FCLK_MULT_360);   /* the SPL's 268 */
    pll = 0x075030EEu; assert(fm1_clock_mult() == FCLK_MULT_320);   /* kept from a 1.3.x / rc session */
    pll = 0x07503127u; assert(fm1_clock_mult() == FCLK_MULT_396);
    pll = 0x07503026u; assert(fm1_clock_mult() == 0u);              /* unknown: the budget assumes 360 */
    assert(reads == 4u);
    puts("clock: read only, the SPL's 360, a kept 320 or 396, an unknown multiplier: OK");
    return 0;
}
