/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* JUNO-106: the native tone format against the instrument's published MIDI
 * implementation, the switch fields, Roland's factory patches as the presets,
 * the voice (pitch, envelope times,
 * filter, resonance, chorus, high-pass), the library, the last state and its
 * pre-JUNO-106 form, favourites, and the editor's command 96. */
#include <assert.h>
#define MELODEE_FLASH 1
#define MELODEE_USB_AUDIO 0
#include "synth_harness.c"
#include "../firmware/src/usb_audio_stream.c"
#define SXQ 1024u
static uint32_t so_w, so_r, wire_n;
static uint8_t wire[2048];
static int ota_wire_send(const uint8_t *p, uint32_t n) { assert(n <= sizeof wire); memcpy(wire, p, n); wire_n = n; return 0; }
#define ED_BK_FLASH_PTR(off) (nor + (off))
static int ota_frame_get(const uint8_t **p, uint32_t *n) { (void)p; (void)n; return 0; }
static void ota_frame_done(void) {}
#include "../firmware/src/editor.c"
static int failures;
static void ok(const char *what, int pass) { printf("j106: %s: %s\n", what, pass ? "ok" : "FAIL"); failures += !pass; }
static void request(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    uint8_t f[1024] = {0x7D, 0x46, 0x4C};
    f[3] = (uint8_t)cmd; if (n) memcpy(f + 4, a, n);
    wire_n = 0; ed_handle(f, n + 4);
}
static j106_patch_t *tone(void) { return j106_patch_of(TSEL); }
static void juno(uint32_t preset)
{
    power_on();
    host_preset(TSEL, ENGI_J106, preset);
    TSEL->p[P_VOICE] = V_POLY;
}
static int32_t block(int32_t *out) { int32_t peak = 0; track_render(TSEL, out, CTL); for (uint32_t i = 0; i < CTL; i++) { int32_t a = out[i] < 0 ? -out[i] : out[i]; if (a > peak) peak = a; } return peak; }
static uint32_t live(void) { uint32_t n = 0; for (uint32_t i = 0; i < NVOICE; i++) n += TSEL->v[i].active; return n; }
static j106_voice_t *voice0(void) { for (uint32_t i = 0; i < NVOICE; i++) if (TSEL->v[i].active) return j106_voice(TSEL, &TSEL->v[i]); return 0; }
/* positive-going zero crossings of the part's output over `seconds`: the fundamental, for a plain wave */
static float crossings(float seconds)
{
    int32_t out[CTL], prev = 0; uint32_t n = 0, blocks = (uint32_t)(seconds * FS / CTL);
    for (uint32_t b = 0; b < 40u; b++) block(out);              /* (the attack, the filter's settling) */
    for (uint32_t b = 0; b < blocks; b++) {
        block(out);
        for (uint32_t i = 0; i < CTL; i++) { n += prev < 0 && out[i] >= 0; prev = out[i]; }
    }
    return (float)n / seconds;
}
static double energy(uint32_t blocks)
{
    int32_t out[CTL]; double e = 0;
    for (uint32_t b = 0; b < blocks; b++) { block(out); for (uint32_t i = 0; i < CTL; i++) e += (double)out[i] * out[i]; }
    return e;
}

