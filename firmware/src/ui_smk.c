/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* The SMK-37's own controls (docs/SMK37_UX.md): knob and fader banks, the 16 pads and their lights, the bank picker, the
 * header's tag, a fader's pickup. A bank is four existing fields, so a bank's edit is a knob's edit: the editor and the
 * native SysEx see it the same way. Included by ui.c. The pads never double the keybed: where you play they are chords,
 * where you choose a sound they are sounds; a held button lends them to what it lists.
 *
 * smk_in is what the bridge reads each pass (platform/instrument.c; the host tests fill it themselves), smk_pad_out what
 * the pads show. Pads 0..7 are the top row, 8..15 the bottom, left to right. */
/* BK_KNOBS8 is the knobs' alone: all eight, K5..K8 too (MASTER, SELECT, ALGORITHM and PRESETS give way), and last, so
 * the faders' banks are the eight before it */
enum { BK_PAGE, BK_MACROS, BK_FILTER, BK_ENV, BK_LFO, BK_ARP, BK_CHORD, BK_MIX, BK_KNOBS8, BK_N };
static const char *const BK_NAME[BK_N] = {"PAGE", "MACROS", "FILTER", "ENV", "LFO", "ARP", "CHORD", "MIX", "8 KNOBS"};
enum { PM_CHORDS, PM_SOUNDS, PM_FAVS, PM_PARTS, PM_N };   /* the pad bank */
static const char *const PM_NAME[PM_N] = {"CHORDS", "SOUNDS", "FAVS", "PARTS"};
/* what the pads are this moment */
enum { PMAP_CHORDS, PMAP_SOUNDS, PMAP_PICK, PMAP_WAIT, PMAP_LIST, PMAP_FAVS, PMAP_PIANO, PMAP_NAME, PMAP_PARTS };
enum { PS_OFF, PS_ON, PS_FILLED, PS_ACTIVE, PS_WAIT, PS_NONE };
#define SMK_X_KBANK 1u
#define SMK_X_FBANK 2u
#define SMK_X_BOTH (SMK_X_KBANK | SMK_X_FBANK)
#define SMK_X_LEFT 4u
#define SMK_X_RIGHT 8u
#define SMK_STRIP_MS 1500u                         /* a bank's cards after a turn on HOME, a fader's anywhere */
#define SMK_NAME_MS 1000u                          /* SAVE's letters: another tap this soon cycles the same letter */

static struct {
    uint32_t extra, pads, pads_hit;                /* the four buttons, the pads down, hit since the last pass */
    uint8_t vel[16], press[16], fader[4];
    int32_t k5;                                    /* K5's detents while the 8 KNOBS bank has it (smk_k5_lent) */
} smk_in;
static uint32_t smk_pad_out[16];                   /* 0xRRGGBB */
static uint32_t smk_led_extra;                     /* the four buttons' LEDs (SMK_X_*), bit 4 PLAY's red */
static struct {
    uint8_t kbank, fbank, pick, pick_sel, pad_both, pad_tap, pad_was, picked, fader_hot, pad_key[16], name_pad, name_i, mom_n, mom_back, last_ok;
    int16_t seen[4], mom_v[4];
    ui_field_t mom_f[4];
    uint8_t last[4];
    uint32_t pads_was, extra_was, bank_t[2], pad_t, fader_t, fader_t0, name_t, snd_t, master_was;
    uint8_t bank_tap[2];
    struct { uint8_t eng, now; } snd[16];
    uint16_t snd_page;
    uint8_t snd_src;
} smk = {.fbank = BK_ENV, .snd_src = 255u, .master_was = 0xFFFFFFFFu};

static void vm_col_of(vm_col_t *c, const ui_field_t *f);   /* ui.c */

