/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* The places the buttons open and what is in them. A place is a row of stops; a stop is one page of one object (an
 * operator, an envelope, the matrix) with the four fields K1..K4 edit. Pressing the place's button again or turning
 * SELECT moves along the row; the header names the object (or, in a place of one object, the page) and its dots count
 * the object's pages. Holding the button lists the objects (or the pages) on the keys. EDIT, ENV and LFO are the
 * engine's: its own panel (a Prophet's, the six operators, a CZ-1 tone's lines and envelopes, a JUNO-106's sliders and
 * switches, a HYDRA's oscillators, mutants and filters) beside the common ones. */
enum { PL_HOME, PL_LIB, PL_SCL, PL_ENV, PL_LFO, PL_EDIT, PL_GLO, PL_SAVE, PL_ARP, PL_SEQ, PL_PLAY, PL_N };
/* the picture between the header and the values (ui_pics.c); a stop's arg says which envelope, operator or filter */
enum { PIC_NONE, PIC_ENV, PIC_LFO, PIC_MATRIX, PIC_ARP, PIC_SCALE, PIC_CHORD, PIC_VOICES, PIC_READOUT, PIC_LIB,
       PIC_FADERS, PIC_P5ENV, PIC_FILTER, PIC_ALGO, PIC_FMEG, PIC_CZEG, PIC_ERASE, PIC_BLE, PIC_BTDEV,
       PIC_HYSLOTS, PIC_HYWAVE, PIC_HYSPEC, PIC_HYNAME };   /* (HYDRA's slots' waves; the WAVE LAB's, hydra_lab.c) */
typedef struct { const char *obj, *page; uint8_t pic; ui_field_t f[4]; uint8_t arg; } ui_stop_t;   /* obj "": the
                                                                                                * place is one object */
typedef struct { const char *name; const ui_stop_t *stop; uint8_t n; } ui_place_t;
#define FT(id, l) {FK_TRACK, (uint8_t)(id), l, 0}
#define FG(id, l) {FK_GLOBAL, (uint8_t)(id), l, 0}
#define FP(id, l) {FK_P5, (uint8_t)(id), l, 0}
#define FM(o, id, l) {FK_FM6OP, (uint8_t)(id), l, o}
#define FV(id, l) {FK_FM6V, (uint8_t)(id), l, 0}
#define FN(id, l) {FK_FM6FN, (uint8_t)(id), l, 0}
#define FC(id, l) {FK_CZ, (uint8_t)(id), l, 0}
#define FJ(id, l) {FK_J106, (uint8_t)(id), l, 0}
#define FH(id, l) {FK_HY, (uint8_t)(id), l, 0}
#define FD(id, l) {FK_DEV, (uint8_t)(id), l, 0}
#define FL(id, l) {FK_LIB, (uint8_t)(id), l, 0}
#define FW(id, l) {FK_HYLAB, (uint8_t)(id), l, 0}
#define F0 {FK_NONE, 0, 0, 0}

/* the library's lists (ui_lib.c); its fields, SAVE's and an import's */
enum { LIB_FAV, LIB_USER, LIB_P5, LIB_FM6, LIB_CZ, LIB_J106, LIB_HY, LIB_PERF, LIB_N };
enum { LF_SOUND, LF_FAV, LF_SLOT, LF_CURSOR, LF_LETTER, LF_SFAV, LF_FIRST, LF_DELETE, LF_ERASE, LF_SEND, LF_PSLOT, LF_N };
static const ui_stop_t ST_LIB[LIB_N] = {
    {"", "FAVOURITES", PIC_LIB, {FL(LF_SOUND, "SOUND"), FL(LF_FAV, "FAV"), FL(LF_SEND, "SEND"), FL(LF_DELETE, "DELETE")}},
    {"", "USER", PIC_LIB, {FL(LF_SOUND, "SOUND"), FL(LF_FAV, "FAV"), FL(LF_SEND, "SEND"), FL(LF_DELETE, "DELETE")}},
    {"", "P5", PIC_LIB, {FL(LF_SOUND, "SOUND"), FL(LF_FAV, "FAV"), FL(LF_SEND, "SEND"), FL(LF_DELETE, "DELETE")}},
    {"", "FM6", PIC_LIB, {FL(LF_SOUND, "SOUND"), FL(LF_FAV, "FAV"), FL(LF_SEND, "SEND"), FL(LF_DELETE, "DELETE")}},
    {"", "CZ-1", PIC_LIB, {FL(LF_SOUND, "SOUND"), FL(LF_FAV, "FAV"), FL(LF_SEND, "SEND"), FL(LF_DELETE, "DELETE")}},
    {"", "J106", PIC_LIB, {FL(LF_SOUND, "SOUND"), FL(LF_FAV, "FAV"), FL(LF_SEND, "SEND"), FL(LF_DELETE, "DELETE")}},
    {"", "HYDRA", PIC_LIB, {FL(LF_SOUND, "SOUND"), FL(LF_FAV, "FAV"), FL(LF_SEND, "SEND"), FL(LF_DELETE, "DELETE")}},
    {"", "PERFORM", PIC_LIB, {FL(LF_SOUND, "SETUP"), F0, F0, FL(LF_DELETE, "DELETE")}},
};
static const ui_stop_t ST_SAVE[] = {
    {"", "SAVE", PIC_NONE, {FL(LF_SLOT, "SLOT"), FL(LF_CURSOR, "CURSOR"), FL(LF_LETTER, "LETTER"), FL(LF_SFAV, "FAV")}},
    {"", "PERFORM", PIC_NONE, {FL(LF_PSLOT, "SLOT"), FL(LF_CURSOR, "CURSOR"), FL(LF_LETTER, "LETTER"), F0}},
};
static const ui_stop_t ST_IMPORT[] = {
    {"", "IMPORT", PIC_NONE, {FL(LF_FIRST, "FIRST"), F0, F0, F0}},
};
static const ui_stop_t ST_SCL[] = {
    {"", "SCALE", PIC_SCALE, {FT(P_SCALE, "SCALE"), FT(P_ROOT, "ROOT"), FT(P_QUANT, "KEYS"), FT(P_TRANS, "SHIFT")}},
    {"", "MPC", PIC_SCALE, {FT(P_MPCDEG, "DEGREE"), F0, F0, F0}},
};

