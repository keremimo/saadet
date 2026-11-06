/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* A field: one value a knob edits, wherever it lives (the selected part's parameters, the song's, an engine's own panel:
 * a Prophet's, an FM6 operator's or voice's, a CZ-1 tone's, a JUNO-106 tone's, a HYDRA patch's; the device's settings; the library and
 * SAVE, ui_lib.c). It
 * is read, stepped and written the way the web editor does it (editor.c SET, its native pages, its preferences), so a
 * change from the panel and one from the editor are the same change, and the editor sees it. */
enum { FK_NONE, FK_TRACK, FK_GLOBAL, FK_P5, FK_FM6OP, FK_FM6V, FK_FM6FN, FK_CZ, FK_DEV, FK_LIB, FK_BLE, FK_J106, FK_HY, FK_HYLAB };
/* HYDRA's WAVE LAB (hydra_lab.c): the user wave edited, then one chain from the wave it started as */
enum { HL_WAVE, HL_FROM, HL_MIX, HL_REVERT, HL_SKEW, HL_SYNC, HL_FOLD, HL_CRUSH, HL_TILT, HL_EVENS, HL_FORM, HL_PEAK,
       HL_H1, HL_CURSOR = HL_H1 + 8, HL_LETTER, HL_N };
typedef struct { uint8_t kind, id; const char *label; uint8_t sub; } ui_field_t;   /* label: 0 = the descriptor's own;
                                                                                     * sub: FM6's operator 0..5 (OP1..) */
/* the device's settings (kept with the settings, not the sound): screen sleep, the LEDs' level, the voice budget,
 * MIDI THRU */
#ifdef SAADET_SMK37
enum { DEV_SLEEP, DEV_LIGHTS, DEV_BUDGET, DEV_THRU, DEV_PART, DEV_VEL, DEV_TRS, DEV_PADS, DEV_MASTER, DEV_N };
#ifndef SMK_MASTER_GET                             /* (the host tests'; the firmware: platform/instrument.c) */
static uint32_t smk_master_q12 = 2896u;
#define SMK_MASTER_GET() smk_master_q12
#define SMK_MASTER_SET(v) (smk_master_q12 = (v))
#endif
static const char *const N_KEYVEL[] = {"NORMAL", "SOFT", "HARD", "FIXED"};
#else
enum { DEV_SLEEP, DEV_LIGHTS, DEV_BUDGET, DEV_THRU, DEV_PART, DEV_N };
#endif
enum { BL_RADIO, BL_RETRY, BL_PICK, BL_DROP, BL_FORGET, BL_ACTION };
static const char *const N_SLEEP[] = {"NEVER", "5 MIN", "15 MIN", "30 MIN", "60 MIN"};
static const char *const N_PART[NPART] = {"A", "B"};
static const param_desc_t DEV_D[DEV_N] = {
    {"SLEEP", F_ENUM, 0, 4, 0, N_SLEEP, 0},
    {"LIGHTS", F_ENUM, 0, LIGHTS_N - 1, LIGHTS_MID, LIGHTS_NAME, 0},
    {"BUDGET", F_INT, 8, 32, VOICE_BUDGET_DEFAULT, 0, "pts"},
    {"THRU", F_ENUM, 0, 1, 0, N_ONOFF, 0},
    {"PART", F_ENUM, 0, NPART - 1, 0, N_PART, 0},
#ifdef SAADET_SMK37
    {"VELOCITY", F_ENUM, 0, 3, 0, N_KEYVEL, 0},
    {"TRS OUT", F_ENUM, 0, 1, 1, N_ONOFF, 0},
    {"PAD LIGHTS", F_ENUM, 0, LIGHTS_N - 1, LIGHTS_MID, LIGHTS_NAME, 0},
    {"MASTER", F_INT, 0, 100, 71, 0, "%"},
#endif
};
static const param_desc_t *lib_desc(uint32_t id);        /* ui_lib.c: FK_LIB */
static int32_t lib_get(uint32_t id);
static void lib_step(uint32_t id, int32_t steps);
static void lib_text(uint32_t id, char *val, char *unit);
static const param_desc_t *hylab_desc(uint32_t id);      /* hydra_lab.c: FK_HYLAB */
static int32_t hylab_get(uint32_t id);
static void hylab_step(uint32_t id, int32_t steps);
static void hylab_text(uint32_t id, char *val, char *unit);
static int hylab_active(uint32_t id);
static void hylab_wave_field(uint32_t id);               /* .. a wave field turned: where the lab starts */