static void format(void)
{
    /* the ST-Log / Roland layout: 1010010 = chorus I on, saw on, pulse off, 8' */
    j106_patch_t p; j106_patch_init(&p);
    p.raw[J106_SW1] = 82; p.raw[J106_SW2] = 0x1F;
    ok("SW1 82: chorus I, saw, no pulse, 8'", j106_chorus(p.raw) == 1u && j106_switch(p.raw, J106_SAW) &&
       !j106_switch(p.raw, J106_PULSE) && j106_range(p.raw) == 1u);
    ok("SW2 bits: PWM manual, envelope inverted, VCA gate, HPF 0", j106_switch(p.raw, J106_PWM_MODE) &&
       j106_switch(p.raw, J106_ENV_POL) && j106_switch(p.raw, J106_VCA_MODE) && j106_hpf(p.raw) == 0u);
    p.raw[J106_SW2] = 0; ok("SW2 0: HPF 3", j106_hpf(p.raw) == 3u);
    p.raw[J106_SW1] = 0x60; ok("SW1 chorus-off bit wins over the level bit", j106_chorus(p.raw) == 0u);
    /* every switch field edits only its own bits */
    int own = 1;
    for (uint32_t f = J106_RANGE; f < J106_FIELDS; f++)
        for (uint32_t base = 0; base < 128u; base += 9u) {
            j106_patch_init(&p); p.raw[J106_SW1] = (uint8_t)base; p.raw[J106_SW2] = (uint8_t)(base & 31u);
            for (int32_t v = J106_PANEL[f].min; v <= J106_PANEL[f].max; v++) {
                j106_patch_t q = p; j106_switch_set(q.raw, f, (uint32_t)v);
                own &= j106_switch(q.raw, f) == (uint32_t)v;
                for (uint32_t g = J106_RANGE; g < J106_FIELDS; g++)
                    if (g != f && !(f == J106_CHORUS && g == J106_CHORUS)) own &= j106_switch(q.raw, g) == j106_switch(p.raw, g) || (f == J106_RANGE && g == J106_RANGE);
                own &= q.raw[J106_SW1] < 128u && q.raw[J106_SW2] < 128u && !memcmp(q.raw, p.raw, J106_SW1);
            }
        }
    ok("each switch field changes only its own bits", own);
    /* exact patch messages */
    int32_t seed = 106;
    int round = 1;
    for (uint32_t k = 0; k < 500u; k++) {
        uint8_t f[J106_FRAME] = {0xF0, 0x41, 0x30, (uint8_t)(k & 15u), (uint8_t)(k & 127u)}, g[J106_FRAME];
        for (uint32_t i = 5; i < J106_FRAME - 1u; i++) f[i] = (uint8_t)(noise32(&seed) & 127u);
        f[J106_FRAME - 1u] = 0xF7;
        j106_patch_t q;
        round &= j106_patch_decode(&q, f, sizeof f) && j106_patch_valid(&q) && q.program == (k & 127u) &&
                 j106_patch_encode(&q, k & 15u, g, sizeof g) == J106_FRAME && !memcmp(f, g, sizeof f);
    }
    ok("random patch messages round-trip every byte", round);
    uint8_t f[J106_FRAME] = {0xF0, 0x41, 0x30, 0, 0x4F}, g[J106_FRAME];
    memset(f + 5, 64, J106_RAW); f[J106_FRAME - 1u] = 0xF7;
    j106_patch_t q, keep; j106_patch_init(&keep); q = keep;
    char name[J106_NAME_LEN + 1u];
    ok("a patch message is named by the instrument's label", j106_patch_decode(&q, f, sizeof f) &&
       (j106_patch_name(name, &q), str_eq(name, "PATCH B28")));
    f[2] = 0x31; ok("a manual-mode message is MANUAL", j106_patch_decode(&q, f, sizeof f) && (j106_patch_name(name, &q), str_eq(name, "MANUAL")));
    int rejected = 1;
    for (uint32_t n = 0; n < J106_FRAME; n++) { q = keep; rejected &= !j106_patch_decode(&q, f, n) && !memcmp(&q, &keep, sizeof q); }
    memcpy(g, f, sizeof g); g[1] = 0x43; rejected &= !j106_patch_decode(&q, g, sizeof g);
    memcpy(g, f, sizeof g); g[2] = 0x32; rejected &= !j106_patch_decode(&q, g, sizeof g);
    memcpy(g, f, sizeof g); g[3] = 16; rejected &= !j106_patch_decode(&q, g, sizeof g);
    memcpy(g, f, sizeof g); g[9] = 0x80; rejected &= !j106_patch_decode(&q, g, sizeof g);
    ok("short, foreign, out-of-channel and 8-bit frames are refused atomically", rejected && !memcmp(&q, &keep, sizeof q));
    q = keep; q.raw[3] = 200; ok("a byte above 127 is not a tone", !j106_patch_valid(&q) && !j106_patch_encode(&q, 0, g, sizeof g));
}