/* ----------------------------------------------------------- the banks --- */
static const ui_stop_t *smk_stop(const ui_stop_t *s, uint32_t n, const char *obj, const char *page)
{
    uint32_t i;
    for (i = 0; i < n; i++)
        if (str_eq(s[i].obj, obj) && str_eq(s[i].page, page))
            return &s[i];
    return 0;
}
/* the operator EDIT shows, or the first: FM6's ENV bank follows it */
static uint32_t smk_fm6_op(void)
{
    uint32_t s = uix.stop[PL_EDIT];
    return s >= 1u && s <= 36u ? (s - 1u) / 6u : 0u;
}
/* field i of a bank (0..3; 0..7 on 8 KNOBS), for the sound playing; 0: the engine has nothing there */
static const ui_field_t *smk_bank_field(uint32_t bank, uint32_t i)
{
    static ui_field_t f;
    const ui_stop_t *st = 0;
    uint32_t e = eng_tint(TSEL->eng_req);
    if (bank == BK_KNOBS8)
        return &HOME_F[e][i & 7u];
    i &= 3u;
    switch (bank) {
    case BK_MACROS: return home_field(i);
    case BK_FILTER:
        st = e == 0u ? smk_stop(ST_EDIT_P5, NELEM(ST_EDIT_P5), "FILTER", "CUTOFF") :
             e == 2u ? smk_stop(ST_EDIT_CZ, NELEM(ST_EDIT_CZ), "LINE 1", "WAVES") :
             e == 3u ? smk_stop(ST_EDIT_J106, NELEM(ST_EDIT_J106), "VCF", "CUTOFF") :
             e == 4u ? smk_stop(ST_EDIT_HY, NELEM(ST_EDIT_HY), "FILTER 1", "CUTOFF") : 0;
        break;
    case BK_ENV:
        if (e == 1u) {
            f = (ui_field_t){FK_FM6OP, (uint8_t)(FP_R1 + i), 0, (uint8_t)smk_fm6_op()};
            return &f;
        }
        st = e == 0u ? smk_stop(ST_ENV_P5, NELEM(ST_ENV_P5), "AMP ENV", "SHAPE") :
             e == 2u ? smk_stop(ST_ENV_CZ, NELEM(ST_ENV_CZ), "DCA 1", "RATES 1-4") :
             e == 3u ? smk_stop(ST_ENV_J106, NELEM(ST_ENV_J106), "ENV", "SHAPE") :
             smk_stop(ST_ENV_HY, NELEM(ST_ENV_HY), "ENV 1", "SHAPE");
        break;
    case BK_LFO: st = e == 0u ? &ST_LFO_P5[0] : e == 1u ? &ST_LFO_FM6[0] : e == 2u ? &ST_LFO_CZ[0] : e == 3u ? &ST_LFO_J106[0] : &ST_LFO_HY[0]; break;
    case BK_ARP: st = &ST_ARP[0]; break;
    case BK_CHORD: st = &ST_SEQ[0]; break;
    case BK_MIX: {
        static const ui_field_t MIX[4] = {FT(P_LEVEL, "LEVEL"), FT(P_PAN, "PAN"), FT(P_SPRD, "SPREAD"), FD(DEV_MASTER, "MASTER")};
        return &MIX[i];
    }
    }
    return st ? &st->f[i] : 0;
}
static int smk_bank_has(uint32_t bank) { return bank == BK_PAGE || smk_bank_field(bank, 0) != 0; }
/* a bank the knobs (w 0) or the faders (1) can take */
static int smk_bank_ok(uint32_t w, uint32_t bank) { return bank < (w ? BK_KNOBS8 : BK_N) && smk_bank_has(bank); }
static uint32_t smk_bank_next(uint32_t w, uint32_t b, int32_t dir)
{
    uint32_t i, n = w ? BK_KNOBS8 : BK_N;
    for (i = 0; i < n; i++) {
        b = (uint32_t)((int32_t)b + dir + (int32_t)n) % n;
        if (smk_bank_ok(w, b))
            break;
    }
    return b;
}
/* the settings, the library and SAVE keep their own four: what they edit is not the sound */
static int smk_place_banked(void) { return uix.place != PL_LIB && uix.place != PL_SAVE && uix.place != PL_GLO; }
/* HOME plays, so the bank stays there. A page you open shows its own four, or its controls could not be reached: the bank
 * takes over only when it is chosen on that page, until the page changes */
