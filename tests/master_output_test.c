/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* The way out: a chord stays under the limiter, CZ-1 at the other engines' level, and every cut to whole samples
 * dithered, with one part and with two. run_tests.sh builds it at the shipping Q23 and in the Q15 fixtures': a cut's LSB
 * is its own in both. MASTER's law and exact silence are adapted from Melodee's test of the same name. */
#include <assert.h>
#define MELODEE_USB_AUDIO 1
#define RESOURCE_HOST_WORDS 4096u
#define RESOURCE_HOST_LOW_WORDS 0u
#define RESOURCE_HOST_DATA_WORDS 0u
#define RESOURCE_HOST_RETAINED_WORDS 0u
#include "synth_harness.c"
#include "../platform/ceiling.h"

static const uint32_t ENGINES_SHIPPED[]={ENGI_PROPHET,ENGI_FM6,ENGI_CZ,ENGI_J106};
static const uint32_t PROBES[]={0,1,2,3,5,8,13,21,34};   /* factory sounds of every engine */
static const uint8_t TRIAD[]={60,64,67},FOUR[]={60,64,67,71};
static void reset_master(void){lim_env=LIM_T;lim_hold=0;dc_l=dc_r=dce_l=dce_r=0;dith_r=DITH_SEED;}
static void settle(void){panic_req|=3;for(uint32_t q=0;q<600;q++){int32_t pcm[2*CTL];mix_block(pcm,CTL);}}
static double db(double x){return 20*log10(x/AUDIO_Q15(32768)+1e-12);}

/* blocks of what is sounding at this MASTER: the peak of the mix before MASTER, the most the limiter took off (dB, <= 0) */
typedef struct{double peak,gr;}level_t;
static level_t level_of(uint32_t blocks,uint32_t master)
{
    int32_t env=lim_env;double peak=0;
    song.master_q12=master;
    for(uint32_t b=0;b<blocks;b++){
        int32_t pcm[2*CTL];mix_block(pcm,CTL);
        for(uint32_t i=0;i<CTL;i++){
            double a=fabs((double)mix_l[i]),c=fabs((double)mix_r[i]);
            if(a>peak)peak=a;if(c>peak)peak=c;
            assert(pcm[2*i]>=-AUDIO_Q15(32768) && pcm[2*i]<AUDIO_Q15(32768));
            assert(pcm[2*i+1]>=-AUDIO_Q15(32768) && pcm[2*i+1]<AUDIO_Q15(32768));
        }
        if(lim_env>env)env=lim_env;
    }
    return (level_t){peak,20*log10((double)LIM_T/env)};
}
/* one engine's sound, these notes, this MASTER, from the boot state */
static level_t sound(uint32_t e,uint32_t pi,const uint8_t *notes,uint32_t n,uint32_t master)
{
    power_on();host_preset(TSEL,e,pi);reset_master();
    for(uint32_t k=0;k<n;k++)trk_note_on(TSEL,notes[k],100);
    level_t r=level_of(1000,master);settle();
    return r;
}

/* the limiter takes the overs only: a peak at -2.2 dBFS passes it, one above is limited, then released */
static void limiter_threshold(void)
{
    power_on();reset_master();
    for(uint32_t i=0;i<20000u;i++){
        int32_t l=AUDIO_Q15(i&64u?25500:-25500),r=l;master_out(&l,&r);
        assert(lim_env==LIM_T);
    }
    int32_t top=LIM_T;
    for(uint32_t i=0;i<4000u;i++){
        int32_t l=AUDIO_Q15(i&64u?31000:-31000),r=l;master_out(&l,&r);
        if(lim_env>top)top=lim_env;
    }
    assert(top>AUDIO_Q15(30000));
    for(uint32_t i=0;i<400000u;i++){int32_t l=0,r=0;master_out(&l,&r);}
    assert(lim_env==LIM_T);
    puts("limiter: a peak at -2.2 dBFS passes untouched, a peak above is limited and released");
}

static void tone(double *re,double *im,const int32_t *x,uint32_t n,double cycles_per_sample);

/* the knee after it: exact up to the threshold, then libm's K + H tanh(d / H) (d stopping at 2 H), odd, rising,
 * under full scale however far the limiter's attack overshoots (its bound is 4 x the threshold) */
