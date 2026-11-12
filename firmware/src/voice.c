/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
#include "performance_options.h"
/* Voice allocation, envelopes, LFO and per-track rendering.
 * Runs in the audio ISR.
 *
 * Each synth part has its own NVOICE voices (so MONO / LEGATO / UNISON keep using
 * v[0..]; POLY uses the engine's cap, NPOLY or FM6's 16), but the parts' sounding voices
 * share a weighted budget (voice_budget: normally 16, resident audio with CPU1
 * online 18 at 360 MHz, scaled with the clock running; FM6 takes one unit,
 * ordinary engines two, serial Prophet three).
 * Each part in use is guaranteed an equal share of the budget and may borrow what the
 * others are not using. A part that starts a voice while the budget is full takes one
 * (voice_victim) from a part over its share when it is within its own, else from itself:
 * the oldest released voice, else the oldest extra UNISON voice, else the oldest held
 * voice of a POLY part that is not its lowest note. A voice given up keeps rendering down
 * an S curve over VOICE_FADE_BLOCKS (stage 4) while the new note starts beside it, in the
 * engine's spare voice (engine_t.spare): a tail cut any faster than that is heard as a tick
 * under a quiet note. The fade spends budget the CPU is known to carry (voice_ceiling), so
 * it never adds to the worst case. With no slot or budget left the note waits (voice_wait)
 * and every fade hurries to let it in. Only a note already sounding restarts in place.
 * Extra UNISON voices only start when there is room. */
#define VOICE_FADE_BLOCKS 32u                           /* 32 x 32 samples: 23 ms */
#define VOICE_FADE_HURRY 4u                             /* a note waits: 5.8 ms at most */
/* Note events only, never per sample: small first. The FM-1's BLE image has almost no flash left, and this unit builds for speed */
#define VOICE_EVENT __attribute__((noinline,minsize))
static uint32_t vage;                                   /* voice ages: one clock for every part */
static int32_t lfo_wave(track_t *t, uint32_t ph)
{
    switch (t->p[P_LWAVE]) {
    case 1:
        return osc_tri(ph);
    case 2:
        return (int32_t)(ph >> 16) - 32768;
    case 3:
        return ph < 0x80000000u ? 32767 : -32767;
    case 4:
        return (int32_t)(t->lfo_rnd >> 16) - 32768;
    default:
        return osc_sine(ph);
    }
}

/* the S&H value of a new LFO cycle. Parts 1..3 draw it from the shared generator (rng); part 4 has its
 * own xorshift, so the shared sequence, which also seeds the engines' noise (PHYS, VOICE, ..), does not
 * depend on part 4's LFO (the golden renders rely on it) */
#define LFO_SHARED_RNG 3u
static uint32_t lfo_rand(track_t *t)
{
    uint32_t s = t->lfo_rnd ? t->lfo_rnd : 0x9E3779B9u;
    if ((uint32_t)(t - trk) < LFO_SHARED_RNG)
        return rng();
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}

/* LFO 2 (1.2, #154; params.c N_LSYNC): with SYNC on, one cycle lasts the note value of the tempo (mix.c div_samples:
 * BPM, or the external clock's measured tempo while it runs), whatever RATE says (the matrix's RATE too): the phase
 * advances 2^32 x CTL / cycle a block (two 32-bit divides; the 1-bit truncation of 0xFFFFFFFF drifts by less than a
 * millionth of a cycle a cycle). OFF: RATE's table, as before */
static uint32_t div_samples(uint32_t div);         /* (mix.c) */
static struct { uint32_t cycle, inc; } lfo_sync_cache[NTRK];
static __attribute__((noinline)) uint32_t lfo_sync_inc(const track_t *t)   /* (not inlined: its divides stay out of
                                                                           * the audio ISR's loop over the tracks) */
{
    uint32_t cyc, q, r;
    cyc = div_samples(LSYNC_DIV[(uint32_t)t->p[P_LSYNC] % NELEM(LSYNC_DIV)]);
    cyc = cyc ? cyc : 1u;
    uint32_t ti=(uint32_t)(t-trk)%NTRK;
    if(lfo_sync_cache[ti].cycle==cyc)return lfo_sync_cache[ti].inc;
    q = 0xFFFFFFFFu / cyc;
    r = 0xFFFFFFFFu % cyc;
    lfo_sync_cache[ti].cycle=cyc;
    return lfo_sync_cache[ti].inc=q*CTL+(r*CTL+CTL)/cyc;
}

static void track_lfo_tick(track_t *t)
{
    uint32_t old = t->lfo_ph;
    t->lfo_ph += t->p[P_LSYNC] ? lfo_sync_inc(t) : LFO_INC[t->p[P_LRATE] & 127];
    if (t->lfo_ph < old) {
        t->lfo_prev = (int16_t)mod_sh(t);               /* mod.c SLEW: it glides on from there */
        t->lfo_rnd = lfo_rand(t);
    }
    t->lfo_val = lfo_wave(t, t->lfo_ph);
    if (t->p[P_LPOL])                                   /* UNI: 0..+1 (the depths reach as far, one way only) */
        t->lfo_val = (t->lfo_val + 32768) >> 1;
    if (t->lfo_fade < 32767) {
        int32_t step = (int32_t)(ENV_LIN[t->p[P_LFADE] & 127] >> 9);
        t->lfo_fade = t->p[P_LFADE] ? clamp(t->lfo_fade + (step ? step : 1), 0, 32767) : 32767;
    }
}

static uint32_t capacity_steals,admission_rejections;
#if MELODEE_AUDIO_BENCH
static uint32_t audio_bench_until;
static int audio_bench_active(void)
{
#ifdef FM1_IRQ_TARGET
    return audio_bench_until && (int32_t)(audio_bench_until-fm1_ticks())>0;
#else
    return audio_bench_until!=0;
#endif
}
#endif
/* voices the engine may use (POLY and UNISON): its cap, else NPOLY */
static uint32_t trk_nvoice(const track_t *t)
{
    const engine_t *e = ENGINES[t->engine];
    uint32_t c = e->cap ? e->cap(t) : e->poly;
    return !c ? NPOLY : c < NVOICE ? c : NVOICE;
}

/* the part's voice mode: P_VOICE, POLY for an engine of hits (engine_t.oneshot: DRUM) */
static uint32_t trk_vmode(const track_t *t)
{
    return ENGINES[t->engine]->oneshot ? V_POLY : (uint32_t)t->p[P_VOICE];
}

