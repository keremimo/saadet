/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
#include "performance_options.h"
/* Engine output -> track level, pan and spread -> master. */
static int32_t part_storage[CTL],mix_l[CTL],mix_r[CTL];
static int32_t *part_buf=part_storage;
static volatile uint32_t kb_mask, kb_layer, kb_pedal;   /* kb_pedal: the button that is the sustain pedal (ARP) */
static volatile uint8_t kb_lock;
#define LIM_T AUDIO_Q15(18000)
#define LIM_HOLD 2048u                   /* 46 ms: longer than a chord's repeat (30.6 ms for C major) */
static int32_t lim_env = LIM_T;
static uint32_t lim_hold;
static audio_dc_t dc_l, dc_r, dce_l, dce_r;

/* DC blocker (~2 Hz), always on: a leaky integrator of the input (Q6 state) subtracted from it.
 * The >> 12 step keeps its remainder (error feedback, 0..4095) and adds it to the next one, so no
 * part of the step is lost: the state follows the input exactly, down to 0 after the sound stops.
 * (A rounded step of (x - dc) / 4096 would stop moving at |x - dc| < 2048 and leave an offset of up
 * to +-31 at the output after silence.) */
static inline __attribute__((always_inline)) int32_t dc_block(int32_t x, audio_dc_t *dc, audio_dc_t *err)
{
    audio_dc_t e = (audio_dc_t)x * 64 - *dc + *err, d = e >> 12;
    *err = e - d * 4096;
    *dc += d;
    return x - ((*dc + 32) >> 6);
}

static __attribute__((noinline,section(".dsp_text"))) void master_out(int32_t *l, int32_t *r)
{
    int32_t al, ar, a;
    *l = dc_block(*l, &dc_l, &dce_l);
    *r = dc_block(*r, &dc_r, &dce_r);

    al = *l < 0 ? -*l : *l;
    ar = *r < 0 ? -*r : *r;
    a = al > ar ? al : ar;
    /* A peak within 1/16 (0.56 dB) of the envelope holds it: the gain stays
     * put while a sustained chord repeats, rather than rising between its
     * peaks and ducking at each one, which growls at the chord's period. */
    if (a > LIM_T && a >= lim_env - (lim_env >> 4))
        lim_hold = LIM_HOLD;
    if (a > lim_env)
        lim_env += (a - lim_env) >> 2;
    else if (lim_hold)
        lim_hold--;
    else if (lim_env > LIM_T) {
        /* Release in whole Q15 steps: the envelope keeps the nightly limiter's timing exactly
         * (its 1/4096 decay and one-unit linear tail) while the gain applies at native precision. */
        lim_env -= (((lim_env - LIM_T) >> (12 + AUDIO_FRAC_BITS)) + 1) * AUDIO_ONE;
        if (lim_env < LIM_T)
            lim_env = LIM_T;
    }
    if (lim_env > LIM_T) {
        int32_t g = (int32_t)(((uint64_t)LIM_T << 15) / (uint32_t)lim_env);   /* < 32768 */
        *l = audio_mul(*l,g,15); /* wide products retain the quiet fractional bits */
        *r = audio_mul(*r,g,15);
    }
    *l = softclip(*l);
    *r = softclip(*r);
}

/* length of one division (N_DIV order) in samples at the song tempo */
static const uint8_t DIV_DEN[6] = {1, 2, 4, 8, 3, 6};    /* original IDs stay fixed; slow rates append */
static uint8_t midi_sync_active;                 /* armed by an incoming Start/Continue/Stop, never a clock pulse */
static uint32_t midi_beat_samples;                    /* zero until an external clock has a measured tempo */
static uint32_t beat_samples(void)
{
    return midi_sync_active && midi_beat_samples ? midi_beat_samples : (uint32_t)FS * 60u / (uint32_t)song.g[G_BPM];
}
static uint32_t division_samples(uint32_t quarter, uint32_t div)
{
    return div < 6u ? quarter / DIV_DEN[div] : div < 10u ? quarter << (div - 5u) : quarter / DIV_DEN[div % 6u];
}
static uint32_t div_samples(uint32_t div) { return division_samples(beat_samples(), div); }

static inline __attribute__((always_inline)) void mix_add(int32_t l,int32_t r,uint32_t i)
{ mix_l[i]+=l;mix_r[i]+=r; }

/* External sequencing uses musical units, independent of the measured tempo.
 * 24 clocks/beat and every division divide exactly; tempo estimates only set
 * LFO timing and the display, never move an already recorded step. */
#define MIDI_BEAT_UNITS 24576u
static uint32_t seq_beat_samples(void)
{
    return midi_sync_active ? MIDI_BEAT_UNITS : beat_samples();
}
static uint32_t seq_div_samples(uint32_t div)
{
    return division_samples(seq_beat_samples(), div);
}


static void events_block(uint32_t n);

