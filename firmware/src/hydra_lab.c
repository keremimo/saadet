/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* HYDRA's WAVE LAB (EDIT > WAVE LAB, docs/hydra-lab/mocks.html): one user wave (hydra_bank.h) edited by ear on the panel.
 *
 * A session starts from the wave as it is (the start) and runs one chain from it: MIX toward FROM, harmonic by harmonic;
 * SHAPE (SKEW, SYNC, FOLD, CRUSH) on a cycle four times the wave's, so what it adds above the 64th harmonic is left out;
 * SPECTRUM (TILT, EVENS, a PEAK round FORMNT); then the drawbars H1..H8, each a level against the start's H1, so a sine
 * can be given harmonics it lacks. The result is kept as the factory waves are (64 harmonics, no DC, its peak at 30000).
 * Every change renders into the bank, heard at once in every part that uses the wave; REVERT at START plays the start
 * instead (A/B). Leaving the lab, or turning WAVE, ends the session: what plays is kept, the values come back to neutral,
 * and the bank goes to flash once the unit rests (hydra_bank.c).
 *
 * The session starts on the wave of the wave field turned last (OSC 1 or 2's WAVE or a SLOT, OSC 3's WAVE). A factory
 * wave there is NEW: the first turn copies it into the first free user wave and points the field at it (a sound edit);
 * with no free wave the turn does nothing. Main loop only. Included by persist.c */
#define HL_OS 1024u                                    /* SHAPE's cycle */
#define HL_H 64u                                       /* the harmonics kept */
#define HL_OFF (-61)                                   /* a drawbar pushed all the way in */
#define HL_RENDER_MS 25u                               /* renders while a knob turns, at most one a frame */
static struct {
    uint8_t on, dirty, field, ftrack;                  /* a session; the chain changed; the wave field it came from (255: none) */
    int8_t k;                                          /* the user wave edited, -1: NEW (copied at the first turn) */
    uint8_t src;                                       /* NEW's source: a wave id (0: the sine) */
    uint8_t from, mix, revert, sync, fold, crush, form, cursor;
    int8_t skew, tilt, evens, peak, hdb[8];
    uint8_t hset;                                      /* the drawbars turned (the others show the chain's level) */
    int8_t pre[8];                                     /* the chain's H1..H8 before the drawbars, against the start's H1 */
    uint8_t last, lastt;                               /* the wave field turned last, its part (255: none) */
    uint16_t shown;                                    /* renders: the picture is drawn again */
    uint32_t gen, t;                                   /* hy_ugen after the lab's own write; the last render */
} hl = {.field = 255u, .last = 255u, .k = -1};
static int16_t hl_start[HY_PTS] __attribute__((section(".pool")));
static int16_t hl_out[HY_PTS] __attribute__((section(".pool")));
static float hl_x[HL_OS] __attribute__((section(".pool")));
static float hl_sre[HL_H + 1u], hl_sim[HL_H + 1u], hl_fre[HL_H + 1u], hl_fim[HL_H + 1u], hl_re[HL_H + 1u], hl_im[HL_H + 1u];
static float hl_ref = 1.0f;                            /* the start's H1 (else its loudest harmonic): the drawbars' 0 dB */
static uint8_t hl_fromid;                              /* the wave whose spectrum hl_fre/hl_fim hold (0: none) */
static uint32_t hl_fromgen;

