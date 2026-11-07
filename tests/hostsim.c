/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Host-side render of the MELODEE DSP (engines, voices, FX, sequencer) to a WAV,
 * for debugging sound without hardware. Same sources as the synth firmware.
 *   tests/run_tests.sh builds it into build/host/;
 *   build/host/hostsim ENGINE PRESET MONO OUT.wav [CHORUS]
 * env: SECS=n renders n s (the 2 s note pattern repeats), CHORD[=k] holds k keys
 * (default 4, up to 8), BENCH=1 prints the render time,
 * DIST=d, LEVEL=l, SENDS=c,d,r, SWEEP=1, NOTE=k, OCT=o, PSET=id:v,...,
 * PRESET=1 (the whole preset: sends, voice mode).
 */
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define __attribute__(x)
#define memset melodee_memset
#define memcpy melodee_memcpy
#define memcmp melodee_memcmp
#include "melodee_tables.h"
#include "../firmware/src/libc.c"
#undef memset
#undef memcpy
#undef memcmp
#undef __attribute__                                  /* the RAM tables the firmware fills at boot (fm1_main) */
__attribute__((constructor)) static void host_dsp_tables(void) { dsp_tables_init(); }
#define __attribute__(x)
static struct { volatile uint32_t notes, buttons; } fm1_in;
#include "../firmware/src/core.h"
#include "../firmware/src/engines.c"
/* the packed factory banks, unpacked for comparisons (four slots each, so a few can be held at once) */
static const p5_patch_t *p5_fac(uint32_t k)
{
    static p5_patch_t s[4];
    static uint32_t n;
    p5_patch_t *p = &s[n++ % 4u];
    p5_factory(k, p);
    return p;
}
static const uint8_t *cz_fac(uint32_t k)
{
    static uint8_t s[4][CZ_BYTES];
    static uint32_t n;
    uint8_t *p = s[n++ % 4u];
    cz_factory(k, p);
    return p;
}
#include "../firmware/src/params.c"
#include "../firmware/src/mod.c"
#include "../firmware/src/voice.c"
#include "../firmware/src/mix.c"
static void fm1_delay_ms(uint32_t ms) { (void)ms; }
#include "../firmware/src/usb.c"
#include "../firmware/src/midi_uart.c"                /* TRS MIDI IN: its parser (um_byte) feeds midi_in_q */
#include "../firmware/src/seq.c"
#define inst (trk[0])                   /* the single-part renders below: part 1 */

static void wav_hdr(FILE *f, uint32_t frames)
{
    uint32_t v;
    fwrite("RIFF", 1, 4, f); v = 36 + frames * 4; fwrite(&v, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f); v = 16; fwrite(&v, 4, 1, f);
    uint16_t a = 1, ch = 2, ba = 4, bits = 16; uint32_t sr = FS, br = FS * 4;
    fwrite(&a, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&sr, 4, 1, f); fwrite(&br, 4, 1, f);
    fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); v = frames * 4; fwrite(&v, 4, 1, f);
}

static void wav_put(FILE *f, int32_t l, int32_t r)
{
    int16_t s[2] = {(int16_t)(l > 32767 ? 32767 : l < -32768 ? -32768 : l),
                    (int16_t)(r > 32767 ? 32767 : r < -32768 ? -32768 : r)};
    fwrite(s, 2, 2, f);
}

/* ---------------------------------------------------------------- TRACKS --- */
static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec;
}

static void host_tracks_init(void)                /* as melodee_init: defaults, empty patterns */
{
    cz_init();
    uint32_t i, k;
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    for (k = 0; k < NTRK; k++) {
        for (i = 0; i < P_E0; i++)
            trk[k].p[i] = TP[i].def;
    }
    song.master_q12 = 4096;
}

/* the factory preset as ui.c apply_preset_to sets it (sound, sends; not the steps, not the track's own:
 * the mix, ARP, SCL, the pattern parameters, the SLICER);
 * the engine as the UI asks for it (eng_req: the audio side switches; host_preset: at once) */