typedef struct {
    int16_t level, pan;
    uint8_t valid;
    int32_t lvl, gl, gr;
} mix_cache_t;
static mix_cache_t mix_cache[NTRK];
static __attribute__((noinline)) void mix_cache_update(const track_t *t, mix_cache_t *mc)
{
    mc->level=t->p[P_LEVEL];mc->pan=t->p[P_PAN];mc->valid=1;
    mc->lvl=LEVEL_Q12[mc->level & 127];
    mc->gl=4096-(mc->pan>0?mc->pan*64:0);mc->gr=4096+(mc->pan<0?mc->pan*64:0);
}
/* b: the part, spread_buf its left side; s: half the width, 64 = hard left and right of PAN */
static __attribute__((noinline)) void mix_spread(track_t *t, int32_t *b, uint32_t n, int32_t s)
{
    int32_t *d=spread_buf;
    int32_t lvl=LEVEL_Q12[t->p[P_LEVEL] & 127],pan=t->p[P_PAN];
    int32_t pa=clamp(pan-s,-64,63),pb=clamp(pan+s,-64,63);
    int32_t gla=4096-(pa>0?pa*64:0),gra=4096+(pa<0?pa*64:0);
    int32_t glb=4096-(pb>0?pb*64:0),grb=4096+(pb<0?pb*64:0);
    int32_t glm=(gla+glb)>>1,grm=(gra+grb)>>1,gld=(gla-glb)>>1,grd=(gra-grb)>>1,pk=AUDIO_Q15(t->peak);
    for(uint32_t i=0;i<n;i++){
        int32_t x=audio_clamp(audio_mul(audio_clamp(b[i],-884000,884000),lvl,12),-524287,524287);
        int32_t y=audio_clamp(audio_mul(audio_clamp((d[i]*2)-b[i],-884000,884000),lvl,12),-524287,524287);
        int32_t a=x<0?-x:x;
        if(a>pk)pk=a;
        mix_add(audio_mul(x,glm,12)+audio_mul(y,gld,12),audio_mul(x,grm,12)+audio_mul(y,grd,12),i);
    }
    t->peak=pk>>AUDIO_FRAC_BITS;
}
#if MELODEE_MIX_FAST
#define MIX_MONO_GENERAL mix_mono_general
#else
#define MIX_MONO_GENERAL mix_mono_samples
#endif
static __attribute__((noinline,section(".dsp_text"))) void MIX_MONO_GENERAL(track_t *t,int32_t *b,uint32_t n,const mix_cache_t *mc)
{
    int32_t lvl=mc->lvl,gl=mc->gl,gr=mc->gr,pk=AUDIO_Q15(t->peak);
    for(uint32_t i=0;i<n;i++){
        int32_t x=audio_clamp(audio_mul(audio_clamp(b[i],-884000,884000),lvl,12),-524287,524287),a=x<0?-x:x;
        if(a>pk)pk=a;
        mix_add(audio_mul(x,gl,12),audio_mul(x,gr,12),i);
    }
    t->peak=pk>>AUDIO_FRAC_BITS;
}
#undef MIX_MONO_GENERAL
#if MELODEE_MIX_FAST
static __attribute__((noinline,section(".dsp_text"))) void mix_mono_samples(track_t *t,int32_t *b,uint32_t n,const mix_cache_t *mc)
{
    if(mc->pan){mix_mono_general(t,b,n,mc);return;}
    /* Centre pan needs no extra channel multiplications. */
    int32_t lvl=mc->lvl,pk=AUDIO_Q15(t->peak);
    for(uint32_t i=0;i<n;i++){
        int32_t x=audio_clamp(audio_mul(audio_clamp(b[i],-884000,884000),lvl,12),-524287,524287),a=x<0?-x:x;
        if(a>pk)pk=a;
        mix_add(x,x,i);
    }
    t->peak=pk>>AUDIO_FRAC_BITS;
}
#endif

