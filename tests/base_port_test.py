#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Exercise the actual patched USB routines and inspect storage boundaries."""
from pathlib import Path
import os
import re
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
s=(ROOT/'build/fm1-base-platform/bsp/chip/usb.c').read_text()

def function(signature):
    start=s.index(signature);start=s.rfind('\n',0,start)+1
    opening=s.index('{',start);depth=1;end=opening+1
    while depth:
        depth+=(s[end]=='{')-(s[end]=='}');end+=1
    return s[start:end]

# Compile the shipping stream definitions/routines with mocked endpoint IO.
ring=s[s.index('#if FELUCCA_UAC_DEEP'):s.index('#if FELUCCA_UAC_48K\n#include "uac_fir.h"')]
ring=re.sub(r'__attribute__\(\(section\(".pool"\)\)\)','',ring)
helper=s[s.index('static int32_t sd_pcm_read'):s.index('static inline int32_t uac_s16')]
desc=s[s.index('#define D_CFG'):s.index('#if FELUCCA_CDC\n#define CFG_LEN')]
prefix='''#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#define FELUCCA_UAC 1
#define FELUCCA_UAC_OUT 1
#define FELUCCA_UAC_48K 0
#define FELUCCA_UAC_DEEP 0
#define FELUCCA_CDC 0
#define FELUCCA_USB_LAYOUT 0
#define FELUCCA_USB_PID 1
#define UAC_AS_IF 2u
#define HALF_FRAMES 128u
#define RING_PUBLISH() ((void)0)
'''
mock='''static struct {int config;} usb={1};
enum {S_INDEX,S_RXCSR1,S_RXCSR2,S_RXCOUNT1,S_RXCOUNT2,S_CSR0};
static uint32_t payload_bytes;
static uint32_t sie_rd(uint32_t r){return r==S_RXCSR1?1:r==S_RXCOUNT1?payload_bytes&255:r==S_RXCOUNT2?payload_bytes>>8:0;}
static void sie_wr(uint32_t r,uint32_t v){(void)r;(void)v;}
static void fm1_usb_rx_sync(void){}
static uint8_t volume_reply[2];
static void e0_send(const uint8_t *p,uint16_t n,uint16_t max){(void)max;memcpy(volume_reply,p,n);}
'''
main=r'''
int main(void){
    const int32_t values[]={-8388608,-8388607,-65537,-257,-1,0,1,255,256,65537,8388607};
    for(uint32_t i=0;i<sizeof values/sizeof *values;i++){
        uint8_t b[3];sd_pcm_write(b,3,values[i]);assert(sd_pcm_read(b,3)==values[i]);
        sd_pcm_write(b,2,values[i]);assert(abs(sd_pcm_read(b,2)-values[i])<=512);if(!values[i])assert(sd_pcm_read(b,2)==0);
    }
    assert(CFG_DESC_PLAIN[2]+256*CFG_DESC_PLAIN[3]==sizeof CFG_DESC_PLAIN);
    uint32_t formats[2]={0},alt2=0;
    for(uint32_t i=0;i<sizeof CFG_DESC_PLAIN;){
        const uint8_t *p=CFG_DESC_PLAIN+i;assert(p[0]&&i+p[0]<=sizeof CFG_DESC_PLAIN);
        if(p[1]==4&&p[3]==2)alt2++;
        if(p[1]==0x24&&p[0]==11&&p[2]==2&&p[3]==1){if(p[5]==2&&p[6]==16)formats[0]++;if(p[5]==3&&p[6]==24)formats[1]++;}
        i+=p[0];
    }
    assert(alt2==2&&formats[0]==2&&formats[1]==2);
    int32_t samples[256];for(uint32_t i=0;i<256;i++)samples[i]=(i&1)?-65537:65537;
    uac.alt=2;uac.flowing=1;uac_render_start();assert(uac.go);
    uac_tap(samples,128);assert(ua_ring[(ua_w-1)&(UA_N-1)].l==65537);
    /* Skip startup silence; native low bits reach the actual packet. */
    ua_r=ua_w-128;uac.fill_min=104;
    uint32_t count=uac_packet(ep4tx);assert(count==44);
    assert(sd_pcm_read((uint8_t *)ep4tx,3)==65537);
    assert(sd_pcm_read((uint8_t *)ep4tx+3,3)==-65537);
    uac.alt=1;count=uac_packet(ep4tx);assert(count==44);
    assert(abs(sd_pcm_read((uint8_t *)ep4tx,2)-65537)<=512);
    /* Playback decodes both widths before native mixing/resampling. */
    uo.alt=2;payload_bytes=2*6;
    for(uint32_t i=0;i<4;i++)sd_pcm_write((uint8_t *)ep4rx+3*i,3,(i&1)?-65537:65537);
    uo_service();assert(uo_w==2&&uo_ring[0].l==65537&&uo_ring[0].r==-65537);
    uo.alt=1;payload_bytes=2*4;
    for(uint32_t i=0;i<4;i++){((uint8_t *)ep4rx)[2*i]=0;((uint8_t *)ep4rx)[2*i+1]=(i&1)?255:1;}
    uo_service();assert(uo_w==4&&uo_ring[2].l==65536&&uo_ring[2].r==-65536);
    uo.alt=2;uo_w=UO_TGT+2;uo_r=0;uo.go=0;
    for(uint32_t i=0;i<uo_w;i++)uo_ring[i]=(sd_pcm_frame){65537,-65537};
    memset(samples,0,sizeof samples);uac_play(samples,32,4096);
    assert(samples[0]==65537&&samples[1]==-65537);
    uint8_t setup[8]={0x21,1,0,2,0,5,2,0},db[2]={0,250}; /* -6 dB */
    assert(sd_volume_setup(setup)&&sd_volume_pending==2);
    assert(sd_volume_data(db,2)&&ua_host_volume_q8==-1536&&ua_host_target_q15==16423);
    setup[0]=0xA1;setup[1]=0x81;assert(sd_volume_setup(setup));
    assert(volume_reply[0]==0&&volume_reply[1]==250);
    uint8_t mute[8]={0x21,1,0,1,0,5,1,0},on=1;
    assert(sd_volume_setup(mute)&&sd_volume_data(&on,1)&&ua_host_mute);
    ua_host_gain_q15=0;uo_r=0;uo_w=UO_TGT+2;uo.go=0;
    memset(samples,0,sizeof samples);uac_play(samples,32,4096);assert(samples[0]==0&&samples[1]==0);
    ua_host_volume_reset();assert(!ua_host_mute&&ua_host_gain_q15==32768);
    puts("base USB: native PCM16/24 duplex, descriptors and packet paths passed");
}
'''
volume=(ROOT/'platform/usb_volume.h').read_text()
code=prefix+ring+desc+helper+mock+volume+'\n'.join(function(x) for x in (
    'void uac_render_start(', 'void uac_tap(const int32_t *out',
    'uint32_t uac_packet(', 'void uo_service(', 'void uac_play('))+main
