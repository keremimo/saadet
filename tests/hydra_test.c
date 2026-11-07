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
static int ota_wire_send(const uint8_t *p, uint32_t n) { assert(n <= sizeof wire); memcpy(wire, p, n); wire_n = n; return 0; }
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
    /* the macros: INIT's third (CUTOFF) darkens a bright saw */
    {
        double e_open = 0.0, e_shut = 0.0;
        t = hy_fresh(0);
        hy_raw(t)[HY_F1_CUT] = 100;
        hy_play(t, 45, 8192, 0);
        for (k = 4096; k < 8192; k++) e_open += fabs((double)hbuf[k] - hbuf[k - 1]);
        t = hy_fresh(0);
        hy_raw(t)[HY_F1_CUT] = 100;
        t->p[P_E2] = -64;
        hy_play(t, 45, 8192, 0);
        for (k = 4096; k < 8192; k++) e_shut += fabs((double)hbuf[k] - hbuf[k - 1]);
        check("a macro routed to CUTOFF (INIT's third) closes FILTER 1", e_shut < e_open * 0.5);
    }
    /* the macros as the patch routes them: a depth of a destination's range, several adding up, clamped */
    {
        uint8_t r[HY_BYTES], eff[HY_BYTES];
        int16_t e[HY_MACROS] = {0};
        t = hy_fresh(0);
        memcpy(r, hy_raw(t), HY_BYTES);
        memset(r + HY_MNAME, 0, HY_MACROS + 2u * HY_MSLOTS);
        r[HY_F1_CUT] = 60; r[HY_F1_RES] = 20;
        r[HY_MA] = HY_SLOT(0, HT_CUT); r[HY_MA + 1] = HY_DEPTH0 + 127;
        r[HY_MA + 2] = HY_SLOT(0, HT_RES); r[HY_MA + 3] = HY_DEPTH0 - 64;
        r[HY_MA + 4] = HY_SLOT(5, HT_CUT); r[HY_MA + 5] = HY_DEPTH0 + 64;
        hy_macros(r, e, eff);
        ok = !memcmp(r, eff, HY_BYTES);
        e[0] = 16;
        hy_macros(r, e, eff);
        ok &= eff[HY_F1_CUT] == 60 + 32 && eff[HY_F1_RES] == 20 - 16;   /* (16/64 of 127; of -64/127 of 127) */
        e[0] = 63; e[5] = 63;
        hy_macros(r, e, eff);
        ok &= eff[HY_F1_CUT] == 127;
        e[0] = -64; e[5] = 0;
        hy_macros(r, e, eff);
        ok &= eff[HY_F1_CUT] == 0 && eff[HY_F1_RES] == 20 + 64;
        check("a macro moves each of its destinations by its depth, several adding up, clamped; at 0 the sound as stored", ok);
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
            {"plain", {0}, 0, 0, 0, 155, 5}, {"O1+O2", {0}, 80, 0, 0, 180, 6}, {"O1+O2+O3", {0}, 80, 60, 0, 192, 7},
            {"PW", {MU_PW}, 0, 0, 0, 231, 8}, {"SQUEEZE", {MU_SQZ}, 0, 0, 0, 234, 8}, {"STACK", {MU_STACK}, 0, 0, 0, 237, 8},
            {"PW both", {MU_PW, 0, MU_PW}, 80, 0, 0, 322, 11}, {"STACK both", {MU_STACK, 0, MU_STACK}, 80, 0, 0, 333, 11},
            {"STACK+PW", {MU_STACK, MU_PW}, 0, 0, 0, 416, 14},     /* (units: steps, VQ a unit; eight voices up to 8 steps) */
            {"STACK+FM", {MU_STACK, MU_FM}, 0, 0, 0, 455, 15},     /* (HIT C2 ! LA less its OSC 2, OSC 3 and FILTER 2) */
        };
        ok = 1;
        for (k = 0; k < NELEM(COST); k++) {
            hy_cost_t c;
            t = hy_fresh(0);
            for (uint32_t m = 0; m < 4u; m++) hy_raw(t)[HY_M1 + m * HM_N + HM_MODE] = COST[k].mode[m];
            hy_raw(t)[HY_LVL + 1] = COST[k].lvl2; hy_raw(t)[HY_LVL + 2] = COST[k].lvl3;
            c = hy_cost(hy_raw(t));
            if (c.c1 * 103u / 100u < COST[k].c1 || hy_units(t) != COST[k].units || HY_STEPS / hy_units(t) * c.c1 > HY_BLOCK_US) {
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
        check("few heavy voices are bound by the cores' pairing: 1079 us a share plays one, 567 three, a plain eight",
              hy_cap_of(1079u) == 1u && hy_cap_of(567u) == 3u && hy_cap_of(155u) == 8u && hy_cap_of(283u) == 8u &&
              hy_units_of(hy_raw(t), (hy_cost_t){1079u, 0u}) == (HY_ALONE(1079u) * HY_STEPS + HY_BLOCK_US - 1u) / HY_BLOCK_US);
        check("the heaviest patch plays two voices at the most", HY_STEPS / hy_units(t) <= 2u && HY_STEPS / hy_units(t) * hy_cost_most(hy_raw(t)).c1 <= HY_BLOCK_US);
        {                                              /* LAYER with it on both parts: one voice is more than a part's half */
            track_t *b = &trk[1];
            host_preset(b, ENGI_HYDRA, 0);
            memcpy(hy_raw(b), hy_raw(t), HY_BYTES);
            song.g[G_PMODE] = PT_LAYER;
            t->p[P_VOICE] = b->p[P_VOICE] = V_POLY;
            trk_note_on(t, 48, 100);
            trk_note_on(b, 48, 100);
            check("LAYER with the heaviest patch on both: part B's note waits rather than take the CPU past what it carries",
                  voices_busy() <= voice_ceiling() && b->wait_n == 1u && t->v[0].active);
            trk_note_off(t, 48); trk_note_off(b, 48);
            check(".. and is forgotten at its key-up", b->wait_n == 0u);
            song.g[G_PMODE] = PT_ONE;
            hy_quiet(b);
            hy_quiet(t);
            t = hy_fresh(0);
            for (uint32_t m = 0; m < 4u; m++) hy_raw(t)[HY_M1 + m * HM_N + HM_MODE] = MU_STACK;
            hy_raw(t)[HY_O1 + HO_MODE] = hy_raw(t)[HY_O2 + HO_MODE] = 1;
            hy_raw(t)[HY_M2 + HM_MODE] = MU_FM; hy_raw(t)[HY_M4 + HM_MODE] = MU_SYNC;
            hy_raw(t)[HY_LVL + 1] = 80;
        }
        check("every factory sound's full budget fits the block", ({
            int all = 1;
            for (pi = 1; pi < ENG_HYDRA.npresets; pi++) {
                t = hy_fresh(pi);
                all &= HY_STEPS / hy_units(t) * hy_cost_most(hy_raw(t)).c1 <= HY_BLOCK_US;
            }
            all;
        }));
        /* a macro that can open what costs is costed open: OSC 3's level, FILTER 2 */
        t = hy_fresh(0);
        {
            uint8_t *r = hy_raw(t);
            hy_cost_t c0 = hy_cost(r), c;
            memset(r + HY_MA, 0, 2u * HY_MSLOTS);
            r[HY_MA] = HY_SLOT(3, HT_OSC3); r[HY_MA + 1] = HY_DEPTH0 - 20;
            c = hy_cost_most(r);
            ok = c.c1 == c0.c1 + 12u && hy_cost(r).c1 == c0.c1;
            r[HY_MA + 2] = HY_SLOT(7, HT_F2CUT); r[HY_MA + 3] = HY_DEPTH0 + 1;
            ok &= hy_cost_most(r).c1 == c0.c1 + 12u + 17u;
            check("a macro that can open OSC 3 or FILTER 2 is budgeted with them open", ok);
        }
        /* no turn of any macro, the matrix's included, costs more than the budget took: every factory sound, every macro
         * at either end */
        ok = 1;
        for (pi = 0; pi < ENG_HYDRA.npresets; pi++) {
            uint8_t eff[HY_BYTES];
            hy_cost_t most;
            t = hy_fresh(pi);
            most = hy_cost_most(hy_raw(t));
            for (k = 0; k < 256u; k++) {
                int16_t e[HY_MACROS];
                for (uint32_t m = 0; m < HY_MACROS; m++) e[m] = (int16_t)((k >> m) & 1u ? 63 : -64);
                hy_macros(hy_raw(t), e, eff);
                if (hy_cost(eff).c1 > most.c1 || hy_cost(eff).os > most.os) { printf("  preset %u, macros %02x: over\n", pi, k); ok = 0; }
            }
            ok &= hy_cost_most(hy_raw(t)).c1 == hy_cost(hy_raw(t)).c1;   /* (the factory's macros open nothing that costs) */
        }
        check("every macro at either end stays within the budget; the factory's open nothing that costs", ok);
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
    /* twice the rate: what the block has left once the part plays all it can at the rate, across both parts, each voice's
     * extra at its time alone. A note alone has it only where that leaves room: a lead (LEGATO) does, a heavy chord does not */
    {
        static const uint8_t HIGH[8] = {72, 76, 79, 83, 86, 89, 93, 96};
        uint32_t n[3];
        static const uint8_t WHICH[3] = {0u, 5u, 4u};  /* INIT; FM BELL; SUPERSTACK */
        for (uint32_t which = 0; which < 3u; which++) {
            int32_t out[CTL];
            t = hy_fresh(WHICH[which]);
            t->p[P_VOICE] = V_POLY;
            for (k = 0; k < 8u; k++) trk_note_on(t, HIGH[k], 100);
            track_render(t, out, CTL);
            n[which] = 0;
            for (k = 0; k < NVOICE; k++)
                if (t->v[k].active) n[which] += hy_voice(hy_part(0), t, &t->v[k])->os;
            printf("  high chord: %u voices at twice the rate\n", n[which]);
            hy_quiet(t);
        }
        check("twice the rate: all of a plain patch, one of FM BELL, none of SUPERSTACK (its six at the rate take the rest)",
              n[0] == 8u && n[1] == 1u && n[2] == 0u);
        for (uint32_t which = 0; which < 2u; which++) {
            uint32_t os = 0;
            t = hy_fresh(which ? 4u : 3u);             /* SYNC LEAD (LEGATO); SUPERSTACK */
            if (which) {                               /* (the heaviest patch, as above: it plays one voice) */
                for (uint32_t m = 0; m < 4u; m++) hy_raw(t)[HY_M1 + m * HM_N + HM_MODE] = MU_STACK;
                hy_raw(t)[HY_O1 + HO_MODE] = hy_raw(t)[HY_O2 + HO_MODE] = 1;
                hy_raw(t)[HY_M2 + HM_MODE] = MU_FM; hy_raw(t)[HY_M4 + HM_MODE] = MU_SYNC;
                hy_raw(t)[HY_LVL + 1] = 80;
            }
            trk_note_on(t, 96, 100);
            { int32_t out[CTL]; track_render(t, out, CTL); }
            for (k = 0; k < NVOICE; k++)
                if (t->v[k].active) os += hy_voice(hy_part(0), t, &t->v[k])->os;
            check(which ? "the heaviest patch's high note alone stays at the rate" : "a lead's high note runs at twice the rate",
                  os == (which ? 0u : 1u));
            hy_quiet(t);
        }
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
#define HY_STOPS (NELEM(ST_EDIT_HY) + NELEM(ST_ENV_HY) + NELEM(ST_LFO_HY))
#define HY_STOP(i) ((i) < NELEM(ST_EDIT_HY) ? &ST_EDIT_HY[i] : (i) < NELEM(ST_EDIT_HY) + NELEM(ST_ENV_HY) ? \
                    &ST_ENV_HY[(i) - NELEM(ST_EDIT_HY)] : &ST_LFO_HY[(i) - NELEM(ST_EDIT_HY) - NELEM(ST_ENV_HY)])
    ok = 1;
    for (i = 0; i < HY_STOPS; i++) {
        const ui_stop_t *st = HY_STOP(i);
        for (k = 0; k < 4u; k++) {
            const param_desc_t *d;
            if (st->f[k].kind != FK_HY) continue;
            d = field_desc(&st->f[k]);
            ok &= d != 0 && field_label(&st->f[k], d) != 0 && d->max - d->min <= 255;
        }
    }
    check("every field of HYDRA's pages reads", ok);
    /* every value of the patch has a page (the macros' slots: through their macros' destinations) */
    ok = 1;
    for (i = 0; i < NELEM(HY_PANEL); i++) {
        int seen = i >= HY_MA && i < HY_SPARE;
        if (!HY_PANEL[i].label) continue;
        for (uint32_t s = 0; s < HY_STOPS; s++) {
            const ui_stop_t *st = HY_STOP(s);
            for (k = 0; k < 4u; k++) seen |= st->f[k].kind == FK_HY && st->f[k].id == i;
        }
        if (!seen) { printf("  byte %u (%s) has no page\n", i, HY_PANEL[i].label); ok = 0; }
    }
    for (uint32_t m = 0; m < HY_MACROS; m++)
        for (uint32_t j = 0; j < HY_MDESTS; j++) {
            int seen = 0;
            for (uint32_t s = 0; s < HY_STOPS; s++)
                for (k = 0; k < 4u; k++) seen |= HY_STOP(s)->f[k].kind == FK_HY && HY_STOP(s)->f[k].id == HYV_DEST(m, j);
            ok &= seen;
        }
    check("every value of the patch has a page", ok);
    /* the macros' pages: a destination added takes a free slot, NONE gives it back; a full pool lights no new one */
    {
        const uint8_t *r;
        ui_field_t d2 = {FK_HY, HYV_DEST(0, 1), 0, 0}, dp2 = {FK_HY, HYV_DEPTH(0, 1), 0, 0}, d3 = {FK_HY, HYV_DEST(0, 2), 0, 0};
        uint32_t free0 = 0;
        t = hy_fresh(0);
        r = hy_raw(t);
        for (k = 0; k < HY_MSLOTS; k++) free0 += !HY_SLOT_TGT(r[HY_MA + 2u * k]);
        ok = hy_get(t, HYV_DEST(0, 0)) == (int32_t)HT_SCAN1 && hy_get(t, HYV_DEPTH(0, 0)) == 64 && !hy_ed_active(t, HYV_DEST(0, 2)) &&
             hy_ed_active(t, HYV_DEST(0, 1)) && !hy_ed_active(t, HYV_DEPTH(0, 1)) && !strcmp(hy_macro_desc(t, 2)->label, "CUTOFF");
        hy_edit_value(t, HYV_DEST(0, 1), HT_DRIVE);
        ok &= hy_get(t, HYV_DEST(0, 1)) == (int32_t)HT_DRIVE && hy_get(t, HYV_DEPTH(0, 1)) == 64 && hy_ed_active(t, HYV_DEPTH(0, 1)) &&
              hy_ed_active(t, HYV_DEST(0, 2)) && hy_slots_used(r, 0) == 2u;
        field_step(&dp2, -10);
        ok &= hy_get(t, HYV_DEPTH(0, 1)) == 54 && field_get(&d2) == (int32_t)HT_DRIVE && field_get(&d3) == 0;
        hy_edit_value(t, HYV_DEST(0, 0), HT_NONE);
        ok &= hy_slots_used(r, 0) == 1u && hy_get(t, HYV_DEST(0, 0)) == (int32_t)HT_DRIVE;
        for (uint32_t m = 0; m < HY_MACROS; m++)       /* (the pool fills before every page does: 14 of 24) */
            while (hy_slots_used(r, m) < HY_MDESTS && hy_slot_free(r) >= 0)
                hy_edit_value(t, HYV_DEST(m, hy_slots_used(r, m)), HT_LEVEL);
        ok &= free0 == 6u && hy_slot_free(r) < 0 && hy_slots_used(r, 7) == 1u && !hy_ed_active(t, HYV_DEST(7, 1)) &&
              hy_patch_valid(hy_patch_of(t));
        hy_edit_value(t, HYV_DEST(7, 1), HT_CUT);
        ok &= hy_slots_used(r, 7) == 1u;
        hy_raw(t)[HY_MNAME + 2] = HN_BRIGHT;
        ok &= !strcmp(hy_macro_desc(t, 2)->label, "BRIGHT") && !strcmp(track_desc(t, P_E2)->label, "BRIGHT");
        check("a macro's page: NAME, destinations from the pool (a free slot, NONE back), the knobs' label its name", ok);
    }
    /* every factory sound names all eight and routes each somewhere */
    ok = 1;
    for (uint32_t pi = 1; pi < ENG_HYDRA.npresets; pi++) {
        t = hy_fresh(pi);
        for (uint32_t m = 0; m < HY_MACROS; m++)
            if (!hy_slots_used(hy_raw(t), m) || !hy_raw(t)[HY_MNAME + m]) { printf("  preset %u: macro %u\n", pi, m + 1u); ok = 0; }
    }
    check("every factory sound's eight macros have a name and a destination", ok);
    /* CC74 / CC71: the third and the fourth */
    t = hy_fresh(1);
    midi_parameter(t, 74, 127);
    midi_parameter(t, 71, 0);
    check("CC74 and CC71 turn macros 3 and 4", t->p[P_E2] == 63 && t->p[P_E3] == -64);
    t = hy_fresh(1);
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
        int wave = i == HY_O3_WAVE || i == HY_O1 + HO_WAVE || i == HY_O2 + HO_WAVE ||
                   (i >= HY_O1 + HO_L0 && i < HY_O1 + HO_L0 + 8u) || (i >= HY_O2 + HO_L0 && i < HY_O2 + HO_L0 + 8u);
        if (!wave && HY_PANEL[i].max - HY_PANEL[i].min < 255) {   /* (a wave's: the ids reserved past the bank, below) */
            p.raw[i]++;
            if (hy_patch_valid(&p)) { printf("  byte %u: past its range is valid\n", i); ok = 0; }
        }
    }
    check("the stored ranges are the panel's", ok);
    {   /* a wave field stores every id the format reserves (hydra_patch.h: the user waves up to U64), the panel reaches the bank */
        hy_patch_t p = *hy_patch_of(t);
        int r = HY_PANEL[HY_O1 + HO_L0].max == (int)(HY_USER0 + HY_UWAVES - 1u) &&
                HY_PANEL[HY_O1 + HO_WAVE].max == (int)(HY_USER0 + HY_UWAVES - 2u);
        p.raw[HY_O1 + HO_L0 + 3] = (uint8_t)HY_ID_LAST; p.raw[HY_O2 + HO_WAVE] = (uint8_t)(HY_ID_LAST - 1u); p.raw[HY_O3_WAVE] = (uint8_t)(HY_ID_LAST - 1u);
        r &= hy_patch_valid(&p);
        p.raw[HY_O1 + HO_L0 + 3] = (uint8_t)(HY_ID_LAST + 1u);
        r &= !hy_patch_valid(&p);
        p.raw[HY_O1 + HO_L0 + 3] = 0; p.raw[HY_O3_WAVE] = (uint8_t)HY_ID_LAST;
        r &= !hy_patch_valid(&p);
        check("wave fields store the reserved user ids, the panel reaches the bank's", r);
    }
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
        uint8_t reserved[] = {2, 0, HY_SPARE, 65, 0}, before = hy_raw(t)[HY_SPARE];
        request(ED_HYDRA, reserved, sizeof reserved);
        check(".. and what is no field", hy_raw(t)[HY_SPARE] == before);
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
    {   /* the editor's parameter list names the macros as the sound does */
        uint8_t q[2] = {0, P_E2};
        t = hy_fresh(1);
        request(ED_DESC, q, sizeof q);
        check("the editor's E3 is the sound's third macro, by its name (WAVESCAN: BRIGHT)", wire_n > 16u && memmem(wire, wire_n, "BRIGHT", 6) != 0);
    }
    hy_quiet(t);
}