static int32_t solo_gain[NPART]={32768};
static void mix_solo(track_t *t,int32_t *b,uint32_t n)
{
    uint32_t ti=(uint32_t)(t-trk);
    int32_t from=solo_gain[ti],to=1?32768:0;
    if(from==32768 && to==32768)return;
    for(uint32_t i=0;i<n;i++){
        from += clamp(to-from,-256,256); /* 128 samples, same GLO solo ramp as before */
        b[i]=mulq16(b[i],(uint32_t)from<<1);
        if(spread_on)spread_buf[i]=mulq16(spread_buf[i],(uint32_t)from<<1);
    }
    solo_gain[ti]=from;
}
#if MELODEE_BATCH_AUDIO && !MELODEE_LEGACY_EXTRAS
static int32_t batch_pcm[NPART][CTL],batch_spread[NPART][CTL];
static uint32_t batch_rendered[NPART];static uint8_t batch_spread_on[NPART],batch_ready;
static __typeof__(mod) batch_mod[NPART];
#if MELODEE_DUAL_CORE
static uint8_t batch_posted[NPART];
#endif
static int16_t batch_params[NPART][P_COUNT];
static __attribute__((noinline)) void mix_prepare_batch(uint32_t n,uint32_t nt)   /* nt: the tracks in use */
{
    audio_batch_active=1;
#if MELODEE_DUAL_CORE
    memset(batch_posted,0,sizeof batch_posted);
    /* Keep the measured CPU0 service reserve for native FM6 workloads.
     * Per-voice controls and ghosts themselves are costed queue jobs. */
    audio_batch_native_fm6=0;
    for(uint32_t p=0;p<nt && !audio_batch_native_fm6;p++)if(trk[p].engine==ENGI_FM6 && resource[RES_ENGINE0+p].size==sizeof(fm6_part_t)){
        const fm6_part_t *f=resource[RES_ENGINE0+p].ptr;
        for(uint32_t v=0;v<NVOICE;v++)if(f->v[v].played && !f->v[v].still && !f->v[v].frozen){audio_batch_native_fm6=1;break;}
    }
#endif
    for(uint32_t p=0;p<nt;p++){
        track_t *t=&trk[p];mod_begin(t);batch_mod[p]=mod;memcpy(batch_params[p],t->p,sizeof t->p);
        spread_on=t->p[P_SPRD]>0 && (trk_vmode(t)==V_POLY || trk_vmode(t)==V_UNISON);
        batch_spread_on[p]=spread_on;spread_buf=batch_spread[p];
        batch_rendered[p]=track_render_audio(t,batch_pcm[p],n,0);
        if(mod.on)mod_end(t);
    }
    audio_batch_flush();audio_batch_active=0;batch_ready=1;
}
#endif
static __attribute__((noinline)) void mix_part(track_t *t,uint32_t n)
{
    int rendered;
#if MELODEE_BATCH_AUDIO && !MELODEE_LEGACY_EXTRAS
    if(batch_ready){
        uint32_t p=(uint32_t)(t-trk);mod=batch_mod[p];
        part_buf=batch_pcm[p];
        if(mod.on)for(uint32_t k=0;k<mod.nk;k++)t->p[mod.kid[k]]=batch_params[p][mod.kid[k]];
        spread_on=batch_spread_on[p];spread_buf=batch_spread[p];rendered=(int)batch_rendered[p];
        if(rendered){
            if(spread_on)spread_sum(part_buf);
            const engine_t *e=ENGINES[t->engine];
#if MELODEE_DUAL_CORE
            if(!batch_posted[p] && e->post)e->post(t,part_buf,n,(uint32_t)rendered);
#else
            if(e->post)e->post(t,part_buf,n,(uint32_t)rendered);
#endif
        }
    }else
#endif
    {
        part_buf=part_storage;spread_buf=spread_storage;mod_begin(t);
        spread_on=t->p[P_SPRD]>0 && (trk_vmode(t)==V_POLY || trk_vmode(t)==V_UNISON);
        rendered=(int)track_render_audio(t,part_buf,n,0);
    }
    /* the engine's own stereo stage (JUNO-106's chorus) replaces SPREAD: its input is the whole mono part */
    int32_t width=(t->p[P_SPRD]+1)>>1;
    if(rendered && ENGINES[t->engine]->stereo && ENGINES[t->engine]->stereo(t,part_buf,spread_buf,n)){spread_on=1;width=64;}
    AP_START(mix_start);
    if(rendered){
        mix_solo(t,part_buf,n);
        if(spread_on)mix_spread(t,part_buf,n,width);
        else{
            mix_cache_t *mc=&mix_cache[(uint32_t)(t-trk)];
            if(!mc->valid || mc->level!=t->p[P_LEVEL] || mc->pan!=t->p[P_PAN])mix_cache_update(t,mc);
            void (*volatile mix)(track_t *,int32_t *,uint32_t,const mix_cache_t *)=mix_mono_samples;
            mix(t,part_buf,n,mc);
        }
    }
    AP_END(AP_MIX,mix_start);
    if(mod.on)mod_end(t);
}
static __attribute__((noinline,section(".dsp_text"))) void master_samples(int32_t *out,uint32_t n)
{
    for(uint32_t i=0;i<n;i++){int32_t l=mix_l[i],r=mix_r[i];master_out(&l,&r);out[2u*i]=l;out[2u*i+1u]=r;}
}
static __attribute__((noinline)) void mix_block(int32_t *out,uint32_t n)
{
    memset(mix_l,0,n*sizeof *mix_l);memset(mix_r,0,n*sizeof *mix_r);
    AP_START(events_start);events_block(n);AP_END(AP_EVENTS,events_start);
    uint32_t nt=trk_n();                                /* (the tracks not in use: none of their CPU) */
#if MELODEE_BATCH_AUDIO && !MELODEE_LEGACY_EXTRAS
    mix_prepare_batch(n,nt);
#endif
    for(uint32_t i=0;i<nt;i++)mix_part(&trk[i],n);
#if MELODEE_BATCH_AUDIO && !MELODEE_LEGACY_EXTRAS
    batch_ready=0;part_buf=part_storage;spread_buf=spread_storage;
#endif
    /* The sole synth feeds the master directly. */
    for(uint32_t i=0;i<n;i++){mix_l[i]=audio_mul(mix_l[i],(int32_t)song.master_q12,12);mix_r[i]=audio_mul(mix_r[i],(int32_t)song.master_q12,12);}
    void (*volatile run)(int32_t *,uint32_t)=master_samples;
    AP_START(output_start);run(out,n);AP_END(AP_OUTPUT,output_start);
}
