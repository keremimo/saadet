/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* HYDRA's factory sounds (eng_hydra.c): each a list of {patch byte, raw value} over INIT (HY_INIT) without its macros,
 * ended by 0xFF; the track's ADSR (ATK DEC SUS REL) and MONO beside it. Our own sounds, one a corner of the engine each.
 *
 * Their macros keep one plan, so a hand learns it once: 1 and 2 the sound's character and its motion, 3 BRIGHT (CC74)
 * and 4 the resonance or edge (CC71), the HOME row; 5..8 what that sound has to give. None opens what a voice costs (a
 * silent level, FILTER 2 wide open): a macro never takes polyphony away (tests/hydra_test.c holds it). */
#define HF_O1(f) (HY_O1 + (f))
#define HF_O2(f) (HY_O2 + (f))
#define HF_M(k, f) (HY_M1 + ((k) - 1) * HM_N + (f))
#define HF_W(w) ((w) + 1)                              /* a list slot holding wave w */
#define HF_SEMI(x) (HY_SEMI0 + (x))
#define HF_CENT(x) (HY_CENT0 + (x))
#define HF_AMT(x) (HY_ENV0 + (x))                      /* a signed amount */
#define HF_END 0xFF
#define HF_NAME(m, n) HY_MNAME + (m) - 1, HN_##n                 /* macro m's name */
#define HF_TO(k, m, g, d) HY_MA + 2 * (k), HY_SLOT((m) - 1, HT_##g), HY_MA + 2 * (k) + 1, HY_DEPTH0 + (d)   /* slot k: macro m to g */
#define HF_NAMES(a, b, c, d, e, f, g, h) HF_NAME(1, a), HF_NAME(2, b), HF_NAME(3, c), HF_NAME(4, d), HF_NAME(5, e), \
    HF_NAME(6, f), HF_NAME(7, g), HF_NAME(8, h)
typedef struct { const char *name; uint8_t env[4], mono; const uint8_t *d; } hy_factory_t;

static const uint8_t HYF_WAVESCAN[] = {
    HF_O1(HO_MODE), 1, HF_O1(HO_L0 + 4), HF_W(HYW_PM_2), HF_O1(HO_L0 + 5), HF_W(HYW_BUZZ),
    HF_O1(HO_L0 + 6), HF_W(HYW_THIN), HF_O1(HO_L0 + 7), HF_W(HYW_AIR), HY_E1_SC1, HF_AMT(50),
    HF_O2(HO_SEMI), HF_SEMI(-12), HF_O2(HO_CENT), HF_CENT(6), HY_LVL + 1, 55,
    HY_F1_CUT, 92, HY_F1_RES, 30, HY_F1_ENV, HF_AMT(12),
    HY_E1_ATK, 40, HY_E1_DEC, 90, HY_E1_SUS, 40, HY_E1_REL, 70,
    HF_NAMES(SCAN, MOTION, BRIGHT, RES, SUB, DETUNE, GRIT, DECAY),
    HF_TO(0, 1, SCAN1, 64), HF_TO(1, 2, ENVSCAN, 64), HF_TO(2, 3, CUT, 64), HF_TO(3, 3, F1ENV, 32),
    HF_TO(4, 4, RES, 64), HF_TO(5, 5, OSC2, 64), HF_TO(6, 6, FINE2, 48), HF_TO(7, 7, DRIVE, 90),
    HF_TO(8, 8, E1DEC, 64), HF_END};