static int smk_knob_banked(void)
{
    return smk.kbank != BK_PAGE && smk_place_banked() && !uix.br.on && (uix.place == PL_HOME || smk_knob_here);
}
static const ui_field_t *smk_knob_field(uint32_t k)
{
    static const ui_field_t none = F0;
    const ui_field_t *f = k < 4u || smk.kbank == BK_KNOBS8 ? smk_bank_field(smk.kbank, k) : 0;
    return f ? f : &none;
}
/* the 8 KNOBS bank, on: K5..K8 are the sound's (ui_input), and the board lends K5 (platform/instrument.c) */
static int smk_knobs8(void) { return smk.kbank == BK_KNOBS8 && smk_knob_banked(); }
static int smk_k5_lent(void) { return smk_knobs8(); }
/* the pad bank. HOME's is the player's and stays, saved when the buttons let go; on a page it lasts until the page changes */
static void smk_pad_bank(uint32_t m)
{
    smk.pick_sel = (uint8_t)m;
    smk.pad_tap = 0;
    if (uix.place == PL_HOME) settings_pad_bank = (uint8_t)m;
    else smk_pad_here = (uint8_t)(m + 1u);
    smk.snd_t = 0;
    ui_say("PADS ", PM_NAME[m]);
}
/* a turn goes to the picker while it is up (K1 walks it): 1 = taken */
static int smk_knob_taken(uint32_t k, int32_t s)
{
    smk.fader_t = 0;                               /* (the strip is the knobs' again) */
    if (!smk.pick)
        return 0;
    if (smk.pick == 3u) {
        if (!k)
            smk_pad_bank((smk.pick_sel + (s > 0 ? 1u : PM_N - 1u)) % PM_N);
        return 1;
    }
    if (!k)
        smk.pick_sel = (uint8_t)smk_bank_next(smk.pick - 1u, smk.pick_sel, s > 0 ? 1 : -1);
    if (smk.pick == 1u) { smk.kbank = smk.pick_sel; smk_knob_here = 1; } else smk.fbank = smk.pick_sel;
    smk.picked = 0;
    return 1;
}
static int smk_faders_up(void) { return smk.fader_t && fm1_ms - (smk.fader_t & ~1u) < SMK_STRIP_MS; }
/* a field set to a value: a knob's steps, so ranges, enums and everything a knob's edit does apply */
static void smk_field_to(const ui_field_t *f, int32_t target)
{
    uint32_t n;
    for (n = 0; n < 32u; n++) {
        int32_t cur = field_get(f);
        if (cur == target || !field_step(f, target - cur))
            break;
    }
}

/* ---------------------------------------------------------- the faders --- */
static const ui_field_t *smk_fader_field(uint32_t i)
{
    static const ui_field_t none = F0;
    const ui_field_t *f;
    if (smk.fbank == BK_PAGE) {                    /* the page's four, on a page of the sound */
        if (!smk_place_banked())
            return &none;
        f = uix.place == PL_HOME ? home_field(i) : &place_of(uix.place).stop[ui_stop(uix.place)].f[i];
    } else {
        f = smk_bank_field(smk.fbank, i);
    }
    return f ? f : &none;
}
static int32_t smk_fader_value(const ui_field_t *f)   /* its value as 0..127, -1: nothing there */
{
    const param_desc_t *d = field_desc(f);
    if (!d || d->max <= d->min)
        return -1;
    return (field_get(f) - d->min) * 127 / (d->max - d->min);
}
/* NOTE REPEAT held: a fader's edit lasts as long as the button */
static void smk_momentary_take(const ui_field_t *f)
{
    uint32_t i;
    if (!(fm1_in.buttons & ui_btn_bit(B_LFO)))
        return;
    if (uix.held == PL_LFO) {
        uix.jumped = 1;                            /* (the release turns no page, shows no list) */
        if (!smk.mom_n && uix.held_new)
            smk.mom_back = (uint8_t)(smk_place_before + 1u);
    }
    for (i = 0; i < smk.mom_n; i++)
        if (smk.mom_f[i].kind == f->kind && smk.mom_f[i].id == f->id && smk.mom_f[i].sub == f->sub)
            return;
    if (smk.mom_n < 4u) {
        smk.mom_f[smk.mom_n] = *f;
        smk.mom_v[smk.mom_n++] = (int16_t)field_get(f);
    }
}
static void smk_momentary_poll(void)
{
    uint32_t i, back = smk.mom_back;
    if (!smk.mom_n || (fm1_in.buttons & ui_btn_bit(B_LFO)))
        return;
    for (i = 0; i < smk.mom_n; i++)
        smk_field_to(&smk.mom_f[i], smk.mom_v[i]);
    smk.mom_n = smk.mom_back = 0;
    if (back)
        ui_open(back - 1u);
}
/* A fader is away after anything moves its value under it (a bank, a load, a knob, the editor): it takes over crossing the
 * value or coming next to it, and never jumps there. */
static void smk_faders(void)
{
    uint32_t i;
    if (!smk.fader_t0)                             /* the clock is running long before the first call */
        smk.fader_t0 = fm1_ms | 1u;
    if (fm1_ms - (smk.fader_t0 & ~1u) < 1000u) {   /* (the first readings settle: a fader stands where it stands) */
        for (i = 0; i < 4u; i++)
            smk.last[i] = smk_in.fader[i];
        return;
    }
    for (i = 0; i < 4u; i++) {
        int32_t v = smk_in.fader[i], prev = smk.last[i], moved = v != prev;
        const ui_field_t *f = smk_fader_field(i);
        const param_desc_t *d = field_desc(f);
        int32_t cur = smk_fader_value(f);
        smk.last[i] = (uint8_t)v;
        if (cur != smk.seen[i]) {
            smk.seen[i] = (int16_t)cur;
            smk.picked &= (uint8_t)~(1u << i);
        }
        if (moved) {
            smk.fader_t = fm1_ms | 1u;
            smk.fader_hot = (uint8_t)i;
        }
        if (!moved || cur < 0)
            continue;
        if (!((smk.picked >> i) & 1u)) {
            if ((prev - cur) * (v - cur) > 0 && v - cur != 1 && cur - v != 1)
                continue;
            smk.picked |= (uint8_t)(1u << i);
        }
        smk_momentary_take(f);
        smk_field_to(f, d->min + (d->max - d->min) * v / 127);
        smk.seen[i] = (int16_t)smk_fader_value(f);
    }
}

