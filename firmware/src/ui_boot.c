/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Shared, synchronous progress for boot maintenance. No timer IRQ, audio or
 * normal UI loop is needed: flush the warning before any flash write begins.
 * Errors remain on screen until acknowledged; callers retain their write guards. */
static struct {
    const char *title;
    uint16_t done, total;
    uint8_t state;                             /* 0 idle, 1 working, 2 failed */
} boot_work;

static void boot_work_draw(void)
{
    char count[24];
    lcd_fill(0, 0, 240, 240, T_BG);
    draw_text_box(12, 28, 216, &AF_S, "STARTUP MAINTENANCE", T_MID, 1);
    draw_text_box(12, 60, 216, &AF_M, boot_work.title, T_TEXT, 1);
    if (boot_work.state == 2u) {
        draw_text_box(12, 98, 216, &AF_M, "COULD NOT FINISH", T_THEME, 1);
        draw_text_box(12, 134, 216, &AF_S, "Restart to retry", T_TEXT, 1);
        draw_text_box(12, 204, 216, &AF_S, "OCT+ to continue", T_THEME, 1);
    } else {
        uint32_t width = boot_work.total ? 200u * boot_work.done / boot_work.total : 0u;
        fmt_int(count, boot_work.done);
        uint32_t n = str_len(count);
        str_cpy(count + n, " / ", sizeof count - n);
        fmt_int(count + str_len(count), boot_work.total);
        draw_text_box(12, 98, 216, &AF_M, count, T_THEME, 1);
        lcd_fill(20, 130, 200, 8, T_LINE);
        if (width) lcd_fill(20, 130, width, 8, T_THEME);
        draw_text_box(12, 163, 216, &AF_M,
                      boot_work.state ? "KEEP POWER CONNECTED" : "COMPLETE", T_TEXT, 1);
        draw_text_box(12, 190, 216, &AF_S,
                      boot_work.state ? "Please wait. Do not switch off." : "Starting up...", T_MID, 1);
    }
    lcd_sync();                                 /* physically visible before erases/programs */
#ifdef FM1_IRQ_TARGET
    fm1_wdt_feed();                             /* first-boot writes can span several watchdog periods */
#endif
}
static void boot_work_begin(const char *title, uint32_t total)
{
    scr_wake_now();
    boot_work.title = title;
    boot_work.done = 0;
    boot_work.total = (uint16_t)total;
    boot_work.state = 1;
    boot_work_draw();
}
static void boot_work_progress(uint32_t done)
{
    boot_work.done = (uint16_t)(done > boot_work.total ? boot_work.total : done);
    boot_work_draw();
}
static void boot_work_end(int failed)
{
    boot_work.state = failed ? 2u : 0u;
    boot_work_draw();
    ui.force = 1;
}
/* Input is consumed by this modal screen until the operation has finished.
 * Ignore simultaneous OCT keys so a calibration hold cannot dismiss a failure. */
static void boot_work_input(uint32_t pressed)
{
    uint32_t dn = 1u << panel.btn[B_OCTDN], up = 1u << panel.btn[B_OCTUP];
    uint32_t keys = pressed & (dn | up);
    if ((fm1_in.buttons & (dn | up)) == (dn | up)) return;
    if (boot_work.state == 2u && keys == up) {
        boot_work.state = 0;
        ui.force = 1;
    }
}
#define BOOT_WORK_BEGIN(title, total) boot_work_begin(title, total)
#define BOOT_WORK_PROGRESS(done) boot_work_progress(done)
#define BOOT_WORK_END(failed) boot_work_end(failed)