static void voice(void)
{
    /* pitch: A4 at 8', then 16' and 4'; the sub an octave below */
    juno(0);
    tone()->raw[J106_VCF_FREQ] = 127; tone()->raw[J106_ENV_S] = 127;
    trk_note_on(TSEL, 69, 100);
    float f8 = crossings(1.0f);
    ok("8' A4 plays 440 Hz", f8 > 437.0f && f8 < 443.0f);
    j106_switch_set(tone()->raw, J106_RANGE, 0); float f16 = crossings(1.0f);
    j106_switch_set(tone()->raw, J106_RANGE, 2); float f4 = crossings(1.0f);
    printf("  8' %.1f Hz, 16' %.1f Hz, 4' %.1f Hz\n", f8, f16, f4);
    ok("16' and 4' an octave either side", f16 > 218.0f && f16 < 222.0f && f4 > 875.0f && f4 < 885.0f);
    j106_switch_set(tone()->raw, J106_RANGE, 1); j106_switch_set(tone()->raw, J106_SAW, 0);
    tone()->raw[J106_SUB] = 127; float fs = crossings(1.0f);
    ok("the sub-octave square alone is 220 Hz", fs > 218.0f && fs < 222.0f);
    trk_all_off(TSEL);

    /* envelope times: the attack 1.5 ms .. 3 s, the release to 1 % 1.5 ms .. 12 s */
    int32_t out[CTL];
    for (uint32_t a = 0; a <= 127u; a += 127u) {
        juno(0); tone()->raw[J106_ENV_A] = (uint8_t)a; trk_note_on(TSEL, 60, 100);
        uint32_t b = 0; block(out);
        while (voice0() && voice0()->stage == 1u && b < 400000u) { block(out); b++; }
        float s = (float)(b + 1u) * CTL / FS;
        printf("  attack %u: %.4f s\n", (unsigned)a, s);
        ok(a ? "attack 127 near 3 s" : "attack 0 within 3 ms", a ? s > 2.6f && s < 3.4f : s < 0.003f);
    }
    for (uint32_t r = 0; r <= 127u; r += 127u) {
        juno(0); tone()->raw[J106_ENV_A] = 0; tone()->raw[J106_ENV_S] = 127; tone()->raw[J106_ENV_R] = (uint8_t)r;
        trk_note_on(TSEL, 60, 100); for (uint32_t b = 0; b < 20u; b++) block(out);
        trk_note_off(TSEL, 60);
        uint32_t b = 0;
        while (voice0() && voice0()->env > 0.01f && b < 400000u) { block(out); b++; }
        float s = (float)b * CTL / FS;
        printf("  release %u to 1%%: %.4f s\n", (unsigned)r, s);
        ok(r ? "release 127 near 12 s" : "release 0 within 3 ms", r ? s > 10.5f && s < 13.5f : s < 0.003f);
        for (b = 0; live() && b < 100000u; b++) block(out);
        ok("a released voice ends", !live());
    }
    juno(0); j106_switch_set(tone()->raw, J106_VCA_MODE, 1); tone()->raw[J106_ENV_R] = 127;
    trk_note_on(TSEL, 60, 100); for (uint32_t b = 0; b < 20u; b++) block(out);
    trk_note_off(TSEL, 60); uint32_t b;
    for (b = 0; live() && b < 1000u; b++) block(out);
    ok("GATE: the VCA closes within a few blocks of the key", !live() && b < 8u);

    /* the filter: more cutoff, more of the upper harmonics; full resonance whistles with the mixer shut */
    juno(0); tone()->raw[J106_VCF_FREQ] = 30; trk_note_on(TSEL, 48, 100);
    double low = energy(200); trk_all_off(TSEL);
    juno(0); tone()->raw[J106_VCF_FREQ] = 90; trk_note_on(TSEL, 48, 100);
    double high = energy(200); trk_all_off(TSEL);
    ok("the cutoff opens the sound", high > low * 4.0);
    juno(0); j106_switch_set(tone()->raw, J106_SAW, 0); tone()->raw[J106_SUB] = 0;
    tone()->raw[J106_VCF_RES] = 127; tone()->raw[J106_VCF_FREQ] = 70; tone()->raw[J106_ENV_S] = 127;
    trk_note_on(TSEL, 60, 100); energy(400);
    double whistle = energy(400); trk_all_off(TSEL);
    juno(0); j106_switch_set(tone()->raw, J106_SAW, 0); tone()->raw[J106_SUB] = 0;
    tone()->raw[J106_VCF_RES] = 80; tone()->raw[J106_VCF_FREQ] = 70;
    trk_note_on(TSEL, 60, 100); energy(400);
    double quiet = energy(400); trk_all_off(TSEL);
    printf("  silent mixer: resonance 127 %.3g, 80 %.3g\n", whistle, quiet);
    ok("full resonance self-oscillates; moderate resonance does not", whistle > 1e9 && quiet < whistle * 1e-6);

    /* every preset (INIT, the factory patches) sounds within a second (FX RISE 2 rises from 0.28 s), stays
     * finite and bounded (three voices at the VCA's +6 dB, through HPF 0's +4 dB shelf), and ends */
    int sound = 1;
    for (uint32_t k = 0; k < ENG_J106.npresets; k++) {
        juno(k);
        trk_note_on(TSEL, 48, 100); trk_note_on(TSEL, 55, 100); trk_note_on(TSEL, 64, 100);
        int32_t peak = 0; double e = 0;
        for (uint32_t i = 0; i < FS / CTL; i++) { int32_t p = block(out); if (p > peak) peak = p; for (uint32_t j = 0; j < CTL; j++) e += (double)out[j] * out[j]; }
        trk_all_off(TSEL);
        for (b = 0; live() && b < 30000u; b++) block(out);
        char nm[24]; str_cpy(nm, eng_preset(&ENG_J106, k)->name, sizeof nm);
        if (!(e > 0 && peak < 3 * 2 * VOICE_FS * AUDIO_ONE * 8 / 5 && !live())) { printf("  tone %u %s: peak %d live %u\n", (unsigned)k, nm, (int)peak, (unsigned)live()); sound = 0; }
    }
    ok("every preset sounds, is bounded and ends", sound);
}

