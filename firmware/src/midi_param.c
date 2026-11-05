/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Controllers mapped to sound parameters (CC5 glide, CC7 level, CC10 pan, CC71/74 the engine's macros (HYDRA: its macros 4 and 3), CC72/73/75
 * release/attack/decay, the native engines' own envelopes included). Included by seq.c before midi_input.c. */
/* Native envelope loops run only for their CC, outside the audio IRQ's hot loops. */
static __attribute__((noinline)) void midi_parameter(track_t *t,uint32_t cc,uint32_t value)
{
    uint32_t id=P_COUNT;
    switch(cc){case 5:id=P_GLIDE;break;case 7:id=P_LEVEL;break;case 10:id=P_PAN;break;
    case 72:id=P_REL;break;case 73:id=P_ATK;break;case 75:id=P_DEC;break;
    case 74:case 71:{
        static const uint8_t map[NENGINES]={0,0,0x30,0x56,0,0x07,0,0,0,0,0x30,0x34,0x30,0,0,0,0,0,0,0x12,0,0x12,0,0,0,0x34};
        uint32_t macro=(map[t->eng_req%NENGINES]>>(cc==74?4:0))&15u;
        if(macro)id=P_E0+macro-1u;else if(cc==74 && t->eng_req==ENGI_CZ)id=P_ED_FLT;
        break; }default:return;}
    if((cc==72 || cc==73 || cc==75) && (t->eng_req==ENGI_PROPHET || t->eng_req==ENGI_FM6 || t->eng_req==ENGI_CZ || t->eng_req==ENGI_J106)){   /* (the engine's own envelope: its motion too) */

        if(t->eng_req==ENGI_PROPHET){
            p5_edit_value(t,cc==72?P5_RELEASE_AMP:cc==73?P5_ATTACK_AMP:P5_DECAY_AMP,value);
        }else if(t->eng_req==ENGI_J106){
            j106_edit_value(t,cc==72?J106_ENV_R:cc==73?J106_ENV_A:J106_ENV_D,(int32_t)value);
        }else if(t->eng_req==ENGI_FM6){
            uint8_t raw[FP_SIZE+1u];memcpy(raw,fm6_patch[trk_index(t)],sizeof raw);
            for(uint32_t op=0;op<6;op++)raw[op*FP_OP+(cc==72?3:cc==73?0:1)]=(uint8_t)(99u-value*99u/127u);
            fm6_put_patch(trk_index(t),raw,0);
        }else{
            uint32_t tr=trk_index(t);uint8_t raw[CZ_BYTES];
            for(uint32_t line=0;line<2;line++) {
                uint32_t stage=cc==72?(cz_patch[tr].raw[CZ_ENV_END[line][2]]&7u):cc==73?0:1;
                if(cz_ed_put(tr,LCZ_EBASE(line,2)+stage,99u-value*99u/127u,raw))memcpy(cz_patch[tr].raw,raw,128u);
            }
        }
        return;
    }
    if(id>=P_COUNT)return;
    const param_desc_t *d=id<P_E0?&TP[id]:&ENGINES[t->eng_req]->edit[id-P_E0];
    if(d->max==d->min)return;
    int32_t v=d->min+(int32_t)(value*(uint32_t)(d->max-d->min)/127u);
    if(id==P_PAN)v=(int32_t)value-64;
    t->p[id]=(int16_t)v;
}
