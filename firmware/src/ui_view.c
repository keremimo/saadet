/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* What the screen shows, as a view (ui_vm_t: every string and bar the frame draws), and the drawing of a view. ui.c
 * builds the view every frame and draws it only when it differs from the last one drawn, so a change from anywhere
 * (a knob, the web editor, MIDI) reaches the screen, and an idle frame sends nothing. A view is drawn whole, a band
 * of rows at a time (gfx.c cv_begin_band); the panel gets only the blocks that changed (gfx.c df_diff). */
enum { VM_PAGE, VM_HOME, VM_POP, VM_REVEAL, VM_BROWSE, VM_SAVE, VM_IMPORT, VM_ERASE, VM_COUNTDOWN };
enum { BAR_NONE, BAR_FILL, BAR_BIPOLAR, BAR_TICK, BAR_STEPS };
enum { KM_KEY, KM_CHORD, KM_NOTE, KM_NOTES };      /* HOME's chord line: the key, a chord, one note, notes */
#ifdef SAADET_SMK37
#define KB_LO 48u                                   /* HOME's keyboard: the keybed, C3 .. C6 */
#define KB_SPAN 37u
#define KB_WW 10                                    /* a white key's pitch and width */
#define KB_KW 9
#else
#define KB_LO 36u                                   /* HOME's keyboard: C2 .. B5 */
#define KB_SPAN 48u
#define KB_WW 8
#define KB_KW 7
#endif
#define KB_Y 180
#define KB_H 32
#define VM_ROWS 7u                                  /* the held button's list: rows on the screen */
#define UI_COL_X(i) (7 + 59 * (int32_t)(i))         /* the four columns (K1..K4): each value fits 54 px */
typedef struct {
    char label[12], val[16], unit[6];
    int16_t a, b;                                   /* BAR_FILL: 0..b px; BAR_BIPOLAR: a..b around 25; BAR_TICK: at a;
                                                     * BAR_STEPS: step a of b */
    uint8_t bar, on, dim;                           /* dim: it does nothing to the sound as it is (field_active) */
} vm_col_t;
typedef struct {
    uint8_t mode, eng, hot;                         /* hot: the column last turned (4: none) */
    uint8_t ndots, dot;
    char head[12], sub[16], toast[28];
    char warn[32];
    vm_col_t col[4];
    char name[24], oct[8], state[48];               /* HOME (name: also POP, SAVE; an empty list's word) */
    uint8_t macro[4];
    uint8_t kmode, kfade;                           /* HOME's chord line (KM_*), the voicing let go (ui_chord.c) */
    char kroot[4], kq[8], kbass[4], kline[40], knotes[48];
    uint32_t kon[2];                                /* the keyboard C2..B5 (MIDI 36..83): the notes sounding */
    uint16_t kscale;                                /* .. the scale's pitch classes from its root (0: a tuning) */
    uint8_t kkey;                                   /* .. its root */
    uint16_t sweep;                                 /* POP: the ring, degrees of 270 */
    uint8_t nrows, first, cur;                      /* REVEAL (cur 255: none) */
    uint32_t keys;
    char row[VM_ROWS][24];
    char brow[5][24], btag[5][6], bcount[12];       /* BROWSE, LIBRARY: the sounds around the one chosen, their tags */
    uint8_t bstar, bnow, bsrc;                      /* .. starred, playing now (bit per row); the source ALGORITHM picks */
    uint8_t cursor;                                 /* SAVE: the letter under the cursor */
    char line[3][28];                               /* SAVE, IMPORT: what the store does */
    uint8_t pic, parg;                              /* PAGE: its picture (ui_pics.c), which one, the picture's data */
    int16_t pv[24];
    char ps[4][24];
    char tag[8];                                    /* the header's state: the part; a bank, FADERS, a pad mode picked by hand */
#ifdef SAADET_SMK37
    uint8_t strip;                                  /* the strip is a bank's cards: on HOME, for the keyboard */
    struct { uint8_t on, picked, val, pos; } gauge[4];   /* a fader's card: its value's bar, where the fader stands */
#endif
} ui_vm_t;
static void pic_draw(const ui_vm_t *v);             /* ui_pics.c */

