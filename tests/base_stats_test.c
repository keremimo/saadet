/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Editor command 72 as the fm1-base build compiles it (SAADET_FM1_BASE, no
 * legacy USB audio, no cache RAM): the base's render time and missed renders,
 * the voices, the second core and the memory, in the legacy reply layout
 * that tools/usb_audio_stats.py reads. */
#include <assert.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#define MELODEE_FLASH 1
#define MELODEE_USB_AUDIO 0
#define MELODEE_CACHE_RAM 0
#define SAADET_FM1_BASE 1
static void sd_reset_intended(void) {}   /* (the base's boot guard: platform/app.c) */
/* the base target is dual-core; no worker thread runs here (tests/dual_core_test.c runs one) */
#define MELODEE_DUAL_CORE 1
static uint32_t worker_clock(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)((uint64_t)ts.tv_sec * 1000000u + ts.tv_nsec / 1000u);
}
#define AW_WORD _Atomic uint32_t
#define AW_LOAD(p) atomic_load_explicit(p, memory_order_acquire)
#define AW_STORE(p,v) atomic_store_explicit(p,v,memory_order_release)
#define AW_TICKS() worker_clock()
#define AW_TIMEOUT_TICKS 2000000u
#define AW_RAM_LOOP static
#define AW_IDLE() ((void)0)
#define AW_HOLD() ((void)0)
#define AW_FAULTS() 0u
#include "../firmware/src/audio_worker.h"
static struct { volatile uint32_t count; } fm1_core1_fault;   /* (platform/hardware/cpu1.h) */
static uint32_t sd_render_max_us, host_xruns;
static uint32_t sd_xruns(void) { return host_xruns; }
/* the base's radio, which the GLOBAL pages ask about: none here */
static int sd_ble_available(void) { return 0; }
static int sd_ble_enabled(void) { return 0; }
static int sd_ble_state(void) { return 0; }
static int sd_ble_linked(void) { return 0; }
static int sd_ble_connected(void) { return 0; }
static void sd_ble_enable(int on) { (void)on; }
static void sd_ble_reconnect(void) {}
static int32_t sd_ble_pairing_code(void){return -1;}
static int sd_ble_device_state(void) { return 0; }
static int sd_ble_device_count(void) { return 0; }
static int sd_ble_device_pick(void) { return 0; }
static const char *sd_ble_device_name(void) { return ""; }
static int sd_ble_device_found(int i, char *name, int *rssi) { (void)i; (void)name; (void)rssi; return 0; }
static void sd_ble_device_choose(int steps) { (void)steps; }
static const char *sd_ble_device_action(void) { return ""; }
static const char *sd_ble_page(int devices, int pairing) { (void)devices; (void)pairing; return ""; }
static void sd_ble_drop_host(void) {}
static void sd_ble_forget_arm(int on) { (void)on; }
static int sd_ble_forget_state(void) { return 0; }
static void sd_ble_forget_confirm(void) {}
#include "synth_harness.c"
#define SXQ 1024u
static uint32_t so_w, so_r, wire_n;
static uint8_t wire[2048];
static int ota_wire_send(const uint8_t *p, uint32_t n) { assert(n <= sizeof wire); memcpy(wire, p, n); wire_n = n; return 0; }
#define ED_BK_FLASH_PTR(off) (nor + (off))
static int ota_frame_get(const uint8_t **p, uint32_t *n) { (void)p; (void)n; return 0; }
static void ota_frame_done(void) {}
#include "../platform/deadline.h"
#include "../firmware/hal/fm1_cache.h"
#include "../firmware/src/editor.c"
static int failures;
static void ok(const char *what, int pass) { printf("base stats: %s: %s\n", what, pass ? "ok" : "FAIL"); failures += !pass; }
/* command 72 with flags; the reply's schema, or 0 */
static uint32_t stats(uint8_t flags)
{
    uint8_t f[] = {0x7D, 0x46, 0x4C, ED_AUDIO_STATS, flags};
    wire_n = 0; ed_handle(f, sizeof f);
    return wire_n > 6u && wire[0] == 0xF0 && wire[4] == ED_AUDIO_STATS && wire[wire_n - 1u] == 0xF7 ? wire[5] : 0u;
}
static uint32_t field(uint32_t i)
{
    uint32_t v = 0;
    for (uint32_t k = 0; k < 5u; k++) v |= (uint32_t)wire[6u + 5u * i + k] << (7u * k);
    return v;
}

int main(void)
{
    power_on(); memset(nor, 255, sizeof nor); persist_boot(); settings_init();
    host_preset(TSEL, ENGI_J106, 1);
    trk_note_on(TSEL, 60, 100); trk_note_on(TSEL, 64, 100);
    int32_t out[CTL];
    for (uint32_t b = 0; b < 8u; b++) track_render(TSEL, out, CTL);
    /* a sustained overload is measured, never answered: a stored SAFETY ON (Melodee's) changes nothing */
    uint32_t kills = voice_kills;
    settings_safety = 1;
    for (uint32_t b = 0; b < 64u; b++) { saadet_deadline(2901, 2902); track_render(TSEL, out, CTL); }
    settings_safety = 0;
    /* the base's deadline hook after each render: 1234 us the longest of a 2902 us budget */
    sd_render_max_us = 0; host_xruns = 3; song.cpu_q8 = 0;
    saadet_deadline(800, 2902); saadet_deadline(1234, 2902); saadet_deadline(900, 2902);
    uint32_t load = song.cpu_q8;

    ok("every section: schema 6, 55 fields", stats(2 | 4 | 8 | 16) == 6u && wire_n == 6u + 55u * 5u + 1u);
    uint32_t usb = 0;
    for (uint32_t i = 0; i < 16u; i++) usb |= field(i);
    ok("the base's USB audio fields read 0", !usb && !field(17));
    ok("missed renders, longest render, load", field(16) == 3u && field(18) == 1234u && field(19) == load && load > 0u);
    ok("voices sounding and held", field(20) == 2u && field(21) == 2u);
    ok("a sustained overload cuts no voice", voice_kills == kills && !field(24));
    ok("CPU1 offline here", !field(26));
    audio_worker_online = 1; audio_worker.jobs = 7; fm1_core1_fault.count = 2;   /* (no render while set) */
    stats(4);
    printf("  CPU1 online %u, jobs %u, faults %u\n", (unsigned)field(26), (unsigned)field(27), (unsigned)field(23));
    ok("the second core: online, its jobs and faults", field(26) == 1u && field(27) == 7u && field(23) == 2u);
    audio_worker_online = 0; fm1_core1_fault.count = 0;
    stats(2 | 4 | 8 | 16);
    printf("  memory: capacity %u, used %u, cache status %u, cache capacity %u\n",
           (unsigned)field(41), (unsigned)field(42), (unsigned)field(32), (unsigned)field(45));
    ok("memory: the engine bank in use, no cache RAM", field(41) == RESOURCE_CAPACITY && field(42) >= sizeof(j106_part_t) &&
       field(32) == FM1_CACHE_DISABLED && !field(45) && !field(46));
    ok("the short form: schema 2, 20 fields", stats(0) == 2u && wire_n == 6u + 20u * 5u + 1u && field(18) == 1234u);
    stats(1);
    ok("flag 1 resets the longest render", sd_render_max_us == 0u && stats(0) == 2u && field(18) == 0u && field(16) == 3u);
    printf(failures ? "BASE STATS TEST FAILED (%d)\n" : "BASE STATS TEST PASSED\n", failures);
    return failures != 0;
}
