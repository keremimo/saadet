/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* The interface. Every button is one place (ui_pages.c) and the screen names it; the four knobs edit the four values at
 * the foot of the screen; pressing the place's button again or turning SELECT turns its page; holding the button lists
 * what is inside on the screen and on the keys (a key jumps there, the keys play no notes while it is held), but ARP
 * held is the sustain pedal (seq.c), which keeps its notes until ARP is pressed again, and opens its place on a tap only
 * when it keeps none. On HOME the four knobs are the sound's macros
 * and a turn shows its value over the whole screen; holding HOME lists the favourites on the keys. PRESETS browses the sounds and ALGORITHM the engines (the sound loads when they rest); FX opens the
 * library, SAVE stores the sound (pressed again) or a dump that came in. REC latches the notes held, OCT- / OCT+ shift
 * the octave (both: back to 0; both for 5 s: UPDATE MODE, main.c). Startup maintenance (ui_boot.c), the update
 * countdown below, UPDATE (ota_hw.c) and the crash report (main.c) are the system's own screens. */
static struct {
    uint8_t force;               /* the screen to be drawn again whole */
    uint8_t uboot;               /* main.c: seconds left before UPDATE MODE (OCT- + OCT+ held), 0 = none */
} ui;

static void ui_message(const char *s);
static void ui_say(const char *a, const char *b);

#include "screen.c"
#include "ui_boot.c"
#include "ui_field.c"
#include "ui_pages.c"
#include "ui_view.c"
#include "ui_pics.c"
#include "ui_lib.c"
#include "ui_chord.c"
#ifdef SAADET_SMK37
#define KB_LIST 16u                                /* a held button's list: the pads */
static uint8_t smk_place_before;                   /* the place a button's press left */
static uint8_t smk_knob_here;                      /* a bank was chosen on this page: until the page changes */
static uint8_t smk_pad_here;                       /* .. a pad bank, its number + 1 */
static int smk_knob_banked(void);                  /* ui_smk.c */
static const ui_field_t *smk_knob_field(uint32_t k);
static int smk_knob_taken(uint32_t k, int32_t s);
#else
#define KB_LIST 27u
#endif

static int persist_erase_all(void);                /* persist.c */

#define POP_MS 1200u             /* HOME: a macro's value over the screen, after the knob rests */
#define TOAST_MS 1500u           /* a message in the header */
#define LOAD_MS 300u             /* PRESETS / ALGORITHM at rest: the sound loads */
#define BROWSE_MS 1500u          /* .. and the list closes */
static struct {
    uint8_t place, stop[PL_N];   /* the place on the screen; each place's stop, kept */
    uint8_t held;                /* the place whose button is down (PL_N: none) */
    uint8_t held_new, jumped, reveal;   /* it opened with this press; a key jumped; the list is shown */
    uint32_t held_t;
    uint8_t hot, pop;            /* the column last turned (4: none); HOME: its value over the screen */
    uint32_t hot_t, knob_t[4];
    struct { uint8_t on, src; uint32_t t; } br;   /* browsing the sounds: the engine's list (ui_lib.c) */
    uint8_t eng;                 /* the engine the pages were of (another: EDIT, ENV, LFO from their first) */
    char toast[28];
    uint32_t toast_t;
    uint8_t erasing;             /* armed GLOBAL > ERASE: a fresh, uninterrupted GLO hold */
} uix = {.held = PL_N, .hot = 4u, .eng = 255u};

static void fmt3(char *b, int32_t v)             /* "041" */
{
    b[0] = (char)('0' + v / 100 % 10); b[1] = (char)('0' + v / 10 % 10); b[2] = (char)('0' + v % 10); b[3] = 0;
}
static void ui_message(const char *s)            /* a message in the header for a moment */
{
    str_cpy(uix.toast, s, sizeof uix.toast);
    uix.toast_t = fm1_ms;
}
static void ui_say(const char *a, const char *b)
{
    char m[28];
    str_cpy(m, a, sizeof m);
    str_cpy(m + str_len(m), b, sizeof m - str_len(m));
    ui_message(m);
}

/* ----------------------------------------------------------- places --- */
static uint32_t ui_stop(uint32_t p)
{
    ui_place_t pl = place_of(p);
    return pl.n ? uix.stop[p] % pl.n : 0u;
}
static void ui_open(uint32_t p)
{
#ifdef SAADET_SMK37
    smk_knob_here = smk_pad_here = 0;                  /* a page opened shows its own four, its pads their own */
#endif
    lib_delete_cancel();
    lib.erase = uix.erasing = 0;
    if (uix.place == PL_SAVE && p != PL_SAVE && sx_staged()) {   /* away from an import: it is dropped */
        sx_drop();
        ui_message("IMPORT CANCELLED");
    }
    if (p == PL_SAVE && uix.place != PL_SAVE) {
        if (sx_staged()) import_open(); else save_open();
    }
    uix.place = (uint8_t)p;
    uix.hot = 4u;
    uix.pop = 0;
    if (p == PL_LIB)
        lib_open(ui_stop(PL_LIB));
}
static void ui_turn_page(uint32_t p, int32_t d)  /* the place's next (d > 0) or previous stop, round */
{
#ifdef SAADET_SMK37
    smk_knob_here = smk_pad_here = 0;
#endif
    lib_delete_cancel();
    lib.erase = uix.erasing = 0;
    ui_place_t pl = place_of(p);
    if (pl.n)
        uix.stop[p] = (uint8_t)(((int32_t)ui_stop(p) + d % pl.n + pl.n) % pl.n);
    uix.hot = 4u;
    if (p == PL_LIB)
        lib_open(ui_stop(PL_LIB));
    if (p == PL_SAVE && !sx_staged())                  /* another thing to save: its slot and name */
        save_open();
}
/* the field knob k edits now: a HOME macro, the stop's, or none */
static const ui_field_t *ui_knob_field(uint32_t k)
{
    static const ui_field_t none = F0;
    ui_place_t pl = place_of(uix.place);
#ifdef SAADET_SMK37
    if (smk_knob_banked())
        return smk_knob_field(k);
#endif
    if (uix.place == PL_HOME)
        return &HOME_F[eng_tint(TSEL->eng_req)][k];
    return pl.n ? &pl.stop[ui_stop(uix.place)].f[k] : &none;
}