static uint32_t ui_tr(void) { return song.sel % NTRK; }
static const uint8_t *fm6_op_bytes(uint32_t op) { return &fm6_patch[ui_tr()][(5u - op % 6u) * FP_OP]; }   /* (OP6 first) */
static void ui_cz_panel(uint8_t *p) { cz_ed_decode(p, cz_patch[ui_tr()].raw); }

/* its descriptor now (the engine's labels and ranges), 0 = nothing to show */
static const param_desc_t *field_desc(const ui_field_t *f)
{
    const param_desc_t *d = 0;
    uint32_t e = TSEL->eng_req;
    switch (f->kind) {
    case FK_TRACK: d = track_desc(TSEL, f->id); break;
    case FK_GLOBAL: d = global_desc(f->id); break;
    case FK_P5:
        if (e == ENGI_PROPHET && f->id < NELEM(P5_PANEL) && P5_PANEL[f->id].label)
            d = p5_keyed_tune(TSEL, f->id) ? &P5_TUNE_DISPLAY[f->id == P5_FREQ_B] :
                f->id == P5_BEND ? &P5_BEND_DISPLAY : &P5_PANEL[f->id];
        break;
    case FK_FM6OP: if (e == ENGI_FM6 && f->id <= FP_OP) d = &FM6_OPD[f->id]; break;
    case FK_FM6V: if (e == ENGI_FM6 && f->id >= FP_PR1 && f->id < FP_NAME) d = &FM6_GD[f->id - FP_PR1]; break;
    case FK_FM6FN: if (e == ENGI_FM6 && f->id < FM6_NFN) d = &FM6_FD[f->id]; break;
    case FK_CZ: if (e == ENGI_CZ && f->id < LCZ_NP && CZ_PD[f->id].label) d = &CZ_PD[f->id]; break;
    case FK_J106: if (e == ENGI_J106 && f->id < J106_FIELDS) d = &J106_PANEL[f->id]; break;
    case FK_HY: if (e == ENGI_HYDRA && hy_field_ok(f->id)) d = hy_desc(TSEL, f->id); break;
    case FK_HYLAB: if (e == ENGI_HYDRA) d = hylab_desc(f->id); break;
    case FK_DEV: d = f->id < DEV_N && (f->id != DEV_PART || trk_n() > 1u) ? &DEV_D[f->id] : 0; break;
    case FK_LIB: d = lib_desc(f->id); break;
#ifdef SAADET_FM1_BASE
    case FK_BLE: {
        static const char *const names[] = {"OFF", "ON"};
        static const param_desc_t radio = {"RADIO", F_ENUM, 0, 1, 1, names, 0};
        static const param_desc_t retry = {"RETRY", F_INT, 0, 1, 0, 0, 0};
        static const param_desc_t pick = {"DEVICE", F_INT, 0, 7, 0, 0, 0};
        if (sd_ble_available()) d = f->id == BL_RADIO ? &radio : f->id == BL_PICK ? &pick : &retry;
        break;
    }
#endif
    }
    return d && d->label && d->label[0] && !str_eq(d->label, "-") ? d : 0;   /* (an engine's unused slot: "-") */
}
static int32_t field_get(const ui_field_t *f)
{
    uint32_t tr = ui_tr();
    switch (f->kind) {
    case FK_TRACK: return TSEL->p[f->id];
    case FK_GLOBAL: return f->id == G_A4 ? tuning_a4 : song.g[f->id];
    case FK_P5: {                                     /* (the tunings in semitones, the bend from 1: as the editor) */
        int32_t raw = p5_patch_of(TSEL)->raw[f->id];
        return p5_keyed_tune(TSEL, f->id) ? raw / 2 - 12 : f->id == P5_BEND ? raw + 1 : raw;
    }
    case FK_FM6OP:
        return f->id == FP_OP ? (int32_t)((fm6_on[tr] >> (5u - f->sub % 6u)) & 1u) : fm6_op_bytes(f->sub)[f->id];
    case FK_FM6V: return fm6_patch[tr][f->id];
    case FK_FM6FN: return fm6_fn[tr][f->id % FM6_NFN];
    case FK_CZ: {
        uint8_t p[LCZ_PACKED];
        ui_cz_panel(p);
        return p[f->id % LCZ_NP];
    }
    case FK_J106: return j106_get(TSEL, f->id);
    case FK_HY: return hy_get(TSEL, f->id);
    case FK_HYLAB: return hylab_get(f->id);
    case FK_DEV:
        if (f->id == DEV_PART) return song.sel;
#ifdef SAADET_SMK37
        if (f->id == DEV_VEL) return settings_key_vel;
        if (f->id == DEV_TRS) return !settings_trs_off;
        if (f->id == DEV_PADS) return settings_pad_lights;
        if (f->id == DEV_MASTER) return (int32_t)((SMK_MASTER_GET() * 100u + 2048u) / 4096u);
#endif
        return f->id == DEV_SLEEP ? (int32_t)scr_get() : f->id == DEV_LIGHTS ? settings_lights :
               f->id == DEV_THRU ? settings_thru : settings_budget;
    case FK_LIB: return lib_get(f->id);
#ifdef SAADET_FM1_BASE
    case FK_BLE: return f->id == BL_RADIO ? sd_ble_enabled() : f->id == BL_PICK ? sd_ble_device_pick() : 0;
#endif
    }
    return 0;
}
static const char *field_label(const ui_field_t *f, const param_desc_t *d) { return f->label ? f->label : d->label; }
/* 0: it does nothing to the sound as it is now (a pulse width with the pulse off, a stage past the envelope's end):
 * shown dim, still editable (prophet_edit.h, cz_edit.h) */