/* ENV: the engine's envelopes, then the common modulation envelope. */
#define MOD_ENV(n) \
    {n, "SHAPE", PIC_ENV, {FT(P_ATK, "ATTACK"), FT(P_DEC, "DECAY"), FT(P_SUS, "SUSTAIN"), FT(P_REL, "RELEASE")}}, \
    {n, "AMOUNTS", PIC_ENV, {FT(P_ED_FLT, "FILTER"), FT(P_ED_PIT, "PITCH"), FT(P_ED_SHP, "SHAPE"), F0}}
static const ui_stop_t ST_ENV_P5[] = {
    {"FILTER ENV", "SHAPE", PIC_P5ENV, {FP(P5_ATTACK_FILTER, "ATTACK"), FP(P5_DECAY_FILTER, "DECAY"),
                                        FP(P5_SUSTAIN_FILTER, "SUSTAIN"), FP(P5_RELEASE_FILTER, "RELEASE")}, 0},
    {"AMP ENV", "SHAPE", PIC_P5ENV, {FP(P5_ATTACK_AMP, "ATTACK"), FP(P5_DECAY_AMP, "DECAY"), FP(P5_SUSTAIN_AMP, "SUSTAIN"),
                                     FP(P5_RELEASE_AMP, "RELEASE")}, 1},
    {"AMP ENV", "SWITCHES", PIC_P5ENV, {FP(P5_RELEASE_ON, "RELEASE"), FP(P5_VEL_AMP, "VELOCITY"), F0, F0}, 1},
    MOD_ENV("MOD ENV"),
};
static const ui_stop_t ST_ENV_J106[] = {           /* (one envelope: the filter's and, in ENV mode, the VCA's) */
    {"ENV", "SHAPE", PIC_P5ENV, {FJ(J106_ENV_A, "ATTACK"), FJ(J106_ENV_D, "DECAY"), FJ(J106_ENV_S, "SUSTAIN"),
                                 FJ(J106_ENV_R, "RELEASE")}, 2},
    {"ENV", "ROUTING", PIC_P5ENV, {FJ(J106_VCA_MODE, "VCA"), FJ(J106_VCF_ENV, "VCF ENV"), FJ(J106_ENV_POL, "ENV POL"),
                                   F0}, 2},
    MOD_ENV("MOD ENV"),
};
static const ui_stop_t ST_ENV_HY[] = {             /* (ENV 1 moves the filters and the scans; the amp is the common envelope) */
    {"ENV 1", "SHAPE", PIC_P5ENV, {FH(HY_E1_ATK, "ATTACK"), FH(HY_E1_DEC, "DECAY"), FH(HY_E1_SUS, "SUSTAIN"),
                                   FH(HY_E1_REL, "RELEASE")}, 3},
    {"ENV 1", "TIMING", PIC_P5ENV, {FH(HY_E1_DLY, "DELAY"), FH(HY_E1_HLD, "HOLD"), F0, F0}, 3},
    {"ENV 1", "TARGETS", PIC_P5ENV, {FH(HY_E1_SC1, "SCAN 1"), FH(HY_E1_SC2, "SCAN 2"), FH(HY_E1_MU1, "MUTANT 1"), F0}, 3},
    MOD_ENV("AMP ENV"),
};
static const ui_stop_t ST_ENV_FM6[] = {
    {"PITCH EG", "RATES", PIC_FMEG, {FV(FP_PR1, 0), FV(FP_PR1 + 1, 0), FV(FP_PR1 + 2, 0), FV(FP_PR1 + 3, 0)}, 6},
    {"PITCH EG", "LEVELS", PIC_FMEG, {FV(FP_PL1, 0), FV(FP_PL1 + 1, 0), FV(FP_PL1 + 2, 0), FV(FP_PL1 + 3, 0)}, 6},
    MOD_ENV("MOD ENV"),
};
#define CZ_EG(l, e, n) \
    {n, "RATES 1-4", PIC_CZEG, {FC(LCZ_EBASE(l, e), 0), FC(LCZ_EBASE(l, e) + 1, 0), FC(LCZ_EBASE(l, e) + 2, 0), \
                                FC(LCZ_EBASE(l, e) + 3, 0)}, (l) * 3 + (e)}, \
    {n, "RATES 5-8", PIC_CZEG, {FC(LCZ_EBASE(l, e) + 4, 0), FC(LCZ_EBASE(l, e) + 5, 0), FC(LCZ_EBASE(l, e) + 6, 0), \
                                FC(LCZ_EBASE(l, e) + 7, 0)}, (l) * 3 + (e)}, \
    {n, "LEVELS 1-4", PIC_CZEG, {FC(LCZ_EBASE(l, e) + 8, 0), FC(LCZ_EBASE(l, e) + 9, 0), FC(LCZ_EBASE(l, e) + 10, 0), \
                                 FC(LCZ_EBASE(l, e) + 11, 0)}, (l) * 3 + (e)}, \
    {n, "LEVELS 5-8", PIC_CZEG, {FC(LCZ_EBASE(l, e) + 12, 0), FC(LCZ_EBASE(l, e) + 13, 0), FC(LCZ_EBASE(l, e) + 14, 0), \
                                 FC(LCZ_EBASE(l, e) + 15, 0)}, (l) * 3 + (e)}, \
    {n, "POINTS", PIC_CZEG, {FC(LCZ_EBASE(l, e) + 16, "SUSTAIN"), FC(LCZ_EBASE(l, e) + 17, "END"), F0, F0}, (l) * 3 + (e)}