/* a wave's points as command 98 carries them: int16 low byte first, seven-to-eight packed (editor_backup.c ed_bk_pack) */
static uint32_t hy_pack(const int16_t *w, uint8_t *out)
{
    uint8_t b[2u * HY_PTS];
    uint32_t i, n = 0, at;
    for (i = 0; i < HY_PTS; i++) { b[2u * i] = (uint8_t)((uint16_t)w[i] & 255u); b[2u * i + 1u] = (uint8_t)((uint16_t)w[i] >> 8); }
    for (at = 0; at < sizeof b; at += 7u) {
        uint32_t k = sizeof b - at > 7u ? 7u : sizeof b - at, m = 0;
        for (i = 0; i < k; i++) m |= (uint32_t)(b[at + i] >> 7) << i;
        out[n++] = (uint8_t)m;
        for (i = 0; i < k; i++) out[n++] = b[at + i] & 127u;
    }
    return n;
}
static int hy_row_is(const hy_part_t *P, uint32_t row, const int16_t *w) { return !memcmp(P->wave[row], w, HY_PTS * sizeof *w); }
/* a reply's payload (after F0 7D 46 4C 98, before F7), for web/test_synth.mjs */
static void hy_fixture(const char *path)
{
    FILE *f = fopen(path, "wb");
    if (f) { fwrite(wire + 5, 1, wire_n - 6u, f); fclose(f); }
}

