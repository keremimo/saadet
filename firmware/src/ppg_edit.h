/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* PPG's page fields (ppg_patch.h PG_*): the 2.2's knobs as 0..63, its codes by name. SEMI and ENV 3's AMOUNT show
 * signed; the patch keeps them offset. A field edits the group GROUP selects (the 2.2's A/B button); TABLE is the
 * program's, both groups'. After the group's fields, the program's own: GROUP, KEYB and SPLIT; then V1, voice 1's SEMIT
 * alone (SEMI on TUNE moves all eight voices' together, a chord kept) */
enum { PGX_SEL = PG_RAW, PGX_KEYB, PGX_SPLIT, PGX_V1, PGX_N };
#define PG_SPLIT0 36                                   /* SPLIT shows as a key: the 2.2's lowest C is MIDI 36 */
static const char *const PG_TABLE_NAMES[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13",
                                             "14", "15", "16", "17", "18", "19", "20", "21", "22", "23", "24", "25",
                                             "26", "27", "28", "29", "UPPER", "SAMPLE"};
static const char *const PG_SW_NAMES[] = {"PARA", "FIXED", "ENV 3", "OFF"};
static const char *const PG_DETU_NAMES[] = {"OFF", "1", "2", "3", "4", "5TH", "OCT", "2 OCT"};
static const char *const PG_BI_NAMES[] = {"2ND", "3RD", "5TH", "OCT"};
static const char *const PG_BD_NAMES[] = {"OFF", "PITCH", "FILTER", "WAVES", "SUB", "P+F", "P+W", "F+W"};
static const char *const PG_TO_NAMES[] = {"OFF", "OSC", "SUB", "BOTH"};
static const char *const PG_SHAPE_NAMES[] = {"1", "2", "3", "4"};
static const char *const PG_GROUP_NAMES[] = {"A", "B"};
_Static_assert(NELEM(PG_TABLE_NAMES) == PG_SAMPLE + 1u, "PPG tables");
#define PG_K(n, l) [n] = PD(l, F_INT, 0, 63, 0)
#define PG_C(n, l) [n] = PD(l, F_INT, 0, 7, 0)
static const param_desc_t PPG_PANEL[PGX_N] = {
    [PG_TABLE] = PE("TABLE", PG_TABLE_NAMES, 0), PG_K(PG_OSC, "OSC"), PG_K(PG_SUB, "SUB"), [PG_SW] = PE("SUB", PG_SW_NAMES, 3),
    PG_K(PG_E1W, "ENV 1"), PG_C(PG_KW, "KEYS"), [PG_UW] = PE("UPPER", N_ONOFF, 0),
    [PG_SEMI] = PD("SEMI", F_INT, -24, 24, 0), [PG_DETU] = PE("DETUNE", PG_DETU_NAMES, 0),
    [PG_BI] = PE("BEND", PG_BI_NAMES, 0), [PG_BD] = PE("BEND TO", PG_BD_NAMES, 1),
    PG_K(PG_CUT, "CUTOFF"), PG_K(PG_EMPH, "EMPH"), PG_K(PG_E1F, "ENV 1"), [PG_KF] = PD("KEYS", F_INT, 0, 7, 3),
    PG_K(PG_A1, "ATTACK"), PG_K(PG_D1, "DECAY"), PG_K(PG_S1, "SUSTAIN"), PG_K(PG_R1, "RELEASE"),
    PG_K(PG_A2, "ATTACK"), PG_K(PG_D2, "DECAY"), PG_K(PG_S2, "SUSTAIN"), PG_K(PG_R2, "RELEASE"),
    PG_K(PG_LOUD, "LEVEL"), [PG_KL] = PD("KEYS", F_INT, 0, 7, 4),
    PG_K(PG_A3, "ATTACK"), PG_K(PG_D3, "DECAY"), [PG_E3AMT] = PD("AMOUNT", F_INT, -32, 31, 0),
    [PG_E3TO] = PE("TO", PG_TO_NAMES, 0),
    PG_K(PG_RATE, "RATE"), [PG_SHAPE] = PE("SHAPE", PG_SHAPE_NAMES, 0), PG_K(PG_DELAY, "DELAY"), PG_K(PG_WHEEL, "WHEEL"),
    [PG_MP] = PE("PITCH", PG_TO_NAMES, 0), [PG_MW] = PE("WAVES", N_ONOFF, 0), [PG_MF] = PE("FILTER", N_ONOFF, 0),
    [PG_ML] = PE("LOUD", N_ONOFF, 0),
    [PG_TW] = PE("WAVES", N_ONOFF, 0), [PG_TF] = PE("FILTER", N_ONOFF, 0), [PG_TL] = PE("LOUD", N_ONOFF, 0),
    [PG_TM] = PE("MOD", N_ONOFF, 0),
    [PG_VF] = PE("FILTER", N_ONOFF, 0), [PG_VL] = PE("LOUD", N_ONOFF, 0),
    /* KEYB as the 2.2 shows it, a digit: 0 one group (GROUP's); 1..3 both on each key; 4..8 B below SPLIT, A from it */
    [PGX_SEL] = PE("GROUP", PG_GROUP_NAMES, 0), [PGX_KEYB] = PD("KEYB", F_INT, 0, 8, 0),
    [PGX_SPLIT] = PD("SPLIT", F_KEY, PG_SPLIT0, PG_SPLIT0 + 60, PG_SPLIT0 + 24),
    [PGX_V1] = PD("V1", F_INT, -24, 24, 0), [PG_SEMI2] = PD("V2", F_INT, -24, 24, 0), [PG_SEMI3] = PD("V3", F_INT, -24, 24, 0),
    [PG_SEMI4] = PD("V4", F_INT, -24, 24, 0), [PG_SEMI5] = PD("V5", F_INT, -24, 24, 0), [PG_SEMI6] = PD("V6", F_INT, -24, 24, 0),
    [PG_SEMI7] = PD("V7", F_INT, -24, 24, 0), [PG_SEMI8] = PD("V8", F_INT, -24, 24, 0),
};
static int pg_is_semi(uint32_t id) { return id == PG_SEMI || (id >= PG_SEMI2 && id <= PG_SEMI8); }
#undef PG_K
#undef PG_C
static int32_t ppg_get(const track_t *t, uint32_t id)
{
    const ppg_patch_t *pp = ppg_patch_of(t);
    const uint8_t *p = ppg_group_c(pp, pp->sel);
    if (id == PGX_SEL) return pp->sel;
    if (id == PGX_KEYB) return pp->keyb;
    if (id == PGX_SPLIT) return pp->split + PG_SPLIT0;
    if (id == PGX_V1) id = PG_SEMI;
    if (id >= PG_RAW) return 0;
    return pg_is_semi(id) ? (int32_t)p[id] - PG_SEMI0 : id == PG_E3AMT ? (int32_t)p[id] - PG_E3AMT0 : (int32_t)p[id];
}
static void ppg_edit_value(track_t *t, uint32_t id, int32_t value)
{
    if (id >= PGX_N) return;
    ppg_patch_t *pp = ppg_patch_of(t);
    value = clamp(value, PPG_PANEL[id].min, PPG_PANEL[id].max);
    if (id == PGX_SEL) { pp->sel = (uint8_t)value; return; }
    if (id == PGX_KEYB) { pp->keyb = (uint8_t)value; return; }
    if (id == PGX_SPLIT) { pp->split = (uint8_t)(value - PG_SPLIT0); return; }
    uint8_t *g = ppg_group(pp, pp->sel);
    if (id == PG_SEMI) {                               /* TUNE's SEMI: every voice by as much as voice 1 moves */
        int32_t d = value + PG_SEMI0 - (int32_t)g[PG_SEMI];
        for (uint32_t h = 0; h < 8u; h++) g[pg_semi_id(h)] = (uint8_t)clamp((int32_t)g[pg_semi_id(h)] + d, 0, 48);
        return;
    }
    if (id == PGX_V1) id = PG_SEMI;
    if (pg_is_semi(id)) value += PG_SEMI0;
    if (id == PG_E3AMT) value += PG_E3AMT0;
    if (id == PG_TABLE) pp->raw[id] = pp->rawb[id] = (uint8_t)value;   /* (the program's one wavetable) */
    else g[id] = (uint8_t)value;
}
/* hints, as the other engines': a control whose source or destination is off is dimmed, still editable */
static int ppg_ed_active(const track_t *t, uint32_t id)
{
    const ppg_patch_t *pp = ppg_patch_of(t);
    const uint8_t *p = ppg_group_c(pp, pp->sel);
    int sub = p[PG_SW] != 3u, lfo = p[PG_MP] || p[PG_MW] || p[PG_MF] || p[PG_ML];
    switch (id) {
    case PG_SUB: case PG_DETU: return sub;
    case PG_A3: case PG_D3: case PG_E3AMT: return p[PG_E3TO] || p[PG_SW] == 2u;
    case PG_RATE: case PG_SHAPE: case PG_DELAY: case PG_WHEEL: return lfo;
    case PG_BI: return p[PG_BD] == 1u || (p[PG_BD] >= 4u && p[PG_BD] <= 6u);
    case PGX_SPLIT: return pp->keyb >= 4u;
    default: return 1;
    }
}