/* ------------------------------------------------------------ the chain --- */
/* the first 64 harmonics of a cycle of n points (256 or HL_OS): x = sum re cos + im sin */
static void hl_dft(const float *x, uint32_t n, float *re, float *im)
{
    uint32_t k, i, step = 1024u / n;
    re[0] = im[0] = 0.0f;
    for (k = 1; k <= HL_H; k++) {
        float a = 0.0f, b = 0.0f;
        uint32_t ph = 0, inc = k * step;
        for (i = 0; i < n; i++, ph += inc) {
            a += x[i] * (float)SINE[(ph + 256u) & 1023u];
            b += x[i] * (float)SINE[ph & 1023u];
        }
        re[k] = a * (2.0f / 32767.0f) / (float)n;
        im[k] = b * (2.0f / 32767.0f) / (float)n;
    }
}
static void hl_idft(const float *re, const float *im, float *x, uint32_t n)
{
    uint32_t k, i, step = 1024u / n;
    for (i = 0; i < n; i++)
        x[i] = 0.0f;
    for (k = 1; k <= HL_H; k++) {
        float a = re[k] * (1.0f / 32767.0f), b = im[k] * (1.0f / 32767.0f);
        uint32_t ph = 0, inc = k * step;
        if (a == 0.0f && b == 0.0f)
            continue;
        for (i = 0; i < n; i++, ph += inc)
            x[i] += a * (float)SINE[(ph + 256u) & 1023u] + b * (float)SINE[ph & 1023u];
    }
}
static void hl_spectrum(const int16_t *w, float *re, float *im)
{
    for (uint32_t i = 0; i < HY_PTS; i++)
        hl_x[i] = (float)w[i];
    hl_dft(hl_x, HY_PTS, re, im);
}
static const int16_t *hl_points(uint32_t id)           /* a wave by its id (0: none) */
{
    return id >= HY_USER0 && id < HY_USER0 + HY_UWAVES ? hy_uwave(id - HY_USER0)->pts :
           id && id <= HY_NWAVE ? HY_WAVES[id - 1u] : 0;
}
static float hl_mag(uint32_t k) { return fm_sqrtf(hl_re[k] * hl_re[k] + hl_im[k] * hl_im[k]); }
static int8_t hl_db(float m)                           /* against the start's H1, OFF under -60 */
{
    float d = m > 1e-6f * hl_ref ? 20.0f * fm_log2f(m / hl_ref) * 0.30103f : -100.0f;
    return (int8_t)(d < -60.5f ? HL_OFF : d > 12.0f ? 12 : (int32_t)(d + (d < 0.0f ? -0.5f : 0.5f)));
}
static int hl_shaping(void) { return hl.skew || hl.sync > 10u || hl.fold || hl.crush; }
/* the start through the chain into hl_out */
static void hl_render(void)
{
    uint32_t k, i;
    float t = (float)hl.mix * 0.01f, m = 0.0f;
    const int16_t *fp = hl_points(hl.from);
    if (hl.mix && fp && (hl_fromid != hl.from || hl_fromgen != hy_ugen)) {
        hl_spectrum(fp, hl_fre, hl_fim);
        hl_fromid = hl.from;
        hl_fromgen = hy_ugen;
    }
    for (k = 1; k <= HL_H; k++) {                      /* MIX: each harmonic's level, and its phase along the shorter way */
        float ar = hl_sre[k], ai = hl_sim[k], am = fm_sqrtf(ar * ar + ai * ai);
        if (hl.mix && fp) {
            float br = hl_fre[k], bi = hl_fim[k], bm = fm_sqrtf(br * br + bi * bi), ur, ui, um, mag;
            ur = (am > 0.0f ? (1.0f - t) * ar / am : 0.0f) + (bm > 0.0f ? t * br / bm : 0.0f);
            ui = (am > 0.0f ? (1.0f - t) * ai / am : 0.0f) + (bm > 0.0f ? t * bi / bm : 0.0f);
            um = fm_sqrtf(ur * ur + ui * ui);
            mag = (1.0f - t) * am + t * bm;
            hl_re[k] = um > 1e-9f ? mag * ur / um : 0.0f;
            hl_im[k] = um > 1e-9f ? mag * ui / um : 0.0f;
        } else {
            hl_re[k] = ar;
            hl_im[k] = ai;
        }
    }
    if (hl_shaping()) {                                /* SHAPE, on a cycle of HL_OS points */
        static float y[HL_OS] __attribute__((section(".pool")));
        float knee = 0.5f + (float)hl.skew * 0.009f, r = (float)hl.sync * 0.1f, peak = 0.0f, g = 1.0f + (float)hl.fold * 0.07f;
        uint32_t levels = hl.crush ? 2u + (uint32_t)(62.0f * (1.0f - (float)hl.crush * 0.01f) * (1.0f - (float)hl.crush * 0.01f)) : 0u;
        hl_idft(hl_re, hl_im, hl_x, HL_OS);
        for (i = 0; i < HL_OS; i++)
            if (fm_fabsf(hl_x[i]) > peak) peak = fm_fabsf(hl_x[i]);
        for (i = 0; i < HL_OS; i++) {
            float ph = (float)i * (1.0f / HL_OS), at, f, v;
            uint32_t a;
            ph = ph < knee ? 0.5f * ph / knee : 0.5f + 0.5f * (ph - knee) / (1.0f - knee);   /* SKEW: a CZ's knee */
            ph *= r;                                   /* SYNC: the cycle read r times over */
            ph -= (float)(int32_t)ph;
            at = ph * HL_OS;
            a = (uint32_t)at;
            f = at - (float)a;
            v = hl_x[a % HL_OS] + f * (hl_x[(a + 1u) % HL_OS] - hl_x[a % HL_OS]);
            v = peak > 0.0f ? v / peak : 0.0f;
            if (hl.fold) {                             /* FOLD: reflected at +-1 */
                float u = v * g + 1.0f;
                u -= 4.0f * (float)(int32_t)(u * 0.25f);
                if (u < 0.0f) u += 4.0f;
                v = u < 2.0f ? u - 1.0f : 3.0f - u;
            }
            if (levels) {                              /* CRUSH: so many levels */
                float q = 2.0f / (float)(levels - 1u), s = (v + 1.0f) / q;
                v = (float)(int32_t)(s + 0.5f) * q - 1.0f;
            }
            y[i] = v * peak;                           /* (the level it came at: the drawbars' 0 dB stays the start's H1) */
        }
        hl_dft(y, HL_OS, hl_re, hl_im);
    }
    for (k = 1; k <= HL_H; k++) {                      /* SPECTRUM: lean, the evens or the odds, a peak */
        float db = (float)hl.tilt * fm_log2f((float)k);
        if (hl.evens < 0 && !(k & 1u)) db += (float)hl.evens;
        if (hl.evens > 0 && (k & 1u) && k > 1u) db -= (float)hl.evens;
        if (hl.peak) {
            float o = fm_log2f((float)k / (float)hl.form);
            db += (float)hl.peak * fm_expf(-o * o * (1.0f / (2.0f * 0.3f * 0.3f)));
        }
        if (db != 0.0f) {
            float gl = fm_db2lin(db);
            hl_re[k] *= gl;
            hl_im[k] *= gl;
        }
    }
    for (k = 0; k < 8u; k++) {                         /* the drawbars: what the chain gives, those turned set */
        float mag = hl_mag(k + 1u);
        hl.pre[k] = hl_db(mag);
        if (hl.hset & (1u << k)) {
            float want = hl.hdb[k] <= HL_OFF ? 0.0f : hl_ref * fm_db2lin((float)hl.hdb[k]);
            if (mag > 1e-9f) {
                hl_re[k + 1u] *= want / mag;
                hl_im[k + 1u] *= want / mag;
            } else {
                hl_re[k + 1u] = 0.0f;
                hl_im[k + 1u] = want;                  /* (none there: in sine phase) */
            }
        }
    }
    hl_idft(hl_re, hl_im, hl_x, HY_PTS);
    for (i = 0; i < HY_PTS; i++)
        if (fm_fabsf(hl_x[i]) > m) m = fm_fabsf(hl_x[i]);
    for (i = 0; i < HY_PTS; i++) {
        float v = m > 1e-6f ? hl_x[i] * (30000.0f / m) : 0.0f;
        hl_out[i] = (int16_t)(v < 0.0f ? v - 0.5f : v + 0.5f);
    }
    hl.shown++;
}

