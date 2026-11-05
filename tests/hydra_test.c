/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* HYDRA (eng_hydra.c): every factory sound, mutant and filter type sounds and stays in range; SCAN crossfades its slots and
 * an EMPTY slot is silence; ENV 1 and the macros reach the scan and the filters; the oscillators alias little up the
 * keyboard; the state follows the patch and survives a full arena; a re-press rings on; the panel, the user slots, the last
 * state, performances and their older forms, favourites, command 97 and the native message keep the patch whole. */
#include <assert.h>
#include <math.h>
#define MELODEE_FLASH 1
#define MELODEE_USB_AUDIO 0
/* (a small arena: part B can leave part A no room to grow) */
#define RESOURCE_HOST_WORDS 4096u
#define RESOURCE_HOST_LOW_WORDS 16u
#define RESOURCE_HOST_DATA_WORDS 16u
#define RESOURCE_HOST_RETAINED_WORDS 16u
#include "synth_harness.c"
#define SXQ 1024u
static uint32_t so_w, so_r, wire_n;
static uint8_t wire[2048];
static void ota_wire_send(const uint8_t *p, uint32_t n) { assert(n <= sizeof wire); memcpy(wire, p, n); wire_n = n; }
#define ED_BK_FLASH_PTR(off) (nor + (off))
static int ota_frame_get(const uint8_t **p, uint32_t *n) { (void)p; (void)n; return 0; }
static void ota_frame_done(void) {}
#include "../firmware/src/editor.c"

static int bad;
static void hcheck(const char *what, int pass) { printf("hydra: %s: %s\n", what, pass ? "ok" : "FAIL"); bad += !pass; }
#define check hcheck
static void request(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    uint8_t f[1024] = {0x7D, 0x46, 0x4C};
    f[3] = (uint8_t)cmd; if (n) memcpy(f + 4, a, n);
    wire_n = 0; ed_handle(f, n + 4);
}

#define HY_N 32768u
static int32_t hbuf[HY_N];
static void hy_quiet(track_t *t)                       /* every voice gone, a block with none (the DC blocker afresh) */
{
    int32_t out[CTL];
    for (uint32_t i = 0; i < NVOICE; i++)
        t->v[i].active = t->v[i].gate = t->v[i].stage = 0;
    track_render(t, out, CTL);
}
static track_t *hy_fresh(uint32_t preset)
{
    track_t *t = &trk[0];
    hy_quiet(t);
    host_preset(t, ENGI_HYDRA, preset);
    return t;
}
/* note held for n samples (from the first block), into hbuf; returns the peak after skip samples */
static int32_t hy_play(track_t *t, uint32_t note, uint32_t n, uint32_t skip)
{
    int32_t out[CTL], peak = 0;
    uint32_t i, k;
    trk_note_on(t, note, 100);
    for (i = 0; i < n; i += CTL) {
        track_render(t, out, CTL);
        for (k = 0; k < CTL && i + k < n && i + k < HY_N; k++) {
            hbuf[i + k] = out[k];
            if (i + k >= skip && abs(out[k]) > peak)
                peak = abs(out[k]);
        }
    }
    trk_note_off(t, note);
    hy_quiet(t);
    return peak;
}
static uint64_t hy_hash(uint32_t n)
{
    uint64_t h = 1469598103934665603ull;
    for (uint32_t i = 0; i < n; i++)
        h = (h ^ (uint32_t)hbuf[i]) * 1099511628211ull;
    return h;
}
/* the share of the energy away from the harmonics of f0 (dB), over N samples from at (a 4-term Blackman-Harris window: its
 * side lobes, -92 dB, stay out of the count) */
static double hy_alias_db(uint32_t at, uint32_t N, double f0)
{
    double harm = 0.0, all = 0.0;
    for (uint32_t b = 1; b < N / 2u; b++) {
        double re = 0.0, im = 0.0, f = (double)b * FS / N, p;
        for (uint32_t i = 0; i < N; i++) {
            double a = 2.0 * M_PI * i / N, x;
            double w = 0.35875 - 0.48829 * cos(a) + 0.14128 * cos(2.0 * a) - 0.01168 * cos(3.0 * a);
            x = hbuf[at + i] * w;
            re += x * cos(2.0 * M_PI * b * i / N);
            im -= x * sin(2.0 * M_PI * b * i / N);
        }
        p = re * re + im * im;
        all += p;
        if (fabs(f / f0 - floor(f / f0 + 0.5)) * f0 < 4.5 * FS / N)
            harm += p;
    }
    return 10.0 * log10((all - harm + 1e-9) / (all + 1e-9));
}
static uint8_t *hy_raw(track_t *t) { return hy_patch_of(t)->raw; }