static void knee(void)
{
    const double K=LIM_T,H=KNEE_H,tol=AUDIO_ONE>1?4.0:1.5;
    double err=0;int32_t last=0;
    for(int32_t x=0;x<=4*LIM_T;x+=AUDIO_ONE>1?97:1){
        int32_t y=master_knee(x);
        assert(master_knee(-x)==-y);
        if(x<=LIM_T)assert(y==x);
        else{double want=K+H*tanh(fmin(x-K,2*H)/H),e=fabs(y-want);if(e>err)err=e;}
        assert(y>=last && y<AUDIO_Q15(32767));last=y;
    }
    printf("knee: exact to the threshold, libm within %.2f of %.1f native LSBs above it, top %.1f\n",err,tol,(double)last/AUDIO_ONE);
    assert(err<tol && last>AUDIO_Q15(32500));
}

/* a chord held just under the threshold comes out with no intermod (a tanh over the whole range made it -26 dB) */
static void chord_intermod(void)
{
    enum{N=65536};
    static int32_t sig[N];
    static const int32_t k[3]={400,504,599};                 /* cycles in the window: every product on a bin of its own */
    static const int8_t prod[][3]={{2,-1,0},{-1,2,0},{0,2,-1},{0,-1,2},{2,0,-1},{-1,0,2},{1,1,-1},{1,-1,1},{-1,1,1},
                                   {3,0,0},{0,3,0},{0,0,3}};
    double top=0,scale,re,im,tone_min=1e300,worst=0;
    for(uint32_t pass=0;pass<2;pass++){
        if(pass){scale=AUDIO_Q15(24576)/top;power_on();reset_master();}   /* -2.5 dBFS at the peak */
        for(uint32_t i=0;i<N;i++){
            double s=0;
            for(uint32_t j=0;j<3;j++)s+=sin(2*M_PI*k[j]*i/N+0.7*j);
            if(!pass){if(fabs(s)>top)top=fabs(s);continue;}
            int32_t l=(int32_t)lrint(s*scale),r=l;master_out(&l,&r);
            assert(l==r && lim_env==LIM_T);
            sig[i]=l;
        }
    }
    for(uint32_t j=0;j<3;j++){tone(&re,&im,sig,N,(double)k[j]/N);if(hypot(re,im)<tone_min)tone_min=hypot(re,im);}
    for(uint32_t p=0;p<NELEM(prod);p++){
        int32_t f=prod[p][0]*k[0]+prod[p][1]*k[1]+prod[p][2]*k[2];
        tone(&re,&im,sig,N,(double)f/N);if(hypot(re,im)>worst)worst=hypot(re,im);
    }
    printf("chord at -2.5 dBFS: the loudest intermod or third harmonic %.1f dB under the quietest note\n",20*log10(worst/tone_min+1e-300));
    assert(worst/tone_min<pow(10.0,-80.0/20));
}

/* a voice of CZ-1 about as loud as the other engines (Melodee's: CZ-1 had twice their scale) */
static void cz_level(void)
{
    double sum[3]={0,0,0};
    static const uint32_t e[3]={ENGI_PROPHET,ENGI_FM6,ENGI_CZ};
    static const uint8_t c4[]={60};
    for(uint32_t k=0;k<3;k++){
        for(uint32_t p=0;p<NELEM(PROBES);p++){
            level_t r=sound(e[k],PROBES[p],c4,1,4096);
            sum[k]+=db(r.peak);
        }
        sum[k]/=NELEM(PROBES);
    }
    printf("voice peak: PROPHET %.1f, FM6 %.1f, CZ-1 %.1f dBFS (mean of %zu sounds)\n",sum[0],sum[1],sum[2],NELEM(PROBES));
    assert(fabs(sum[2]-(sum[0]+sum[1])/2)<3.0);
}

