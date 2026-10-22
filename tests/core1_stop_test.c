/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Register-model coverage of the real hold helper. The delay's sufficiency
 * and physical bus behavior still require an FM-1. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#define MELODEE_DUAL_CORE 1
#define FM1_CORE1_STOP_TEST 1
static uint32_t control, unlocked, enables, pending, stage, syncs, toggles;
static const uint32_t bus_mask = 0x003F0030u, other_fault = 1u << 13;

static uint32_t reg_read(uint32_t address)
{
    switch (address) {
    case 0x1EEE004u: return control;
    case 0x1EEE240u: return unlocked;
    case 0x1EEE340u: return enables;
    default: assert(!"unexpected register read"); return 0;
    }
}
static void reg_write(uint32_t address, uint32_t value)
{
    switch (address) {
    case 0x1EEE240u:
        assert(value == 0xE7u);
        unlocked ^= 1u;
        toggles++;
        break;
    case 0x1EEE340u:
        assert(unlocked);
        if (!stage) {
            assert(value == (enables & ~bus_mask));
            stage = 1;
        } else {
            assert(stage == 4 && syncs == 3 && !(pending & bus_mask));
            stage = 5;
        }
        enables = value;
        break;
    case 0x1EEE004u:
        assert(unlocked && !(enables & bus_mask) && syncs == 1);
        if (stage == 1) {
            assert(value == (control | 2u));
            stage = 2;
        } else {
            assert(stage == 2 && value == (control & ~8u));
            stage = 3;
        }
        control = value;
        break;
    case 0x1EEE248u:
        assert(unlocked && stage == 3 && syncs == 2 && value == bus_mask);
        assert((pending & bus_mask) == bus_mask);
        pending &= ~value;
        stage = 4;
        break;
    default: assert(!"unexpected register write");
    }
}
static void reg_sync(void)
{
    syncs++;
    if (syncs == 2) {
        assert(stage == 3 && (control & 0xAu) == 2u);
        pending |= bus_mask;             /* error arrives after the hold */
    }
    assert(!(pending & enables & bus_mask));
}
#define FM1_C1_READ(a) reg_read(a)
#define FM1_C1_WRITE(a, v) reg_write(a, v)
#define FM1_C1_SYNC() reg_sync()
#include "../firmware/hal/fm1_core1_stop.h"

int main(void)
{
    const uint32_t masks[] = { 0, 1u << 4, 1u << 21, 0xFFFFFFFFu };
    for (uint32_t lock = 0; lock < 2; lock++)
        for (uint32_t m = 0; m < sizeof masks / sizeof *masks; m++) {
            control = 0xA5A50008u;
            unlocked = lock;
            enables = masks[m];
            for (uint32_t repeat = 0; repeat < 2; repeat++) {
                pending = other_fault;
                stage = syncs = toggles = 0;
                fm1_core1_stop();
                assert(stage == 5 && syncs == 4);
                assert(control == 0xA5A50002u && enables == masks[m]);
                assert(unlocked == lock && toggles == (lock ? 0u : 2u));
                assert(pending == other_fault);   /* retain unrelated diagnostics */
            }
        }
    puts("CPU1 stop: delayed bus flags masked and cleared, unrelated state preserved, guards and lock restored, repeated hold: OK");
    return 0;
}