static const uint8_t HYF_VOXCHOIR[] = {
    HF_O1(HO_MODE), 1, HF_O1(HO_L0), HF_W(HYW_VOX_A), HF_O1(HO_L0 + 1), HF_W(HYW_VOX_O), HF_O1(HO_L0 + 2),
    HF_W(HYW_VOX_U), HF_O1(HO_L0 + 3), HF_W(HYW_CHOIR), HF_O1(HO_L0 + 4), HF_W(HYW_VOX_E), HF_O1(HO_L0 + 5),
    HF_W(HYW_VOX_AE), HF_O1(HO_L0 + 6), HF_W(HYW_VOX_I), HF_O1(HO_L0 + 7), HF_W(HYW_NASAL),
    HF_O2(HO_MODE), 1, HF_O2(HO_L0), HF_W(HYW_CHOIR), HF_O2(HO_L0 + 1), HF_W(HYW_VOX_O), HF_O2(HO_L0 + 2),
    HF_W(HYW_VOX_A), HF_O2(HO_L0 + 3), HF_W(HYW_VOX_E), HF_O2(HO_CENT), HF_CENT(9), HY_LVL + 1, 85,
    HY_E1_SC1, HF_AMT(40), HY_E1_SC2, HF_AMT(55), HF_M(1, HM_MODE), MU_STACK, HF_M(1, HM_DEPTH), 40,
    HF_M(1, HM_WET), 80, HY_F1_TYPE, F1_LAD12, HY_F1_CUT, 104, HY_F1_RES, 12,
    HY_E1_ATK, 92, HY_E1_DEC, 104, HY_E1_SUS, 70, HY_E1_REL, 94,
    HF_NAMES(VOWEL, STACK, BRIGHT, RES, MOTION, BLEND, DETUNE, SWELL),
    HF_TO(0, 1, SCAN1, 64), HF_TO(1, 1, SCAN2, 64), HF_TO(2, 2, MUT1, 64), HF_TO(3, 3, CUT, 64),
    HF_TO(4, 4, RES, 64), HF_TO(5, 5, ENVSCAN, 64), HF_TO(6, 6, OSC2, 64), HF_TO(7, 7, FINE2, 40),
    HF_TO(8, 8, E1ATK, 64), HF_END};
static const uint8_t HYF_SYNCLEAD[] = {
    HF_M(1, HM_MODE), MU_SYNC, HF_M(1, HM_RATIO), 20, HF_M(1, HM_DEPTH), 30, HF_M(1, HM_AUX), 40,
    HY_E1_MU1, HF_AMT(40), HY_LVL + 1, 70, HF_O2(HO_CENT), HF_CENT(7),
    HY_F1_CUT, 100, HY_F1_RES, 25, HY_F1_ENV, HF_AMT(20), HY_E1_DEC, 72, HY_E1_SUS, 20,
    HF_NAMES(SYNC, MOTION, BRIGHT, RES, DETUNE, SUB, GRIT, DECAY),
    HF_TO(0, 1, MUT1, 64), HF_TO(1, 1, RATIO1, 40), HF_TO(2, 2, ENVMUT, 64), HF_TO(3, 3, CUT, 64),
    HF_TO(4, 3, F1ENV, 32), HF_TO(5, 4, RES, 64), HF_TO(6, 5, FINE2, 50), HF_TO(7, 6, OSC2, 64),
    HF_TO(8, 7, DRIVE, 90), HF_TO(9, 8, E1DEC, 64), HF_END};
static const uint8_t HYF_SUPERSTACK[] = {
    HF_M(1, HM_MODE), MU_STACK, HF_M(1, HM_DEPTH), 70, HF_M(3, HM_MODE), MU_STACK, HF_M(3, HM_DEPTH), 55,
    HY_LVL + 1, 90, HF_O2(HO_CENT), HF_CENT(-6), HY_F1_CUT, 96, HY_F1_RES, 14, HY_F1_ENV, HF_AMT(10),
    HY_E1_DEC, 80, HY_E1_SUS, 50,
    HF_NAMES(WIDTH, DETUNE, BRIGHT, RES, BLEND, GRIT, SWELL, DECAY),
    HF_TO(0, 1, MUT1, 64), HF_TO(1, 1, MUT3, 64), HF_TO(2, 2, FINE2, 50), HF_TO(3, 3, CUT, 64),
    HF_TO(4, 3, F1ENV, 24), HF_TO(5, 4, RES, 64), HF_TO(6, 5, OSC2, 64), HF_TO(7, 6, DRIVE, 80),
    HF_TO(8, 7, E1ATK, 64), HF_TO(9, 8, E1DEC, 64), HF_END};