/* -------------------------------------------------------- the parts --- */
static void vm_header(const ui_vm_t *v)
{
    if (v->warn[0]) {
        cv_rect(0, 0, 240, 30, T_TEXT);
        cv_text_fit(10, 8, TY_HD, v->warn, T_BG, T_TEXT, 220);
        return;
    }
    if (v->toast[0]) {                              /* a message stands in for the header a moment */
        cv_text_fit(10, 8, TY_TX, v->toast, T_TEXT, T_BG, 220);
        return;
    }
    if (v->head[0]) {
        int32_t x = cv_text(10, 8, TY_HD, v->head, v->mode == VM_HOME || v->mode == VM_BROWSE ? T_THEME : T_TEXT);
        int32_t right = v->ndots > 1u ? 216 - 10 * (int32_t)v->ndots : 230, w = text_w(TY_LB, v->tag);
        int32_t room = right - w - 6 - x - 8;
        if (v->tag[0] && v->mode != VM_HOME && (!v->sub[0] || room >= 40)) {   /* (HOME's stands where its octave does; */
            cv_text_r(right, 9, TY_LB, v->tag, T_THEME, T_BG);                 /* too many pages for both: the page's name) */
            if (v->sub[0])
                cv_text_fit(x + 8, 8, TY_TX, v->sub, T_MID, T_BG, room);
        } else
        if (v->sub[0])
            cv_text_fit(x + 8, 8, TY_TX, v->sub, T_MID, T_BG, (v->ndots > 1u ? 216 - 10 * (int32_t)v->ndots : 220) - x);
    }
    for (uint32_t i = 0; v->ndots > 1u && i < v->ndots; i++)
        cv_disc(227 - 10 * (int32_t)(v->ndots - 1u - i), 16, 6, i == v->dot ? T_TEXT : T_OFF, 0);
}
static void vm_bar(int32_t x, const vm_col_t *c, uint16_t ink)
{
    int32_t i;
    cv_rect(x, 231, 50, 2, T_LINE);
    switch (c->bar) {
    case BAR_FILL: cv_rect(x, 231, c->b, 2, ink); break;
    case BAR_BIPOLAR:
        cv_rect(x + 24, 228, 1, 8, T_FILL);
        cv_rect(x + c->a, 231, c->b - c->a, 2, ink);
        break;
    case BAR_TICK: cv_rect(x + c->a, 231, 6, 2, ink); break;
    case BAR_STEPS:                                 /* a few values: one segment each, the value's lit */
        for (i = 0; i < c->b; i++) {
            int32_t w = (50 - 2 * (c->b - 1)) / c->b;
            cv_rect(x + i * (w + 2), 231, w, 2, i == c->a ? ink : T_LINE);
        }
        break;
    }
}
static void vm_cols(const ui_vm_t *v)
{
    for (uint32_t i = 0; i < 4u; i++) {
        const vm_col_t *c = &v->col[i];
        int32_t x = UI_COL_X(i), w;
        uint16_t ink = i == v->hot ? T_THEME : c->dim ? T_DIM : T_TEXT;
        const aafont_t *f = TY_VL;
        if (!c->on)
            continue;
        cv_text(x, 184, TY_LB, c->label, c->dim ? T_DIM : T_MID);
        if (text_w(f, c->val) > 54)                 /* a long name: the row face, the label's, cut if need be */
            f = text_w(TY_RW, c->val) > 54 ? TY_LB : TY_RW;
        w = cv_text_fit(x, 199 + TY_VL->asc - f->asc, f, c->val, ink, T_BG, 54) - x;
        if (c->unit[0] && w + 2 + text_w(TY_LB, c->unit) <= 54)
            cv_text(x + w + 2, 199 + TY_VL->asc - TY_LB->asc, TY_LB, c->unit, T_MID);
#ifdef SAADET_SMK37
        if (v->gauge[i].on) {                       /* a fader: away, its bar waits grey and the tick stands out */
            uint16_t lc = v->gauge[i].picked ? (i == v->hot ? T_THEME : T_FILL) : T_DIM;
            cv_rect(x, 231, 50, 2, T_LINE);
            cv_rect(x, 231, v->gauge[i].val, 2, lc);
            cv_rect(x + v->gauge[i].pos - (v->gauge[i].pos ? 1 : 0), 228, 2, 8, v->gauge[i].picked ? lc : T_TEXT);
            continue;
        }
#endif
        vm_bar(x, c, i == v->hot ? T_THEME : c->dim ? T_LINE : T_FILL);
    }
}
/* the keyboard (27 keys from F) as a strip at y, h tall: the keys of `keys` lit, key `cur` in the engine's colour */
static void vm_keys(int32_t y, int32_t h, uint32_t keys, uint32_t cur)
{
    static const uint8_t BLACK = 0x2A;             /* F F# G G# A A# B: bit k = semitone k (from F) is black; C..E: 0x0A */
    int32_t white = 0;
    for (uint32_t pass = 0; pass < 2u; pass++) {   /* the white keys, then the black ones over them */
        white = 0;
        for (uint32_t k = 0; k < 27u; k++) {
            uint32_t s = k % 12u, black = s < 7u ? (BLACK >> s) & 1u : (0x0Au >> (s - 7u)) & 1u;
            uint16_t c = k == cur ? T_THEME : (keys >> k) & 1u ? T_MID : black ? T_LINE : T_WELL;
            if (!black) {
                if (!pass) cv_rect(8 + 14 * white, y, 13, h, c);
                white++;
            } else if (pass) {
                int32_t x = 8 + 14 * white - 5;
                cv_rect(x - 2, y, 13, h * 6 / 10 + 2, T_BG);
                cv_rect(x, y, 9, h * 6 / 10, c);
            }
        }
    }
}
#ifdef SAADET_SMK37
/* the pads as a strip at y: two rows of eight, the list's entries lit, the current one in the engine's colour */
static void vm_pads(int32_t y, int32_t h, uint32_t keys, uint32_t cur)
{
    uint32_t k;
    for (k = 0; k < 16u; k++) {
        uint16_t c = k == cur ? T_THEME : (keys >> k) & 1u ? T_MID : T_WELL;
        cv_rect(8 + 28 * (int32_t)(k % 8u), y + (int32_t)(k / 8u) * (h / 2), 26, h / 2 - 1, c);
    }
}
#endif
/* five sounds from y: the one chosen large in the middle (its tag in the engine's colour), the others smaller and
 * fainter; a star (a disc) on the right of a favourite, the tag lit for the sound playing now */