static void the_state(void)
{
    int32_t out[CTL];
    uint32_t b1, b2, b3, k, sz, i;
    int32_t pk = 0;
    track_t *t;
    int ok;
    check("the part state fits its linker bank at the most", eng_state_size(ENGI_HYDRA) <= 16384u && eng_state_size(ENGI_HYDRA) == HY_BYTES_OF(HY_ROWS));
    t = hy_fresh(0);
    trk_note_on(t, 57, 100);
    track_render(t, out, CTL);
    b1 = resource[RES_ENGINE0].size;
    hy_raw(t)[HY_O1 + HO_MODE] = 1;                    /* (an edit while a note sounds: reallocated, playing on) */
    track_render(t, out, CTL);
    b2 = resource[RES_ENGINE0].size;
    hy_raw(t)[HY_O2 + HO_MODE] = 1;
    track_render(t, out, CTL);
    b3 = resource[RES_ENGINE0].size;
    hy_raw(t)[HY_O1 + HO_MODE] = hy_raw(t)[HY_O2 + HO_MODE] = 0;
    track_render(t, out, CTL);
    ok = b1 == HY_BYTES_OF(3) && b2 == HY_BYTES_OF(10) && b3 == HY_BYTES_OF(17) &&
         resource[RES_ENGINE0].size == HY_BYTES_OF(3) && t->v[0].active;
    for (k = 0; k < 40u; k++) {
        track_render(t, out, CTL);
        for (i = 0; i < CTL; i++) if (abs(out[i]) > pk) pk = abs(out[i]);
    }
    ok &= pk > 1000 * AUDIO_ONE;
    printf("  part state: %u B single, %u one scanning, %u both\n", b1, b2, b3);
    hy_quiet(t);
    check("the part state follows the patch (3, 10, 17 wave copies), edits while sounding keep playing", ok);

    /* the arena full: a patch that needs more wave copies keeps the ones it has and plays each scanning oscillator's first
     * wave (no silence); once memory frees it grows, within 64 blocks */
    pk = 0;
    t = hy_fresh(2);                                   /* VOX CHOIR, made SINGLE / SINGLE: 3 wave copies */
    hy_raw(t)[HY_O1 + HO_MODE] = hy_raw(t)[HY_O2 + HO_MODE] = 0;
    trk_note_on(t, 57, 100);
    track_render(t, out, CTL);
    for (sz = 65536u; sz >= 256u; sz >>= 1)            /* part B takes what it can */
        if (resource_get(RES_ENGINE0 + 1u, sz))
            break;
    hy_raw(t)[HY_O1 + HO_MODE] = hy_raw(t)[HY_O2 + HO_MODE] = 1;   /* back to scanning: 17 wanted */
    for (k = 0; k < 400u; k++) {                       /* (VOX CHOIR's slow attack: 0.3 s) */
        track_render(t, out, CTL);
        for (i = 0; i < CTL; i++) if (abs(out[i]) > pk) pk = abs(out[i]);
    }
    ok = resource[RES_ENGINE0].size == HY_BYTES_OF(HY_ROWS_MIN) && hy_part(0) && hy_part(0)->b.fallback && pk > 1000 * AUDIO_ONE;
    printf("  arena full: held %u B, fallback %u, peak %d\n", resource[RES_ENGINE0].size, hy_part(0) ? hy_part(0)->b.fallback : 9u, pk);
    resource_release(RES_ENGINE0 + 1u);
    for (k = 0; k < 70u; k++)
        track_render(t, out, CTL);
    ok &= resource[RES_ENGINE0].size == HY_BYTES_OF(HY_ROWS) && !hy_part(0)->b.fallback && t->v[0].active;
    hy_quiet(t);
    check("arena full: HYDRA keeps its wave copies and plays first waves, then grows back", ok);
}