/* ------------------------------------------------------------ the pads --- */
static uint32_t smk_mode_auto(void) { return uix.place == PL_LIB ? PM_SOUNDS : PM_CHORDS; }
static uint32_t smk_mode_now(void)
{
    if (uix.place == PL_HOME)
        return settings_pad_bank % PM_N;
    return smk_pad_here ? smk_pad_here - 1u : smk_mode_auto();
}
static uint32_t smk_map(void)
{
    if (smk.pick)
        return PMAP_PICK;
    if (sx_staged() || lib.deleting || lib.erase || uix.erasing)
        return PMAP_WAIT;
    if (uix.held < PL_N && uix.reveal)
        return uix.held == PL_HOME ? PMAP_FAVS : uix.held == PL_SCL ? PMAP_PIANO : PMAP_LIST;
    if (uix.place == PL_SAVE)
        return PMAP_NAME;
    return smk_mode_now() == PM_CHORDS ? PMAP_CHORDS : smk_mode_now() == PM_PARTS ? PMAP_PARTS : PMAP_SOUNDS;
}
/* the PARTS pads. Top row: the two parts, then the four modes on the right. Bottom row: the first eight performances */
#define SMK_PART_MODE0 4u
static int smk_pads_sounds(void) { return smk_mode_now() == PM_SOUNDS || smk_mode_now() == PM_FAVS; }
/* the sounds on the pads: FAVS the favourites; in LIBRARY the list it has open; elsewhere the engine's own */
static uint32_t smk_list(void)
{
    uint32_t s;
    if (smk_mode_now() == PM_FAVS)
        return LIB_FAV;
    if (uix.place != PL_LIB)
        for (s = LIB_P5; s <= LIB_HY; s++)
            if (lib_engine(s) == (uint32_t)TSEL->eng_req)
                return s;
    return ui_stop(PL_LIB);
}
static const char *const SMK_LETTERS[16] = {"ABC", "DEF", "GHI", "JKL", "MNO", "PQR", "STU", "VWX", "YZ", "123", "456", "789",
                                            "0.#", "&+/", "-", " "};   /* (every one of ui_lib.c LIB_CHARS) */