static int field_active(const ui_field_t *f)
{
    switch (f->kind) {
    case FK_P5: return p5_ed_active(TSEL, f->id);
    case FK_CZ: return cz_ed_active(cz_patch[ui_tr()].raw, f->id);
    case FK_J106: return j106_ed_active(TSEL, f->id);
    case FK_HY: return hy_ed_active(TSEL, f->id);
    case FK_HYLAB: return hylab_active(f->id);
    case FK_FM6OP: return f->id == FP_OP || ((fm6_on[ui_tr()] >> (5u - f->sub % 6u)) & 1u);
    case FK_GLOBAL:                                   /* (the parts' settings that do nothing in this mode) */
        return f->id == G_PSPLIT ? parts_mode() == PT_SPLIT : f->id == G_PCH2 ? parts_mode() != PT_ONE : 1;
    }
    return 1;
}
/* steps (+ clockwise) on it: the range, the aliases an enum skips, the shared scale, A4's save. 1 = it changed. A short
 * range or a list moves one step a detent however fast the knob turns */
static int field_step(const ui_field_t *f, int32_t steps)
{
    const param_desc_t *d = field_desc(f);
    uint32_t tr = ui_tr();
    int32_t from, v;
    if (f->kind == FK_LIB) {                          /* (the library's own steps: ui_lib.c) */
        lib_step(f->id, steps);
        return 1;
    }
    if (f->kind == FK_HYLAB) {                        /* (the lab's: hydra_lab.c) */
        if (TSEL->eng_req == ENGI_HYDRA && steps) hylab_step(f->id, steps);
        return 1;
    }
#ifdef SAADET_FM1_BASE
    if (f->kind == FK_BLE) {
        if (!sd_ble_available() || !steps) return 0;
        switch(f->id){
        case BL_RADIO: sd_ble_enable(steps > 0); break;
        case BL_RETRY: if(steps > 0)sd_ble_reconnect(); break;
        case BL_PICK: sd_ble_device_choose(steps); break;
        case BL_DROP: if(steps > 0)sd_ble_drop_host(); break;
        case BL_FORGET: sd_ble_forget_arm(steps > 0); break;
        case BL_ACTION: if(steps > 0)ui_message(sd_ble_device_action()); break;
        }
        return 1;
    }
#endif
    if (!d || d->max <= d->min || !steps)
        return 0;
    if (d->fmt == F_ENUM || d->max - d->min < 24)
        steps = steps > 0 ? 1 : -1;
    from = field_get(f);
    v = clamp(from + steps, d->min, d->max);
    if (d->fmt == F_ENUM)
        v = enum_step(d, from, v);
    if (v == from)
        return 0;
    switch (f->kind) {
    case FK_TRACK:
        TSEL->p[f->id] = (int16_t)v;
        if (scale_shared(f->id))
            scale_share(TSEL);
        break;
    case FK_GLOBAL:
        if (f->id == G_A4) {
            tuning_a4 = (int16_t)v;
            settings_save();
#ifdef SAADET_SMK37
        } else if (f->id == G_CLOCK && v == 2) {       /* the TRS jack only sends here: no clock to follow, so step over it */
            song.g[G_CLOCK] = song.g[G_CLOCK] < 2 && global_desc(G_CLOCK)->max > 2 ? 3 : 1;
#endif
        } else {
            song.g[f->id] = (int16_t)v;
        }
        break;
    case FK_P5:
        p5_edit_value(TSEL, f->id, p5_panel_raw(TSEL, f->id, v));
        break;
    case FK_J106:                                     /* (an edit, not a load: the notes sounding follow it) */
        j106_edit_value(TSEL, f->id, v);
        break;
    case FK_HY:                                       /* (an edit, not a load: the notes sounding follow it) */
        hy_edit_value(TSEL, f->id, v);
        hylab_wave_field(f->id);
        break;
    case FK_FM6OP:                                    /* (an edit, not a load: the notes sounding follow it) */
        if (f->id == FP_OP) {
            fm6_on[tr] = (uint8_t)((fm6_on[tr] & ~(1u << (5u - f->sub % 6u))) | (uint32_t)v << (5u - f->sub % 6u));
            fm6_pgen[tr]++;
            break;
        }
        /* fall through */
    case FK_FM6V: {
        uint8_t p[FP_SIZE + 1u];
        memcpy(p, fm6_patch[tr], sizeof p);
        p[f->kind == FK_FM6V ? f->id : (5u - f->sub % 6u) * FP_OP + f->id] = (uint8_t)v;
        fm6_put_patch(tr, p, 0);
        break;
    }
    case FK_FM6FN: fm6_fn_set(tr, f->id, v); break;
    case FK_CZ: {                                     /* (COMP keeps the tone from before its first edit) */
        uint8_t raw[CZ_BYTES];
        if (cz_ed_put(tr, f->id, (uint32_t)v, raw)) {
            cz_compare_take(tr);
            fm1_irq_off();                            /* the audio reads the tone */
            memcpy(cz_patch[tr].raw, raw, 128u);
            fm1_irq_on();
        }
        break;
    }
    case FK_DEV:                                      /* (as the editor's preferences: saved with the settings) */
        if (f->id == DEV_PART) { track_select((uint32_t)v); break; }   /* (the state's, not a setting) */
        if (f->id == DEV_SLEEP) scr_put((uint32_t)v);
        else if (f->id == DEV_LIGHTS) settings_lights = (uint8_t)v;
        else if (f->id == DEV_THRU) settings_thru = (uint8_t)v;
#ifdef SAADET_SMK37
        else if (f->id == DEV_VEL) settings_key_vel = (uint8_t)v;
        else if (f->id == DEV_TRS) settings_trs_off = (uint8_t)!v;
        else if (f->id == DEV_PADS) settings_pad_lights = (uint8_t)v;
        else if (f->id == DEV_MASTER) { SMK_MASTER_SET((uint32_t)v * 4096u / 100u); break; }   /* (kept by the base) */
#endif
        else settings_budget = (int16_t)v;
        settings_save();
        break;
    }
    return 1;
}
/* its value as the screen shows it: the digits or the name (val), a unit apart ("ms", "Hz"; "%" joins the digits) */
static void field_text(const ui_field_t *f, const param_desc_t *d, char *val, char *unit)
{
    const char *u = "";
#ifdef SAADET_FM1_BASE
    if (f->kind == FK_BLE && f->id == BL_PICK) {
        if(sd_ble_device_count())fmt_int(val,sd_ble_device_pick()+1);
        else str_cpy(val,"-",16);
        unit[0]=0;return;
    }
    if (f->kind == FK_BLE && f->id != BL_RADIO && f->id != BL_PICK) {
        str_cpy(val, "+", 16); unit[0] = 0; return;
    }
#endif
    if (f->kind == FK_LIB) {
        lib_text(f->id, val, unit);
        return;
    }
    if (f->kind == FK_HYLAB) {
        hylab_text(f->id, val, unit);
        return;
    }
    if (f->kind == FK_FM6OP)
        fm6_opsel = (uint8_t)(f->sub % 6u);           /* (an operator's frequency reads its own bytes, params.c) */
    if (f->kind == FK_HY && hy_value_name(TSEL, f->id, d)) {
        str_cpy(val, hy_value_name(TSEL, f->id, d), 16);
        unit[0] = 0;
        return;
    }
    param_format(d, field_get(f), val, &u);
    if (u[0] == '%' && !u[1]) {
        str_cpy(val + str_len(val), "%", 2);
        u = "";
    }
    str_cpy(unit, u, 6);
}