static void vm_list(const ui_vm_t *v, int32_t y)
{
    static const int8_t DY[5] = {0, 22, 52, 98, 124};
    if (!v->brow[2][0]) {                           /* an empty list: its word */
        cv_text_c(120, y + 56, TY_RW, v->name, T_MID, T_BG);
        return;
    }
    for (uint32_t i = 0; i < 5u; i++) {
        int32_t ry = y + DY[i], mid = i == 2u, near = i == 1u || i == 3u;
        const aafont_t *f = mid ? TY_NM : near ? TY_RW : TY_TX;
        uint16_t ink = mid ? T_TEXT : near ? T_MID : T_FAINT;
        int32_t ty = ry + f->asc - (mid ? TY_HD : TY_TX)->asc;
        if (!v->brow[i][0])
            continue;
        cv_text(10, ty, mid ? TY_HD : TY_TX, v->btag[i], mid || (v->bnow >> i) & 1u ? T_THEME : T_DIM);
        cv_text_fit(52, ry, f, v->brow[i], ink, T_BG, (v->bstar >> i) & 1u ? 164 : 178);
        if ((v->bstar >> i) & 1u)
            cv_disc(225, ry + f->h / 2, mid ? 9 : 6, T_THEME, 0);
    }
}

/* ---------------------------------------------------------- screens --- */
static void vm_draw_page(const ui_vm_t *v)
{
    vm_header(v);
    if (v->pic == PIC_LIB) vm_list(v, 34);
    else pic_draw(v);
    vm_cols(v);
}
/* HOME's keyboard C2..B5 at KB_Y: the scale's keys lighter, its root lighter still, the notes sounding in the engine's
 * colour (let go: faded); 28 white keys 7 px wide, 8 px apart, the black ones 5 px over them */