static void factory(void)
{
    /* Roland's factory patches: preset k is program k - 1 of the vendor file, byte for byte; A11 as both
     * cassette recordings hold it (assets/j106-factory/README.md) */
    static const uint8_t A11[J106_RAW] = {0x14, 0x31, 0x00, 0x66, 0x00, 0x23, 0x0D, 0x3A, 0x00,
                                          0x56, 0x6C, 0x03, 0x31, 0x2D, 0x20, 0x00, 0x51, 0x11};
    uint8_t syx[128u * J106_FRAME];
    FILE *f = fopen("assets/j106-factory/j106-factory.syx", "rb");
    size_t n = f ? fread(syx, 1, sizeof syx, f) : 0;
    if (f) fclose(f);
    ok("the factory bank file", n == sizeof syx && J106_FACTORY_N == 128u && ENG_J106.npresets == J106_FACTORY_N + 1u);
    int same = n == sizeof syx;
    for (uint32_t k = 1; same && k <= J106_FACTORY_N; k++) {
        j106_patch_t want; char nm[16];
        juno(k);
        j106_patch_name(nm, tone());
        same = j106_patch_decode(&want, syx + (k - 1u) * J106_FRAME, J106_FRAME) && !memcmp(tone()->raw, want.raw, J106_RAW) &&
               tone()->program == k - 1u && str_eq(nm, eng_preset(&ENG_J106, k)->name);
        if (!same) printf("  preset %u %s\n", (unsigned)k, nm);
    }
    ok("presets 1..128: the factory patches A11..B88, named as their presets", same);
    juno(1);
    ok("A11 BRASS", !memcmp(tone()->raw, A11, J106_RAW) && str_eq(eng_preset(&ENG_J106, 1)->name, "BRASS"));
    power_on(); set_engine_of(TSEL, ENGI_J106);
    ok("choosing JUNO-106 plays A11", TSEL->preset == 1u && !memcmp(tone()->raw, A11, J106_RAW));
    /* VCA LEVEL is the panel's -5..+5 trim: five factory patches (B23 ORCH PAD, B85 ROCKET MEN ..) sit at 0 */
    double e[2];
    for (uint32_t i = 0; i < 2u; i++) {
        juno(0); tone()->raw[J106_VCA_LEVEL] = i ? 64u : 0u;
        trk_note_on(TSEL, 60, 100); energy(50); e[i] = energy(300); trk_all_off(TSEL);
    }
    printf("  VCA LEVEL 0 against 64: %.1f dB\n", 10.0 * log10(e[0] / e[1]));
    ok("VCA LEVEL 0 is quieter, not silent", e[0] > e[1] * 0.1 && e[0] < e[1] * 0.5);
    /* the envelope law between the published ends, as the factory patches need it: their sustain-0 decays
     * (key held, to 1 %) a pluck's, a piano's and a held flute's, not clicks */
    static const struct { uint32_t k; float lo, hi; } DECAY[] = {{22, 0.05f, 0.2f},    /* A37 PIZZICATO */
                                                               {7, 2.0f, 5.0f},      /* A18 PIANO I */
                                                               {3, 3.0f, 8.0f}};     /* A14 FLUTES */
    int32_t out[CTL];
    for (uint32_t i = 0; i < NELEM(DECAY); i++) {
        juno(DECAY[i].k + 1u); trk_note_on(TSEL, 60, 100);
        uint32_t b = 0; int peaked = 0;
        while (voice0() && b < 40000u && !(peaked && voice0()->env < 0.01f)) { block(out); b++; peaked |= voice0()->stage == 2u; }
        float s = (float)b * CTL / FS;
        char nm[16]; j106_patch_name(nm, tone());
        printf("  %s decays in %.2f s\n", nm, s);
        ok(DECAY[i].k == 22u ? "PIZZICATO plucks" : DECAY[i].k == 7u ? "PIANO I rings" : "FLUTES holds", s > DECAY[i].lo && s < DECAY[i].hi);
        trk_all_off(TSEL); for (b = 0; live() && b < 60000u; b++) block(out);
    }
}