/* ---------------------------------------------------------- the session --- */
static int16_t hl_wave_id(uint32_t field, const uint8_t *r)   /* the wave a field names (its id) */
{
    return (int16_t)(field == HY_O1 + HO_WAVE || field == HY_O2 + HO_WAVE || field == HY_O3_WAVE ? r[field] + 1u : r[field]);
}
static int hl_is_wave_field(uint32_t id)
{
    return id == HY_O1 + HO_WAVE || id == HY_O2 + HO_WAVE || id == HY_O3_WAVE ||
           (id >= HY_O1 + HO_L0 && id < HY_O1 + HO_L0 + 8u) || (id >= HY_O2 + HO_L0 && id < HY_O2 + HO_L0 + 8u);
}
static void hylab_wave_field(uint32_t id)
{
    if (hl_is_wave_field(id)) {
        hl.last = (uint8_t)id;
        hl.lastt = (uint8_t)song.sel;
    }
}
static void hl_neutral(void)
{
    hl.from = HYW_SAW + 1u;
    hl.mix = hl.revert = hl.fold = hl.crush = hl.hset = 0;
    hl.skew = hl.tilt = hl.evens = hl.peak = 0;
    hl.sync = 10u;
    hl.form = 8u;
    hl.cursor = 0;
}
/* the start: a wave's points, their spectrum, the drawbars' reference */
static void hl_take(const int16_t *w)
{
    uint32_t k;
    float top = 0.0f;
    if (w) memcpy(hl_start, w, sizeof hl_start);
    else memcpy(hl_start, HY_WAVES[HYW_SINE], sizeof hl_start);
    hl_spectrum(hl_start, hl_sre, hl_sim);
    for (k = 1; k <= HL_H; k++) {
        float m = fm_sqrtf(hl_sre[k] * hl_sre[k] + hl_sim[k] * hl_sim[k]);
        if (m > top) top = m;
    }
    hl_ref = fm_sqrtf(hl_sre[1] * hl_sre[1] + hl_sim[1] * hl_sim[1]);
    if (hl_ref < top * 1e-3f) hl_ref = top > 0.0f ? top : 1.0f;   /* (no fundamental: the loudest) */
    hl_neutral();
    hl_render();                                       /* (the drawbars' levels; nothing written) */
    hl.dirty = 0;
}
/* a session on the wave field turned last, else OSC 1's */
static void hl_begin(void)
{
    const uint8_t *r = hy_patch_of(TSEL)->raw;
    int32_t id;
    hl.field = hl.last != 255u && hl.lastt == song.sel ? hl.last : r[HY_O1 + HO_MODE] ? (uint8_t)(HY_O1 + HO_L0) : (uint8_t)(HY_O1 + HO_WAVE);
    hl.ftrack = (uint8_t)song.sel;
    id = hl_wave_id(hl.field, r);
    if (id >= (int32_t)HY_USER0 && id < (int32_t)(HY_USER0 + HY_UWAVES)) {
        hl.k = (int8_t)(id - (int32_t)HY_USER0);
        hl.src = 0;
    } else {
        hl.k = -1;
        hl.src = (uint8_t)(id <= (int32_t)HY_NWAVE ? id : 0);
    }
    hl_take(hl.k >= 0 ? hy_uwave(hl.k)->pts : hl_points(hl.src));
    hl.on = 1;
    hl.gen = hy_ugen;
}
/* what plays is kept: the session ends (rendered first if a turn waits) */
static void hl_end(void)
{
    if (!hl.on)
        return;
    if (hl.k >= 0 && hl.dirty) {
        hl_render();
        hy_uset((uint32_t)hl.k, hl.revert ? hl_start : hl_out, 0, 1);
    }
    hl.on = hl.dirty = 0;
}
/* the first turn on NEW: the start into the first free user wave, the field pointed at it. 0: the bank is full */
static int hl_copy(void)
{
    int32_t k = hy_ufree();
    const char *name = hl.src ? HY_ALL_NAMES[hl.src] : "SINE";
    char m[28];
    if (k < 0) {
        ui_message("BANK FULL");
        return 0;
    }
    hy_uset((uint32_t)k, hl_start, name, 1);
    hl.k = (int8_t)k;
    if (hl.field != 255u && hl.ftrack == song.sel) {    /* (the field's byte: a slot holds the id, a WAVE the id - 1) */
        uint32_t id = HY_USER0 + (uint32_t)k;
        int w = hl.field == HY_O1 + HO_WAVE || hl.field == HY_O2 + HO_WAVE || hl.field == HY_O3_WAVE;
        hy_edit_value(TSEL, hl.field, (int32_t)(w ? id - 1u : id));
    }
    str_cpy(m, name, sizeof m);
    str_cpy(m + str_len(m), " \x9b U", sizeof m - str_len(m));
    fmt_int(m + str_len(m), k + 1);
    ui_message(m);
    hl.gen = hy_ugen;
    return 1;
}
/* every frame (ui.c ui_timers): a session while a lab page is shown, a render while the knobs turn */
static int hl_page(void)
{
    ui_place_t pl;
    if (uix.place != PL_EDIT || TSEL->eng_req != ENGI_HYDRA)
        return 0;
    pl = place_of(PL_EDIT);
    return pl.n && pl.stop[ui_stop(PL_EDIT) % pl.n].f[0].kind == FK_HYLAB;
}
static void hylab_poll(void)
{
    int page = hl_page();
    if (hl.on && (!page || hl.ftrack != song.sel)) {
        hl_end();
        return;
    }
    if (!page)
        return;
    if (!hl.on)
        hl_begin();
    else if (!hl.dirty && hy_ugen != hl.gen) {         /* (the bank changed elsewhere, the editor: the wave from it again) */
        if (hl.k >= 0) hl_take(hy_uwave(hl.k)->pts);
        hl.gen = hy_ugen;
    }
    if (hl.dirty && hl.k >= 0 && fm1_ms - hl.t >= HL_RENDER_MS) {
        hl_render();
        hy_uset((uint32_t)hl.k, hl.revert ? hl_start : hl_out, 0, 1);
        hl.gen = hy_ugen;
        hl.dirty = 0;
        hl.t = fm1_ms;
    }
}

