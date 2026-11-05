/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* HYDRA's native patch (eng_hydra.c): the same 144 bytes in a part, the last state, a performance and a user slot, so a
 * sound moves between them whole. HY_RSV is held for the modulation section, which no field reads yet; a stored patch may
 * hold anything there. Signed fields are stored raw = value + offset (HY_SEMI0, HY_CENT0, HY_ENV0). */
#pragma once
enum { HO_MODE, HO_WAVE, HO_L0, HO_SCAN = HO_L0 + 8, HO_SEMI, HO_CENT, HO_KEY, HO_N };   /* OSC 1 / OSC 2 */
enum { HM_MODE, HM_SRC, HM_RATIO, HM_DEPTH, HM_AUX, HM_WET, HM_N };                     /* a MUTANT */
enum {
    HY_O1 = 0, HY_O2 = HY_O1 + HO_N,
    HY_O3 = HY_O2 + HO_N, HY_O3_WAVE = HY_O3, HY_O3_SEMI, HY_O3_CENT, HY_O3_KEY,
    HY_M1, HY_M2 = HY_M1 + HM_N, HY_M3 = HY_M2 + HM_N, HY_M4 = HY_M3 + HM_N,
    HY_RING_A = HY_M4 + HM_N, HY_RING_B, HY_NOISE,
    HY_LVL, HY_BAL = HY_LVL + 5, HY_ROUTE = HY_BAL + 5,          /* O1 O2 O3 RING NOISE */
    HY_F1_TYPE, HY_F1_CUT, HY_F1_RES, HY_F1_DRIVE, HY_F1_DPOS, HY_F1_ENV, HY_F1_KEY, HY_F1_VEL,
    HY_F2_TYPE = HY_F1_VEL + 2, HY_F2_MORPH, HY_F2_CUT, HY_F2_RES, HY_F2_ENV, HY_F2_KEY,
    HY_AMP, HY_E1_SC1, HY_E1_SC2, HY_E1_MU1,                     /* ENV 1 -> SCAN 1, SCAN 2, MUTANT 1 DEPTH */
    HY_E1 = 100, HY_E1_DLY = HY_E1, HY_E1_ATK, HY_E1_HLD, HY_E1_DEC, HY_E1_SUS, HY_E1_REL,
    HY_RSV,                                                      /* (.. HY_NAME - 1) */
    HY_NAME = 132, HY_NAME_LEN = 12, HY_BYTES = 144
};
_Static_assert(HY_E1_MU1 < HY_E1 && HY_RSV <= HY_NAME && HY_NAME + HY_NAME_LEN == HY_BYTES, "HYDRA patch layout");
enum { MU_OFF, MU_FM, MU_STACK, MU_SYNC, MU_PW, MU_SQZ, MU_WARP, MU_HARM, MU_N };
enum { HYS_OSC1, HYS_OSC2, HYS_OSC3, HYS_RING, HYS_NOISE, HYS_N };            /* FM sources */
enum { F1_LAD12, F1_LAD24, F1_FAT12, F1_FAT24, F1_MSLP, F1_MSHP, F1_LPG, F1_3LP, F1_3BP, F1_3HP, F1_VOWEL, F1_N };
#define HY_SEMI0 36
#define HY_CENT0 50
#define HY_ENV0 63
#define HY_WAVE_COUNT 56u                              /* tools/gen_hy_waves.py (eng_hydra.c checks it) */
#define HY_FRAME 294u                                  /* a native message, F0 to F7: its header, two nibbles a byte */
typedef struct { uint8_t raw[HY_BYTES]; } hy_patch_t;