/* A chord over a held note: under the limiter at a MASTER of 2048 (70 % of its travel), and a light touch at full. */
static void chords(void)
{
    static const uint32_t e[3]={ENGI_PROPHET,ENGI_FM6,ENGI_CZ};
    for(uint32_t k=0;k<3;k++){
        double mean=0,worst=0;
        for(uint32_t p=0;p<NELEM(PROBES);p++){
            level_t half=sound(e[k],PROBES[p],TRIAD,3,2048),full=sound(e[k],PROBES[p],TRIAD,3,4096);
            assert(half.gr==0.0);
            mean+=full.gr;if(full.gr<worst)worst=full.gr;
        }
        mean/=NELEM(PROBES);
        printf("%s triad at full MASTER: the limiter takes %.1f dB (worst sound %.1f), none at 2048\n",ENGINES[e[k]]->name,mean,worst);
        assert(mean>-3.0 && worst>-5.0);
    }
    puts("chords: stay under the limiter at MASTER 2048, a light touch at full");
}

/* LAYER: both parts summing. Under the limiter at 2048 but for the JUNO-106, the loudest; bounded at full, output in range */
static void two_parts(void)
{
    for(uint32_t k=0;k<NELEM(ENGINES_SHIPPED);k++){
        uint32_t e=ENGINES_SHIPPED[k];level_t r[2];
        for(uint32_t g=0;g<2;g++){
            power_on();host_preset(&trk[0],e,1);host_preset(&trk[1],e,2);
            song.g[G_PMODE]=PT_LAYER;for(uint32_t q=0;q<8;q++){int32_t pcm[2*CTL];mix_block(pcm,CTL);}
            reset_master();
            for(uint32_t n=0;n<NELEM(FOUR);n++){trk_note_on(&trk[0],FOUR[n],100);trk_note_on(&trk[1],FOUR[n],100);}
            r[g]=level_of(1000,g?4096:2048);
            settle();
        }
        printf("LAYER %s, 4 + 4 notes: the limiter takes %.1f dB at 2048, %.1f dB at full\n",ENGINES[e]->name,r[0].gr,r[1].gr);
        assert(e==ENGI_J106?r[0].gr>-2.0:r[0].gr==0.0);
        assert(r[1].gr>-8.0);
    }
    /* the same seeds, the same two parts: the dither table belongs to the block, not to the order of the parts' work */
    int32_t a[2*CTL],b[2*CTL];
    for(uint32_t run=0;run<2;run++){
        memset(resource,0,sizeof resource);memset(mix_cache,0,sizeof mix_cache);memset(&mod,0,sizeof mod);fm6_fn_reset();
        power_on();host_preset(&trk[0],ENGI_CZ,1);host_preset(&trk[1],ENGI_J106,1);
        song.g[G_PMODE]=PT_LAYER;parts_mode_was=PT_LAYER;
        reset_master();vage=0;rng_state=0x12345678u;mod_seed=0x2545F491u;
        trk_note_on(&trk[0],60,100);trk_note_on(&trk[1],67,100);
        for(uint32_t q=0;q<40;q++)mix_block(run?b:a,CTL);
        settle();
    }
    assert(!memcmp(a,b,sizeof a));
    puts("two parts: LAYER stays in range, bounded by the limiter, and repeatable");
}

