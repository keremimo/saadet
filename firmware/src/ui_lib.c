/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* The library (the FX button), SAVE and an import. A list is the sounds of one source: FAVOURITES (the factory sounds
 * starred, the engines in their order, then the user sounds starred), USER (every user slot in use), or one engine's
 * (its factory sounds, then the user sounds on it). PRESETS browses the engine's list from any page, ALGORITHM the
 * next engine's. The sound chosen loads when its knob rests. SAVE stores the sound in a user slot under a name; a
 * dump that came in (sound.c sysex_stage) waits in SAVE and goes into the slots from the one chosen. */
static uint32_t ui_stop(uint32_t p);                      /* ui.c */
static void ui_sound_name(char *b);
static int up_load(uint32_t k);                           /* upreset.c */
static int up_store(uint32_t k, const char *name);
static int up_delete(uint32_t k);
static void up_name(uint32_t k, char *b);
static void up_slot_label(char *b, uint32_t k);
static void up_ui_named(uint32_t op, uint32_t k, const char *name);
static int up_old(uint32_t k);
static void sx_name(uint32_t i, char *b);                 /* shared_presets.c */
static int sx_commit(uint32_t first, uint32_t *saved);
static int library_sysex_send(uint32_t engine, uint32_t index, int user, int live);
static int perf_used(uint32_t k);                         /* perf.c */
static uint32_t perf_engine(uint32_t k);
static void perf_name(uint32_t k, char *b);
static void perf_label(char *b, uint32_t k);
static int perf_store(uint32_t k, const char *name);
static int perf_load(uint32_t k);
static int perf_delete(uint32_t k);
static uint8_t perf_now;

typedef struct { uint8_t user, eng; uint16_t idx; } lib_item_t;   /* user 0: a factory sound (eng, preset idx); 1: user slot
                                                                   * idx; 2: performance idx (eng: its part A's) */
#define LIB_ROWS 5u
static struct {
    uint16_t at[LIB_N];          /* each list's chosen sound */
    uint8_t pending;             /* the chosen sound loads when the knob rests */
    uint32_t t;
    lib_item_t want;
    uint16_t slot;               /* SAVE: the slot, the name and its cursor, the star for it; a performance's slot */
    uint8_t pslot;
    uint8_t cursor, star, edited;
    char name[13];
    uint16_t first;              /* IMPORT: the first slot */
    uint8_t deleting, erase;     /* explicit knob arming; delete is tied to a slot and bank generation */
    uint16_t delete_slot;
    uint32_t delete_t, delete_gen;
} lib;

static uint32_t lib_engine(uint32_t src) { return eng_vis(src - LIB_P5); }   /* LIB_P5..LIB_J106 -> P5, FM6, CZ-1, JUNO-106 */
/* one pass over list src: its count; the sounds from .. from + nw - 1 into w (w[i].eng 255: none) */
static uint32_t lib_scan(uint32_t src, int32_t from, lib_item_t *w, uint32_t nw)
{
    uint32_t c = 0, e, k;
    for (k = 0; k < nw; k++) w[k].eng = 255u;
#define TAKE(u, en, i) do { if ((int32_t)c >= from && (int32_t)c < from + (int32_t)nw) { \
        lib_item_t *x = &w[c - (uint32_t)from]; x->user = (u); x->eng = (uint8_t)(en); x->idx = (uint16_t)(i); } c++; } while (0)
    if (src == LIB_PERF) {                           /* the performances in use */
        for (k = 0; k < PERF_SLOTS; k++)
            if (perf_used(k))
                TAKE(2u, perf_engine(k), k);
        return c;
    }
    for (e = 0; e < NENG_SHOWN; e++) {               /* the factory sounds */
        uint32_t eng = eng_vis(e), n = ENGINES[eng]->npresets;
        if (src != LIB_FAV && (src == LIB_USER || lib_engine(src) != eng))
            continue;
        for (k = 0; k < n; k++)
            if (src != LIB_FAV || favorite_has(eng, k))
                TAKE(0u, eng, k);
    }
    for (k = 0; k < UP_SLOTS; k++) {                 /* the user sounds */
        if (!up_used(k))
            continue;
        if (src == LIB_FAV ? !up_favorite(k) : src != LIB_USER && up_engine(k) != lib_engine(src))
            continue;
        TAKE(1u, up_engine(k), k);
    }