/* the user waves (hydra_bank.h): a free bank is sines named USER n; a slot plays the bank's points exactly, as a factory wave
 * holding the same points sounds; a change reaches the part at the next block and only its user waves are copied again; an
 * id past the bank is silent; command 98 lists, gets, puts, stores, reverts and clears; the bank survives a power cycle, a
 * torn write keeps flash's copy, and the at-rest autosave waits while a note sounds */
static void the_bank(void)
{
    static int16_t w[HY_PTS], v[HY_PTS];
    static uint8_t put[2u + HY_UNAME + 600u];
    track_t *t;
    hy_part_t *P;
    uint64_t h1, h2;
    uint32_t i, r0, n;
    int32_t out[CTL], pk;
    int ok;
    power_on();
    memset(nor, 255, sizeof nor);
    hy_bank_boot();
    ok = 1;
    for (i = 0; i < HY_UWAVES; i++)
        ok &= !hy_uwave(i)->used && !memcmp(hy_uwave(i)->pts, HY_WAVES[HYW_SINE], sizeof w);
    check("a free bank: fourteen sines", ok && !strcmp(hy_uwave(0)->name, "USER 1") && !strcmp(hy_uwave(13)->name, "USER 14"));

    hy_uset(0, HY_WAVES[HYW_BELL], "MYBELL", 1);       /* the same points as a factory wave: the same sound */
    t = hy_fresh(0);
    hy_raw(t)[HY_O1 + HO_WAVE] = HYW_BELL;
    pk = hy_play(t, 60, 8192, 0); h1 = hy_hash(8192);
    t = hy_fresh(0);
    hy_raw(t)[HY_O1 + HO_WAVE] = (uint8_t)(HY_USER0 - 1u);   /* (WAVE holds the id - 1: U1) */
    hy_play(t, 60, 8192, 0); h2 = hy_hash(8192);
    check("a user wave sounds exactly as the factory wave whose points it holds", h1 == h2 && pk > 1000 * AUDIO_ONE);
    t = hy_fresh(0);
    hy_raw(t)[HY_O1 + HO_WAVE] = (uint8_t)(HY_USER0 + HY_UWAVES - 1u);   /* (one past the bank) */
    pk = hy_play(t, 60, 8192, 0);
    check("a wave past the bank is silent", pk == 0);

    for (i = 0; i < HY_PTS; i++) w[i] = (int16_t)((int32_t)((i * 977u) % 50000u) - 25000);
    hy_uset(4, w, "MINE", 1);
    t = hy_fresh(0);
    hy_raw(t)[HY_O1 + HO_MODE] = 1;                    /* SCAN: U5, SAW, U5 */
    hy_raw(t)[HY_O1 + HO_L0] = (uint8_t)(HY_USER0 + 4u);
    hy_raw(t)[HY_O1 + HO_L0 + 1] = HYW_SAW + 1;
    hy_raw(t)[HY_O1 + HO_L0 + 2] = (uint8_t)(HY_USER0 + 4u);
    trk_note_on(t, 57, 100);
    track_render(t, out, CTL);
    track_render(t, out, CTL);
    P = hy_part(0);
    r0 = P->b.row0[0];
    check("a slot's user wave is copied whole from the bank", P->b.scanning[0] && hy_row_is(P, r0, w) && hy_row_is(P, r0 + 2u, w) &&
                                                            hy_row_is(P, r0 + 1u, HY_WAVES[HYW_SAW]));
    P->wave[r0 + 1u][0] ^= 0x5555;                     /* (a mark a copy of SAW would undo) */
    memcpy(v, w, sizeof v); v[0] = 1234;
    hy_uset(4, v, 0, 1);
    track_render(t, out, CTL);
    P = hy_part(0);
    check("a change to the bank reaches the part at the next block, only its user waves copied again",
          hy_row_is(P, r0, v) && hy_row_is(P, r0 + 2u, v) && P->wave[r0 + 1u][0] == (int16_t)(HY_WAVES[HYW_SAW][0] ^ 0x5555));
    P->wave[r0 + 1u][0] ^= 0x5555;
    {
        char val[16], unit[6];
        ui_field_t f = {FK_HY, HY_O1 + HO_L0, "SLOT 1", 0};
        field_text(&f, field_desc(&f), val, unit);
        check("the panel names a user wave by the bank's name", !strcmp(val, "MINE"));
    }
    trk_note_off(t, 57);
    hy_quiet(t);

    /* command 98 */
    {
        uint8_t list[] = {0};
        request(ED_HYWAVE, list, sizeof list);
        n = 5u + 5u + HY_UWAVES * (1u + HY_UNAME + 5u) + 1u;
        hy_fixture("build/host/hywave-list.bin");
        check("98 LIST: every user wave, written, named, its points' CRC",
              wire_n == n && !wire[6] && wire[8] == HY_UWAVES && wire[9] == HY_USER0 && wire[10] == 1u && !memcmp(wire + 11, "MYBELL", 6) &&
              ed_bk_r32(wire + 18) == st_crc32(HY_WAVES[HYW_BELL], sizeof w) && wire[10u + 13u] == 0u);
    }
    {
        uint8_t get[] = {1, HYW_SAW + 1};
        static uint8_t b[2u * HY_PTS];
        request(ED_HYWAVE, get, sizeof get);
        hy_fixture("build/host/hywave-get-saw.bin");
        ok = wire_n == 5u + 3u + HY_UNAME + 586u + 1u && !wire[6] && !memcmp(wire + 8, "SAW\0\0\0\0", 7) &&
             ed_unpack7(wire + 8u + HY_UNAME, 586u, b, sizeof b) == sizeof b;
        for (i = 0; ok && i < HY_PTS; i++)
            ok = (int16_t)(b[2u * i] | b[2u * i + 1u] << 8) == HY_WAVES[HYW_SAW][i];
        check("98 GET: a factory wave's name and every point", ok);
        get[1] = 0; request(ED_HYWAVE, get, sizeof get); ok = wire[6] == 1u;
        get[1] = (uint8_t)(HY_USER0 + HY_UWAVES); request(ED_HYWAVE, get, sizeof get); ok &= wire[6] == 1u;
        check(".. no wave 0, none past the bank", ok);
    }
    {
        for (i = 0; i < HY_PTS; i++) w[i] = (int16_t)(i & 1u ? -32768 : 32767);
        put[0] = 2; put[1] = (uint8_t)(HY_USER0 + 2u);
        memset(put + 2, 0, HY_UNAME); memcpy(put + 2, "ZAP", 3);
        n = 2u + HY_UNAME + hy_pack(w, put + 2u + HY_UNAME);
        request(ED_HYWAVE, put, n);
        check("98 PUT: a user wave heard at once, marked to keep", !wire[6] && hy_uwave(2)->used && !strcmp(hy_uwave(2)->name, "ZAP") &&
                                                                !memcmp(hy_uwave(2)->pts, w, sizeof w) && (hy_udirty & 4u));
        put[1] = HYW_SAW + 1; request(ED_HYWAVE, put, n); ok = wire[6] == 1u;
        put[1] = (uint8_t)(HY_USER0 + 2u); put[2] = 7; request(ED_HYWAVE, put, n); ok &= wire[6] == 1u; put[2] = 'Z';
        request(ED_HYWAVE, put, n - 1u); ok &= wire[6] == 1u;
        check(".. not a factory wave, nor an unprintable name or a short wave", ok && !memcmp(hy_uwave(2)->pts, w, sizeof w));
    }
    {
        uint8_t store[] = {3}, revert[] = {4, (uint8_t)(HY_USER0 + 2u)}, clear[] = {5, (uint8_t)(HY_USER0 + 2u)};
        request(ED_HYWAVE, store, sizeof store);
        ok = !wire[6] && !hy_udirty;
        memset(hy_ubank, 0, sizeof hy_ubank);          /* (a power cycle) */
        hy_bank_boot();
        check("98 STORE: the bank survives a power cycle", ok && hy_uwave(2)->used && !strcmp(hy_uwave(2)->name, "ZAP") &&
                                                            !memcmp(hy_uwave(2)->pts, w, sizeof w) && !strcmp(hy_uwave(4)->name, "MINE"));
        hy_uset(2, HY_WAVES[HYW_TRI], "TRI", 1);
        fail_after = 3;                                /* (the write torn part-way) */
        ok = hy_bank_store() != 0 && (hy_udirty & 4u);
        fail_after = -1;
        hy_bank_boot();
        check("a torn write keeps the bank flash held", ok && !strcmp(hy_uwave(2)->name, "ZAP") && !memcmp(hy_uwave(2)->pts, w, sizeof w));
        hy_uset(2, HY_WAVES[HYW_TRI], "TRI", 1);
        request(ED_HYWAVE, revert, sizeof revert);
        check("98 REVERT: back to flash's", !wire[6] && !strcmp(hy_uwave(2)->name, "ZAP") && !memcmp(hy_uwave(2)->pts, w, sizeof w) &&
                                            !(hy_udirty & 4u));
        request(ED_HYWAVE, clear, sizeof clear);
        check("98 CLEAR: free again", !wire[6] && !hy_uwave(2)->used && !strcmp(hy_uwave(2)->name, "USER 3") &&
                                      !memcmp(hy_uwave(2)->pts, HY_WAVES[HYW_SINE], sizeof w) && hy_ufree() == 1);
    }
    {   /* the at-rest autosave: not while a note sounds */
        uint32_t e;
        hy_uset(6, HY_WAVES[HYW_GLASS], "GLASS", 1);
        t = hy_fresh(0);
        trk_note_on(t, 60, 100);
        track_render(t, out, CTL);
        e = erases;
        for (i = 0; i < 20u; i++) { fm1_ms += 400u; autosave_poll(0); }
        ok = erases == e && (hy_udirty & 64u);
        trk_note_off(t, 60);
        hy_quiet(t);
        for (i = 0; i < 20u; i++) { fm1_ms += 400u; autosave_poll(0); }
        check("the bank is kept at rest, never while a note sounds", ok && !hy_udirty && erases > e);
    }
}