static void host_preset_values(track_t *t, uint32_t e, uint32_t pi, const preset_t *p)
{
    uint32_t i;
    t->eng_req = (uint8_t)e;
    t->preset = (uint8_t)pi;
    for (i = 0; i < 8u; i++)
        t->p[P_E0 + i] = p->e[i];
    t->p[P_ATK] = p->env[0];
    t->p[P_DEC] = p->env[1];
    t->p[P_SUS] = p->env[2];
    t->p[P_REL] = p->env[3];
    t->p[P_ED_FLT] = p->fenv;
    t->p[P_VOICE] = p->mono ? V_LEGATO : V_POLY;
    cz_factory_loaded(t);
    if(e==ENGI_PROPHET){uint32_t k=(uint32_t)(t-trk)%NTRK;p5_ready[k]=1;p5_preset_loaded(t,pi);}   /* (its program, as ui.c) */
    if(e==ENGI_J106)j106_preset_loaded(t,pi);
    if(e==ENGI_HYDRA)hy_preset_loaded(t,pi);
#if SAADET_PPG
    if(e==ENGI_PPG)ppg_preset_loaded(t,pi);
#endif
    fm6_track_loaded(t);                          /* FM6: the preset's patch (ui.c apply_preset_to) */
}
static void host_preset_req(track_t *t, uint32_t e, uint32_t pi)
{
    uint32_t n = ENGINES[e]->npresets;
    if (!n) {
        t->eng_req = (uint8_t)e;
        t->preset = 0;
        for (uint32_t i = 0; i < 8u; i++)
            t->p[P_E0 + i] = ENGINES[e]->edit[i].def;
        return;
    }
    pi %= n;
    host_preset_values(t, e, pi, eng_preset(ENGINES[e], pi));
}
static void host_preset(track_t *t, uint32_t e, uint32_t pi)
{
    host_preset_req(t, e, pi);
    if (t->engine != t->eng_req && t >= &trk[0] && t < &trk[NPART])
        eng_state_clear((uint32_t)(t - trk));     /* as voice.c engine_block on a switch */
    t->engine = t->eng_req;
}

/* part k's power-on sound (main.c, TRK_DEF) */
static void host_power_on_sound(track_t *t, uint32_t k) { host_preset(t, TRK_DEF[k][0], TRK_DEF[k][1]); }

/* the plain synth a test plays when the engine is not what it tests (ANALOG's place until 2.0 retired it): CZ-1,
 * on the generic voice path (NPOLY voices, no cap of its own) as ANALOG was */
#define HOST_SYNTH ENGI_CZ

static uint32_t busy_now(void)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < NVOICE; i++) n += !!trk[0].v[i].active;
    return n;
}