/* Hann-windowed power at one frequency. The audio checks below measure the
 * unwanted modulation relative to the musical signal, independently of gain
 * and of the Q15/Q23 container used by this fixture. */
static double tone_power(const int32_t *pcm, uint32_t n, double hz)
{
    double re = 0, im = 0, weight = 0;
    for (uint32_t i = 0; i < n; i++) {
        double w = 0.5 - 0.5 * cos(2.0 * FM_PI * i / (n - 1u));
        double phase = 2.0 * FM_PI * hz * i / FS;
        double x = pcm[i] * w;
        re += x * cos(phase); im += x * sin(phase); weight += w;
    }
    return 2.0 * (re * re + im * im) / (weight * weight);
}

static void ethereal(void)
{
    static const uint8_t notes[] = {48, 55, 60, 64, 67, 72};
    const uint32_t n = 8u * FS;
    int32_t *pcm = malloc(n * sizeof *pcm), out[CTL];
    assert(pcm);
    for (uint32_t chord = 0; chord < 2u; chord++) {
        juno(102);                         /* B56 ETHEREAL, unchanged factory bytes */
        assert(str_eq(eng_preset(&ENG_J106, 102)->name, "ETHEREAL"));
        if (chord) for (uint32_t k = 0; k < NELEM(notes); k++) trk_note_on(TSEL, notes[k], 100);
        else trk_note_on(TSEL, 60, 100);
        for (uint32_t b = 0; b < 2u * FS / CTL; b++) block(out);  /* past the slow attack */
        double total = 0;
        for (uint32_t pos = 0; pos < n; pos += CTL) {
            block(pcm + pos);
            for (uint32_t i = 0; i < CTL; i++) total += (double)pcm[pos + i] * pcm[pos + i];
        }
        total /= n;
        double lfo = 0.1 * pow(2.0, 69.0 * 8.23 / 127.0), rumble = 0;
        for (uint32_t h = 1; h <= 6u; h++) rumble += tone_power(pcm, n, lfo * h);
        printf("  ETHEREAL %s: PWM rumble %.1f dBr\n", chord ? "six-note chord" : "C4", 10.0 * log10(rumble / total));
        ok(chord ? "ETHEREAL chords do not add a subsonic pulse" : "ETHEREAL single-note rumble stays inaudible",
           total > 1.0 && rumble < total * (chord ? 1e-4 : 3e-5));
        /* Only the sub square plays at the lowest note's sub-octave, and PWM
         * changes the pulse, not the sub (DCO LFO is zero): sidebands there
         * are the filter's limit pumping a sub that sits on its resonant peak. */
        double sub = (double)tuned_pitch_inc((chord ? notes[0] : 60u) * 16u) * FS / 8589934592.0;
        double body = tone_power(pcm, n, sub);
        double side = tone_power(pcm, n, sub - lfo) + tone_power(pcm, n, sub + lfo);
        printf("  ETHEREAL %s sub-octave PWM sidebands %.1f dBr\n", chord ? "chord's lowest" : "C4", 10.0 * log10(side / body));
        ok(chord ? "ETHEREAL chords keep a steady sub" : "ETHEREAL keeps a steady sub", body > total * (chord ? 0.02 : 0.5) && side < body * 1e-4);
        trk_all_off(TSEL);
    }
    free(pcm);
}