static const ui_stop_t ST_ENV_CZ[] = {             /* (DCO: pitch, DCW: the wave's shape, DCA: the amp) */
    CZ_EG(0, 0, "DCO 1"), CZ_EG(0, 1, "DCW 1"), CZ_EG(0, 2, "DCA 1"),
    CZ_EG(1, 0, "DCO 2"), CZ_EG(1, 1, "DCW 2"), CZ_EG(1, 2, "DCA 2"),
    MOD_ENV("MOD ENV"),
};
#undef CZ_EG

/* LFO: the engine's own, then the common LFO and the matrix */
#define LFO_COMMON \
    {"LFO", "WAVE", PIC_LFO, {FT(P_LRATE, "RATE"), FT(P_LWAVE, "WAVE"), FT(P_LFADE, "FADE IN"), FT(P_LTRIG, "RESTART")}}, \
    {"LFO", "SYNC", PIC_LFO, {FT(P_LSYNC, "SYNC"), FT(P_LPHASE, "PHASE"), FT(P_LPOL, "RANGE"), F0}}, \
    {"LFO", "DEPTH", PIC_LFO, {FT(P_LD_PIT, "PITCH"), FT(P_LD_FLT, "FILTER"), FT(P_LD_SHP, "SHAPE"), FT(P_LD_AMP, "AMP")}}, \
    {"MATRIX", "AMOUNTS", PIC_MATRIX, {FT(P_M1AMT, "ROUTE 1"), FT(P_M2AMT, "ROUTE 2"), FT(P_M3AMT, "ROUTE 3"), \
                                       FT(P_M4AMT, "ROUTE 4")}}, \
    ROUTE(0, "ROUTE 1"), ROUTE(1, "ROUTE 2"), ROUTE(2, "ROUTE 3"), ROUTE(3, "ROUTE 4")
#define ROUTE(k, n) {"MATRIX", n, PIC_MATRIX, {FT(P_M1SRC + (k) * 3, "SOURCE"), FT(P_M1DST + (k) * 3, "DEST"), \
                                               FT(P_M1AMT + (k) * 3, "AMOUNT"), F0}}
static const ui_stop_t ST_LFO_P5[] = {
    {"P5 LFO", "WAVE", PIC_FADERS, {FP(P5_LFO_RATE, "RATE"), FP(P5_LFO_SAW, "SAW"), FP(P5_LFO_TRI, "TRI"),
                                    FP(P5_LFO_PULSE, "SQUARE")}},
    LFO_COMMON,
};
static const ui_stop_t ST_LFO_J106[] = {
    {"J106 LFO", "RATE", PIC_FADERS, {FJ(J106_LFO_RATE, "RATE"), FJ(J106_LFO_DELAY, "DELAY"), FJ(J106_DCO_LFO, "DCO"),
                                      FJ(J106_VCF_LFO, "VCF")}},
    LFO_COMMON,
};
/* HYDRA's macros beside the matrix: a macro's NAME, its VALUE (E1..E8, as the knobs turn it) and up to three of its
 * destinations with their depths, from the patch's pool of HY_MSLOTS (hydra_edit.h) */
#define HY_MACRO_STOPS(m, n) \
    {n, "DEST 1", PIC_FADERS, {FH(HY_MNAME + (m), "NAME"), FT(P_E0 + (m), "VALUE"), FH(HYV_DEST(m, 0), 0), FH(HYV_DEPTH(m, 0), 0)}}, \
    {n, "DESTS 2-3", PIC_FADERS, {FH(HYV_DEST(m, 1), 0), FH(HYV_DEPTH(m, 1), 0), FH(HYV_DEST(m, 2), 0), FH(HYV_DEPTH(m, 2), 0)}}
static const ui_stop_t ST_LFO_HY[] = {
    LFO_COMMON,
    HY_MACRO_STOPS(0, "MACRO 1"), HY_MACRO_STOPS(1, "MACRO 2"), HY_MACRO_STOPS(2, "MACRO 3"), HY_MACRO_STOPS(3, "MACRO 4"),
    HY_MACRO_STOPS(4, "MACRO 5"), HY_MACRO_STOPS(5, "MACRO 6"), HY_MACRO_STOPS(6, "MACRO 7"), HY_MACRO_STOPS(7, "MACRO 8"),
};
#undef HY_MACRO_STOPS
static const ui_stop_t ST_LFO_FM6[] = {
    {"FM LFO", "WAVE", PIC_FADERS, {FV(FP_LFW, "WAVE"), FV(FP_LFS, "SPEED"), FV(FP_LFD, "DELAY"), FV(FP_LKS, "RESTART")}},
    {"FM LFO", "DEPTH", PIC_FADERS, {FV(FP_LPMD, "PITCH"), FV(FP_LAMD, "AMP"), FV(FP_LPMS, "PMS"), F0}},
    LFO_COMMON,
};
static const ui_stop_t ST_LFO_CZ[] = {
    {"VIBRATO", "WAVE", PIC_FADERS, {FC(LCZ_VWAVE, "WAVE"), FC(LCZ_VRATE, "RATE"), FC(LCZ_VDEP, "DEPTH"),
                                     FC(LCZ_VDELAY, "DELAY")}},
    LFO_COMMON,
};
#undef ROUTE
#undef LFO_COMMON