/* ----------------------------------------------------------- browse --- */
static void browse_load(void) { lib_poll(fm1_ms, 0u); }   /* (the chosen sound now) */
static void browse_turn(int engine, int32_t s)  /* PRESETS: the engine's list; ALGORITHM: the next engine's */
{
    lib_delete_cancel();
    lib.erase = uix.erasing = 0;
    if (!uix.br.on) {
        uix.br.on = 1;
        uix.br.src = (uint8_t)(LIB_P5 + eng_tint(eng_ok(TSEL->eng_req) ? TSEL->eng_req : ENGI_PROPHET));
        lib_open(uix.br.src);
    }
    if (engine) {
        uint32_t e = eng_step(lib_engine(uix.br.src), s);
        uix.br.src = (uint8_t)(LIB_P5 + eng_tint(e));
        lib.at[uix.br.src] = e == ENGI_PROPHET || e == ENGI_J106 ? 1u : 0u;   /* (set_engine_of: the first program or tone) */
        lib_move(uix.br.src, 0);
    } else {
        lib_move(uix.br.src, s);
    }
    uix.br.t = fm1_ms;
    uix.pop = 0;
}

/* ------------------------------------------------------------ input --- */
static uint32_t ui_btn_bit(uint32_t b) { return 1u << panel.btn[b]; }
static int ui_erase_page(void)
{
    return uix.place == PL_GLO && place_of(PL_GLO).stop[ui_stop(PL_GLO)].pic == PIC_ERASE;
}
static void ui_octave(uint32_t b)
{
    char m[16] = "OCTAVE ";
    uint32_t both = ui_btn_bit(B_OCTDN) | ui_btn_bit(B_OCTUP);
    if ((fm1_in.buttons & both) == both)
        song.octave = 0;
    else
        song.octave = (int8_t)clamp(song.octave + (b == B_OCTUP ? 1 : -1), -3, 3);
    if (song.octave > 0)
        m[7] = '+';
    fmt_int(m + str_len(m), song.octave);
    if (uix.place != PL_HOME || uix.pop || uix.br.on)
        ui_message(m);
}
static void ui_button_down(uint32_t b)
{
    uint32_t p = place_of_button(b), browsing = uix.br.on;
#ifdef SAADET_SMK37
    smk_place_before = uix.place;
#endif
#ifdef SAADET_FM1_BASE
    if(b==B_SAVE&&uix.place==PL_GLO&&uix.held==PL_N&&!uix.reveal){
        uint32_t picture=place_of(PL_GLO).stop[ui_stop(PL_GLO)].pic;
        if(picture==PIC_BTDEV){ui_message(sd_ble_device_action());return;}
        if(picture==PIC_BLE&&sd_ble_forget_state()==1){sd_ble_forget_confirm();return;}
    }
#endif
    lib_delete_poll();
    if (lib.deleting) {
        if (b == B_FX && uix.place == PL_LIB && fm1_in.buttons == ui_btn_bit(B_FX)) {
            lib_delete();
            return;                              /* confirmation is consumed, including its release */
        }
        lib_delete_cancel();
    }
    if (lib.erase) {
        if (b == B_GLO && ui_erase_page() && fm1_in.buttons == ui_btn_bit(B_GLO)) {
            uix.held = PL_GLO;
            uix.held_t = fm1_ms;
            uix.jumped = uix.erasing = 1;
            uix.reveal = 0;
            return;
        }
        lib.erase = uix.erasing = 0;
    }
    if (uix.br.on) {                                   /* a button closes the browsing (its sound loads now) */
        browse_load();
        uix.br.on = 0;
    }
    if (b == B_OCTDN || b == B_OCTUP) {
        ui_octave(b);
        return;
    }
    if (b == B_REC) {                                  /* latch the notes held, or let them go: on all the keys play */
        int16_t on = (int16_t)!TSEL->p[P_AHOLD];
        for (uint32_t i = 0; i < NPART; i++)
            if ((keyboard_parts() >> i) & 1u)
                trk[i].p[P_AHOLD] = on;
        return;
    }
    if (p >= PL_N)
        return;
    uix.held_new = uix.place != p || browsing;
    uix.held = (uint8_t)p;
    uix.held_t = fm1_ms;
    uix.jumped = uix.reveal = 0;
    if (p == PL_ARP) {                                 /* held, ARP is the sustain pedal (seq.c): it opens on a tap. */
        uix.jumped = fm1_in.notes != 0u;               /* Keys already down: the pedal */
        if (sustain_on()) {                            /* Notes kept: this press ends them, no tap */
            kb_sustain_end = 1;
            uix.jumped = 1;
        }
        return;
    }
    if (uix.held_new)
        ui_open(p);
    uix.pop = 0;
}
static void ui_button_up(uint32_t b)
{
    uint32_t p = place_of_button(b);
    if (p >= PL_N || p != uix.held)
        return;
    if (uix.erasing) {
        lib.erase = uix.erasing = 0;
        uix.jumped = 1;
    }
    if (p == PL_ARP) {                                 /* a tap: ARP opens, or turns its page (else: the pedal) */
        if (!uix.jumped && fm1_ms - uix.held_t < HOLD_MS[settings_hold % 4u]) {
            if (uix.held_new) ui_open(p);
            else ui_turn_page(p, 1);
        }
    } else if (!uix.held_new && !uix.jumped && !uix.reveal) {  /* a tap on the open place: its next page; SAVE: */
        if (p == PL_SAVE) {                                     /* it stores */
            if (sx_staged()) import_store(); else save_store();
        } else if (p == PL_HOME && trk_n() > 1u) {              /* HOME: the other part */
            track_select(song.sel ^ 1u);
            ui_say("PART ", N_PART[song.sel]);
        } else {
            ui_turn_page(p, 1);
        }
    }
    uix.held = PL_N;
    uix.reveal = 0;
}
/* the held button's entries: the place's objects or pages; HOME's: the favourites (n of them, the one playing: *cur) */
static uint32_t held_entries(uint32_t p, uint8_t *s, uint32_t *cur)
{
    ui_place_t pl = place_of(p);
    uint32_t n, k;
    *cur = 255u;
    if (p == PL_HOME) {
        int32_t now = lib_find_now(LIB_FAV);
        n = lib_count(LIB_FAV);
        *cur = now >= 0 && now < (int32_t)KB_LIST ? (uint32_t)now : 255u;
        return n < KB_LIST ? n : KB_LIST;
    }
    n = place_entries(&pl, s, KB_LIST);
    for (k = 0; k < n; k++)
        if (s[k] <= ui_stop(p)) *cur = k;
    return n;
}
static void ui_keys(uint32_t notes)                    /* a key while a place's button is held: jump there */
{
    uint8_t s[KB_LIST];
    uint32_t k, n, cur;
#ifdef SAADET_SMK37
    return;                                            /* the keys only play here: the pads choose (ui_smk.c) */
#endif
    if (uix.held >= PL_N || !notes || uix.erasing)
        return;
    if (uix.held == PL_ARP) {                          /* the keys play under the pedal: no tap */
        uix.jumped = 1;
        return;
    }
    n = held_entries(uix.held, s, &cur);
    for (k = 0; k < 27u && !((notes >> k) & 1u); k++)
        ;
    if (uix.held == PL_HOME) {                         /* HOME: the favourite of the key plays */
        lib_item_t it;
        if (k < n && lib_item(LIB_FAV, k, &it)) {
            lib_load(&it);
            uix.jumped = 1;
        }
        return;
    }
    if (k < n) {
        uix.stop[uix.held] = s[k];
        ui_open(uix.held);
        uix.jumped = 1;
    }
}
static void ui_knob(uint32_t k, int32_t s)
{
#ifdef SAADET_SMK37
    if (smk_knob_taken(k, s))
        return;
#endif
    const ui_field_t *f = ui_knob_field(k);
    const param_desc_t *d = field_desc(f);
    uint32_t dt = fm1_ms - uix.knob_t[k];
    int32_t range;
    if (uix.place == PL_LIB && k != 3u) lib_delete_cancel();
    if (uix.erasing) {                           /* changing a knob cancels an in-progress hold */
        lib.erase = uix.erasing = 0;
        uix.jumped = 1;
    }
    if (!d)
        return;
    range = d->max - d->min;
    uix.knob_t[k] = fm1_ms;
    if (dt < 60u && range >= 40)                       /* turned fast: bigger steps */
        s *= range >= 120 ? 4 : 2;
    field_step(f, s);
    if (f->kind == FK_LIB && f->id == LF_ERASE && lib.erase && uix.held == PL_GLO) {
        lib.erase = 0;                          /* arming never counts a button that was already held */
        uix.jumped = 1;
    }
    uix.hot = (uint8_t)k;
    uix.hot_t = fm1_ms;
#ifdef SAADET_SMK37
    uix.pop = uix.place == PL_HOME && !smk_knob_banked();
#else
    uix.pop = uix.place == PL_HOME;
#endif
}
static void ui_timers(void)
{
    uint32_t now = fm1_ms;
#ifdef SAADET_FM1_BASE
    uint32_t picture=uix.place==PL_GLO?place_of(PL_GLO).stop[ui_stop(PL_GLO)].pic:PIC_NONE;
    const char *event=sd_ble_page(picture==PIC_BTDEV&&!uix.reveal,picture==PIC_BLE&&!uix.reveal);
    if(event[0])ui_message(event);
#endif
    lib_delete_poll();
    if (uix.erasing && (!lib.erase || !ui_erase_page() || fm1_in.buttons != ui_btn_bit(B_GLO))) {
        lib.erase = uix.erasing = 0;
        uix.jumped = 1;
    }
    if (uix.erasing && now - uix.held_t >= 3000u) {
        lib.erase = uix.erasing = 0;
        uix.held = PL_N;
        uix.jumped = 1;
        if (persist_erase_all()) ui_message("ERASE ERROR");
        return;
    }
    if (uix.pop && now - uix.hot_t >= POP_MS)
        uix.pop = 0;
    lib_poll(now, LOAD_MS);
    if (uix.br.on && !lib.pending && now - uix.br.t >= BROWSE_MS)
        uix.br.on = 0;
    if (uix.held < PL_N && uix.held != PL_ARP && !uix.erasing && !uix.jumped && !uix.reveal &&
        now - uix.held_t >= HOLD_MS[settings_hold % 4u]) {
        ui_place_t pl = place_of(uix.held);
        uix.reveal = uix.held == PL_HOME ? lib_count(LIB_FAV) > 0u : pl.n > 1u;
    }
    if (sx_stage.arrived) {                            /* a dump came in: SAVE shows it */
        sx_stage.arrived = 0;
        uix.br.on = 0;
        if (uix.place == PL_SAVE) import_open();
        else ui_open(PL_SAVE);
    }
    if (eng_tint(TSEL->eng_req) != uix.eng) {          /* another engine: its own pages from the first */
        uix.eng = (uint8_t)eng_tint(TSEL->eng_req);
        uix.stop[PL_EDIT] = uix.stop[PL_ENV] = uix.stop[PL_LFO] = 0;
    }
    if (uix.toast[0] && now - uix.toast_t >= TOAST_MS)
        uix.toast[0] = 0;
}