#undef TAKE
    return c;
}
static uint32_t lib_count(uint32_t src) { lib_item_t w; return lib_scan(src, 0, &w, 0); }
static int lib_item(uint32_t src, uint32_t n, lib_item_t *it) { return n < lib_scan(src, (int32_t)n, it, 1) && it->eng != 255u; }
static void lib_item_name(const lib_item_t *it, char *b)   /* b holds 24 */
{
    if (it->user == 2u) perf_name(it->idx, b);
    else if (it->user) up_name(it->idx, b);
    else str_cpy(b, eng_preset(ENGINES[it->eng], it->idx)->name, 22);
}
static int lib_item_star(const lib_item_t *it)
{
    return it->user == 2u ? 0 : it->user ? up_favorite(it->idx) : favorite_has(it->eng, it->idx);
}
static int lib_item_now(const lib_item_t *it)      /* the sound playing now */
{
    uint32_t u = user_of(TSEL);
    if (it->user == 2u)
        return perf_now == it->idx + 1u;
    return it->user ? u == it->idx : u == USER_NONE && TSEL->eng_req == it->eng && TSEL->preset == it->idx;
}
/* the list's position of the sound playing now, or -1 */
static int32_t lib_find_now(uint32_t src)
{
    lib_item_t w[16];
    uint32_t n = lib_count(src), at, k;
    for (at = 0; at < n; at += 16u) {
        lib_scan(src, (int32_t)at, w, 16u);
        for (k = 0; k < 16u && at + k < n; k++)
            if (lib_item_now(&w[k])) return (int32_t)(at + k);
    }
    return -1;
}
static void lib_load(const lib_item_t *it)
{
    if (it->user == 2u) perf_load(it->idx);
    else if (it->user) up_load(it->idx);
    else if (it->eng != TSEL->eng_req) engine_load_of(TSEL, it->eng, it->idx);
    else apply_preset(it->idx);
}
static void lib_poll(uint32_t now, uint32_t rest)   /* the chosen sound at rest: it loads */
{
    if (lib.pending && now - lib.t >= rest) {
        lib.pending = 0;
        lib_load(&lib.want);
    }
}
/* src's position moved by s (round); the sound there loads at rest. 0: an empty list */
static int lib_move(uint32_t src, int32_t s)
{
    uint32_t n = lib_count(src);
    if (!n)
        return 0;
    lib.at[src] = (uint16_t)(((int32_t)(lib.at[src] % n) + s % (int32_t)n + (int32_t)n) % (int32_t)n);
    if (lib_item(src, lib.at[src], &lib.want)) {
        lib.pending = 1;
        lib.t = fm1_ms;
    }
    return 1;
}
static void lib_open(uint32_t src)                 /* a list opened: at the sound playing now, if it is in it */
{
    int32_t k = lib_find_now(src);
    if (k >= 0) lib.at[src] = (uint16_t)k;
}

static int lib_deletable(lib_item_t *it)
{
    uint32_t src = ui_stop(PL_LIB), n = lib_count(src);
    return n && lib_item(src, lib.at[src] % n, it) && it->user;
}
static void lib_delete_cancel(void) { lib.deleting = 0; }
static void lib_delete_poll(void)
{
    lib_item_t it;
    if (lib.deleting && (fm1_ms - lib.delete_t >= 3000u || up_gen != lib.delete_gen ||
        !lib_deletable(&it) || it.idx != lib.delete_slot))
        lib_delete_cancel();
}
static void lib_delete(void)
{
    char name[24], msg[28] = "DELETED ";
    uint32_t src = ui_stop(PL_LIB), n;
    lib_item_t it;
    int rc;
    lib_delete_poll();
    if (!lib.deleting) return;
    lib_delete_cancel();
    if (!lib_deletable(&it) || it.idx != lib.delete_slot) return;
    lib_item_name(&it, name);
    rc = it.user == 2u ? perf_delete(it.idx) : up_delete(it.idx);
    if (rc == 2 && it.user == 2u) { ui_message("STOP TO SAVE"); return; }
    if (rc && rc != 3) { ui_message("DELETE ERROR"); return; }
    if (it.user == 2u) perf_label(msg + 8, it.idx);
    else up_slot_label(msg + 8, it.idx);
    str_cpy(msg + str_len(msg), " ", 2);
    str_cpy(msg + str_len(msg), name, sizeof msg - str_len(msg));
    ui_message(rc == 3 ? "DELETED (RAM)" : msg);
    lib.pending = 0;                              /* the sound playing stays; the next row is not loaded */
    n = lib_count(src);
    if (!n) lib.at[src] = 0;
    else if (lib.at[src] >= n) lib.at[src] = (uint16_t)(n - 1u);
}

