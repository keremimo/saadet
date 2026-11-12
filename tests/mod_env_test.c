/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* The common MOD ENV on the engines with their own envelopes (ownenv: P5, JUNO-106, FM6, CZ-1, PPG): ENV > AMOUNTS
 * FILTER moves the engine's filter (its brightness), its SUSTAIN holds it there, the ADSR never touches the voice's
 * amplitude, and its RELEASE never ends a voice the engine's own release still sounds. */
#include "ppg_rom.h"
#if PPG_HAVE_ROM
#define SAADET_PPG 1
#endif
#include <assert.h>
#include "synth_harness.c"
static int failures;
static void ok(const char *eng, const char *what, int pass, double a, double b)
{
    printf("mod env: %s: %s (%.4g, %.4g): %s\n", eng, what, a, b, pass ? "ok" : "FAIL");
    failures += !pass;
}
/* the engine's preset with the common ADSR and its FILTER amount, one key down */
static void play(uint32_t eng, uint32_t pi, int flt, int atk, int dec, int sus, int rel)
{
    power_on();
    rng_state = 0x1234567u;
    host_preset(TSEL, eng, pi);
    TSEL->p[P_VOICE] = V_POLY;
    TSEL->p[P_ATK] = (int16_t)atk; TSEL->p[P_DEC] = (int16_t)dec;
    TSEL->p[P_SUS] = (int16_t)sus; TSEL->p[P_REL] = (int16_t)rel;
    TSEL->p[P_ED_FLT] = (int16_t)flt;
    trk_note_on(TSEL, 48, 100);
}
/* `blocks` blocks: their energy, and how bright they are (the first difference's energy over the signal's) */
static double bright, level;
static void listen(uint32_t blocks)
{
    int32_t out[CTL], prev = 0;
    double e = 0, d = 0;
    for (uint32_t b = 0; b < blocks; b++) {
        track_render(TSEL, out, CTL);
        for (uint32_t i = 0; i < CTL; i++) {
            double x = out[i], dx = (double)out[i] - prev;
            e += x * x; d += dx * dx; prev = out[i];
        }
    }
    level = e;
    bright = e > 0 ? d / e : 0;
}
static uint32_t live(void) { uint32_t n = 0; for (uint32_t i = 0; i < NVOICE; i++) n += TSEL->v[i].active; return n; }
/* blocks from the key's release until the engine ends its voice */
static uint32_t tail(uint32_t eng, uint32_t pi, int rel)
{
    int32_t out[CTL];
    uint32_t b;
    play(eng, pi, 0, 0, 0, 127, rel);
    for (b = 0; b < 200u; b++) track_render(TSEL, out, CTL);
    trk_note_off(TSEL, 48);
    for (b = 0; b < 40000u && live(); b++) track_render(TSEL, out, CTL);
    return b;
}
static void engine(const char *name, uint32_t eng, uint32_t pi)
{
    double up, down, held, gone, l0, l1;
    assert(ENGINES[eng]->ownenv);
    /* AMOUNTS FILTER: + opens the filter, - closes it, with the ADSR at its sustain */
    play(eng, pi, 63, 0, 0, 127, 60); listen(300); up = bright;
    play(eng, pi, -64, 0, 0, 127, 60); listen(300); down = bright;
    ok(name, "FILTER +63 brighter than -64", up > down * 1.15, up, down);
    /* it is an envelope: with FILTER -64, SUSTAIN 127 holds the filter closed, SUSTAIN 0 lets it back open after a
     * short DECAY (closing: a preset's filter may already be open all the way) */
    play(eng, pi, -64, 0, 20, 127, 60); listen(30); listen(150); held = bright;
    play(eng, pi, -64, 0, 20, 0, 60); listen(30); listen(150); gone = bright;
    ok(name, "SUSTAIN 0 brighter than 127 once decayed", gone > held * 1.15, gone, held);
    /* the ADSR is not the amplitude: no FILTER amount, a slow attack sounds as an instant one */
    play(eng, pi, 0, 127, 0, 0, 60); listen(300); l0 = level;
    play(eng, pi, 0, 0, 0, 127, 60); listen(300); l1 = level;
    ok(name, "ADSR leaves the level alone", l1 > 0 && l0 > l1 * 0.9 && l0 < l1 * 1.1, l0, l1);
    /* its RELEASE ends nothing: the engine's own release does */
    uint32_t t0 = tail(eng, pi, 0), t1 = tail(eng, pi, 127);
    ok(name, "RELEASE 0 keeps the engine's tail", t0 + 2u >= t1 && t1 + 2u >= t0 && t0 > 1u, t0, t1);
}
int main(void)
{
    engine("P5", ENGI_PROPHET, 0);
    engine("J106", ENGI_J106, 0);
    engine("FM6", ENGI_FM6, 0);
    engine("CZ-1", ENGI_CZ, 0);
#if PPG_HAVE_ROM
    engine("PPG", ENGI_PPG, PPG_INIT);
#else
    printf("mod env: PPG: no PPG wave image: skipped\n");
#endif
    printf("mod env: %s\n", failures ? "FAILED" : "all ok");
    return failures != 0;
}
