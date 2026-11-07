/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#define SAADET_FM1_BASE 1
static void sd_reset_intended(void){}   /* (the base's boot guard: platform/app.c) */
static int radio_available=1,radio_on=1,radio_state=2,radio_link,radio_midi,retries;
static int sd_ble_available(void){return radio_available;}
static int sd_ble_enabled(void){return radio_on;}
static int sd_ble_state(void){return radio_state;}
static int sd_ble_linked(void){return radio_link;}
static int sd_ble_connected(void){return radio_midi;}
static void sd_ble_enable(int on){radio_on=on;radio_state=on?2:0;if(!on)radio_link=radio_midi=0;}
static void sd_ble_reconnect(void){retries++;sd_ble_enable(0);sd_ble_enable(1);}
static int device_state,device_pick,device_count,forget_state;
static int32_t pair_code=-1;
static int32_t sd_ble_pairing_code(void){return pair_code;}
static int sd_ble_device_state(void){return device_state;}
static int sd_ble_device_count(void){return device_count;}
static int sd_ble_device_pick(void){return device_pick;}
static const char *sd_ble_device_name(void){return "External MIDI Device";}
static int sd_ble_device_found(int i,char *name,int *rssi){
    if(i<0||i>=device_count)return 0;
    if(name)snprintf(name,21,"MIDI Device %d",i+1);
    if(rssi)*rssi=-50-i*5;return 1;
}
static void sd_ble_device_choose(int steps){if(device_count){device_pick+=steps>0?1:-1;if(device_pick<0)device_pick=0;if(device_pick>=device_count)device_pick=device_count-1;}}
static const char *sd_ble_device_action(void){device_state=device_state>=2?0:2;return "";}
static const char *sd_ble_page(int devices,int pairing){(void)devices;if(!pairing&&forget_state==1)forget_state=0;return "";}
static void sd_ble_drop_host(void){radio_link=radio_midi=0;}
static void sd_ble_forget_arm(int on){forget_state=on;}
static int sd_ble_forget_state(void){return forget_state;}
static void sd_ble_forget_confirm(void){if(forget_state==1)forget_state=2;}
#include "ui_harness.c"
static void screen_check(const char *status,const char *image){
    frame();if(!lint_has(status)){fprintf(stderr,"Missing %s\n",status);for(unsigned i=0;i<lint_ns;i++)fprintf(stderr,"text %s\n",lint_scr[i].s);}assert(lint_has(status));assert(!lint_check(image));ui_png(image);
}
int main(void){
    power_on();frames(800);ui_open(PL_GLO);
    uint32_t page=0;while(page<NELEM(ST_GLO)&&ST_GLO[page].pic!=PIC_BLE)page++;
    assert(page<NELEM(ST_GLO));uix.stop[PL_GLO]=(uint8_t)page;
    screen_check("ADVERTISING","ble_advertising");
    radio_link=1;screen_check("WAITING FOR MIDI","ble_link_only");
    assert(lint_has("Audio MIDI Setup"));
    radio_midi=1;screen_check("MIDI CONNECTED","ble_midi_ready");
    turn(EN_K4,-1);assert(!retries&&radio_midi);
    turn(EN_K4,1);assert(retries==1&&!radio_link&&!radio_midi&&radio_on);
    screen_check("ADVERTISING","ble_retry");
    turn(EN_K1,-1);screen_check("OFF","ble_off");assert(!radio_on);
    turn(EN_K1,1);screen_check("ADVERTISING","ble_on");
    radio_state=0;screen_check("STOPPED","ble_stopped");
    turn(EN_K4,1);assert(radio_state==2&&retries==2);
    radio_state=1;screen_check("STARTING","ble_starting");
    radio_state=3;screen_check("FAILED","ble_failed");
    radio_available=0;screen_check("UNAVAILABLE","ble_unavailable");
    turn(EN_K4,1);assert(retries==2);
    radio_available=1;radio_state=2;
    device_state=2;screen_check("DEVICE LINK IN USE","ble_device_link_in_use");
    assert(lint_has("Disconnect the device"));device_state=0;
    turn(EN_K3,1);screen_check("FORGET ALL PAIRINGS?","ble_forget_armed");
    turn(EN_K3,-1);assert(!forget_state);
    turn(EN_K3,1);press(B_SAVE);assert(forget_state==2&&uix.place==PL_GLO);
    screen_check("FORGETTING...","ble_forgetting");forget_state=0;
    uix.stop[PL_GLO]++;device_state=1;device_count=8;
    screen_check("MIDI DEVICES","ble_devices");assert(lint_has("SAVE: CONNECT"));
    for(unsigned i=0;i<7;i++)turn(EN_K1,1);
    screen_check("MIDI DEVICES","ble_devices_scrolled");assert(device_pick==7&&lint_has("MIDI Device 8"));
    press(B_SAVE);assert(device_state==2&&uix.place==PL_GLO);
    screen_check("CONNECTING","ble_device_connecting");
    device_state=3;screen_check("FINDING MIDI","ble_device_searching");
    pair_code=123;screen_check("PAIRING CODE","ble_device_pairing");
    assert(lint_has("000123")&&lint_has("Enter on other device"));
    pair_code=0;screen_check("000000","ble_device_pairing_zero");
    pair_code=999999;screen_check("999999","ble_device_pairing_max");
    pair_code=-1;screen_check("FINDING MIDI","ble_device_pairing_done");
    device_state=4;pair_code=123;screen_check("CONNECTED","ble_device_connected");
    assert(!lint_has("PAIRING CODE"));pair_code=-1;
    press(B_SAVE);assert(device_state==0&&uix.place==PL_GLO);
    device_state=4;turn(EN_K4,1);assert(device_state==0);
    device_count=0;screen_check("LOOKING FOR DEVICES","ble_devices_empty");
    radio_link=1;screen_check("COMPUTER CONNECTED","ble_devices_computer");radio_link=0;
    puts("BLE UI: live state, link without MIDI, radio controls, retry and screen layout passed");
}