#ifdef SAADET_SMK37
#include "ui_smk.c"
#endif
static void ui_input(void)
{
    uint32_t released, pressed = fm1_input_edges(&released), notes = fm1_input_note_edges(), k;
    int32_t s;
    if (boot_work.state) {                       /* maintenance: its own OCT- / OCT+ answers only */
        for (k = 0; k < NE; k++) (void)panel_enc(k);
        panel_moved = 0;
        boot_work_input(pressed);
        return;
    }
    kb_mask = 0;                                 /* a place's button held: the keys choose, they play no notes; */
#ifndef SAADET_SMK37                             /* (here the pads choose and the keys play on) */
    for (k = 0; k < PL_N; k++)                   /* ARP's is the sustain pedal, they play */
        if (k != PL_ARP)
            kb_mask |= ui_btn_bit(button_of_place(k));
#endif
    kb_pedal = ui_btn_bit(B_ARP);
    if (scr_input(pressed, notes)) return;
    for (k = 0; k < NB; k++) {
        if (pressed & ui_btn_bit(k)) ui_button_down(k);
        if (released & ui_btn_bit(k)) ui_button_up(k);
    }
    ui_keys(notes);
#ifdef SAADET_SMK37
    smk_input();
#endif
    if ((s = panel_enc(EN_SELECT)) && uix.place != PL_HOME)
        ui_turn_page(uix.place, s);
    if ((s = panel_enc(EN_PRESET)))
        browse_turn(0, s);
    if ((s = panel_enc(EN_ALGO)))
        browse_turn(1, s);
    for (k = 0; k < 4u; k++)
        if ((s = panel_enc(EN_K1 + k)))
            ui_knob(k, s);
    ui_timers();
    autosave_poll(pressed || notes || fm1_in.buttons || fm1_in.notes);
}

