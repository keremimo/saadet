/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Main-loop-owned physical holds. Shared channel controls retain Saadet's
 * policy; one port's note-off/disconnect cannot release another port's key. */
static uint32_t sd_midi_keys[4][16][4];
static uint8_t sd_midi_pedal[4][16],sd_ble_was_up;
static void sd_queue_midi(uint32_t packet,uint32_t source)
{
    if(midi_in_overflow)return;
    if(mi_w-mi_r>=MQ){
        memset(sd_midi_keys,0,sizeof sd_midi_keys);memset(sd_midi_pedal,0,sizeof sd_midi_pedal);
        midi_in_overflow=1;return;
    }
    uint32_t i=mi_w%MQ;midi_in_q[i]=packet;midi_in_ms[i]=fm1_ms;
    midi_in_time[i]=fm1_ticks();midi_in_source[i]=(uint8_t)(source==4u?3u:source); /* either BLE link shares the clock source */
    midi_in_pulse[i]=midi_rx_pulses[midi_in_source[i]-1u];
    RING_PUBLISH();mi_w++;
}
static uint32_t sd_other_key(uint32_t port,uint32_t ch,uint32_t note)
{
    uint32_t held=0;
    for(uint32_t p=0;p<4u;p++)if(p!=port)held|=sd_midi_keys[p][ch][note>>5];
    return (held>>(note&31u))&1u;
}
static uint32_t sd_pedal(uint32_t ch)
{
    return sd_midi_pedal[0][ch]|sd_midi_pedal[1][ch]|sd_midi_pedal[2][ch]|sd_midi_pedal[3][ch];
}
static void sd_release_port(uint32_t port,uint32_t ch)
{
    for(uint32_t note=0;note<128u;note++){
        uint32_t *word=&sd_midi_keys[port][ch][note>>5],bit=1u<<(note&31u);
        if(!(*word&bit))continue;*word&=~bit;
        if(!sd_other_key(port,ch,note))sd_queue_midi(8u|(0x80u|ch)<<8|note<<16,port+1u);
    }
    if(sd_midi_pedal[port][ch]){
        sd_midi_pedal[port][ch]=0;
        sd_queue_midi(11u|(0xB0u|ch)<<8|64u<<16|sd_pedal(ch)<<24,port+1u);
    }
}
static void sd_ingress(uint32_t packet,uint32_t source)
{
    uint32_t port=source-1u,status=(packet>>8)&255u,ch=status&15u;
    uint32_t st=status&240u,note=(packet>>16)&127u,vel=(packet>>24)&127u;
    if(port>=4u)return;
    /* Count real-time pulses even while the audio consumer is clearing an
     * overflowing note stream. Queue snapshots recover every discarded pulse. */
    if(status==0xF8u)midi_rx_pulses[(source==4u?3u:source)-1u]++;
    if(midi_in_overflow)return;
    if(st==0x80u||(st==0x90u&&!vel)){
        uint32_t *word=&sd_midi_keys[port][ch][note>>5],bit=1u<<(note&31u);
        if(!(*word&bit))return;*word&=~bit;
        if(sd_other_key(port,ch,note))return;
    }else if(st==0x90u)sd_midi_keys[port][ch][note>>5]|=1u<<(note&31u);
    else if(st==0xB0u&&note==64u){
        sd_midi_pedal[port][ch]=vel>=64u?127u:0u;
        packet=(packet&0xFFFFFFu)|sd_pedal(ch)<<24;
    }else if(st==0xB0u&&(note==120u||note==123u)){
        sd_release_port(port,ch);return;
    }
    sd_queue_midi(packet,source);
}
/* THRU passed a port's holds on (sd_midi_thru): a link that drops, or THRU turned off, ends them there too, before
 * sd_release_port forgets them. Its own note-offs, whatever another port holds: the out is that port's alone */
static const uint8_t sd_thru_port[4]={0,2,1,3};   /* port -> sd_midi_in's: USB, TRS, BLE, the BLE device */
static uint8_t sd_thru_was;
static void sd_thru_release(uint32_t port,uint32_t ch)
{
    for(uint32_t note=0;note<128u;note++)
        if((sd_midi_keys[port][ch][note>>5]>>(note&31u))&1u)sd_midi_thru(8u|(0x80u|ch)<<8|note<<16,sd_thru_port[port]);
    if(sd_midi_pedal[port][ch])sd_midi_thru(11u|(0xB0u|ch)<<8|64u<<16,sd_thru_port[port]);
}
static void sd_connections(void)
{
    uint8_t up=(uint8_t)(!!sd_ble_connected()|(sd_ble_device_state()==4?2:0));
    if(sd_thru_was&&!settings_thru)
        for(uint32_t port=0;port<4u;port++)for(uint32_t ch=0;ch<16u;ch++)sd_thru_release(port,ch);
    sd_thru_was=settings_thru;
    for(uint32_t link=0;link<2u;link++)if((sd_ble_was_up&~up)&(1u<<link))
        for(uint32_t ch=0;ch<16u;ch++){
            if(settings_thru)sd_thru_release(2u+link,ch);
            sd_release_port(2u+link,ch);
        }
    sd_ble_was_up=up;
}