/* ------------------------------------------------- the shared voice budget --- */
#if MELODEE_ADMISSION
static const track_t *voice_admitting;
static uint8_t voice_admitting_p5parts;
static uint32_t voice_p5_parts(void)
{
    if(voice_admitting)return voice_admitting_p5parts;
    uint32_t parts=0;
    for(uint32_t p=0;p<NPART;p++)if(trk[p].engine==ENGI_PROPHET){
        uint32_t live=0;for(uint32_t i=0;i<NVOICE;i++)live|=trk[p].v[i].active && trk[p].v[i].stage!=4u;
        parts+=live!=0;
    }
    return parts;
}
#endif
static VOICE_EVENT uint32_t voice_units_now(const track_t *t)
{
    const engine_t *e = ENGINES[t->engine];
    /* GLO > WEIGHTS: the player's points per voice replace the built-in rule; AUTO (0) keeps it. */
    int k = e == &ENG_P5_TEST ? 0 : e == &ENG_CZ ? 1 : e == &ENG_FM6 ? 2 : -1;
    if (k >= 0 && settings_vpoints[k] > 0)
        return (uint32_t)settings_vpoints[k] * VQ;
#if MELODEE_ADMISSION
    if(e==&ENG_P5_TEST){
        return (voice_p5_parts()>1u?4u:3u)*VQ;
    }
    if(e==&ENG_CZ)return 3u*VQ;
    if(e==&ENG_FM6)return 2u*VQ;
#endif
    return e->units ? e->units(t) : (trk_nvoice(t) > NPOLY ? 1u : 2u) * VQ;
}
/* A chord asks the budget dozens of times in one block, and an engine may work its cost out each time (HYDRA from its patch and
 * macros): within a block (events_block, voice_units_block) a part's is worked out once, for the engine it has then. Nothing
 * else changes a part's patch while the block's events run */
static struct { uint8_t on, have, eng[NPART]; uint32_t units[NPART]; } vu;
static void voice_units_block(int on) { vu.on = (uint8_t)on; vu.have = 0; }
static VOICE_EVENT uint32_t voice_units(const track_t *t)
{
    uint32_t p = (uint32_t)(t - trk);
    if (!vu.on || p >= NPART)
        return voice_units_now(t);
    if (!((vu.have >> p) & 1u) || vu.eng[p] != t->engine) {
        vu.units[p] = voice_units_now(t);
        vu.eng[p] = t->engine;
        vu.have |= (uint8_t)(1u << p);
    }
    return vu.units[p];
}

/* the budget with CPU1: VBUDGET at 360 MHz, scaled with the clock running by main.c clock_boot (16 at a 320 MHz a
 * soft reset kept from a 1.3.x session); VBUDGET on the host. Units here, steps (VQ a unit) from voice_budget on */
static uint32_t vbudget_cpu1 = VBUDGET;
static uint32_t voice_budget(void)
{
#if MELODEE_AUDIO_BENCH
    if(audio_bench_active())return NPART*NVOICE*3u*VQ;
#endif
#if MELODEE_RAM_AUDIO
#if MELODEE_DUAL_CORE
    /* CPU1 retirement removes the extra headroom. New notes then use the
     * original budget; overload shedding still handles already-active work. */
    uint32_t selected=(uint32_t)settings_budget*VQ;
    return audio_worker_online?selected*vbudget_cpu1/VBUDGET:selected<16u*VQ?selected:16u*VQ;
#else
    return ((uint32_t)settings_budget<16u?(uint32_t)settings_budget:16u)*VQ;
#endif
#else
    return (uint32_t)settings_budget*VQ;
#endif
}

static VOICE_EVENT uint32_t part_busy(const track_t *t)             /* what a part claims of its share: its voices that stay and its waiting notes */
{
    uint32_t i, n = t->wait_n;
    for (i = 0; i < NVOICE; i++)
        n += t->v[i].active && t->v[i].stage != 4u;
    return n * voice_units(t);
}
static VOICE_EVENT uint32_t voices_busy(void)                       /* the budget units rendering now: every voice, a fading one too */
{
    uint32_t p, i, u = 0;
    for (p = 0; p < NPART; p++) {
        uint32_t n = 0;
        for (i = 0; i < NVOICE; i++)
            n += trk[p].v[i].active;
        u += n * voice_units(&trk[p]);
    }
    return u;
}
static VOICE_EVENT uint32_t voices_claimed(void)                    /* the units the parts' notes claim: a fading voice claims none */
{
    uint32_t p, u = 0;
    for (p = 0; p < NPART; p++)
        u += part_busy(&trk[p]);
    return u;
}
/* the units the CPU is known to carry at once, fades included: the calibrated budget, or the player's own if higher */
static VOICE_EVENT uint32_t voice_ceiling(void)
{
    uint32_t c, b = voice_budget();
#if MELODEE_RAM_AUDIO && MELODEE_DUAL_CORE
    c = (audio_worker_online ? vbudget_cpu1 : 16u) * VQ;
#elif MELODEE_RAM_AUDIO
    c = 16u * VQ;
#else
    c = VBUDGET * VQ;
#endif
    return b > c ? b : c;
}
/* the units notes may claim before a new one takes a voice. Two parts sounding can fill the budget, so they leave one voice's
 * room for its fade; one part's voice cap leaves it already where the CPU has it to spare (Prophet: 8 x 2 of 18) */
static VOICE_EVENT uint32_t voice_limit(const track_t *t)
{
    uint32_t p, others = 0, b = voice_budget(), c = voice_ceiling(), u = voice_units(t);
    for (p = 0; p < NPART; p++)
        if (&trk[p] != t)
            others += part_busy(&trk[p]);
    return others && b + u > c && c > u ? c - u : b;
}
static VOICE_EVENT uint32_t voices_waiting(void)
{
    uint32_t p, n = 0;
    for (p = 0; p < NPART; p++)
        n += trk[p].wait_n;
    return n;
}
static VOICE_EVENT uint32_t voices_fading(void)
{
    uint32_t p, i;
    for (p = 0; p < NPART; p++)
        for (i = 0; i < NVOICE; i++)
            if (trk[p].v[i].active && trk[p].v[i].stage == 4u)
                return 1;
    return 0;
}
/* the parts a voice for self may come from (bit per part): within its share, those over theirs; over it, its own */
static VOICE_EVENT uint32_t voice_lenders(const track_t *self)
{
    uint32_t p, mask = 0, share = voice_budget() / trk_n();
    if (trk_n() == 1u)
        return (1u << NPART) - 1u;
    if (part_busy(self) + voice_units(self) > share)
        return 1u << (uint32_t)(self - trk);
    for (p = 0; p < NPART; p++)
        if (&trk[p] != self && part_busy(&trk[p]) > share)
            mask |= 1u << p;
    return mask;
}

static VOICE_EVENT uint32_t lowest_held(const track_t *t)           /* index of the lowest held note (the bass), NVOICE = none */
{
    uint32_t i, low = NVOICE;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].gate && t->v[i].stage != 4u && (low == NVOICE || t->v[i].note < t->v[low].note))
            low = i;
    return low;
}

/* the voice to give up for a new one (see the top); soft: only released voices and
 * extra voices of other non-POLY parts (also ones left after a mode change).
 * Returns its index, *pp its part; NVOICE = none */
