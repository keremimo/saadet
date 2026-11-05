/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */

enum {
    ED_UI_PALETTE = 1u, ED_UI_FONT = 2u, ED_UI_MONITOR = 4u,
    ED_UI_FAVORITES = 8u, ED_UI_RECORDING = 16u, ED_UI_PERFORMANCE = 32u
};
static uint32_t ed_ui_caps(void)
{
    uint32_t caps = ED_UI_PALETTE | ED_UI_PERFORMANCE;
#ifdef MELODEE_MONITOR
    caps |= ED_UI_MONITOR;
#endif
#ifdef MELODEE_FAVORITES
    caps |= ED_UI_FAVORITES;
#endif
    return caps;
}
static void prefs_unsigned28(uint32_t value)
{
    for (uint32_t i = 0; i < 4u; ++i) ed_b(value >> (7u * i));
}
static uint32_t prefs_signature(void)
{
    uint32_t hash = 0;
#ifdef MELODEE_FAVORITES
    hash = 2166136261u;
    const uint8_t *bytes = (const uint8_t *)&favorites;
    for (uint32_t i = 0; i < sizeof favorites; ++i) hash = (hash ^ bytes[i]) * 16777619u;
    bytes = (const uint8_t *)&favorites_user_hi;
    for (uint32_t i = 0; i < sizeof favorites_user_hi; ++i) hash = (hash ^ bytes[i]) * 16777619u;
#endif
    return (hash ^ up_gen) * 16777619u;
}
static void prefs_state(void)
{
    ed_b(ed_ui_caps());
    ed_b(settings.palette);
    ed_b(127);
#ifdef MELODEE_MONITOR
    ed_b(settings.monitor);
#else
    ed_b(127);
#endif
#ifdef MELODEE_FAVORITES
    ed_b(favorites.filter);
#else
    ed_b(127);
#endif
    prefs_unsigned28(prefs_signature());
    prefs_unsigned28(up_gen);
    ed_b(settings_click); ed_b(settings_click_level); ed_b(settings_countin);
    ed_b(settings_preview); ed_b(settings_chord_add); ed_b(127); ed_b(scr_get());
}

/* settings_save owns transport deferral, old-record protection and retry timing. */
static uint32_t prefs_save_status(void)
{
    settings_save();
#if MELODEE_FLASH
    if (!flash_ok || persist_pending == 2u) return 3;
    if (persist_pending == 1u) return 4;
    persist_t expected = persist_saved;
    settings_export(&expected);
    if (expected.zoom != persist_saved.zoom ||
        palette_from_stored(expected.palette) != palette_from_stored(persist_saved.palette)) return 3;
#ifdef MELODEE_FAVORITES
    if (memcmp(&expected.favorites, &persist_saved.favorites, sizeof expected.favorites)) return 3;
#endif
    return 0;
#else
    return 3;
#endif
}
static uint32_t prefs_set(uint32_t id, uint32_t value, uint32_t count)
{
    if (count != 2u || id > 10u) return 1;
    uint32_t required = id < 4u ? 1u << id : id < 9u ? ED_UI_RECORDING : ED_UI_PERFORMANCE;
    if (!(ed_ui_caps() & required)) return 2;
    uint32_t limit = id == 0u ? NPALETTES : id == 2u || (id >= 4u && id <= 6u) ? 3u :
                     id == 10u ? 5u : 2u;
    if (value >= limit) return 1;
    switch (id) {
    case 0: settings.palette = value; palette_set(value); break;
#ifdef MELODEE_MONITOR
    case 2:
        fm1_irq_off();
        settings.monitor = value;
        monitor_mode = value;
        monitor_event.valid = 0;
        fm1_irq_on();
        break;
#endif
#ifdef MELODEE_FAVORITES
    case 3: favorites.filter = value; break;
#endif
    case 10: scr_put(value); break;
    default: return 2;
    }
    ui.force = 1;
    return prefs_save_status();
}

