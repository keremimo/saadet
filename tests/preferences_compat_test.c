/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Behavioural oracle captured before the independent preferences replacement. */
#include <assert.h>
#define MELODEE_FLASH 1
#define MELODEE_USB_AUDIO 0
#include "synth_harness.c"
#define SXQ 1024u
static uint32_t so_w, so_r, wire_n;
static uint8_t wire[2048];
static int ota_wire_send(const uint8_t *bytes, uint32_t size)
{
    assert(size <= sizeof wire); memcpy(wire, bytes, size); wire_n = size;
    return 0;
}
#define ED_BK_FLASH_PTR(off) (nor + (off))
static int ota_frame_get(const uint8_t **bytes, uint32_t *size) { (void)bytes; (void)size; return 0; }
static void ota_frame_done(void) {}
#include "../firmware/src/editor.c"
static void request(uint32_t command, const uint8_t *args, uint32_t count)
{
    uint8_t bytes[64] = {0x7D, 0x46, 0x4C};
    assert(count <= sizeof bytes - 4); bytes[3] = command;
    if (count) memcpy(bytes + 4, args, count);
    wire_n = 0; ed_handle(bytes, count + 4);
}

static uint32_t compat_hash = 2166136261u, compat_cases;
static FILE *compat_file;
static void capture(const void *data, size_t size)
{
    const uint8_t *bytes = data;
    for (size_t k = 0; k < size; ++k)
        compat_hash = (compat_hash ^ bytes[k]) * 16777619u;
    if (compat_file) assert(fwrite(data, 1, size, compat_file) == size);
}
static void number(uint32_t value)
{
    uint8_t bytes[4] = {value, value >> 8, value >> 16, value >> 24};
    capture(bytes, sizeof bytes);
}
static void runtime(void)
{
    number(settings.magic); number(settings.palette); number(settings.lowcut); number(settings.zoom);
    number(settings_boot); number(settings_drumch); number(settings_lights); number(settings_hold);
    number(settings_budget); number(settings_batt_full); number(tuning_a4); number(settings_screen);
    number(settings_autosave); number(settings_thru); number(settings_safety);
    number(settings_click); number(settings_click_level); number(settings_countin);
    number(settings_preview); number(settings_chord_add);
    capture(settings_glo, sizeof settings_glo); capture(settings_vpoints, sizeof settings_vpoints);
    capture(&favorites, sizeof favorites); number(favorites_user_hi);
}
static uint32_t random_word(uint32_t *seed)
{
    *seed ^= *seed << 13; *seed ^= *seed >> 17; *seed ^= *seed << 5;
    return *seed;
}
static void settings_cases(void)
{
    uint32_t seed = 0xCAD01234u;
    for (uint32_t k = 0; k < 1024; ++k) {
        persist_t record;
        uint8_t *bytes = (uint8_t *)&record;
        for (size_t j = 0; j < sizeof record; ++j) bytes[j] = random_word(&seed);
        record.magic = k % 17 ? PERSIST_MAGIC : 0x50455234u;
        record.palette = k % 3 ? palette_to_stored(k % NPALETTES) : k % 256;
        record.panel.magic = k % 3 ? PANEL_MAGIC : 0;
        record.lowcut = k % 4;
        record.zoom = k % 2 ? (0x52500000u | ((k & 255) << 8) | (k % 3)) : k % 3;
        record.bold = k % 2 ? HOLD_TAG | (k % 4) : k % 5;
        record.favorites.filter = k % 3;
        record.ext.usb_off = k % 3 ? WORK_BUDGET_TAG | ((k % 40) << 8) | (k % 8) : k % 5;
        record.ext.boot = k % 7; record.ext.drumch = k % 20; record.ext.lights = k % 8;
        record.ext.spare[1] = k % 3 ? A4_TAG | (390 + k % 100) : k;
        record.favorites.factory[15][27] = k % 10;
        record.favorites.factory[15][28] = k % 2 ? AUTOSAVE_OFF_TAG : k;
        uint8_t *batt = record.favorites.factory[FX_PREF_ROW] + FX_PREF_BYTE;
        batt[0] = k % 3 ? 'B' : 'F'; batt[1] = 1;
        batt[2] = k; batt[3] = (k + 1) >> 8;
        uint8_t *weights = record.favorites.factory[8] + 8;
        weights[0] = k % 3 ? 'W' : 0; weights[1] = 1;
        for (uint32_t j = 0; j < VOICE_KINDS; ++j) weights[2 + j] = (k + j) % 20;
        if (k % 2) memcpy(record.favorites.factory[1] + 28, "MDS\1", 4);
        number(settings_owned(&record)); number(settings_legacy_valid(&record));
        int len = k % 13 ? sizeof record : k % 2 ? -1 : sizeof record - 1;
        number(settings_import(&record, len)); capture(&record, sizeof record); runtime();
        settings_export(&record); capture(&record, sizeof record);
        ++compat_cases;
    }
}
static void favourite_cases(void)
{
    static const uint32_t indices[] = {0,1,7,8,23,24,63,64,127,128,200,223,224,255,256,383,384,UINT32_MAX};
    memset(&favorites, 0, sizeof favorites); favorites_user_hi = 0;
    for (uint32_t version = 0; version < 3; ++version) {
        memset(favorites.factory[3], 0xA5, 32);
        if (version) { memcpy(favorites.factory[3] + 28, "J6F", 3); favorites.factory[3][31] = version; }
        for (uint32_t ns = 0; ns <= USER_NATIVE_J106 + 2; ++ns)
            for (uint32_t j = 0; j < sizeof indices / sizeof *indices; ++j)
                for (int on = 0; on < 3; ++on) {
                    number(favorite_has(ns, indices[j])); number(favorite_set(ns, indices[j], on));
                    number(favorite_has(ns, indices[j])); number(favorite_set(ns, indices[j], on));
                    capture(&favorites, sizeof favorites); number(favorites_user_hi); ++compat_cases;
                }
    }
    for (uint32_t tagged = 0; tagged < 2; ++tagged) {
        memset(favorites.factory[14], 0xA5, 32);
        if (tagged) memcpy(favorites.factory[14] + 28, "SCL\1", 4);
        for (uint32_t scale = 0; scale < 100; ++scale)
            for (int on = 0; on < 3; ++on) {
                number(scale_favorite(scale)); number(scale_favorite_set(scale, on));
                number(scale_favorite(scale)); capture(favorites.factory[14], 32); ++compat_cases;
            }
    }
}
static void reply(uint32_t cmd, const uint8_t *args, uint32_t count)
{
    request(cmd, args, count); number(wire_n); capture(wire, wire_n); ++compat_cases;
}
static void protocol_cases(void)
{
    power_on(); memset(nor, 255, sizeof nor); persist_boot(); settings_init();
    settings_batt_full = 628; song.batt_raw = 615;
    const uint8_t shared_slot[] = {3,127,2,0,'C','O','M','P','A','T'};
    reply(ED_LIBRARY, shared_slot, sizeof shared_slot);
    assert(up_used(383));
    reply(ED_UI_STATE, 0, 0); reply(ED_UI_PALETTES, 0, 0);
    uint8_t args[6] = {0};
    for (uint32_t state = 0; state < 5; ++state) {
        flash_ok = state != 1; erase_error = state == 2;
        song.playing = state == 3; persist_old = state == 4;
        for (uint32_t id = 0; id < 13; ++id)
            for (uint32_t value = 0; value < 7; ++value) {
                args[0] = id; args[1] = value;
                reply(ED_UI_SET, args, 2); reply(ED_UI_SET, args, 2);
            }
        for (uint32_t n = 0; n < 6; ++n) reply(ED_UI_SET, args, n);
        for (uint32_t ns = 0; ns <= USER_NATIVE_J106 + 2; ++ns)
            for (uint32_t start = 0; start < 5; ++start) {
                int32_t index = start == 0 ? -1 : start == 1 ? 0 : start == 2 ? 127 : start == 3 ? 383 : 384;
                uint32_t encoded = index + 8192;
                args[0] = ns; args[1] = encoded & 127; args[2] = encoded >> 7;
                for (uint32_t count = 0; count <= 33; count += count == 0 ? 1 : 16) {
                    args[3] = count; reply(ED_FAV_GET, args, 4);
                }
                for (uint32_t on = 0; on < 3; ++on) { args[3] = on; reply(ED_FAV_SET, args, 4); }
            }
        for (uint32_t n = 0; n < 6; ++n) {
            reply(ED_FAV_GET, args, n); reply(ED_FAV_SET, args, n);
            args[0] = 1; args[1] = 116; args[2] = 4; reply(ED_BATTERY, args, n);
        }
    }
    erase_error = 0; song.playing = 0; persist_old = 0; flash_ok = 1;
    for (int32_t raw = -1; raw <= 1025; ++raw) {
        song.batt_raw = raw; reply(ED_BATTERY, 0, 0);
    }
}
int main(int argc, char **argv)
{
    if (argc > 1) { compat_file = fopen(argv[1], "wb"); assert(compat_file); }
    power_on(); settings_cases(); favourite_cases(); protocol_cases();
    if (compat_file) assert(!fclose(compat_file));
    printf("Preferences compatibility: %u cases, digest %08x\n", compat_cases, compat_hash);
    /* Captured from the pre-replacement implementation, 23 October 2026.
     * Keep this fixed: changing it needs an intentional compatibility decision. */
    assert(compat_cases == 12109u && compat_hash == 0x7079ed26u);
    return 0;
}