/* EDIT: the engine's own panel */
static const ui_stop_t ST_EDIT_P5[] = {
    {"OSC A", "WAVE", PIC_FADERS, {FP(P5_FREQ_A, "FREQ"), FP(P5_SAW_A, "SAW"), FP(P5_PULSE_A, "PULSE"), FP(P5_PW_A, "WIDTH")}},
    {"OSC B", "WAVE", PIC_FADERS, {FP(P5_FREQ_B, "FREQ"), FP(P5_SAW_B, "SAW"), FP(P5_TRI_B, "TRI"),
                                   FP(P5_PULSE_B, "PULSE")}},
    {"OSC B", "TUNE", PIC_FADERS, {FP(P5_FINE_B, "FINE"), FP(P5_PW_B, "WIDTH"), FP(P5_LOW_B, "LOW"), FP(P5_KEY_B, "KEYS")}},
    {"MIXER", "LEVELS", PIC_FADERS, {FP(P5_LEVEL_A, "OSC A"), FP(P5_LEVEL_B, "OSC B"), FP(P5_NOISE, "NOISE"),
                                     FP(P5_SYNC, "SYNC")}},
    {"FILTER", "CUTOFF", PIC_FILTER, {FP(P5_CUTOFF, "CUTOFF"), FP(P5_RESONANCE, "RES"), FP(P5_ENV_FILTER, "ENV AMT"),
                                      FP(P5_KEY_FILTER, "KEYS")}},
    {"FILTER", "MODEL", PIC_FILTER, {FP(P5_FILTER_REV, "MODEL"), FP(P5_VEL_FILTER, "VELOCITY"),
                                     FP(P5_PRESS_FILTER, "PRESS"), F0}},
    {"POLY MOD", "SOURCES", PIC_FADERS, {FP(P5_POLY_ENV, "FILT ENV"), FP(P5_POLY_B, "OSC B"), F0, F0}},
    {"POLY MOD", "TARGETS", PIC_FADERS, {FP(P5_POLY_FREQ, "FREQ A"), FP(P5_POLY_PW, "PW A"), FP(P5_POLY_FILTER, "FILTER"),
                                        F0}},
    {"WHEEL MOD", "SOURCES", PIC_FADERS, {FP(P5_WHEEL_MIX, "MIX"), FP(P5_LFO_INITIAL, "INITIAL"),
                                          FP(P5_PRESS_LFO, "PRESS"), F0}},
    {"WHEEL MOD", "TARGETS", PIC_FADERS, {FP(P5_WHEEL_FREQ_A, "FREQ A"), FP(P5_WHEEL_FREQ_B, "FREQ B"),
                                          FP(P5_WHEEL_PW_A, "PW A"), FP(P5_WHEEL_PW_B, "PW B")}},
    {"WHEEL MOD", "FILTER", PIC_FADERS, {FP(P5_WHEEL_FILTER, "FILTER"), F0, F0, F0}},
    {"PLAY", "UNISON", PIC_VOICES, {FP(P5_UNISON, "UNISON"), FP(P5_UNISON_COUNT, "VOICES"), FP(P5_UNISON_DETUNE, "DETUNE"),
                                    FP(P5_RETRIGGER, "PRIORITY")}},
    {"PLAY", "FEEL", PIC_FADERS, {FP(P5_GLIDE, "GLIDE"), FP(P5_BEND, "BEND"), FP(P5_VINTAGE, "VINTAGE"), F0}},
};
static const ui_stop_t ST_EDIT_J106[] = {
    {"DCO", "WAVES", PIC_FADERS, {FJ(J106_RANGE, "RANGE"), FJ(J106_SAW, "SAW"), FJ(J106_PULSE, "PULSE"),
                                  FJ(J106_SUB, "SUB")}},
    {"DCO", "MOD", PIC_FADERS, {FJ(J106_DCO_PWM, "PWM"), FJ(J106_PWM_MODE, "PWM BY"), FJ(J106_DCO_LFO, "LFO"),
                                FJ(J106_NOISE, "NOISE")}},
    {"VCF", "CUTOFF", PIC_FILTER, {FJ(J106_VCF_FREQ, "FREQ"), FJ(J106_VCF_RES, "RES"), FJ(J106_VCF_ENV, "ENV"),
                                   FJ(J106_ENV_POL, "ENV POL")}, 2},
    {"VCF", "MOD", PIC_FILTER, {FJ(J106_VCF_LFO, "LFO"), FJ(J106_VCF_KYBD, "KEYS"), FJ(J106_HPF, "HPF"), F0}, 2},
    {"VCA", "LEVEL", PIC_FADERS, {FJ(J106_VCA_MODE, "MODE"), FJ(J106_VCA_LEVEL, "LEVEL"), FJ(J106_CHORUS, "CHORUS"),
                                  F0}},
};
#define FM_OP(o, n) \
    {n, "LEVEL", PIC_ALGO, {FM(o, FP_OL, "LEVEL"), FM(o, FP_OP, "ON"), FM(o, FP_KVS, "VEL"), FM(o, FP_AMS, "AMS")}, o}, \
    {n, "FREQ", PIC_ALGO, {FM(o, FP_MODE, "MODE"), FM(o, FP_FC, "COARSE"), FM(o, FP_FF, "FINE"), FM(o, FP_DET, "DETUNE")}, o}, \
    {n, "RATES", PIC_FMEG, {FM(o, FP_R1, 0), FM(o, FP_R1 + 1, 0), FM(o, FP_R1 + 2, 0), FM(o, FP_R1 + 3, 0)}, o}, \
    {n, "LEVELS", PIC_FMEG, {FM(o, FP_L1, 0), FM(o, FP_L1 + 1, 0), FM(o, FP_L1 + 2, 0), FM(o, FP_L1 + 3, 0)}, o}, \
    {n, "SCALING", PIC_FADERS, {FM(o, FP_BP, "BREAK"), FM(o, FP_LD, "L DEPTH"), FM(o, FP_RD, "R DEPTH"), \
                                FM(o, FP_RS, "RATE SCL")}, o}, \
    {n, "CURVES", PIC_FADERS, {FM(o, FP_LC, "L CURVE"), FM(o, FP_RC, "R CURVE"), F0, F0}, o}
