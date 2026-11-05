/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* HYDRA's patch as the panel shows it (eng_hydra.c, ui_pages.c): a descriptor per native byte, raw = value - min. Editing
 * one field leaves every other byte as it is. Included by params.c */
static const char *const N_HY_OMODE[] = {"SINGLE", "SCAN"};
static const char *const N_HY_MUT[] = {"OFF", "FM", "STACK", "SYNC", "PW", "SQZ", "WARP", "HARM"};
static const char *const N_HY_SRC[] = {"OSC 1", "OSC 2", "OSC 3", "RING", "NOISE"};
static const char *const N_HY_OSC[] = {"OSC 1", "OSC 2", "OSC 3"};
static const char *const N_HY_NOISE[] = {"WHITE", "PINK", "BROWN"};
static const char *const N_HY_ROUTE[] = {"SERIES", "PARLLEL"};
static const char *const N_HY_F1[] = {"LAD12", "LAD24", "FAT12", "FAT24", "MS LP", "MS HP", "LPG", "3P LP",
                                      "3P BP", "3P HP", "VOWEL"};
static const char *const N_HY_F2[] = {"BAND", "NOTCH"};
static const char *const N_HY_DPOS[] = {"PRE", "POST"};
_Static_assert(NELEM(N_HY_MUT) == MU_N && NELEM(N_HY_SRC) == HYS_N && NELEM(N_HY_F1) == F1_N, "HYDRA names");
#define HY_PW(df) {"WAVE", F_ENUM, 0, (int16_t)(HY_NWAVE - 1u), df, HY_SLOT_NAMES + 1, 0}   /* (the slots' names past EMPTY) */
#define HY_OSC(b) \
    [b + HO_MODE] = PE("MODE", N_HY_OMODE, 0), [b + HO_WAVE] = HY_PW(HYW_SAW), \
    [b + HO_L0] = PE("SLOT 1", HY_SLOT_NAMES, 0), [b + HO_L0 + 1] = PE("SLOT 2", HY_SLOT_NAMES, 0), \
    [b + HO_L0 + 2] = PE("SLOT 3", HY_SLOT_NAMES, 0), [b + HO_L0 + 3] = PE("SLOT 4", HY_SLOT_NAMES, 0), \
    [b + HO_L0 + 4] = PE("SLOT 5", HY_SLOT_NAMES, 0), [b + HO_L0 + 5] = PE("SLOT 6", HY_SLOT_NAMES, 0), \
    [b + HO_L0 + 6] = PE("SLOT 7", HY_SLOT_NAMES, 0), [b + HO_L0 + 7] = PE("SLOT 8", HY_SLOT_NAMES, 0), \
    [b + HO_SCAN] = PD("SCAN", F_PCT, 0, 127, 0), [b + HO_SEMI] = PD("SEMI", F_SEMI, -HY_SEMI0, HY_SEMI0, 0), \
    [b + HO_CENT] = {"CENT", F_INT, -HY_CENT0, HY_CENT0, 0, 0, "ct"}, [b + HO_KEY] = {"KEY", F_INT, 0, 200, 100, 0, "%"}
#define HY_MUTANT(b) \
    [b + HM_MODE] = PE("MODE", N_HY_MUT, 0), [b + HM_SRC] = PE("FROM", N_HY_SRC, HYS_OSC3), \
    [b + HM_RATIO] = PD("RATIO", F_INT, 0, 127, 32), [b + HM_DEPTH] = PD("DEPTH", F_PCT, 0, 127, 0), \
    [b + HM_AUX] = PD("AUX", F_PCT, 0, 127, 0), [b + HM_WET] = PD("WET", F_PCT, 0, 127, 127)