static const uint8_t HYF_FMBELL[] = {
    HF_O1(HO_WAVE), HYW_SINE, HF_M(1, HM_MODE), MU_FM, HF_M(1, HM_SRC), HYS_OSC3, HF_M(1, HM_DEPTH), 50,
    HY_E1_MU1, HF_AMT(45), HY_O3_SEMI, HF_SEMI(19), HY_O3_CENT, HF_CENT(14),
    HY_E1_DEC, 74, HY_E1_SUS, 0, HY_E1_REL, 74,
    HF_NAMES(FM, MOTION, TONE, EDGE, DECAY, BODY, GRIT, RELEASE),
    HF_TO(0, 1, MUT1, 64), HF_TO(1, 2, ENVMUT, 64), HF_TO(2, 3, CUT, 64), HF_TO(3, 4, AUX1, 64),
    HF_TO(4, 5, E1DEC, 64), HF_TO(5, 6, OSC3, 64), HF_TO(6, 7, DRIVE, 80), HF_TO(7, 8, E1REL, 64), HF_END};
static const uint8_t HYF_PWMSTRINGS[] = {
    HF_M(1, HM_MODE), MU_PW, HF_M(1, HM_DEPTH), 50, HF_M(3, HM_MODE), MU_PW, HF_M(3, HM_DEPTH), 40,
    HY_LVL + 1, 95, HF_O2(HO_CENT), HF_CENT(9), HY_F1_TYPE, F1_LAD12, HY_F1_CUT, 84, HY_F1_ENV, HF_AMT(8),
    HY_E1_ATK, 70, HY_E1_DEC, 90, HY_E1_SUS, 80, HY_E1_REL, 84,
    HF_NAMES(PW, DETUNE, BRIGHT, RES, BLEND, SWELL, GRIT, RELEASE),
    HF_TO(0, 1, MUT1, 64), HF_TO(1, 1, MUT3, 64), HF_TO(2, 2, FINE2, 50), HF_TO(3, 3, CUT, 64),
    HF_TO(4, 4, RES, 64), HF_TO(5, 5, OSC2, 64), HF_TO(6, 6, E1ATK, 64), HF_TO(7, 7, DRIVE, 64),
    HF_TO(8, 8, E1REL, 64), HF_END};
static const uint8_t HYF_WARPBASS[] = {
    HF_O1(HO_WAVE), HYW_SQR, HF_M(1, HM_MODE), MU_WARP, HF_M(1, HM_RATIO), 96, HF_M(1, HM_DEPTH), 60,
    HF_O2(HO_WAVE), HYW_SINE, HF_O2(HO_SEMI), HF_SEMI(-12), HF_O2(HO_CENT), HF_CENT(0), HY_LVL + 1, 80,
    HY_F1_CUT, 50, HY_F1_RES, 40, HY_F1_ENV, HF_AMT(40), HY_E1_DEC, 62, HY_E1_SUS, 0,
    HF_NAMES(WARP, SHAPE, BRIGHT, RES, SUB, GRIT, DECAY, PLUCK),
    HF_TO(0, 1, MUT1, 64), HF_TO(1, 2, RATIO1, 64), HF_TO(2, 3, CUT, 64), HF_TO(3, 3, F1ENV, 32),
    HF_TO(4, 4, RES, 64), HF_TO(5, 5, OSC2, 64), HF_TO(6, 6, DRIVE, 100), HF_TO(7, 7, E1DEC, 64),
    HF_TO(8, 8, E1DEC, -48), HF_TO(9, 8, F1ENV, 40), HF_END};