static VOICE_EVENT uint32_t voice_victim(const track_t *self, int soft, track_t **pp)
{
    uint32_t p, i, best = NVOICE, cat = 4, lenders = voice_lenders(self);
    for (p = 0; p < NPART; p++) {
        track_t *t = &trk[p];
        uint32_t mode = trk_vmode(t);
        uint32_t low = mode == V_POLY ? lowest_held(t) : NVOICE;
        if ((soft && t == self) || !((lenders >> p) & 1u))
            continue;
        for (i = 0; i < NVOICE; i++) {
            const voice_t *v = &t->v[i];
            uint32_t c;
            if (!v->active || v->stage == 4u)
                continue;
            if (!v->gate)
                c = 1;                                  /* released */
            else if (mode != V_POLY && i > 0)
                c = 2;                                  /* extra UNISON, or a voice left by a mode / cap change */
            else if (!soft && mode == V_POLY && i != low)
                c = 3;                                  /* held, not the bass */
            else
                continue;
            if (c < cat || (c == cat && v->age < (*pp)->v[best].age)) {
                cat = c;
                best = i;
                *pp = t;
            }
        }
    }
    return best;
}

static uint32_t voice_kills;                            /* voices given up (budget, overload): console, hostsim */
static uint32_t voice_kills_part[NPART];                /* identify the audible victim, not only total load */
/* fade out over the next blocks (env_tick stage 4). A held voice stays gated while it does, so the fade is not cut short
 * by a patch's own fast key-up (a CZ-1 end step can take 1.4 ms); everything that asks what is held skips stage 4 */
static VOICE_EVENT void voice_fade(voice_t *v)
{
    v->stage = 4;
    v->fade = VOICE_FADE_BLOCKS;
}
static VOICE_EVENT void voice_kill(voice_t *v)
{
    uint32_t part = ((uintptr_t)v - (uintptr_t)trk) / sizeof(track_t);
    if (v->stage == 4u)
        return;                                         /* already going: a new fade would jump back up */
    voice_fade(v);
    voice_kills++;
    if (part < NPART) voice_kills_part[part]++;
}

static VOICE_EVENT void voice_capacity_kill(voice_t *v)
{
    capacity_steals++;
    voice_kill(v); /* preserve the legacy explicit-fade counter */
}

/* the fade's gain with k blocks to go, Q15: an S curve, flat at both ends so neither end is a corner */
static int32_t voice_fade_gain(uint32_t k)
{
    return (int32_t)(k * k * (3u * VOICE_FADE_BLOCKS - 2u * k) * 32767u /
                     (VOICE_FADE_BLOCKS * VOICE_FADE_BLOCKS * VOICE_FADE_BLOCKS));
}

/* a note that has to wait for a fade: voice_wait_block plays it. (Full: the last one is replaced.) */
static VOICE_EVENT void voice_wait(track_t *t, uint32_t note, uint32_t vel)
{
    uint32_t i;
    for (i = 0; i < t->wait_n && t->wait_note[i] != note; i++)
        ;
    if (i == t->wait_n && t->wait_n < NVOICE)
        t->wait_n++;
    i = i < t->wait_n ? i : t->wait_n - 1u;
    t->wait_note[i] = (uint8_t)note;
    t->wait_vel[i] = (uint8_t)vel;
}

/* More notes than voices to take: the oldest waiting note gives way now rather than start and be stolen by the next,
 * except the lowest of a part with no bass sounding. The chord that remains is what a steal in place left: bass and last notes */
static VOICE_EVENT void voice_wait_drop(track_t *t)
{
    uint32_t i, low = t->wait_n, drop = t->wait_n;
    if (lowest_held(t) == NVOICE)
        for (i = low = 0; i < t->wait_n; i++)
            if (t->wait_note[i] < t->wait_note[low])
                low = i;
    for (i = 0; i < t->wait_n && drop == t->wait_n; i++)
        if (i != low)
            drop = i;
    if (drop == t->wait_n)
        return;
    for (i = drop; i + 1u < t->wait_n; i++) {
        t->wait_note[i] = t->wait_note[i + 1u];
        t->wait_vel[i] = t->wait_vel[i + 1u];
    }
    t->wait_n--;
}

/* may a voice of part t start without taking one? An extra one (soft) only then; a part's last resort regardless */
static VOICE_EVENT int voice_room(const track_t *t, int soft)
{
    return voices_claimed() + voice_units(t) <= voice_limit(t) || !soft;
}

/* the part's own voice to give up when its slots are full: the oldest released, else the oldest held but the bass, and
 * the bass only when nothing is going for it already. NVOICE = none */
static VOICE_EVENT uint32_t voice_own_victim(const track_t *t, uint32_t ns)
{
    uint32_t i, low = ns, best = ns;
    for (i = 0; i < ns; i++)                             /* the lowest held note is protected */
        if (t->v[i].active && t->v[i].gate && t->v[i].stage != 4u && (low == ns || t->v[i].note < t->v[low].note))
            low = i;
    for (i = 0; i < ns; i++)
        if (t->v[i].active && !t->v[i].gate && t->v[i].stage != 4u && (best == ns || t->v[i].age < t->v[best].age))
            best = i;
    if (best == ns)
        for (i = 0; i < ns; i++)
            if (t->v[i].active && i != low && t->v[i].stage != 4u && (best == ns || t->v[i].age < t->v[best].age))
                best = i;
    if (best == ns)
        best = low < ns && !voices_fading() ? low : NVOICE;
    return best;
}

/* POLY allocation (within the engine's voice cap). DRUM: reuse its lane; other engines: the same note.
 * Else a free voice: ROTATE takes
 * the next one round-robin (release tails ring out), REUSE (or any GLIDE) takes
 * the free voice whose last pitch is closest, so poly portamento moves each
 * voice the shortest way. At the cap or the budget a voice is given up: the oldest released, else the oldest
 * held one, never the lowest held note (the bass), and the note starts beside its fade. Returns 0 when it cannot yet:
 * no slot is free or the CPU has no room, so it waits and the fades hurry. */
