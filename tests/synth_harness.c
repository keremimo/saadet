/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The whole instrument on the host: engines, MIDI, the stores and the blank screen, with the panel's HAL stubbed. */
#define main hostsim_main
#include "hostsim.c"
#undef main

/* ------------------------------------------------------------ HAL stubs --- */
#define FM1_NCOL 11u
static const int8_t FM1_KEYMAP[6][FM1_NCOL] = {   /* (hal/fm1_input.h: the LEDs and keys of each matrix spot) */
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 5, 11,  4, 10,  3,  9,  2,  8, -1, -1, -1},
    {34, 35, 36, 37, 38, 40, 39, 13,  7,  6, 12},
    {23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33},
    { 0,  1, 15, 14, 17, 16, 19, 18, 20, 21, 22},
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
};
static uint8_t fm1_led[FM1_NCOL];
static uint8_t fm1_led_dim[3][FM1_NCOL], fm1_led_dim_mask[3];   /* (hal/fm1_input.h: the dim planes) */
#define FM1_TICKS_PER_US 1u
static uint32_t host_ticks, host_pressed, host_released, host_notes;
static uint32_t host_px_sent;                      /* pixels sent to the panel (blits and fills): the draw cost */
static int32_t host_enc[7];
static uint32_t fm1_ticks(void) { return host_ticks; }
static uint32_t fm1_input_edges(uint32_t *released)
{
    uint32_t p = host_pressed;
    if (released) *released = host_released;
    host_pressed = host_released = 0;
    return p;
}
static uint32_t fm1_input_note_edges(void) { uint32_t n = host_notes; host_notes = 0; return n; }
static int32_t fm1_enc_take(uint32_t e)
{
    e %= 7u;
    int32_t s = host_enc[e]; host_enc[e] = 0;
    return s;
}
static void fm1_wdt_feed(void) {}
static uint32_t host_reboots;
static void fm1_reboot(void)
{
    host_reboots++;
#ifdef HOST_REBOOT_HOOK
    HOST_REBOOT_HOOK();
#endif
}
static void fm1_irq_off(void) {}
static void fm1_irq_on(void) {}
static uint16_t host_screen[240 * 240];
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, j;
    host_px_sent += w * h;
    for (j = 0; j < h && y + j < 240u; j++)
        for (i = 0; i < w && x + i < 240u; i++)
            host_screen[(y + j) * 240u + x + i] = (uint16_t)((c >> 8) | (c << 8));
}
static void lcd_sync(void) {}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{
    uint32_t i, j;
    host_px_sent += w * h;
    for (j = 0; j < h && y + j < 240u; j++)
        for (i = 0; i < w && x + i < 240u; i++) host_screen[(y + j) * 240u + x + i] = p[j * w + i];
}
static struct { uint32_t stage; } melodee_dbg;
#ifndef MELODEE_FLASH
#define MELODEE_FLASH 0
#endif
#ifndef MELODEE_VERSION
#define MELODEE_VERSION "TEST"
#endif
#include "../firmware/src/gfx.c"
#include "../firmware/src/panel.c"
#include "../firmware/src/sound.c"
#include "../firmware/src/ui.c"
#if MELODEE_FLASH
#include "synth_flash_harness.c"
#endif
#include "../firmware/src/upreset.c"
#if !MELODEE_FLASH
#define ST_PAYLOAD_MAX 3840u
#endif
#include "../firmware/src/persist.c"
#include "../firmware/src/cz_store.c"
#include "../firmware/src/fm6_store.c"

static void power_on(void)
{
    memset(trk, 0, sizeof trk); memset(&song, 0, sizeof song); memset(&ui, 0, sizeof ui);
    memset(&midi_clock, 0, sizeof midi_clock); midi_input_reset(); midi_forget_track(0);
    up_cache_reset(); native_cache_reset();
    cz_init();
    for (uint32_t i = 0; i < G_COUNT; i++) song.g[i] = GP[i].def;
    track_defaults(&trk[0]); set_engine_of(&trk[0], TRK_DEF[0][0]);
    apply_preset_to(&trk[0], TRK_DEF[0][1]); trk[0].engine = trk[0].eng_req;
    song.master_q12 = 4096; ui.force = 1;
    panel = PANEL_DEFAULT; settings_init(); palette_set(settings.palette);
    kb_prev = kb_layer = kb_lock = kb_mask = 0;
    memset(kb_chn, 0, sizeof kb_chn); memset(mchord, 0, sizeof mchord);
    memset(chord_last, 0, sizeof chord_last);
    fm1_in.buttons = fm1_in.notes = host_pressed = host_released = host_notes = 0;
#ifdef SAADET_SMK37
    sd_kb_keys = sd_kb_retrig = 0; sd_jack_pedal = 0;
#endif
    memset(host_enc, 0, sizeof host_enc);
    midi_sync_active=0;midi_beat_samples=beat_pos=beat_n=0;
    kb_strum_pending=midi_strum_pending=0;
    panic_req = 0; mi_r = mi_w = midi_in_overflow = 0;
}
static void frame(void)
{
    host_ticks += 16000u; fm1_ms += 16u; ui_input(); ui_leds(); ui_draw();   /* (as main.c's loop) */
}
static void frames(uint32_t ms) { uint32_t i; for (i = 0; i < ms / 16u; i++) frame(); }
static void press(uint32_t label)
{
    fm1_ms += 320u;
    fm1_in.buttons |= 1u << panel.btn[label]; host_pressed |= 1u << panel.btn[label];
    frame(); fm1_in.buttons &= ~(1u << panel.btn[label]); host_released |= 1u << panel.btn[label]; frame();
}
static void turn(uint32_t role, int32_t s)
{
    host_enc[panel.enc[role]] += s * panel.dir[role]; frame();
    host_ticks += 200000u; fm1_ms += 200u;
}
#ifdef SAADET_SMK37
/* the SMK-37's key path: mix.c's numbering, the FM-1's 27 first; the bridge publishes the keys and their velocities */
static void key_down_vel(uint32_t k, uint32_t vel)
{
    if (k < 27u) { fm1_in.notes |= 1u << k; host_notes |= 1u << k; }
    sd_kb_vel[k] = (uint8_t)vel;
    sd_kb_keys |= KB_BIT(k);
    keyboard_block();
}
static void key_down(uint32_t k) { key_down_vel(k, 100u); }
static void key_up(uint32_t k)
{
    if (k < 27u) fm1_in.notes &= ~(1u << k);
    sd_kb_keys &= ~KB_BIT(k);
    keyboard_block();
}
#else
static void key_down(uint32_t k) { fm1_in.notes |= 1u << k; host_notes |= 1u << k; keyboard_block(); }
static void key_up(uint32_t k) { fm1_in.notes &= ~(1u << k); keyboard_block(); }
#endif
static uint32_t gates(void)                        /* voices of every track with their key down */
{
    uint32_t p, i, n = 0;
    for (p = 0; p < NTRK; p++)
        for (i = 0; i < NVOICE; i++)
            n += trk[p].v[i].gate != 0;
    return n;
}
static int check(const char *what,int ok){printf("%s: %s\n",what,ok?"ok":"FAIL");return !ok;}