static void vm_keyboard(const ui_vm_t *v)
{
    uint16_t lit = v->kfade ? ux_mix(T_BG, T_THEME, 45) : T_THEME;
    uint32_t pass, n, w;
    for (pass = 0; pass < 2u; pass++)
        for (n = KB_LO, w = 0; n < KB_LO + KB_SPAN; n++) {
            uint32_t pc = n % 12u, black = (0x54Au >> pc) & 1u, rel = (pc + 12u - v->kkey) % 12u;
            uint16_t c = (v->kon[(n - KB_LO) >> 5] >> ((n - KB_LO) & 31u)) & 1u ? lit :
                         v->kscale && !rel ? T_FILL : (v->kscale >> rel) & 1u ? (black ? T_OFF : T_LIT) :
                         black ? T_BG : T_WELL;
            if (!black) {
                if (!pass) cv_rect(8 + KB_WW * (int32_t)w, KB_Y, KB_KW, KB_H, c);
                w++;
            } else if (pass) {
                int32_t x = 8 + KB_WW * (int32_t)w - 3;
                cv_rect(x - 1, KB_Y, 7, 20, T_BG);
                cv_rect(x, KB_Y, 5, 19, c == T_BG ? ux_mix(T_BG, T_WELL, 50) : c);   /* (off the scale: darkest) */
            }
        }
}
/* the chord line (y 92) and its notes (y 132): the chord's root large, its quality smaller on the same baseline, a
 * bass not its root after a slash; one note large; notes without a chord in the row face; nothing played: the key */