int main(int argc, char **argv)
{
    int eng = argc > 1 ? atoi(argv[1]) : (int)HOST_SYNTH, preset = argc > 2 ? atoi(argv[2]) : 0;
    int mono = argc > 3 ? atoi(argv[3]) : 1;
    const char *out = argc > 4 ? argv[4] : "out.wav";
    uint32_t i, frames = FS * (getenv("SWEEP") ? 4 : 2), f, nk = 0;
    clock_t c0;
    FILE *w = fopen(out, "wb");
    if (getenv("SECS"))
        frames = FS * (uint32_t)atoi(getenv("SECS"));
    if (getenv("CHORD"))
        nk = atoi(getenv("CHORD")) > 4 ? (uint32_t)atoi(getenv("CHORD")) : 4u;
    for (i = 0; i < G_COUNT; i++) song.g[i] = GP[i].def;
    for (i = 0; i < P_E0; i++) inst.p[i] = TP[i].def;
    inst.eng_req = inst.engine = (uint8_t)eng;
    for (i = 0; i < 8; i++) inst.p[P_E0 + i] = ENGINES[eng]->edit[i].def;
    if (ENGINES[eng]->npresets) {
        const preset_t *p = eng_preset(ENGINES[eng], preset % ENGINES[eng]->npresets);
        for (i = 0; i < 8; i++) inst.p[P_E0 + i] = p->e[i];
        inst.p[P_ATK] = p->env[0]; inst.p[P_DEC] = p->env[1];
        inst.p[P_SUS] = p->env[2]; inst.p[P_REL] = p->env[3]; inst.p[P_ED_FLT] = p->fenv;
    }
    fm6_track_loaded(&inst);                            /* FM6: the preset's patch */
    inst.p[P_VOICE] = (int16_t)mono;
    if (getenv("PRESET"))                               /* PRESET=1: sends, voice mode too (then SENDS, PSET) */
        host_preset(&inst, (uint32_t)eng, (uint32_t)preset);
    if (getenv("OCT"))                                   /* OCT=o: keyboard octave shift (key 7 = C4 + 12 o) */
        song.octave = (int8_t)atoi(getenv("OCT"));
    if (getenv("PSET")) {                               /* PSET=id:value,... (track parameter ids, P_E0 = 61) */
        const char *s = getenv("PSET");
        int id, val, k;
        while (sscanf(s, "%d:%d%n", &id, &val, &k) == 2) {
            if (id >= 0 && id < P_COUNT)
                inst.p[id] = (int16_t)val;
            s += k;
            if (*s == ',')
                s++;
        }
    }
    song.master_q12 = 4096;
    wav_hdr(w, frames);
    c0 = clock();
    for (f = 0; f < frames; f += CTL) {
        int32_t o[2 * CTL];
        uint32_t fp = f % (2u * FS);    /* position in the repeating 2 s pattern */
        {   /* C4 0.1-0.5 s, E4 0.45-0.8 s (overlap: legato/steal), G4 0.9-1.2 s */
            uint32_t n = 0, f = fp;
            if (f > FS / 10 && f < FS / 2) n |= 1u << 7;
            if (f > FS * 45 / 100 && f < FS * 8 / 10) n |= 1u << 11;
            if (f > FS * 9 / 10 && f < FS * 12 / 10) n |= 1u << 14;
            fm1_in.notes = getenv("STEPS") ? 0u : n;   /* STEPS: only the sequencer plays */
        }
        if (nk) {                       /* keys held 0.1-1.5 s (C E G B, then D F A C) */
            static const uint8_t K[8] = {7, 11, 14, 18, 9, 12, 16, 19};
            uint32_t n = 0, k;
            for (k = 0; k < nk && k < 8u; k++)
                n |= 1u << K[k];
            fm1_in.notes = (fp > FS / 10 && fp < FS * 3 / 2) ? n : 0;
        }
        mix_block(o, CTL);
        {   /* VOICES=1: report the most voices active at once (engine voice caps) */
            static uint32_t vmax, hi;
            uint32_t k, a = 0;
            for (k = 0; k < NVOICE; k++)
                if (inst.v[k].active) {
                    a++;
                    hi |= 1u << k;
                }
            if (a > vmax)
                vmax = a;
            if (getenv("VOICES") && f + CTL >= frames)
                fprintf(stderr, "voices: at most %u active, slots used 0x%02X\n", vmax, hi);
        }
        for (i = 0; i < 2 * CTL; i++) {
            int16_t s = (int16_t)(o[i] > 32767 ? 32767 : o[i] < -32768 ? -32768 : o[i]);
            fwrite(&s, 2, 1, w);
        }
    }
    if (getenv("BENCH"))
        fprintf(stderr, "%.1f s audio in %.1f ms (%.2f %% of real time)\n", (double)frames / FS,
                (double)(clock() - c0) * 1000.0 / CLOCKS_PER_SEC,
                (double)(clock() - c0) * 100.0 / CLOCKS_PER_SEC / ((double)frames / FS));
    fclose(w);
    return 0;
}