static void smk_name_tap(uint32_t p)
{
    const char *g = SMK_LETTERS[p & 15u];
    uint32_t n = str_len(g), i = p == smk.name_pad && fm1_ms - smk.name_t < SMK_NAME_MS ? (smk.name_i + 1u) % n : 0u;
    if (str_len(lib.name) <= lib.cursor) {
        lib.name[lib.cursor] = ' ';
        lib.name[lib.cursor + 1u] = 0;
    }
    lib.name[lib.cursor] = g[i];
    lib.edited = 1;
    smk.name_pad = (uint8_t)p;
    smk.name_i = (uint8_t)i;
    smk.name_t = fm1_ms;
}
/* the piano of SCALE held: the pitch class of a pad, -1 none */
static int32_t smk_piano_pc(uint32_t p)
{
    static const int8_t PC[16] = {0, 2, 4, 5, 7, 9, 11, 0, 1, 3, -1, 6, 8, 10, -1, 1};
    return PC[p & 15u];
}
static void smk_pad_down(uint32_t p, uint32_t vel)
{
    uint32_t map = smk_map();
    smk.pad_key[p] = 0;
    switch (map) {
    case PMAP_CHORDS:
        if (p < 8u) {
            static const ui_field_t CH = FT(P_CHRD, "CHORD");
            const param_desc_t *d = field_desc(&CH);
            smk_field_to(&CH, (int32_t)p);
            if (d && uix.place != PL_SEQ) ui_say("CHORD ", N_CHRD[p]);
        } else {
            uint32_t k = KB_PAD0 + p - 8u;
            smk.pad_key[p] = (uint8_t)k;
            sd_kb_vel[k] = (uint8_t)(vel ? vel : 1u);
            fm1_irq_off();
            sd_kb_pads |= KB_BIT(k);
            fm1_irq_on();
        }
        break;
    case PMAP_SOUNDS: {
        uint32_t src = smk_list(), n = lib_count(src), idx = (n ? lib.at[src] % n : 0u) / 16u * 16u + p;
        lib_item_t it;
        if (idx < n && lib_item(src, idx, &it)) {
            lib.at[src] = (uint16_t)idx;
            lib.pending = 0;
            lib_load(&it);
            smk.snd_t = 0;
        }
        break;
    }
    case PMAP_PICK:
        if (smk.pick == 3u) {
            if (p < PM_N)
                smk_pad_bank(p);
        } else if (smk_bank_ok(smk.pick - 1u, p)) {
            smk.pick_sel = (uint8_t)p;
            if (smk.pick == 1u) { smk.kbank = (uint8_t)p; smk_knob_here = 1; } else smk.fbank = (uint8_t)p;
            smk.picked = 0;
        }
        break;
    case PMAP_LIST: {
        uint8_t s[KB_LIST];
        uint32_t cur, n = held_entries(uix.held, s, &cur);
        if (p < n) {
            uix.stop[uix.held] = s[p];
            ui_open(uix.held);
            uix.jumped = 1;
        }
        break;
    }
    case PMAP_FAVS: {
        lib_item_t it;
        if (lib_item(LIB_FAV, p, &it)) {
            lib_load(&it);
            uix.jumped = 1;
            smk.snd_t = 0;
        }
        break;
    }
    case PMAP_PIANO: {
        static const ui_field_t ROOT = FT(P_ROOT, "ROOT");
        int32_t pc = smk_piano_pc(p);
        if (pc >= 0) {
            smk_field_to(&ROOT, pc);
            uix.jumped = 1;
        }
        break;
    }
    case PMAP_NAME:
        smk_name_tap(p);
        break;
    case PMAP_PARTS:
        if (p < NPART) {
            if (p < trk_n()) {
                track_select(p);
                ui_say("PART ", N_PART[p]);
            }
        } else if (p >= SMK_PART_MODE0 && p < SMK_PART_MODE0 + PT_N) {
            song.g[G_PMODE] = (int16_t)(p - SMK_PART_MODE0);
            ui_say("PARTS ", N_PMODE[p - SMK_PART_MODE0]);
        } else if (p >= 8u && perf_used(p - 8u)) {
            char n[PERF_NAME];
            perf_load(p - 8u);
            perf_name(p - 8u, n);
            ui_message(n);
        }
        break;
    default:                                       /* PMAP_WAIT: the question is the button's */
        break;
    }
}
static void smk_pad_up(uint32_t p)                 /* a pad lets go of what it pressed, whatever the pads are now */
{
    if (smk.pad_key[p]) {
        fm1_irq_off();
        sd_kb_pads &= ~KB_BIT(smk.pad_key[p]);
        fm1_irq_on();
        smk.pad_key[p] = 0;
    }
}
/* every pass, from the bridge: a pad struck plays now, not at the next frame */
static void smk_pads_poll(void)
{
    uint32_t hit = smk_in.pads_hit, up = smk.pads_was & ~smk_in.pads & ~hit, p;
    smk_in.pads_hit = 0;
    for (p = 0; p < 16u; p++) {
        if ((up >> p) & 1u)
            smk_pad_up(p);
        if ((hit >> p) & 1u)
            smk_pad_down(p, smk_in.vel[p]);
    }
    smk.pads_was = smk_in.pads | hit;
    for (p = 0; p < 8u; p++)
        sd_kb_press[p] = smk_in.press[8u + p];
}

