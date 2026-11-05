/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
#include "recording_preferences.h"

#define PERSIST_MAGIC 0x50455235u
#define A4_TAG 0x41340000u
#define AUTOSAVE_OFF_TAG 0xA1u
#define WORK_BUDGET_TAG 0x57420000u
#define FX_PREF_ROW 1u
#define FX_PREF_BYTE 24u

typedef struct {
    uint32_t magic, palette, lowcut, zoom;
    panel_t panel;
    uint32_t bold;
    struct { uint8_t factory[16][32]; uint32_t user, filter; } favorites;
    struct {
        uint32_t usb_off, boot;
        int16_t glo[4];
        uint32_t drumch, lights, spare[2];
    } ext;
} persist_t;
_Static_assert(sizeof(persist_t) == 604, "settings record ABI");
_Static_assert(sizeof(((persist_t *)0)->ext) == 32, "settings extension ABI");
static int16_t settings_glo[4];

static int settings_owned(const persist_t *p)
{
    const uint8_t *tag = p->favorites.factory[1] + 28;
    return tag[0] == 'M' && tag[1] == 'D' && tag[2] == 'S' && tag[3] == 1;
}
static uint16_t battery_full_of(const persist_t *p)
{
    const uint8_t *b = p->favorites.factory[FX_PREF_ROW] + FX_PREF_BYTE;
    uint32_t value = b[2] | (uint32_t)b[3] << 8;
    return b[0] == 'B' && b[1] == 1 && value > 0 && value <= 1023 ? (uint16_t)value : 0;
}
static int settings_usb_shape(uint32_t word)
{
    return word <= 3u || (word & ~(0xFF00u | 3u)) == WORK_BUDGET_TAG;
}
static uint32_t settings_budget_of(uint32_t word)
{
    uint32_t value = (word >> 8) & 255u;
    return (word & 0xFFFF0000u) == WORK_BUDGET_TAG && value >= 8u && value <= 32u ?
           value : VOICE_BUDGET_DEFAULT;
}
static uint32_t settings_usb_word(uint32_t flags, uint32_t budget)
{
    return (flags & 3u) | (budget != VOICE_BUDGET_DEFAULT ?
                            WORK_BUDGET_TAG | budget << 8 : 0u);
}
static int settings_legacy_valid(const persist_t *p)
{
    uint32_t usb = p->ext.usb_off;
    return p->magic == PERSIST_MAGIC && palette_stored_ok(p->palette) &&
           p->panel.magic == PANEL_MAGIC && p->lowcut <= 2u && (p->zoom & 255u) <= 1u &&
           (p->zoom <= 255u || (p->zoom >> 16) == 0x5250u) && hold_stored_ok(p->bold) &&
           p->favorites.filter <= 1u && settings_usb_shape(usb) &&
           (usb <= 3u || (((usb >> 8) & 255u) >= 8u && ((usb >> 8) & 255u) <= 32u)) &&
           p->ext.boot <= 4u && p->ext.drumch <= 17u && p->ext.lights <= LIGHTS_N;
}
static int settings_import(persist_t *p, int length)
{
    if (length != (int)sizeof *p || p->magic != PERSIST_MAGIC) return 0;
    settings.magic = SETTINGS_MAGIC;
    settings.palette = palette_from_stored(p->palette);
    p->palette = palette_to_stored(settings.palette);
    settings.lowcut = 0;
    settings.zoom = (p->zoom & 255u) == 1u;
    uint32_t rec = (p->zoom >> 16) == 0x5250u ? (p->zoom >> 8) & 255u : 0u;
    settings_click = (rec & 3u) < 3u ? rec & 3u : 0;
    settings_click_level = ((rec >> 2) & 3u) == 1u ? 0 : ((rec >> 2) & 3u) == 2u ? 2 : 1;
    settings_countin = ((rec >> 4) & 3u) < 3u ? (rec >> 4) & 3u : 0;
    settings_preview = !!(rec & 64u);
    settings_chord_add = !!(rec & 128u);
    panel = p->panel.magic == PANEL_MAGIC ? p->panel : PANEL_DEFAULT;
    settings_hold = hold_from_stored(p->bold);
#ifdef MELODEE_FAVORITES
    memcpy(&favorites, &p->favorites, sizeof favorites);
    favorites.filter = p->favorites.filter == 1u;
    favorites_user_hi = p->ext.spare[0];
#endif
    settings_batt_full = battery_full_of(p);
    uint32_t flags = settings_usb_shape(p->ext.usb_off) ? p->ext.usb_off & 3u : 0u;
    settings_budget = (int16_t)settings_budget_of(p->ext.usb_off);
    p->ext.usb_off = settings_usb_word(flags, (uint32_t)settings_budget);
#if MELODEE_USB_AUDIO
    ua_off = ua_off_want = flags;
#endif
    if (p->ext.boot > 4u) p->ext.boot = 0;
    settings_boot = (uint8_t)p->ext.boot;
    memcpy(settings_glo, p->ext.glo, sizeof settings_glo);
    if (p->ext.drumch > 17u) p->ext.drumch = 0;
    settings_drumch = p->ext.drumch == 0 ? 10 : p->ext.drumch == 17u ? 0 : p->ext.drumch;
    settings_lights = p->ext.lights >= 1u && p->ext.lights <= LIGHTS_N ? p->ext.lights - 1u : LIGHTS_MID;
    uint32_t a4 = p->ext.spare[1] & 0xFFFFu;
    tuning_a4 = (p->ext.spare[1] & 0xFFFF0000u) == A4_TAG && a4 >= A4_MIN && a4 <= A4_MAX ? a4 : A4_DEFAULT;
    settings_screen = p->favorites.factory[15][27] < 8u ? p->favorites.factory[15][27] : 0;
    settings_autosave = p->favorites.factory[15][28] != AUTOSAVE_OFF_TAG;
    settings_thru = !!(p->favorites.factory[15][24] & 64u);
#ifdef SAADET_SMK37
    settings_key_vel = (p->favorites.factory[15][29] & 0xFCu) == 0xA4u ? p->favorites.factory[15][29] & 3u : 0u;
    settings_trs_off = p->favorites.factory[15][30] == 0xA9u;
    settings_pad_lights = (p->favorites.factory[15][31] & 0xF8u) == 0xB0u && (p->favorites.factory[15][31] & 7u) <= 4u ? p->favorites.factory[15][31] & 7u : 2u;
#endif
    const uint8_t *costs = p->favorites.factory[8] + 8;
    int cost_tag = costs[0] == 'W' && costs[1] == 1;
    for (uint32_t i = 0; i < VOICE_KINDS; ++i)
        settings_vpoints[i] = cost_tag && costs[2 + i] <= 15u ? costs[2 + i] : 0;
    settings_safety = cost_tag && !(costs[7] & 1u);
    return 1;
}
static void settings_export(persist_t *p)
{
    p->magic = PERSIST_MAGIC;
    p->palette = palette_to_stored(settings.palette);
    p->lowcut = 0;
    uint32_t rec = settings_click % 3u | (settings_countin % 3u) << 4 |
                   (settings_click_level == 0 ? 1u : settings_click_level == 2 ? 2u : 0u) << 2 |
                   (uint32_t)!!settings_preview << 6 | (uint32_t)!!settings_chord_add << 7;
    p->zoom = (settings.zoom & 255u) | (rec ? 0x52500000u | rec << 8 : 0u);
    p->panel = panel;
    p->bold = hold_to_stored(p->bold, settings_hold);
#ifdef MELODEE_FAVORITES
    memcpy(&p->favorites, &favorites, sizeof favorites);
    p->ext.spare[0] = favorites_user_hi;
#endif
    uint8_t *owner = p->favorites.factory[1] + 28;
    owner[0] = 'M'; owner[1] = 'D'; owner[2] = 'S'; owner[3] = 1;
    uint8_t *batt = p->favorites.factory[FX_PREF_ROW] + FX_PREF_BYTE;
    memset(batt, 0, 4);
    if (settings_batt_full > 0 && settings_batt_full <= 1023u) {
        batt[0] = 'B'; batt[1] = 1;
        batt[2] = (uint8_t)settings_batt_full; batt[3] = (uint8_t)(settings_batt_full >> 8);
    }
#if MELODEE_USB_AUDIO
    uint32_t flags = ua_off;
#else
    uint32_t flags = p->ext.usb_off & 3u;
#endif
    p->ext.usb_off = settings_usb_word(flags, (uint32_t)settings_budget);
    p->ext.boot = settings_boot;
    memcpy(p->ext.glo, settings_glo, sizeof settings_glo);
    p->ext.drumch = settings_drumch == 10 ? 0 : settings_drumch == 0 ? 17 : settings_drumch;
    p->ext.lights = settings_lights == LIGHTS_MID ? 0 : settings_lights + 1u;
    p->ext.spare[1] = tuning_a4 == A4_DEFAULT ? 0 : A4_TAG | (uint32_t)tuning_a4;
    p->favorites.factory[15][27] = settings_screen;
    p->favorites.factory[15][28] = settings_autosave ? 0 : AUTOSAVE_OFF_TAG;
#ifdef SAADET_SMK37
    {   /* (bytes that are not our tags stay as they came: settings keep an opaque legacy tail) */
        uint8_t *vel = &p->favorites.factory[15][29], *trs = &p->favorites.factory[15][30];
        if (settings_key_vel) *vel = (uint8_t)(0xA4u | (settings_key_vel & 3u));
        else if ((*vel & 0xFCu) == 0xA4u && (*vel & 3u)) *vel = 0u;
        if (settings_trs_off) *trs = 0xA9u;
        else if (*trs == 0xA9u) *trs = 0u;
        {
            uint8_t *pads = &p->favorites.factory[15][31];
            if (settings_pad_lights != 2u) *pads = (uint8_t)(0xB0u | (settings_pad_lights & 7u));
            else if ((*pads & 0xF8u) == 0xB0u && (*pads & 7u) <= 4u && (*pads & 7u) != 2u) *pads = 0u;
        }
    }
#endif
    uint8_t *thru = &p->favorites.factory[15][24];
    *thru = (uint8_t)((*thru & ~64u) | (settings_thru ? 64u : 0u));
    uint8_t *costs = p->favorites.factory[8] + 8;
    int custom = !!settings_safety;
    for (uint32_t i = 0; i < VOICE_KINDS; ++i) custom |= settings_vpoints[i] != 0;
    memset(costs, 0, 8);
    if (custom) {
        costs[0] = 'W'; costs[1] = 1;
        for (uint32_t i = 0; i < VOICE_KINDS; ++i) costs[2 + i] = (uint8_t)settings_vpoints[i];
        costs[7] = !settings_safety;
    }
}