static const uint8_t HYF_HARMSWEEP[] = {
    HF_M(1, HM_MODE), MU_HARM, HF_M(1, HM_RATIO), 64, HF_M(1, HM_DEPTH), 0, HY_E1_MU1, HF_AMT(63),
    HY_F1_CUT, 112, HY_F1_RES, 20, HY_E1_ATK, 76, HY_E1_DEC, 96, HY_E1_SUS, 30, HY_E1_REL, 90,
    HF_NAMES(HARMONIC, MOTION, BRIGHT, RES, SHAPE, SWELL, GRIT, DECAY),
    HF_TO(0, 1, MUT1, 64), HF_TO(1, 2, ENVMUT, 64), HF_TO(2, 3, CUT, 64), HF_TO(3, 4, RES, 64),
    HF_TO(4, 5, RATIO1, 64), HF_TO(5, 6, E1ATK, 64), HF_TO(6, 7, DRIVE, 80), HF_TO(7, 8, E1DEC, 64), HF_END};
static const uint8_t HYF_MS20ACID[] = {
    HY_F1_TYPE, F1_MSLP, HY_F1_CUT, 40, HY_F1_RES, 92, HY_F1_ENV, HF_AMT(46), HY_F1_DRIVE, 40,
    HY_E1_DEC, 58, HY_E1_SUS, 0,
    HF_NAMES(ENV, DECAY, BRIGHT, RES, GRIT, ATTACK, BITE, RELEASE),
    HF_TO(0, 1, F1ENV, 64), HF_TO(1, 2, E1DEC, 64), HF_TO(2, 3, CUT, 64), HF_TO(3, 4, RES, 64),
    HF_TO(4, 5, DRIVE, 80), HF_TO(5, 6, E1ATK, 64), HF_TO(6, 7, RES, 40), HF_TO(7, 7, DRIVE, 40),
    HF_TO(8, 7, F1ENV, 30), HF_TO(9, 8, E1REL, 64), HF_END};
static const uint8_t HYF_LPGPLUCK[] = {
    HF_O1(HO_WAVE), HYW_TINE, HF_O2(HO_WAVE), HYW_SINE, HF_O2(HO_SEMI), HF_SEMI(12), HF_O2(HO_CENT), HF_CENT(0),
    HY_LVL + 1, 50, HY_F1_TYPE, F1_LPG, HY_F1_CUT, 24, HY_F1_RES, 20, HY_F1_ENV, HF_AMT(63),
    HY_E1_DEC, 70, HY_E1_SUS, 0, HY_E1_REL, 60,
    HF_NAMES(BLEND, DECAY, BRIGHT, RES, DETUNE, RELEASE, GRIT, SWELL),
    HF_TO(0, 1, OSC2, 64), HF_TO(1, 2, E1DEC, 64), HF_TO(2, 3, CUT, 64), HF_TO(3, 4, RES, 64),
    HF_TO(4, 5, FINE2, 40), HF_TO(5, 6, E1REL, 64), HF_TO(6, 7, DRIVE, 64), HF_TO(7, 8, E1ATK, 64), HF_END};
static const uint8_t HYF_VOWELTALK[] = {
    HF_O1(HO_WAVE), HYW_BUZZ, HY_F1_TYPE, F1_VOWEL, HY_F1_CUT, 0, HY_F1_RES, 72, HY_F1_ENV, HF_AMT(63),
    HY_E1_ATK, 64, HY_E1_DEC, 92, HY_E1_SUS, 40, HY_E1_REL, 80,
    HF_NAMES(VOWEL, TALK, RES, GRIT, MOTION, SWELL, RELEASE, LEVEL),
    HF_TO(0, 1, CUT, 127), HF_TO(1, 2, F1ENV, 64), HF_TO(2, 3, RES, 64), HF_TO(3, 4, DRIVE, 64),
    HF_TO(4, 5, E1DEC, 64), HF_TO(5, 6, E1ATK, 64), HF_TO(6, 7, E1REL, 64), HF_TO(7, 8, LEVEL, 64), HF_END};
