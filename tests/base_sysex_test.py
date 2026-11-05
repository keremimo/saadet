#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Run the shipping base USB receiver through native parsing, import UI and storage."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
usb = (ROOT / 'build/fm1-base-platform/bsp/chip/usb.c').read_text()
instrument = (ROOT / 'platform/instrument.c').read_text()


def routine(source, signature):
    start = source.index(signature)
    start = source.rfind('\n', 0, start) + 1
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


# Keep the legacy transport only for harness/editor dependencies. All incoming
# packets below execute the actual overlaid base USB routines, with their own
# collector. Native parsers, dispatcher, panel and flash stores are production code.
prefix = r'''
#include <assert.h>
#define MELODEE_FLASH 1
#define MELODEE_OTA 1
#define sysex_byte legacy_sysex_byte
#define midi_in_event legacy_midi_in_event
#include "tests/ui_harness.c"
#undef sysex_byte
#undef midi_in_event
static uint8_t sent[8192];
static uint32_t sent_n;
static void drain(void){
    while(so_r!=so_w){
        uint32_t p=sx_out_q[so_r++%SXQ],cin=p&15u,nb=cin==4u||cin==7u?3u:cin==6u?2u:1u;
        assert(sent_n+nb<=sizeof sent);
        for(uint32_t k=0;k<nb;k++)sent[sent_n++]=(uint8_t)(p>>(8u*(k+1u)));
    }
}
static uint32_t ota_now_ms(void){return fm1_ms;}
static void ota_idle(void){drain();fm1_ms++;}
#define ED_BK_FLASH_PTR(off) (nor + (off))
#include "firmware/src/editor.c"
static uint32_t sd_ms(void){return fm1_ms;}
static struct {uint8_t sysex[16],sx_len,sx_on,uboot_req,ota_req;} base_usb;
static uint8_t base_sx_frame[640],base_sx_ready,base_sx_collect;
static uint32_t base_sx_pos,base_sx_frame_len,base_events;
static void base_midi_enqueue(uint32_t p,uint32_t port){(void)p;(void)port;base_events++;}
#define FELUCCA_OTA 1
'''
rx = '\n'.join(routine(usb, sig) for sig in (
    'static void sysex_byte(', 'static void midi_in_event('))
rx = re.sub(r'\b(usb|sx_frame|sx_pos|sx_frame_len|sx_ready|sx_collect|midi_enqueue)\b',
            lambda m: 'base_' + m[0], rx)
bridge = '\n'.join(routine(instrument, sig) for sig in (
    'void saadet_sysex_byte(', 'int saadet_sysex('))