/* ------------------------------------------------------------- the fields --- */
static const param_desc_t *hylab_desc(uint32_t id)
{
    static const param_desc_t D[HL_N] = {
        [HL_WAVE] = {"WAVE", F_INT, 0, HY_UWAVES, 0, 0, 0}, [HL_FROM] = {"FROM", F_INT, 1, HY_NWAVE + HY_UWAVES, HYW_SAW + 1, 0, 0},
        [HL_MIX] = {"MIX", F_INT, 0, 100, 0, 0, "%"}, [HL_REVERT] = {"REVERT", F_INT, 0, 1, 0, 0, 0},
        [HL_SKEW] = {"SKEW", F_INT, -50, 50, 0, 0, 0}, [HL_SYNC] = {"SYNC", F_INT, 10, 80, 10, 0, "x"},
        [HL_FOLD] = {"FOLD", F_INT, 0, 100, 0, 0, 0}, [HL_CRUSH] = {"CRUSH", F_INT, 0, 100, 0, 0, 0},
        [HL_TILT] = {"TILT", F_INT, -12, 12, 0, 0, "dB"}, [HL_EVENS] = {"EVENS", F_INT, -40, 40, 0, 0, "dB"},
        [HL_FORM] = {"FORMNT", F_INT, 1, HL_H, 8, 0, 0}, [HL_PEAK] = {"PEAK", F_INT, -24, 24, 0, 0, "dB"},
        [HL_H1] = {"H1", F_INT, HL_OFF, 12, 0, 0, "dB"}, [HL_H1 + 1] = {"H2", F_INT, HL_OFF, 12, 0, 0, "dB"},
        [HL_H1 + 2] = {"H3", F_INT, HL_OFF, 12, 0, 0, "dB"}, [HL_H1 + 3] = {"H4", F_INT, HL_OFF, 12, 0, 0, "dB"},
        [HL_H1 + 4] = {"H5", F_INT, HL_OFF, 12, 0, 0, "dB"}, [HL_H1 + 5] = {"H6", F_INT, HL_OFF, 12, 0, 0, "dB"},
        [HL_H1 + 6] = {"H7", F_INT, HL_OFF, 12, 0, 0, "dB"}, [HL_H1 + 7] = {"H8", F_INT, HL_OFF, 12, 0, 0, "dB"},
        [HL_CURSOR] = {"CURSOR", F_INT, 0, HY_UNAME - 1, 0, 0, 0},
        [HL_LETTER] = {"LETTER", F_INT, 0, (int16_t)(sizeof LIB_CHARS - 2u), 0, 0, 0},
    };
    if (!hl.on && hl_page())
        hl_begin();
    return id < HL_N ? &D[id] : 0;
}
static int32_t hylab_get(uint32_t id)
{
    const char *n = hl.k >= 0 ? hy_uwave(hl.k)->name : "";
    uint32_t k;
    switch (id) {
    case HL_WAVE: return hl.k + 1;
    case HL_FROM: return hl.from;
    case HL_MIX: return hl.mix;
    case HL_REVERT: return hl.revert;
    case HL_SKEW: return hl.skew;
    case HL_SYNC: return hl.sync;
    case HL_FOLD: return hl.fold;
    case HL_CRUSH: return hl.crush;
    case HL_TILT: return hl.tilt;
    case HL_EVENS: return hl.evens;
    case HL_FORM: return hl.form;
    case HL_PEAK: return hl.peak;
    case HL_CURSOR: return hl.cursor;
    case HL_LETTER:
        for (k = 0; LIB_CHARS[k] && LIB_CHARS[k] != (str_len(n) > hl.cursor ? n[hl.cursor] : ' '); k++)
            ;
        return LIB_CHARS[k] ? (int32_t)k : 0;
    }
    if (id >= HL_H1 && id < HL_H1 + 8u)
        return hl.hset & (1u << (id - HL_H1)) ? hl.hdb[id - HL_H1] : hl.pre[id - HL_H1];
    return 0;
}
static int hylab_active(uint32_t id) { return id != HL_REVERT || hl.k >= 0; }
static void hylab_step(uint32_t id, int32_t s)
{
    const param_desc_t *d = hylab_desc(id);
    int32_t v, was;
    if (!d || !hl.on)
        return;
    was = hylab_get(id);
    v = clamp(was + s, d->min, d->max);
    if (id == HL_REVERT || id == HL_WAVE || id == HL_CURSOR || id == HL_LETTER)
        v = clamp(was + (s > 0 ? 1 : -1), d->min, d->max);
    if (v == was)
        return;
    if (id == HL_WAVE) {                               /* another wave: this one is kept, the next from as it is */
        hl_end();
        hl_begin();
        if (v) {
            hl.k = (int8_t)(v - 1);
            hl.src = 0;
            hl_take(hy_uwave(hl.k)->pts);
        } else {                                       /* NEW: a copy of the field's wave, made at the first turn */
            int32_t w = hl.field != 255u ? hl_wave_id(hl.field, hy_patch_of(TSEL)->raw) : 0;
            hl.k = -1;
            hl.src = (uint8_t)(w < (int32_t)(HY_USER0 + HY_UWAVES) ? w : 0);
            hl_take(hl_points(hl.src));
        }
        hl.gen = hy_ugen;
        return;
    }
    if (id == HL_CURSOR) {
        hl.cursor = (uint8_t)v;
        return;
    }
    if (hl.k < 0 && !hl_copy())                        /* (NEW: the first turn copies it; a full bank: nothing) */
        return;
    if (id == HL_LETTER) {                             /* (as SAVE: spaces fill a gap; the name is the wave's) */
        char n[HY_UNAME + 1u];
        str_cpy(n, hy_uwave(hl.k)->name, sizeof n);
        while (str_len(n) < hl.cursor) str_cpy(n + str_len(n), " ", 2);
        if (str_len(n) <= hl.cursor) n[hl.cursor + 1u] = 0;
        n[hl.cursor] = LIB_CHARS[v];
        hy_uset((uint32_t)hl.k, 0, n, 1);
        hl.gen = hy_ugen;
        return;
    }
    switch (id) {
    case HL_FROM: hl.from = (uint8_t)v; break;
    case HL_MIX: hl.mix = (uint8_t)v; break;
    case HL_REVERT: hl.revert = (uint8_t)v; break;
    case HL_SKEW: hl.skew = (int8_t)v; break;
    case HL_SYNC: hl.sync = (uint8_t)v; break;
    case HL_FOLD: hl.fold = (uint8_t)v; break;
    case HL_CRUSH: hl.crush = (uint8_t)v; break;
    case HL_TILT: hl.tilt = (int8_t)v; break;
    case HL_EVENS: hl.evens = (int8_t)v; break;
    case HL_FORM: hl.form = (uint8_t)v; break;
    case HL_PEAK: hl.peak = (int8_t)v; break;
    default:
        if (id >= HL_H1 && id < HL_H1 + 8u) {
            hl.hdb[id - HL_H1] = (int8_t)v;
            hl.hset = (uint8_t)(hl.hset | 1u << (id - HL_H1));
        }
    }
    hl.dirty = 1;
}
static void hl_signed(char *b, int32_t v)
{
    if (v > 0) { b[0] = '+'; fmt_int(b + 1, v); }
    else fmt_int(b, v);
}
static void hylab_text(uint32_t id, char *val, char *unit)
{
    const param_desc_t *d = hylab_desc(id);
    int32_t v = hylab_get(id);
    unit[0] = 0;
    switch (id) {
    case HL_WAVE:
        if (hl.k >= 0) {
            val[0] = 'U';
            fmt_int(val + 1, hl.k + 1);
            str_cpy(val + str_len(val), " ", 2);
            str_cpy(val + str_len(val), hy_uwave(hl.k)->name, 16 - str_len(val));
        } else
            str_cpy(val, hl.src && hl.src <= HY_NWAVE ? HY_ALL_NAMES[hl.src] : "NEW", 16);
        return;
    case HL_FROM: str_cpy(val, HY_ALL_NAMES[clamp(v, 1, (int32_t)(HY_NWAVE + HY_UWAVES))], 16); return;
    case HL_REVERT: str_cpy(val, hl.k < 0 ? "\x96" : v ? "START" : "EDIT", 16); return;
    case HL_SYNC: fmt_int(val, v / 10); str_cpy(val + str_len(val), ".", 2); fmt_int(val + str_len(val), v % 10); break;
    case HL_FORM: val[0] = 'H'; fmt_int(val + 1, v); return;
    case HL_SKEW: case HL_TILT: case HL_EVENS: case HL_PEAK: hl_signed(val, v); break;
    case HL_CURSOR: fmt_int(val, v + 1); str_cpy(unit, "/7", 6); return;
    case HL_LETTER: {
        char c = LIB_CHARS[clamp(v, 0, (int32_t)sizeof LIB_CHARS - 2)];
        if (c == ' ') str_cpy(val, "SPACE", 6);
        else { val[0] = c; val[1] = 0; }
        return;
    }
    default:
        if (id >= HL_H1 && id < HL_H1 + 8u) {
            if (v <= HL_OFF) { str_cpy(val, "OFF", 16); return; }
            hl_signed(val, v);
        } else
            fmt_int(val, v);
    }
    if (d && d->unit) str_cpy(unit, d->unit, 6);
}