/* --------------------------------------------------------------- SAVE --- */
static const char LIB_CHARS[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-+.#&/";
static int save_perf(void) { return ui_stop(PL_SAVE) == 1u; }
static void save_open(void)                        /* the sound's own slot and name, or the first free slot */
{
    uint32_t u = user_of(TSEL), k;
    char b[24];
    if (save_perf()) {                             /* a performance: the one playing, or the first free slot */
        if (perf_now)
            k = perf_now - 1u;
        else
            for (k = 0; k < PERF_SLOTS - 1u && perf_used(k); k++)
                ;
        lib.pslot = (uint8_t)k;
        if (perf_used(k)) {
            perf_name(k, lib.name);
        } else {
            perf_label(b, k);
            str_cpy(lib.name, b, sizeof lib.name);
        }
        lib.cursor = 0;
        lib.edited = 0;
        return;
    }
    if (u != USER_NONE) {
        lib.slot = (uint16_t)u;
    } else {
        for (k = 0; k < UP_SLOTS && (up_used(k) || up_old(k)); k++)
            ;
        lib.slot = (uint16_t)(k < UP_SLOTS ? k : 0u);
    }
    ui_sound_name(b);
    for (k = 0; k < 12u && b[k]; k++)
        lib.name[k] = b[k] >= 'a' && b[k] <= 'z' ? (char)(b[k] - 32) : b[k];
    lib.name[k] = 0;
    lib.cursor = 0;
    lib.star = u != USER_NONE && up_favorite(u);
    lib.edited = 0;
}
static void save_name(char *b)                     /* the name as it will be stored: no trailing spaces */
{
    uint32_t n = str_len(lib.name);
    str_cpy(b, lib.name, 13);
    while (n && b[n - 1u] == ' ') b[--n] = 0;
}
static void save_store(void)                       /* SAVE pressed again: the sound into the slot */
{
    char b[13];
    uint32_t gen = up_gen;
    save_name(b);
    if (save_perf()) {
        char l[5];
        int rc = perf_store(lib.pslot, b);
        perf_label(l, lib.pslot);
        if (rc == 2) ui_message("STOP TO SAVE");
        else if (rc) ui_message("SAVE ERROR");
        else ui_say("SAVED ", l);
        ui.force = 1;
        return;
    }
    up_ui_named(2u, lib.slot, b);
    if (up_gen != gen && up_used(lib.slot)) {          /* (stored: not refused while playing, no write error) */
        up_favorite_set(lib.slot, lib.star);
        if (!TSEL->user || TSEL->user != lib.slot + 1u) {   /* the sound is that slot's from now on */
            TSEL->user = (uint16_t)(lib.slot + 1u);
            TSEL->user_native = 0;
        }
    }
}
static void import_open(void)                      /* the first run of free slots the dump fits, else slot 1 */
{
    uint32_t k, run = 0;
    for (k = 0; k < UP_SLOTS; k++) {
        run = up_used(k) || up_old(k) ? 0u : run + 1u;
        if (run >= sx_stage.count) { lib.first = (uint16_t)(k + 1u - run); return; }
    }
    lib.first = 0;
}
static uint32_t import_taken(void)                 /* the used slots the import would replace */
{
    uint32_t k, n = 0;
    for (k = lib.first; k < lib.first + sx_stage.count && k < UP_SLOTS; k++)
        n += (uint32_t)up_used(k);
    return n;
}
static void import_store(void)
{
    char l[5], m[28];
    uint32_t saved;
    int rc = sx_commit(lib.first, &saved);
    up_slot_label(l, lib.first);
    if (rc == 0 || rc == 3) {
        if (saved == 1u) {
            str_cpy(m, "IMPORTED TO ", sizeof m);
        } else {
            fmt_int(m, (int32_t)saved);
            str_cpy(m + str_len(m), " IMPORTED FROM ", sizeof m - str_len(m));
        }
        str_cpy(m + str_len(m), l, sizeof m - str_len(m));
        ui_message(rc == 3 ? "IMPORTED (RAM)" : m);
        if (saved == 1u) up_load(lib.first);       /* (one sound: it plays now) */
    } else {
        ui_message(rc == 2 ? "STOP TO SAVE" : "IMPORT ERROR");
    }
}

/* ------------------------------------------------------------- fields --- */
static const param_desc_t *lib_desc(uint32_t id)
{
    static param_desc_t d;
    static const char *const N_STAR[] = {"OFF", "ON"};
    lib_item_t it;
    d = (param_desc_t){"", F_INT, 0, 0, 0, 0, 0};
    switch (id) {
    case LF_SOUND: {                                   /* (its bar: where the sound is in the list) */
        uint32_t n = lib_count(ui_stop(PL_LIB));
        d.label = "SOUND"; d.max = (int16_t)(n > 1u ? n - 1u : 1u);
        break;
    }
    case LF_FAV: case LF_SFAV: d.label = "FAV"; d.fmt = F_ENUM; d.max = 1; d.names = N_STAR; break;
    case LF_SLOT: d.label = "SLOT"; d.max = UP_SLOTS - 1u; break;
    case LF_PSLOT: d.label = "SLOT"; d.max = PERF_SLOTS - 1u; break;
    case LF_CURSOR: d.label = "CURSOR"; d.max = 11; break;
    case LF_LETTER: d.label = "LETTER"; d.max = (int16_t)(sizeof LIB_CHARS - 2u); break;
    case LF_FIRST: d.label = "FIRST"; d.max = (int16_t)(UP_SLOTS - (sx_stage.count ? sx_stage.count : 1u)); break;
    case LF_SEND:
        if (!lib_item(ui_stop(PL_LIB), lib.at[ui_stop(PL_LIB)], &it)) return 0;
        d.label = "SEND"; break;
    case LF_DELETE:
        if (!lib_deletable(&it)) return 0;
        d.label = "DELETE"; d.max = 1; break;
    case LF_ERASE: d.label = "ERASE ALL"; d.max = 1; break;
    default: return 0;
    }
    return &d;
}
static uint32_t lib_src(void) { return ui_stop(PL_LIB); }
static int32_t lib_get(uint32_t id)
{
    lib_item_t it;
    uint32_t src = lib_src(), k;
    switch (id) {
    case LF_SOUND: return lib.at[src];
    case LF_FAV: return lib_item(src, lib.at[src], &it) && lib_item_star(&it);
    case LF_SLOT: return lib.slot;
    case LF_PSLOT: return lib.pslot;
    case LF_CURSOR: return lib.cursor;
    case LF_LETTER:
        for (k = 0; LIB_CHARS[k] && LIB_CHARS[k] != (lib.name[lib.cursor] ? lib.name[lib.cursor] : ' '); k++)
            ;
        return LIB_CHARS[k] ? (int32_t)k : 0;
    case LF_SFAV: return lib.star;
    case LF_FIRST: return lib.first;
    case LF_DELETE: return lib.deleting;
    case LF_ERASE: return lib.erase;
    }
    return 0;
}
static void lib_step(uint32_t id, int32_t s)
{
    lib_item_t it;
    uint32_t src = lib_src(), n;
    int32_t one = s > 0 ? 1 : -1;
    switch (id) {
    case LF_SOUND: lib_move(src, s); break;
    case LF_FAV:                                       /* (as the editor's favourites: saved with the settings) */
        if (lib_item(src, lib.at[src], &it) && it.user != 2u) {
            if (it.user) up_favorite_set(it.idx, s > 0);
            else if (favorite_set(it.eng, it.idx, s > 0)) settings_save();
            n = lib_count(src);                         /* (FAVOURITES: the list itself changed) */
            if (n && lib.at[src] >= n) lib.at[src] = (uint16_t)(n - 1u);
        }
        break;
    case LF_SLOT: lib.slot = (uint16_t)clamp((int32_t)lib.slot + s, 0, (int32_t)UP_SLOTS - 1); break;
    case LF_PSLOT:
        lib.pslot = (uint8_t)clamp((int32_t)lib.pslot + s, 0, (int32_t)PERF_SLOTS - 1);
        if (perf_used(lib.pslot)) {                    /* a slot in use: its name, to keep or change */
            perf_name(lib.pslot, lib.name);
            lib.cursor = 0;
        } else if (!lib.edited) {
            perf_label(lib.name, lib.pslot);
            lib.cursor = 0;
        }
        break;
    case LF_CURSOR:
        lib.cursor = (uint8_t)clamp((int32_t)lib.cursor + one, 0, 11);
        while (str_len(lib.name) < lib.cursor) str_cpy(lib.name + str_len(lib.name), " ", 2);   /* (a gap: spaces) */
        break;
    case LF_LETTER: {
        int32_t k = (lib_get(LF_LETTER) + one + (int32_t)sizeof LIB_CHARS - 1) % ((int32_t)sizeof LIB_CHARS - 1);
        if (str_len(lib.name) <= lib.cursor) { lib.name[lib.cursor] = ' '; lib.name[lib.cursor + 1u] = 0; }
        lib.name[lib.cursor] = LIB_CHARS[k];
        lib.edited = 1;
        break;
    }
    case LF_SFAV: lib.star = s > 0; break;
    case LF_FIRST: lib.first = (uint16_t)clamp((int32_t)lib.first + s, 0, lib_desc(LF_FIRST)->max); break;
    case LF_SEND:
        if (s <= 0 || !lib_item(src, lib.at[src], &it) || it.user == 2u) break;
        if (!usb.config) { ui_message("CONNECT USB TO SEND"); break; }
        ui_message(library_sysex_send(it.eng, it.idx, it.user, lib_item_now(&it)) ?
                   "SYSEX SEND ERROR" : "SOUND SENT");
        break;
    case LF_DELETE:
        lib.deleting = (uint8_t)(s > 0 && lib_deletable(&it));
        if (lib.deleting) {
            lib.delete_slot = it.idx;
            lib.delete_gen = up_gen;
            lib.delete_t = fm1_ms;
        }
        break;
    case LF_ERASE: lib.erase = s > 0; break;
    }
}
static void lib_text(uint32_t id, char *val, char *unit)
{
    unit[0] = 0;
    switch (id) {
    case LF_SOUND: {
        uint32_t n = lib_count(lib_src());
        fmt_int(val, n ? (int32_t)(lib.at[lib_src()] % n) + 1 : 0);
        str_cpy(unit, "/", 2);
        fmt_int(unit + 1, (int32_t)(n > 999u ? 999u : n));
        break;
    }
    case LF_FAV: case LF_SFAV: str_cpy(val, lib_get(id) ? "ON" : "OFF", 4); break;
    case LF_SLOT: up_slot_label(val, lib.slot); break;
    case LF_PSLOT: perf_label(val, lib.pslot); break;
    case LF_CURSOR: fmt_int(val, lib.cursor + 1); break;
    case LF_LETTER: {
        char c = LIB_CHARS[lib_get(LF_LETTER)];
        if (c == ' ') str_cpy(val, "SPACE", 6);
        else { val[0] = c; val[1] = 0; }
        break;
    }
    case LF_FIRST: up_slot_label(val, lib.first); break;
    case LF_SEND: str_cpy(val, "+", 2); break;
    case LF_DELETE: str_cpy(val, lib.deleting ? "ON" : "\x96", 4); break;
    case LF_ERASE: str_cpy(val, lib.erase ? "ON" : "\x96", 4); break;
    }
}