static const ui_stop_t ST_EDIT_FM6[] = {
    {"VOICE", "ALGORITHM", PIC_ALGO, {FV(FP_ALG, "ALGO"), FV(FP_FB, "FEEDBK"), FV(FP_TRNSP, "SHIFT"),
                                      FV(FP_OKS, "SYNC")}, 6},
    FM_OP(0, "OP1"), FM_OP(1, "OP2"), FM_OP(2, "OP3"), FM_OP(3, "OP4"), FM_OP(4, "OP5"), FM_OP(5, "OP6"),
    {"MACROS", "1", PIC_FADERS, {FT(P_E0, 0), FT(P_E1, 0), FT(P_E2, 0), FT(P_E3, 0)}},
    {"MACROS", "2", PIC_FADERS, {FT(P_E4, 0), FT(P_E5, 0), FT(P_E6, 0), FT(P_E7, 0)}},
    {"FUNCTION", "BEND", PIC_FADERS, {FN(FN_PBUP, "BEND UP"), FN(FN_PBDN, "BEND DN"), FN(FN_PBSTEP, "STEP"),
                                      FN(FN_ENGINE, "ENGINE")}},
    {"FUNCTION", "GLIDE", PIC_FADERS, {FN(FN_PMODE, "MODE"), FN(FN_PTIME, "TIME"), FN(FN_GLISS, "GLISS"),
                                       FN(FN_VNORM, "DX VEL")}},
    {"FUNCTION", "WHEEL", PIC_FADERS, {FN(FN_MWR, "WHEEL"), FN(FN_MWA, "DEST"), FN(FN_ATR, "PRESS"),
                                       FN(FN_ATA, "DEST")}},
    {"FUNCTION", "PEDALS", PIC_FADERS, {FN(FN_FCR, "FOOT"), FN(FN_FCA, "DEST"), FN(FN_BCR, "BREATH"),
                                        FN(FN_BCA, "DEST")}},
};
#undef FM_OP
#define CZ_LINE(l, n) \
    {n, "WAVES", PIC_FADERS, {FC(LCZ_LBASE(l) + LCZ_W1, "WAVE"), FC(LCZ_LBASE(l) + LCZ_W2, "WAVE 2"), \
                              FC(LCZ_WIN(l), "WINDOW"), FC(LCZ_LBASE(l) + LCZ_LEVEL, "LEVEL")}}, \
    {n, "KEYS", PIC_FADERS, {FC(LCZ_LBASE(l) + LCZ_KW, "WAVE"), FC(LCZ_LBASE(l) + LCZ_KA, "AMP"), F0, F0}}, \
    {n, "VELOCITY", PIC_FADERS, {FC(LCZ_LBASE(l) + LCZ_VP, "PITCH"), FC(LCZ_LBASE(l) + LCZ_VW, "WAVE"), \
                                 FC(LCZ_LBASE(l) + LCZ_VA, "AMP"), F0}}
static const ui_stop_t ST_EDIT_CZ[] = {
    {"TONE", "LINES", PIC_FADERS, {FC(LCZ_LINE, "LINES"), FC(LCZ_MOD, "MOD"), FC(LCZ_OCT, "OCTAVE"), F0}},
    {"TONE", "DETUNE", PIC_FADERS, {FC(LCZ_SIGN, "SIGN"), FC(LCZ_DOCT, "OCTAVE"), FC(LCZ_NOTE, "NOTE"), FC(LCZ_FINE, "FINE")}},
    CZ_LINE(0, "LINE 1"), CZ_LINE(1, "LINE 2"),
};
#undef CZ_LINE