static void the_sound(void)
{
    uint32_t pi, k;
    int32_t peak;
    track_t *t;
    int ok = 1;
    for (pi = 0; pi < ENGINES[ENGI_HYDRA]->npresets; pi++) {
        hy_patch_t p;
        t = hy_fresh(pi);
        peak = hy_play(t, 57, FS / 2u, 2048);
        if (peak < 300 * AUDIO_ONE || peak >= 30000 * AUDIO_ONE) {
            printf("  %s: peak %d\n", eng_preset(ENGINES[ENGI_HYDRA], pi)->name, peak);
            ok = 0;
        }
        p = *hy_patch_of(t);
        ok &= hy_patch_valid(&p);
    }
    check("every factory sound plays, below full scale, and is a valid patch", ok);
    {   /* at the other engines' level, in either precision: an engine written for another scale is 48 dB off */
        int32_t hy, p5;
        t = hy_fresh(0);
        hy = hy_play(t, 57, FS / 2u, 2048);
        hy_quiet(t);
        host_preset(t, ENGI_PROPHET, 1);
        p5 = 0;
        trk_note_on(t, 57, 100);
        for (k = 0; k < FS / 2u / CTL; k++) {
            int32_t out[CTL];
            track_render(t, out, CTL);
            for (uint32_t i = 0; i < CTL; i++) if (abs(out[i]) > p5) p5 = abs(out[i]);
        }
        trk_note_off(t, 57);
        hy_quiet(t);
        printf("  one note: HYDRA INIT peak %.1f dB against the Prophet's first program\n", 20.0 * log10((double)hy / p5));
        check("HYDRA stands at the other engines' level", hy > p5 / 4 && hy < p5 * 4);
    }

    t = hy_fresh(0);
    hy_raw(t)[HY_F1_CUT] = 90;
    hy_play(t, 57, 8192, 0);
    {
        uint64_t plain = hy_hash(8192), seen[MU_N];
        ok = 1;
        for (k = 1; k < MU_N; k++) {
            t = hy_fresh(0);
            hy_raw(t)[HY_F1_CUT] = 90;
            hy_raw(t)[HY_M1 + HM_MODE] = (uint8_t)k;
            hy_raw(t)[HY_M1 + HM_DEPTH] = 80;
            hy_raw(t)[HY_M1 + HM_AUX] = 40;
            peak = hy_play(t, 57, 8192, 1024);
            seen[k] = hy_hash(8192);
            ok &= peak > 300 * AUDIO_ONE && peak < 30000 * AUDIO_ONE && seen[k] != plain;
            for (uint32_t j = 1; j < k; j++)
                ok &= seen[j] != seen[k];
        }
        check("each MUTANT mode changes the sound, each its own way, in range", ok);
    }
    {
        uint64_t seen[F1_N];
        ok = 1;
        for (k = 0; k < F1_N; k++) {
            t = hy_fresh(0);
            hy_raw(t)[HY_F1_TYPE] = (uint8_t)k;
            hy_raw(t)[HY_F1_CUT] = 70;
            hy_raw(t)[HY_F1_RES] = 80;
            hy_raw(t)[HY_F1_DRIVE] = 30;
            peak = hy_play(t, 45, 8192, 1024);
            seen[k] = hy_hash(8192);
            if (peak < 200 * AUDIO_ONE || peak >= 30000 * AUDIO_ONE)
                printf("  FILTER 1 %s: peak %d\n", N_HY_F1[k], peak), ok = 0;
            for (uint32_t j = 0; j < k; j++)
                ok &= seen[j] != seen[k];
        }
        check("each FILTER 1 type sounds its own way, in range", ok);
    }
    t = hy_fresh(0);
    hy_raw(t)[HY_F2_RES] = 60;
    hy_raw(t)[HY_F2_CUT] = 70;
    hy_play(t, 45, 8192, 0);
    {
        uint64_t lp = hy_hash(8192);
        t = hy_fresh(0);
        hy_raw(t)[HY_F2_RES] = 60;
        hy_raw(t)[HY_F2_CUT] = 70;
        hy_raw(t)[HY_F2_MORPH] = 127;
        peak = hy_play(t, 45, 8192, 1024);
        check("FILTER 2 morphs (low pass to high pass)", hy_hash(8192) != lp && peak > 200 * AUDIO_ONE);
    }

    /* SCAN: slot 8 alone holds a wave; at 0 the list reads EMPTY, at the top that wave */
    t = hy_fresh(0);
    hy_raw(t)[HY_O1 + HO_MODE] = 1;
    for (k = 0; k < 8u; k++)
        hy_raw(t)[HY_O1 + HO_L0 + k] = 0;
    hy_raw(t)[HY_O1 + HO_L0 + 7] = HYW_SAW + 1;
    peak = hy_play(t, 57, 4096, 512);
    check("an EMPTY slot is silence", peak < 4 * AUDIO_ONE);
    t = hy_fresh(0);
    hy_raw(t)[HY_O1 + HO_MODE] = 1;
    for (k = 0; k < 8u; k++)
        hy_raw(t)[HY_O1 + HO_L0 + k] = 0;
    hy_raw(t)[HY_O1 + HO_L0 + 7] = HYW_SAW + 1;
    hy_raw(t)[HY_O1 + HO_SCAN] = 127;
    peak = hy_play(t, 57, 4096, 512);
    check("SCAN at the top reads the last slot", peak > 1000 * AUDIO_ONE);
    {   /* a sweep over smooth waves (the macro, a block at a time): no step bigger than the waves' own */
        static const uint8_t W[8] = {HYW_SINE, HYW_TRI, HYW_SINE, HYW_TRI, HYW_SINE, HYW_TRI, HYW_SINE, HYW_TRI};
        int32_t out[CTL], prev = 0, step = 0, stat = 0;
        t = hy_fresh(0);
        hy_raw(t)[HY_O1 + HO_MODE] = 1;
        hy_raw(t)[HY_O1 + HO_SCAN] = 0;
        for (k = 0; k < 8u; k++)
            hy_raw(t)[HY_O1 + HO_L0 + k] = (uint8_t)(W[k] + 1u);
        hy_raw(t)[HY_F1_CUT] = 127;
        trk_note_on(t, 57, 100);
        for (uint32_t b = 0; b < 600u; b++) {
            t->p[P_E0] = (int16_t)(b < 64u ? -63 : b < 64u + 254u ? -63 + (int32_t)(b - 64u) / 2 : 63);
            track_render(t, out, CTL);
            for (k = 0; k < CTL; k++) {
                int32_t d = abs(out[k] - prev);
                if (b >= 8u && b < 64u && d > stat) stat = d;
                if (b >= 64u && d > step) step = d;
                prev = out[k];
            }
        }
        hy_quiet(t);
        check("a SCAN sweep moves without clicks (largest step within 2x a still wave's)", stat > 0 && step < stat * 2);
    }
    /* ENV 1 -> SCAN 1: the list read moves on its own */
    t = hy_fresh(0);
    hy_raw(t)[HY_O1 + HO_MODE] = 1;
    hy_play(t, 57, 8192, 0);
    {
        uint64_t still = hy_hash(8192);
        t = hy_fresh(0);
        hy_raw(t)[HY_O1 + HO_MODE] = 1;
        hy_raw(t)[HY_E1_SC1] = HY_ENV0 + 60;
        hy_play(t, 57, 8192, 0);
        check("ENV 1 moves SCAN 1", hy_hash(8192) != still && hy_ed_active(t, HY_E1_SC1));
    }
    /* the macros: E4 (FILTER 1 CUT) darkens a bright saw */
    {
        double e_open = 0.0, e_shut = 0.0;
        t = hy_fresh(0);
        hy_raw(t)[HY_F1_CUT] = 100;
        hy_play(t, 45, 8192, 0);
        for (k = 4096; k < 8192; k++) e_open += fabs((double)hbuf[k] - hbuf[k - 1]);
        t = hy_fresh(0);
        hy_raw(t)[HY_F1_CUT] = 100;
        t->p[P_E4] = -63;
        hy_play(t, 45, 8192, 0);
        for (k = 4096; k < 8192; k++) e_shut += fabs((double)hbuf[k] - hbuf[k - 1]);
        check("macro CUT (E4) closes FILTER 1", e_shut < e_open * 0.5);
    }
    /* aliasing: a plain SAW, the filters open; energy off the harmonics */
    {
        static const uint8_t NOTE[3] = {48, 72, 84};
        static const double LIM[3] = {-70.0, -45.0, -40.0};
        ok = 1;
        for (k = 0; k < 3u; k++) {
            double db;
            t = hy_fresh(0);
            hy_play(t, NOTE[k], 16384u, 0);
            db = hy_alias_db(16384u - 4096u, 4096u, 440.0 * pow(2.0, (NOTE[k] - 69) / 12.0));
            printf("  SAW at MIDI %u: %.1f dB off the harmonics (limit %.0f)\n", NOTE[k], db, LIM[k]);
            ok &= db < LIM[k];
        }
        check("the oscillators alias little up the keyboard", ok);
    }
    /* a key struck in its own tail at another velocity: a new voice, the tail rings on (velocity is the ADSR's gain) */
    {
        int32_t out[CTL];
        uint32_t i, live = 0;
        t = hy_fresh(0);
        t->p[P_REL] = 90;
        t->p[P_VOICE] = V_POLY;
        trk_note_on(t, 57, 40);
        for (k = 0; k < 40u; k++) track_render(t, out, CTL);
        trk_note_off(t, 57);
        for (k = 0; k < 8u; k++) track_render(t, out, CTL);
        trk_note_on(t, 57, 127);
        for (i = 0; i < NVOICE; i++) live += t->v[i].active;
        check("a re-press takes a new voice while the tail rings on", live == 2u && ENG_HYDRA.rings_on && ENG_HYDRA.spare == 1u);
        hy_quiet(t);
    }
    /* the budget against what the Elite measured (docs/VALIDATION.md): the rule never promises less than it took */
    {
        static const struct { const char *name; uint8_t mode[4], lvl2, lvl3, scan; uint16_t c1; uint8_t units; } COST[] = {
            {"plain", {0}, 0, 0, 0, 155, 2}, {"O1+O2", {0}, 80, 0, 0, 180, 2}, {"O1+O2+O3", {0}, 80, 60, 0, 192, 2},
            {"PW", {MU_PW}, 0, 0, 0, 231, 3}, {"SQUEEZE", {MU_SQZ}, 0, 0, 0, 234, 3}, {"STACK", {MU_STACK}, 0, 0, 0, 237, 3},
            {"PW both", {MU_PW, 0, MU_PW}, 80, 0, 0, 322, 3}, {"STACK both", {MU_STACK, 0, MU_STACK}, 80, 0, 0, 333, 3},
            {"STACK+PW", {MU_STACK, MU_PW}, 0, 0, 0, 416, 4},
        };
        ok = 1;
        for (k = 0; k < NELEM(COST); k++) {
            hy_cost_t c;
            t = hy_fresh(0);
            for (uint32_t m = 0; m < 4u; m++) hy_raw(t)[HY_M1 + m * HM_N + HM_MODE] = COST[k].mode[m];
            hy_raw(t)[HY_LVL + 1] = COST[k].lvl2; hy_raw(t)[HY_LVL + 2] = COST[k].lvl3;
            c = hy_cost(hy_raw(t));
            if (c.c1 * 103u / 100u < COST[k].c1 || hy_units(t) != COST[k].units || 18u / hy_units(t) * c.c1 > HY_BLOCK_US) {
                printf("  %s: rule %u us, %u units; measured %u, wanted %u units\n", COST[k].name, c.c1, hy_units(t), COST[k].c1, COST[k].units);
                ok = 0;
            }
        }
        check("voice units follow the measured costs, a full budget within the block", ok);
        t = hy_fresh(0);
        ok = hy_cap(t) == 8u;
        for (uint32_t m = 0; m < 4u; m++) hy_raw(t)[HY_M1 + m * HM_N + HM_MODE] = MU_STACK;
        hy_raw(t)[HY_O1 + HO_MODE] = hy_raw(t)[HY_O2 + HO_MODE] = 1;
        hy_raw(t)[HY_M2 + HM_MODE] = MU_FM; hy_raw(t)[HY_M4 + HM_MODE] = MU_SYNC;
        hy_raw(t)[HY_LVL + 1] = 80;
        check("the heaviest patch takes most of the budget", hy_units(t) >= 9u && 18u / hy_units(t) * hy_cost(hy_raw(t)).c1 <= HY_BLOCK_US);
        check("every factory sound's full budget fits the block", ({
            int all = 1;
            for (pi = 1; pi < ENG_HYDRA.npresets; pi++) {
                t = hy_fresh(pi);
                all &= 18u / hy_units(t) * hy_cost(hy_raw(t)).c1 <= HY_BLOCK_US;
            }
            all;
        }));
    }
    /* a heavier sound loaded over eight notes: the surplus of their tails fades; nothing held is touched */
    {
        int32_t out[CTL];
        uint32_t live = 0, going = 0;
        t = hy_fresh(0);
        t->p[P_VOICE] = V_POLY;
        t->p[P_REL] = 120;
        for (k = 0; k < 8u; k++) trk_note_on(t, (uint32_t)(48 + k), 100);
        track_render(t, out, CTL);
        for (k = 0; k < NVOICE; k++) t->v[k].gate = 0;
        hy_preset_loaded(t, 4u);                       /* SUPERSTACK */
        track_render(t, out, CTL);
        for (k = 0; k < NVOICE; k++) { live += t->v[k].active && t->v[k].stage != 4u; going += t->v[k].active && t->v[k].stage == 4u; }
        check("a heavier sound over eight tails: what the block cannot carry fades", live == 6u && going == 2u);
        for (going = k = 0; k < NVOICE; k++) going += t->v[k].active && t->v[k].stage == 4u && hy_voice(hy_part(0), t, &t->v[k])->thin;
        check(".. without its mutants while it does", going == 2u);
        hy_quiet(t);
        t = hy_fresh(0);
        t->p[P_VOICE] = V_POLY;
        for (k = 0; k < 8u; k++) trk_note_on(t, (uint32_t)(48 + k), 100);
        track_render(t, out, CTL);
        hy_preset_loaded(t, 4u);
        track_render(t, out, CTL);
        for (going = k = 0; k < NVOICE; k++) going += t->v[k].active && t->v[k].stage == 4u;
        check(".. held notes are never touched", going == 0u);
        hy_quiet(t);
    }
    /* twice the rate: as many voices as the block has room for, across both parts; one note alone always has it */
    {
        static const uint8_t HIGH[8] = {72, 76, 79, 83, 86, 89, 93, 96};
        uint32_t n[3];
        for (uint32_t which = 0; which < 3u; which++) {
            int32_t out[CTL];
            t = hy_fresh(which == 0u ? 0u : 4u);       /* INIT; SUPERSTACK */
            if (which == 2u) { hy_raw(t)[HY_M3 + HM_MODE] = MU_OFF; hy_raw(t)[HY_LVL + 1] = 0; }   /* (one stack alone) */
            t->p[P_VOICE] = V_POLY;
            for (k = 0; k < 8u; k++) trk_note_on(t, HIGH[k], 100);
            track_render(t, out, CTL);
            n[which] = 0;
            for (k = 0; k < NVOICE; k++)
                if (t->v[k].active) n[which] += hy_voice(hy_part(0), t, &t->v[k])->os;
            printf("  high chord: %u voices at twice the rate\n", n[which]);
            hy_quiet(t);
        }
        check("twice the rate: all of a plain patch, one of SUPERSTACK, some of one stack", n[0] == 8u && n[1] == 1u && n[2] >= 3u && n[2] < 6u);
        t = hy_fresh(4);
        trk_note_on(t, 96, 100);
        { int32_t out[CTL]; track_render(t, out, CTL); }
        for (k = 0; k < NVOICE; k++)
            if (t->v[k].active) check("a high note alone runs at twice the rate", hy_voice(hy_part(0), t, &t->v[k])->os);
        hy_quiet(t);
    }
}