static const uint8_t HYF_RINGBELLS[] = {
    HF_O1(HO_WAVE), HYW_GLASS, HY_O3_SEMI, HF_SEMI(24), HY_O3_CENT, HF_CENT(13), HY_RING_A, HYS_OSC1,
    HY_RING_B, HYS_OSC3, HY_LVL, 40, HY_LVL + 3, 90, HY_F1_CUT, 120,
    HF_NAMES(RING, BLEND, TONE, RES, BODY, GRIT, SWEEP, DECAY),
    HF_TO(0, 1, RING, 64), HF_TO(1, 2, OSC1, 64), HF_TO(2, 3, CUT, 64), HF_TO(3, 4, RES, 64),
    HF_TO(4, 5, OSC3, 64), HF_TO(5, 6, DRIVE, 80), HF_TO(6, 7, F1ENV, 64), HF_TO(7, 8, E1DEC, 64), HF_END};
static const uint8_t HYF_NOISESWEEP[] = {
    HY_LVL, 0, HY_LVL + 4, 110, HY_NOISE, 1, HY_F1_TYPE, F1_3BP, HY_F1_CUT, 30, HY_F1_RES, 100,
    HY_F1_ENV, HF_AMT(60), HY_E1_ATK, 100, HY_E1_DEC, 110, HY_E1_SUS, 20, HY_E1_REL, 100,
    HF_NAMES(SWEEP, SWELL, BRIGHT, RES, BODY, GRIT, DECAY, RELEASE),
    HF_TO(0, 1, F1ENV, 64), HF_TO(1, 2, E1ATK, 64), HF_TO(2, 3, CUT, 64), HF_TO(3, 4, RES, 64),
    HF_TO(4, 5, OSC1, 64), HF_TO(5, 6, DRIVE, 80), HF_TO(6, 7, E1DEC, 64), HF_TO(7, 8, E1REL, 64), HF_END};
static const uint8_t HYF_SQUEEZEKEYS[] = {
    HF_O1(HO_WAVE), HYW_SQR, HF_M(1, HM_MODE), MU_SQZ, HF_M(1, HM_DEPTH), 40,
    HF_O2(HO_WAVE), HYW_TRI, HF_O2(HO_SEMI), HF_SEMI(12), HF_O2(HO_CENT), HF_CENT(0), HY_LVL + 1, 50,
    HY_F1_TYPE, F1_FAT24, HY_F1_CUT, 84, HY_F1_RES, 20, HY_F1_ENV, HF_AMT(25), HY_E1_DEC, 70, HY_E1_SUS, 30,
    HF_NAMES(SQUEEZE, BLEND, BRIGHT, RES, DETUNE, GRIT, DECAY, PLUCK),
    HF_TO(0, 1, MUT1, 64), HF_TO(1, 2, OSC2, 64), HF_TO(2, 3, CUT, 64), HF_TO(3, 3, F1ENV, 24),
    HF_TO(4, 4, RES, 64), HF_TO(5, 5, FINE2, 40), HF_TO(6, 6, DRIVE, 80), HF_TO(7, 7, E1DEC, 64),
    HF_TO(8, 8, E1DEC, -40), HF_TO(9, 8, F1ENV, 40), HF_END};
static const uint8_t HYF_THREEPOLE[] = {
    HF_O2(HO_WAVE), HYW_PULSE25, HF_O2(HO_SEMI), HF_SEMI(12), HF_O2(HO_CENT), HF_CENT(4), HY_LVL + 1, 60,
    HF_M(3, HM_MODE), MU_PW, HF_M(3, HM_DEPTH), 20, HY_F1_TYPE, F1_3LP, HY_F1_CUT, 74, HY_F1_RES, 70,
    HY_F1_ENV, HF_AMT(25), HY_E1_DEC, 66, HY_E1_SUS, 30,
    HF_NAMES(PW, BLEND, BRIGHT, RES, DETUNE, GRIT, DECAY, SWELL),
    HF_TO(0, 1, MUT3, 64), HF_TO(1, 2, OSC2, 64), HF_TO(2, 3, CUT, 64), HF_TO(3, 3, F1ENV, 24),
    HF_TO(4, 4, RES, 64), HF_TO(5, 5, FINE2, 40), HF_TO(6, 6, DRIVE, 90), HF_TO(7, 7, E1DEC, 64),
    HF_TO(8, 8, E1ATK, 64), HF_END};