/* ---------------------------------------------------- the four buttons --- */
static void smk_bank_apply(uint32_t which, uint32_t b)
{
    if (which) smk.fbank = (uint8_t)b; else { smk.kbank = (uint8_t)b; smk_knob_here = 1; }
    smk.picked = 0;
    ui_say(which ? "FADERS " : "KNOBS ", BK_NAME[b]);
}
static void smk_buttons(void)
{
    uint32_t now = fm1_ms | 1u, x = smk_in.extra, hit = x & ~smk.extra_was, gone = smk.extra_was & ~x, w;
    uint32_t hold = HOLD_MS[settings_hold % 4u];
    int32_t d = (int32_t)((hit & SMK_X_RIGHT) != 0u) - (int32_t)((hit & SMK_X_LEFT) != 0u);
    smk.extra_was = x;
    if (!smk.pad_both && (x & SMK_X_BOTH) == SMK_X_BOTH) {   /* both down: the pad bank, whatever the first had begun */
        smk.pad_both = smk.pad_tap = 1;
        smk.pad_t = fm1_ms;
        smk.pad_was = settings_pad_bank;
        smk.bank_tap[0] = smk.bank_tap[1] = 0;
        smk.pick = 3;
        smk.pick_sel = (uint8_t)smk_mode_now();
    }
    if (smk.pad_both) {                                /* until both are up, neither is a BANK button */
        if (d) {
            smk_pad_bank((smk.pick_sel + (d > 0 ? 1u : PM_N - 1u)) % PM_N);
            d = 0;
        }
        if (!(x & SMK_X_BOTH)) {
            if (smk.pad_tap && fm1_ms - smk.pad_t < hold)
                smk_pad_bank((smk.pick_sel + 1u) % PM_N);
            smk.pad_both = 0;
            smk.pick = 0;
            if (smk_pads_sounds()) {                   /* the pads open on the page of the sound playing */
                uint32_t src = smk_list();
                int32_t at = lib_find_now(src);
                if (at >= 0) lib.at[src] = (uint16_t)at;
                smk.snd_t = 0;
            }
            if (settings_pad_bank != smk.pad_was)
                settings_save();
        }
        return;
    }
    for (w = 0; w < 2u; w++) {
        uint32_t bit = w ? SMK_X_FBANK : SMK_X_KBANK;
        if (hit & bit) {
            smk.bank_t[w] = now;
            smk.bank_tap[w] = 1;
        }
        if ((x & bit) && smk.bank_tap[w] && fm1_ms - (smk.bank_t[w] & ~1u) >= hold) {
            smk.bank_tap[w] = 0;
            smk.pick = (uint8_t)(w + 1u);
            smk.pick_sel = w ? smk.fbank : smk.kbank;
        }
        if ((x & bit) && d) {                      /* held: the arrows walk its banks */
            smk.bank_tap[w] = 0;
            smk.pick = (uint8_t)(w + 1u);
            smk.pick_sel = (uint8_t)smk_bank_next(w, w ? smk.fbank : smk.kbank, d);
            if (w) smk.fbank = smk.pick_sel; else { smk.kbank = smk.pick_sel; smk_knob_here = 1; }
            smk.picked = 0;
            d = 0;
        }
        if (gone & bit) {
            if (smk.bank_tap[w]) {
                /* on a page showing its own four, the first tap brings the bank picked back; then the next */
                uint32_t resume = !w && smk.kbank != BK_PAGE && !smk_knob_banked();
                smk_bank_apply(w, resume ? smk.kbank : smk_bank_next(w, w ? smk.fbank : smk.kbank, 1));
            }
            else if (smk.pick == w + 1u)
                ui_say(w ? "FADERS " : "KNOBS ", BK_NAME[w ? smk.fbank : smk.kbank]);
            smk.bank_tap[w] = 0;
            if (smk.pick == w + 1u)
                smk.pick = 0;
        }
    }
    if (d) {
        if (smk_pads_sounds() && !uix.br.on) {     /* the next 16 sounds; the chosen one follows, nothing loads */
            uint32_t src = smk_list(), n = lib_count(src), at = n ? lib.at[src] % n : 0u;
            if (n) {
                int32_t to = (int32_t)(at / 16u) * 16 + d * 16;
                lib.at[src] = (uint16_t)(to < 0 ? (n - 1u) / 16u * 16u : (uint32_t)to >= n ? 0u : (uint32_t)to);
                smk.snd_t = 0;
            }
        } else if (uix.place == PL_HOME || uix.br.on) {
            browse_turn(0, d);
        } else {
            ui_turn_page(uix.place, d);
        }
    }
}
static void smk_master_poll(void)                  /* K5: a moment in the header, as the octave */
{
    uint32_t m = SMK_MASTER_GET();
    if (smk.master_was != 0xFFFFFFFFu && m != smk.master_was && !(uix.place == PL_GLO)) {
        char b[8];
        fmt_int(b, (int32_t)(m * 100u / 4096u));
        str_cpy(b + str_len(b), "%", 2);
        ui_say("MASTER ", b);
    }
    smk.master_was = m;
}
/* the keys never choose on this keyboard, so any key down under ARP is the pedal's, not a tap of ARP */
static void smk_input(void)
{
    smk_buttons();
    smk_faders();
    smk_momentary_poll();
    smk_master_poll();
    if (uix.held == PL_ARP && KB_KEYS())
        uix.jumped = 1;
}