/* ------------------------------------------------------------- view --- */
static void ui_sound_name(char *b)               /* the sound's name: its user slot's, or its factory preset's */
{
    uint32_t u = user_of(TSEL);
    const engine_t *e = ENGINES[eng_idx(TSEL->eng_req)];
    if (u != USER_NONE)
        up_name(u, b);
    else if (e->npresets && TSEL->preset < e->npresets)
        str_cpy(b, eng_preset(e, TSEL->preset)->name, 22);
    else
        str_cpy(b, "INIT", 22);
}
static void vm_col_of(vm_col_t *c, const ui_field_t *f)
{
    const param_desc_t *d = field_desc(f);
    int32_t v, r;
    if (!d)
        return;
    c->on = 1;
    c->dim = (uint8_t)!field_active(f);
    str_cpy(c->label, field_label(f, d), sizeof c->label);
    field_text(f, d, c->val, c->unit);
    v = field_get(f);
    r = d->max - d->min;
    if (r <= 0) {
        c->bar = BAR_NONE;
    } else if (r <= 3) {
        c->bar = BAR_STEPS; c->a = (int16_t)(v - d->min); c->b = (int16_t)(r + 1);
    } else if (d->fmt == F_ENUM) {
        c->bar = BAR_TICK; c->a = (int16_t)((v - d->min) * 44 / r);
    } else if (d->min < 0 && d->max > 0) {
        int32_t m = d->max > -d->min ? d->max : -d->min, x = 25 + v * 25 / m;
        c->bar = BAR_BIPOLAR; c->a = (int16_t)(x < 25 ? x : 25); c->b = (int16_t)(x < 25 ? 25 : x);
    } else {
        c->bar = BAR_FILL; c->b = (int16_t)((v - d->min) * 50 / r);
    }
}
static void vm_state(char *b, uint32_t n)        /* what changes the notes now: arp, chords, latch, pedal (the key: the
                                                  * chord line) */
{
    const track_t *t = TSEL;
    b[0] = 0;
#define ADD(s) do { if (b[0]) str_cpy(b + str_len(b), " \xb7 ", n - str_len(b)); str_cpy(b + str_len(b), (s), n - str_len(b)); } while (0)
    if (t->p[P_AMODE]) {
        char a[24] = "ARP ";
        str_cpy(a + 4, N_AMODE[clamp(t->p[P_AMODE], 0, (int32_t)NELEM(N_AMODE) - 1)], 8);
        str_cpy(a + str_len(a), " ", 2);
        str_cpy(a + str_len(a), N_DIV[clamp(t->p[P_ARATE], 0, (int32_t)NELEM(N_DIV) - 1)], 8);
        ADD(a);
    }
    if (t->p[P_CHRD]) {
        char a[16] = "CHORD ";
        str_cpy(a + 6, N_CHRD[clamp(t->p[P_CHRD], 0, (int32_t)NELEM(N_CHRD) - 1)], 8);
        ADD(a);
    }
    if (t->p[P_AHOLD])
        ADD("LATCH");
#ifdef SAADET_SMK37
    if ((fm1_in.buttons & kb_pedal) || sd_jack_pedal || sustain_on())
#else
    if ((fm1_in.buttons & kb_pedal) || sustain_on())
#endif
        ADD("SUSTAIN");
#undef ADD
}
/* the list's five sounds around the one chosen (an empty list: its word in name); the count */
static uint32_t vm_list_of(ui_vm_t *v, uint32_t src)
{
    static const char *const EMPTY[LIB_N] = {"NO FAVOURITES YET", "NO USER SOUNDS YET", "", "", "", "", "NO SETUPS YET"};
    lib_item_t w[5];
    uint32_t n = lib_count(src), i, at = n ? lib.at[src] % n : 0u;
    lib_scan(src, (int32_t)at - 2, w, 5u);
    str_cpy(v->name, EMPTY[src % LIB_N], sizeof v->name);
    for (i = 0; i < 5u; i++) {
        if (w[i].eng == 255u) continue;
        lib_item_name(&w[i], v->brow[i]);
        if (!v->brow[i][0]) str_cpy(v->brow[i], "-", 2);
        if (w[i].user == 2u) perf_label(v->btag[i], w[i].idx);
        else if (w[i].user) up_slot_label(v->btag[i], w[i].idx);
        else fmt3(v->btag[i], w[i].idx);
        if (lib_item_star(&w[i])) v->bstar |= (uint8_t)(1u << i);
        if (lib_item_now(&w[i])) v->bnow |= (uint8_t)(1u << i);
    }
    return n;
}
/* SAVE: the name with its cursor, what the store does; IMPORT: the dump, its first names, its slots */
static void vm_save(ui_vm_t *v)
{
    char l[5], b[13];
    uint32_t i;
    if (sx_staged()) {
        uint32_t taken = import_taken();
        v->mode = VM_IMPORT;
        str_cpy(v->sub, ENG_TAG[eng_tint(sx_stage.e)], sizeof v->sub);
        fmt_int(v->name, sx_stage.count);
        str_cpy(v->name + str_len(v->name), sx_stage.count == 1u ? " SOUND" : " SOUNDS", 9);
        for (i = 0; i < 3u && i < sx_stage.count; i++) {
            sx_name(i, v->row[i]);
            if (i == 2u && sx_stage.count > 3u) str_cpy(v->row[i] + str_len(v->row[i]), " \x85", 3);
        }
        up_slot_label(v->line[0], lib.first);
        if (sx_stage.count > 1u) {
            up_slot_label(l, lib.first + sx_stage.count - 1u);
            str_cpy(v->line[0] + str_len(v->line[0]), " \x96 ", 4);
            str_cpy(v->line[0] + str_len(v->line[0]), l, 5);
        }
        if (taken) {
            str_cpy(v->line[0] + str_len(v->line[0]), " \xb7 REPLACES ", sizeof v->line[0] - str_len(v->line[0]));
            fmt_int(v->line[0] + str_len(v->line[0]), (int32_t)taken);
        }
        str_cpy(v->line[2], "PRESS SAVE TO IMPORT", sizeof v->line[2]);
        return;
    }
    v->mode = VM_SAVE;
    str_cpy(v->name, lib.name, sizeof v->name);
    v->cursor = lib.cursor;
    if (save_perf()) {                                 /* a performance */
        char n[PERF_NAME];
        perf_label(v->sub, lib.pslot);
        if (perf_used(lib.pslot)) {
            perf_name(lib.pslot, n);
            str_cpy(v->line[0], "REPLACES", sizeof v->line[0]);
            str_cpy(v->line[1], n, sizeof v->line[1]);
        } else {
            str_cpy(v->line[0], "EMPTY SLOT", sizeof v->line[0]);
        }
        str_cpy(v->line[2], "PRESS SAVE: BOTH PARTS", sizeof v->line[2]);
        return;
    }
    up_slot_label(v->sub, lib.slot);
    if (up_used(lib.slot)) {
        up_name(lib.slot, b);
        str_cpy(v->line[0], "REPLACES", sizeof v->line[0]);
        str_cpy(v->line[1], b, sizeof v->line[1]);
    } else {
        str_cpy(v->line[0], up_old(lib.slot) ? "OLD FORMAT SLOT" : "EMPTY SLOT", sizeof v->line[0]);
    }
    str_cpy(v->line[2], "PRESS SAVE TO STORE", sizeof v->line[2]);
}
/* HOME's key and chord: the selected part's notes as played (one ISR snapshot), the chord they make (ui_chord.c) */
static void vm_keys_now(ui_vm_t *v)
{
    uint32_t bits[4], held = 0, i, n = 0, pcs = 0, root = 0, k = song.sel % NTRK, micro = micro_active(TSEL) != 0;
    uint8_t note[8];
    const char *q = 0;
    fm1_irq_off();
    for (i = 0; i < 4u; i++) {
        bits[i] = live_last[k][i];
        held |= live_held[k][i];
    }
    fm1_irq_on();
    for (i = 0; i < 128u; i++)
        if ((bits[i >> 5] >> (i & 31u)) & 1u) {
            pcs |= 1u << (i % 12u);
            if (n < sizeof note) note[n] = (uint8_t)i;
            if (i >= KB_LO && i < KB_LO + KB_SPAN) v->kon[(i - KB_LO) >> 5] |= 1u << ((i - KB_LO) & 31u);
            n++;
        }
    v->kscale = micro ? 0u : (uint16_t)scale_mask(TSEL);
    v->kkey = (uint8_t)(TSEL->p[P_ROOT] % 12);
    v->kfade = (uint8_t)(n && !held);
    if (!n) {
        v->kmode = KM_KEY;
        key_name(v->kline, sizeof v->kline);
        return;
    }
    if (n == 1u) {
        v->kmode = KM_NOTE;
        note_name(v->kline, note[0]);
        return;
    }
    if (!micro)
        q = chord_of(pcs, note[0] % 12u, &root);
    if (!q) {
        v->kmode = KM_NOTES;
        notes_fit(v->kline, note, n, TY_RW, 220);
        return;
    }
    v->kmode = KM_CHORD;
    str_cpy(v->kroot, N_NOTE[root], sizeof v->kroot);
    str_cpy(v->kq, q, sizeof v->kq);
    if (root != note[0] % 12u)
        str_cpy(v->kbass, N_NOTE[note[0] % 12u], sizeof v->kbass);
    notes_fit(v->knotes, note, n, TY_LB, 220);
}
static void vm_build(ui_vm_t *v)
{
    uint32_t i, e = eng_tint(TSEL->eng_req);
    ui_place_t pl = place_of(uix.place);
    memset(v, 0, sizeof *v);
    v->eng = (uint8_t)e;
    v->hot = uix.hot;
    str_cpy(v->toast, uix.toast, sizeof v->toast);
    if (uix.erasing) {
        uint32_t dt = fm1_ms - uix.held_t;
        v->mode = VM_COUNTDOWN;
        str_cpy(v->warn, "ERASING EVERYTHING IN", sizeof v->warn);
        fmt_int(v->name, (int32_t)(3u - (dt < 3000u ? dt / 1000u : 2u)));
        v->sweep = (uint16_t)(dt < 3000u ? dt * 360u / 3000u : 360u);
        return;
    }
#ifdef SAADET_SMK37
    smk_tag(v);
    if (smk_pick_view(v))
        return;
#endif
    if (trk_n() > 1u && !v->tag[0]) {                  /* which part the pages and the knobs are on */
        str_cpy(v->tag, "PART ", sizeof v->tag);
        str_cpy(v->tag + 5, N_PART[song.sel], sizeof v->tag - 5u);
        if (uix.place == PL_HOME && song.octave) {     /* (it stands where the octave does: both) */
            str_cpy(v->tag, N_PART[song.sel], sizeof v->tag);
            str_cpy(v->tag + 1, song.octave > 0 ? " OCT+" : " OCT", sizeof v->tag - 1u);
            fmt_int(v->tag + str_len(v->tag), song.octave);
        }
    }
    if (uix.br.on) {                                   /* browsing: the sounds around the one chosen */
        uint32_t n = vm_list_of(v, uix.br.src);
        v->mode = VM_BROWSE;
        v->bsrc = (uint8_t)(uix.br.src - LIB_P5);
        str_cpy(v->head, ENG_TAG[v->bsrc], sizeof v->head);
        fmt_int(v->bcount, n ? (int32_t)(lib.at[uix.br.src] % n) + 1 : 0);
        str_cpy(v->bcount + str_len(v->bcount), " / ", 4);
        fmt_int(v->bcount + str_len(v->bcount), (int32_t)n);
        return;
    }
    if (uix.held < PL_N && uix.reveal) {               /* the held button's list, on the keys too */
        ui_place_t hp = place_of(uix.held);
        uint8_t s[KB_LIST];
        uint32_t cur, n = held_entries(uix.held, s, &cur), c = cur < n ? cur : 0u;
        v->mode = VM_REVEAL;
        str_cpy(v->head, uix.held == PL_HOME ? "FAVOURITES" : hp.name, sizeof v->head);
        if (uix.held == PL_EDIT || uix.held == PL_ENV || uix.held == PL_LFO) str_cpy(v->sub, ENG_TAG[e], sizeof v->sub);
        v->cur = (uint8_t)cur;
        v->first = (uint8_t)(c > 3u && n > VM_ROWS ? (c - 3u < n - VM_ROWS ? c - 3u : n - VM_ROWS) : 0u);
        v->nrows = (uint8_t)(n - v->first < VM_ROWS ? n - v->first : VM_ROWS);
        for (i = 0; i < v->nrows; i++) {
            if (uix.held == PL_HOME) {
                lib_item_t it;
                if (lib_item(LIB_FAV, v->first + i, &it)) lib_item_name(&it, v->row[i]);
            } else {
                str_cpy(v->row[i], entry_name(&hp, s[v->first + i]), sizeof v->row[i]);
            }
        }
        v->keys = n >= 32u ? 0xFFFFFFFFu : (1u << n) - 1u;
        return;
    }
    if (uix.place == PL_HOME) {
        if (uix.pop && uix.hot < 4u) {                 /* a macro turned: its name and value over the screen */
            const ui_field_t *f = &HOME_F[e][uix.hot];
            const param_desc_t *d = field_desc(f);
            v->mode = VM_POP;
            if (d) {
                vm_col_of(&v->col[0], f);
                if (v->col[0].unit[0]) {
                    str_cpy(v->col[0].val + str_len(v->col[0].val), " ", 2);
                    str_cpy(v->col[0].val + str_len(v->col[0].val), v->col[0].unit, 6);
                }
                str_cpy(v->sub, field_label(f, d), sizeof v->sub);
                v->sweep = (uint16_t)(d->max > d->min ? (field_get(f) - d->min) * 270 / (d->max - d->min) : 0);
            }
            ui_sound_name(v->name);
            return;
        }
        v->mode = VM_HOME;
        str_cpy(v->head, ENG_TAG[e], sizeof v->head);
        if (user_of(TSEL) != USER_NONE) {
            str_cpy(v->sub, "USER ", sizeof v->sub);
            up_slot_label(v->sub + 5, user_of(TSEL));
        } else {
            str_cpy(v->sub, "FACTORY ", sizeof v->sub);
            fmt3(v->sub + 8, TSEL->preset);
        }
        if (song.octave) {
            str_cpy(v->oct, song.octave > 0 ? "OCT +" : "OCT ", sizeof v->oct);
            fmt_int(v->oct + str_len(v->oct), song.octave);
        }
        ui_sound_name(v->name);
        vm_state(v->state, sizeof v->state);
        vm_keys_now(v);
        for (i = 0; i < 4u; i++) {
            const ui_field_t *f = &HOME_F[e][i];
            const param_desc_t *d = field_desc(f);
            if (d && d->max > d->min)
                v->macro[i] = (uint8_t)((field_get(f) - d->min) * 50 / (d->max - d->min));
        }
#ifdef SAADET_SMK37
        smk_strip_view(v, 1);
#endif
        return;
    }
    str_cpy(v->head, pl.name, sizeof v->head);
    v->mode = VM_PAGE;
    if (pl.n) {
        uint32_t s = ui_stop(uix.place);
        if (uix.place != PL_SAVE) str_cpy(v->sub, stop_title(&pl, s), sizeof v->sub);
        stop_dots(&pl, s, &v->ndots, &v->dot);
        for (i = 0; i < 4u; i++) {
            vm_col_of(&v->col[i], &pl.stop[s].f[i]);
            if (uix.place == PL_GLO && trk_n() > 1u && pl.stop[s].f[i].kind == FK_TRACK && v->col[i].on) {
                char l[sizeof v->col[i].label];        /* (no room for the part in GLOBAL's header: the values say it) */
                str_cpy(l, N_PART[song.sel], sizeof l);
                str_cpy(l + 1, " ", sizeof l - 1u);
                str_cpy(l + 2, v->col[i].label, sizeof l - 2u);
                str_cpy(v->col[i].label, l, sizeof l);
            }
        }
        if (uix.place == PL_SAVE) vm_save(v);
        else if (pl.stop[s].pic == PIC_LIB) {
            v->pic = PIC_LIB;
            vm_list_of(v, ui_stop(PL_LIB));
            if (lib.deleting) {
                str_cpy(v->warn, "DELETE ", sizeof v->warn);
                if (ui_stop(PL_LIB) == LIB_PERF) perf_label(v->warn + 7, lib.delete_slot);
                else up_slot_label(v->warn + 7, lib.delete_slot);
                str_cpy(v->warn + str_len(v->warn), "? PRESS FX", sizeof v->warn - str_len(v->warn));
            }
        } else if (pl.stop[s].pic == PIC_ERASE) {
            v->mode = VM_ERASE;
            if (lib.erase) str_cpy(v->warn, "HOLD GLO 3 S TO ERASE ALL", sizeof v->warn);
        }
        else pic_build(v, &pl.stop[s]);
#ifdef SAADET_SMK37
        smk_strip_view(v, 0);
#endif
    }
}