/* the bytes a stored or received patch may hold: {first, last, the largest value} */
static int hy_patch_valid(const hy_patch_t *p)
{
#define HY_OSC_RULES(b) {b + HO_MODE, b + HO_MODE, 1}, {b + HO_WAVE, b + HO_WAVE, HY_WAVE_COUNT - 1u}, \
        {b + HO_L0, b + HO_L0 + 7, HY_WAVE_COUNT}, {b + HO_SCAN, b + HO_SCAN, 127}, {b + HO_SEMI, b + HO_SEMI, 2 * HY_SEMI0}, \
        {b + HO_CENT, b + HO_CENT, 2 * HY_CENT0}, {b + HO_KEY, b + HO_KEY, 200}
#define HY_MUT_RULES(b) {b + HM_MODE, b + HM_MODE, MU_N - 1}, {b + HM_SRC, b + HM_SRC, HYS_N - 1}, {b + HM_RATIO, b + HM_WET, 127}
    static const uint8_t RULE[][3] = {
        HY_OSC_RULES(HY_O1), HY_OSC_RULES(HY_O2),
        {HY_O3_WAVE, HY_O3_WAVE, HY_WAVE_COUNT - 1u}, {HY_O3_SEMI, HY_O3_SEMI, 2 * HY_SEMI0},
        {HY_O3_CENT, HY_O3_CENT, 2 * HY_CENT0}, {HY_O3_KEY, HY_O3_KEY, 200},
        HY_MUT_RULES(HY_M1), HY_MUT_RULES(HY_M2), HY_MUT_RULES(HY_M3), HY_MUT_RULES(HY_M4),
        {HY_RING_A, HY_NOISE, 2}, {HY_LVL, HY_BAL + 4, 127}, {HY_ROUTE, HY_ROUTE, 1},
        {HY_F1_TYPE, HY_F1_TYPE, F1_N - 1}, {HY_F1_CUT, HY_F1_DRIVE, 127}, {HY_F1_DPOS, HY_F1_DPOS, 1},
        {HY_F1_ENV, HY_F1_ENV, 2 * HY_ENV0}, {HY_F1_KEY, HY_F1_KEY, 200}, {HY_F1_VEL, HY_F1_VEL, 127},
        {HY_F2_TYPE, HY_F2_TYPE, 1}, {HY_F2_MORPH, HY_F2_RES, 127}, {HY_F2_ENV, HY_F2_ENV, 2 * HY_ENV0},
        {HY_F2_KEY, HY_F2_KEY, 200}, {HY_AMP, HY_AMP, 127}, {HY_E1_SC1, HY_E1_MU1, 2 * HY_ENV0},
        {HY_E1_DLY, HY_E1_REL, 127},
    };
#undef HY_OSC_RULES
#undef HY_MUT_RULES
    for (uint32_t k = 0; k < sizeof RULE / sizeof RULE[0]; k++)
        for (uint32_t i = RULE[k][0]; i <= RULE[k][1]; i++)
            if (p->raw[i] > RULE[k][2])
                return 0;
    for (uint32_t i = 0; i < HY_NAME_LEN; i++)
        if (p->raw[HY_NAME + i] < 32u || p->raw[HY_NAME + i] > 126u)
            return 0;
    return 1;
}
/* the native message: F0 7D 48 59 01, two nibbles a byte (low first), F7. 7D is the header the editor's frames carry too
 * ("non-commercial"): HYDRA is no instrument with a message of its own */
static uint32_t hy_patch_encode(const hy_patch_t *p, uint8_t *frame, uint32_t cap)
{
    if (cap < HY_FRAME)
        return 0;
    frame[0] = 0xF0u; frame[1] = 0x7Du; frame[2] = 0x48u; frame[3] = 0x59u; frame[4] = 1u;
    for (uint32_t i = 0; i < HY_BYTES; i++) {
        frame[5u + 2u * i] = p->raw[i] & 15u;
        frame[6u + 2u * i] = p->raw[i] >> 4;
    }
    frame[HY_FRAME - 1u] = 0xF7u;
    return HY_FRAME;
}
/* the bytes between F0 and F7. 1: p holds a valid patch */
static int hy_patch_decode(hy_patch_t *p, const uint8_t *body, uint32_t n)
{
    if (n != HY_FRAME - 2u || body[0] != 0x7Du || body[1] != 0x48u || body[2] != 0x59u || body[3] != 1u)
        return 0;
    for (uint32_t i = 0; i < HY_BYTES; i++) {
        if (body[4u + 2u * i] > 15u || body[5u + 2u * i] > 15u)
            return 0;
        p->raw[i] = (uint8_t)(body[4u + 2u * i] | body[5u + 2u * i] << 4);
    }
    return hy_patch_valid(p);
}
