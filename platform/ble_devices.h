/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Saadet's adapter to Charles Vestal's unchanged fm1-base btdev helper.
 * The helper and JieLi MIDI client retain their upstream notices. */
#include "btdev.h"
#include "ble_reconnect.h"
static uint8_t sd_radio_on=1,sd_forget,sd_forget_resume;
static uint32_t sd_forget_at;
int sd_ble_connected(void){return plat_ble_connected();}
int sd_ble_state(void){return plat_ble_state();}
int sd_ble_linked(void){return plat_ble_linked();}
int sd_ble_available(void){return FM1B_BLE;}
int sd_ble_enabled(void){return FM1B_BLE&&sd_radio_on;}
int sd_ble_device_state(void){return plat_ble_device();}
int32_t sd_ble_pairing_code(void){
#if FM1B_BLE && !defined(PLAT_HOST)
    extern int32_t ble_midi_pairing_code(void);
    return ble_midi_pairing_code();
#else
    return -1;
#endif
}
int sd_ble_device_count(void){return btdev_count();}
int sd_ble_device_pick(void){return btdev_pick();}
int sd_ble_device_found(int i,char *name,int *rssi){return btdev_found(i,name,rssi);}
const char *sd_ble_device_name(void){return sd_bt_name();}
void sd_ble_enable(int on){
    if(sd_forget==2)return;
    if(on)sd_bt_resume();else sd_bt_pause();
    sd_radio_on=(uint8_t)!!on;plat_ble_enable(on);
}
void sd_ble_drop_host(void){
#if FM1B_BLE && !defined(PLAT_HOST)
    extern void ble_midi_drop_host(void);
    if(plat_ble_state()==BLE_READY)ble_midi_drop_host();
#elif FM1B_BLE
    ble_conn=ble_link=0;
#endif
}
void sd_ble_reconnect(void){
    if(sd_forget==2)return;
    sd_ble_enable(1);
#if FM1B_BLE && !defined(PLAT_HOST)
    extern void ble_midi_reconnect(void);
    if(plat_ble_state()==BLE_READY)ble_midi_reconnect();
#elif FM1B_BLE
    sd_ble_drop_host();
#endif
}
void sd_ble_device_choose(int steps){
    if(sd_forget!=2&&!btdev_busy())btdev_choose(btdev_pick()+(steps>0?1:-1));
}
const char *sd_ble_device_action(void){
    if(sd_forget==2||plat_ble_state()!=BLE_READY)return "RADIO NOT READY";
    if(btdev_busy())sd_bt_pause();else sd_bt_resume();
    return btdev_word(btdev_save());
}
void sd_ble_forget_arm(int on){
    if(sd_forget==2)return;
    sd_forget=(uint8_t)(on&&plat_ble_state()==BLE_READY);sd_forget_at=plat_ms();
}
int sd_ble_forget_state(void){return sd_forget;}
#if FM1B_BLE_CENTRAL && !defined(PLAT_HOST)
extern int saadet_ble_cal(const uint8_t *copy,uint8_t *st);
int sd_ble_cal(const uint8_t *copy,uint8_t *st){return saadet_ble_cal(copy,st);}
#else
int sd_ble_cal(const uint8_t *copy,uint8_t *st){st[0]=st[1]=st[2]=st[3]=0;return copy?5:0;}   /* (no radio of ours) */
#endif
void sd_ble_forget_confirm(void){
    if(sd_forget!=1||plat_ms()-sd_forget_at>=15000u)return;
    sd_forget=2;sd_forget_at=plat_ms();sd_forget_resume=sd_radio_on;
    sd_bt_pause();
    plat_ble_enable(0); /* let the link finish before removing the bonds */
}
const char *sd_ble_page(int devices,int pairing){
    const char *message="";
    if(sd_forget==1&&(!pairing||plat_ms()-sd_forget_at>=15000u))sd_forget=0;
    if(sd_forget==2){
        int ready=0,ok=0;
#if FM1B_BLE && !defined(PLAT_HOST)
        extern int ble_midi_can_forget(void),ble_midi_forget_all(void);
        ready=ble_midi_can_forget();
#else
        ready=1;
#endif
        if(ready&&plat_ms()-sd_forget_at>=250u){
#if FM1B_BLE && !defined(PLAT_HOST)
            ok=ble_midi_forget_all();
#else
            ok=1;
#endif
            if(ok)ok=sd_bt_forget();
            sd_forget=0;plat_ble_enable(sd_forget_resume);
            message=ok?"PAIRINGS CLEARED":"FORGET FAILED";
        }else if(plat_ms()-sd_forget_at>=6000u){
            sd_forget=0;plat_ble_enable(sd_forget_resume);message="FORGET FAILED";
        }
    }
    btdev_page(devices&&sd_radio_on&&sd_forget!=2);
    sd_bt_hold(pairing);                          /* GLOBAL > BLE open: no rounds looking for the last device */
    if(!message[0])message=sd_bt_message();
    if(!btdev_busy())btdev_choose(btdev_pick()); /* a new scan can have fewer devices */
    for(int event;(event=btdev_event())!=BTDEV_NONE;){
        if(!message[0])message=btdev_word(event);
    }
    return message;
}