with tempfile.TemporaryDirectory() as d:
    path=Path(d);(path/'test.c').write_text(code)
    subprocess.run([os.getenv('CC','cc'),'-O1','-g','-fsanitize=address,undefined','-fno-sanitize=shift','-o',str(path/'test'),str(path/'test.c')],check=True)
    subprocess.run([str(path/'test')],check=True)
# Evaluate the actual object enum/map, excluding retired object identities.
st=(ROOT/'firmware/src/storage.c').read_text()
enum=st[st.index('enum { OBJ_SETTINGS'):st.index('\n',st.index('enum { OBJ_SETTINGS'))]
mapping=st[st.index('static uint32_t st_sector'):st.index('\n\n',st.index('static uint32_t st_sector'))]
m=re.search(r'static int st_retired\([^}]+}',st,re.S)
assert m
storage='#include <stdint.h>\n#include <stdio.h>\n#define ST_SECTOR 4096u\n'+enum+'\n'+mapping+'\n'+m[0]+'''\nint main(void){for(unsigned i=0;i<OBJ_COUNT;i++)if(!st_retired(i))for(unsigned j=0;j<2;j++)printf("%x\\n",st_sector(i,j));}'''
with tempfile.TemporaryDirectory() as d:
    path=Path(d);(path/'map.c').write_text(storage)
    subprocess.run(['cc',str(path/'map.c'),'-o',str(path/'map')],check=True)
    sectors=[int(x,16) for x in subprocess.check_output([str(path/'map')],text=True).split()]