/* the WAVE LAB (hydra_lab.c), driven as the panel drives it: a page of EDIT > WAVE LAB, its knobs */
static uint32_t lab_stop(const char *page)
{
    ui_place_t pl = place_of(PL_EDIT);
    for (uint32_t i = 0; i < pl.n; i++)
        if (str_eq(pl.stop[i].obj, "WAVE LAB") && str_eq(pl.stop[i].page, page)) return i;
    return 0;
}
static void lab_go(const char *page)                   /* (a frame first: another engine opens EDIT on its first page) */
{
    frame();
    uix.place = PL_EDIT; uix.stop[PL_EDIT] = (uint8_t)lab_stop(page);
    frames(64);
}
static void lab_knob(uint32_t k, int32_t s) { field_step(&place_of(PL_EDIT).stop[ui_stop(PL_EDIT)].f[k], s); frames(64); }
static int lab_near(const int16_t *a, const int16_t *b, int32_t tol)
{
    for (uint32_t i = 0; i < HY_PTS; i++)
        if (abs(a[i] - b[i]) > tol) return 0;
    return 1;
}
static float lab_h(const int16_t *w, uint32_t k)       /* harmonic k's level */
{
    float re[HL_H + 1u], im[HL_H + 1u];
    hl_spectrum(w, re, im);
    return fm_sqrtf(re[k] * re[k] + im[k] * im[k]);
}
/* neutral values give the start back, MIX at 100 % gives FROM; the first turn on a factory wave copies it into a free
 * user wave and points the slot there, a full bank leaves all as it was; REVERT at START plays the start and, left there,
 * keeps it; a drawbar gives a sine the harmonic it lacked; what plays while the knobs turn is the bank's */
