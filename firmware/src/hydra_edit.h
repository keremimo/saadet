/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* HYDRA's patch as the panel shows it (eng_hydra.c, ui_pages.c): a descriptor per native byte, raw = value - min. Editing
 * one field leaves every other byte as it is. The macros' pages (LFO > MACRO 1..8) show a macro's NAME and up to three of
 * its destinations from the patch's shared pool: fields past the patch's bytes (HYV_DEST, HYV_DEPTH), a destination set
 * takes a free slot, NONE gives it back. Included by params.c */
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
#define X(n, b, l) l,
static const char *const N_HY_TGT[] = {HY_TARGETS};
#undef X
_Static_assert(NELEM(N_HY_MUT) == MU_N && NELEM(N_HY_SRC) == HYS_N && NELEM(N_HY_F1) == F1_N, "HYDRA names");
/* every wave by its id: EMPTY, the factory waves, then the user waves by the names the bank gives them (hydra_bank.h) */
static const char *const HY_ALL_NAMES[] = {"EMPTY", HY_WAVE_NAME_LIST, HY_UNAMES};
_Static_assert(NELEM(HY_ALL_NAMES) == HY_USER0 + HY_UWAVES, "a name for every wave");
#define HY_PW(df) {"WAVE", F_ENUM, 0, (int16_t)(HY_NWAVE + HY_UWAVES - 1u), df, HY_ALL_NAMES + 1, 0}   /* (past EMPTY) */
#define HY_OSC(b) \
    [b + HO_MODE] = PE("MODE", N_HY_OMODE, 0), [b + HO_WAVE] = HY_PW(HYW_SAW), \
    [b + HO_L0] = PE("SLOT 1", HY_ALL_NAMES, 0), [b + HO_L0 + 1] = PE("SLOT 2", HY_ALL_NAMES, 0), \
    [b + HO_L0 + 2] = PE("SLOT 3", HY_ALL_NAMES, 0), [b + HO_L0 + 3] = PE("SLOT 4", HY_ALL_NAMES, 0), \
    [b + HO_L0 + 4] = PE("SLOT 5", HY_ALL_NAMES, 0), [b + HO_L0 + 5] = PE("SLOT 6", HY_ALL_NAMES, 0), \
    [b + HO_L0 + 6] = PE("SLOT 7", HY_ALL_NAMES, 0), [b + HO_L0 + 7] = PE("SLOT 8", HY_ALL_NAMES, 0), \
    [b + HO_SCAN] = PD("SCAN", F_PCT, 0, 127, 0), [b + HO_SEMI] = PD("SEMI", F_SEMI, -HY_SEMI0, HY_SEMI0, 0), \
    [b + HO_CENT] = {"CENT", F_INT, -HY_CENT0, HY_CENT0, 0, 0, "ct"}, [b + HO_KEY] = {"KEY", F_INT, 0, 200, 100, 0, "%"}
#define HY_MUTANT(b) \
    [b + HM_MODE] = PE("MODE", N_HY_MUT, 0), [b + HM_SRC] = PE("FROM", N_HY_SRC, HYS_OSC3), \
    [b + HM_RATIO] = PD("RATIO", F_INT, 0, 127, 32), [b + HM_DEPTH] = PD("DEPTH", F_PCT, 0, 127, 0), \
    [b + HM_AUX] = PD("AUX", F_PCT, 0, 127, 0), [b + HM_WET] = PD("WET", F_PCT, 0, 127, 127)
