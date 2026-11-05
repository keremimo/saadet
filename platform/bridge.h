/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
#pragma once
#include <stdint.h>
#if defined(PLAT_SMK37) && PLAT_SMK37 && !defined(SAADET_SMK37)
#define SAADET_SMK37 1    /* tools/build_base.py --board smk37: instrument.c gets it from the generated fm1.toml's o2_cflags */
#endif
#ifdef SAADET_SMK37
void sd_input_smk(void);
#endif
uint32_t sd_ms(void);
uint32_t sd_ticks(void);
uint32_t sd_lock(void);
void sd_unlock(uint32_t token);
void sd_watchdog(void);
void sd_delay(uint32_t ms);
void sd_reboot(void);
void sd_input(uint32_t *buttons,uint32_t *keys,int32_t *enc);
void sd_lights(const uint8_t *bright,const uint8_t *dim,const uint8_t *masks);
void sd_fill(uint32_t x,uint32_t y,uint32_t w,uint32_t h,uint16_t color);
void sd_blit(uint32_t x,uint32_t y,uint32_t w,uint32_t h,const uint16_t *pixels);
void sd_sync(void);
void sd_lcd_power(uint32_t state);
uint32_t sd_master(void);
int sd_midi_in(uint32_t *packet,int *port);
void sd_midi_out(uint32_t packet);
void sd_midi_thru(uint32_t packet,int port);
int sd_sysex_send(const uint8_t *frame,uint32_t n);
int sd_read(uint32_t off,void *dst,uint32_t n);
int sd_erase(uint32_t off);
int sd_write(uint32_t off,const void *src,uint32_t n);
const uint8_t *sd_flash(uint32_t off);
int sd_flash_ok(void);
uint32_t sd_battery(void);
uint32_t sd_xruns(void);
void saadet_init(void);
void saadet_render(int32_t *lr,uint32_t n,uint32_t gain);
void saadet_frame(void);
void saadet_input_only(void);
void saadet_panic(void);
void saadet_say(const char *a,const char *b);
int saadet_sysex(const uint8_t *p,uint32_t n);
int saadet_expect(const char *what,int *value);

void saadet_sysex_byte(uint8_t byte);

int sd_fault_frame(const uint32_t *frame);
void sd_worker_stop(void);
void sd_worker_crash(void);
int sd_worker_allowed(void);
#ifdef SAADET_SMK37
int sd_boot_failed(void);
#endif
int sd_power_init(void);

int sd_ble_connected(void);
int sd_ble_state(void);
int sd_ble_linked(void);
int sd_ble_available(void);
int sd_ble_enabled(void);
void sd_ble_enable(int on);
void sd_ble_reconnect(void);
int sd_ble_device_state(void);
int32_t sd_ble_pairing_code(void);
int sd_ble_device_count(void);
int sd_ble_device_pick(void);
int sd_ble_device_found(int i,char *name,int *rssi);
const char *sd_ble_device_name(void);
void sd_ble_device_choose(int steps);
const char *sd_ble_device_action(void);
const char *sd_ble_page(int devices,int pairing);
void sd_ble_drop_host(void);
void sd_ble_forget_arm(int on);
int sd_ble_forget_state(void);
void sd_ble_forget_confirm(void);

void sd_panel_set(const uint8_t *buttons,const uint8_t *enc,const int8_t *direction);
void saadet_panel_calibrated(const uint8_t *buttons,const uint8_t *enc,const int8_t *direction);

void sd_usb_state(uint32_t *values);

#ifdef SAADET_SMK37
/* the SMK-37's own controls, from the main loop: the 37 keys by keybed position (0 = C3), their last velocities, the
 * wheels, the pedal, and KNOB BANK, FADER BANK, <, > as bits 0..3 of extra */
typedef struct { uint64_t keys, retrig; uint8_t vel[37]; int32_t bend, mod; uint8_t pedal; uint32_t extra;
                 uint32_t pads, pads_hit; uint8_t pad_vel[16], pad_press[16], fader[4]; } sd_smk_t;
void sd_smk_read(sd_smk_t *s);
void sd_smk_lights(uint32_t extra, const uint32_t *pad_rgb);
void sd_master_set(uint32_t q12);
void sd_smk_config(uint32_t vel, uint32_t trs_off);
#endif

void saadet_deadline(uint32_t us,uint32_t budget);