/* ------------------------------------------------------------ the pictures --- */
/* the view holds what makes the picture differ (ui_view.c draws a view that changed): the ids, a render's count, the
 * bank's generation; the drawing reads the waves themselves */
static void hylab_pic(ui_vm_t *v, const ui_stop_t *st)
{
    const uint8_t *r = hy_patch_of(TSEL)->raw;
    uint32_t i;
    v->parg = st->arg;
    v->pv[6] = (int16_t)hy_ugen;
    if (st->pic == PIC_HYSLOTS) {                      /* the four slots the page's knobs turn */
        for (i = 0; i < 4u; i++)
            v->pv[i] = st->f[i].kind == FK_HY ? r[st->f[i].id] : 0;
        return;
    }
    if (!hl.on)
        hl_begin();
    v->pv[0] = (int16_t)hl.shown;
    v->pv[1] = hl.k;
    v->pv[2] = hl.revert;
    v->pv[3] = (int16_t)hl.form;
    str_cpy(v->ps[0], st->page, sizeof v->ps[0]);
    if (hl.k >= 0) {
        v->ps[1][0] = 'U';
        fmt_int(v->ps[1] + 1, hl.k + 1);
        str_cpy(v->ps[1] + str_len(v->ps[1]), " ", 2);
        str_cpy(v->ps[1] + str_len(v->ps[1]), hy_uwave(hl.k)->name, sizeof v->ps[1] - str_len(v->ps[1]));
    } else
        str_cpy(v->ps[1], hl.src && hl.src <= HY_NWAVE ? HY_ALL_NAMES[hl.src] : "NEW", sizeof v->ps[1]);
    if (st->pic == PIC_HYNAME) {                       /* as SAVE draws a name */
        str_cpy(v->name, hl.k >= 0 ? hy_uwave(hl.k)->name : v->ps[1], sizeof v->name);
        v->cursor = hl.cursor;
        str_cpy(v->line[0], "USER WAVE ", sizeof v->line[0]);
        if (hl.k >= 0) {
            fmt_int(v->line[0] + str_len(v->line[0]), hl.k + 1);
            str_cpy(v->line[0] + str_len(v->line[0]), " OF 14", sizeof v->line[0] - str_len(v->line[0]));
        } else
            str_cpy(v->line[0], "A COPY AT THE FIRST TURN", sizeof v->line[0]);
        if (hl.field != 255u) {
            uint32_t o = hl.field >= HY_O3 ? 2u : hl.field >= HY_O2 ? 1u : 0u, b = o == 1u ? HY_O2 : HY_O1;
            str_cpy(v->line[1], o == 2u ? "IN OSC 3" : o ? "IN OSC 2" : "IN OSC 1", sizeof v->line[1]);
            if (o < 2u && hl.field >= b + HO_L0 && hl.field < b + HO_L0 + 8u) {
                str_cpy(v->line[1] + str_len(v->line[1]), ", SLOT ", sizeof v->line[1] - str_len(v->line[1]));
                fmt_int(v->line[1] + str_len(v->line[1]), (int32_t)(hl.field - b - HO_L0 + 1u));
            }
        }
        str_cpy(v->line[2], hl.k >= 0 && (hy_udirty >> hl.k & 1u) ? "KEPT ONCE SAADET RESTS" : "KEPT", sizeof v->line[2]);
    }
}
/* one cycle across w px from x, its middle at y, h px to a peak */
static void hl_cycle(const int16_t *p, int32_t x, int32_t y, int32_t w, int32_t h, uint16_t ink, int32_t width)
{
    int32_t xy[2 * 65];
    uint32_t i, n = w >= 128 ? 65u : 33u;
    for (i = 0; i < n; i++) {
        uint32_t at = i * HY_PTS / (n - 1u);
        int32_t s = p ? p[at % HY_PTS] : 0;
        xy[2 * i] = (x + (int32_t)i * w / (int32_t)(n - 1u)) * 16;
        xy[2 * i + 1] = y * 16 - s * h * 16 / 32768;
    }
    cv_poly(xy, n, width, ink, 3u);
}
static void hylab_draw(const ui_vm_t *v)
{
    uint32_t i, k;
    if (v->pic == PIC_HYSLOTS) {                       /* each slot's cycle over its knob, the one turning in the colour */
        for (i = 0; i < 4u; i++) {
            const int16_t *p = hl_points((uint32_t)v->pv[i]);
            int32_t x = UI_COL_X(i);
            cv_rect(x, 100, 50, 1, T_LINE);
            if (p)
                hl_cycle(p, x, 100, 50, 36, i == v->hot ? T_THEME : T_TEXT, 24);
        }
        return;
    }
    if (v->pic == PIC_HYNAME) {                        /* the name, the letter under the cursor marked; where it is */
        char pre[16], c[2] = {v->name[v->cursor] ? v->name[v->cursor] : ' ', 0};
        int32_t x0, w;
        str_cpy(pre, v->name, (unsigned)v->cursor + 1u);
        x0 = 10 + text_w(TY_NM, pre);
        w = text_w(TY_NM, c);
        cv_rect(x0, 92, w > 8 ? w : 8, 3, T_THEME);
        cv_text_fit(10, 48, TY_NM, v->name, T_TEXT, T_BG, 220);
        cv_text_fit(10, 112, TY_TX, v->line[0], T_MID, T_BG, 220);
        cv_text_fit(10, 134, TY_TX, v->line[1], T_TEXT, T_BG, 220);
        cv_text_fit(10, 156, TY_LB, v->line[2], T_DIM, T_BG, 220);
        return;
    }
    cv_text(10, 34, TY_LB, v->ps[0], T_MID);           /* the page, the wave it edits */
    cv_text_r(230, 34, TY_LB, v->ps[1], T_TEXT, T_BG);
    if (v->pic == PIC_HYWAVE) {                        /* the cycle as it plays, the start in grey behind it */
        cv_rect(12, 106, 216, 1, T_LINE);
        hl_cycle(hl_start, 12, 106, 216, 54, T_FILL, 16);
        hl_cycle(v->pv[2] ? hl_start : hl_out, 12, 106, 216, 54, T_THEME, 32);
        return;
    }
    /* the 64 harmonics, 60 dB tall against the start's H1 (its level ticked); the start's ticks; FORMNT, or the
     * drawbars of the page lit */
    for (k = 1; k <= HL_H; k++) {
        int32_t x = 12 + (int32_t)(k - 1u) * 216 / (int32_t)HL_H, bw = 216 / (int32_t)HL_H - 1;
        float m = fm_sqrtf(hl_re[k] * hl_re[k] + hl_im[k] * hl_im[k]);
        float s = fm_sqrtf(hl_sre[k] * hl_sre[k] + hl_sim[k] * hl_sim[k]);
        int32_t db = hl_db(m), sdb = hl_db(s), hgt = db <= HL_OFF ? 0 : (db + 60) * 100 / 72;
        uint32_t lit = v->parg ? k > 4u * (v->parg - 1u) && k <= 4u * v->parg : 0;
        uint16_t ink = lit ? T_THEME : v->parg ? T_FILL : T_TEXT;
        if (hgt > 0)
            cv_rect(x, 160 - hgt, bw, hgt, ink);
        if (sdb > HL_OFF)
            cv_rect(x, 160 - (sdb + 60) * 100 / 72, bw, 1, T_MID);
    }
    cv_rect(12, 160 - 60 * 100 / 72, 216, 1, T_LINE); /* (0 dB: the start's H1) */
    if (!v->parg) {
        int32_t x = 12 + (int32_t)(v->pv[3] - 1) * 216 / (int32_t)HL_H + 1;
        cv_rect(x, 52, 1, 108, T_MID);
    }
}