static const ui_stop_t ST_GLO[] = {
    {"", "TEMPO", PIC_READOUT, {FG(G_BPM, "TEMPO"), FG(G_CLOCK, "CLOCK"), FG(G_SWING, "SWING"), F0}},
#ifdef SAADET_SMK37
    {"", "OUTPUT", PIC_READOUT, {FT(P_LEVEL, "LEVEL"), FT(P_PAN, "PAN"), FT(P_SPRD, "SPREAD"), FD(DEV_MASTER, "MASTER")}},
#else
    {"", "OUTPUT", PIC_READOUT, {FT(P_LEVEL, "LEVEL"), FT(P_PAN, "PAN"), FT(P_SPRD, "SPREAD"), F0}},
#endif
    {"", "PARTS", PIC_READOUT, {FG(G_PMODE, "MODE"), FD(DEV_PART, "PART"), FG(G_PSPLIT, "SPLIT"), FG(G_PCH2, "B CH")}},
    {"", "TUNING", PIC_READOUT, {FG(G_A4, "A4"), FG(G_TUNE, "FINE"), F0, F0}},
#ifdef SAADET_FM1_BASE
    {"", "MIDI", PIC_READOUT, {FG(G_ROUTE, "IN CH"), FG(G_DRUMCH, "OUT CH"), FD(DEV_THRU, "THRU"), F0}},
#else
    {"", "MIDI", PIC_READOUT, {FG(G_ROUTE, "IN CH"), FG(G_DRUMCH, "OUT CH"), F0, F0}},
#endif
#ifdef SAADET_FM1_BASE
    {"", "BLE", PIC_BLE, {{FK_BLE, BL_RADIO, "RADIO", 0}, {FK_BLE, BL_DROP, "DROP", 0},
                          {FK_BLE, BL_FORGET, "FORGET", 0}, {FK_BLE, BL_RETRY, "RETRY", 0}}},
    {"", "DEV", PIC_BTDEV, {{FK_BLE, BL_PICK, "DEVICE", 0}, F0,
                            {FK_BLE, BL_RADIO, "RADIO", 0}, {FK_BLE, BL_ACTION, "LINK", 0}}},
#endif
    {"", "VOICES", PIC_VOICES, {FD(DEV_BUDGET, "BUDGET"), F0, F0, F0}},
    {"", "DISPLAY", PIC_READOUT, {FD(DEV_SLEEP, "SLEEP"), FD(DEV_LIGHTS, "LIGHTS"), F0, F0}},
#ifdef SAADET_SMK37
    {"", "KEYS", PIC_READOUT, {FD(DEV_VEL, "VELOCITY"), FD(DEV_PADS, "PADS"), FD(DEV_TRS, "TRS OUT"), F0}},
#endif
    {"", "ERASE", PIC_ERASE, {FL(LF_ERASE, "ERASE ALL"), F0, F0, F0}},
};
static const ui_stop_t ST_ARP[] = {
    {"", "PATTERN", PIC_ARP, {FT(P_AMODE, "MODE"), FT(P_ARATE, "RATE"), FT(P_AOCT, "OCTAVES"), FT(P_AGATE, "GATE")}},
    {"", "FEEL", PIC_ARP, {FT(P_ASWING, "SWING"), FT(P_APROB, "CHANCE"), FT(P_AORDER, "ORDER"), FG(G_BPM, "TEMPO")}},
};
static const ui_stop_t ST_SEQ[] = {
    {"", "CHORD", PIC_CHORD, {FT(P_CHRD, "CHORD"), FT(P_VOIC, "VOICING"), FT(P_STRUM, "STRUM"), FT(P_STROKE, "STROKE")}},
};
#define HY_OSC_STOPS(n, b) \
    {n, "WAVE", PIC_FADERS, {FH(b + HO_MODE, "MODE"), FH(b + HO_WAVE, "WAVE"), FH(b + HO_SCAN, "SCAN"), FH(b + HO_SEMI, "SEMI")}}, \
    {n, "TUNE", PIC_FADERS, {FH(b + HO_CENT, "CENT"), FH(b + HO_KEY, "KEYS"), F0, F0}}, \
    {n, "SLOTS 1-4", PIC_HYSLOTS, {FH(b + HO_L0, "SLOT 1"), FH(b + HO_L0 + 1, "SLOT 2"), FH(b + HO_L0 + 2, "SLOT 3"), FH(b + HO_L0 + 3, "SLOT 4")}}, \
    {n, "SLOTS 5-8", PIC_HYSLOTS, {FH(b + HO_L0 + 4, "SLOT 5"), FH(b + HO_L0 + 5, "SLOT 6"), FH(b + HO_L0 + 6, "SLOT 7"), FH(b + HO_L0 + 7, "SLOT 8")}}
#define HY_MUT_STOPS(n, b) \
    {n, "MODE", PIC_FADERS, {FH(b + HM_MODE, "MODE"), FH(b + HM_DEPTH, "DEPTH"), FH(b + HM_RATIO, 0), FH(b + HM_WET, "WET")}}, \
    {n, "SOURCE", PIC_FADERS, {FH(b + HM_SRC, "FROM"), FH(b + HM_AUX, 0), F0, F0}}