static VOICE_EVENT voice_t *voice_alloc(track_t *t, uint32_t note)
{
    const engine_t *e = ENGINES[t->engine];
    uint32_t i, np = trk_nvoice(t), ns = np + e->spare < NVOICE ? np + e->spare : NVOICE, best = ns, nfree = 0, claim = t->wait_n;
    int32_t bd = 0x7FFFFFFF;
    for (i = 0; i < ns; i++) {
        if (t->v[i].active && t->v[i].stage != 4u && t->v[i].note == note) {
            if (!e->rings_on)
                return &t->v[i];
            if (t->v[i].gate) {                          /* struck again: the old one rings out under the new one */
                t->v[i].gate = 0;
                t->v[i].stage = 3;
            }
        }
        nfree += !t->v[i].active;
        claim += t->v[i].active && t->v[i].stage != 4u;
    }
    if (claim >= np || !voice_room(t, 1)) {
        track_t *vp = t;
        uint32_t k = claim >= np ? voice_own_victim(t, ns) : voice_victim(t, 0, &vp);
        if (k < NVOICE) {
            voice_capacity_kill(&vp->v[k]);
        } else if (voices_fading() && (claim >= np || voices_claimed() + voice_units(t) > voice_budget())) {
            voice_wait_drop(t);                          /* nothing left to take: this note or an older one */
            return 0;
        } else if (voices_busy() + voice_units(t) > voice_ceiling()) {
            return 0;                                    /* nothing to take and no fade to wait for, and more than the CPU
                                                          * carries: it waits (a heavy HYDRA voice can be more than a part's
                                                          * half, and LAYER's second part then has none of its own to give) */
        }
    }
    if (!nfree || (voices_fading() && voices_busy() + voice_units(t) > voice_ceiling()))
        return 0;
    if (t->p[P_ALLOC] || t->p[P_GLIDE]) {
        int32_t target=micro_active(t)?clamp(micro_pitch(t,(int32_t)note),0,1270000)*16/10000:(int32_t)note*16;
        for (i = 0; i < ns; i++) {
            int32_t d = t->v[i].pitch_cur - target;
            if (t->v[i].active)
                continue;
            d = d < 0 ? -d : d;
            if (!t->v[i].pitch_cur)
                d = 0x7FFFFFF0;                          /* never played: last choice */
            if (d < bd) {
                bd = d;
                best = i;
            }
        }
    } else {
        for (i = 0; i < np && best == ns; i++) {
            uint32_t k = (t->rr + i) % np;
            if (!t->v[k].active)
                best = k;
        }
        t->rr = (uint8_t)((best == ns ? t->rr : best) + 1u) % np;
        for (i = np; i < ns && best == ns; i++)          /* the spare, while another voice fades */
            if (!t->v[i].active)
                best = i;
    }
    return best < ns ? &t->v[best] : 0;
}

/* glide: RATE = a fixed speed, TIME = the same time for any interval */
static VOICE_EVENT void glide_set(track_t *t, voice_t *v, int glide)
{
    if (!glide || !v->pitch_cur) {
        v->pitch_cur = v->pitch16;
        v->gstep = 0;
        return;
    }
    if (t->p[P_GLMODE]) {
        int32_t d = v->pitch16 - v->pitch_cur, ticks = 1 + t->p[P_GLIDE] * t->p[P_GLIDE] / 8;
        d = d < 0 ? -d : d;
        v->gstep = d / ticks > 0 ? d / ticks : 1;
    } else {
        v->gstep = 0;
    }
}

/* Keep each sounding voice's pitch after SCL/ROOT/QNT changes, including release tails. */
static VOICE_EVENT void voice_scale_pitch(const track_t *t, voice_t *v, uint32_t note)
{
    if (micro_active(t)) {
        int32_t pitch = clamp(micro_pitch(t, (int32_t)note), 0, 1270000);
        v->pitch16 = pitch * 16 / 10000;
        v->scale_fine = (pitch * 16 - v->pitch16 * 10000) * 2367 / 1600000;
    } else {
        v->pitch16 = (int32_t)note * 16;
        v->scale_fine = 0;
    }
}

static VOICE_EVENT void voice_start(track_t *t, voice_t *v, uint32_t note, uint32_t vel, int glide)
{
    AP_START(attack_start);
    const engine_t *e = ENGINES[t->engine];
    int sounding = v->active && v->stage != 0;
    uint32_t ph0 = v->ph[0], ph1 = v->ph[1], ph2 = v->ph[2];
    int32_t s0 = v->s[0], s1 = v->s[1], s4 = v->s[4], s5 = v->s[5];
#if MELODEE_LEGACY_EXTRAS
    int32_t s6 = v->s[6], s7 = v->s[7];
#endif
    uint32_t keep = e->keep;
    voice_was = (uint8_t)(!v->active ? 0u : v->gate ? 2u : 1u);
    v->note = (uint8_t)note;
    v->vel = (uint8_t)vel;
    v->gate = 1;
    v->active = 1;
    v->stage = 1;
    v->fade = 0;
    v->age = ++vage;
    voice_scale_pitch(t, v, note);
    v->mvel = t->m_vel;                                 /* mod.c: the note's VEL and RAND */
    v->mrnd = t->m_rnd;
    t->m_vi = (uint8_t)(v - t->v);
    glide_set(t, v, glide);
    if (!sounding) {
        v->env = 0;
        v->env_out = 0;
        v->side = t->sp_alt; t->sp_alt ^= 1u;
    }                                                   /* sounding: the attack starts from the current level */
    if (!eng_state_prepare(t)) { v->active = v->gate = 0; return; }
    e->note_on(t, v);
    AP_END(AP_ATTACK,attack_start);
    if (sounding && !e->sampled) {                     /* retrigger / steal: keep phases and filter */
        v->ph[0] = ph0;                                 /* states (resetting them clicks) */
        v->ph[1] = ph1;
        v->ph[2] = ph2;
        if (keep & 0x01u)                               /* engine_t.keep: the slots the engines use */
            v->s[0] = s0;
        if (keep & 0x02u)
            v->s[1] = s1;
        if (keep & 0x10u)
            v->s[4] = s4;
        if (keep & 0x20u)
            v->s[5] = s5;
#if MELODEE_LEGACY_EXTRAS
        if (keep & 0x40u)
            v->s[6] = s6;
        if (keep & 0x80u)
            v->s[7] = s7;
#endif
    }
}

/* the voices of a MONO / LEGATO / UNISON note that are not sounding yet need room, and an engine that restarts its
 * envelopes from nothing (engine_t.restarts) cannot retrigger a sounding one in place. 1: the note has to wait for a fade */
static VOICE_EVENT int mono_fade(track_t *t, uint32_t nv, int retrig)
{
    uint32_t i, units = voice_units(t), want = 0;
    int wait = 0;
    for (i = 0; i < nv; i++) {
        voice_t *v = &t->v[i];
        if (v->active) {
            if (v->stage != 4u && ENGINES[t->engine]->restarts && (retrig || !v->gate))
                voice_fade(v);
            wait |= v->stage == 4u;                     /* its own slot, until it has gone */
            continue;
        }
        want += units;
        if (voices_claimed() + want > voice_limit(t)) {
            track_t *vp = 0;
            uint32_t k = voice_victim(t, i > 0u, &vp);
            if (k < NVOICE)
                voice_capacity_kill(&vp->v[k]);
            else if (i)
                want -= units;                              /* no room for this extra UNISON voice */
        }
    }
    return wait || (voices_fading() && voices_busy() + want > voice_ceiling());
}