/* the mean of a cut over many tables: a whole-sample error that follows the signal has a mean, dither has none */
static double level_mean(int32_t x,int32_t lvl,uint32_t pan)
{
    track_t *t=&trk[0];mix_cache_t mc={.level=0,.pan=(int16_t)pan,.valid=1,.lvl=lvl,.gl=4096,.gr=4096};
    int32_t b[CTL];int64_t sum=0;
    for(uint32_t k=0;k<20000u;k++){
        for(uint32_t i=0;i<CTL;i++)b[i]=x;                   /* (the level works in place) */
        memset(mix_l,0,sizeof mix_l);memset(mix_r,0,sizeof mix_r);dither_block();
        if(pan)mix_mono_general(t,b,CTL,&mc);
        else mix_mono_samples(t,b,CTL,&mc);
        for(uint32_t i=0;i<CTL;i++)sum+=mix_l[i];
    }
    return (double)sum/(20000.0*CTL);
}
static double master_mean(int32_t x,uint32_t master)
{
    int64_t sum=0;
    for(uint32_t k=0;k<20000u;k++){
        dither_block();
        for(uint32_t i=0;i<CTL;i++){
            int32_t l=x,r=x;master_gain(&l,&r,(int32_t)master,i);
            assert(l==r);sum+=l;                             /* one dither: mono stays L = R */
        }
    }
    return (double)sum/(20000.0*CTL);
}
static void near(double got,double want,const char *what)
{
    if(fabs(got-want)>=0.03)printf("%s: %.3f, not %.3f\n",what,got,want);
    assert(fabs(got-want)<0.03);
}
/* Below one LSB, the cuts: the rounded-down part's and MASTER's lose it, or turn it into an offset */
static void cuts_keep_the_mean(void)
{
    power_on();
    for(int32_t x=-3;x<=3;x++){
        if(!x)continue;
        near(level_mean(x,2048,0),x*0.5,"part level, centre");
        near(level_mean(x,1024,0),x*0.25,"part level, -12 dB");
        near(level_mean(x,2048,1),x*0.5,"part level, panned path");
        near(master_mean(x,2048),x*0.5,"MASTER -6 dB");
        near(master_mean(x,1024),x*0.25,"MASTER -12 dB");
    }
    /* the spread path: its level for both channels */
    trk[0].p[P_SPRD]=64;
    {
        int32_t b[CTL],left[CTL];int64_t sum=0;track_t *t=&trk[0];
        t->p[P_LEVEL]=98;t->p[P_PAN]=0;                      /* LEVEL_Q12: -7 dB */
        spread_buf=left;
        for(uint32_t k=0;k<20000u;k++){
            for(uint32_t i=0;i<CTL;i++)b[i]=left[i]=1;
            memset(mix_l,0,sizeof mix_l);memset(mix_r,0,sizeof mix_r);dither_block();
            mix_spread(t,b,CTL,0);
            for(uint32_t i=0;i<CTL;i++)sum+=mix_l[i];
        }
        near((double)sum/(20000.0*CTL),LEVEL_Q12[98]/4096.0,"part level, spread");
        spread_buf=spread_storage;
    }
    puts("cuts: below one LSB, the part's level and MASTER keep the signal's mean, and mono stays L = R");
}
/* The pan paths on one level kernel: each still gives its law's left and right, and the meter reads the part's level */
static void pan_paths(void)
{
    enum{X=3000,Y=1000};
    const int32_t x=AUDIO_Q15(X),left=AUDIO_Q15(Y);
    track_t *t=&trk[0];
    int32_t b[CTL],d[CTL];
    power_on();t->p[P_LEVEL]=112;t->p[P_PAN]=0;                  /* LEVEL_Q12: unity */
    assert(LEVEL_Q12[112]==4096);
    mix_cache_t centre={.level=112,.pan=0,.valid=1,.lvl=4096,.gl=4096,.gr=4096};
    mix_cache_t panned={.level=112,.pan=32,.valid=1,.lvl=4096,.gl=4096-32*64,.gr=4096};
    for(uint32_t path=0;path<3;path++){
        for(uint32_t i=0;i<CTL;i++){b[i]=x;d[i]=left;}
        memset(mix_l,0,sizeof mix_l);memset(mix_r,0,sizeof mix_r);dither_block();t->peak=0;
        spread_buf=d;
        if(path==0)mix_mono_samples(t,b,CTL,&centre);
        else if(path==1)mix_mono_general(t,b,CTL,&panned);
        else mix_spread(t,b,CTL,64);                             /* hard apart: left holds Y, the sum X */
        /* spread: the side 2Y - X, gains from mix_spread's pan law at +-64 */
        double wl=path==0?X:path==1?X*0.5:(X*2080.0+(2.0*Y-X)*2016.0)/4096.0;
        double wr=path==2?(X*2048.0-(2.0*Y-X)*2048.0)/4096.0:X;
        for(uint32_t i=0;i<CTL;i++){
            if(fabs(mix_l[i]-wl*AUDIO_ONE)>4.0||fabs(mix_r[i]-wr*AUDIO_ONE)>4.0)printf("path %u: %d %d, not %.0f %.0f\n",path,mix_l[i],mix_r[i],wl*AUDIO_ONE,wr*AUDIO_ONE);
            assert(fabs(mix_l[i]-wl*AUDIO_ONE)<=4.0 && fabs(mix_r[i]-wr*AUDIO_ONE)<=4.0);
        }
        assert(t->peak>=X-1 && t->peak<=X+1);
    }
    spread_buf=spread_storage;
    puts("pan: centre, panned and spread keep their law on the shared level, the meter reads the level");
}
/* A whole sample's error of each engine's voice: truncating follows the note (-1/2 LSB times its sign), dithered none.
 * The same voice twice from the same state, a loud one to know the note, then a very quiet one against it */
