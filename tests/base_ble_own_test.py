#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Run Saadet's Bluetooth rules (platform/ble_own.h) against a fake link layer, on the stack's real headers, device
store, scan table and calibration reader: stored or measured calibration, the last device looked for and found, never a
new pairing loop, bounded attempts, and the calls the pages use. Then the stack in platform/ble against its sources."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'build/fm1-base-platform'
BLE = ROOT / 'platform/ble'

central = (BASE / 'device/bt_own_central.c').read_text()
own = (BASE / 'device/bt_own.c').read_text()
for needed in ("sd_own_keys(k);", "sd_own_poll();", "!sd_relink_scanning()"):
    assert needed in central, f'the overlay lost {needed}'
for gone in ("void plat_ble_scan(int on)", "int plat_ble_connect(int i)"):
    assert gone not in central, f'the bridge has {gone}, and the base still does'
for needed in ('#include "saadet_ble_own.h"', '#include "ble_vm.c"', 'sd_own_trims(&trims);', 'ble_central_cancel();',
               '#define BLE_LL_MAX_OCTETS 27u', '#define BLE_MIDI_INITIAL_READ 1', '#define BLE_DIAG 0'):
    assert needed in own, f'the overlay lost {needed}'
assert 'bd.up' in (BASE / 'lib/btdev.c').read_text().split('plat_ble_scan(0)')[0][-120:], 'the overlay lost the end of a scan wish'
for f in BLE.iterdir():
    if f.is_file():
        assert (BASE / 'bt/ble' / f.name).read_bytes() == f.read_bytes(), f'bt/ble/{f.name} is not platform/ble/{f.name}'
for f in (BLE / 'hal').iterdir():
    assert (BASE / 'bsp/chip' / f.name).read_bytes() == f.read_bytes(), f'bsp/chip/{f.name} is not platform/ble/hal/{f.name}'
# What each side fixed, both in (Melodee's changes; fm1-base's link fixes): the marks of each.
hw = (BLE / 'ble_hw_wl82.c').read_text()
assert 'return w < 16u ? 16u : w;' in hw and '(st & 0xFu) == 1u' in hw and '2u * drv.widening + HW_WIN_NORMAL' in hw, 'fm1-base link fixes'
assert 'static struct ble_bb bb BLE_DMA_MEMORY;' in hw, 'Melodee: the baseband block in the pool'
assert 'ble_host_new_key' in (BLE / 'ble_smp.c').read_text() and 'settle_wait' in (BLE / 'ble_central.c').read_text()
assert 'BLE_MIDI_INITIAL_READ' in (BLE / 'ble_gattc.c').read_text()

code = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define BLE_API static
#define BLE_READY 2
#define BLE_ERR_REMOTE_USER 0x13
#define RING_PUBLISH() ((void)0)
#include "ble.h"
#include "ble_util.h"
#include "ble_scan.h"
#include "ble_store.h"
#include "ble_vm.h"
#include "ble_scan.c"
#include "ble_store.c"
#include "ble_vm.c"
enum { OBJ_BLE = 1, OBJ_BT_LAST = 2, PORT_BLE = 1 };
static uint32_t now;
static uint8_t st,fail,pairing,own_role,ble_state=BLE_READY,own_dirty,held,fm1_rf_cal_on,ble_off,key_refused;
static unsigned cancels,connects,refuse,disconnects,enables,scans_on,scans_off,smp_resets,store_fail,saves,saves_ble;
static int enabled=1,linked,passkey=-1,scanning,midi_count,midi_ago=100000;
static struct ble_peer last;
static struct { uint8_t valid,rand[8],ltk[16]; uint16_t ediv; } bhs_key;
static struct { uint8_t have_bond,bond_rand[8],bond_ltk[16],sd_rf[BLE_RF_COPY_SIZE]; uint16_t bond_ediv; } own;
static const struct ble_rf_trims OWN_TRIMS={{0x0B,0x0B},{1,7,4,7,11,1,7},{0},{0}};
static struct ble_scan_tab own_scan;
static uint8_t flash[0x100000],stored[80];
static int stored_len=-1;
static const uint8_t *resolves;                    /* the address the fake IRK resolves */
static uint32_t shim_ms(void){return now;}
static void shim_log(const char *s){(void)s;}
static void own_held(int h){held=(uint8_t)h;}
static int st_read(uint32_t off,void *dst,uint32_t n){memcpy(dst,flash+off,n);return 0;}
static uint8_t ble_central_state(void){return st;}
static uint8_t ble_central_fail(void){return fail;}
static int ble_central_pairing(void){return pairing;}
static void ble_central_cancel(void){assert(held);cancels++;st=BLE_CS_IDLE;}
static int ble_central_connect(const struct ble_peer *p){
    assert(held);
    if(refuse||linked)return 0;
    connects++;last=*p;st=BLE_CS_CONNECTING;fail=BLE_CF_NONE;scanning=0;return 1;
}
static int ble_connected(void){return linked;}
static void ble_enable(int on){assert(held);enabled=on;enables++;}
static void ble_ll_disconnect(uint8_t why){assert(held&&why==BLE_ERR_REMOTE_USER);disconnects++;}
static void ble_ll_scan(int on){assert(held);if(on){scans_on++;scanning=1;}else{scans_off++;scanning=0;}}
static int ble_rpa_resolve(const uint8_t irk[16],const uint8_t addr[6]){(void)irk;return resolves&&!memcmp(addr,resolves,6);}
static int ble_host_key_refused(void){int r=key_refused;key_refused=0;return r;}
static void ble_smp_reset(void){assert(held);smp_resets++;}
static int plat_ble_passkey(void){return passkey;}
static int plat_midi_count(int p,int o){(void)p;(void)o;return midi_count;}
static int plat_midi_ago(int p,int o){(void)p;(void)o;return midi_ago;}
static int plat_store_load(uint32_t o,void *p,uint32_t n){
    assert(o==OBJ_BT_LAST);if(stored_len<0)return -1;
    if((uint32_t)stored_len<n)n=(uint32_t)stored_len;
    memcpy(p,stored,n);return stored_len;
}
static int plat_store_save(uint32_t o,const void *p,uint32_t n){
    if(o==OBJ_BLE){saves_ble++;return store_fail?-1:0;}
    assert(o==OBJ_BT_LAST&&n<=sizeof stored);saves++;
    if(store_fail)return -1;
    memcpy(stored,p,n);stored_len=(int)n;return 0;
}
#include "ble_own.h"