/* MONO / LEGATO / UNISON: one note on one voice (NPOLY for UNISON, or the
 * engine's voice cap, spread by DETUNE over the same width) */
static VOICE_EVENT void mono_play(track_t *t, uint32_t note, uint32_t vel, int retrig, int glide)
{
    uint32_t mode = (uint32_t)t->p[P_VOICE], nv = mode == V_UNISON ? trk_nvoice(t) : 1u, i;
    if (mono_fade(t, nv, retrig)) {
        voice_wait(t, note, vel);
        return;
    }
    for (i = 0; i < nv; i++) {
        voice_t *v = &t->v[i];
        int32_t k = 2 * (int32_t)i - (int32_t)(nv - 1u);   /* -7 .. 7 */
        if (nv > 1u && nv != NPOLY)
            k = k * (NPOLY - 1) / (int32_t)(nv - 1u);       /* other counts: the outer ones as wide */
        v->fine = nv > 1u ? k * t->p[P_DETUNE] * 56 / 889 : 0;   /* up to ~±40 cents */
        if (retrig || !v->active || !v->gate) {
            if (!v->active && !voice_room(t, i > 0))
                continue;                                   /* no room for this extra UNISON voice */
            /* level: about the same sum for 8 or 4 voices at random phases */
            voice_start(t, v, note, nv >= NPOLY ? vel * 36u / 100u : nv > 1u ? vel / 2u : vel, glide);
            v->side = (uint8_t)(i & 1u);
            if (nv > 1u && i && !ENGINES[t->engine]->sampled) {   /* random start phases: */
                static uint32_t seed = 0x1234567u;          /* in phase they stack, evenly spread they cancel */
                seed = seed * 1664525u + 1013904223u;
                v->ph[0] += seed;
            }
        } else {                                            /* legato: new pitch, same envelope */
            v->note = (uint8_t)note;
            voice_scale_pitch(t, v, note);
            v->mvel = t->m_vel;
            v->mrnd = t->m_rnd;
            if (vel > v->vel && nv == 1u)
                v->vel = (uint8_t)vel;
            glide_set(t, v, glide);
            if (ENGINES[t->engine]->legato)
                ENGINES[t->engine]->legato(t, v);
        }
    }
    t->mono_note = (uint8_t)note;
}

/* the note the mono stack asks for (PRIO: LAST / LOW / HIGH), 0 = none */
static VOICE_EVENT uint32_t mono_pick(const track_t *t)
{
    uint32_t i, n;
    if (!t->nmono)
        return 0;
    n = t->mono_stack[t->nmono - 1u];
    for (i = 0; i < t->nmono; i++) {
        if (t->p[P_PRIO] == 1 && t->mono_stack[i] < n)
            n = t->mono_stack[i];
        if (t->p[P_PRIO] == 2 && t->mono_stack[i] > n)
            n = t->mono_stack[i];
    }
    return n;
}

static VOICE_EVENT void mono_remove(track_t *t, uint32_t note)
{
    uint32_t j, k = 0;
    for (j = 0; j < t->nmono; j++)
        if (t->mono_stack[j] != note)
            t->mono_stack[k++] = t->mono_stack[j];
    t->nmono = (uint8_t)k;
}

static VOICE_EVENT void mono_push(track_t *t, uint32_t note)       /* the latest key on top (full: the oldest goes) */
{
    mono_remove(t, note);
    if (t->nmono >= NVOICE)
        mono_remove(t, t->mono_stack[0]);
    t->mono_stack[t->nmono++] = (uint8_t)note;
}

static int trk_chord(const track_t *t) { (void)t; return 0; }

static VOICE_EVENT void trk_note_on_work(track_t *t, uint32_t note, uint32_t vel)
{
    uint32_t any = 0, i, mode = trk_vmode(t);
    if (t->p[P_MUTE] || (uint32_t)(t - trk) >= trk_n())   /* (a track not in use: silent) */
        return;
    if (t->xf_on || t->eng_req != t->engine) {          /* engine switch under way: after the fade */
        for (i = 0; i < t->xp_n && t->xp_note[i] != note; i++)
            ;
        if (i == t->xp_n && t->xp_n < 4u)
            t->xp_n++;
        i = i < t->xp_n ? i : t->xp_n - 1u;             /* (full: the last one is replaced) */
        t->xp_note[i] = (uint8_t)note;
        t->xp_vel[i] = (uint8_t)vel;
        return;
    }
    mod_note(t, note, vel);                             /* the matrix's VEL / KEY / RAND */
    for (i = 0; i < NVOICE; i++)
        any |= t->v[i].gate && t->v[i].stage != 4u;
    if (!any) {                                        /* fresh phrase: LFO retrigger (TRIG NOTE) and fade */
        if (!t->p[P_LTRIG])
            t->lfo_ph = (uint32_t)t->p[P_LPHASE] << 25;
        t->lfo_fade = 0;
    }
    if (mode == V_POLY) {
        if (ENGINES[t->engine]->poly_key)
            ENGINES[t->engine]->poly_key(t, note);
        AP_START(alloc_start);voice_t *v = voice_alloc(t, note);AP_END(AP_ALLOC,alloc_start);
        if (!v) {
            voice_wait(t, note, vel);
            return;
        }
        if (!trk_chord(t) || !any)
            t->nmono = 0;                               /* no stale mono stack after a mode change */
        if (trk_chord(t))
            mono_push(t, note);                         /* C64: the chord grows (the new note sounds first) */
        t->mono_note = 0;
        v->fine = 0;
        voice_start(t, v, note, vel, t->p[P_GLIDE] != 0 && !ENGINES[t->engine]->oneshot);
        return;
    }
    {
        uint32_t sounding = t->mono_note && t->v[0].active && t->v[0].gate && t->v[0].stage != 4u, want;
        mono_push(t, note);
        if (ENGINES[t->engine]->mono_key)
            ENGINES[t->engine]->mono_key(t, note);
        want = mono_pick(t);
        if (sounding && want == t->mono_note)
            return;                                     /* LOW / HIGH priority: this key does not win */
        if (mode == V_MONO)                             /* MONO: always retrigger, glide when GLIDE is set */
            mono_play(t, want, vel, 1, t->p[P_GLIDE] != 0);
        else                                            /* LEGATO / UNISON: glide only between held notes */
            mono_play(t, want, vel, !sounding, sounding && (t->p[P_GLIDE] != 0));
    }
}

static VOICE_EVENT void trk_note_on_engine(track_t *t,uint32_t note,uint32_t vel)
{
#if MELODEE_ADMISSION
    const track_t *previous=voice_admitting;uint8_t previous_parts=voice_admitting_p5parts;
    voice_admitting=0;uint32_t parts=voice_p5_parts();
    if(t->engine==ENGI_PROPHET){uint32_t live=0;for(uint32_t i=0;i<NVOICE;i++)live|=t->v[i].active && t->v[i].stage!=4u;parts+=!live;}
    voice_admitting=t;voice_admitting_p5parts=(uint8_t)parts;
    trk_note_on_work(t,note,vel);voice_admitting=previous;voice_admitting_p5parts=previous_parts;
#else
    trk_note_on_work(t,note,vel);
#endif
}