static void part(void)
{
    /* the chorus: a stereo pair from the mono part; off, the part stays mono */
    int32_t out[CTL], left[CTL];
    juno(0);
    j106_switch_set(tone()->raw, J106_CHORUS, 1);
    trk_note_on(TSEL, 60, 100);
    double diff = 0, sum = 0; uint32_t stereo = 1;
    for (uint32_t b = 0; b < 400u; b++) {
        track_render(TSEL, out, CTL);
        stereo &= ENG_J106.stereo(TSEL, out, left, CTL);
        for (uint32_t i = 0; i < CTL; i++) { double l = left[i], r = out[i] - left[i]; diff += (l - r) * (l - r); sum += (l + r) * (l + r); }
    }
    ok("chorus I: left and right differ", stereo && diff > sum * 0.01);
    j106_switch_set(tone()->raw, J106_CHORUS, 2);
    for (uint32_t b = 0; b < 4u; b++) { track_render(TSEL, out, CTL); stereo &= ENG_J106.stereo(TSEL, out, left, CTL); }
    j106_switch_set(tone()->raw, J106_CHORUS, 0);
    track_render(TSEL, out, CTL);
    ok("chorus II is stereo; off is mono", stereo && !ENG_J106.stereo(TSEL, out, left, CTL));
    trk_all_off(TSEL);
    /* the whole mixer: chorus on, both channels sound and differ */
    juno(0); j106_switch_set(tone()->raw, J106_CHORUS, 2); trk_note_on(TSEL, 60, 100);
    double dl = 0, dr = 0, dd = 0;
    for (uint32_t b = 0; b < 400u; b++) {
        int32_t st[CTL * 2]; mix_block(st, CTL);
        for (uint32_t i = 0; i < CTL; i++) { dl += fabs((double)st[2 * i]); dr += fabs((double)st[2 * i + 1]); dd += fabs((double)st[2 * i] - st[2 * i + 1]); }
    }
    ok("mixed: both channels sound and differ", dl > 0 && dr > 0 && dd > (dl + dr) * 0.01 && dl < dr * 1.5 && dr < dl * 1.5);
    trk_all_off(TSEL);

    /* the high-pass: 3 thins a low note, 0 lifts it */
    double e[4];
    for (uint32_t h = 0; h < 4u; h++) {
        juno(0); j106_switch_set(tone()->raw, J106_HPF, h); tone()->raw[J106_VCF_FREQ] = 40;
        trk_note_on(TSEL, 36, 100); energy(50); e[h] = energy(300); trk_all_off(TSEL);
    }
    printf("  HPF energy 0..3: %.3g %.3g %.3g %.3g\n", e[0], e[1], e[2], e[3]);
    ok("HPF 0 > 1 > 2 > 3 for a low note", e[0] > e[1] && e[1] > e[2] && e[2] > e[3]);

    /* live single controls (41 32) edit the tone; patch messages wait in SAVE */
    juno(1);
    const uint8_t freq[] = {0x41, 0x32, 0x00, J106_VCF_FREQ, 0x21}, sw[] = {0x41, 0x32, 0x03, J106_SW2, 0x05};
    ok("41 32: a slider", j106_native_frame(freq, sizeof freq) && tone()->raw[J106_VCF_FREQ] == 0x21);
    ok("41 32: a switch byte", j106_native_frame(sw, sizeof sw) && tone()->raw[J106_SW2] == 0x05);
    const uint8_t other[] = {0x41, 0x10, 0x42, 0x12, 0x40, 0x00, 0x7F, 0x00, 0x41};
    ok("other Roland messages are not ours", !j106_native_frame(other, sizeof other));
    uint8_t msg[J106_FRAME - 2u] = {0x41, 0x30, 0x00, 0x09};
    for (uint32_t i = 4; i < sizeof msg; i++) msg[i] = (uint8_t)(i * 5u);
    sx_drop();
    ok("a patch message waits for SAVE", j106_native_frame(msg, sizeof msg) && sx_staged() && sx_stage.e == ENGI_J106 &&
       sx_stage.size == sizeof(j106_patch_t) && ((const j106_patch_t *)sx_stage.raw)->raw[3] == 35u);
    char nm[16]; sx_name(0, nm);
    ok(".. under the instrument's label", str_eq(nm, "PATCH A22"));
}