/* ------------------------------------------------------------- LEDs --- */
/* The lights' grammar (Melodee's), keys and buttons alike: dark nothing there, dim something there, bright happening
 * now, breathing waiting. Every button glows when idle (LIGHTS not OFF); the place on the screen is bright, the ARP
 * flashes on the beat, REC is bright while it latches, an octave shift lights its button, SAVE breathes while a dump
 * waits. A key is bright while it sounds, dim when it plays something (QNT OFF: the notes of the scale; a layout: every
 * key not silent), else dark; a held button's entries are dim on the keys, the current one bright. The picture is
 * built off-line and copied one byte a column: clearing and relighting would let the 10 kHz scan catch the dark gap
 * and flicker. ui_led_lv: each LED's level as drawn (0 dark, 1 dim, 2 breathing, 3 bright; ids: buttons 0..13,
 * keys 14..40) */
#define UI_LEDS 41u
static uint8_t ui_led_lv[UI_LEDS];
static uint8_t led_pos[UI_LEDS];                         /* (col << 3) | row bit, 0xFF = none */
static void led_pos_init(void)
{
    uint32_t id, p, r;
    for (id = 0; id < UI_LEDS; id++) {
        led_pos[id] = 0xFF;
        for (p = 0; p < FM1_NCOL; p++)
            for (r = 1; r < 5u; r++)
                if (FM1_KEYMAP[r][p] == (int8_t)id)
                    led_pos[id] = (uint8_t)((p << 3) | r);
    }
}
static void led_put(uint8_t *nl, uint32_t id, int on)
{
    uint8_t q = led_pos[id];
    if (q != 0xFF && on)
        nl[q >> 3] |= (uint8_t)(1u << (q & 7u));
}
static int led_get(const uint8_t *nl, uint32_t id)
{
    uint8_t q = led_pos[id];
    return q != 0xFF && ((nl[q >> 3] >> (q & 7u)) & 1u);
}
/* an ARP playing on any part flashes the ARP button on the beat, the bar's first beat longer: 1 lit, 0 dark,
 * 2 no ARP playing */
