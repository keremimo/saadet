/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Parameter descriptors, formatting and the page table. */
static const char *const N_LSYNC[] = {"OFF", "4BAR", "2BAR", "1/1", "1/2", "1/4", "1/8", "8T", "1/16", "16T", "1/32"};
static const uint8_t LSYNC_DIV[11] = {0, 9, 8, 7, 6, 0, 1, 4, 2, 5, 3};
static const char *const N_LWAVE[] = {"SIN", "TRI", "SAW", "SQR", "S&H"};
static const char *const N_AMODE[] = {"OFF", "UP", "DN", "UPDN", "RND", "ORD", "REPEAT"};
static const char *const N_DIV[] = {"1/4", "1/8", "1/16", "1/32", "8T", "16T", "1/2", "1/1", "2BAR", "4BAR"};
static const char *const N_RECQ[] = {"OFF", "1/4", "1/8", "1/16", "1/32", "8T", "16T", "1/2", "1/1", "2BAR", "4BAR"};
static const char *const N_LTRIG[] = {"NOTE", "FREE"};
static const char *const N_LPOL[] = {"BI", "UNI"};
static const char *const N_ONOFF[] = {"OFF", "ON"};
static const char *const N_VPOINTS[] = {"AUTO", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13",
                                        "14", "15"};
