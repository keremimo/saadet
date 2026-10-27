/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Instrument integration for fm1-base. Included legacy modules retain their
 * own notices and licences; this file does not relicense those modules. */
#include "bridge.h"

#ifdef PLAT_HOST
#undef MELODEE_DUAL_CORE
#define MELODEE_DUAL_CORE 0
#endif
#define MELODEE_CACHE_RAM 0
#include "../firmware/src/build_options.h"
#ifdef PLAT_HOST
#include <string.h>
#define __attribute__(x)
#endif
#define SAADET_FM1_BASE 1
#define MELODEE_FLASH 1
#define MELODEE_OTA 1
#define MELODEE_USB_AUDIO 0 /* physical streams are owned by fm1-base */
#define MELODEE_CDC 0
#define panel sd_instrument_panel
#define settings sd_instrument_settings
#define bootguard sd_instrument_bootguard
#ifdef APP_VERSION
#define MELODEE_VERSION "v" APP_VERSION
#else
#define MELODEE_VERSION "v0.1.0-base-dev"
#endif
#define FM1_TICKS_PER_US 24u
#ifndef PLAT_HOST
#define FM1_IRQ_TARGET 1
#endif
static volatile uint32_t fm1_ms;
static uint32_t fm1_ticks(void){return sd_ticks();}
static uint32_t irq_save(void){return sd_lock();}
static void irq_restore(uint32_t token){sd_unlock(token);}
static uint32_t sd_irq_depth,sd_irq_token;
static void fm1_irq_off(void){if(!sd_irq_depth++)sd_irq_token=sd_lock();}
static void fm1_irq_on(void){if(sd_irq_depth&&! --sd_irq_depth)sd_unlock(sd_irq_token);}
#ifndef PLAT_HOST
#include "hardware/cpu1.h"
#else
static void fm1_aw_relax(void){}
#endif
#define FM1_ADC_BATT 3u
static int32_t fm1_adc_read(uint32_t channel){(void)channel;return (int32_t)sd_battery();}
static void fm1_wdt_feed(void){sd_watchdog();}
static void fm1_delay_ms(uint32_t n){sd_delay(n);}
static void fm1_reboot(void){sd_reboot();}
#define FM1_NCOL 11u
static const int8_t FM1_KEYMAP[6][FM1_NCOL]={
    {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {5,11,4,10,3,9,2,8,-1,-1,-1},
    {34,35,36,37,38,40,39,13,7,6,12},
    {23,24,25,26,27,28,29,30,31,32,33},
    {0,1,15,14,17,16,19,18,20,21,22},
    {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}
};
static struct {uint32_t buttons,notes;} fm1_in;
static uint32_t sd_pressed,sd_released,sd_notes;
static int32_t sd_enc[7];
static uint8_t fm1_led[11],fm1_led_dim[3][11],fm1_led_dim_mask[3];
static uint32_t fm1_input_edges(uint32_t *released){uint32_t p=sd_pressed;*released=sd_released;sd_pressed=sd_released=0;return p;}
static uint32_t fm1_input_note_edges(void){uint32_t p=sd_notes;sd_notes=0;return p;}
static int32_t fm1_enc_take(uint32_t i){int32_t n=sd_enc[i%7u];sd_enc[i%7u]=0;return n;}
static void lcd_fill(uint32_t x,uint32_t y,uint32_t w,uint32_t h,uint16_t c){sd_fill(x,y,w,h,c);}
#define lcd_blit sd_blit
#define lcd_sync sd_sync
#define UI_LCD_POWER(s) sd_lcd_power(s)
#define UI_LCD_WAKE() sd_lcd_power(2)
#define memset saadet_memset
#define memcpy saadet_memcpy
#define memcmp saadet_memcmp
#include "../firmware/src/libc.c"
#include "melodee_tables.h"
#include "../firmware/src/gfx.c"
#include "../firmware/src/core.h"
#include "../firmware/src/engines.c"
#include "../firmware/src/params.c"
#include "../firmware/src/mod.c"
#include "../firmware/src/voice.c"
#include "../firmware/src/mix.c"

#define MQ 128u
static uint32_t midi_in_q[MQ],midi_in_ms[MQ],midi_in_time[MQ];
static uint8_t midi_in_source[MQ];
/* Reception counts survive note-queue overflow; both BLE links share CLK=BLE. */
static uint16_t midi_in_pulse[MQ],midi_rx_pulses[3];
static volatile uint32_t mi_w,mi_r;
static volatile uint8_t midi_in_overflow;
static uint32_t midi_render_time;
static uint8_t midi_render_timed;
#define MIDI_TICKS_PER_MS 24000u
#define MIDI_TIME_NOW() fm1_ticks()
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")
static void midi_out_event(uint32_t p){sd_midi_out(p);}
/* Legacy editor framing over fm1-base's SysEx transport, without ota.c. */
static struct {uint32_t config,resets,up,suspended,rx_pkts;} usb={1,0,1,0};

#define SXQ 128u
static uint32_t so_w,so_r;
static int ota_wire_send(const uint8_t *p,uint32_t n){return sd_sysex_send(p,n);}
#include "../firmware/src/seq.c"
#include "../firmware/src/panel.c"
#include "../firmware/src/sound.c"
#include "../firmware/src/ui.c"
static uint8_t flash_ok;
static int st_read(uint32_t off,void *p,uint32_t n){return sd_read(off,p,n);}
static int st_erase(uint32_t off){return sd_erase(off);}
static int st_prog(uint32_t off,const void *p,uint32_t n){return sd_write(off,p,n);}
static uint32_t fl_jedec_ram(void){return sd_flash_ok()?0x856014u:0;}
static void fl_plain_window_init(void){}
#define FL_FAR(fn) (fn)
#include "../firmware/src/storage.c"
#include "../firmware/src/upreset.c"
#include "../firmware/src/persist.c"
#include "../firmware/src/cz_store.c"
#include "../firmware/src/fm6_store.c"

static int ota_frame_get(const uint8_t **p,uint32_t *n){(void)p;(void)n;return 0;}
static void ota_frame_done(void){}
#define ED_BK_FLASH_PTR(off) sd_flash(off)
/* editor command 72: the longest render the base measured (saadet_deadline), the overload counters, and the
 * cache-RAM status (off here) */
static uint32_t sd_render_max_us;
#include "deadline.h"
#include "../firmware/hal/fm1_cache.h"
#include "../firmware/src/editor.c"

#include "midi_ingress.h"

static void sd_poll(void)
{
    uint32_t u[5];sd_usb_state(u);usb.config=u[0];usb.resets=u[1];usb.up=u[2];usb.suspended=u[3];usb.rx_pkts=u[4];
    uint32_t buttons,keys;int32_t enc[7];sd_input(&buttons,&keys,enc);
    fm1_ms=sd_ms();sd_pressed|=buttons&~fm1_in.buttons;sd_released|=fm1_in.buttons&~buttons;
    sd_notes|=keys&~fm1_in.notes;fm1_in.buttons=buttons;fm1_in.notes=keys;
    for(uint32_t i=0;i<7u;i++)sd_enc[i]+=enc[i];
    uint32_t packet;int port;
    while(sd_midi_in(&packet,&port)){
        if(settings_thru)sd_midi_thru(packet,port);
        sd_ingress(packet,(uint32_t)(port==3?4:port==2?2:port==1?3:1));
    }
    sd_connections();
}
void saadet_init(void)
{
    fm1_ms=sd_ms();dsp_tables_init();
    mi_w=mi_r=0;midi_in_overflow=0;
    memset(midi_rx_pulses,0,sizeof midi_rx_pulses);
    memset(sd_midi_keys,0,sizeof sd_midi_keys);memset(sd_midi_pedal,0,sizeof sd_midi_pedal);
    midi_input_reset();memset(mchord,0,sizeof mchord);sd_ble_was_up=sd_thru_was=0;
    memset(trk,0,sizeof trk);memset(&song,0,sizeof song);eng_state_reset();
    memset(&ui,0,sizeof ui);memset(&uix,0,sizeof uix);uix.held=PL_N;uix.hot=4;uix.eng=255;
    panel=PANEL_DEFAULT;settings_init();persist_boot();panel_init();settings_init();sd_panel_set(panel.btn,panel.enc,panel.dir);
    fm6_init();cz_init();
    for(uint32_t i=0;i<G_COUNT;i++)song.g[i]=GP[i].def;
    track_defaults(TSEL);set_engine_of(TSEL,TRK_DEF[0][0]);apply_preset_to(TSEL,TRK_DEF[0][1]);
    TSEL->engine=TSEL->eng_req;song.master_q12=2048;state_boot();
    ui.force=1;sd_poll();
#ifndef PLAT_HOST
    uint32_t mult=fm1_clock_mult();vbudget_cpu1=VBUDGET*(mult?fm1_clock_mhz_of(mult):360u)/360u;
    if(sd_worker_allowed()&&!fm1_core1_barred())fm1_multicore_start();
#endif
}
void saadet_render(int32_t *out,uint32_t n,uint32_t gain)
{
    fm1_ms=sd_ms();song.master_q12=(int32_t)gain;
    if(sd_shed_pending){sd_shed_pending=0;if(settings_safety)shed_voice();}
#if MELODEE_DUAL_CORE
    audio_worker_check();
#endif
    uint32_t t0=fm1_ticks();midi_render_timed=1;
    for(uint32_t at=0;at<n;at+=CTL){
        uint32_t frames=n-at<CTL?n-at:CTL;
        midi_render_time=t0-(n-at)*544u;
        mix_block(out+2u*at,frames);
    }
    midi_render_timed=0;
}
void saadet_input_only(void){sd_poll();ui_input();}
void saadet_frame(void)
{
    sd_poll();song.batt_raw=(int16_t)sd_battery();
    ui_input();ui_leds();ui_draw();sd_lights(fm1_led,&fm1_led_dim[0][0],fm1_led_dim_mask);
    settings_poll();panel_moved=0;cz_service();fm6_service();cz_bank_poll();ed_sync();
}
void saadet_panic(void){panic_req|=(1u<<NTRK)-1u;}
void saadet_say(const char *a,const char *b){ui_say(a,b);ui.force=1;}
int saadet_sysex(const uint8_t *p,uint32_t n)
{
    if(n>=4&&p[0]==ED_HDR0&&p[1]==ED_HDR1&&p[2]==ED_HDR2){ed_w.last_ms=sd_ms();ed_handle(p,n);return 1;}
    if(general_sysex_frame(p,n)||p5_native_frame(p,n)||j106_native_frame(p,n))return 1;
    return 0;
}
int saadet_expect(const char *s,int *v)
{
    if(str_eq(s,"engine")){*v=TSEL->eng_req;return 1;}
    if(str_eq(s,"held")){*v=(int)(midi_held_count(0)+TSEL->nheld);return 1;}
    if(str_eq(s,"clock_source")){*v=song.g[G_CLOCK];return 1;}
    if(str_eq(s,"clock")){*v=midi_clock.mode;return 1;}
    if(str_eq(s,"playing")){*v=song.playing;return 1;}
    if(str_eq(s,"ble")){*v=sd_ble_connected();return 1;}
    if(str_eq(s,"ble_linked")){*v=sd_ble_linked();return 1;}
    if(str_eq(s,"ble_state")){*v=sd_ble_state();return 1;}
    if(str_eq(s,"ble_enabled")){*v=sd_ble_enabled();return 1;}
    if(str_eq(s,"ble_device")){*v=sd_ble_device_state();return 1;}
    if(str_eq(s,"ble_found")){*v=sd_ble_device_count();return 1;}
    if(str_eq(s,"ble_pick")){*v=sd_ble_device_pick();return 1;}
    if(str_eq(s,"ble_forget")){*v=sd_ble_forget_state();return 1;}
    if(str_eq(s,"thru")){*v=settings_thru;return 1;}
    if(str_eq(s,"engines")){*v=NENG_SHOWN;return 1;}
    return 0;
}

void saadet_sysex_byte(uint8_t byte){fm6_sx_byte(byte);cz_sx_byte(byte);}

void saadet_panel_calibrated(const uint8_t *buttons,const uint8_t *enc,const int8_t *direction){
    memcpy(panel.btn,buttons,14u);memcpy(panel.enc,enc,7u);memcpy(panel.dir,direction,7u);
    panel.magic=PANEL_MAGIC;panel_init();settings_save();
}