static uint32_t arp_led(void)
{
    uint32_t k, on = 0, b = seq_beat_samples();
    for (k = 0; k < NPART; k++)
        on |= trk[k].p[P_AMODE] && trk[k].nheld;
    return !on ? 2u : beat_pos < (beat_n ? b / 6u : b / 2u);
}
/* key k of track t as the keys play now (LIGHTS): bright while it sounds (pressed, or its note held on the track by
 * a key or MIDI), dim when it plays something (a kit's own map: every key; QNT OFF: the notes in the scale; the
 * layouts: the keys not silent), else dark */
enum { KL_OFF, KL_DIM, KL_ON };
static uint32_t play_key_led(const track_t *t, uint32_t k)
{
    const engine_t *e = ENGINES[eng_idx(t->eng_req)];
    uint32_t n = kb_map(t, k), ti = trk_index(t);
    if (((fm1_in.notes >> k) & 1u) || (n < 128u && ((live_held[ti][n >> 5] >> (n & 31u)) & 1u)))
        return KL_ON;
    if (n >= 128u)
        return KL_OFF;
    if (e->keys && e->keys(t, k) >= 0)
        return KL_DIM;
    if (t->p[P_QUANT] == Q_OFF)
        return (scale_mask(t) >> ((n + 120u - (uint32_t)t->p[P_ROOT]) % 12u)) & 1u ? KL_DIM : KL_OFF;
    return KL_DIM;
}
static const uint8_t LIGHTS_MASK[LIGHTS_N] = {0, 7, 3, 1, 0};   /* the dim keys' duty: -, 1/8, 1/4, 1/2, all */
#define BTN_DIM_MASK 3u                                         /* idle buttons: 1/4 duty each scan */
/* breathing: the third dim plane, its level swept full .. 1/8 .. dark .. 1/8 .. full over a second (125 ms a step) */
static const uint8_t BREATH[8] = {0, 1, 3, 7, 0xFF, 7, 3, 1};
static uint32_t breath_on(void) { return BREATH[(fm1_ms / 125u) & 7u] != 0xFFu; }