main = r'''
/* Model a main-loop turn between USB endpoint batches, as on the target. */
static void service(void){
    if(base_sx_ready){saadet_sysex(base_sx_frame,base_sx_frame_len);base_sx_ready=0;}
    cz_service();fm6_service();frame();
}
/* 0: normal CIN 4..7; 1: every byte CIN F; 2: mixed packets with clocks.
 * Both endpoints of a frame and interior data can arrive as single bytes. */
static void feed(const uint8_t *p,uint32_t n,uint32_t mode){
    uint32_t at=0,packets=0;
    while(at<n){
        uint32_t take=n-at,cin;
        if(mode==1u || (mode==2u && (at==0u || take==1u || at%17u==0u))){take=1;cin=15;}
        else {if(take>3u)take=3;cin=at+take<n?4u:4u+take;}
        uint32_t packet=cin;
        for(uint32_t k=0;k<take;k++)packet|=(uint32_t)p[at+k]<<(8u*(k+1u));
        midi_in_event(packet);usb.rx_pkts++;at+=take;
        if(mode==2u && !(packets%11u))midi_in_event(15u|0xF8u<<8);
        if(++packets%16u==0u)service();
    }
    service();
}
static uint32_t playing_raw(uint32_t e,uint8_t *raw){
    assert(TSEL->eng_req==e);
    if(e==ENGI_FM6)fm6_pack(fm6_patch[0],raw);
    else if(e==ENGI_CZ)memcpy(raw,cz_patch[0].raw,CZ_BYTES);
    else if(e==ENGI_PROPHET)memcpy(raw,p5_patch_of(TSEL),sizeof(p5_patch_t));
    else if(e==ENGI_J106)memcpy(raw,j106_patch_of(TSEL),sizeof(j106_patch_t));
    else if(e==ENGI_HYDRA)memcpy(raw,hy_patch_of(TSEL),sizeof(hy_patch_t));
    else assert(0);
    return native_size(e);
}
static void imported(uint32_t engine,const uint8_t *raw,uint32_t count,uint32_t size){
    assert(sx_staged() && sx_stage.e==engine && sx_stage.count==count && sx_stage.size==size);
    assert(!memcmp(sx_stage.raw,raw,count*size));
    if(count==1u){uint8_t now[CZ_BYTES];assert(playing_raw(engine,now)==size&&!memcmp(now,raw,size));}
    assert(uix.place==PL_SAVE && lint_has("PRESS SAVE TO IMPORT"));
    uint32_t slot=lib.first,before=lib_count(LIB_USER);
    assert(!up_used(slot));
    press(B_SAVE);
    assert(!sx_staged() && lib_count(LIB_USER)==before+count);
    for(uint32_t k=0;k<count;k++){
        assert(native_used(engine,slot+k));
        assert(!memcmp(native_raw(engine,slot+k),raw+k*size,size));
    }
    up_cache_reset();native_cache_reset(); /* read back from the NOR model */
    assert(native_used(engine,slot));
    assert(!memcmp(native_raw(engine,slot),raw,size));
    press(B_HOME);
}
static void export_selected(uint32_t e,const uint8_t *want,uint32_t size){
    uint32_t payload=e==ENGI_J106?J106_NAME:size; /* Roland's wire format has no name */
    uint8_t before[CZ_BYTES];uint32_t was=TSEL->eng_req;
    playing_raw(was,before);uint32_t writes=erases;
    so_r=so_w;sent_n=0;
    turn(EN_K3,-1);assert(so_r==so_w);
    turn(EN_K3,1);drain();
    assert(sent_n&&lint_has("SOUND SENT")&&!lint_check("library send"));
    ui_png("library_send");
    assert(erases==writes&&TSEL->eng_req==was);
    uint8_t after[CZ_BYTES];playing_raw(was,after);assert(!memcmp(before,after,native_size(was)));
    if(e==ENGI_FM6)assert(sent[2]==5u);
    else if(e==ENGI_CZ)assert(sent[4]==0x75u);
    else if(e==ENGI_J106)assert(sent[3]==5u);
    feed(sent,sent_n,2);
    if(!sx_staged()||sx_stage.e!=e||sx_stage.size!=size||memcmp(sx_stage.raw,want,payload)){
        fprintf(stderr,"export mismatch engine %u size %u got engine %u size %u, staged %u\n",e,size,sx_stage.e,sx_stage.size,sx_staged());
        for(uint32_t k=0;k<payload;k++)if(sx_stage.raw[k]!=want[k])fprintf(stderr," byte %u: got %u want %u\n",k,sx_stage.raw[k],want[k]);
    }
    assert(sx_staged()&&sx_stage.e==e&&sx_stage.size==size&&!memcmp(sx_stage.raw,want,payload));
    int32_t audio[CTL*2];mix_block(audio,CTL);playing_raw(e,after);assert(!memcmp(after,want,payload));
    assert(erases==writes&&so_r==so_w); /* audition and receive never echo or save */
    press(B_ENV);assert(!sx_staged());
}
int main(void){
    memset(nor,255,sizeof nor);power_on();persist_boot();frames(800);usb.config=1;
    uint8_t framebuf[FM6_RX],packed[FM6_PACKED],voice[FP_SIZE+1u];
    for(uint32_t mode=0;mode<3u;mode++){
        fm6_factory(9u+mode,packed);fm6_unpack(packed,voice);
        memcpy(framebuf,(uint8_t[]){0xF0,0x43,0x0F,0,1,0x1B},6);
        memcpy(framebuf+6,voice,155);framebuf[161]=fm6_chk(framebuf+6,155);framebuf[162]=0xF7;
        /* Packing a VCED normalizes unused bits; compare the exact accepted record. */
        fm6_pack(voice,packed);
        uint32_t writes=erases;
        feed(framebuf,163,mode);assert(erases==writes);
        imported(ENGI_FM6,packed,1,sizeof packed);
        printf("base SysEx: DX7 voice packet mode %u -> SAVE -> flash passed\n",mode);
        static uint8_t bank[4096];
        for(uint32_t k=0;k<32u;k++)fm6_factory(k,bank+k*FM6_PACKED);
        memcpy(framebuf,(uint8_t[]){0xF0,0x43,0,9,0x20,0},6);
        memcpy(framebuf+6,bank,sizeof bank);framebuf[4102]=fm6_chk(bank,sizeof bank);framebuf[4103]=0xF7;
        uint8_t playing[CZ_BYTES];uint32_t was=TSEL->eng_req;playing_raw(was,playing);
        writes=erases;feed(framebuf,sizeof framebuf,mode);assert(erases==writes);
        uint8_t still[CZ_BYTES];playing_raw(was,still);assert(!memcmp(playing,still,native_size(was)));
        imported(ENGI_FM6,bank,32,FM6_PACKED);
        printf("base SysEx: DX7 32-voice bank packet mode %u -> SAVE -> flash passed\n",mode);
    }
    /* Other native formats share the repaired USB collector. */
    uint8_t cz[CZ_BYTES];cz_factory(3,cz);
    memcpy(framebuf,(uint8_t[]){0xF0,0x44,0,0,0x70,0x30},6);
    for(uint32_t k=0;k<CZ_BYTES;k++){framebuf[6+2*k]=cz[k]&15;framebuf[7+2*k]=cz[k]>>4;}
    framebuf[6+2*CZ_BYTES]=0xF7;feed(framebuf,7+2*CZ_BYTES,2);
    imported(ENGI_CZ,cz,1,CZ_BYTES);
    p5_patch_t p5;p5_factory(1,&p5);
    uint32_t n=p5_patch_encode(&p5,framebuf,sizeof framebuf);assert(n);
    feed(framebuf,n,2);imported(ENGI_PROPHET,(const uint8_t *)&p5,1,sizeof p5);
    j106_patch_t juno;j106_patch_init(&juno);
    n=j106_patch_encode(&juno,0,framebuf,sizeof framebuf);assert(n);
    j106_patch_t decoded;assert(j106_patch_decode(&decoded,framebuf,n));
    feed(framebuf,n,1);imported(ENGI_J106,(const uint8_t *)&decoded,1,sizeof decoded);
    hy_patch_t hydra;hy_factory(3u,&hydra);
    n=hy_patch_encode(&hydra,framebuf,sizeof framebuf);assert(n);
    feed(framebuf,n,2);imported(ENGI_HYDRA,(const uint8_t *)&hydra,1,sizeof hydra);
    puts("base SysEx: CZ-1, Prophet, JUNO-106 and HYDRA native imports passed");
    /* Native preset export round-trips through the generic framed dispatcher. */
    uint32_t slot=user_of(TSEL);up_rec_t record=*up_rec(slot);
    so_r=so_w;sent_n=0;assert(!general_sysex_send(slot));drain();
    feed(sent,sent_n,2);
    assert(sx_staged() && sx_stage.size==sizeof record && !memcmp(sx_stage.raw,&record,sizeof record));
    press(B_LFO);assert(!sx_staged());
    puts("base SysEx: generic preset round trip and import cancellation passed");
    /* Invalid Yamaha dumps must neither open SAVE nor replace the sound. */
    memcpy(framebuf,(uint8_t[]){0xF0,0x43,0,0,1,0x1B},6);
    memcpy(framebuf+6,voice,155);framebuf[161]=fm6_chk(framebuf+6,155)^1u;framebuf[162]=0xF7;
    feed(framebuf,163,2);assert(!sx_staged());
    framebuf[161]^=1u;
    feed(framebuf,81,2);midi_in_event(9u|0x90u<<8|60u<<16|100u<<24);
    feed(framebuf+81,82,2);assert(!sx_staged());
    feed(framebuf,163,2);assert(sx_staged());press(B_LFO);assert(!sx_staged());
    uint32_t events=base_events,pos=base_usb.sx_len;
    midi_in_event(15u|0x42u<<8);assert(base_events==events && base_usb.sx_len==pos && !base_usb.sx_on);
    assert(base_events>0); /* clocks and channel messages still reach MIDI */
    puts("base SysEx: checksum rejection, interruption, recovery and realtime passed");
    /* The visible K3 action sends INIT, both ends of every factory list, a
     * playing sound with edits, and a stored user sound without loading it. */
    song.g[G_DRUMCH]=6;
    for(uint32_t visible=0;visible<NENG_SHOWN;visible++){
        uint32_t e=eng_vis(visible),src=LIB_P5+visible;
        const uint32_t presets[]={0,1,ENGINES[e]->npresets-1u};
        for(uint32_t k=0;k<3u;k++){
            uint8_t want[CZ_BYTES];host_preset(TSEL,e,presets[k]);playing_raw(e,want);
            host_preset(TSEL,e,presets[k]?0:1);panic_req=0;
            ui_open(PL_LIB);uix.stop[PL_LIB]=(uint8_t)src;lib.at[src]=(uint16_t)presets[k];frame();
            assert(lint_has("SEND"));export_selected(e,want,native_size(e));
        }
        host_preset(TSEL,e,1);panic_req=0;
        if(e==ENGI_J106)j106_patch_of(TSEL)->raw[J106_VCF_FREQ]=29;
        else if(e==ENGI_HYDRA)hy_patch_of(TSEL)->raw[HY_F1_CUT]=29;
        else if(e==ENGI_PROPHET)p5_patch_of(TSEL)->raw[P5_CUTOFF]=29;
        else if(e==ENGI_FM6)fm6_patch[0][FP_NAME]='X';
        else cz_patch[0].raw[128]='X';
        uint8_t edited[CZ_BYTES];playing_raw(e,edited);
        assert(!up_store(200u+visible,"SEND USER"));
        ui_open(PL_LIB);uix.stop[PL_LIB]=(uint8_t)src;lib_open(src);frame();
        export_selected(e,edited,native_size(e));
        memcpy(edited,native_raw(e,200u+visible),native_size(e));
        host_preset(TSEL,eng_vis((visible+1u)%NENG_SHOWN),1);panic_req=0;
        ui_open(PL_LIB);uix.stop[PL_LIB]=LIB_USER;
        lib_item_t item;uint32_t at=0;
        while(at<lib_count(LIB_USER)&&(!lib_item(LIB_USER,at,&item)||item.idx!=200u+visible))at++;
        assert(at<lib_count(LIB_USER));lib.at[LIB_USER]=(uint16_t)at;frame();
        export_selected(e,edited,native_size(e));
    }
    ui_open(PL_LIB);uix.stop[PL_LIB]=LIB_FAV;frame();assert(!lint_has("SEND"));
    uix.stop[PL_LIB]=LIB_J106;lib.at[LIB_J106]=1;frame();
    usb.config=0;so_r=so_w;turn(EN_K3,1);
    assert(so_r==so_w&&lint_has("CONNECT USB TO SEND"));usb.config=1;
    puts("base SysEx: K3 exports every engine, native edits, user sounds, channels and offline/empty lists passed");
    for(uint32_t visible=0;visible<NENG_SHOWN;visible++){
        uint32_t e=eng_vis(visible);host_preset(TSEL,e,0);sx_drop();press(B_HOME);
        so_r=so_w;sent_n=0;assert(!library_sysex_send(e,0,0,1));drain();
        if(e==ENGI_J106)sent[5+J106_VCF_FREQ]=29;
        else if(e==ENGI_HYDRA){sent[5+2*HY_F1_CUT]=29u&15u;sent[6+2*HY_F1_CUT]=29u>>4;}
        else if(e==ENGI_PROPHET){
            p5_patch_t patch;assert(p5_patch_decode(&patch,sent,sent_n));patch.raw[P5_CUTOFF]=29;
            sent_n=p5_patch_encode(&patch,sent,sizeof sent);assert(sent_n);
        }else if(e==ENGI_FM6){sent[6+FP_ALG]=2;sent[161]=fm6_chk(sent+6,155);}
        else {sent[6+2*3]=12;sent[7+2*3]=0;} /* CZ detune, a valid synthesis byte */
        int32_t stereo[CTL*2];mix_block(stereo,CTL);panic_req=0;
        trk_note_on(TSEL,60,100);mix_block(stereo,CTL);assert(gates());
        uint32_t writes=erases;feed(sent,sent_n,2);
        assert(TSEL->eng_req==e&&!panic_req&&gates()&&erases==writes&&sx_staged());
        mix_block(stereo,CTL);assert(gates());
        if(e==ENGI_J106)assert(j106_patch_of(TSEL)->raw[J106_VCF_FREQ]==29);
        else if(e==ENGI_HYDRA)assert(hy_patch_of(TSEL)->raw[HY_F1_CUT]==29);
        else if(e==ENGI_PROPHET)assert(p5_patch_of(TSEL)->raw[P5_CUTOFF]==29);
        else if(e==ENGI_FM6)assert(fm6_patch[0][FP_ALG]==2);
        else assert(cz_patch[0].raw[3]==12);
        trk_all_off(TSEL);press(B_ENV);
    }
    puts("base SysEx: every engine's native edit buffer changes synthesis parameters with held notes passed");
    /* J106 sends a whole manual buffer while editing, then may send individual
     * controls. Both paths must affect held audio without a SAVE or retrigger. */
    host_preset(TSEL,ENGI_J106,1);sx_drop();press(B_HOME);panic_req=0;
    trk_note_on(TSEL,60,100);int32_t audio[CTL];track_render(TSEL,audio,CTL);
    j106_patch_t live=*j106_patch_of(TSEL);live.raw[J106_VCF_FREQ]=41;
    n=j106_patch_encode(&live,0,framebuf,sizeof framebuf);framebuf[2]=0x31;
    uint32_t writes=erases;feed(framebuf,n,2);
    assert(TSEL->eng_req==ENGI_J106&&j106_patch_of(TSEL)->raw[J106_VCF_FREQ]==41);
    assert(!panic_req&&gates()&&erases==writes&&sx_staged());
    lib.first=300;
    const uint8_t slider[]={0xF0,0x41,0x32,0,J106_VCF_FREQ,19,0xF7};
    feed(slider,sizeof slider,1);
    assert(j106_patch_of(TSEL)->raw[J106_VCF_FREQ]==19&&((j106_patch_t *)sx_stage.raw)->raw[J106_VCF_FREQ]==19);
    framebuf[5+J106_VCF_FREQ]=67;feed(framebuf,n,2);
    assert(lib.first==300&&j106_patch_of(TSEL)->raw[J106_VCF_FREQ]==67&&!panic_req&&gates());
    track_render(TSEL,audio,CTL);assert(TSEL->v[0].active);
    trk_note_off(TSEL,60);press(B_SAVE);
    assert(!sx_staged()&&native_used(ENGI_J106,300)&&native_raw(ENGI_J106,300)[J106_VCF_FREQ]==67);
    press(B_ENV);
    puts("base SysEx: J106 manual buffers and sliders update held notes and retain SAVE destination passed");
    puts("ALL BASE SYSEX TESTS PASSED");
}
'''
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory)
    (path / 'test.c').write_text(prefix + bridge + rx + main)
    subprocess.run([os.getenv('CC', 'cc'), '-O1', '-g', '-w',
                    '-fsanitize=address,undefined', '-fno-sanitize=shift,alignment',
                    '-fno-sanitize-recover=undefined', '-I', str(ROOT),
                    '-I', str(ROOT / 'build/gen'), str(path / 'test.c'),
                    '-lm', '-pthread', '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True, cwd=ROOT)