static const ui_stop_t ST_EDIT_HY[] = {
    HY_OSC_STOPS("OSC 1", HY_O1), HY_MUT_STOPS("MUTANT 1", HY_M1), HY_MUT_STOPS("MUTANT 2", HY_M2),
    HY_OSC_STOPS("OSC 2", HY_O2), HY_MUT_STOPS("MUTANT 3", HY_M3), HY_MUT_STOPS("MUTANT 4", HY_M4),
    {"OSC 3", "WAVE", PIC_FADERS, {FH(HY_O3_WAVE, "WAVE"), FH(HY_O3_SEMI, "SEMI"), FH(HY_O3_CENT, "CENT"), FH(HY_O3_KEY, "KEYS")}},
    /* the WAVE LAB (hydra_lab.c): one user wave by ear, a chain from the wave it started as (arg: the spectrum's page) */
    {"WAVE LAB", "WAVE", PIC_HYWAVE, {FW(HL_WAVE, "WAVE"), FW(HL_FROM, "FROM"), FW(HL_MIX, "MIX"), FW(HL_REVERT, "REVERT")}},
    {"WAVE LAB", "SHAPE", PIC_HYWAVE, {FW(HL_SKEW, "SKEW"), FW(HL_SYNC, "SYNC"), FW(HL_FOLD, "FOLD"), FW(HL_CRUSH, "CRUSH")}},
    {"WAVE LAB", "SPECTRUM", PIC_HYSPEC, {FW(HL_TILT, "TILT"), FW(HL_EVENS, "EVENS"), FW(HL_FORM, "FORMNT"), FW(HL_PEAK, "PEAK")}},
    {"WAVE LAB", "HARMONICS 1-4", PIC_HYSPEC, {FW(HL_H1, "H1"), FW(HL_H1 + 1, "H2"), FW(HL_H1 + 2, "H3"), FW(HL_H1 + 3, "H4")}, 1},
    {"WAVE LAB", "HARMONICS 5-8", PIC_HYSPEC, {FW(HL_H1 + 4, "H5"), FW(HL_H1 + 5, "H6"), FW(HL_H1 + 6, "H7"), FW(HL_H1 + 7, "H8")}, 2},
    {"WAVE LAB", "NAME", PIC_HYNAME, {FW(HL_CURSOR, "CURSOR"), FW(HL_LETTER, "LETTER"), F0, F0}},
    {"MIXER", "LEVELS", PIC_FADERS, {FH(HY_LVL, "OSC 1"), FH(HY_LVL + 1, "OSC 2"), FH(HY_LVL + 2, "OSC 3"), FH(HY_AMP, "LEVEL")}},
    {"MIXER", "RING", PIC_FADERS, {FH(HY_LVL + 3, "RING"), FH(HY_RING_A, "RING A"), FH(HY_RING_B, "RING B"), F0}},
    {"MIXER", "NOISE", PIC_FADERS, {FH(HY_LVL + 4, "NOISE"), FH(HY_NOISE, "COLOR"), F0, F0}},
    {"MIXER", "TO FILTER 2", PIC_FADERS, {FH(HY_BAL, "OSC 1"), FH(HY_BAL + 1, "OSC 2"), FH(HY_BAL + 2, "OSC 3"), FH(HY_BAL + 3, "RING")}},
    {"MIXER", "ROUTE", PIC_FADERS, {FH(HY_BAL + 4, "NOISE"), FH(HY_ROUTE, "ROUTE"), F0, F0}},
    {"FILTER 1", "CUTOFF", PIC_FILTER, {FH(HY_F1_TYPE, "TYPE"), FH(HY_F1_CUT, "CUTOFF"), FH(HY_F1_RES, "RES"), FH(HY_F1_ENV, "ENV 1")}, 3},
    {"FILTER 1", "DRIVE", PIC_FILTER, {FH(HY_F1_DRIVE, "DRIVE"), FH(HY_F1_DPOS, "ORDER"), FH(HY_F1_KEY, "KEYS"), FH(HY_F1_VEL, "VEL")}, 3},
    {"FILTER 2", "MORPH", PIC_FADERS, {FH(HY_F2_TYPE, "TYPE"), FH(HY_F2_MORPH, "PASS"), FH(HY_F2_CUT, "CUTOFF"), FH(HY_F2_RES, "RES")}},
    {"FILTER 2", "MOD", PIC_FADERS, {FH(HY_F2_ENV, "ENV 1"), FH(HY_F2_KEY, "KEYS"), F0, F0}},
};
#undef HY_OSC_STOPS
#undef HY_MUT_STOPS
static const ui_stop_t ST_PLAY[] = {
    {"", "VOICE", PIC_VOICES, {FT(P_VOICE, "MODE"), FT(P_GLIDE, "GLIDE"), FT(P_GLMODE, "GLIDE BY"), FT(P_PRIO, "PRIORITY")}},
    {"", "UNISON", PIC_VOICES, {FT(P_DETUNE, "DETUNE"), FT(P_ALLOC, "ASSIGN"), F0, F0}},
};

/* the engine's place in the colours and the tags: 0 PROPHET, 1 FM6, 2 CZ-1, 3 JUNO-106, 4 HYDRA */
static uint32_t eng_tint(uint32_t e) { return e == ENGI_FM6 ? 1u : e == ENGI_CZ ? 2u : e == ENGI_J106 ? 3u : e == ENGI_HYDRA ? 4u : 0u; }
static const char *const ENG_TAG[5] = {"P5", "FM6", "CZ-1", "J106", "HYDRA"};
static int sx_staged(void);                             /* sound.c: a dump waits for its slots */

static ui_place_t place_of(uint32_t p)
{
    static const char *const NAME[PL_N] = {"HOME", "LIBRARY", "SCALE", "ENV", "LFO", "EDIT", "GLOBAL", "SAVE", "ARP",
                                           "CHORD", "PLAY"};
    uint32_t e = eng_tint(TSEL->eng_req);
    ui_place_t r = {NAME[p % PL_N], 0, 0};
#define SET(t) (r.stop = (t), r.n = (uint8_t)NELEM(t))
    switch (p) {
    case PL_LIB: SET(ST_LIB); break;
    case PL_SAVE:
        if (sx_staged()) { SET(ST_IMPORT); r.name = "IMPORT"; }
        else { SET(ST_SAVE); if (trk_n() == 1u) r.n = 1; }   /* (a performance is both parts) */
        break;
    case PL_SCL: SET(ST_SCL); break;
    case PL_ENV:
        if (e == 0u) SET(ST_ENV_P5); else if (e == 1u) SET(ST_ENV_FM6); else if (e == 2u) SET(ST_ENV_CZ); else if (e == 3u) SET(ST_ENV_J106); else SET(ST_ENV_HY);
        break;
    case PL_LFO:
        if (e == 0u) SET(ST_LFO_P5); else if (e == 1u) SET(ST_LFO_FM6); else if (e == 2u) SET(ST_LFO_CZ); else if (e == 3u) SET(ST_LFO_J106); else SET(ST_LFO_HY);
        break;
    case PL_EDIT:
        if (e == 0u) SET(ST_EDIT_P5); else if (e == 1u) SET(ST_EDIT_FM6); else if (e == 2u) SET(ST_EDIT_CZ); else if (e == 3u) SET(ST_EDIT_J106); else SET(ST_EDIT_HY);
        break;
    case PL_GLO: SET(ST_GLO); break;
    case PL_ARP: SET(ST_ARP); break;
    case PL_SEQ: SET(ST_SEQ); break;
    case PL_PLAY: SET(ST_PLAY); break;
    }
#undef SET
    return r;
}

