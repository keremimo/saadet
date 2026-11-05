/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Actual single-part editor, the panel, native synths, power-loss-safe state and archive paths. */
#include <assert.h>
#define MELODEE_FLASH 1
#define MELODEE_USB_AUDIO 0
#include "synth_harness.c"
#include "../firmware/src/usb_audio_stream.c"
#define SXQ 1024u
static uint32_t so_w,so_r,wire_n;
static uint8_t wire[2048];
static void ota_wire_send(const uint8_t *p,uint32_t n){assert(n<=sizeof wire);memcpy(wire,p,n);wire_n=n;}
#define ED_BK_FLASH_PTR(off) (nor+(off))
static int ota_frame_get(const uint8_t **p,uint32_t *n){(void)p;(void)n;return 0;}
static void ota_frame_done(void){}
#include "../firmware/src/editor.c"
static void request(uint32_t cmd,const uint8_t *a,uint32_t n){uint8_t f[1024]={0x7D,0x46,0x4C};f[3]=cmd;if(n)memcpy(f+4,a,n);wire_n=0;ed_handle(f,n+4);}
static void tick(void){int32_t out[CTL*2];mix_block(out,CTL);}
static void packet(uint32_t st,uint32_t note,uint32_t vel){midi_event(st&0xF0u,st&15u,note,vel);}
static uint32_t held_count(void){uint32_t n=0;for(uint32_t i=0;i<4u;i++)n+=__builtin_popcount(live_held[0][i]);return n;}
static void save_file(const char *name,const void *p,uint32_t n){FILE *f=fopen(name,"wb");assert(f);assert(fwrite(p,1,n,f)==n);fclose(f);}
static void playing(void){
    power_on();assert(NTRK==2 && NENG_SHOWN==5 && !eng_ok(3) && !eng_ok(ENGI_DRUM) && !eng_ok(USER_NATIVE_P5) && !eng_ok(USER_NATIVE_J106) && eng_ok(ENGI_J106) && eng_ok(ENGI_HYDRA));
    for(uint32_t i=0;i<NENG_SHOWN;i++){
        host_preset(TSEL,ENGINE_ORDER[i],0);trk_note_on(TSEL,60,100);uint64_t energy=0;
        for(uint32_t b=0;b<160;b++){int32_t out[CTL*2];mix_block(out,CTL);for(uint32_t j=0;j<CTL*2;j++)energy+=labs(out[j]);}
        assert(energy);trk_all_off(TSEL);
    }
    power_on();for(uint32_t ch=0;ch<16;ch++){packet(0x90|ch,60,100);assert(held_count()==1);packet(0x80|ch,60,0);assert(!held_count());}
    song.g[G_ROUTE]=6;packet(0x90,64,100);assert(!held_count());packet(0x95,64,100);assert(held_count()==1);
    song.g[G_ROUTE]=2;packet(0x85,64,0);assert(!held_count());song.g[G_ROUTE]=0;
    packet(0x90,64,100);packet(0xB0,64,127);packet(0x80,64,0);assert(held_count());packet(0xB0,64,0);assert(!held_count());
    TSEL->p[P_CHRD]=3;TSEL->p[P_STRUM]=3;packet(0x90,60,100);assert(held_count()==1 && midi_strum_pending);
    for(uint32_t i=0;i<200;i++)tick();assert(held_count()==3 && !midi_strum_pending);packet(0x80,60,0);assert(!held_count());
    packet(0x90,60,100);packet(0x80,60,0);for(uint32_t i=0;i<200;i++)tick();assert(!held_count() && !midi_strum_pending);
    power_on();                                       /* every button and knob (the interface: tests/ui_test.c) */
    for(uint32_t b=0;b<NB;b++)press(b);
    for(uint32_t r=0;r<NE;r++)turn(r,3);
    frames(2000);                                     /* (PRESETS / ALGORITHM: the sound loads at rest) */
    assert(song.octave==0 && TSEL->p[P_AHOLD]);       /* (OCT- then OCT+; REC latched) */
    TSEL->p[P_AHOLD]=0;
    for(uint32_t i=0;i<400;i++)tick();               /* (an engine change completes in the audio blocks) */
    key_down(0);frame();assert(gates());key_up(0);frame();assert(!gates());   /* the keys still play */
    puts("SYNTH: five audible engines, MIDI receive-channel ownership, sustain, strum, latch and the panel passed");
}
static void persistence(void){
    power_on();memset(nor,255,sizeof nor);persist_boot();settings_init();
    for(uint32_t i=0;i<NENG_SHOWN;i++){
        power_on();host_preset(TSEL,ENGINE_ORDER[i],1);tick();TSEL->user=384;TSEL->mpc_octave=-2;
        TSEL->p[P_SPRD]=51;song.g[G_SWING]=20;
        song.g[G_BPM]=137;song.g[G_ROUTE]=11;song.g[G_DRUMCH]=7;song.octave=2;
        synth_state_t expected,actual;state_capture(&expected);assert(state_autosave());uint32_t before=erases;assert(state_autosave() && before==erases);
        power_on();syn_saved=0;state_boot();state_capture(&actual);assert(!memcmp(&expected,&actual,sizeof actual));
    }
    {   /* REC's latch starts off, even from a state an earlier build saved with it on, and is never worth a flash write */
        synth_state_t old;power_on();state_capture(&old);old.part[0].p[P_AHOLD]=1;old.sum=syn_hash(&old,sizeof old-4u);
        assert(st_save(OBJ_SYNTH,&old,sizeof old)==0);
        power_on();syn_saved=0;state_boot();assert(!TSEL->p[P_AHOLD]);
        uint32_t before=erases;TSEL->p[P_AHOLD]=1;assert(state_autosave() && before==erases);TSEL->p[P_AHOLD]=0;
    }
    TSEL->p[P_SPRD]=63;erase_error=1;assert(!state_autosave() && syn_retry);erase_error=0;assert(state_autosave() && !syn_retry);
    uint32_t saved=syn_saved;TSEL->p[P_SPRD]=71;fail_after=1;assert(!state_autosave());fail_after=-1;
    power_on();state_boot();assert(TSEL->p[P_SPRD]==63 && syn_saved==saved);
    synth_state_t bad;state_capture(&bad);bad.part[0].engine=ENGI_DRUM;bad.sum=syn_hash(&bad,sizeof bad-4u);assert(!state_valid(&bad));
    bootguard.failed=1;power_on();state_boot();assert(TSEL->p[P_SPRD]!=63);bootguard.failed=0;
    for(uint32_t b=0;b<UP_BANKS;b++)for(uint32_t c=0;c<2;c++)assert(st_sector(up_obj(b),c)!=st_sector(OBJ_SYNTH,0) && st_sector(up_obj(b),c)!=st_sector(OBJ_SYNTH,1));
    puts("STATE: five engines, slot 384, track/MIDI settings, unchanged saves, failed writes and boot-loop recovery passed");
}
static void renderer_migration(void){
    for(uint32_t mode=1;mode<=2;mode++){
        power_on();memset(nor,255,sizeof nor);persist_boot();settings_init();
        host_preset(TSEL,ENGI_FM6,2);tick();
        fm6_fn[0][FN_ENGINE]=(uint8_t)mode;fm6_fn[0][FN_PBUP]=7;
        uint8_t patch[FP_SIZE+1];memcpy(patch,fm6_patch[0],sizeof patch);
        assert(state_autosave());
        power_on();syn_saved=0;state_boot();
        assert(TSEL->eng_req==ENGI_FM6 && fm6_fn[0][FN_ENGINE]==FM6_MODERN);
        assert(fm6_fn[0][FN_PBUP]==7 && !memcmp(patch,fm6_patch[0],sizeof patch));
    }
    synth_state_t retired;state_capture(&retired);retired.part[0].engine=3;
    retired.sum=syn_hash(&retired,sizeof retired-4u);assert(!state_valid(&retired));
    assert(ENGINES[3]==&ENG_RETIRED && eng_extra_retired(3));
    fm6_fn_set(0,FN_ENGINE,2);assert(fm6_fn[0][FN_ENGINE]==FM6_MODERN);
    puts("MIGRATION: legacy FM6 modes retain native patches/controllers; SID identity is retired");
}
static void editor(void){
    power_on();memset(nor,255,sizeof nor);persist_boot();settings_init();request(ED_INFO,0,0);assert(wire_n>12);
    save_file("build/host/synth-info.bin",wire+5,wire_n-6);
    uint32_t removed[]={ED_STEP_GET,ED_STEP_SET,ED_PROJECT,ED_TRACK_STEP,ED_SONG,ED_MOTION,ED_PATTERN,ED_BANK_SONG};uint8_t args[16]={0};
    for(uint32_t i=0;i<sizeof removed/sizeof *removed;i++){request(removed[i],args,sizeof args);assert(!wire_n);}
    uint8_t library[]={3,127,2,0,'L','A','S','T'};request(ED_LIBRARY,library,sizeof library);assert(wire_n && wire[8]==0 && up_used(383));assert(!up_used(127));
    up_ui_named(3,383,"LAST384");char label[20];up_name(383,label);assert(str_eq(label,"LAST384"));
    TSEL->p[P_SPRD]=51;assert(state_autosave());assert(!ed_bk_capture());assert(ED_BK_N==30 && ed_bk_supported(32) && ed_bk_supported(28));assert(!ed_bk_supported(0) && !ed_bk_supported(2));
    uint32_t len;const uint8_t *bytes=ed_bk_object(32,&len);assert(len==sizeof(synth_state_t) && state_valid((const synth_state_t *)bytes));
    save_file("build/host/synth-state.bin",bytes,len);save_file("build/host/synth-settings.bin",&set_rec,ed_bk_set_len);
    for(uint32_t i=0;i<ED_BK_N;i++){uint32_t n,id=ED_BK_IDS[i];const uint8_t *p=ed_bk_object(id,&n);char path[80];snprintf(path,sizeof path,"build/host/archive-%u.bin",id);save_file(path,p,n);}
    synth_state_t state;state_capture(&state);memcpy(ED_BK_RAW,&state,sizeof state);ed_bk_id=32;ed_bk_len=ed_bk_pos=sizeof state;ed_bk_crc=st_crc32(ED_BK_RAW,ed_bk_len);
    TSEL->p[P_SPRD]=7;assert(!ed_bk_commit_inner() && TSEL->p[P_SPRD]==51);ED_BK_RAW[0]^=1;assert(ed_bk_commit_inner()==2);
    uint8_t put[12]={0,2};request(ED_BACKUP_PUT,put,sizeof put);assert(wire[7]==1);
    puts("EDITOR: removed commands, shared slot 384, distinct synth/preset archives and corrupt-restore rejection passed");
}
static void battery_calibration(void)
{
    power_on(); memset(nor, 255, sizeof nor); persist_boot(); settings_init();
    song.batt_raw = 615;
    request(ED_BATTERY, 0, 0);
    assert(wire_n == 12 && wire[5] == 1 && wire[6] == 0 && (wire[7] | wire[8] << 7) == 615);
    uint8_t a[3] = {1, 612 & 127, 612 >> 7};
    request(ED_BATTERY, a, sizeof a);
    assert(wire[6] == 0 && settings_batt_full == 612);
    persist_t saved;
    assert(st_load(OBJ_SETTINGS, &saved, sizeof saved) == sizeof saved && battery_full_of(&saved) == 612);
    settings_batt_full = 0;
    assert(settings_import(&saved, sizeof saved) && settings_batt_full == 612);
    uint32_t count = erases;
    request(ED_BATTERY, a, sizeof a); assert(wire[6] == 0 && erases == count);
    a[1] = a[2] = 0; request(ED_BATTERY, a, sizeof a);
    assert(wire[6] == 1 && settings_batt_full == 612 && erases == count);
    a[1] = 127; a[2] = 8; request(ED_BATTERY, a, sizeof a);
    assert(wire[6] == 1 && settings_batt_full == 612 && erases == count);
    a[1] = 101; a[2] = 4;
    song.playing = 1; request(ED_BATTERY, a, sizeof a); song.playing = 0;
    assert(wire[6] == 2 && settings_batt_full == 612 && erases == count);
    erase_error = 1; request(ED_BATTERY, a, sizeof a); erase_error = 0;
    assert(wire[6] == 3 && battery_full_of(&persist_saved) == 612);
    assert(st_load(OBJ_SETTINGS, &saved, sizeof saved) == sizeof saved && battery_full_of(&saved) == 612);
    song.batt_raw = 0; request(ED_BATTERY, 0, 0); assert(wire[6] == 2);
    puts("BATTERY: raw read, full-reference persistence, idempotence, invalid data, transport and flash failures passed");
}
int main(void){
    playing();persistence();renderer_migration();editor();battery_calibration();memset(&ua,0,sizeof ua);ua.cap_alt=2;
    int32_t audio[64];for(uint32_t i=0;i<32;i++){audio[2*i]=1234;audio[2*i+1]=-4321;}ua_audio(audio,32,4096);assert(ua.cw==32 && ua.cap[0]==1234 && ua.cap[1]==-4321);
    puts("SAADET INTEGRATION TESTS PASSED");return 0;
}