static void library(void)
{
    power_on(); memset(nor, 255, sizeof nor); persist_boot(); settings_init();
    juno(5);
    tone()->raw[J106_VCF_RES] = 99; TSEL->p[P_E0] = 7;
    j106_patch_t before = *tone();
    ok("store a tone in slot 300", !up_store_to(299, 0, "MY JUNO") && up_used(299) && up_engine(299) == ENGI_J106);
    char nm[16]; up_name(299, nm);
    ok(".. under its name", str_eq(nm, "MY JUNO"));
    juno(2);
    ok(".. and load it", !up_load(299) && TSEL->eng_req == ENGI_J106 && !memcmp(tone()->raw, before.raw, J106_RAW) && TSEL->p[P_E0] == 7);
    up_rec_t r = *up_rec(299); r.packed[J106_VCF_FREQ] = 200;
    ok("a record with an 8-bit byte is not valid", !up_valid(&r));
    ok("native slots: used, named, raw", native_used(ENGI_J106, 299) && !memcmp(native_raw(ENGI_J106, 299), before.raw, J106_RAW));
    j106_patch_t imp; j106_patch_init(&imp); imp.raw[0] = 1; j106_patch_name_set(&imp, "IMPORTED");
    uint32_t saved = 0;
    ok("an import into the shared slots", !shared_import(ENGI_J106, 10, 1, (const uint8_t *)&imp, sizeof imp, &saved) && saved == 1u &&
       (up_name(10, nm), !memcmp(nm, "IMPORTED", 8)));      /* (native names keep their padding, as other native slots) */
    /* favourites: the presets in their tagged row, the old SID stars and the former starter tones' cleared */
    memset(favorites.factory[3], 0xFF, 32);
    ok("an untagged row holds no JUNO-106 star", !favorite_has(ENGI_J106, 3));
    ok("a star", favorite_set(ENGI_J106, 3, 1) && favorite_has(ENGI_J106, 3) && !favorite_has(ENGI_J106, 4) && favorites.factory[3][28] == 'J');
    ok(".. and off", favorite_set(ENGI_J106, 3, 0) && !favorite_has(ENGI_J106, 3));
    ok("no star past the row", !favorite_set(ENGI_J106, J106_STAR_CAP, 1) && J106_STAR_CAP >= ENG_J106.npresets);
    memset(favorites.factory[3], 0, 32); favorites.factory[3][0] = 0xFF; memcpy(favorites.factory[3] + 28, "J6F\1", 4);
    ok("the former starter tones' stars are not the factory patches'", !favorite_has(ENGI_J106, 1));
    ok(".. and are cleared at the first star", favorite_set(ENGI_J106, 3, 1) && !favorite_has(ENGI_J106, 1) && favorites.factory[3][31] == 2u);
}

static void state(void)
{
    power_on(); memset(nor, 255, sizeof nor); persist_boot(); settings_init();
    juno(9); tone()->raw[J106_VCF_KYBD] = 77; j106_patch_name_set(tone(), "LAST ONE");
    j106_patch_t want = *tone();
    assert(state_autosave());
    power_on(); syn_saved = 0; state_boot();
    ok("the last state keeps the tone", TSEL->eng_req == ENGI_J106 && !memcmp(tone(), &want, sizeof want));
    /* a state from before JUNO-106 (it ends after the Prophet program) loads with the INIT tone */
    power_on(); host_preset(TSEL, ENGI_FM6, 3);
    synth_state_t s; state_capture(&s);
    uint8_t old[sizeof s]; memset(old, 0, sizeof old);
    {
        static synth_state1_t o; memset(&o, 0, sizeof o);
        const synth_part_t *a = &s.part[0];
        o.magic = SYN_MAGIC1; o.size = SYN_SIZE_V1;
        memcpy(o.g, s.g, sizeof o.g); o.octave = s.octave;
        o.engine = a->engine; o.preset = a->preset; o.user_native = a->user_native; o.mpc_octave = a->mpc_octave; o.user = a->user;
        memcpy(o.p, a->p, sizeof o.p); memcpy(o.fm6, a->fm6, sizeof o.fm6); memcpy(o.fm6_fn, a->fm6_fn, sizeof o.fm6_fn);
        o.cz = a->cz; o.p5 = a->p5;
        memcpy(old, &o, __builtin_offsetof(synth_state1_t, j106));
    }
    uint32_t sum = syn_hash(old, SYN_SIZE_V1 - 4u); memcpy(old + SYN_SIZE_V1 - 4u, &sum, 4);
    synth_state_t up; memcpy(&up, old, sizeof up);
    j106_patch_t init; j106_patch_init(&init);
    ok("an older state upgrades", state_upgrade(&up, SYN_SIZE_V1) && state_valid(&up) && up.part[0].engine == ENGI_FM6 && !memcmp(&up.part[0].j106, &init, sizeof init));
    memcpy(&up, old, sizeof up); ((synth_state1_t *)&up)->p[0] ^= 1;
    ok(".. not a corrupt one", !state_upgrade(&up, SYN_SIZE_V1));
    assert(!st_save(OBJ_SYNTH, old, SYN_SIZE_V1));
    power_on(); host_preset(TSEL, ENGI_J106, 4); syn_saved = 0; state_boot();
    ok("an older state boots", TSEL->eng_req == ENGI_FM6);
    /* a backup from before JUNO-106 restores */
    memcpy(ED_BK_RAW, old, SYN_SIZE_V1); ed_bk_id = 32; ed_bk_len = ed_bk_pos = SYN_SIZE_V1; ed_bk_crc = st_crc32(ED_BK_RAW, ed_bk_len);
    host_preset(TSEL, ENGI_J106, 4);
    ok("an older backup's state restores", !ed_bk_commit_inner() && TSEL->eng_req == ENGI_FM6);
}