/* HOME's macros, the engine's eight in two rows of four (a turn shows the name and value over the whole screen). HOME's
 * four knobs edit one row, SELECT on HOME picks which; the SMK-37's 8 KNOBS bank has both at once (ui_smk.c), K5..K8
 * the second. HYDRA's are its own eight macros, named and routed by the sound (LFO > MACRO 1..8) */
static const ui_field_t HOME_F[5][8] = {
    {FP(P5_CUTOFF, "CUTOFF"), FP(P5_RESONANCE, "RESONANCE"), FP(P5_ENV_FILTER, "ENV AMOUNT"), FP(P5_RELEASE_AMP, "RELEASE"),
     FP(P5_ATTACK_AMP, "ATTACK"), FP(P5_DECAY_FILTER, "FILT DECAY"), FP(P5_FINE_B, "B FINE"), FP(P5_LFO_RATE, "LFO RATE")},
    {FT(P_E2, "MOD LEVEL"), FT(P_E3, "MOD RATIO"), FT(P_E4, "MOD EG"), FT(P_E6, "DETUNE"),
     FT(P_E0, "ALGO"), FT(P_E1, "FEEDBACK"), FT(P_E5, "VELOCITY"), FV(FP_LFS, "LFO SPEED")},
    {FT(P_LD_PIT, "VIBRATO"), FT(P_LRATE, "LFO RATE"), FT(P_GLIDE, "GLIDE"), FT(P_LEVEL, "LEVEL"),
     FC(LCZ_LBASE(0) + LCZ_W1, "WAVE"), FC(LCZ_WIN(0), "WINDOW"), FC(LCZ_FINE, "DETUNE"), FC(LCZ_LBASE(1) + LCZ_LEVEL, "LEVEL 2")},
    {FJ(J106_VCF_FREQ, "CUTOFF"), FJ(J106_VCF_RES, "RESONANCE"), FJ(J106_VCF_ENV, "ENV AMOUNT"), FJ(J106_ENV_R, "RELEASE"),
     FJ(J106_ENV_A, "ATTACK"), FJ(J106_LFO_RATE, "LFO RATE"), FJ(J106_VCF_LFO, "VCF LFO"), FJ(J106_DCO_PWM, "PWM")},
    {FT(P_E0, 0), FT(P_E1, 0), FT(P_E2, 0), FT(P_E3, 0), FT(P_E4, 0), FT(P_E5, 0), FT(P_E6, 0), FT(P_E7, 0)},
};

/* the buttons' places (panel.c B_*); REC is no place (it latches). FX opens the library */
static uint32_t place_of_button(uint32_t b)
{
    switch (b) {
    case B_FX: return PL_LIB;
    case B_SCL: return PL_SCL;
    case B_ENV: return PL_ENV;
    case B_LFO: return PL_LFO;
    case B_EDIT: return PL_EDIT;
    case B_GLO: return PL_GLO;
    case B_HOME: return PL_HOME;
    case B_SAVE: return PL_SAVE;
    case B_ARP: return PL_ARP;
    case B_SEQ: return PL_SEQ;
    case B_PLAY: return PL_PLAY;
    }
    return PL_N;
}
static uint32_t button_of_place(uint32_t p)
{
    static const uint8_t B[PL_N] = {B_HOME, B_FX, B_SCL, B_ENV, B_LFO, B_EDIT, B_GLO, B_SAVE, B_ARP, B_SEQ, B_PLAY};
    return B[p % PL_N];
}

/* the entries the held button lists (and the keys light): the objects, or the pages of a place of one object.
 * Entry k's first stop into s[k]; returns the count */
static int obj_starts(const ui_place_t *pl, uint32_t i) { return !i || !str_eq(pl->stop[i].obj, pl->stop[i - 1u].obj); }
static uint32_t place_objects(const ui_place_t *pl)
{
    uint32_t i, n = 0;
    for (i = 0; i < pl->n; i++)
        n += (uint32_t)obj_starts(pl, i);
    return n;
}
static uint32_t place_entries(const ui_place_t *pl, uint8_t *s, uint32_t max)
{
    uint32_t i, n = 0, many = place_objects(pl) > 1u;
    for (i = 0; i < pl->n && n < max; i++)
        if (!many || obj_starts(pl, i))
            s[n++] = (uint8_t)i;
    return n;
}
static const char *entry_name(const ui_place_t *pl, uint32_t stop)
{
    const ui_stop_t *st = &pl->stop[stop];
    return place_objects(pl) > 1u ? st->obj : st->page;
}
/* the header's second word (the object, or the page of a place of one object), the stop's dots: its object's pages */
static const char *stop_title(const ui_place_t *pl, uint32_t s) { return pl->stop[s].obj[0] ? pl->stop[s].obj : pl->stop[s].page; }
static void stop_dots(const ui_place_t *pl, uint32_t s, uint8_t *n, uint8_t *at)
{
    uint32_t a = s, b = s;
    while (a && !obj_starts(pl, a)) a--;
    while (b + 1u < pl->n && !obj_starts(pl, b + 1u)) b++;
    *n = (uint8_t)(b - a + 1u);
    *at = (uint8_t)(s - a);
}