static void voice_run(uint32_t e,uint32_t pi,int32_t amp,int32_t *out,uint32_t blocks)
{
    power_on();host_preset(TSEL,e,pi);reset_master();vage=0;rng_state=0x12345678u;mod_seed=0x2545F491u;
    trk_note_on(TSEL,60,100);
    voice_t *v=0;for(uint32_t i=0;i<NVOICE && !v;i++)if(TSEL->v[i].active)v=&TSEL->v[i];
    assert(v);
    const engine_t *E=ENGINES[e];
    vmod_t m={0};m.pitch16=60*16;m.inc=tuned_pitch_inc((uint32_t)m.pitch16);m.shape=64<<8;m.envq15=32767;m.amp0=m.amp1=amp;
    for(uint32_t b=0;b<blocks;b++){
        dither_block();
        if(E->block)E->block(TSEL);
        memset(out+b*CTL,0,CTL*sizeof *out);
        E->render(TSEL,v,out+b*CTL,CTL,&m);
    }
}
static void voices_follow_no_error(void)
{
    enum{BLOCKS=4000};
    static int32_t loud[BLOCKS*CTL],quiet[BLOCKS*CTL];
    /* a few LSBs at Q15, a few hundred at Q23: the mean error must vanish either way */
    const int32_t amp=AUDIO_FRAC_BITS?1:4;
    for(uint32_t k=0;k<NELEM(ENGINES_SHIPPED);k++){
        uint32_t e=ENGINES_SHIPPED[k];
        double sum=0,n=0;
        voice_run(e,0,0,quiet,BLOCKS);                       /* an exact 0 stays 0 */
        for(uint32_t i=0;i<BLOCKS*CTL;i++)assert(!quiet[i]);
        voice_run(e,0,32767,loud,BLOCKS);
        voice_run(e,0,amp,quiet,BLOCKS);
        for(uint32_t i=0;i<BLOCKS*CTL;i++){
            double want=loud[i]*(double)amp/32767.0;
            if(fabs(want)<2.0)continue;
            sum+=(quiet[i]-want)*(want<0?-1.0:1.0);n+=1.0;
        }
        assert(n>1000.0);
        printf("%s: the error along the note %.3f LSB over %.0f samples\n",ENGINES[e]->name,sum/n,n);
        assert(fabs(sum/n)<0.1);
        settle();
    }
    puts("voices: no engine's cut follows its note");
}
/* The JUNO-106's own stages after its voices: the low shelf, the high-pass and the chorus cut a whole part */
static void j106_stages(void)
{
    power_on();host_preset(TSEL,ENGI_J106,0);
    track_t *t=TSEL;uint8_t *raw=j106_patch_of(t)->raw;
    int32_t a[CTL],b[CTL];
    for(uint32_t q=0;q<10;q++){mix_block(a,CTL/2);}
    j106_part_t *P=j106_part(0);
    assert(P);
    /* a low shelf holds 1 at 1.6 */
    j106_switch_set(raw,J106_HPF,0);P->hp_x=P->hp_y=P->boost=0;P->hpf=0;
    {
        int64_t sum=0;
        for(uint32_t k=0;k<20000u;k++){
            dither_block();for(uint32_t i=0;i<CTL;i++)a[i]=1;
            j106_post(t,a,CTL,1);
            if(k>=10000u)for(uint32_t i=0;i<CTL;i++)sum+=a[i];
        }
        near((double)sum/(10000.0*CTL),1.6,"JUNO-106 low shelf");
    }
    /* the high-pass passes a signal at half the rate: 1 stays near 1, not 0 */
    j106_switch_set(raw,J106_HPF,2);P->hp_x=P->hp_y=0;P->hpf=2;
    {
        double sum=0;uint32_t phase=0;
        for(uint32_t k=0;k<20000u;k++){
            dither_block();for(uint32_t i=0;i<CTL;i++)a[i]=(phase+i)&1u?1:-1;
            j106_post(t,a,CTL,1);
            for(uint32_t i=0;i<CTL;i++)sum+=a[i]*((phase+i)&1u?1.0:-1.0);
        }
        sum/=20000.0*CTL;
        printf("JUNO-106 high-pass: 1 LSB at half the rate reads %.3f\n",sum);
        assert(sum>0.8);
    }
    /* the chorus: a steady 1 comes out at sqrt(2) on each side, 2 sqrt(2) in the sum */
    j106_switch_set(raw,J106_CHORUS,1);P->chorus=0;
    {
        int64_t l=0,s=0;
        for(uint32_t k=0;k<20000u;k++){
            dither_block();for(uint32_t i=0;i<CTL;i++)a[i]=1;
            assert(j106_stereo(t,a,b,CTL));
            if(k>=10000u)for(uint32_t i=0;i<CTL;i++){s+=a[i];l+=b[i];}
        }
        near((double)l/(10000.0*CTL),2*0.7071,"JUNO-106 chorus, left");
        near((double)s/(10000.0*CTL),4*0.7071,"JUNO-106 chorus, sum");
    }
    puts("JUNO-106: the shelf, high-pass and chorus keep what is below one LSB");
}
/* the spectrum of a fading note at the LSB: its error's harmonics stand still when the cut rounds down */
static void tone(double *re,double *im,const int32_t *x,uint32_t n,double cycles_per_sample)
{
    *re=*im=0;
    for(uint32_t i=0;i<n;i++){
        double w=2*M_PI*cycles_per_sample*i;
        *re+=x[i]*cos(w);*im+=x[i]*sin(w);
    }
}
static void harmonics(void)
{
    enum{BLOCKS=8192,PERIOD=256};
    static int32_t sig[BLOCKS*CTL];
    int32_t b[CTL];
    for(uint32_t where=0;where<2;where++){
        power_on();
        track_t *t=&trk[0];mix_cache_t mc={.level=0,.pan=0,.valid=1,.lvl=4,.gl=4096,.gr=4096};
        for(uint32_t k=0;k<BLOCKS;k++){
            memset(mix_l,0,sizeof mix_l);memset(mix_r,0,sizeof mix_r);dither_block();
            for(uint32_t i=0;i<CTL;i++){   /* 4000 * 4 / 4096: a tone of about 4 LSBs after the cut */
                int32_t x=(int32_t)(4000.0*sin(2*M_PI*3.0*(k*CTL+i)/PERIOD));
                if(where){int32_t r=x;master_gain(&x,&r,4,i);mix_l[i]=x;}else b[i]=x;
            }
            if(!where)mix_mono_samples(t,b,CTL,&mc);
            memcpy(sig+k*CTL,mix_l,sizeof mix_l);
        }
        double re,im,f,h;
        tone(&re,&im,sig,BLOCKS*CTL,3.0/PERIOD);f=hypot(re,im);
        tone(&re,&im,sig,BLOCKS*CTL,9.0/PERIOD);h=hypot(re,im);
        printf("%s: third harmonic %.1f dB under the tone\n",where?"MASTER":"part level",20*log10(h/f));
        assert(h/f<pow(10.0,-50.0/20));
    }
    puts("harmonics: a tone at the LSB leaves none standing");
}
/* MASTER scales what is under it, within the dither; zero is silent and full is exact; silence stays exact */
static void volume(void)
{
    static const uint32_t levels[]={0,1,256,1024,2048,4095};
    enum{BLOCKS=64};
    static int32_t unity[2*BLOCKS*CTL];
    /* a held note's mix at full MASTER */
    power_on();host_preset(TSEL,ENGI_CZ,1);reset_master();
    trk_note_on(TSEL,60,100);
    for(uint32_t b=0;b<BLOCKS;b++){
        int32_t pcm[2*CTL];mix_block(pcm,CTL);
        memcpy(unity+2*b*CTL,mix_l,CTL*sizeof *unity);memcpy(unity+(2*b+1)*CTL,mix_r,CTL*sizeof *unity);
    }
    settle();
    for(uint32_t g=0;g<NELEM(levels);g++){
        uint32_t master=levels[g];
        for(uint32_t i=0;i<NELEM(unity);i++){
            int32_t l=unity[i],r=unity[i];
            dith_r=i*2654435761u+1u;dither_block();
            master_gain(&l,&r,(int32_t)master,0);
            int32_t want=(int32_t)(((int64_t)unity[i]*(int32_t)master+2048)>>12);
            assert(unity[i]&&master?l>=want-1&&l<=want+1:!l);
        }
    }
    for(uint32_t i=0;i<NELEM(unity);i++){
        int32_t l=unity[i],r=unity[i];
        dith_r=i*2654435761u+1u;dither_block();
        master_gain(&l,&r,4096,0);assert(l==unity[i]);
    }
    /* .. and the output has it: a step at 1/4 of MASTER comes out as a step a quarter as high */
    {
        int32_t pcm[2*CTL],want=AUDIO_Q15(2000);
        want-=want>>12;                                      /* (the DC blocker's first step takes 1/4096) */
        power_on();reset_master();song.master_q12=1024;dither_block();
        for(uint32_t i=0;i<CTL;i++)mix_l[i]=mix_r[i]=AUDIO_Q15(8000);
        master_samples(pcm,CTL);
        assert(abs(pcm[0]-want)<=3 && pcm[0]==pcm[1]);
    }
    /* a new table each block: the cuts of one block don't repeat the last one's */
    {
        int16_t was[DITH_N];int32_t pcm[2*CTL];
        power_on();reset_master();mix_block(pcm,CTL);
        for(uint32_t b=0;b<16;b++){
            memcpy(was,dith,sizeof was);mix_block(pcm,CTL);
            assert(memcmp(was,dith,sizeof was));
        }
    }
    /* nothing sounding: silence at every MASTER, from the voices' buffer to the output */
    for(uint32_t g=0;g<NELEM(levels);g++){
        power_on();reset_master();song.master_q12=levels[g];
        for(uint32_t b=0;b<400;b++){
            int32_t pcm[2*CTL];mix_block(pcm,CTL);
            for(uint32_t i=0;i<2*CTL;i++)assert(!pcm[i]);
        }
    }
    puts("MASTER: within the dither of its law, exact at full, silence stays exact");
}
/* The -6 dB ceiling: one bit, the last cut before the codec */
static void ceiling(void)
{
    int32_t lr[2*CTL];
    for(int32_t x=-5;x<=5;x++){
        int64_t sum=0;
        for(uint32_t k=0;k<20000u;k++){
            for(uint32_t i=0;i<2*CTL;i++)lr[i]=x;
            sd_ceiling(lr,CTL);
            for(uint32_t i=0;i<CTL;i++){
                assert(lr[2*i]==lr[2*i+1]);                  /* mono stays L = R */
                assert(x&1?abs(2*lr[2*i]-x)==1:2*lr[2*i]==x);   /* even halves exactly, 0 included; odd rounds either way */
                sum+=lr[2*i];
            }
        }
        double err=(double)sum/(20000.0*CTL)-x*0.5;
        if(fabs(err)>=0.03)printf("ceiling %d: mean error %.3f\n",(int)x,err);
        assert(fabs(err)<0.03);                              /* the mean follows the signal, odd or even */
    }
    for(uint32_t i=0;i<2*CTL;i++)lr[i]=i&1u?-9000000:9000000;
    sd_ceiling(lr,CTL);
    for(uint32_t i=0;i<2*CTL;i++)assert(lr[i]>=-4194304 && lr[i]<=4194304);
    puts("ceiling: half the level, no error in the mean on odd or even samples, L = R, silence exact");
}

int main(void)
{
    setvbuf(stdout,0,_IOLBF,0);
    limiter_threshold();
    knee();
    chord_intermod();
    cz_level();
    chords();
    two_parts();
    cuts_keep_the_mean();
    pan_paths();
    voices_follow_no_error();
    j106_stages();
    harmonics();
    volume();
    ceiling();
    puts("master output: ALL PASSED");
    return 0;
}