#define HY_ENVAMT(l) PD(l, F_INT, -HY_ENV0, HY_ENV0, 0)
static const param_desc_t HY_PANEL[HY_E1_REL + 1] = {   /* (the name, the reserved bytes: none) */
    HY_OSC(HY_O1), HY_OSC(HY_O2),
    [HY_O3_WAVE] = HY_PW(HYW_SINE), [HY_O3_SEMI] = PD("SEMI", F_SEMI, -HY_SEMI0, HY_SEMI0, 0),
    [HY_O3_CENT] = {"CENT", F_INT, -HY_CENT0, HY_CENT0, 0, 0, "ct"}, [HY_O3_KEY] = {"KEY", F_INT, 0, 200, 100, 0, "%"},
    HY_MUTANT(HY_M1), HY_MUTANT(HY_M2), HY_MUTANT(HY_M3), HY_MUTANT(HY_M4),
    [HY_RING_A] = PE("RING A", N_HY_OSC, HYS_OSC1), [HY_RING_B] = PE("RING B", N_HY_OSC, HYS_OSC2),
    [HY_NOISE] = PE("COLOR", N_HY_NOISE, 0),
    [HY_LVL] = PD("OSC 1", F_PCT, 0, 127, 100), [HY_LVL + 1] = PD("OSC 2", F_PCT, 0, 127, 0),
    [HY_LVL + 2] = PD("OSC 3", F_PCT, 0, 127, 0), [HY_LVL + 3] = PD("RING", F_PCT, 0, 127, 0),
    [HY_LVL + 4] = PD("NOISE", F_PCT, 0, 127, 0),
    [HY_BAL] = PD("OSC 1>F2", F_PCT, 0, 127, 0), [HY_BAL + 1] = PD("OSC 2>F2", F_PCT, 0, 127, 0),
    [HY_BAL + 2] = PD("OSC 3>F2", F_PCT, 0, 127, 0), [HY_BAL + 3] = PD("RING>F2", F_PCT, 0, 127, 0),
    [HY_BAL + 4] = PD("NOISE>F2", F_PCT, 0, 127, 0), [HY_ROUTE] = PE("ROUTE", N_HY_ROUTE, 0),
    [HY_F1_TYPE] = PE("TYPE", N_HY_F1, F1_LAD24), [HY_F1_CUT] = PD("CUT", F_CUTOFF, 0, 127, 127),
    [HY_F1_RES] = PD("RES", F_PCT, 0, 127, 0), [HY_F1_DRIVE] = PD("DRIVE", F_PCT, 0, 127, 0),
    [HY_F1_DPOS] = PE("ORDER", N_HY_DPOS, 0), [HY_F1_ENV] = HY_ENVAMT("ENV 1"),
    [HY_F1_KEY] = {"KEY", F_INT, 0, 200, 0, 0, "%"}, [HY_F1_VEL] = PD("VEL", F_PCT, 0, 127, 0),
    [HY_F2_TYPE] = PE("TYPE", N_HY_F2, 0), [HY_F2_MORPH] = PD("PASS", F_PCT, 0, 127, 0),
    [HY_F2_CUT] = PD("CUT", F_CUTOFF, 0, 127, 127), [HY_F2_RES] = PD("RES", F_PCT, 0, 127, 0),
    [HY_F2_ENV] = HY_ENVAMT("ENV 1"), [HY_F2_KEY] = {"KEY", F_INT, 0, 200, 0, 0, "%"},
    [HY_AMP] = PD("LEVEL", F_PCT, 0, 127, 100),
    [HY_E1_SC1] = HY_ENVAMT("SCAN 1"), [HY_E1_SC2] = HY_ENVAMT("SCAN 2"), [HY_E1_MU1] = HY_ENVAMT("MUTANT 1"),
    [HY_E1_DLY] = PD("DELAY", F_TIME, 0, 127, 0), [HY_E1_ATK] = PD("ATK", F_TIME, 0, 127, 0),
    [HY_E1_HLD] = PD("HOLD", F_TIME, 0, 127, 0), [HY_E1_DEC] = PD("DEC", F_TIME, 0, 127, 64),
    [HY_E1_SUS] = PD("SUS", F_PCT, 0, 127, 0), [HY_E1_REL] = PD("REL", F_TIME, 0, 127, 50),
};
#undef HY_OSC
#undef HY_MUTANT
#undef HY_ENVAMT
/* a mutant's RATIO and AUX say what they do in its mode */
static const param_desc_t *hy_desc(const track_t *t, uint32_t id)
{
    static param_desc_t d;
    uint32_t f, mode;
    if (id < HY_M1 || id >= HY_M4 + HM_N)
        return id < NELEM(HY_PANEL) ? &HY_PANEL[id] : &HY_PANEL[HY_AMP];
    f = (id - HY_M1) % HM_N;
    mode = hy_patch_of(t)->raw[id - f + HM_MODE];
    d = HY_PANEL[id];
    if (f == HM_AUX)
        d.label = mode == MU_FM ? "FEEDBACK" : mode == MU_SYNC ? "WINDOW" : "AUX";
    if (f == HM_RATIO)
        d.label = mode == MU_WARP ? "PARTS" : mode == MU_HARM ? "RANGE" : "RATIO";
    return &d;
}
/* dim what has no effect on the sound as the patch is (still editable) */
static int hy_ed_active(const track_t *t, uint32_t id)
{
    const uint8_t *p = hy_patch_of(t)->raw;
    if (id < HY_O3) {                                  /* OSC 1, 2 */
        uint32_t b = id < HY_O2 ? HY_O1 : HY_O2, f = id - b, o = b == HY_O2;
        int heard = p[HY_LVL + o] || (p[HY_LVL + HYS_RING] && (p[HY_RING_A] == o || p[HY_RING_B] == o));
        if (!heard)
            for (uint32_t k = 0; k < 4u; k++)
                heard |= p[HY_M1 + k * HM_N + HM_MODE] == MU_FM && p[HY_M1 + k * HM_N + HM_SRC] == o;
        if (!heard)
            return 0;
        if (f == HO_WAVE)
            return !p[b + HO_MODE];
        if (f == HO_SCAN || (f >= HO_L0 && f < HO_L0 + 8u))
            return p[b + HO_MODE] != 0;
        return 1;
    }
    if (id >= HY_M1 && id < HY_M4 + HM_N) {            /* a mutant: its fields as its mode reads them */
        uint32_t f = (id - HY_M1) % HM_N, mode = p[id - f + HM_MODE];
        if (f == HM_MODE)
            return 1;
        if (!mode)
            return 0;
        if (f == HM_SRC)
            return mode == MU_FM;
        if (f == HM_RATIO)
            return mode == MU_SYNC || mode == MU_WARP || mode == MU_HARM;
        if (f == HM_AUX)
            return mode == MU_FM || mode == MU_SYNC;
        return 1;
    }
    if (id == HY_RING_A || id == HY_RING_B)
        return p[HY_LVL + HYS_RING] != 0;
    if (id == HY_NOISE)
        return p[HY_LVL + HYS_NOISE] != 0;
    if (id >= HY_BAL && id < HY_BAL + HYS_N)
        return p[HY_LVL + (id - HY_BAL)] != 0;
    if (id == HY_F1_DPOS)
        return p[HY_F1_DRIVE] != 0;
    if (id == HY_E1_MU1)
        return p[HY_M1 + HM_MODE] != 0;
    if (id == HY_E1_SC1)
        return p[HY_O1 + HO_MODE] != 0;
    if (id == HY_E1_SC2)
        return p[HY_O2 + HO_MODE] != 0;
    return 1;
}
/* a field's value in its own units; its name (the wave lists' up to seven letters: param_format keeps five) */
static const char *hy_value_name(const track_t *t, uint32_t id, const param_desc_t *d)
{
    return d->fmt == F_ENUM ? d->names[clamp(hy_patch_of(t)->raw[id], 0, d->max)] : 0;
}
static int32_t hy_get(const track_t *t, uint32_t id)
{
    const param_desc_t *d = hy_desc(t, id);
    return clamp(hy_patch_of(t)->raw[id] + d->min, d->min, d->max);
}
static void hy_edit_value(track_t *t, uint32_t id, int32_t v)
{
    const param_desc_t *d;
    if (id >= NELEM(HY_PANEL) || !HY_PANEL[id].label)
        return;
    d = &HY_PANEL[id];
    hy_patch_of(t)->raw[id] = (uint8_t)(clamp(v, d->min, d->max) - d->min);
}