static const char *const N_QUANT[] = {"OFF", "SNAP", "WHITE", "ALL", "MPC"};   /* Q_OFF .. Q_MPC (seq.c kb_map, midi_map) */
/* chord keys (chord.c): OFF, the diatonic triad / seventh of the track's ROOT and SCALE on the key, fixed shapes */
static const char *const N_CHRD[] = {"OFF", "DIA3", "DIA7", "MAJ", "MIN", "DOM7", "MAJ7", "MIN7", "SUS4", "POW"};
static const char *const N_STRUM[] = {"OFF", "1/8", "1/16", "1/32", "1/64", "1/128", "16T", "32T"};
static const char *const N_STROKE[] = {"DOWN", "UP", "ALT"};
static const char *const N_VOIC[] = {"CLOSE", "OPEN", "INV1", "INV2", "+OCT"};   /* VC_CLOSE .. VC_BASS */
static const char *const N_VOICE[] = {"POLY", "MONO", "LEG", "UNI"};   /* V_POLY .. V_UNISON */
static const char *const N_GLMODE[] = {"RATE", "TIME"};
static const char *const N_PRIO[] = {"LAST", "LOW", "HIGH"};
static const char *const N_ALLOC[] = {"ROT", "REUSE"};
static const char *const N_ORDER[] = {"NOTE", "PLAY"};
static const char *const N_CLOCK[] = {"INT", "USB", "TRS"
#ifdef SAADET_FM1_BASE
    , "BLE"
#endif
};
static const char *const N_MIDI_INPUT[] = {"USB", "TRS"};
static const char *const N_ROUTE[] = {"OMNI", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16"}; /* Keep saved CH1-4/SEL ids; append ranges. */
static const char *const N_NOTE[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static const char *const N_DASH[] = {"--"};
static const char *const N_GO[] = {"--", "GO"};
static const char *const N_BOOT[] = {"OFF", "A", "B", "C", "D"};
static const char *const N_DRUMCH[] = {"OFF", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14",
                                       "15", "16"};
/* GLO > SYSTEM KNOB 2, DRUM: the MIDI channel that plays the first DRUM track within ROUT (none: normal
 * routing), 0 OFF, 1..16; 10 as General MIDI's. A device setting kept with the settings (ext.drumch), not the
 * project's: the column shows drumch_cell (seq.c midi_track) */
static uint8_t settings_drumch = 10;
static int16_t drumch_cell;
/* GLO > MIDI KNOB 3, THRU (fm1-base): what comes in goes out again where it did not come from (platform/app.c
 * sd_midi_thru), 0 OFF. A device setting kept with the settings (favorites row 15 byte 24 bit 6) */
static uint8_t settings_thru;
#ifdef SAADET_SMK37
static uint8_t settings_key_vel, settings_trs_off;   /* GLOBAL > KEYS: 0 NORMAL .. 3 FIXED; the TRS jack silent */
#endif
#define PROJ_TMPL 5                                      /* SLOT 5: the template (project.c tmpl) */
/* SAVE > PROJECT KNOB 2, BOOT: the project power-on loads (OFF, A..D; OFF or an empty slot: the template, if one is
 * saved). A device setting kept with the settings (ext.boot), not the project's: the column shows boot_cell */
static uint8_t settings_boot;
static int16_t settings_budget = VOICE_BUDGET_DEFAULT;
/* GLO > WEIGHTS: budget points per sounding voice of PROPHET, CZ-1 and FM6 (legacy positions stay reserved) (0 AUTO: voice.c's
 * built-in rule) and SAFETY (0: overload never cuts voices, audio.c). Device settings (settings_persist.c). */
#define VOICE_KINDS 5u
static int16_t settings_vpoints[VOICE_KINDS];
static int16_t settings_safety;
/* Device recording preferences: defaults preserve existing behavior. */
#include "recording_preferences.h"
static int16_t boot_cell;
/* modulation matrix (mod.c): sources, destinations (E1..E8 = P_E0..P_E7: shown with the engine's labels) */
static const char *const N_MSRC[] = {"OFF", "LFO", "ENV", "VEL", "KEY", "RAND", "MODW", "AT", "EXPR", "S&H", "SLEW"};
static const char *const N_MDST[] = {"OFF", "PITCH", "CUT", "SHP", "AMP", "PAN", "", "", "", "", "RATE",
                                     "VIB", "E1", "E2", "E3", "E4", "E5", "E6", "E7", "E8", "DEPTH"};
static const char *const N_ENGNAME[] = {"-", "-", MELODEE_LEGACY_EXTRAS ? "PHASE" : "-", "-", "-", MELODEE_LEGACY_EXTRAS ? "VOICE" : "-", MELODEE_LEGACY_EXTRAS ? "TRIO" : "-", MELODEE_LEGACY_EXTRAS ? "WHEEL" : "-", "-", MELODEE_LEGACY_EXTRAS ? "PHYS" : "-",
                                             "DRUM", MELODEE_LEGACY_EXTRAS ? "NOISE" : "-", "FM6", MELODEE_SLICE ? "SLICE" : "-", "-", "CZ-1", "-", "-", "-", "PROPHET",
                                             "-", "JUNO-106"};

#define PD(l, f, mn, mx, df) {l, f, mn, mx, df, 0, 0}
#define PE(l, n, df) {l, F_ENUM, 0, (int16_t)(sizeof(n) / sizeof(n[0]) - 1), df, n, 0}

/* FM6's pages (an FM6 track only): the selected track's patch (fm6_patch), its operator switches and the FM6 function
 * settings, in the DX7's ranges. The operator pages show operator fm6_opsel (the PRESETS knob picks it); the STORE
 * page saves the whole sound into user preset slot fm6_bslot (fm6_store.c). The values are not track parameters: page_desc gives a copy
 * (fm6_cell), fm6_page_put writes it back */
static const char *const N_FM6ALG[32] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15",
                                        "16", "17", "18", "19", "20", "21", "22", "23", "24", "25", "26", "27", "28",
                                        "29", "30", "31", "32"};
static const char *const N_FMCRV[] = {"-LIN", "-EXP", "+EXP", "+LIN"};
static const char *const N_FMMODE[] = {"RATIO", "FIXED"};
static const char *const N_FMLFW[] = {"TRI", "SAWDN", "SAWUP", "SQR", "SIN", "S&H"};
static const char *const N_FMPM[] = {"PEDAL", "ON"};   /* portamento: while CC 65 is down (Dexed), always */
static const char *const N_FMDEST[] = {"-", "P", "A", "PA", "E", "PE", "AE", "PAE"};   /* pitch, amp, EG bias */
static const char *const N_FMENG[] = {"MSFA"};                  /* the sole supported renderer */
static const char *const N_FMONOFF[] = {"OFF", "ON"};
static uint8_t fm6_opsel;
static uint16_t fm6_bslot;                    /* OP1..OP6 = 0..5; user preset slot 0..63 (U01..U64) */
static int16_t fm6_cell[4];
static const param_desc_t FM6_OPD[FP_OP + 1] = {        /* an operator; FP_OP: its switch */
    [FP_R1] = PD("R1", F_INT, 0, 99, 99), [FP_R1 + 1] = PD("R2", F_INT, 0, 99, 99),
    [FP_R1 + 2] = PD("R3", F_INT, 0, 99, 99), [FP_R1 + 3] = PD("R4", F_INT, 0, 99, 99),
    [FP_L1] = PD("L1", F_INT, 0, 99, 99), [FP_L1 + 1] = PD("L2", F_INT, 0, 99, 99),
    [FP_L1 + 2] = PD("L3", F_INT, 0, 99, 99), [FP_L1 + 3] = PD("L4", F_INT, 0, 99, 0),
    [FP_BP] = PD("BRK", F_FMNOTE, 0, 99, 39), [FP_LD] = PD("LDEP", F_INT, 0, 99, 0),
    [FP_RD] = PD("RDEP", F_INT, 0, 99, 0), [FP_LC] = PE("LCRV", N_FMCRV, 0), [FP_RC] = PE("RCRV", N_FMCRV, 0),
    [FP_RS] = PD("RSCL", F_INT, 0, 7, 0), [FP_AMS] = PD("AMS", F_INT, 0, 3, 0), [FP_KVS] = PD("VEL", F_INT, 0, 7, 0),
    [FP_OL] = PD("OUT", F_INT, 0, 99, 99), [FP_MODE] = PE("MODE", N_FMMODE, 0),
    [FP_FC] = PD("CRS", F_FMFRQ, 0, 31, 1), [FP_FF] = PD("FINE", F_FMFRQ, 0, 99, 0),
    [FP_DET] = PD("DTN", F_OFS, 0, 14, 7), [FP_OP] = PE("ON", N_FMONOFF, 1),
};
static const param_desc_t FM6_GD[FP_NAME - FP_PR1] = {  /* the voice: FP_PR1 .. FP_TRNSP */
    PD("R1", F_INT, 0, 99, 99), PD("R2", F_INT, 0, 99, 99), PD("R3", F_INT, 0, 99, 99), PD("R4", F_INT, 0, 99, 99),
    PD("L1", F_INT, 0, 99, 50), PD("L2", F_INT, 0, 99, 50), PD("L3", F_INT, 0, 99, 50), PD("L4", F_INT, 0, 99, 50),
    PE("ALG", N_FM6ALG, 0), PD("FB", F_INT, 0, 7, 0), PE("SYNC", N_FMONOFF, 1),
    PD("SPEED", F_INT, 0, 99, 35), PD("DELAY", F_INT, 0, 99, 0), PD("PMD", F_INT, 0, 99, 0), PD("AMD", F_INT, 0, 99, 0),
    PE("KSYNC", N_FMONOFF, 1), PE("WAVE", N_FMLFW, 0), PD("PMS", F_INT, 0, 7, 3), {"TRNS", F_OFS, 0, 48, 24, 0, "st"},
};
static const param_desc_t FM6_FD[FM6_NFN] = {           /* the FM6 function settings: FN_PBUP .. FN_ENGINE */
    {"BEND+", F_INT, 0, 12, 3, 0, "st"}, {"BEND-", F_INT, 0, 12, 3, 0, "st"}, PD("STEP", F_INT, 0, 12, 0),
    PE("PORTA", N_FMPM, 0), PD("TIME", F_INT, 0, 127, 0), PE("GLISS", N_FMONOFF, 0),
    PD("WHEEL", F_INT, 0, 99, 99), PE("W.DST", N_FMDEST, 1), PD("FOOT", F_INT, 0, 99, 0), PE("F.DST", N_FMDEST, 0),
    PD("BRTH", F_INT, 0, 99, 0), PE("B.DST", N_FMDEST, 0), PD("AFTER", F_INT, 0, 99, 0), PE("A.DST", N_FMDEST, 0),
    PE("DXVEL", N_FMONOFF, 0), PE("ENGIN", N_FMENG, FM6_MODERN),
};
static const param_desc_t FM6_SD[4] = {                 /* the STORE page: the slot, then its actions (OCT+) */
    PD("SLOT", F_INT, 1, UP_SLOTS, 1), PD("STORE", F_INT, 0, 1, 0), PD("SEND", F_INT, 0, 1, 0), PD("INIT", F_INT, 0, 1, 0),
};

/* the frequency of operator fm6_opsel for its CRS and FINE columns: a ratio (CRS: its step 0.5, 1, 2 ..; FINE: the
 * whole ratio) or, FIXED, Hz (CRS: 1, 10, 100, 1000) */
static void fm6_freq_text(char *val, const char **unit, int coarse)
{
    const uint8_t *op = &fm6_patch[song.sel % NTRK][(5u - fm6_opsel % 6u) * FP_OP];
    int32_t c = op[FP_FC], f = coarse ? 0 : op[FP_FF];
    if (!op[FP_MODE]) {
        if (coarse && c)
            fmt_int(val, c);
        else
            fmt_fix(val, (c ? c * 100 : 50) * (100 + f) / 100, 2);
        *unit = "";
    } else {                                          /* Hz x 100 = 2^(log2(10) (COARSE % 4 + FINE / 100) + log2(100)) */
        uint32_t h = fm6_pow2(217706 * ((c & 3) * 100 + f) / 100 + 435412 - (16 << 16));
        if (h < 1000u)
            fmt_fix(val, (int32_t)h, 2);
        else if (h < 100000u)
            fmt_fix(val, (int32_t)(h / 10u), 1);
        else
            fmt_int(val, (int32_t)(h / 100u));
        *unit = "Hz";
    }
}

static const param_desc_t TP[P_COUNT] = {
    /* the retired inserts (param_reserved) */
    [P_FXTYPE] = PD("", F_INT, 0, 0, 0), [P_FXMIX] = PD("", F_INT, 0, 0, 0), [P_FXRATE] = PD("", F_INT, 0, 0, 0),
    [P_FXCHAR] = PD("", F_INT, 0, 0, 0), [P_FX2TYPE] = PD("", F_INT, 0, 0, 0), [P_FX2MIX] = PD("", F_INT, 0, 0, 0),
    [P_FX2RATE] = PD("", F_INT, 0, 0, 0), [P_FX2CHAR] = PD("", F_INT, 0, 0, 0), [P_FX3TYPE] = PD("", F_INT, 0, 0, 0),
    [P_FX3MIX] = PD("", F_INT, 0, 0, 0), [P_FX3RATE] = PD("", F_INT, 0, 0, 0), [P_FX3CHAR] = PD("", F_INT, 0, 0, 0),
    [P_LN0] = PD("", F_INT, 0, 0, 0),
    [P_LN1] = PD("", F_INT, 0, 0, 0),
    [P_LN2] = PD("", F_INT, 0, 0, 0),
    [P_LN3] = PD("", F_INT, 0, 0, 0),
    [P_LN4] = PD("", F_INT, 0, 0, 0),
    [P_LN5] = PD("", F_INT, 0, 0, 0),
    [P_LN6] = PD("", F_INT, 0, 0, 0),
    [P_LN7] = PD("", F_INT, 0, 0, 0),
    [P_LEVEL] = PD("LVL", F_DB, 0, 127, 104),
    [P_ATK] = PD("ATK", F_TIME, 0, 127, 10),
    [P_DEC] = PD("DEC", F_TIME, 0, 127, 70),
    [P_SUS] = PD("SUS", F_PCT, 0, 127, 90),
    [P_REL] = PD("REL", F_TIME, 0, 127, 60),
    [P_ED_FLT] = PD("FLT", F_BIPCT, -64, 63, 0),
    [P_ED_PIT] = PD("PIT", F_BIPCT, -64, 63, 0),
    [P_ED_SHP] = PD("SHP", F_BIPCT, -64, 63, 0),
    [P_RECQ] = PD("", F_INT, 0, 0, 0),
    [P_LSYNC] = PE("SYNC", N_LSYNC, 0),
    [P_LTRIG] = PE("TRIG", N_LTRIG, 0),
    [P_LPOL] = PE("POL", N_LPOL, 0),
    [P_SPRD] = PD("SPRD", F_PCT, 0, 127, 0),
    [P_LRATE] = PD("RATE", F_LFOHZ, 0, 127, 60),
    [P_LWAVE] = PE("WAVE", N_LWAVE, 0),
    [P_LPHASE] = PD("PHS", F_INT, 0, 127, 0),
    [P_LFADE] = PD("FADE", F_TIME, 0, 127, 0),
    [P_LD_PIT] = PD("PIT", F_BIPCT, -64, 63, 0),
    [P_LD_FLT] = PD("FLT", F_BIPCT, -64, 63, 0),
    [P_LD_SHP] = PD("SHP", F_BIPCT, -64, 63, 0),
    [P_LD_AMP] = PD("AMP", F_PCT, 0, 127, 0),
    [P_AMODE] = PE("MODE", N_AMODE, 0),
    [P_ARATE] = PE("RATE", N_DIV, 2),
    [P_AOCT] = PD("OCT", F_INT, 1, 4, 1),
    [P_AGATE] = PD("GATE", F_PCT, 1, 127, 64),
    [P_ASWING] = PD("SWG", F_PCT, 0, 100, 0),
    [P_APROB] = PD("PROB", F_PCT, 0, 127, 127),
    [P_AHOLD] = PE("HOLD", N_ONOFF, 0),
    [P_AORDER] = PE("ORD", N_ORDER, 0),
    [P_ROOT] = PD("ROOT", F_NOTE, 0, 11, 0),
    [P_SCALE] = PE("SCL", N_SCALE, 0),
    [P_QUANT] = PE("QNT", N_QUANT, 0),
    [P_TRANS] = PD("TRN", F_SEMI, -24, 24, 0),
    [P_SLEN] = PD("", F_INT, 0, 0, 0),
    [P_SDIV] = PD("", F_INT, 0, 0, 0),
    [P_SSWING] = PD("", F_INT, 0, 0, 0),
    [P_SGATE] = PD("", F_INT, 0, 0, 0),
    [P_RESERVED33] = PD("", F_INT, 0, 127, 0),
    [P_RESERVED34] = PD("", F_INT, 0, 127, 0),
    [P_RESERVED35] = PD("", F_INT, 0, 127, 0),
    [P_RESERVED36] = PD("", F_INT, 0, 127, 0),
    [P_VOICE] = PE("VCE", N_VOICE, 0),
    [P_GLIDE] = PD("GLD", F_TIME, 0, 127, 0),
    [P_GLMODE] = PE("GLMOD", N_GLMODE, 0),
    [P_PRIO] = PE("PRIO", N_PRIO, 0),
    [P_ALLOC] = PE("ALLOC", N_ALLOC, 0),
    [P_DETUNE] = PD("DTUNE", F_INT, 0, 127, 40),
    [P_PAN] = PD("PAN", F_BIPCT, -64, 63, 0),
    [P_MUTE] = PE("MUTE", N_ONOFF, 0),
    [P_RESERVED45] = PD("", F_INT, 0, 127, 0),
    [P_RESERVED46] = PD("", F_INT, 0, 127, 0),
    [P_RESERVED47] = PD("", F_INT, 0, 127, 0),
    [P_RESERVED48] = PD("", F_INT, 0, 127, 0),
#define MSLOT(k) [P_M##k##SRC] = PE("SRC" #k, N_MSRC, 0), [P_M##k##DST] = PE("DST" #k, N_MDST, 0), \
                 [P_M##k##AMT] = PD("AMT" #k, F_BIPCT, -64, 63, 0)
    MSLOT(1), MSLOT(2), MSLOT(3), MSLOT(4),
#undef MSLOT
/* the OP ENV / OP LEVEL values of DIGITAL (ids 61..80, retired): inert, on no page and in no editor layout. Their labels
 * stay: the editor's library files key parameters by label ("ATK#2" ..), so an old file's values still find them */
#define FMOP(k) [P_FM##k##_ATK] = PD("ATK", F_TIME, 0, 127, 0), \
                [P_FM##k##_DEC] = PD("DEC", F_TIME, 0, 127, 0), \
                [P_FM##k##_SUS] = PD("SUS", F_PCT, 0, 127, 127), \
                [P_FM##k##_REL] = PD("REL", F_TIME, 0, 127, 0), \
                [P_FM##k##_LEVEL] = PD("LVL", F_PCT, 0, 127, 127)
    FMOP(1), FMOP(2), FMOP(3), FMOP(4),
#undef FMOP
    [P_FXSEND] = PD("", F_INT, 0, 0, 0), [P_FXSEND2] = PD("", F_INT, 0, 0, 0), [P_FXSEND3] = PD("", F_INT, 0, 0, 0),
    [P_FXTONE] = PD("", F_INT, 0, 0, 0), [P_FX2TONE] = PD("", F_INT, 0, 0, 0), [P_FX3TONE] = PD("", F_INT, 0, 0, 0),
    [P_CHRD] = PE("CHRD", N_CHRD, 0),
    [P_VOIC] = PE("VOIC", N_VOIC, 0),
    [P_STRUM] = PE("STRUM", N_STRUM, 0),
    [P_STROKE] = PE("STROKE", N_STROKE, 0),
    [P_MPCDEG] = PD("DEG", F_INT, 1, 12, 1),         /* its max: the scale's notes (track_desc) */
};

static const param_desc_t GP[G_COUNT] = {
    [G_BPM] = PD("BPM", F_BPM, 40, 240, 120),
    [G_SWING] = PD("SWG", F_PCT, 0, 100, 0),
    [G_CLOCK] = PE("CLK", N_CLOCK, 0),
    [G_TUNE] = PD("TUNE", F_INT, -50, 50, 0),
    [G_RESERVED4] = PD("", F_INT, 0, 127, 0),
    [G_RESERVED5] = PD("", F_INT, 0, 127, 0),
    [G_RESERVED6] = PD("", F_INT, 0, 127, 0),
    [G_RESERVED7] = PD("", F_INT, 0, 127, 0),
    [G_RESERVED8] = PD("", F_INT, 0, 127, 0),
    [G_RESERVED9] = PD("", F_INT, 0, 127, 0),
    [G_RESERVED10] = PD("", F_INT, 0, 127, 0),
    [G_RESERVED11] = PD("", F_INT, 0, 127, 0),
    [G_MIDI] = PE("MIDI", N_MIDI_INPUT, 0),
    [G_DRUMCH] = PD("OUT", F_INT, 1, 16, 1),
    [G_ROUTE] = PE("IN", N_ROUTE, 0),          /* (was "--": stored 0 = CH1-4, as MIDI IN always was) */
    [G_INFO] = PD("CPU", F_INT, 0, 0, 0),
    [G_SLOT] = PD("", F_INT, 0, 0, 0),
    [G_BOOT] = PD("", F_INT, 0, 0, 0),
    [G_LOAD] = PD("", F_INT, 0, 0, 0),
    [G_SAVE] = PD("", F_INT, 0, 0, 0),
    [G_ENGSEL] = PE("ENG", N_ENGNAME, 0),
    [G_A4] = {"A4", F_INT, A4_MIN, A4_MAX, A4_DEFAULT, 0, "Hz"},
    [G_CLRSEQ] = PD("", F_INT, 0, 0, 0),
    [G_INITSND] = PE("INIT", N_GO, 0),
    [G_RESERVED24] = PD("", F_INT, 0, 127, 0),
    [G_RESERVED25] = PD("", F_INT, 0, 127, 0),
    [G_RESERVED26] = PD("", F_INT, 0, 127, 0),
    /* the retired shared inserts (global_reserved) */
    [G_MFXTYPE] = PD("", F_INT, 0, 0, 0), [G_MFXMIX] = PD("", F_INT, 0, 0, 0), [G_MFXRATE] = PD("", F_INT, 0, 0, 0),
    [G_MFXCHAR] = PD("", F_INT, 0, 0, 0), [G_MFX2TYPE] = PD("", F_INT, 0, 0, 0), [G_MFX2MIX] = PD("", F_INT, 0, 0, 0),
    [G_MFX2RATE] = PD("", F_INT, 0, 0, 0), [G_MFX2CHAR] = PD("", F_INT, 0, 0, 0),
    [G_MFX3TYPE] = PD("", F_INT, 0, 0, 0), [G_MFX3MIX] = PD("", F_INT, 0, 0, 0),
    [G_MFX3RATE] = PD("", F_INT, 0, 0, 0), [G_MFX3CHAR] = PD("", F_INT, 0, 0, 0),
    [G_FXRETURN] = PD("", F_INT, 0, 0, 0), [G_FXMODE] = PD("", F_INT, 0, 0, 0),
    [G_BUDGET] = {"BUDGET", F_INT, 8, 32, VOICE_BUDGET_DEFAULT, 0, "pts"},
    [G_MFXTONE] = PD("", F_INT, 0, 0, 0), [G_MFX2TONE] = PD("", F_INT, 0, 0, 0),
    [G_MFX3TONE] = PD("", F_INT, 0, 0, 0),

};

static const param_desc_t *global_desc(uint32_t id) { return &GP[id]; }
static uint32_t scale_count(const track_t *t);          /* seq.c */
static const param_desc_t *track_desc(const track_t *t, uint32_t id)
{
    if (id == P_MPCDEG) {                             /* a degree of the track's scale */
        static param_desc_t degree;
        degree = TP[P_MPCDEG];
        degree.max = (int16_t)scale_count(t);
        return &degree;
    }
    if (id >= P_E0 && id <= P_E7) {                   /* the engine asked for (t->engine follows after a fade) */
        const engine_t *e = ENGINES[eng_idx(t->eng_req)];
        const param_desc_t *d = e->desc ? e->desc(t, id - P_E0) : 0;   /* a mode-dependent label / names */
        return d ? d : &e->edit[id - P_E0];
    }
    return &TP[id];
}

/* the descriptor of parameter id for engine e without the engine's desc hook (the device display
 * only; range and default are the same): what the editor protocol, user presets and projects use */
static const param_desc_t *param_desc_of(uint32_t e, uint32_t id)
{
    return id >= P_E0 ? &ENGINES[e]->edit[id - P_E0] : &TP[id];
}

/* a retired F_ENUM value kept as an alias, so stored values stay valid: SAMPLE SET and GRAIN SRC 1, once
 * TRANH, play PIANO (tools/gen_samples.py SMP_SET_ORIG). It shows the original's name; knobs step over it
 * and the editor's SET lands on the original. -> the value v stands for */
static int32_t enum_orig(const param_desc_t *d, int32_t v)
{
    return d->fmt==F_ENUM && d->names && !d->names[v][0] ? 0 : v;
}

/* a knob moved an F_ENUM from `from` to v: past any alias in that direction (back to `from` at the end) */
static int32_t enum_step(const param_desc_t *d, int32_t from, int32_t v)
{
    int32_t dir = v > from ? 1 : -1;
    while (v != from && enum_orig(d, v) != v)
        v = v + dir > d->max || v + dir < d->min ? from : v + dir;
    return v;
}

static uint8_t fmt_named;        /* the last param_format was a name (F_ENUM, even "4"): ui_draw.c's digits do not roll it */

/* value string (<= 5 chars) and unit for a parameter value */
static void param_format(const param_desc_t *d, int32_t v, char *val, const char **unit)
{
    *unit = "";
    fmt_named = d->fmt == F_ENUM;
    switch (d->fmt) {
    case F_PCT:                                     /* a 0..100 range (SWG) is its value, others a share of 127 */
        fmt_int(val, d->max == 100 ? v : (v * 100 + 63) / 127);
        *unit = "%";
        break;
    case F_BIPCT:
        fmt_int(val, v * 100 / 64);
        if (v > 0) {
            char t[8];
            fmt_int(t, v * 100 / 64);
            val[0] = '+';
            str_cpy(val + 1, t, 6);
        }
        *unit = "%";
        break;
    case F_TIME: {
        uint32_t ms10 = TIME_MS_X10[v & 127];
        if (ms10 < 100u) {
            fmt_fix(val, (int32_t)ms10, 1);
            *unit = "ms";
        } else if (ms10 < 10000u) {
            fmt_int(val, (int32_t)((ms10 + 5u) / 10u));
            *unit = "ms";
        } else {
            fmt_fix(val, (int32_t)(ms10 / 100u), 2);
            if (ms10 >= 100000u)
                fmt_fix(val, (int32_t)(ms10 / 1000u), 1);
            *unit = "s";
        }
        break;
    }
    case F_LFOHZ: {
        uint32_t h = LFO_HZ_X100[v & 127];
        if (h < 1000u)
            fmt_fix(val, (int32_t)h, 2);
        else
            fmt_fix(val, (int32_t)(h / 10u), 1);
        *unit = "Hz";
        break;
    }
    case F_CUTOFF: {
        uint32_t h = CUTOFF_HZ[v & 127];
        if (h < 1000u) {
            fmt_int(val, (int32_t)h);
            *unit = "Hz";
        } else {
            fmt_fix(val, (int32_t)(h / 100u), 1);
            *unit = "kHz";
        }
        break;
    }
    case F_DB:
        if (v <= 0) {
            str_cpy(val, "OFF", 6);
        } else {
            fmt_fix(val, LEVEL_DB_X10[v], 1);
            *unit = "dB";
        }
        break;
    case F_SEMI:
        fmt_int(val, v);
        if (v > 0) {
            char t[8];
            fmt_int(t, v);
            val[0] = '+';
            str_cpy(val + 1, t, 6);
        }
        *unit = "st";
        break;
    case F_ENUM:
        if (d->names == N_ROUTE) {                       /* OMNI or the channel, as the names say */
            v = clamp(v, d->min, d->max);
            str_cpy(val, d->names[v], 6);
        } else
            str_cpy(val, d->names==N_ENGNAME && v==ENGI_PROPHET ? "P5" : d->names==N_ENGNAME && v==ENGI_J106 ? "J106" :
                    d->names[v < d->min ? d->min : v > d->max ? d->max : v], 6);
        if (d->unit)
            *unit = d->unit;
        break;
    case F_BPM:
        fmt_int(val, v);
        *unit = "BPM";
        break;
    case F_NOTE:
        str_cpy(val, N_NOTE[v % 12], 6);
        break;
    case F_ONOFF:
        str_cpy(val, N_ONOFF[v ? 1 : 0], 6);
        break;
    case F_STEPS:
        fmt_int(val, v);
        *unit = "STEP";
        break;
    case F_OFS: {                                     /* FM6: 0 at the middle of the range (DTN -7..7, TRNS +-24) */
        int32_t o = v - (d->min + d->max) / 2;
        if (o > 0) {
            val[0] = '+';
            fmt_int(val + 1, o);
        } else {
            fmt_int(val, o);
        }
        if (d->unit)
            *unit = d->unit;
        break;
    }
    case F_FMNOTE:                                    /* the DX7 break point: 0 = A-1, 39 = C3 (MIDI 60) */
        str_cpy(val, N_NOTE[(v + 21) % 12], 6);
        fmt_int(val + str_len(val), (v + 21) / 12 - 1);
        break;
    case F_FMFRQ:                                     /* an operator's frequency: CRS its step, FINE the result */
        fm6_freq_text(val, unit, d == &FM6_OPD[FP_FC]);
        break;
    default:
        if (d->names) {                               /* F_INT with a 0-terminated name list: the range */
            uint32_t k = 0;                           /* split evenly over the names (engine desc hooks) */
            while (d->names[k])
                k++;
            str_cpy(val, d->names[(uint32_t)(clamp(v, d->min, d->max) - d->min) * k / (uint32_t)(d->max - d->min + 1)], 6);
        } else {
            fmt_int(val, v);
        }
        if (d->unit)
            *unit = d->unit;
        break;
    }
}

/* --------------------------------------------------- native panels --- */
#include "cz_edit.h"
#include "prophet_edit.h"
#include "j106_edit.h"