static const ui_field_t *stop_field(const ui_stop_t *st, uint32_t n, const char *obj, const char *page, uint32_t k)
{
    for (uint32_t i = 0; i < n; i++)
        if (!strcmp(st[i].obj, obj) && !strcmp(st[i].page, page))
            return &st[i].f[k];
    return 0;
}
static void the_panel(void)
{
    track_t *t = hy_fresh(1);
    const ui_field_t *f;
    uint8_t before;
    uint32_t i, k;
    int ok;
    check("the engine is one to pick, in the list after JUNO-106", eng_ok(ENGI_HYDRA) && !eng_ok(USER_NATIVE_J106) && !eng_ok(23u) && !eng_ok(24u) &&
          eng_vis(NENG_SHOWN - 1u) == ENGI_HYDRA && lib_engine(LIB_HY) == ENGI_HYDRA && eng_tint(ENGI_HYDRA) == 4u);
    f = stop_field(ST_EDIT_HY, NELEM(ST_EDIT_HY), "FILTER 1", "CUTOFF", 1);
    before = hy_raw(t)[HY_F1_CUT];
    check("FILTER 1's page edits the patch's CUTOFF", f && field_desc(f) && field_step(f, -5) && hy_raw(t)[HY_F1_CUT] < before);
    hy_raw(t)[HY_M1 + HM_MODE] = MU_FM;
    check("a mutant's AUX says what it does (FEEDBACK in FM-LIN, WINDOW in SYNC)",
          !strcmp(hy_desc(t, HY_M1 + HM_AUX)->label, "FEEDBACK") &&
          (hy_raw(t)[HY_M1 + HM_MODE] = MU_SYNC, !strcmp(hy_desc(t, HY_M1 + HM_AUX)->label, "WINDOW")));
    hy_raw(t)[HY_M1 + HM_MODE] = MU_OFF;
    hy_raw(t)[HY_O1 + HO_MODE] = 1;
    check("SCAN dims WAVE, lights the slots", !hy_ed_active(t, HY_O1 + HO_WAVE) && hy_ed_active(t, HY_O1 + HO_L0));
    hy_raw(t)[HY_O1 + HO_SEMI] = HY_SEMI0 - 12;
    check("signed fields show their value (SEMI -12)", hy_get(t, HY_O1 + HO_SEMI) == -12);
    /* every field of the place reads and every range holds a stored byte */
    ok = 1;
    for (i = 0; i < NELEM(ST_EDIT_HY) + NELEM(ST_ENV_HY); i++) {
        const ui_stop_t *st = i < NELEM(ST_EDIT_HY) ? &ST_EDIT_HY[i] : &ST_ENV_HY[i - NELEM(ST_EDIT_HY)];
        for (k = 0; k < 4u; k++) {
            const param_desc_t *d;
            if (st->f[k].kind != FK_HY) continue;
            d = field_desc(&st->f[k]);
            ok &= d != 0 && field_label(&st->f[k], d) != 0 && d->max - d->min <= 255;
        }
    }
    check("every field of HYDRA's pages reads", ok);
    /* every value of the patch has a page */
    ok = 1;
    for (i = 0; i < NELEM(HY_PANEL); i++) {
        int seen = 0;
        if (!HY_PANEL[i].label) continue;
        for (uint32_t s = 0; s < NELEM(ST_EDIT_HY) + NELEM(ST_ENV_HY); s++) {
            const ui_stop_t *st = s < NELEM(ST_EDIT_HY) ? &ST_EDIT_HY[s] : &ST_ENV_HY[s - NELEM(ST_EDIT_HY)];
            for (k = 0; k < 4u; k++) seen |= st->f[k].kind == FK_HY && st->f[k].id == i;
        }
        if (!seen) { printf("  byte %u (%s) has no page\n", i, HY_PANEL[i].label); ok = 0; }
    }
    check("every value of the patch has a page", ok);
    /* a list's name is shown whole (param_format keeps five letters, the wave lists hold seven) */
    {
        char val[16], unit[6];
        ui_field_t w = {FK_HY, HY_O1 + HO_WAVE, "WAVE", 0};
        hy_raw(t)[HY_O1 + HO_WAVE] = HYW_PULSE25;
        field_text(&w, field_desc(&w), val, unit);
        check("a wave's whole name is shown", !strcmp(val, "PULSE25"));
    }
    /* every byte the panel reaches keeps a valid patch at both ends of its range */
    ok = 1;
    for (i = 0; i < NELEM(HY_PANEL); i++) {
        hy_patch_t p;
        if (!HY_PANEL[i].label) continue;
        hy_patch_of(t);
        p = *hy_patch_of(t);
        p.raw[i] = (uint8_t)(HY_PANEL[i].max - HY_PANEL[i].min);
        ok &= hy_patch_valid(&p);
        if (i != HY_O3_WAVE && i != HY_O1 + HO_WAVE && i != HY_O2 + HO_WAVE) {
            p.raw[i]++;
            if (hy_patch_valid(&p)) { printf("  byte %u: past its range is valid\n", i); ok = 0; }
        }
    }
    check("the stored ranges are the panel's", ok);
    hy_quiet(t);
}