static void vm_chord(const ui_vm_t *v)
{
    uint16_t ink = v->kfade ? ux_mix(T_BG, T_TEXT, 45) : T_TEXT;
    int32_t x = 10, base = 92 + TY_NM->asc;
    switch (v->kmode) {
    case KM_KEY: cv_text_fit(10, 96, TY_VL, v->kline, T_MID, T_BG, 220); break;
    case KM_NOTE: cv_text(10, 92, TY_NM, v->kline, ink); break;
    case KM_NOTES: cv_text_fit(10, 98, TY_RW, v->kline, ink, T_BG, 220); break;
    default: {
        const aafont_t *rf = TY_NM, *qf = TY_VL;
        int32_t w = text_w(rf, v->kroot) + 1 + text_w(qf, v->kq) + (v->kbass[0] ? 2 + text_w(rf, "/") + text_w(rf, v->kbass) : 0);
        if (w > 220) {                                /* (too wide: a size smaller) */
            rf = TY_VL; qf = TY_TX; base = 96 + TY_VL->asc;
        }
        x = cv_text(x, base - rf->asc, rf, v->kroot, ink);
        if (v->kq[0]) x = cv_text(x + 1, base - qf->asc, qf, v->kq, ink);
        if (v->kbass[0]) {
            x = cv_text(x + 2, base - rf->asc, rf, "/", ink);
            cv_text(x, base - rf->asc, rf, v->kbass, ink);
        }
        break;
    }
    }
    if (v->knotes[0])
        cv_text_fit(10, 132, TY_LB, v->knotes, v->kfade ? ux_mix(T_BG, T_THEME, 55) : T_THEME, T_BG, 220);
}
static void vm_draw_home(const ui_vm_t *v)
{
    const aafont_t *f = text_has(TY_HR, v->name) && text_w(TY_HR, v->name) <= 220 ? TY_HR : TY_NM;
    int32_t i;
    vm_header(v);
    if (!v->toast[0] && v->tag[0]) {
        cv_text_r(230, 9, TY_LB, v->tag, T_THEME, T_BG);
    } else
    if (!v->toast[0]) {
        cv_text_r(230, 8, TY_TX, v->oct, T_MID, T_BG);
    }
    cv_text_fit(10, f == TY_HR ? 34 : 42, f, v->name, T_TEXT, T_BG, 220);   /* (long: the smaller face, cut) */
    vm_chord(v);
    cv_text_fit(10, 154, TY_TX, v->state, T_MID, T_BG, 220);
#ifdef SAADET_SMK37
    if (v->strip) {
        vm_cols(v);
        return;
    }
#endif
    vm_keyboard(v);
    for (i = 0; i < 4; i++) {
        cv_rect(UI_COL_X(i), 222, 50, 4, T_LINE);
        cv_rect(UI_COL_X(i), 222, v->macro[i], 4, T_THEME);
    }
}
static void vm_draw_pop(const ui_vm_t *v)
{
    const aafont_t *f = text_has(TY_PO, v->col[0].val) ? TY_PO : TY_HR;
    cv_arc(120 * 16, 128 * 16, 82 * 16, 6 * 16, 135, 270, T_LINE);
    if (v->sweep)
        cv_arc(120 * 16, 128 * 16, 82 * 16, 6 * 16, 135, v->sweep, T_THEME);
    cv_text(10, 8, TY_HD, v->sub, T_TEXT);
    cv_text_c(120, ty_y(128, f), f, v->col[0].val, T_TEXT, T_BG);
    cv_text_c(120, 212, TY_TX, v->name, T_MID, T_BG);
}
static void vm_draw_reveal(const ui_vm_t *v)
{
    char n[4];
    vm_header(v);
    for (uint32_t i = 0; i < v->nrows; i++) {
        int32_t y = 30 + 24 * (int32_t)i;
        fmt_int(n, (int32_t)(v->first + i + 1u));
        cv_text(10, y + 3, TY_HD, n, T_THEME);
        cv_text_fit(34, y, TY_RW, v->row[i], v->first + i == v->cur ? T_TEXT : T_MID, T_BG, 190);
    }
#ifdef SAADET_SMK37
    vm_pads(206, 28, v->keys, v->cur);
#else
    vm_keys(206, 28, v->keys, v->cur);
#endif
}
static void vm_draw_browse(const ui_vm_t *v)
{
    int32_t x = 10;
    vm_header(v);
    if (!v->toast[0])
        cv_text_r(230, 8, TY_TX, v->bcount, T_MID, T_BG);
    vm_list(v, 40);
    int32_t total = 0, gap = 16;
    for (uint32_t e = 0; e < NELEM(ENG_TAG); e++) total += text_w(e == v->bsrc ? TY_HD : TY_TX, ENG_TAG[e]);
    if (NELEM(ENG_TAG) > 1u && total + gap * (int32_t)(NELEM(ENG_TAG) - 1u) > 220)   /* (a sixth engine: closer) */
        gap = (220 - total) / (int32_t)(NELEM(ENG_TAG) - 1u);
    if (gap < 4) gap = 4;
    for (uint32_t e = 0; e < NELEM(ENG_TAG); e++) {  /* the sources ALGORITHM picks, the chosen one lit */
        const aafont_t *f = e == v->bsrc ? TY_HD : TY_TX;
        int32_t w = text_w(f, ENG_TAG[e]);
        cv_text(x, 212, f, ENG_TAG[e], e == v->bsrc ? T_TEXT : T_DIM);
        if (e == v->bsrc)
            cv_rect(x, 231, w, 2, T_THEME);
        x += w + gap;
    }
}
static void vm_draw_save(const ui_vm_t *v)          /* the name, the letter under the cursor marked; what a press does */
{
    char pre[16];
    int32_t x0;
    vm_header(v);
    str_cpy(pre, v->name, (unsigned)v->cursor + 1u);
    x0 = 10 + text_w(TY_NM, pre);
    {
        char c[2] = {v->name[v->cursor] ? v->name[v->cursor] : ' ', 0};
        int32_t w = text_w(TY_NM, c);
        cv_rect(x0, 92, w > 8 ? w : 8, 3, T_THEME);
    }
    cv_text_fit(10, 48, TY_NM, v->name, T_TEXT, T_BG, 220);
    cv_text_fit(10, 112, TY_TX, v->line[0], T_MID, T_BG, 220);
    cv_text_fit(10, 134, TY_TX, v->line[1], T_TEXT, T_BG, 220);
    cv_text_fit(10, 156, TY_LB, v->line[2], T_DIM, T_BG, 220);
    vm_cols(v);
}
static void vm_draw_import(const ui_vm_t *v)        /* the dump: how many, the first names, the slots it goes to */
{
    vm_header(v);
    cv_text_fit(10, 40, TY_NM, v->name, T_TEXT, T_BG, 220);
    for (uint32_t i = 0; i < 3u; i++)
        if (v->row[i][0]) cv_text_fit(10, 82 + 18 * (int32_t)i, TY_TX, v->row[i], T_MID, T_BG, 220);
    cv_text_fit(10, 138, TY_TX, v->line[0], T_TEXT, T_BG, 220);
    cv_text_fit(10, 158, TY_LB, v->line[2], T_DIM, T_BG, 220);
    vm_cols(v);
}
static void vm_draw_erase(const ui_vm_t *v)
{
    static const char *const WHAT[] = {"All 384 user sound slots", "Favourites", "CZ-1 and Prophet banks",
        "Settings and calibration", "The sound restored at power-on"};
    vm_header(v);
    cv_text(10, 40, TY_RW, "Erase everything", T_TEXT);
    for (uint32_t i = 0; i < NELEM(WHAT); i++)
        cv_text_fit(10, 66 + 20 * (int32_t)i, TY_TX, WHAT[i], v->warn[0] ? T_TEXT : T_DIM, T_BG, 220);
    cv_text(10, 166, TY_LB, v->warn[0] ? "K1 back to cancel" : "The firmware stays.", T_MID);
    vm_cols(v);
}
static void vm_draw_countdown(const ui_vm_t *v)
{
    vm_header(v);
    cv_arc(120 * 16, 124 * 16, 68 * 16, 8 * 16, 0, 360, T_LINE);
    cv_arc(120 * 16, 124 * 16, 68 * 16, 8 * 16, 270, v->sweep, T_THEME);
    cv_text_c(120, ty_y(124, TY_PO), TY_PO, v->name, T_TEXT, T_BG);
    cv_text_c(120, 212, TY_TX, "Let go to cancel", T_MID, T_BG);
}
static void vm_draw(const ui_vm_t *v)
{
    uint32_t rows = CV_MAX / 240u, top;
    for (top = 0; top < 240u; top += rows) {
        cv_begin_band(240, 240, T_BG, top);
        switch (v->mode) {
        case VM_HOME: vm_draw_home(v); break;
        case VM_POP: vm_draw_pop(v); break;
        case VM_REVEAL: vm_draw_reveal(v); break;
        case VM_BROWSE: vm_draw_browse(v); break;
        case VM_SAVE: vm_draw_save(v); break;
        case VM_IMPORT: vm_draw_import(v); break;
        case VM_ERASE: vm_draw_erase(v); break;
        case VM_COUNTDOWN: vm_draw_countdown(v); break;
        default: vm_draw_page(v); break;
        }
        cv_blit_band(0, 0);
    }
}