static void ui_leds(void)
{
    uint8_t nl[FM1_NCOL] = {0}, nd[FM1_NCOL] = {0}, nb[FM1_NCOL] = {0}, nw[FM1_NCOL] = {0};
    uint32_t k, c, keys = 0, dimkeys = 0, play = 1, lights = settings_lights % LIGHTS_N, id;
    uint32_t place = button_of_place(uix.place);
    static uint8_t ready;
    if (!ready) {
        led_pos_init();
        ready = 1;
    }
    led_put(nl, panel.btn[place], 1);
    if ((k = arp_led()) != 2u)
        led_put(nl, panel.btn[B_ARP], place == B_ARP ? !k : (int)k);   /* (on ARP's page: dark flashes) */
    else if (sustain_on())
        led_put(nl, panel.btn[B_ARP], 1);                    /* the pedal keeps notes */
    led_put(nl, panel.btn[B_REC], TSEL->p[P_AHOLD] != 0);   /* REC: the latch */
    led_put(nl, panel.btn[B_OCTDN], song.octave < 0);
    led_put(nl, panel.btn[B_OCTUP], song.octave > 0);
#ifdef SAADET_SMK37
    led_put(nl, panel.btn[B_SEQ], TSEL->p[P_CHRD] != 0);     /* SEQ: a chord type is on */
    smk_lights();
#endif
    led_put(nw, panel.btn[B_SAVE], sx_staged());             /* a dump waits in SAVE */
    if (lib.deleting) {
        led_put(nw, panel.btn[B_FX], 1);
        nl[led_pos[panel.btn[B_FX]] >> 3] &= (uint8_t)~(1u << (led_pos[panel.btn[B_FX]] & 7u));
    }
    if (uix.held < PL_N && uix.reveal) {                     /* the held button's entries, the current one bright */
        uint8_t s[KB_LIST];
        uint32_t cur, n = held_entries(uix.held, s, &cur);
        dimkeys = n >= 27u ? 0x7FFFFFFu : (1u << n) - 1u;
        keys = cur < 27u ? 1u << cur : 0u;
        play = 0;
    } else {
        keys = fm1_in.notes & ~kb_layer;                    /* the keys held */
    }
    for (k = 0; k < 27u; k++) {
        uint32_t lv = play ? play_key_led(key_track(k), k) : KL_OFF;   /* playing: the layout, MIDI's notes too (LIGHTS) */
        led_put(nl, 14u + k, (int)(((keys >> k) & 1u) || lv == KL_ON));
        led_put(nd, 14u + k, (((dimkeys >> k) & 1u) || lv == KL_DIM) && (lights != LIGHTS_OFF || !play));
    }
    for (k = 0; lights != LIGHTS_OFF && k < NB; k++)       /* the buttons glow when idle (one breathing: not under it) */
        led_put(nb, panel.btn[k], !led_get(nw, panel.btn[k]));
    fm1_led_dim_mask[0] = lights != LIGHTS_OFF ? LIGHTS_MASK[lights] : 3u;   /* (OFF: a held list's keys at 1/4) */
    fm1_led_dim_mask[1] = BTN_DIM_MASK;
    fm1_led_dim_mask[2] = BREATH[(fm1_ms / 125u) & 7u];
    for (c = 0; c < FM1_NCOL; c++) {
        fm1_led_dim[0][c] = (uint8_t)(nd[c] | nl[c]);   /* first: a key going dim <-> bright never goes dark */
        fm1_led_dim[1][c] = nb[c];
        fm1_led_dim[2][c] = breath_on() ? nw[c] : 0u;
        fm1_led[c] = nl[c];
    }
    for (id = 0; id < UI_LEDS; id++)                         /* (the levels drawn, for the tests) */
        ui_led_lv[id] = (uint8_t)(led_get(nl, id) ? 3u : led_get(nw, id) ? 2u :
                                  led_get(nd, id) || led_get(nb, id) ? 1u : 0u);
}