static void the_stores(void)
{
    track_t *t;
    hy_patch_t saved, init;
    uint32_t i;
    power_on();
    memset(nor, 255, sizeof nor);
    persist_boot();
    settings_init();
    t = hy_fresh(4);
    hy_raw(t)[HY_F1_CUT] = 33;
    t->p[P_E4] = 7;
    saved = *hy_patch_of(t);
    hy_patch_init(&init);
    check("store the patch in slot 300", !up_store_to(299, 0, "MY HYDRA") && up_used(299) && up_engine(299) == ENGI_HYDRA && native_used(ENGI_HYDRA, 299));
    {
        char name[13];
        hy_patch_t stored;
        memcpy(&stored, native_raw(ENGI_HYDRA, 299), sizeof stored);
        up_name(299, name);
        check(".. the name goes with it", !strcmp(name, "MY HYDRA") && !memcmp(stored.raw + HY_NAME, "MY HYDRA    ", HY_NAME_LEN));
    }
    hy_fresh(0);
    check(".. and loads back whole", !up_load_to(299, 0) && t->eng_req == ENGI_HYDRA && hy_raw(t)[HY_F1_CUT] == 33 && t->p[P_E4] == 7 &&
          !memcmp(hy_raw(t), saved.raw, HY_NAME) && user_of(t) == 299u);
    /* stars */
    check("an untagged row holds no HYDRA star", !favorite_has(ENGI_HYDRA, 3));
    check("a star", favorite_set(ENGI_HYDRA, 3, 1) && favorite_has(ENGI_HYDRA, 3) && !favorite_has(ENGI_HYDRA, 4) && favorites.factory[2][28] == 'H');
    check(".. and off", favorite_set(ENGI_HYDRA, 3, 0) && !favorite_has(ENGI_HYDRA, 3));
    check("no star past the row", !favorite_set(ENGI_HYDRA, HY_STAR_CAP, 1) && HY_STAR_CAP >= ENG_HYDRA.npresets);
    /* the last state */
    {
        static synth_state_t s;
        hy_raw(&trk[0])[HY_F1_RES] = 77;
        saved = *hy_patch_of(&trk[0]);
        state_capture(&s);
        check("the last state holds the patch", state_valid(&s) && !memcmp(&s.part[0].hy, &saved, sizeof saved));
        s.part[0].hy.raw[HY_F1_TYPE] = 200;
        s.sum = syn_hash(&s, sizeof s - 4u);
        check(".. a bad patch fails it", !state_valid(&s));
    }
    /* a state from before HYDRA (SDS2): the same parts with no patch; they gain INIT */
    {
        static synth_state_t now, old;
        synth_state2_t *o = (synth_state2_t *)&old;
        state_capture(&now);
        memset(&old, 0, sizeof old);
        o->magic = SYN_MAGIC2; o->size = sizeof *o;
        memcpy(o->g, now.g, sizeof o->g);
        o->octave = now.octave; o->sel = now.sel;
        for (i = 0; i < NPART; i++)
            memcpy(&o->part[i], &now.part[i], sizeof o->part[i]);
        o->part[1].preset = 3;
        o->sum = syn_hash(o, sizeof *o - 4u);
        {
            static uint8_t copy[sizeof(synth_state_t)];
            memcpy(copy, &old, sizeof old);
            check("a state from before HYDRA upgrades", state_upgrade(&old, sizeof *(synth_state2_t *)0) && state_valid(&old) &&
                  old.magic == SYN_MAGIC && old.part[0].engine == now.part[0].engine && old.part[1].preset == 3 &&
                  !memcmp(old.part[0].p, now.part[0].p, sizeof old.part[0].p) && !memcmp(&old.part[1].j106, &now.part[1].j106, sizeof now.part[1].j106) &&
                  !memcmp(&old.part[0].hy, &init, sizeof init) && !memcmp(&old.part[1].hy, &init, sizeof init));
            ((synth_state2_t *)copy)->sum ^= 1u;
            check(".. not a corrupt one", !state_upgrade((synth_state_t *)copy, sizeof(synth_state2_t)));
        }
    }
    /* performances, and their bank from before HYDRA */
    {
        hy_fresh(1);
        host_preset(&trk[1], ENGI_HYDRA, 2);
        saved = *hy_patch_of(&trk[1]);
        check("a performance holds both patches", !perf_store(4, "TWO HYDRAS") && perf_used(4));
        hy_fresh(0);
        host_preset(&trk[1], ENGI_FM6, 0);
        check(".. and plays them back", !perf_load(4) && trk[1].eng_req == ENGI_HYDRA && !memcmp(hy_patch_of(&trk[1]), &saved, sizeof saved));
        perf_bank_t *b = (perf_bank_t *)ED_BK_RAW;
        perf_bank2_t *o = (perf_bank2_t *)ED_BK_RAW;
        perf_bank_t cur;
        perf_bank_load(2);
        cur = perf_buf;
        memset(b, 0, sizeof *b);
        o->magic = PERF_MAGIC; o->rec_size = sizeof(perf_rec2_t);
        for (i = 0; i < PERF_PER_OBJ; i++) {
            memcpy(&o->r[i], &cur.r[i], __builtin_offsetof(perf_rec2_t, part));
            for (uint32_t k = 0; k < NPART; k++) memcpy(&o->r[i].part[k], &cur.r[i].part[k], sizeof(synth_part2_t));
        }
        {
            uint32_t len = sizeof(perf_bank2_t);
            perf_bank_upgrade(b, &len);
            check("a performance bank from before HYDRA upgrades", len == sizeof *b && perf_bank_valid(b, len) && !strcmp(b->r[0].name, "TWO HYDRAS") &&
                  b->r[0].part[1].engine == ENGI_HYDRA && !memcmp(&b->r[0].part[1].hy, &init, sizeof init) && !memcmp(b->r[0].part[0].p, cur.r[0].part[0].p, sizeof cur.r[0].part[0].p));
        }
        /* .. through flash, as a unit updated over it finds it */
        memset(ED_BK_RAW, 0, sizeof(perf_bank_t));
        o->magic = PERF_MAGIC; o->rec_size = sizeof(perf_rec2_t);
        for (i = 0; i < PERF_PER_OBJ; i++) {
            memcpy(&o->r[i], &cur.r[i], __builtin_offsetof(perf_rec2_t, part));
            for (uint32_t k = 0; k < NPART; k++) memcpy(&o->r[i].part[k], &cur.r[i].part[k], sizeof(synth_part2_t));
        }
        assert(!st_save(OBJ_PERF0 + 2u, o, sizeof *o));
        perf_boot();
        check(".. from flash: the slot stays, and loads", perf_used(4) && !perf_load(4) && trk[1].eng_req == ENGI_HYDRA);
    }
}

