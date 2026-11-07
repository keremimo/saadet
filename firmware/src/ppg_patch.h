/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* PPG: a part's sound, one PPG Wave 2.2 group (A or B). One byte a control, in the 2.2's own numbers: its knobs 0..63
 * (the engine steps them as the 2.2 quantises them), its DIGITAL codes (UW, SW, KW, KF, KL, MW/MF/ML, BD, TW/TF/TL/TM,
 * VF/VL) and its TUNING codes (DETU, MO/MS, EO/ES, BI). The order is the pages' (docs/ppg/mocks.html), not the 2.2's
 * memory: no 2.2 program is read or written. Bytes stored offset: SEMI and SEMI2..8 (24 = none) and ENV 3's AMOUNT
 * (32 = none, as the 2.2's ENV 3 ATT knob). SEMI is the 2.2's SEMIT of voice 1, SEMI2..8 of voices 2..8: each of its
 * eight voices transposes by its own (a voice playing this group; KEYB 2 and 3 stack them into chords). */
#pragma once
enum {
    PG_TABLE, PG_OSC, PG_SUB, PG_SW,           /* WAVES · TABLE: 0..29 and 30 UPPER; WAVES-OSC, WAVES-SUB; SW 0..3 */
    PG_E1W, PG_KW, PG_UW,                      /* WAVES · SWEEP: ENVELOPE 1 - WAVES; KW 0..7; UW */
    PG_SEMI, PG_DETU, PG_BI, PG_BD,            /* TUNE */
    PG_CUT, PG_EMPH, PG_E1F, PG_KF,            /* FILTER: VCF-CUTOFF, VCF-EMPHASIS, ENVELOPE 1 - VCF, KF 0..7 */
    PG_A1, PG_D1, PG_S1, PG_R1,                /* ENV 1 */
    PG_A2, PG_D2, PG_S2, PG_R2,                /* ENV 2 */
    PG_LOUD, PG_KL,                            /* LOUD · LEVEL: ENVELOPE 2 - LOUDN.; KL 0..7 (4 flat) */
    PG_A3, PG_D3, PG_E3AMT, PG_E3TO,           /* ENV 3: ATTACK 3, DECAY 3, ENV 3 ATT (32 = none); EO/ES as 0..3 */
    PG_RATE, PG_SHAPE, PG_DELAY, PG_WHEEL,     /* LFO: RATE, WAVESHAPE 0..3, DELAY; the stored mod wheel */
    PG_MP, PG_MW, PG_MF, PG_ML,                /* LFO · TO: MO/MS as 0..3, MW, MF, ML */
    PG_TW, PG_TF, PG_TL, PG_TM,                /* PLAY · TOUCH */
    PG_VF, PG_VL,                              /* PLAY · VELOCITY */
    PG_SEMI2, PG_SEMI3, PG_SEMI4, PG_SEMI5, PG_SEMI6, PG_SEMI7, PG_SEMI8,   /* SEMIT of voices 2..8 (PG_SEMI: voice 1's) */
    PG_RAW
};
#define PG_UPPER 30u                           /* TABLE 30: the upper table alone (the 2.2's WAVETABLE 30 with UW 1) */
#define PG_SAMPLE 31u                          /* TABLE 31: the 2.2's natural sounds, piano and saxophone, in its short-loop
                                                * transient mode: OSC the start, ENV 1 WAVES the end it loops (eng_ppg.c) */
#define PG_SEMI0 24
#define PG_E3AMT0 32
#define PG_NAME_LEN 12u
/* A program, as the 2.2's: two groups (A, B), each a whole sound, the keyboard mode that plays them and the split point
 * in keys from the lowest C. Measured on the V3 OS: its voices pair up, even ones playing the A slot, odd ones the B
 * slot; KEYB 0 all eight A; 1 A+B a key (4 keys); 2 two A+B a key (2 keys); 3 four A+B on one key; 4..8 split, keys up
 * to SPLIT (the OS's key numbers, lowest C 1) B, above A. A recalled program has A in the A slot. SEL is the group
 * KEYB 0 plays and the pages edit (the GROUP button: the 2.2's load of B into the A slot) */
typedef struct {
    uint8_t raw[PG_RAW];                       /* group A */
    uint8_t rawb[PG_RAW];                      /* group B */
    uint8_t keyb, split, sel;
    char name[PG_NAME_LEN];                    /* ASCII, space-padded */
} ppg_patch_t;
static inline uint8_t *ppg_group(ppg_patch_t *p, uint32_t g) { return g ? p->rawb : p->raw; }
static inline const uint8_t *ppg_group_c(const ppg_patch_t *p, uint32_t g) { return g ? p->rawb : p->raw; }

/* each byte's largest value: the pages' ranges, and what a load accepts */
static const uint8_t PG_MAX[PG_RAW] = {
    [PG_TABLE] = 31, [PG_OSC] = 63, [PG_SUB] = 63, [PG_SW] = 3, [PG_E1W] = 63, [PG_KW] = 7, [PG_UW] = 1,
    [PG_SEMI] = 48, [PG_DETU] = 7, [PG_BI] = 3, [PG_BD] = 7, [PG_CUT] = 63, [PG_EMPH] = 63, [PG_E1F] = 63, [PG_KF] = 7,
    [PG_A1] = 63, [PG_D1] = 63, [PG_S1] = 63, [PG_R1] = 63, [PG_A2] = 63, [PG_D2] = 63, [PG_S2] = 63, [PG_R2] = 63,
    [PG_LOUD] = 63, [PG_KL] = 7, [PG_A3] = 63, [PG_D3] = 63, [PG_E3AMT] = 63, [PG_E3TO] = 3,
    [PG_RATE] = 63, [PG_SHAPE] = 3, [PG_DELAY] = 63, [PG_WHEEL] = 63, [PG_MP] = 3, [PG_MW] = 1, [PG_MF] = 1, [PG_ML] = 1,
    [PG_TW] = 1, [PG_TF] = 1, [PG_TL] = 1, [PG_TM] = 1, [PG_VF] = 1, [PG_VL] = 1,
    [PG_SEMI2 ... PG_SEMI8] = 48,
};
/* voice h's (0..7) SEMIT byte */
static inline uint32_t pg_semi_id(uint32_t h) { return h ? PG_SEMI2 + h - 1u : PG_SEMI; }

/* INIT: wave 63 of table 0 (the saw every table ends with), no SUB, the filter open, the loudness held while the key is */
static void ppg_patch_init(ppg_patch_t *p)
{
    static const uint8_t INIT[][2] = {
        {PG_OSC, 63}, {PG_SW, 3}, {PG_SEMI, PG_SEMI0}, {PG_BD, 1}, {PG_CUT, 63}, {PG_KF, 3},
        {PG_D1, 30}, {PG_S1, 63}, {PG_R1, 20}, {PG_D2, 30}, {PG_S2, 63}, {PG_R2, 16}, {PG_LOUD, 63}, {PG_KL, 4},
        {PG_D3, 20}, {PG_E3AMT, PG_E3AMT0}, {PG_RATE, 32},
        {PG_SEMI2, PG_SEMI0}, {PG_SEMI3, PG_SEMI0}, {PG_SEMI4, PG_SEMI0}, {PG_SEMI5, PG_SEMI0}, {PG_SEMI6, PG_SEMI0},
        {PG_SEMI7, PG_SEMI0}, {PG_SEMI8, PG_SEMI0},
    };
    memset(p, 0, sizeof *p);
    for (uint32_t i = 0; i < NELEM(INIT); i++) p->raw[INIT[i][0]] = INIT[i][1];
    memcpy(p->rawb, p->raw, PG_RAW);
    p->split = 24;
    memcpy(p->name, "INIT WAVE   ", PG_NAME_LEN);
}
static int ppg_patch_valid(const ppg_patch_t *p)
{
    for (uint32_t i = 0; i < PG_RAW; i++) if (p->raw[i] > PG_MAX[i] || p->rawb[i] > PG_MAX[i]) return 0;
    return p->keyb <= 8u && p->split <= 60u && p->sel <= 1u;
}