static void the_lab(void)
{
    track_t *t;
    uint8_t *r;
    int ok;
    uint32_t i;
    power_on();
    memset(nor, 255, sizeof nor);
    hy_bank_boot();
    hl_take(HY_WAVES[HYW_BELL]);
    check("lab: neutral values give the start back", lab_near(hl_out, HY_WAVES[HYW_BELL], 1));
    hl.from = HYW_SAW + 1u; hl.mix = 100; hl_render();
    check("lab: MIX at 100 % gives FROM", lab_near(hl_out, HY_WAVES[HYW_SAW], 2));
    hl.on = 0;

    t = hy_fresh(0);
    r = hy_raw(t);
    r[HY_O1 + HO_MODE] = 1;
    r[HY_O1 + HO_L0 + 1] = HYW_BELL + 1;
    hylab_wave_field(HY_O1 + HO_L0 + 1);               /* (as turning SLOT 2 does) */
    lab_go("WAVE");
    ok = hl.on && hl.k < 0 && hl.src == HYW_BELL + 1 && hylab_get(HL_WAVE) == 0;
    lab_knob(2, 35);
    check("lab: the first turn copies BELL into U1 and points the slot there",
          ok && hl.k == 0 && hy_uwave(0)->used && !strcmp(hy_uwave(0)->name, "BELL") && r[HY_O1 + HO_L0 + 1] == HY_USER0 &&
          !strcmp(uix.toast, "BELL \x9b U1"));
    check("lab: a turn is heard at once", !lab_near(hy_uwave(0)->pts, HY_WAVES[HYW_BELL], 1) && lab_near(hy_uwave(0)->pts, hl_out, 0));
    lab_knob(3, 1);                                    /* REVERT: START */
    ok = lab_near(hy_uwave(0)->pts, HY_WAVES[HYW_BELL], 0);
    lab_knob(3, -1);
    ok &= lab_near(hy_uwave(0)->pts, hl_out, 0);
    check("lab: REVERT plays the start, EDIT the result", ok);
    lab_knob(3, 1);
    uix.place = PL_HOME; frames(64);                   /* (left at START: the session ends on the start) */
    check("lab: left at START, the start is kept", !hl.on && lab_near(hy_uwave(0)->pts, HY_WAVES[HYW_BELL], 0));

    lab_go("WAVE");
    ok = hl.k == 0 && hylab_get(HL_MIX) == 0 && hylab_get(HL_REVERT) == 0;   /* (a new session: neutral) */
    lab_knob(2, 50);
    uix.place = PL_HOME; frames(64);
    check("lab: a session ends keeping what plays, the next starts neutral from it",
          ok && !lab_near(hy_uwave(0)->pts, HY_WAVES[HYW_BELL], 1) && hy_uwave(0)->used);

    for (i = 1; i < HY_UWAVES; i++) hy_uset(i, HY_WAVES[HYW_TRI], "FULL", 1);
    r[HY_O1 + HO_L0 + 2] = HYW_GLASS + 1;
    hylab_wave_field(HY_O1 + HO_L0 + 2);
    lab_go("WAVE");
    ok = hl.k < 0;
    lab_knob(2, 20);
    check("lab: with the bank full a turn changes nothing", ok && hl.k < 0 && r[HY_O1 + HO_L0 + 2] == HYW_GLASS + 1 &&
                                                            !strcmp(uix.toast, "BANK FULL") && hylab_get(HL_MIX) == 0);
    uix.place = PL_HOME; frames(64);

    hy_uclear(5);                                      /* a sine in U6, given its second harmonic */
    hy_uset(5, HY_WAVES[HYW_SINE], "PURE", 1);
    r[HY_O1 + HO_L0 + 3] = (uint8_t)(HY_USER0 + 5u);
    hylab_wave_field(HY_O1 + HO_L0 + 3);
    lab_go("HARMONICS 1-4");
    ok = hl.k == 5 && hylab_get(HL_H1) == 0 && hylab_get(HL_H1 + 1) == HL_OFF;
    for (i = 0; i < 61u; i++) lab_knob(1, 1);          /* H2 up from OFF to 0 dB: the start's H1 */
    frames(64);
    {
        float h1 = lab_h(hy_uwave(5)->pts, 1), h2 = lab_h(hy_uwave(5)->pts, 2);
        printf("  drawbar: H2 %.0f against H1 %.0f\n", h2, h1);
        check("lab: a drawbar gives a sine the harmonic it lacked", ok && h2 > 0.89f * h1 && h2 < 1.12f * h1);
    }
    uix.place = PL_HOME; frames(3500);                 /* (at rest: ui_input's autosave writes the bank) */
    {
        int16_t keep[HY_PTS];
        memcpy(keep, hy_uwave(5)->pts, sizeof keep);
        memset(hy_ubank, 0, sizeof hy_ubank);          /* (a power cycle) */
        hy_bank_boot();
        check("lab: the edits are in flash once the unit rests", lab_near(hy_uwave(5)->pts, keep, 0) && hy_uwave(0)->used &&
                                                                !strcmp(hy_uwave(0)->name, "BELL"));
    }
    hy_quiet(t);
}

