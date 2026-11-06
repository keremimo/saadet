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
static int wire_fail;                               /* the host stopped taking SysEx: the next sends fail */
static int ota_wire_send(const uint8_t *p,uint32_t n){if(wire_fail)return -1;assert(n<=sizeof wire);memcpy(wire,p,n);wire_n=n;return 0;}
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
    hy_bank_boot();hy_uset(0,HY_WAVES[HYW_SAW],"SAWZ",1);   /* (a user wave not yet in flash: the backup keeps it first) */
    TSEL->p[P_SPRD]=51;assert(state_autosave());hy_uset(1,HY_WAVES[HYW_BELL],"BELLZ",1);assert(!ed_bk_capture());assert(!hy_udirty);assert(ED_BK_N==32 && ed_bk_supported(32) && ed_bk_supported(28) && ed_bk_supported(39) && ed_bk_supported(40));assert(!ed_bk_supported(0) && !ed_bk_supported(2));
    uint32_t len;const uint8_t *bytes=ed_bk_object(32,&len);assert(len==sizeof(synth_state_t) && state_valid((const synth_state_t *)bytes));
    save_file("build/host/synth-state.bin",bytes,len);save_file("build/host/synth-settings.bin",&set_rec,ed_bk_set_len);
    for(uint32_t i=0;i<ED_BK_N;i++){uint32_t n,id=ED_BK_IDS[i];const uint8_t *p=ed_bk_object(id,&n);char path[80];snprintf(path,sizeof path,"build/host/archive-%u.bin",id);save_file(path,p,n);}
    synth_state_t state;state_capture(&state);memcpy(ED_BK_RAW,&state,sizeof state);ed_bk_id=32;ed_bk_len=ed_bk_pos=sizeof state;ed_bk_crc=st_crc32(ED_BK_RAW,ed_bk_len);
    TSEL->p[P_SPRD]=7;assert(!ed_bk_commit_inner() && TSEL->p[P_SPRD]==51);ED_BK_RAW[0]^=1;assert(ed_bk_commit_inner()==2);
    uint8_t put[12]={0,2};request(ED_BACKUP_PUT,put,sizeof put);assert(wire[7]==1);
    {   /* HYDRA's user waves restore whole, a bank that is not one fails, an empty object frees them */
        const uint8_t *w=ed_bk_object(39,&len);assert(len==sizeof(hy_uobj_t));memcpy(ED_BK_RAW,w,len);
        hy_uclear(1);ed_bk_id=39;ed_bk_len=ed_bk_pos=len;ed_bk_crc=st_crc32(ED_BK_RAW,len);
        assert(!ed_bk_commit_inner() && hy_uwave(1)->used && str_eq(hy_uwave(1)->name,"BELLZ") && !hy_udirty);
        ED_BK_RAW[0]^=1;ed_bk_crc=st_crc32(ED_BK_RAW,len);assert(ed_bk_commit_inner()==2 && hy_uwave(1)->used);ED_BK_RAW[0]^=1;
        ed_bk_id=40;ed_bk_len=ed_bk_pos=0;ed_bk_crc=st_crc32(ED_BK_RAW,0);assert(!ed_bk_commit_inner() && hy_uwave(0)->used);
    }
    puts("EDITOR: removed commands, shared slot 384, distinct synth/preset archives and corrupt-restore rejection passed");
}
/* one pass of the live sync as main.c runs it, the editor heard from just now: wire holds its last push, 0 none */
static void sync_pass(void){fm1_ms+=40u;ed_w.last_ms=fm1_ms;ed_w.run_ms=fm1_ms-10u;wire_n=0;ed_sync();}
static int push_is(uint32_t kind,uint32_t part){return wire_n>=7u && wire[4]==ED_PUSH && wire[5]==kind && (kind==2u || wire[6]==part);}
static void editor_parts(void){
    power_on();memset(nor,255,sizeof nor);persist_boot();settings_init();perf_boot();
    request(ED_INFO,0,0);int found=0;   /* INFO's editor features: PUSH, performances, voices, CZ live, FM6 functions, BUDGET */
    for(uint32_t i=5;i+7<wire_n;i++)if(wire[i]==0x45 && wire[i+1]==1 && wire[i+2]==63 && wire[i+3]==ED_PERF && wire[i+4]==ED_PUSH && wire[i+5]==ED_VOICES && wire[i+6]==ED_FM6_FN && wire[i+7]==PERF_SLOTS)found=1;
    assert(found);
    /* performances: list, store, rename, load, delete, and names that are refused */
    uint8_t list[]={0,0};request(ED_PERF,list,2);assert(wire[5]==0 && wire[7]==0 && wire[8]==PERF_SLOTS && wire[9]==0 && wire[10]==0);
    uint8_t store[]={2,3,'L','I','V','E',' ','S','E','T'};request(ED_PERF,store,sizeof store);assert(wire[6]==3 && wire[7]==0 && perf_used(3));
    request(ED_PERF,list,2);assert(wire[9]==4);{uint32_t at=10;for(uint32_t k=0;k<3u;k++){at+=2;while(wire[at])at++;at++;}assert(wire[at]==1 && !memcmp(wire+at+2,"LIVE SET",9));}
    uint8_t ren[]={3,3,'B','A','N','D'};request(ED_PERF,ren,sizeof ren);assert(!wire[7]);char nm[PERF_NAME];perf_name(3,nm);assert(str_eq(nm,"BAND"));
    uint8_t bad[][4]={{2,4,1,0},{3,9,'X',0},{5,0,0,0}};request(ED_PERF,bad[0],3);assert(wire[7]==1);request(ED_PERF,bad[1],3);assert(wire[7]==1);request(ED_PERF,bad[2],2);assert(wire[7]==1);
    uint8_t long_name[2+PERF_NAME]={2,5};memset(long_name+2,'A',PERF_NAME);request(ED_PERF,long_name,sizeof long_name);assert(wire[7]==1 && !perf_used(5));
    sync_reload=0;uint8_t load[]={1,3};request(ED_PERF,load,2);assert(!wire[7] && sync_reload && perf_now==4);
    uint8_t del[]={4,3};request(ED_PERF,del,2);assert(!wire[7] && !perf_used(3));request(ED_PERF,load,2);assert(wire[7]==1);
    /* the voice budget: VQ, budget, ceiling, parts sounding, then per part cost, playing, fading, waiting, claimed, share */
    power_on();host_preset(TSEL,ENGI_J106,1);trk_note_on(TSEL,60,100);trk_note_on(TSEL,64,100);tick();
    request(ED_VOICES,0,0);assert(wire_n==5u+1u+2u+2u+1u+1u+NPART*9u+1u && wire[5]==VQ && wire[10]==1 && wire[11]==NPART);
    {uint32_t budget=wire[6]|wire[7]<<7,cost=wire[12]|wire[13]<<7,claimed=wire[17]|wire[18]<<7;
     assert(budget==voice_budget() && cost==voice_units_now(TSEL) && wire[14]==2 && claimed==2u*cost && (wire[19]|wire[20]<<7)==budget);}
    trk_all_off(TSEL);
    /* the voice budget is the instrument's setting: SET reaches it, DUMP and GET read it */
    uint8_t budget[]={1,G_BUDGET,20&127,(20+8192)>>7};budget[2]=(20+8192)&127;request(ED_SET,budget,4);assert(settings_budget==20);
    uint8_t get_budget[]={1,G_BUDGET};request(ED_GET,get_budget,2);assert(ed_rv(wire+7)==20);settings_budget=VOICE_BUDGET_DEFAULT;
    /* FM6's function settings and operator switches */
    host_preset(TSEL,ENGI_FM6,0);uint8_t fget[]={0,0};request(ED_FM6_FN,fget,2);assert(!wire[7] && wire_n==8u+FM6_NFN+2u && wire[8+FN_PBUP]==fm6_fn[0][FN_PBUP] && wire[8+FM6_NFN]==fm6_on[0]);
    uint8_t fset[]={1,0,FN_PBUP,7};request(ED_FM6_FN,fset,4);assert(!wire[7] && fm6_fn[0][FN_PBUP]==7);
    uint8_t fon[]={1,0,FM6_NFN,FM6_ON_ALL&~1u};request(ED_FM6_FN,fon,4);assert(!wire[7] && fm6_on[0]==(FM6_ON_ALL&~1u));
    uint8_t fbad[]={1,0,FN_PBUP,13};request(ED_FM6_FN,fbad,4);assert(wire[7]==1 && fm6_fn[0][FN_PBUP]==7);
    /* a CZ-1 tone into a part that plays one: no note cut, E1-E8 kept, no reload; refused for another engine */
    host_preset(TSEL,ENGI_CZ,1);assert(TSEL->p[P_E7]==CZ_NATIVE);TSEL->p[P_E2]=5;trk_note_on(TSEL,60,100);tick();sync_reload=0;
    uint8_t cz[2+2*CZ_BYTES]={3,0},raw[CZ_BYTES];memcpy(raw,cz_patch[0].raw,CZ_BYTES);raw[CZ_BYTES-1]=raw[CZ_BYTES-1]=='Z'?'Y':'Z';
    for(uint32_t i=0;i<CZ_BYTES;i++){cz[2+2*i]=raw[i]&15u;cz[3+2*i]=raw[i]>>4;}
    request(ED_CZ_PUT,cz,sizeof cz);assert(!wire[7] && !memcmp(cz_patch[0].raw,raw,CZ_BYTES) && TSEL->p[P_E2]==5 && !sync_reload && !(panic_req&1u) && TSEL->v[0].active);
    trk_all_off(TSEL);host_preset(TSEL,ENGI_PROPHET,0);request(ED_CZ_PUT,cz,sizeof cz);assert(wire[7]==1);
    /* PUSH: a panel edit of a native patch, the editor's own (no echo), a read (seen), another part's sound, the user waves,
     * and a push that could not be sent (still due) */
    power_on();usb.config=1;song.g[G_PMODE]=PT_MULTI;host_preset(TSEL,ENGI_PROPHET,0);
    uint8_t watch[]={7};request(ED_WATCH,watch,1);assert(wire[5]==7);sync_pass();assert(!wire_n);
    fm1_irq_off();p5_edit_value(TSEL,P5_CUTOFF,p5_patch_of(TSEL)->raw[P5_CUTOFF]==40?41:40);fm1_irq_on();sync_pass();assert(push_is(0,0) && wire[7]==ENGI_PROPHET);
    sync_pass();assert(!wire_n);
    uint8_t edit[]={2,0,P5_CUTOFF,0};request(95,edit,4);assert(!wire[6]);sync_pass();assert(!wire_n);
    fm1_irq_off();p5_edit_value(TSEL,P5_CUTOFF,10);fm1_irq_on();uint8_t p5get[]={0,0};request(95,p5get,2);sync_pass();assert(!wire_n);
    fm1_irq_off();p5_edit_value(TSEL,P5_CUTOFF,20);fm1_irq_on();wire_fail=1;sync_pass();wire_fail=0;assert(!wire_n);
    sync_pass();assert(push_is(0,0));
    host_preset(&trk[1],ENGI_J106,3);sync_pass();assert(push_is(1,1) && wire[7]==ENGI_J106 && wire[8]==3);sync_pass();assert(!wire_n);
    /* a RELOAD (here a preset on the selected part) with a native change of the same moment: the change still pushes */
    apply_preset(2);fm1_irq_off();p5_edit_value(TSEL,P5_CUTOFF,p5_patch_of(TSEL)->raw[P5_CUTOFF]==50?51:50);fm1_irq_on();
    sync_pass();assert(wire_n && wire[4]==ED_RELOAD);sync_pass();assert(push_is(0,0));sync_pass();assert(!wire_n);
    hy_uset(2,HY_WAVES[HYW_SAW],"RAMP",1);sync_pass();assert(push_is(2,0));sync_pass();assert(!wire_n);
    request(ED_PUSH,0,0);assert(wire[5]==NTRK && wire[6]==ENGI_PROPHET && wire_n==5u+1u+NTRK*6u+5u+1u);
    uint8_t old_watch[]={3};request(ED_WATCH,old_watch,1);assert(wire[5]==3);   /* (an editor without PUSH gets none) */
    fm1_irq_off();p5_edit_value(TSEL,P5_CUTOFF,30);fm1_irq_on();sync_pass();assert(!wire_n);
    usb.config=0;ed_w.on=0;song.g[G_PMODE]=PT_ONE;
    puts("EDITOR PARTS: features, performances, voice budget, BUDGET, FM6 functions, CZ without a cut, PUSH passed");
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
    playing();persistence();renderer_migration();editor();editor_parts();battery_calibration();memset(&ua,0,sizeof ua);ua.cap_alt=2;
    int32_t audio[64];for(uint32_t i=0;i<32;i++){audio[2*i]=1234;audio[2*i+1]=-4321;}ua_audio(audio,32,4096);assert(ua.cw==32 && ua.cap[0]==1234 && ua.cap[1]==-4321);
    puts("SAADET INTEGRATION TESTS PASSED");return 0;
}
