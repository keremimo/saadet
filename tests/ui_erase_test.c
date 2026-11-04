/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
#include <assert.h>
#include <setjmp.h>
static jmp_buf reboot_point;
#define HOST_REBOOT_HOOK() longjmp(reboot_point, 1)
#define MELODEE_FLASH 1
#include "ui_harness.c"

static void down(uint32_t b) { fm1_in.buttons |= ui_btn_bit(b); host_pressed |= ui_btn_bit(b); frame(); }
static void up(uint32_t b) { fm1_in.buttons &= ~ui_btn_bit(b); host_released |= ui_btn_bit(b); frame(); }
static void show(uint32_t p, uint32_t s) { ui_open(p); uix.stop[p] = (uint8_t)s; frame(); }
static uint32_t erase_page(void) { return place_of(PL_GLO).n - 1u; }
static void ready(void)
{
    memset(nor, 255, sizeof nor);
    memset(&lib, 0, sizeof lib);
    memset(&uix, 0, sizeof uix); uix.held = PL_N; uix.hot = 4;
    boot_work.state = 0;
    power_on(); persist_boot();
    frames(800);
}
static void deletions(void)
{
    char b[28];
    ready();
    assert(up_store(6, "FIRST") == 0 && up_store(7, "SECOND") == 0);
    assert(up_load(6) == 0);
    up_favorite_set(6, 1);
    show(PL_LIB, LIB_USER);
    assert(lint_has("DELETE") && !lint_check("user delete"));
    ui_png("delete_user");
    turn(EN_K4, 1);
    assert(lib.deleting && lint_has("DELETE U007? PRESS FX") && ui_led_lv[panel.btn[B_FX]] == 2u);
    assert(!lint_check("delete armed")); ui_png("delete_armed");
    turn(EN_K4, -1); assert(!lib.deleting && up_used(6));
    turn(EN_K4, 1); frames(3000); assert(!lib.deleting && up_used(6));
    turn(EN_K4, 1); press(B_ENV); assert(!lib.deleting && up_used(6));
    show(PL_LIB, LIB_USER);
    turn(EN_K4, 1); turn(EN_K1, 1); assert(!lib.deleting && up_used(6));
    lib.pending = 0; lib.at[LIB_USER] = 0; frame();
    turn(EN_K4, 1); turn(EN_SELECT, 1); assert(!lib.deleting && up_used(6));
    show(PL_LIB, LIB_USER);
    turn(EN_K4, 1); turn(EN_PRESET, 1); assert(!lib.deleting && up_used(6));
    uix.br.on = 0; lib.pending = 0;
    show(PL_LIB, LIB_USER); lib.at[LIB_USER] = 0;
    turn(EN_K4, 1);
    assert(up_rename(6, "RENAMED") == 0); frame();
    assert(!lib.deleting && up_used(6));             /* a remote edit invalidates the confirmation */
    turn(EN_K4, 1); fm1_ms = lib.delete_t + 3000u;
    press(B_FX); assert(up_used(6));                /* expiry is checked before a confirming press */
    show(PL_LIB, LIB_FAV); lib_open(LIB_FAV);
    assert(lint_has("DELETE"));
    turn(EN_K4, 1);
    erase_error = 1; press(B_FX); erase_error = 0;
    assert(up_used(6) && up_favorite(6) && !lib.deleting && lint_has("DELETE ERROR"));
    show(PL_LIB, LIB_FAV);
    assert(up_load(6) == 0);
    uint8_t patch[sizeof p5_patch_of(TSEL)->raw]; memcpy(patch, p5_patch_of(TSEL)->raw, sizeof patch);
    turn(EN_K4, 1); press(B_FX);
    assert(!up_used(6) && !up_favorite(6) && up_used(7) && !lib.pending);
    assert(user_of(TSEL) == USER_NONE && !memcmp(patch, p5_patch_of(TSEL)->raw, sizeof patch));
    assert(uix.place == PL_LIB && ui_stop(PL_LIB) == LIB_FAV && uix.held == PL_N);
    assert(lint_has("DELETED U007 RENAMED"));
    ui_png("delete_done");
    show(PL_LIB, LIB_USER); lib_open(LIB_USER);
    turn(EN_K4, 1); press(B_FX);
    assert(!lib_count(LIB_USER) && !lint_has("DELETE") && lint_has("NO USER SOUNDS YET"));
    show(PL_LIB, LIB_P5); lib.at[LIB_P5] = 0; frame();
    assert(!lint_has("DELETE"));
    uint32_t n = erases; turn(EN_K4, 1); press(B_FX);
    assert(erases == n && !lib.deleting);
    up_cache_reset(); up_boot();                    /* both deleted records remain gone after reload */
    assert(!up_used(6) && !up_used(7));
    up_name(6, b);
    for (uint32_t e = 0; e < NENG_SHOWN; e++) {
        engine_load_of(TSEL, eng_vis(e), 1); frames(800);
        uint32_t slot = e + 1u == NENG_SHOWN ? 383u : 20u + e;
        assert(up_store(slot, "ENGINE USER") == 0);
        show(PL_LIB, LIB_P5 + e);
        lib.at[LIB_P5 + e] = (uint16_t)(lib_count(LIB_P5 + e) - 1u); frame();
        assert(lint_has("DELETE"));
        turn(EN_K4, 1); press(B_FX);
        assert(!up_used(slot) && !lint_has("DELETE") && ui_stop(PL_LIB) == LIB_P5 + e);
    }
    puts("DELETE: arming, expiry, cancellation, factory protection, failures, favourites and reload passed");
}
static void countdown(void)
{
    ready();
    uint32_t n = erases, page = erase_page();
    show(PL_GLO, page);
    assert(ui_erase_page() && lint_has("The firmware stays.") && !lint_check("erase page"));
    ui_png("erase_page");
    ui_hold(B_GLO, 3200);                           /* no knob arm: this is still the page list */
    assert(!host_reboots && erases == n && !lib.erase);
    show(PL_GLO, page); turn(EN_K1, 1);
    assert(lib.erase && lint_has("HOLD GLO 3 S TO ERASE ALL") && lint_has("K1 back to cancel"));
    assert(!lint_check("erase armed")); ui_png("erase_armed");
    turn(EN_K1, -1); assert(!lib.erase);
    turn(EN_K1, 1); turn(EN_SELECT, -1); assert(!lib.erase);
    show(PL_GLO, page); turn(EN_K1, 1); press(B_HOME); assert(!lib.erase);
    show(PL_GLO, page); turn(EN_K1, 1); down(B_GLO);
    frames(1800);
    assert(uix.erasing && !uix.reveal && lint_has("ERASING EVERYTHING IN"));
    assert(!lint_check("erase countdown")); ui_png("erase_countdown");
    key_down(0); frame(); key_up(0);
    assert(uix.erasing && ui_stop(PL_GLO) == page && !uix.reveal);
    up(B_GLO);
    assert(!lib.erase && !uix.erasing && ui_stop(PL_GLO) == page && erases == n);
    turn(EN_K1, 1); down(B_GLO); frames(2800); up(B_GLO);
    assert(!host_reboots && !lib.erase && erases == n && ui_stop(PL_GLO) == page);
    down(B_GLO); turn(EN_K1, 1); frames(3200); up(B_GLO);
    assert(!host_reboots && erases == n);           /* cannot arm under an existing hold */
    show(PL_GLO, page); turn(EN_K1, 1); down(B_GLO); turn(EN_K1, -1); frames(3200); up(B_GLO);
    assert(!host_reboots && erases == n && ui_stop(PL_GLO) == page);
    turn(EN_K1, 1); down(B_GLO); down(B_ENV); frames(3200); up(B_GLO); up(B_ENV);
    assert(!host_reboots && erases == n && !lib.erase);
    show(PL_GLO, page); turn(EN_K1, 1);
    flash_ok = 0; ui_hold(B_GLO, 3200); flash_ok = 1;
    assert(!host_reboots && erases == n && ui_stop(PL_GLO) == page && lint_has("ERASE ERROR"));
    puts("ERASE UI: explicit arm, uninterrupted hold, countdown, cancellation and no release paging passed");
}
static void storage(void)
{
    static uint8_t before[sizeof nor], live[sizeof nor / ST_SECTOR];
    static const uint16_t RANGE[][2] = {           /* independent map from the approved storage plan, in sectors */
#ifdef SAADET_SMK37
        {0x9D, 2}, {0xA1, 2}, {0xA3, 10}, {0xC8, 16}, {0xD8, 8},
        {0xE0, 3}, {0xE5, 5}, {0xEB, 2}
#else
        {0x93, 10}, {0x9D, 3}, {0xA0, 2}, {0xC8, 16}, {0xD8, 8},
        {0xE5, 5}, {0xFA, 1}, {0xFC, 3}
#endif
    };
    uint32_t obj, copy, i, n = erases, total = 0;
    ready();
    for (i = 0; i < NELEM(RANGE); i++)
        for (uint32_t k = 0; k < RANGE[i][1]; k++) live[RANGE[i][0] + k] = 1;
    for (i = 0; i < sizeof nor; i++) nor[i] = (uint8_t)(i * 37u + i / ST_SECTOR);
    for (obj = 0; obj < OBJ_COUNT; obj++) {
        if (st_retired(obj)) continue;
        uint32_t payload = obj + 17u;
        assert(st_save(obj, &payload, sizeof payload) == 0);
        assert(st_save(obj, &payload, sizeof payload) == 0);   /* both A and B contain live records */
        for (copy = 0; copy < 2u; copy++) {
            uint32_t sec = st_sector(obj, copy) / ST_SECTOR;
            assert(live[sec]); total++;
        }
        assert(st_exists(obj));
    }
    memcpy(before, nor, sizeof nor);
    uint32_t pm = panel.magic = PANEL_MAGIC, sm = settings.magic = SETTINGS_MAGIC;
    bootguard.pending = 1;
    flash_ok = 0; assert(persist_erase_all() && !host_reboots); flash_ok = 1;
    song.playing = 1; n = erases; assert(persist_erase_all() && erases == n); song.playing = 0;
    erase_error = 1; assert(persist_erase_all()); erase_error = 0;
    assert(boot_work.state == 2u && !host_reboots && panel.magic == pm && settings.magic == sm);
    assert(!memcmp(before, nor, sizeof nor));
    erase_skip = 1; assert(persist_erase_all()); erase_skip = 0;
    assert(!host_reboots && !memcmp(before, nor, sizeof nor)); /* readback catches a silent hardware failure */
    erase_fail_at = (int)erases + 5;
    assert(persist_erase_all() && !host_reboots && boot_work.done == 5u);
    erase_fail_at = -1;
    for (i = 0; i < sizeof nor; i++)
        if (!live[i / ST_SECTOR]) assert(nor[i] == before[i]);
    n = erases;
    show(PL_GLO, erase_page()); turn(EN_K1, 1); down(B_GLO); frames(3100);
    /* boot maintenance stays modal after a failure until acknowledged */
    assert(!host_reboots);
    up(B_GLO); boot_work.state = 0;
    static uint32_t erase_start;
    if (!setjmp(reboot_point)) {
        show(PL_GLO, erase_page()); turn(EN_K1, 1); down(B_GLO);
        erase_start = erases;
        frames(3100);
        assert(!"the confirmed erase must reboot");
    }
    assert(host_reboots == 1u && !boot_work.state && boot_work.done == total && boot_work.total == total);
    assert(erases == erase_start + total && !panel.magic && !settings.magic && !bootguard.pending);
    assert(uix.held == PL_N && !lib.erase && !uix.erasing);
    ui_png("erase_complete");
    fm1_in.buttons = 0; ui_button_up(B_GLO);
    assert(ui_stop(PL_GLO) == erase_page());
    for (obj = 0; obj < OBJ_COUNT; obj++) assert(!st_exists(obj));
    for (i = 0; i < sizeof nor; i++)
        assert(nor[i] == (live[i / ST_SECTOR] ? 255u : before[i]));
    puts("ERASE STORAGE: both copies of every live object erased; firmware, OTA and all other bytes unchanged");
}
int main(void)
{
    deletions(); countdown(); storage();
    puts("UI ERASE TESTS PASSED");
    return 0;
}