/* ---------------------------------------------------------- the lights --- */
static uint32_t smk_rgb(uint32_t st, uint32_t tint)
{
    static const uint8_t LV[LIGHTS_N] = {0, 2, 4, 6, 8};
    static const uint8_t BR[8] = {255, 176, 128, 100, 92, 100, 128, 176};
    uint32_t h = T_ENG(tint), r = (h >> 11) * 255u / 31u, g = ((h >> 5) & 63u) * 255u / 63u, b = (h & 31u) * 255u / 31u;
    uint32_t lv = LV[settings_pad_lights % LIGHTS_N];
    switch (st) {
    case PS_ON: r = 70u + r * 2u / 5u; g = 20u + g * 2u / 5u; b = b * 2u / 5u; break;
    case PS_FILLED: break;
    case PS_ACTIVE: r = 190; g = 230; b = 255; break;
    case PS_WAIT: {
        uint32_t k = BR[(fm1_ms / 125u) & 7u];
        r = 255u * k >> 8; g = 110u * k >> 8; b = 0;
        break;
    }
    case PS_NONE: r = 30; g = 9; b = 0; break;
    default: r = 100; g = 34; b = 0; break;
    }
    return (r * lv >> 3) << 16 | (g * lv >> 3) << 8 | (b * lv >> 3);
}
/* a list's 16 sounds around the chosen one, read a few times a second: a scan walks every slot */
static void smk_snd_refresh(uint32_t src, uint32_t page)
{
    lib_item_t w[16];
    uint32_t i;
    if (smk.snd_t && src == smk.snd_src && page == smk.snd_page && fm1_ms - smk.snd_t < 125u)
        return;
    smk.snd_t = fm1_ms | 1u;
    smk.snd_src = (uint8_t)src;
    smk.snd_page = (uint16_t)page;
    lib_scan(src, (int32_t)page * 16, w, 16u);
    for (i = 0; i < 16u; i++) {
        smk.snd[i].eng = w[i].eng;
        smk.snd[i].now = w[i].eng != 255u && lib_item_now(&w[i]);
    }
}
static void smk_lights(void)
{
    uint32_t p, map = smk_map(), e = eng_tint(TSEL->eng_req);
    uint32_t held = smk_in.pads, cur = 255u, n = 0, list = 0;
    uint8_t s[KB_LIST];
    if (map == PMAP_LIST)
        n = held_entries(uix.held, s, &cur);
    if (map == PMAP_SOUNDS || map == PMAP_FAVS) {
        uint32_t c;
        list = map == PMAP_FAVS ? LIB_FAV : smk_list();
        c = lib_count(list);
        smk_snd_refresh(list, map == PMAP_FAVS || !c ? 0u : (lib.at[list] % c) / 16u);
    }
    for (p = 0; p < 16u; p++) {
        uint32_t st = PS_OFF, tint = e, down = (held >> p) & 1u;
        switch (map) {
        case PMAP_CHORDS:
            if (p < 8u) st = (uint32_t)TSEL->p[P_CHRD] == p ? PS_FILLED : PS_ON;
            else st = down ? PS_ACTIVE : p == 8u ? PS_FILLED : PS_ON;
            break;
        case PMAP_SOUNDS:
        case PMAP_FAVS:
            if (smk.snd[p].eng == 255u) { st = PS_OFF; break; }
            tint = eng_tint(smk.snd[p].eng);
            st = down ? PS_ACTIVE : smk.snd[p].now ? PS_FILLED : PS_ON;
            break;
        case PMAP_PICK:
            if (smk.pick == 3u)
                st = p >= PM_N ? PS_NONE : p == smk.pick_sel ? PS_ACTIVE : PS_ON;
            else if (p < (smk.pick == 2u ? BK_KNOBS8 : BK_N))
                st = p == smk.pick_sel ? PS_ACTIVE : smk_bank_has(p) ? PS_ON : PS_OFF;
            else
                st = PS_NONE;
            break;
        case PMAP_WAIT: st = PS_WAIT; break;
        case PMAP_LIST:
            st = p >= n ? PS_NONE : down ? PS_ACTIVE : p == cur ? PS_FILLED : PS_ON;
            break;
        case PMAP_PIANO: {
            int32_t pc = smk_piano_pc(p);
            if (pc < 0) { st = PS_NONE; break; }
            {
                uint32_t rel = ((uint32_t)pc + 12u - (uint32_t)TSEL->p[P_ROOT] % 12u) % 12u;
                st = !rel ? PS_FILLED : (scale_mask(TSEL) >> rel) & 1u ? PS_ON : PS_OFF;
            }
            break;
        }
        case PMAP_NAME:
            st = down || (p == smk.name_pad && fm1_ms - smk.name_t < SMK_NAME_MS && smk.name_t) ? PS_ACTIVE : PS_ON;
            break;
        case PMAP_PARTS:
            if (p < NPART) {
                tint = eng_tint(trk[p].eng_req);
                st = p >= trk_n() ? PS_NONE : down ? PS_ACTIVE : p == song.sel ? PS_FILLED : PS_ON;
            } else if (p >= SMK_PART_MODE0 && p < SMK_PART_MODE0 + PT_N) {
                st = down ? PS_ACTIVE : p - SMK_PART_MODE0 == parts_mode() ? PS_FILLED : PS_ON;
            } else if (p >= 8u && perf_used(p - 8u)) {
                tint = eng_tint(perf_engine(p - 8u));
                st = down ? PS_ACTIVE : perf_now == p - 7u ? PS_FILLED : PS_ON;
            } else {
                st = PS_NONE;
            }
            break;
        }
        smk_pad_out[p] = smk_rgb(st, tint);
    }
    {
        uint32_t x = 0, x_in = smk_in.extra, blink = (fm1_ms / 125u) & 1u;
        if (x_in & SMK_X_KBANK ? blink : smk.kbank != BK_PAGE) x |= SMK_X_KBANK;
        if (x_in & SMK_X_FBANK ? blink : smk.fbank != BK_ENV) x |= SMK_X_FBANK;
        if (uix.erasing) x |= 16u;
        smk_led_extra = x;
    }
}

