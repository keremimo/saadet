/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Exercise the shipping Q23 path, including signals below one PCM16 LSB.
 * Historical Q15 fixtures run separately as regression coverage. */
#define AUDIO_FRAC_BITS 8
#define MELODEE_USB_AUDIO 1
#define RESOURCE_HOST_WORDS 4096u
#define RESOURCE_HOST_LOW_WORDS 0u
#define RESOURCE_HOST_DATA_WORDS 0u
#define RESOURCE_HOST_RETAINED_WORDS 0u
#include "synth_harness.c"
#include <assert.h>

static void transport(void)
{
    const int32_t edges[]={-8388608,-8388607,-65537,-257,-256,-255,-1,0,1,127,255,256,257,65537,8388606,8388607};
    uint8_t bytes[3];
    for(uint32_t i=0;i<NELEM(edges);i++){
        ua_encode(bytes,3,edges[i]);assert(ua_decode(bytes,3)==edges[i]);
        audio_history_t sample;audio_history_write(&sample,edges[i]);
        assert(audio_history_read(&sample)==edges[i]);
    }
    uint32_t random=0x13579bdf;
    for(uint32_t i=0;i<100000;i++){
        random=random*1664525u+1013904223u;
        int32_t x=(int32_t)(random&0xffffffu)-8388608;
        ua_encode(bytes,3,x);assert(ua_decode(bytes,3)==x);
    }
    /* Quantization must preserve a sub-LSB signal's mean when reduced. */
    int64_t sum=0;
    for(uint32_t i=0;i<200000;i++){
        ua_encode(bytes,2,32);sum+=(int16_t)(bytes[0]|(uint16_t)bytes[1]<<8);
    }
    assert(fabs((double)sum*256/200000-32)<2);
    ua_encode(bytes,2,0);assert(bytes[0]==0 && bytes[1]==0);
    ua_reset();ua.play_alt=ua.cap_alt=2;
    for(uint32_t i=0;i<UA_TARGET;i++){
        ua.play[2*i]=1;ua.play[2*i+1]=-1;
    }
    ua.pw=UA_TARGET;
    int32_t pcm[2*CTL];for(uint32_t i=0;i<CTL;i++){pcm[2*i]=17;pcm[2*i+1]=-19;}
    ua_audio(pcm,CTL,4096);
    assert(pcm[0]==18 && pcm[1]==-20 && ua.cap[0]==17 && ua.cap[1]==-19);
    puts("PCM24: signed edges/random values, all history bits, sub-LSB duplex and unbiased PCM16 reduction passed");
}

static void master_curve(void)
{
    double error=0;
    for(int32_t x=-3*8388608;x<=3*8388608;x+=127){
        double input=(double)x/8388608;
        if(input>2)input=2;if(input<-2)input=-2;
        double reference=tanh(input)*(32767.0*256);
        double e=fabs(softclip(x)-reference);if(e>error)error=e;
    }
    assert(error<3.0);
    printf("Q23 master curve: libm maximum error %.3f native LSBs\n",error);
}

static void engines(void)
{
    for(uint32_t engine=0;engine<NENG_SHOWN;engine++){
        power_on();host_preset(TSEL,ENGINE_ORDER[engine],0);
        TSEL->p[P_LEVEL]=64;TSEL->p[P_SPRD]=64;
        for(uint32_t k=0;k<6;k++)trk_note_on(TSEL,48+k*3,64+k*8);
        uint64_t energy=0;uint32_t fractional=0,peak=0;
        for(uint32_t block=0;block<1500;block++){
            if(block==500)for(uint32_t k=0;k<6;k++)trk_note_off(TSEL,48+k*3);
            int32_t pcm[2*CTL];mix_block(pcm,CTL);
            for(uint32_t i=0;i<2*CTL;i++){
                int32_t x=pcm[i];assert(x>=-8388608 && x<=8388607);
                energy+=abs(x);fractional+=(x&255)!=0;
                if((uint32_t)abs(x)>peak)peak=(uint32_t)abs(x);
            }
        }
        assert(energy && fractional>1000);
        printf("Q23 %-7s: polyphony/spread/release, %u fractional samples, peak %u\n",ENGINES[ENGINE_ORDER[engine]]->name,fractional,peak);
    }
    /* An amplitude below the old output LSB must survive float -> PCM. */
    float source[CTL];int32_t output[CTL];vmod_t modulation={.amp0=32768,.amp1=32768};
    for(uint32_t i=0;i<CTL;i++)source[i]=0.00001f;
    fm6_output(source,output,CTL,&modulation,4096,0);
    assert(output[0]>0 && output[0]<256);
    assert(softclip(32)>0 && softclip(-32)<0);
}

/* The limiter's envelope must release on the nightly Q15 schedule: its
 * gain applies at native precision, its timing never changes. A peak first
 * holds it for LIM_HOLD samples. */
static void limiter_release(void)
{
    int32_t env15=36000;
    lim_env=AUDIO_Q15(env15);lim_hold=0;
    for(uint32_t i=0;i<20000u;i++){
        int32_t l=0,r=0;master_out(&l,&r);
        if(env15>26000)env15-=((env15-26000)>>12)+1;
        assert(lim_env==AUDIO_Q15(env15));
    }
    assert(lim_env==LIM_T);
    puts("Q23 limiter: release timing matches the Q15 envelope sample for sample");
    /* a peak sets about 27000 (after the DC blocker); repeats over the
     * threshold and within 1/16 below it hold it, lower ones do not */
    int32_t l=AUDIO_Q15(30000),r=0;master_out(&l,&r);
    int32_t held=lim_env;
    assert(held>AUDIO_Q15(26990) && held<=AUDIO_Q15(27000));
    for(uint32_t i=1;i<=20000u;i++){
        l=i%1500u?0:AUDIO_Q15(i<10000u?26500:25900);r=0;master_out(&l,&r);
        assert(lim_env==held || i>=9000u+LIM_HOLD);
    }
    assert(lim_env<held);
    env15=29000;lim_env=AUDIO_Q15(env15);lim_hold=LIM_HOLD;
    for(uint32_t i=0;i<LIM_HOLD+20000u;i++){
        l=r=0;master_out(&l,&r);
        if(i>=LIM_HOLD && env15>26000)env15-=((env15-26000)>>12)+1;
        assert(lim_env==AUDIO_Q15(env15));
    }
    puts("Q23 limiter: peaks hold the envelope 46 ms, then it releases on the same schedule");
}

static void memory(void)
{
    /* The minimum target main arena holds the synth's engine state. */
    power_on();
    memset(resource,0,sizeof resource);
    resource_failures=0;
    for(uint32_t e=0;e<NENG_SHOWN;e++){
        host_preset(TSEL,ENGINE_ORDER[e],0);trk_note_on(TSEL,60,100);
        for(uint32_t block=0;block<100;block++){int32_t pcm[2*CTL];mix_block(pcm,CTL);}
        assert(resource[RES_ENGINE0].size && !resource_failures);
        assert(resource_used()<=16384u);
    }
    printf("Q23 arena: all three engines fit a 16 KiB bank without cache or SRAM tails\n");
}

int main(void)
{
    _Static_assert(sizeof(audio_history_t)==3,"Packed native PCM24 history");
    transport();master_curve();limiter_release();engines();memory();
    puts("NATIVE 24-BIT AUDIO TESTS PASSED");return 0;
}