static VOICE_EVENT void trk_note_off_engine(track_t *t, uint32_t note)
{
    uint32_t i, k = 0, mode = trk_vmode(t), chord = trk_chord(t), held = 0;
    for (i = 0; i < t->xp_n; i++)                       /* not sounding yet (engine switch): forget it */
        if (t->xp_note[i] != note) {
            t->xp_note[k] = t->xp_note[i];
            t->xp_vel[k++] = t->xp_vel[i];
        }
    t->xp_n = (uint8_t)k;
    for (i = k = 0; i < t->wait_n; i++)                 /* still waiting for a fade: forget it */
        if (t->wait_note[i] != note) {
            t->wait_note[k] = t->wait_note[i];
            t->wait_vel[k++] = t->wait_vel[i];
        }
    t->wait_n = (uint8_t)k;
    for (i = 0; i < t->nmono; i++)
        held |= t->mono_stack[i] == note;
    mono_remove(t, note);                              /* a mode change must not retain a released key */
    if (mode != V_POLY) {
        uint32_t nv = mode == V_UNISON ? trk_nvoice(t) : 1u;
        if (t->mono_note == note) {
            uint32_t next = mono_pick(t);
            if (next) {                                 /* fall back to a held note, legato */
                mono_play(t, next, t->v[0].vel, 0, t->p[P_GLIDE] != 0);
            } else {
                for (i = 0; i < nv; i++)
                    if (t->v[i].gate && t->v[i].stage != 4u) {
                        t->v[i].gate = 0;
                        t->v[i].stage = 3;
                    }
                t->mono_note = 0;
            }
        }
    }
    /* a fallback above already has its new pitch; a C64 chord (v[0]) ends with its last key, whichever note sounds */
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].gate && t->v[i].stage != 4u && (chord && !i ? !t->nmono && (held || t->v[0].note == note) : t->v[i].note == note)) {
            t->v[i].gate = 0;
            t->v[i].stage = 3;
        }
}

/* A note to the track's engine: none from a muted track or one not in use; velocity 0 is a note off. */
static VOICE_EVENT void trk_note_off(track_t *t, uint32_t note)
{ if (note <= 127u) trk_note_off_engine(t, note); }
static VOICE_EVENT void trk_note_on(track_t *t, uint32_t note, uint32_t vel)
{
    if (!vel) { trk_note_off(t, note); return; }
    if (note > 127u || t->p[P_MUTE] || (uint32_t)(t - trk) >= trk_n()) return;
    trk_note_on_engine(t, note, vel);
}

static VOICE_EVENT void trk_all_off(track_t *t)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++) {
        t->v[i].gate = 0;
        if (t->v[i].stage != 4u || !t->v[i].active)     /* (a fade goes on) */
            t->v[i].stage = t->v[i].active ? 3 : 0;
    }
    t->nmono = 0;
    t->mono_note = 0;
    t->xp_n = 0;
    t->wait_n = 0;
}

/* at each block start (events_block), before the parts: a voice whose fade reached 0 in the last block is free. It stays
 * active through that block so the engine renders it to its last sample, and only here, with the block's CPU1 work over */
static VOICE_EVENT void voice_fades_end(void)
{
    uint32_t p, i;
    for (p = 0; p < NPART; p++)
        for (i = 0; i < NVOICE; i++) {
            voice_t *v = &trk[p].v[i];
            if (v->active && v->stage == 4u && !v->fade) {
                v->env = v->env_out = 0;
                v->stage = 0;
                v->active = v->gate = 0;
            }
        }
}

/* at each block start (events_block), before any note of the block: the notes whose voices have faded start. A note
 * with no fade left to wait for tries again, and may take another voice */
static VOICE_EVENT int voice_fits(const track_t *t)
{
    const engine_t *e = ENGINES[t->engine];
    uint32_t i, np = trk_nvoice(t), ns = np + e->spare < NVOICE ? np + e->spare : NVOICE, nfree = 0;
    if (voices_busy() + voice_units(t) > voice_ceiling())
        return 0;
    for (i = 0; i < ns; i++) {
        nfree += !t->v[i].active;
        if (trk_vmode(t) != V_POLY && t->v[i].active && t->v[i].stage == 4u)
            return 0;
    }
    return nfree != 0u;
}
static VOICE_EVENT void voice_wait_block(track_t *t)
{
    uint8_t note[NVOICE], vel[NVOICE];
    uint32_t i, n = t->wait_n;
    if (!n)
        return;
    for (i = 0; i < n; i++) {
        note[i] = t->wait_note[i];
        vel[i] = t->wait_vel[i];
    }
    t->wait_n = 0;
    for (i = 0; i < n; i++)
        if (voices_fading() && !voice_fits(t))
            voice_wait(t, note[i], vel[i]);
        else
            trk_note_on(t, note[i], vel[i]);
}

/* engine switch, at each block start (events_block), before any note of the block. The UI writes
 * eng_req and the new engine's P_E0..P_E7 together (IRQ off). The part's sounding voices (released by
 * the preset change) then fade out over XF_BLOCKS blocks on the old engine, rendered with its own
 * parameters (pe_old: an engine never reads another engine's values, which index its tables); only
 * then the engine switches and the notes that came during the fade start on it. A part with nothing
 * sounding switches at once. */
#define XF_BLOCKS 4u                                    /* 4 x 32 samples: 2.9 ms */
static void engine_block(track_t *t)
{
    uint32_t i, any = 0;
    if (t->xf_on || t->eng_req != t->engine) {
        if (!t->xf_on) {
            for (i = 0; i < NVOICE; i++)
                any |= t->v[i].active;
            if (any) {
                t->xf_on = 1;
                t->xf = XF_BLOCKS;
                return;
            }
        } else if (t->xf) {
            return;                                     /* still fading (track_render counts down) */
        }
        for (i = 0; i < NVOICE; i++) {                  /* faded to 0: gone */
            voice_t *v = &t->v[i];
            v->active = v->gate = 0;
            v->stage = 0;
            v->env = v->env_out = 0;
        }
        if (t->engine != eng_idx(t->eng_req))           /* another engine: its state as at power-on */
            eng_state_clear((uint32_t)(t - trk));
        t->engine = eng_idx(t->eng_req);
        t->xf_on = 0;
        t->nmono = 0;
        t->mono_note = 0;
        {
            uint32_t n = t->xp_n;
            t->xp_n = 0;
            for (i = 0; i < n; i++)
                trk_note_on(t, t->xp_note[i], t->xp_vel[i]);
        }
    }
    for (i = 0; i < 8u; i++)                            /* the engine's own values, for a later fade */
        t->pe_old[i] = t->p[P_E0 + i];
}