/* ------------------------------------------------------------- draw --- */
static void draw_uboot(void)                     /* OCT- + OCT+ held: the seconds left, letting go cancels */
{
    char d[2] = {(char)('0' + ui.uboot % 10u), 0};
    lcd_fill(0, 0, 240, 240, T_BG);
    draw_text_box(0, 50, 240, TY_TX, "Update mode in", T_TEXT, 1);
    draw_text_box(0, 80, 240, TY_HR, d, T_THEME, 1);
    draw_text_box(0, 170, 240, TY_LB, "Let go to cancel", T_MID, 1);
}

static void ui_draw(void)
{
    static ui_vm_t vm, last;
    gfx_frame(1);
    if (boot_work.state) {                       /* maintenance and its errors: never asleep, never covered */
        scrn.idle = fm1_ms;
        if (ui.force) boot_work_draw();
    } else if (scr_frame()) {
        if (ui.uboot) {
            if (ui.force) draw_uboot();
        } else {
            if (theme_engine(eng_tint(TSEL->eng_req)))
                ui.force = 1;
            vm_build(&vm);
            if (ui.force || memcmp(&vm, &last, sizeof vm)) {
                vm_draw(&vm);
                last = vm;
            }
        }
    }
    ui.force = 0;
    gfx_frame(0);
    scr_shown();
}