#ifdef MELODEE_FAVORITES
/* The protocol's user namespaces map onto native-engine slot limits. */
static uint32_t prefs_native_engine(uint32_t space)
{
    switch (space) {
    case USER_NATIVE_P5: return ENGI_PROPHET;
    case USER_NATIVE_FM: return ENGI_FM6;
    case USER_NATIVE_CZ: return ENGI_CZ;
    case USER_NATIVE_J106: return ENGI_J106;
    case USER_NATIVE_HYDRA: return ENGI_HYDRA;
    default: return NENGINES;
    }
}
static uint32_t prefs_capacity(uint32_t space)
{
    if (space > USER_NATIVE_HYDRA) return 0;
    if (space == USER_GENERAL) return UP_SLOTS;
    uint32_t engine = prefs_native_engine(space);
    if (engine != NENGINES) return native_limit(engine);
    return space < NENGINES ? ENGINES[space]->npresets : 0;
}
static void prefs_fav_get(const uint8_t *args, uint32_t count)
{
    if (count != 4u) { ed_b(1); return; }
    uint32_t space = args[0], size = args[3], cap = prefs_capacity(space);
    int32_t start = ed_rv(args + 1);
    if (start < 0 || size < 1u || size > 32u || (uint32_t)start >= cap || size > cap - (uint32_t)start) {
        ed_b(1); return;
    }
    ed_b(0); ed_b(space); ed_v(start); ed_b(size);
    for (uint32_t i = 0; i < size; ++i) ed_b(favorite_has(space, (uint32_t)start + i));
}
static void prefs_fav_set(const uint8_t *args, uint32_t count)
{
    if (count != 4u) { ed_b(1); return; }
    uint32_t space = args[0], on = args[3], cap = prefs_capacity(space);
    int32_t index = ed_rv(args + 1);
    uint32_t engine = prefs_native_engine(space);
    if (index < 0 || (uint32_t)index >= cap || on > 1u ||
        (on && ((space == USER_GENERAL && !up_used((uint32_t)index)) ||
                (engine != NENGINES && !native_used(engine, (uint32_t)index))))) {
        ed_b(1); return;
    }
    int before = favorite_has(space, (uint32_t)index);
    int changed = favorite_set(space, (uint32_t)index, on);
    int separate_store = space == ENGI_PROPHET || space == ENGI_J106 || space == ENGI_HYDRA ||
                         engine == ENGI_PROPHET || engine == ENGI_J106 || engine == ENGI_HYDRA;
    uint32_t status;
    if (separate_store && !changed && before != (int)on) {
        status = transport_busy() ? 4u : 3u;
        on = (uint32_t)before;
    } else {
        ui.force = 1;
        status = engine == ENGI_PROPHET || engine == ENGI_J106 || engine == ENGI_HYDRA ? 0u : prefs_save_status();
    }
    ed_b(status); ed_b(space); ed_v(index); ed_b(on);
}
#endif

static void prefs_battery(const uint8_t *args, uint32_t count)
{
    /* Capture once before a save can stall the main loop. */
#ifdef FM1_IRQ_TARGET
    int32_t raw = fm1_adc_read(FM1_ADC_BATT);
#else
    int32_t raw = song.batt_raw;
#endif
    uint32_t status = 0;
    if (count) {
        uint32_t full = count == 3u ? args[1] | (uint32_t)args[2] << 7 : 0;
        if (count != 3u || args[0] != 1u || full < 1u || full > 1023u) status = 1;
        else if (transport_busy() || template_old()) status = 2;
        else {
            settings_batt_full = (uint16_t)full;
            status = prefs_save_status();
#if MELODEE_FLASH
            if (!status && battery_full_of(&persist_saved) != settings_batt_full) status = 3;
#endif
        }
    }
    if (raw <= 0 || raw > 1023) {
        raw = 0;
        if (!status) status = 2;
    }
    ed_b(1); ed_b(status); ed_b((uint32_t)raw); ed_b((uint32_t)raw >> 7);
    ed_b(settings_batt_full); ed_b(settings_batt_full >> 7);
}
static int ed_ui_handle(uint32_t cmd, const uint8_t *args, uint32_t count)
{
    switch (cmd) {
    case ED_UI_STATE: prefs_state(); break;
    case ED_UI_PALETTES:
        ed_b(NPALETTES);
        for (uint32_t i = 0; i < NPALETTES; ++i) ed_str(UI_PALETTES[i].name, 12);
        break;
    case ED_UI_SET: {
        uint32_t id = count ? args[0] : 127u, value = count > 1u ? args[1] : 127u;
        ed_b(prefs_set(id, value, count)); ed_b(id); ed_b(value); prefs_state();
        break;
    }
    case ED_FAV_GET:
#ifdef MELODEE_FAVORITES
        prefs_fav_get(args, count);
#else
        ed_b(2);
#endif
        break;
    case ED_FAV_SET:
#ifdef MELODEE_FAVORITES
        prefs_fav_set(args, count);
#else
        ed_b(2);
#endif
        break;
    case ED_BATTERY: prefs_battery(args, count); break;
    default: return 0;
    }
    return 1;
}