static void the_editor(void)
{
    track_t *t;
    power_on();
    t = hy_fresh(1);
    /* command 97 */
    {
        uint8_t get[] = {0, 0};
        request(ED_HYDRA, get, sizeof get);
        printf("  97 GET: %u bytes on the wire\n", wire_n);
        check("97 GET: the patch as nibble pairs", wire_n >= 3u + 2u * HY_BYTES && !memcmp(hy_raw(t) + HY_NAME, "WAVESCAN", 8));
        {
            int found = 0;                             /* (its name's nibbles, whatever the frame around them) */
            for (uint32_t i = 0; i + 2u * HY_BYTES < wire_n; i++) {
                uint32_t k = 0;
                while (k < HY_BYTES && (wire[i + 2u * k] | wire[i + 2u * k + 1u] << 4) == hy_raw(t)[k]) k++;
                found |= k == HY_BYTES;
            }
            check(".. every byte of it", found);
        }
    }
    {
        static uint8_t put[2u + 2u * HY_BYTES];
        hy_patch_t p;
        hy_factory(4u, &p);
        put[0] = 1; put[1] = 0;
        for (uint32_t i = 0; i < HY_BYTES; i++) { put[2u + 2u * i] = p.raw[i] & 15u; put[3u + 2u * i] = p.raw[i] >> 4; }
        request(ED_HYDRA, put, sizeof put);
        check("97 PUT auditions a patch", !memcmp(hy_patch_of(t), &p, sizeof p) && t->eng_req == ENGI_HYDRA);
        put[2u + 2u * HY_F1_TYPE] = 15; put[3u + 2u * HY_F1_TYPE] = 15;
        request(ED_HYDRA, put, sizeof put);
        check(".. refuses a bad one", !memcmp(hy_patch_of(t), &p, sizeof p));
    }
    {
        uint8_t edit[] = {2, 0, HY_O1 + HO_KEY, 200u & 127u, 200u >> 7};
        request(ED_HYDRA, edit, sizeof edit);
        check("97 edits a byte past 127", hy_raw(t)[HY_O1 + HO_KEY] == 200u);
        uint8_t over[] = {2, 0, HY_F1_TYPE, 100, 0};
        request(ED_HYDRA, over, sizeof over);
        check(".. refuses one past its range", hy_raw(t)[HY_F1_TYPE] < F1_N);
        uint8_t reserved[] = {2, 0, HY_RSV, 65, 0}, before = hy_raw(t)[HY_RSV];
        request(ED_HYDRA, reserved, sizeof reserved);
        check(".. and what is no field", hy_raw(t)[HY_RSV] == before);
        uint8_t name[] = {3, 0, 'G', 'L', 'A', 'S', 'S'};
        request(ED_HYDRA, name, sizeof name);
        check("97 names the patch", !memcmp(hy_raw(t) + HY_NAME, "GLASS       ", HY_NAME_LEN));
    }
    /* the native message: send, decode, import */
    {
        uint8_t frame[HY_FRAME];
        hy_patch_t back;
        uint32_t n = hy_patch_encode(hy_patch_of(t), frame, sizeof frame);
        check("the native message round-trips", n == HY_FRAME && hy_patch_decode(&back, frame + 1, n - 2u) && !memcmp(&back, hy_patch_of(t), sizeof back));
        for (uint32_t i = 1; i < n - 1u; i++) assert(frame[i] < 128u);
        host_preset(t, ENGI_FM6, 0);
        check(".. and imports", hy_native_frame(frame + 1, n - 2u) && TSEL->eng_req == ENGI_HYDRA && sx_staged() && sx_stage.e == ENGI_HYDRA && !memcmp(hy_patch_of(TSEL), &back, sizeof back));
        frame[6] = 15; frame[5] = 15;
        check(".. a bad one is refused as HYDRA's", hy_native_frame(frame + 1, n - 2u));
        sx_drop();
    }
    /* INFO names the engine */
    {
        int found = 0;
        request(ED_INFO, 0, 0);
        for (uint32_t i = 0; i + 5u < wire_n; i++)
            if (wire[i] == 0x48u && wire[i + 1u] == 1u && wire[i + 2u] == ENGI_HYDRA && wire[i + 3u] == USER_NATIVE_HYDRA && wire[i + 4u] == ED_HYDRA)
                found = 1;
        check("INFO announces HYDRA", found);
    }
    hy_quiet(t);
}

int main(void)
{
    power_on();
    the_state();
    the_sound();
    the_panel();
    the_stores();
    the_editor();
    printf("%s\n", bad ? "HYDRA test FAILED" : "HYDRA test passed");
    return bad != 0;
}