assert len(sectors)==len(set(sectors))
app_h=(ROOT/'platform/app.h').read_text()
base_a=[int(x,16) for x in re.search(r'#define APP_STORE_MAP \{([^}]+)\}',app_h)[1].replace('u','').split(',')]
base={x+offset for x in base_a for offset in (0,4096)}
assert len(base)==2*len(base_a)
assert all(0x93000<=x<0xE0000 for x in base)
assert not base.intersection(sectors)
assert all(0x93000<=x<0xE0000 or 0xE5000<=x<0xFF000 for x in sectors)
print(f'base storage: {len(sectors)} legacy sectors, no platform/OTA overlap')
# Exercise the real ingress code across an overflow/recovery boundary. A stale
# hold from before the panic must not suppress a later note-off on another port.
ingress=r'''
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#define MQ 4u
#define RING_PUBLISH() ((void)0)
static uint32_t midi_in_q[MQ],midi_in_ms[MQ],midi_in_time[MQ],fm1_ms;
static uint8_t midi_in_source[MQ],midi_in_overflow;
static uint16_t midi_in_pulse[MQ],midi_rx_pulses[3];
static uint32_t mi_w,mi_r;
static uint32_t fm1_ticks(void){return 0;}
static int sd_ble_connected(void){return 1;}
static int sd_ble_device_state(void){return 0;}
static uint8_t settings_thru;
static void sd_midi_thru(uint32_t p,int port){(void)p;(void)port;}
#include "platform/midi_ingress.h"
static unsigned aborts;
static void saadet_sysex_byte(uint8_t b){if(b>=0x80&&b<0xF8)aborts++;}
static void sysex_byte(uint8_t b){(void)b;}
static void midi_enqueue(uint32_t p,uint32_t port){(void)p;(void)port;}
static struct {uint8_t sx_on;} usb;
'''+function('void midi_in_event(uint32_t pkt)')+r'''
int main(void){
    uint32_t on=9u|0x90u<<8|60u<<16|100u<<24,off=8u|0x80u<<8|60u<<16;
    sd_ingress(on,1);sd_ingress(on,3);
    sd_ingress(on+(1u<<16),1);sd_ingress(on+(2u<<16),1);
    sd_ingress(on+(3u<<16),1);assert(midi_in_overflow);
    for(unsigned p=0;p<4;p++)for(unsigned ch=0;ch<16;ch++)for(unsigned w=0;w<4;w++)assert(sd_midi_keys[p][ch][w]==0);
    sd_ingress(on,3);assert(sd_midi_keys[2][0][1]==0); /* ignored until the render acknowledges panic */
    mi_r=mi_w;midi_in_overflow=0;
    sd_ingress(on,1);unsigned before=mi_w;sd_ingress(off,1);
    assert(mi_w==before+1&&midi_in_q[(mi_w-1)%MQ]==off);
    midi_in_event(on);assert(aborts==1);
    midi_in_event(15u|0xF8u<<8);assert(aborts==1); /* real-time clock does not abort */
    puts("base MIDI: overflow recovery and interrupted SysEx status hook passed");
}
'''
with tempfile.TemporaryDirectory() as d:
    path=Path(d);(path/'ingress.c').write_text(ingress)
    subprocess.run(['cc','-O1','-g','-fsanitize=address,undefined','-I',str(ROOT),str(path/'ingress.c'),'-o',str(path/'ingress')],check=True)
    subprocess.run([str(path/'ingress')],check=True)