/* one control tick (CTL samples) of the amplitude envelope; returns Q15. own: the engine's own envelopes are the
 * voice's amplitude (ownenv), this is its MOD ENV alone: its release rests at 0 and the engine's done() ends the voice */
static int32_t env_tick(track_t *t, voice_t *v, int own)
{
    const int16_t *p = t->p;
    int32_t sus = (int32_t)p[P_SUS] << 17;              /* Q24 */
    switch (v->stage) {
    case 1:
        v->env += (int32_t)ENV_LIN[p[P_ATK] & 127];
        if (v->env >= (1 << 24)) {
            v->env = 1 << 24;
            v->stage = 2;
        }
        break;
    case 2:
        v->env += mulq16(sus - v->env, ENV_EXP[p[P_DEC] & 127]);
        break;
    case 3:
        v->env -= mulq16(v->env, ENV_EXP[p[P_REL] & 127]);
        if (v->env < (1 << 12)) {
            v->env = 0;
            if (!own) {
                v->stage = 0;
                v->active = 0;
            }
        }
        break;
    case 4:                                             /* given up: track_render_audio takes it down the fade */
        v->env -= mulq16(v->env, ENV_EXP[p[P_REL] & 127]);
        if (v->fade) {                                  /* the last block's gain is 0: voice_fades_end frees it after */
            uint32_t step = voices_waiting() ? VOICE_FADE_HURRY : 1u;
            v->fade = (uint8_t)(v->fade > step ? v->fade - step : 0u);
            break;
        }
        v->env = 0;                                     /* no fade asked for (FM6's panic), or no block start since */
        v->stage = 0;
        v->active = v->gate = 0;
        break;
    default:
        v->env = 0;
        break;
    }
    return v->env >> 9;
}

/* SPREAD (#148, mix.c mix_spread): while spread_on (mix_part sets it for the part it renders), track_render renders the
 * voices of side 0 into spread_buf and the others into out, then adds spread_buf to out: out is every voice, as always
 * (the engines add into either the same way), spread_buf the left ones. A cost per block, none per voice */
static uint8_t spread_on;
static int32_t spread_storage[CTL];
static int32_t *spread_buf=spread_storage;
static __attribute__((noinline)) void spread_clear(void)
{
    uint32_t i;
    for (i = 0; i < CTL; i++)
        spread_buf[i] = 0;
}
static __attribute__((noinline)) void spread_sum(int32_t *out)
{
    uint32_t i;
    for (i = 0; i < CTL; i++)
        out[i] += spread_buf[i];
}

/* render one block of a part into out (cleared here); returns the voices rendered */
/* MIDI pitch bend per track, Q8 semitones: the target seq.c's MIDI input (midi_input.c) and the Prophet's block set,
 * and the smoothed value the voices play (track_render_audio moves it to the target, a step each block) */
static int32_t midi_bend_target[NTRK], midi_bend_q8[NTRK];
#ifdef SAADET_SMK37
static volatile int16_t sd_vib_q8[NPART];           /* the mod wheel on an engine with no wheel of its own (instrument.c) */
#endif
/* a 14-bit bend (-8192..8191) at range (Q8 semitones) -> Q8 semitones, rounded. Each side is scaled by its own full
 * scale: the centre is 0, the ends exactly +-range */
static int32_t midi_bend_scale(int32_t raw, int32_t range)
{
    return raw < 0 ? (raw * range - 4096) / 8192 : (raw * range + 4095) / 8191;
}
/* the Prophet's bend target (p5_block, every block): its patch's own range, BEND 0..11 = 1..12 semitones */
static void p5_update_bend(track_t *t)
{
    uint32_t ti = (uint32_t)(t - trk);
    if (MELODEE_CONTROL_FAST && !t->bend_raw) {         /* the wheel at rest: no patch to read */
        midi_bend_target[ti] = 0;
        return;
    }
    midi_bend_target[ti] = midi_bend_scale(t->bend_raw, (clamp(p5_patch_of(t)->raw[P5_BEND], 0, 11) + 1) * 256);
}
static struct { int16_t cents; uint8_t valid; int32_t pitch, fine; } tune_cache;
static __attribute__((noinline)) void tune_cache_update(void)
{
    tune_cache.cents=song.g[G_TUNE];tune_cache.valid=1;
    /* TUNE in cents: whole 1/16 semitones plus the exact residual. */
    tune_cache.pitch=song.g[G_TUNE]>=0?song.g[G_TUNE]*16/100:-((-song.g[G_TUNE]*16+99)/100);
    tune_cache.fine=(song.g[G_TUNE]*16-tune_cache.pitch*100)*2367/16000;
}
typedef struct {
    track_t *track;voice_t *voice;const engine_t *engine;vmod_t modulation;uint32_t n;
    int16_t engine_params[8];
} engine_batch_t;
_Static_assert(sizeof(engine_batch_t)<=AUDIO_BATCH_BYTES,"Engine batch descriptor");
static void engine_batch_kernel(const void *data,int32_t *pcm)
{
    const engine_batch_t *j=data;
    j->engine->render(j->track,j->voice,pcm,j->n,&j->modulation);
}

static inline void engine_join(void) {}
static void engine_render(track_t *t,voice_t *v,const engine_t *e,int32_t *out,uint32_t n,const vmod_t *m)
{
    if(audio_batch_active && (e==&ENG_CZ || e==&ENG_J106)){
        engine_batch_t j;j.track=t;j.voice=v;j.engine=e;j.modulation=*m;j.n=n;
        for(uint32_t k=0;k<8u;k++)j.engine_params[k]=t->p[P_E0+k];
        audio_batch_push(engine_batch_kernel,&j,sizeof j,out,n,e==&ENG_CZ?AP_CZ:AP_J106);return;
    }
    e->render(t,v,out,n,m);
}

/* Keep the per-voice renderer separate from the bus/ISR loops. This bounds
 * the ISR's code footprint and permits an independent target cost check. */