/* ------------------------------------------------------------ the view --- */
static void smk_tag(ui_vm_t *v)
{
    if (smk_faders_up())
        str_cpy(v->tag, "FADERS", sizeof v->tag);
    else if (smk_knob_banked())
        str_cpy(v->tag, BK_NAME[smk.kbank], sizeof v->tag);
    else if (smk_mode_now() != smk_mode_auto())
        str_cpy(v->tag, PM_NAME[smk_mode_now()], sizeof v->tag);
}
/* a BANK button held: the banks as the held list, the current one marked, numbered as their pads */
static int smk_pick_view(ui_vm_t *v)
{
    uint32_t i, c = smk.pick_sel;
    if (!smk.pick)
        return 0;
    v->mode = VM_REVEAL;
    v->tag[0] = 0;
    if (smk.pick == 3u) {
        str_cpy(v->head, "PAD BANK", sizeof v->head);
        v->cur = (uint8_t)c;
        v->first = 0;
        v->nrows = PM_N;
        for (i = 0; i < PM_N; i++)
            str_cpy(v->row[i], PM_NAME[i], sizeof v->row[i]);
        v->keys = (1u << PM_N) - 1u;
        return 1;
    }
    {
        uint32_t n = smk.pick == 1u ? BK_N : BK_KNOBS8;       /* (8 KNOBS: the knobs' alone) */
        str_cpy(v->head, smk.pick == 1u ? "KNOB BANK" : "FADER BANK", sizeof v->head);
        v->cur = (uint8_t)c;
        v->first = (uint8_t)(c > 3u && n > VM_ROWS ? (c - 3u < n - VM_ROWS ? c - 3u : n - VM_ROWS) : 0u);
        v->nrows = (uint8_t)(n - v->first < VM_ROWS ? n - v->first : VM_ROWS);
        for (i = 0; i < v->nrows; i++)
            str_cpy(v->row[i], BK_NAME[v->first + i], sizeof v->row[i]);
        v->keys = (1u << n) - 1u;
    }
    return 1;
}
/* the strip: a fader's bank for a moment, with where each fader stands, else the knobs' bank. On HOME the cards stand in
 * for the keyboard; on a page, the page's picture stays */
static void smk_strip_view(ui_vm_t *v, int home)
{
    uint32_t i;
    if (smk_faders_up()) {
        for (i = 0; i < 4u; i++) {
            const ui_field_t *f = smk_fader_field(i);
            int32_t cur = smk_fader_value(f);
            memset(&v->col[i], 0, sizeof v->col[i]);
            if (cur < 0) {
                v->col[i].on = 1;
                str_cpy(v->col[i].label, "-", sizeof v->col[i].label);
                str_cpy(v->col[i].val, "--", sizeof v->col[i].val);
                continue;
            }
            vm_col_of(&v->col[i], f);
            v->gauge[i].on = 1;
            v->gauge[i].picked = (uint8_t)((smk.picked >> i) & 1u);
            v->gauge[i].val = (uint8_t)(cur * 50 / 127);
            v->gauge[i].pos = (uint8_t)(smk_in.fader[i] * 50 / 127);
        }
        v->hot = smk.fader_hot;
        v->strip = 1;
        return;
    }
    if (!smk_knob_banked() || (home && !(uix.hot < 4u && fm1_ms - uix.hot_t < SMK_STRIP_MS)))
        return;
    for (i = 0; i < 4u; i++) {                     /* (8 KNOBS: the row last turned) */
        memset(&v->col[i], 0, sizeof v->col[i]);
        vm_col_of(&v->col[i], smk_knob_field(i + (smk.kbank == BK_KNOBS8 ? 4u * uix.hot_row : 0u)));
    }
    v->strip = 1;
}