static void editor(void)
{
    power_on(); memset(nor, 255, sizeof nor); persist_boot(); settings_init();
    request(ED_INFO, 0, 0);
    uint32_t tag = 0;
    for (uint32_t i = 5; i + 5u < wire_n; i++) if (wire[i] == 0x4a && wire[i + 1] == 1 && wire[i + 2] == ENGI_J106 && wire[i + 4] == ED_J106) tag = i;
    ok("INFO names JUNO-106 and command 96", tag && wire[tag + 3] == USER_NATIVE_J106);
    juno(3);
    song.g[G_DRUMCH] = 3;                                  /* (GLOBAL > MIDI OUT CH 3: the message's channel) */
    uint8_t get[] = {0, 0}; request(ED_J106, get, sizeof get);
    uint8_t frame[J106_FRAME]; j106_patch_encode(tone(), 2, frame, sizeof frame);
    ok("GET: the patch message and the name", wire_n > 30u && wire[5] == 1 && wire[6] == 0 && !memcmp(wire + 8, frame + 1, J106_FRAME - 2u) &&
       !memcmp(wire + 8 + J106_FRAME - 2u, "TRUMPET", 7));
    uint8_t put[2 + J106_FRAME - 2u] = {1, 0};
    memcpy(put + 2, frame + 1, J106_FRAME - 2u); put[2 + 4 + J106_VCF_RES] = 111;   /* (frame byte 5: the first slider) */
    host_preset(TSEL, ENGI_FM6, 0);
    request(ED_J106, put, sizeof put);
    ok("PUT: the part plays the tone", wire[6] == 0 && TSEL->eng_req == ENGI_J106 && tone()->raw[J106_VCF_RES] == 111);
    uint8_t field[] = {2, 0, J106_CHORUS, 2}; request(ED_J106, field, sizeof field);
    ok("a field edit", wire[6] == 0 && j106_chorus(tone()->raw) == 2u);
    uint8_t bad[] = {2, 0, J106_HPF, 4}; request(ED_J106, bad, sizeof bad);
    ok(".. outside its range refused", wire[6] == 1 && j106_hpf(tone()->raw) != 4u);
    uint8_t name[] = {3, 0, 'B', 'E', 'L', 'L'}; request(ED_J106, name, sizeof name);
    char nm[16]; j106_patch_name(nm, tone());
    ok("a name", wire[6] == 0 && str_eq(nm, "BELL"));
    uint8_t fav[] = {USER_NATIVE_J106, 0, 0x40, 4}; request(ED_FAV_GET, fav, sizeof fav);   /* (start: v14, 8192 = 0) */
    ok("favourites answer the JUNO-106 namespace", wire_n && wire[5] == 0 && wire[6] == USER_NATIVE_J106);
    uint8_t star[] = {ENGI_J106, 2, 0x40, 1}; request(ED_FAV_SET, star, sizeof star);
    ok(".. and its presets", favorite_has(ENGI_J106, 2));
}

int main(void)
{
    format();
    factory();
    voice();
    ethereal();
    part();
    library();
    state();
    editor();
    printf(failures ? "J106 TEST FAILED (%d)\n" : "J106 TEST PASSED\n", failures);
    return failures != 0;
}