/* CPU1 retired, or barred until power-off after a crash: nothing pairs, every voice runs on CPU0 at its time alone. Counted in
 * the cores' pairs an Elite with CPU1 barred ran SUPERSTACK late from its fifth voice. In every block, what sounds (fades and
 * twice the rate included) fits the block one voice after another; with CPU1 back the part plays more again (hy_host_cpu1:
 * the host has no worker of its own) */
static void the_one_core(void)
{
    static const uint8_t LOW[8] = {36, 40, 43, 47, 50, 53, 57, 60}, HIGH[8] = {72, 76, 79, 83, 86, 89, 93, 96};
    int32_t out[CTL];
    uint32_t pi, k, b, ok = 1, most[2] = {0, 0}, worst = 0;
    track_t *t;
    for (uint32_t cpu1 = 0; cpu1 < 2u; cpu1++) {
        hy_host_cpu1 = (int)cpu1;
        for (pi = 0; pi < ENG_HYDRA.npresets; pi++)
            for (uint32_t high = 0; high < 2u; high++) {
                const uint8_t *keys = high ? HIGH : LOW;
                uint32_t peak = 0;
                hy_cost_t c;
                t = hy_fresh(pi);
                t->p[P_VOICE] = V_POLY;
                c = hy_cost_most(hy_raw(t));
                for (b = 0; b < 160u; b++) {           /* the chord one key a block, then re-struck from the top */
                    uint32_t us = 0, n = 0;
                    if (b < 8u) trk_note_on(t, keys[b], 100);
                    if (b >= 40u && b % 12u == 0u) { trk_note_off(t, keys[7]); trk_note_on(t, keys[7], 60u + b % 60u); }
                    track_render(t, out, CTL);
                    for (k = 0; k < NVOICE; k++)
                        if (t->v[k].active) {
                            n++;
                            us += HY_ALONE(c.c1) + (hy_voice(hy_part(0), t, &t->v[k])->os ? HY_ALONE(c.os) : 0u);
                        }
                    if (!cpu1 && us > HY_BLOCK_US) {
                        printf("  one core: preset %u %s, block %u: %u voices, %u us alone\n", pi, high ? "high" : "low", b, n, us);
                        ok = 0;
                        b = 160u;
                    }
                    if (!cpu1 && us > worst) worst = us;
                    if (n > peak) peak = n;
                }
                for (k = 0; k < 8u; k++) trk_note_off(t, keys[k]);
                hy_quiet(t);
                if (pi == 4u && !high) most[cpu1] = peak;
            }
    }
    printf("  one core: the longest block %u us alone; SUPERSTACK %u voices, %u with CPU1\n", worst, most[0], most[1]);
    check("CPU1 off: every factory sound's voices fit the block one after another, low and high, struck", ok && worst > 0u);
    check(".. SUPERSTACK plays fewer than with CPU1, and more again with CPU1 back", most[0] >= 1u && most[0] < most[1]);
    hy_host_cpu1 = 1;
}

int main(void)
{
    power_on();
    the_lab();
    the_state();
    the_sound();
    the_one_core();
    the_panel();
    the_stores();
    the_editor();
    the_bank();
    printf("%s\n", bad ? "HYDRA test FAILED" : "HYDRA test passed");
    return bad != 0;
}