static __attribute__((noinline)) uint32_t track_render_audio(track_t *t, int32_t *out, uint32_t n, int clear_idle)
{
    const engine_t *e = ENGINES[t->engine];
    const int16_t *p = t->p;
    uint32_t i;
    int32_t lfo = mulq15(t->lfo_val, t->lfo_fade);
    int32_t tune, tune_fine, bend, bend_fine;
    uint32_t ti = (uint32_t)(t - trk);
    /* the MIDI bend, every block (a part without voices too): a quarter of the way to its target (~2.5 ms), at least
     * one unit, so it lands exactly (the whole +-24 semitones in ~20 ms) */
    int32_t bd = midi_bend_target[ti] - midi_bend_q8[ti];
    midi_bend_q8[ti] += bd / 4 + (bd > 0) - (bd < 0);
    uint32_t nr = 0, fade = t->xf_on && t->xf;
    uint32_t live=0;
    for (i=0;i<NVOICE;i++) if(t->v[i].active){live=1;break;}
    int16_t pe_new[8];
    if(live || clear_idle)for (i = 0; i < n; i++)out[i] = 0;
    if (mod.on && mod.ldep < 32767) lfo = mulq15(lfo, mod.ldep);
    if (spread_on) spread_clear();
    if ((live || e->block || e->post) && !eng_state_prepare(t)) return 0;
    if (fade)                                           /* engine switch: the old engine, its own values */
        for (i = 0; i < 8u; i++) {
            pe_new[i] = t->p[P_E0 + i];
            t->p[P_E0 + i] = t->pe_old[i];
        }
    AP_START(control_start);
    track_lfo_tick(t);
    if (e->block)                                       /* the engine's per-part work (WHEEL: bars, rotor) */
        e->block(t);
    AP_END(AP_TRACK_CONTROL,control_start);
    if(!live){
        /* FM6's block keeps ghost envelopes running; its post resets the DC
         * filter. LFO, bend and switch fades also continue on silent tracks. */
        if(e->post)e->post(t,out,n,0);
        goto track_render_done;
    }
    if(!tune_cache.valid || tune_cache.cents!=song.g[G_TUNE])tune_cache_update();
    tune=tune_cache.pitch;tune_fine=tune_cache.fine;
    /* the bend as TUNE: whole 1/16 semitones (rounded down, so a negative bend's rest is positive too) and the rest,
     * 0..15 Q8 units of 100/256 cent, in 1/4096 (2.367 a cent) */
#ifdef SAADET_SMK37
    {int32_t bend_q8=midi_bend_q8[ti]+sd_vib_q8[ti];bend=bend_q8>>4;bend_fine=((bend_q8&15)*2367+1280)/2560;}
#else
    bend=midi_bend_q8[ti]>>4;bend_fine=((midi_bend_q8[ti]&15)*2367+1280)/2560;
#endif
    for (i = 0; i < NVOICE; i++) {
        voice_t *v = &t->v[i];
        vmod_t m;
        int32_t env, adsr, pitch;
        if (!v->active)
            continue;
        {
            AP_START(env_start);
            env = adsr = env_tick(t, v, e->ownenv);
            if (e->ownenv && v->active) {               /* the engine's envelopes (FM6): they end the voice */
                if (e->done(t, v)) {                    /* its last block, faded out (the engine's state goes on */
                    v->active = v->gate = 0;            /* one more block, as Dexed's) */
                    v->stage = 0;
                    v->env = 0;
                    env = adsr = 0;
                } else
                    env = 32767;                        /* (the ADSR goes on as the MOD ENV) */
            }
            if (e->amp)                                 /* the engine's own amplitude curve */
                env = e->amp(t, v, env);
            m.envq15 = e->ownenv ? adsr : env;
            m.amp1 = e->ownenv ? env : mulq15(env, v->vel * 258);
            if (p[P_LD_AMP])
                m.amp1 = mulq15(m.amp1, 32767 - mulq15(p[P_LPOL] ? lfo : (lfo + 32768) >> 1, p[P_LD_AMP] * 258));
            if (v->stage == 4u)                         /* given up: down the S curve */
                m.amp1 = mulq15(m.amp1, voice_fade_gain(v->fade));
            if (fade)                                   /* linear to 0 over the fade */
                m.amp1 = m.amp1 * (int32_t)(t->xf - 1u) / (int32_t)XF_BLOCKS;
            m.amp0 = v->env_out;
            v->env_out = m.amp1;
            AP_END(AP_ENVELOPE,env_start);
        }
        if (v->pitch_cur != v->pitch16) {               /* glide */
            int32_t d = v->pitch16 - v->pitch_cur;
            int32_t st = v->gstep ? v->gstep : (int32_t)(ENV_LIN[(p[P_GLIDE] ? p[P_GLIDE] : 40) & 127] >> 14);   /* slide: ~45 ms */
            if (st < 1)
                st = 1;
            v->pitch_cur += d > 0 ? (d < st ? d : st) : (-d < st ? d : -st);
        }
        if (!env && !m.amp0 && v->stage == 2 && !e->sampled)
            continue;                                   /* held at a silent sustain (SUS 0): nothing to render */
        pitch = v->pitch_cur + tune + bend + ((lfo * p[P_LD_PIT] * 3) >> 15) + ((m.envq15 * p[P_ED_PIT] * 3) >> 15);
        m.pitch16 = clamp(pitch, 0, 2047);
        m.inc = tuned_pitch_inc(m.pitch16);
        m.fine = v->fine + v->scale_fine + tune_fine + bend_fine;
        if (m.fine)             /* residual below 1/16 semitone */
            m.inc += (uint32_t)((int32_t)(m.inc >> 12) * m.fine);
        m.cutoff = ((lfo * p[P_LD_FLT]) >> 7) + ((m.envq15 * p[P_ED_FLT]) >> 7);
        if (v->vel > 110 && !e->ownenv)                 /* accent opens the filter with the env (an engine with its
                                                         * own envelopes has its own velocity) */
            m.cutoff += (m.envq15 * 24) >> 7;
        m.shape = (64 << 8) + ((lfo * p[P_LD_SHP]) >> 7) + ((m.envq15 * p[P_ED_SHP]) >> 7);
        if (e == &ENG_CZ)                                /* cached native DCA, Q15 */
            m.envq15 = v->s[4];
        if(e==&ENG_P5_TEST)m.envq15=p5_env_source(t,v);
        if(e==&ENG_J106)m.envq15=j106_env_source(t,v);
        if (mod.on){
            AP_START(mod_start);mod_voice(t,v,&m,m.fine);AP_END(AP_MODULATION,mod_start);
        }
        /* 1/16 st and 1/4096 -> Q24 octaves: the voice's own offset (the matrix's pitch too), without TUNE and bend */
        m.plog = (m.pitch16 - tune - bend - (int32_t)v->note * 16) * 87381 + (m.fine - tune_fine - bend_fine) * 5909;
        engine_render(t,v,e,spread_on>v->side?spread_buf:out,n,&m);
        nr++;
    }
    engine_join();
    if (spread_on) fm6_join();
    if (spread_on && !audio_batch_active) spread_sum(out);
    if (e->post && !audio_batch_active)
        e->post(t, out, n, nr);
track_render_done:
    if (fade) {
        for (i = 0; i < 8u; i++)
            t->p[P_E0 + i] = pe_new[i];
        t->xf--;
    }
    return nr;
}
/* Direct callers receive a cleared buffer even for silence. The mixer can
 * omit that write when neither a DIST tail nor a stutter needs the zeros. */
static uint32_t track_render(track_t *t, int32_t *out, uint32_t n)
{
    return track_render_audio(t,out,n,1);
}