static const uint8_t HYF_DUALFILTER[] = {
    HF_O1(HO_MODE), 1, HF_O1(HO_L0 + 1), HF_W(HYW_SQR), HF_O1(HO_L0 + 2), HF_W(HYW_PULSE25),
    HF_O1(HO_L0 + 3), HF_W(HYW_PULSE10), HF_O1(HO_L0 + 4), HF_W(HYW_SYNC_2), HF_O1(HO_L0 + 5), HF_W(HYW_SYNC_3),
    HF_O1(HO_L0 + 6), HF_W(HYW_SYNC_5), HF_O1(HO_L0 + 7), HF_W(HYW_CRUSH), HY_E1_SC1, HF_AMT(63),
    HF_O2(HO_MODE), 1, HF_O2(HO_L0), HF_W(HYW_VOX_A), HF_O2(HO_L0 + 1), HF_W(HYW_VOX_E), HF_O2(HO_L0 + 2),
    HF_W(HYW_VOX_I), HF_O2(HO_L0 + 3), HF_W(HYW_VOX_O), HY_E1_SC2, HF_AMT(-40), HF_O2(HO_SCAN), 127,
    HY_LVL + 1, 90, HY_BAL + 1, 127, HY_ROUTE, 1, HY_F1_CUT, 70, HY_F1_RES, 30,
    HY_F2_MORPH, 64, HY_F2_CUT, 80, HY_F2_RES, 50,
    HY_E1_ATK, 96, HY_E1_DEC, 110, HY_E1_SUS, 50, HY_E1_REL, 96,
    HF_NAMES(SCAN, MOTION, BRIGHT, RES, PASS, BLEND, GRIT, SWELL),
    HF_TO(0, 1, SCAN1, 64), HF_TO(1, 1, SCAN2, -64), HF_TO(2, 2, ENVSCAN, 64), HF_TO(3, 3, CUT, 64),
    HF_TO(4, 3, F2CUT, 64), HF_TO(5, 4, RES, 64), HF_TO(6, 4, F2RES, 64), HF_TO(7, 5, PASS, 64),
    HF_TO(8, 6, OSC2, 64), HF_TO(9, 7, DRIVE, 80), HF_TO(10, 8, E1ATK, 64), HF_END};

static const hy_factory_t HY_FACTORY[] = {
    {"WAVESCAN", {5, 60, 110, 60}, 0, HYF_WAVESCAN},
    {"VOX CHOIR", {80, 90, 120, 95}, 0, HYF_VOXCHOIR},
    {"SYNC LEAD", {0, 60, 115, 40}, 1, HYF_SYNCLEAD},
    {"SUPERSTACK", {0, 60, 120, 60}, 0, HYF_SUPERSTACK},
    {"FM BELL", {0, 85, 0, 85}, 0, HYF_FMBELL},
    {"PWM STRINGS", {60, 90, 115, 80}, 0, HYF_PWMSTRINGS},
    {"WARP BASS", {0, 70, 100, 30}, 1, HYF_WARPBASS},
    {"HARMONIC SWEEP", {40, 90, 110, 85}, 0, HYF_HARMSWEEP},
    {"MS-20 ACID", {0, 60, 90, 20}, 1, HYF_MS20ACID},
    {"LPG PLUCK", {0, 80, 0, 80}, 0, HYF_LPGPLUCK},
    {"VOWEL TALK", {40, 90, 110, 80}, 0, HYF_VOWELTALK},
    {"RING BELLS", {0, 95, 0, 95}, 0, HYF_RINGBELLS},
    {"NOISE SWEEP", {90, 100, 100, 100}, 0, HYF_NOISESWEEP},
    {"SQUEEZE KEYS", {0, 75, 60, 55}, 0, HYF_SQUEEZEKEYS},
    {"THREE POLE", {0, 60, 110, 40}, 1, HYF_THREEPOLE},
    {"DUAL FILTER", {70, 100, 110, 96}, 0, HYF_DUALFILTER},
};
#define HY_FACTORY_N ((uint32_t)NELEM(HY_FACTORY))