/* one ADV_IND with the BLE-MIDI service and a name, through the real parser */
static void heard(uint8_t id,const char *name){
    static const uint8_t uuid[16]={0x00,0xC7,0xC4,0x4E,0xE3,0x6C,0x51,0xA7,0x33,0x4B,0xE8,0xED,0x5A,0x0E,0xB8,0x03};
    uint8_t p[40];uint32_t n=0,len=(uint32_t)strlen(name);
    p[n++]=0x40;p[n++]=0;                          /* ADV_IND, TxAdd random */
    p[n++]=id;for(int i=1;i<5;i++)p[n++]=0;p[n++]=42;
    p[n++]=2;p[n++]=1;p[n++]=6;
    p[n++]=17;p[n++]=7;memcpy(p+n,uuid,16);n+=16;
    p[n++]=(uint8_t)(len+1);p[n++]=9;memcpy(p+n,name,len);n+=len;
    p[1]=(uint8_t)(n-2);
    ble_scan_add(&own_scan,p,n,0,now);
}
static void at(uint32_t t){now=t;sd_own_poll();}
static void boot(void){                            /* a power cycle: RAM gone, the store kept */
    memset(&sd_own,0,sizeof sd_own);memset(&own_scan,0,sizeof own_scan);
    st=BLE_CS_IDLE;fail=pairing=0;scanning=0;linked=0;
}
/* the record a VM's log holds: [check][id 7:0][id 11:8 | len 3:0 << 4][len 11:4], the data */
static uint32_t vm_rec(uint32_t at,uint32_t id,const uint8_t *d,uint32_t n){
    flash[at]=(uint8_t)ble_crc16_xmodem(0,d,n);
    flash[at+1]=(uint8_t)id;flash[at+2]=(uint8_t)(id>>8|(n&15u)<<4);flash[at+3]=(uint8_t)(n>>4);
    memcpy(flash+at+4,d,n);
    return at+4+n;
}
static void vm_write(uint8_t mark){
    uint8_t x106[2]={mark,1},x107[7]={2},x108[20]={3},x187[68];uint32_t c,a=BLE_VM_HW;
    memset(x187,mark,64);c=ble_crc16_xmodem(0,x187,64);x187[64]=(uint8_t)c;x187[65]=(uint8_t)(c>>8);x187[66]=x187[67]=0;
    flash[a]=0x55;flash[a+1]=0xAA;flash[a+2]=0xAA;flash[a+3]=0x55;a+=4;
    a=vm_rec(a,106,x106,2);a=vm_rec(a,107,x107,7);a=vm_rec(a,187,x187,68);a=vm_rec(a,108,x108,20);
    flash[a]=0;flash[a+1]=0;flash[a+2]=0;flash[a+3]=0;
}
int main(void){
    struct ble_rf_trims t;
    memset(flash,0xFF,sizeof flash);

    /* ---- the calibration: nothing stored -> the board's constants and a measurement */
    fm1_rf_cal_on=0;sd_own_trims(&t);
    assert(fm1_rf_cal_on&&t.x106[0]==0x0B&&t.x107[4]==11&&!own_dirty);
    /* the stock trims still there: used, and a copy kept with the address */
    vm_write(0x21);sd_own_trims(&t);
    assert(!fm1_rf_cal_on&&t.x106[0]==0x21&&t.x187[0]==0x21&&own_dirty&&own.sd_rf[0]);
    /* ... which outlives them: Saadet's own presets over that sector */
    memset(flash+BLE_VM_HW,0x5A,BLE_VM_HW_SIZE);own_dirty=0;fm1_rf_cal_on=1;sd_own_trims(&t);
    assert(!fm1_rf_cal_on&&t.x106[0]==0x21&&!own_dirty);
    /* a damaged copy is no copy */
    own.sd_rf[10]^=0xFF;sd_own_trims(&t);assert(fm1_rf_cal_on&&t.x106[0]==0x0B);
    memset(flash,0xFF,sizeof flash);
    puts("own stack: stored calibration first, kept past its sector, else measured");

    /* ---- nothing remembered: no scan, no link, no flash */
    boot();stored_len=-1;at(1000);at(100000);assert(!scans_on&&!connects&&!saves);
    /* picked from the page: a link, its name known, the last device only once it is READY */
    scanning=1;heard(7,"Keys");assert(!plat_ble_connect(0));
    assert(connects==1&&sd_own.attempt&&last.addr[0]==7&&!last.bonded);
    assert(!strcmp(saadet_ble_device_name(),"Keys"));
    assert(plat_ble_connect(0)==-1);               /* one at a time */
    at(100000+SD_ATTEMPT_MS);assert(!cancels);
    /* no answer: given up, with the radio back for a computer; a failed pick never replaces the last device */
    at(100000+SD_ATTEMPT_MS+1);assert(cancels==1&&!sd_own.attempt&&st==BLE_CS_IDLE&&!saves);
    assert(!strcmp(saadet_ble_message(),"CONNECTION TIMEOUT"));assert(!*saadet_ble_message());
    assert(!saadet_ble_device_name());

    /* the same device, this time it answers and pairs: while the code waits the limit is longer */
    now=200000;own_scan=(struct ble_scan_tab){0};heard(7,"Keys");assert(!plat_ble_connect(0));
    st=BLE_CS_SETUP;pairing=1;at(201000);assert(sd_own.deadline==201000);
    at(201000+SD_ATTEMPT_MS+1);assert(sd_own.attempt&&cancels==1);
    /* its keys come in the interrupts' context and are saved from the main loop, once */
    { struct ble_keys k;memset(&k,0,sizeof k);k.has=BLE_KEYS_LTK|BLE_KEYS_AUTH;memset(k.ltk,0xA5,16);memset(k.rand,0x3C,8);k.ediv=0x1234;
      sd_own_keys(&k);assert(sd_own.keys_new); }
    now=202000;st=BLE_CS_READY;pairing=0;sd_own_poll();
    assert(ble_store_has_last(&sd_own.store)&&saves==1&&sd_own.relink&&(sd_own.store.dev.info&BLE_DEV_BONDED));
    at(203000);at(204000);assert(saves==1);        /* no flash wear on repeated frames */
    assert(stored_len==BLE_DEV_STORE_SIZE);
    { uint8_t got[6],same[6]={7,0,0,0,0,42};memcpy(got,sd_own.store.dev.addr,6);assert(!memcmp(got,same,6)); }

    /* ---- a power cycle: looked for at once, a link only when it is heard, with its bond */
    boot();unsigned on=scans_on,off=scans_off,c=connects;
    at(300000);assert(scans_on==on+1&&sd_own.relink_scan&&connects==c);
    at(300000+SD_RELINK_SCAN_MS-1);assert(scanning);
    at(300000+SD_RELINK_SCAN_MS);assert(!scanning&&scans_off==off+1&&connects==c);   /* not heard: advertising back */
    at(300000+SD_RELINK_MS-1);assert(scans_on==on+1);
    at(300000+SD_RELINK_MS);assert(scans_on==on+2&&scanning);                         /* the next round */
    now+=1000;heard(9,"Other");sd_own_poll();assert(connects==c);                     /* another device: not this one */
    now+=1000;heard(7,"Keys");sd_own_poll();
    assert(connects==c+1&&!scanning&&last.addr[0]==7&&last.bonded==1&&last.ediv==0x1234&&last.ltk[15]==0xA5&&(last.sec&BLE_PEER_AUTH));
    st=BLE_CS_READY;now+=100;sd_own_poll();
    /* lost: looked for again; while linked, no rounds */
    { unsigned n=scans_on;now+=SD_RELINK_MS*2;sd_own_poll();assert(scans_on==n);
      own_scan=(struct ble_scan_tab){0};st=BLE_CS_IDLE;fail=BLE_CF_LOST;sd_own_poll();
      assert(sd_own.relink&&!sd_own.attempt&&!*saadet_ble_message()&&scans_on==n+1&&sd_own.relink_scan); }

    /* behind a private address: its IRK finds it, the bond goes with the new address */
    { uint8_t rpa[6]={0x77,0,0,0,0,42};
      memset(sd_own.store.dev.irk,1,16);sd_own.store.dev.info|=BLE_DEV_IRK;resolves=rpa;
      own_scan=(struct ble_scan_tab){0};heard(0x77,"Keys");c=connects;now+=1000;sd_own_poll();
      assert(connects==c+1&&last.addr[0]==0x77&&last.bonded==1);st=BLE_CS_READY;now+=100;sd_own_poll();resolves=0; }

    /* a refused pairing or wrong key is not looked for again: no prompt on the phone every 30 s */
    { unsigned n=scans_on;st=BLE_CS_IDLE;fail=BLE_CF_PAIRING;now+=100;sd_own_poll();
      assert(!sd_own.relink&&!strcmp(saadet_ble_message(),"PAIRING FAILED"));
      now+=SD_RELINK_MS*3;sd_own_poll();assert(scans_on==n); }
    /* a code nobody entered: the link goes after the limit, and no second prompt */
    saadet_ble_relink(1);own_scan=(struct ble_scan_tab){0};heard(7,"Keys");
    sd_own_poll();assert(sd_own.attempt);
    st=BLE_CS_SETUP;pairing=1;now+=100;sd_own_poll();
    { unsigned n=cancels;now+=SD_PAIRING_MS;sd_own_poll();assert(cancels==n);
      now+=1;sd_own_poll();assert(cancels==n+1&&!sd_own.relink&&!sd_own.attempt);pairing=0;
      assert(!strcmp(saadet_ble_message(),"CONNECTION TIMEOUT")); }
    /* a bond it does not know: dropped and saved, the user told */
    saadet_ble_relink(1);own_scan=(struct ble_scan_tab){0};heard(7,"Keys");sd_own_poll();assert(sd_own.attempt&&last.bonded);
    { unsigned s=saves;st=BLE_CS_IDLE;fail=BLE_CF_KEY;now+=100;sd_own_poll();
      assert(!sd_own.relink&&!(sd_own.store.dev.info&BLE_DEV_BONDED)&&saves==s+1);
      assert(!strcmp(saadet_ble_message(),"FORGET ON PEER")); }
    /* one that needs a passkey: one new link that asks for it; refused again, that is the end */
    own_scan=(struct ble_scan_tab){0};heard(7,"Keys");assert(!plat_ble_connect(0));assert(!last.bonded);
    { unsigned n=connects;st=BLE_CS_IDLE;fail=BLE_CF_NEED_MITM;now+=100;sd_own_poll();
      assert(connects==n+1&&(last.sec&BLE_PEER_MITM)&&!last.bonded&&sd_own.attempt);
      st=BLE_CS_IDLE;fail=BLE_CF_NEED_MITM;now+=100;sd_own_poll();
      assert(connects==n+1&&!sd_own.attempt&&!strcmp(saadet_ble_message(),"PAIRING FAILED")); }
    /* a central with a key we do not know */
    key_refused=1;now+=100;sd_own_poll();assert(!strcmp(saadet_ble_message(),"FORGET ON PEER"));

    /* ---- what stops the rounds: the device page (its scan stays its own), GLOBAL > BLE, a computer, the radio off */
    own_scan=(struct ble_scan_tab){0};saadet_ble_relink(1);now+=100;sd_own_poll();assert(sd_own.relink_scan&&scanning&&sd_relink_scanning());
    { unsigned o=scans_off;plat_ble_scan(1);assert(sd_own.page&&!sd_own.relink_scan&&scanning);
      now+=SD_RELINK_MS*2;sd_own_poll();assert(scanning&&scans_off==o);   /* the page's, not ended by the round's clock */
      plat_ble_scan(0);assert(!sd_own.page&&!scanning); }
    saadet_ble_hold(1);now+=SD_RELINK_MS;sd_own_poll();assert(!scanning&&!sd_own.relink_scan);saadet_ble_hold(0);
    linked=1;now+=SD_RELINK_MS;sd_own_poll();assert(!scanning);linked=0;
    ble_off=1;now+=SD_RELINK_MS;sd_own_poll();assert(!scanning);ble_off=0;
    saadet_ble_relink(0);now+=SD_RELINK_MS*2;sd_own_poll();assert(!scanning);/* disconnected by hand: not looked for */

    /* ---- a write waits for MIDI to pause, a failed one is retried, boundedly */
    sd_own.dirty=1;sd_own.save_failed=0;midi_count=1;midi_ago=100;{ unsigned s=saves;now+=10;sd_own_poll();assert(saves==s);
      midi_ago=2000;store_fail=1;now+=10;sd_own_poll();assert(saves==s+1&&sd_own.dirty);
      now+=SD_RETRY_SAVE_MS-1;sd_own_poll();assert(saves==s+1);
      store_fail=0;now+=1;sd_own_poll();assert(saves==s+2&&!sd_own.dirty); }
    midi_count=0;

    /* ---- old and foreign records read as nothing */
    { uint8_t keep[80];int keep_len=stored_len;memcpy(keep,stored,80);
      memset(stored,0xFF,sizeof stored);stored_len=BLE_DEV_STORE_SIZE;boot();unsigned n=scans_on;at(now+1);assert(!sd_own.relink&&scans_on==n);
      stored_len=33;memcpy(stored,"SBT1",4);boot();at(now+1);assert(!sd_own.relink&&scans_on==n);   /* JieLi's record */
      memcpy(stored,keep,80);stored_len=keep_len; }

    /* ---- the computer's link: dropped only when it is the computer's */
    boot();own_role=0;{ unsigned d=disconnects,e=enables;ble_midi_drop_host();assert(disconnects==d);
      own_role=2;ble_midi_drop_host();assert(disconnects==d);
      own_role=1;ble_midi_drop_host();assert(disconnects==d+1&&!held);
      ble_midi_reconnect();assert(disconnects==d+2&&enables==e);
      own_role=2;ble_midi_reconnect();assert(disconnects==d+2&&enables==e);
      own_role=0;ble_midi_reconnect();assert(enables==e+2&&enabled==1&&!held); }

    /* ---- forgetting waits for quiet, clears both sides, and reports each store */
    linked=1;{ unsigned s=saves_ble;assert(!ble_midi_can_forget()&&!ble_midi_forget_all()&&saves_ble==s);linked=0;
      st=BLE_CS_CONNECTING;assert(!ble_midi_can_forget()&&!ble_midi_forget_all()&&saves_ble==s);st=BLE_CS_IDLE;
      own.have_bond=1;own.bond_ltk[3]=9;bhs_key.valid=1;bhs_key.ltk[0]=7;own_dirty=1;unsigned r=smp_resets;
      assert(ble_midi_forget_all()&&saves_ble==s+1&&!own.have_bond&&!own.bond_ltk[3]&&!bhs_key.valid&&!bhs_key.ltk[0]&&!own_dirty&&smp_resets==r+1);
      store_fail=1;assert(!ble_midi_forget_all());store_fail=0; }
    sd_own_load();
    store_fail=1;assert(!saadet_ble_forget_device()&&!sd_own.relink);store_fail=0;
    assert(saadet_ble_forget_device()&&!ble_store_has_last(&sd_own.store));
    boot();{ unsigned n=scans_on;at(now+1);at(now+SD_RELINK_MS);assert(!sd_own.relink&&scans_on==n); }

    /* the pairing code is the stack's, -1 when there is none */
    assert(ble_midi_pairing_code()==-1);passkey=42;assert(ble_midi_pairing_code()==42);
    assert(!held);
    puts("own stack: last device looked for and found, no pairing loop, bounded attempts, drop, retry and forget passed");
}
'''
with tempfile.TemporaryDirectory() as d:
    path = Path(d)
    (path / 'test.c').write_text(code)
    subprocess.run([os.getenv('CC', 'cc'), '-O1', '-g', '-w', '-fsanitize=address,undefined',
                    '-DBLE_CENTRAL=1', '-DBLE_LL_ENC=1', '-DBLE_SMP_LEGACY=1',
                    '-I' + str(ROOT / 'platform'), '-I' + str(BLE),
                    str(path / 'test.c'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