#define HY_ENVAMT(l) PD(l, F_INT, -HY_ENV0, HY_ENV0, 0)
#define HY_SLOTS_PANEL(k) [HY_MA + 2 * (k)] = PD("SLOT", F_INT, 0, 255, 0), [HY_MA + 2 * (k) + 1] = PD("DEPTH", F_INT, -HY_DEPTH0, HY_DEPTH0, 0)
static const param_desc_t HY_PANEL[HY_SPARE] = {       /* (the name, the reserved bytes: none) */
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
    [HY_MNAME] = PE("NAME", HY_MACRO_NAMES, 0), [HY_MNAME + 1] = PE("NAME", HY_MACRO_NAMES, 0),
    [HY_MNAME + 2] = PE("NAME", HY_MACRO_NAMES, 0), [HY_MNAME + 3] = PE("NAME", HY_MACRO_NAMES, 0),
    [HY_MNAME + 4] = PE("NAME", HY_MACRO_NAMES, 0), [HY_MNAME + 5] = PE("NAME", HY_MACRO_NAMES, 0),
    [HY_MNAME + 6] = PE("NAME", HY_MACRO_NAMES, 0), [HY_MNAME + 7] = PE("NAME", HY_MACRO_NAMES, 0),
    HY_SLOTS_PANEL(0), HY_SLOTS_PANEL(1), HY_SLOTS_PANEL(2), HY_SLOTS_PANEL(3), HY_SLOTS_PANEL(4), HY_SLOTS_PANEL(5),
    HY_SLOTS_PANEL(6), HY_SLOTS_PANEL(7), HY_SLOTS_PANEL(8), HY_SLOTS_PANEL(9), HY_SLOTS_PANEL(10), HY_SLOTS_PANEL(11),
    HY_SLOTS_PANEL(12), HY_SLOTS_PANEL(13),
};
#undef HY_SLOTS_PANEL
_Static_assert(HY_MSLOTS == 14, "HY_PANEL lists the slots");
/* the macros' pages: destination j (0..2) of macro m and its depth, ids past the patch's bytes */
#define HY_MDESTS 3u
#define HYV_DEST(m, j) (HY_BYTES + (m) * 2u * HY_MDESTS + (j) * 2u)
#define HYV_DEPTH(m, j) (HYV_DEST(m, j) + 1u)
#define HYV_END HYV_DEST(HY_MACROS, 0)
_Static_assert(HYV_END <= 256u, "a field's id is a byte");
static int hy_field_ok(uint32_t id) { return (id < NELEM(HY_PANEL) && HY_PANEL[id].label) || (id >= HY_BYTES && id < HYV_END); }
/* the slot of macro m's j-th destination, -1 none; the first free slot, -1 none */
static int32_t hy_slot_of(const uint8_t *p, uint32_t m, uint32_t j)
{
    uint32_t k;
    for (k = 0; k < HY_MSLOTS; k++)
        if (HY_SLOT_TGT(p[HY_MA + 2u * k]) && HY_SLOT_MACRO(p[HY_MA + 2u * k]) == m && !j--)
            return (int32_t)k;
    return -1;
}
static int32_t hy_slot_free(const uint8_t *p)
{
    uint32_t k;
    for (k = 0; k < HY_MSLOTS; k++)
        if (!HY_SLOT_TGT(p[HY_MA + 2u * k]))
            return (int32_t)k;
    return -1;
}
static uint32_t hy_slots_used(const uint8_t *p, uint32_t m)
{
    uint32_t n = 0;
    while (n < HY_MSLOTS && hy_slot_of(p, m, n) >= 0)
        n++;
    return n;
}
/* E1..E8 of a HYDRA part: its macros by the names the patch gives them (AUTO: the first destination's) */
static const param_desc_t *hy_macro_desc(const track_t *t, uint32_t k)
{
    static param_desc_t D[NPART][HY_MACROS];
    const uint8_t *p = hy_patch_of(t)->raw;
    param_desc_t *d = &D[(uint32_t)(t - trk) % NPART][k % HY_MACROS];
    int32_t s = hy_slot_of(p, k % HY_MACROS, 0);
    uint32_t n = p[HY_MNAME + k % HY_MACROS];
    *d = ENG_HYDRA.edit[k % HY_MACROS];
    d->label = n && n < HY_NNAME ? HY_MACRO_NAMES[n] : s >= 0 ? N_HY_TGT[HY_SLOT_TGT(p[HY_MA + 2u * (uint32_t)s])] : d->label;
    return d;
}
#undef HY_OSC
#undef HY_MUTANT
#undef HY_ENVAMT
/* a mutant's RATIO and AUX say what they do in its mode; a macro's destinations are numbered */
static const param_desc_t *hy_desc(const track_t *t, uint32_t id)
{
    static param_desc_t d;
    static const char *const DEST[HY_MDESTS] = {"DEST 1", "DEST 2", "DEST 3"}, *const DEP[HY_MDESTS] = {"DEPTH 1", "DEPTH 2", "DEPTH 3"};
    uint32_t f, mode;
    if (id >= HY_BYTES && id < HYV_END) {
        uint32_t j = (id - HY_BYTES) / 2u % HY_MDESTS;
        d = (id - HY_BYTES) & 1u ? (param_desc_t)PD(DEP[j], F_INT, -HY_DEPTH0, HY_DEPTH0, 0) : (param_desc_t)PE(DEST[j], N_HY_TGT, 0);
        return &d;
    }
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
/* dim what has no effect on the sound as the patch is (still editable); a macro's next destination lights while a slot is
 * free, the ones past it and their depths stay dark */
static int hy_ed_active(const track_t *t, uint32_t id)
{
    const uint8_t *p = hy_patch_of(t)->raw;
    if (id >= HY_BYTES && id < HYV_END) {
        uint32_t m = (id - HY_BYTES) / (2u * HY_MDESTS), j = (id - HY_BYTES) / 2u % HY_MDESTS;
        if (hy_slot_of(p, m, j) >= 0)
            return 1;
        return !((id - HY_BYTES) & 1u) && j == hy_slots_used(p, m) && hy_slot_free(p) >= 0;
    }
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
static int32_t hy_get(const track_t *t, uint32_t id);
static const char *hy_value_name(const track_t *t, uint32_t id, const param_desc_t *d)
{
    return d->fmt == F_ENUM ? d->names[clamp(hy_get(t, id), 0, d->max)] : 0;
}
static int32_t hy_get(const track_t *t, uint32_t id)
{
    const uint8_t *p = hy_patch_of(t)->raw;
    const param_desc_t *d = hy_desc(t, id);
    if (id >= HY_BYTES) {                              /* a macro's destination, its depth (none: NONE, 0) */
        int32_t k = id < HYV_END ? hy_slot_of(p, (id - HY_BYTES) / (2u * HY_MDESTS), (id - HY_BYTES) / 2u % HY_MDESTS) : -1;
        if (k < 0)
            return 0;
        return (id - HY_BYTES) & 1u ? (int32_t)p[HY_MA + 2u * (uint32_t)k + 1u] - HY_DEPTH0 : (int32_t)HY_SLOT_TGT(p[HY_MA + 2u * (uint32_t)k]);
    }
    return clamp(p[id] + d->min, d->min, d->max);
}
static void hy_edit_value(track_t *t, uint32_t id, int32_t v)
{
    uint8_t *p = hy_patch_of(t)->raw;
    const param_desc_t *d;
    if (id >= HY_BYTES && id < HYV_END) {
        uint32_t m = (id - HY_BYTES) / (2u * HY_MDESTS), j = (id - HY_BYTES) / 2u % HY_MDESTS;
        int32_t k = hy_slot_of(p, m, j);
        if ((id - HY_BYTES) & 1u) {                    /* a depth: of a destination there is */
            if (k >= 0)
                p[HY_MA + 2u * (uint32_t)k + 1u] = (uint8_t)(clamp(v, -HY_DEPTH0, HY_DEPTH0) + HY_DEPTH0);
            return;
        }
        v = clamp(v, 0, (int32_t)HY_NTGT);
        if (k < 0 && v && j == hy_slots_used(p, m) && (k = hy_slot_free(p)) >= 0)
            p[HY_MA + 2u * (uint32_t)k + 1u] = HY_DEPTH0 + 64u;   /* (a new destination: half its range at a full turn) */
        if (k >= 0) {
            p[HY_MA + 2u * (uint32_t)k] = v ? HY_SLOT(m, v) : 0u;
            if (!v)
                p[HY_MA + 2u * (uint32_t)k + 1u] = HY_DEPTH0;
        }
        return;
    }
    if (id >= NELEM(HY_PANEL) || !HY_PANEL[id].label)
        return;
    d = &HY_PANEL[id];
    p[id] = (uint8_t)(clamp(v, d->min, d->max) - d->min);
}
