/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* The interface on the host (ui_harness.c): every page of every place with each engine passes the lint (nothing cut,
 * off the screen or overlapping); the buttons open their places and turn their pages; a held button lists what is
 * inside and a key jumps there without playing; the knobs edit, and a change from elsewhere reaches the screen; HOME's
 * macros, the octave, the latch (of the arp and of plain notes), ARP held as the sustain pedal, the LEDs, browsing the
 * sounds and the engines; the
 * engines' own pages (every native value on one of them; an edit is the editor's edit); the library, its favourites
 * and HOME's; SAVE and its name; a SysEx dump imported into the slots; the system's own screens; an idle frame sends
 * nothing. UI_PNG=dir saves the screens. */
#include "ui_harness.c"

static int bad;
static void lint_all(const char *where)          /* every stop of every place, as the screen shows it now */
{
    char w[48];
    for (uint32_t p = 0; p < PL_N; p++) {
        ui_place_t pl = place_of(p);
        for (uint32_t s = 0; s < (pl.n ? pl.n : 1u); s++) {
            ui_open(p);
            uix.stop[p] = (uint8_t)s;
            frame();
            snprintf(w, sizeof w, "%s %s %u", where, pl.name, (unsigned)s);
            if (lint_check(w)) bad |= check(w, 0);
        }
    }
    ui_open(PL_HOME);
    frame();
}
static void hold_down(uint32_t b) { fm1_in.buttons |= ui_btn_bit(b); host_pressed |= ui_btn_bit(b); frame(); }
static void hold_up(uint32_t b) { fm1_in.buttons &= ~ui_btn_bit(b); host_released |= ui_btn_bit(b); frame(); }
static uint32_t stop_named(uint32_t p, const char *obj, const char *page)   /* the place's stop of that object, page */
{
    ui_place_t pl = place_of(p);
    for (uint32_t i = 0; i < pl.n; i++)
        if (str_eq(pl.stop[i].obj, obj) && (!page || str_eq(pl.stop[i].page, page))) return i;
    return 255u;
}
static void tick(void) { int32_t out[CTL * 2]; mix_block(out, CTL); }
static void settle(void) { for (uint32_t i = 0; i < 400u; i++) tick(); frame(); }   /* (an engine change: in the audio) */
static void show(uint32_t p, uint32_t stop) { ui_open(p); uix.stop[p] = (uint8_t)stop; frame(); }
/* every native value of the engine on some page of EDIT, ENV or LFO (kind, id; op for an operator's) */
static int covered(uint32_t kind, uint32_t id, uint32_t op)
{
    static const uint8_t PL[3] = {PL_EDIT, PL_ENV, PL_LFO};
    for (uint32_t k = 0; k < 3u; k++) {
        ui_place_t pl = place_of(PL[k]);
        for (uint32_t i = 0; i < pl.n; i++)
            for (uint32_t f = 0; f < 4u; f++)
                if (pl.stop[i].f[f].kind == kind && pl.stop[i].f[f].id == id && (kind != FK_FM6OP || pl.stop[i].f[f].sub == op))
                    return 1;
    }
    return 0;
}
static int coverage(void)
{
    uint32_t i, op, miss = 0;
    engine_load_of(TSEL, ENGI_PROPHET, 1); frame();
    for (i = 0; i < NELEM(P5_PANEL); i++)
        if (P5_PANEL[i].label && !covered(FK_P5, i, 0)) { printf("  P5 %s (%u) on no page\n", P5_PANEL[i].label, (unsigned)i); miss++; }
    engine_load_of(TSEL, ENGI_FM6, 0); frame();
    for (op = 0; op < 6u; op++)
        for (i = 0; i <= FP_OP; i++)
            if (FM6_OPD[i].label && !covered(FK_FM6OP, i, op)) { printf("  OP%u %s on no page\n", (unsigned)op + 1u, FM6_OPD[i].label); miss++; }
    for (i = FP_PR1; i < FP_NAME; i++)
        if (!covered(FK_FM6V, i, 0)) { printf("  FM6 %s on no page\n", FM6_GD[i - FP_PR1].label); miss++; }
    for (i = 0; i < FM6_NFN; i++)
        if (!covered(FK_FM6FN, i, 0)) { printf("  FM6 %s on no page\n", FM6_FD[i].label); miss++; }
    engine_load_of(TSEL, ENGI_CZ, 1); frame();
    for (i = 0; i < LCZ_NP; i++)
        if (CZ_PD[i].label && !covered(FK_CZ, i, 0)) { printf("  CZ %s (%u) on no page\n", CZ_PD[i].label, (unsigned)i); miss++; }
    return miss == 0;
}

int main(void)
{
    char name[24], want[16];
    const char *u;
    power_on();
    frames(200);
    ui_sound_name(name);
    bad |= check("HOME: the engine and the sound", lint_has("P5") && uix.place == PL_HOME && lint_check("home") == 0u);
    ui_png("home");
    bad |= check("an idle frame sends nothing", ui_frame_cost() == 0u);

    /* every page with every engine (the descriptors' labels and names change with the engine) */
    for (uint32_t n = 0; n < NENG_SHOWN; n++) {
        uint32_t e = eng_vis(n);
        engine_load_of(TSEL, e, e == ENGI_PROPHET ? 1u : 0u);
        frame();
        lint_all(ENG_TAG[eng_tint(e)]);
    }
    power_on();
    memset(uix.stop, 0, sizeof uix.stop);          /* (each place's first page, as at power-on) */
    frames(100);

    /* a place's button opens it, again turns its page; SELECT turns it too */
    press(B_ARP);
    bad |= check("ARP opens", uix.place == PL_ARP && lint_has("ARP") && lint_has("PATTERN") && lint_has("MODE"));
    ui_png("arp");
    press(B_ARP);
    bad |= check("ARP again: its next page", ui_stop(PL_ARP) == 1u && lint_has("FEEL"));
    turn(EN_SELECT, 1);
    bad |= check("SELECT turns the page (round)", ui_stop(PL_ARP) == 0u && lint_has("PATTERN"));
    press(B_FX);
    press(B_ARP);
    bad |= check("a place keeps its page", ui_stop(PL_ARP) == 0u && uix.place == PL_ARP);

    /* a knob edits its column; a change from elsewhere (the editor, MIDI) reaches the screen */
    turn(EN_K1, 1);
    bad |= check("K1 edits the first column", TSEL->p[P_AMODE] == 1 && lint_has("UP") && uix.hot == 0u);
    turn(EN_K1, -1);                               /* (the arp off again: a key then sounds at once) */
    TSEL->p[P_AGATE] = 100;
    param_format(track_desc(TSEL, P_AGATE), 100, want, &u);
    str_cpy(want + str_len(want), u, 4);
    frame();
    bad |= check("a change from elsewhere is shown", lint_has(want));
    host_px_sent = 0;
    turn(EN_K4, 1);
    printf("  a knob step sends %u of %u pixels\n", (unsigned)host_px_sent, 240u * 240u);
    bad |= check("a knob step sends what changed only", host_px_sent > 0u && host_px_sent < 240u * 240u / 2u);

    /* the pictures show the sound: the chord's name, the scale's, a route as a sentence, the tempo; the device's
     * settings edit as the editor's preferences do; SNAP lights the scale on the keys */
    TSEL->p[P_CHRD] = 6; TSEL->p[P_ROOT] = 2; TSEL->p[P_SCALE] = 3; TSEL->p[P_QUANT] = Q_SNAP;
    TSEL->p[P_M2SRC] = 6; TSEL->p[P_M2DST] = 2; TSEL->p[P_M2AMT] = 40;
    press(B_SEQ);
    bad |= check("CHORD: the chord's name", lint_has("Dmaj7") && lint_check("chord") == 0u);
    press(B_SCL);
    bad |= check("SCALE: the scale's name", lint_has("D") && lint_has("Dorian"));
    bad |= check("SNAP: every key that plays dim", ui_led_lv[14u + 9u] == 1u && ui_led_lv[14u + 10u] == 1u);
    TSEL->p[P_QUANT] = Q_OFF;
    frame();
    bad |= check("QNT OFF: the scale dim, the rest dark", ui_led_lv[14u + 9u] == 1u &&
                 ui_led_lv[14u + 11u] == 1u && ui_led_lv[14u + 10u] == 0u);   /* (key 9: D, 10: D#, 11: E) */
    press(B_LFO);
    uix.stop[PL_LFO] = (uint8_t)stop_named(PL_LFO, "MATRIX", "AMOUNTS");
    frame();
    bad |= check("MATRIX: a route as a sentence", lint_has("MODW \x9b CUT"));
    press(B_GLO);
    uix.stop[PL_GLO] = 0;
    frame();
    bad |= check("GLOBAL: the tempo large", lint_has("120") && lint_has("BPM"));
    uix.stop[PL_GLO] = 5;
    frame();
    turn(EN_K1, 1);
    turn(EN_K2, 1);
    bad |= check("DISPLAY: sleep and lights are the device's", scr_get() == 1u && settings_lights == LIGHTS_HIGH);
    turn(EN_K1, -1);
    turn(EN_K2, -1);
    uix.stop[PL_GLO] = 4;
    frame();
    turn(EN_K1, -1);
    bad |= check("VOICES: the budget is the device's", settings_budget == VOICE_BUDGET_DEFAULT - 1);
    turn(EN_K1, 1);
    TSEL->p[P_CHRD] = 0; TSEL->p[P_QUANT] = Q_OFF; TSEL->p[P_M2SRC] = TSEL->p[P_M2DST] = TSEL->p[P_M2AMT] = 0;
    memset(uix.stop, 0, sizeof uix.stop);

    /* a held button lists what is inside; a key jumps there and plays no note */
    press(B_LFO);
    hold_down(B_LFO);
    frames(600);
    bad |= check("held: the list", uix.reveal && lint_has("P5 LFO") && lint_has("MATRIX") && lint_check("list") == 0u);
    bad |= check("held: its entries on the keys", ui_led_lv[14] == 3u && ui_led_lv[15] == 1u && ui_led_lv[16] == 1u &&
                 ui_led_lv[17] == 0u);
    ui_png("list");
    key_down(2);
    frame();
    bad |= check("a key jumps", uix.stop[PL_LFO] == stop_named(PL_LFO, "MATRIX", 0) && gates() == 0u);
    key_up(2);
    hold_up(B_LFO);
    bad |= check("letting go stays there", uix.place == PL_LFO && ui_stop(PL_LFO) == stop_named(PL_LFO, "MATRIX", 0) &&
                 !uix.reveal);
    key_down(1);
    frame();
    bad |= check("then the keys play again", gates() > 0u);
    key_up(1);
    frames(500);

    /* HOME: a macro's value over the screen, then HOME again */
    press(B_HOME);
    turn(EN_K1, 2);
    bad |= check("HOME: a macro over the screen", uix.pop && lint_has("CUTOFF") && lint_check("pop") == 0u);
    ui_png("pop");
    frames(1400);
    bad |= check(".. until the knob rests", !uix.pop && !lint_has("CUTOFF") && lint_has("P5"));

    /* OCT- / OCT+, both at once: 0; REC latches */
    press(B_OCTUP);
    bad |= check("OCT+", song.octave == 1 && lint_has("OCT +1"));
    press(B_OCTDN); press(B_OCTDN);
    bad |= check("OCT- twice", song.octave == -1);
    hold_down(B_OCTDN);
    hold_down(B_OCTUP);
    hold_up(B_OCTUP);
    hold_up(B_OCTDN);
    bad |= check("both: octave 0", song.octave == 0);
    press(B_REC);
    bad |= check("REC latches, lit", TSEL->p[P_AHOLD] == 1 && ui_led_lv[panel.btn[B_REC]] == 3u);
    press(B_REC);
    bad |= check("REC again lets go", TSEL->p[P_AHOLD] == 0 && ui_led_lv[panel.btn[B_REC]] == 1u);
    TSEL->p[P_AMODE] = 1;
    frame();
    bad |= check("LEDs: the place bright, the other buttons glow", ui_led_lv[panel.btn[B_HOME]] == 3u &&
                 ui_led_lv[panel.btn[B_ARP]] == 1u && ui_led_lv[panel.btn[B_OCTUP]] == 1u);
    {                                                     /* .. on the matrix: HOME full, the other places' glow */
        uint32_t c, r, home = 0, glow = 0;
        for (c = 0; c < FM1_NCOL; c++)
            for (r = 1; r < 5u; r++) {
                int32_t id = FM1_KEYMAP[r][c];
                if (id == (int32_t)panel.btn[B_HOME]) home = (fm1_led[c] >> r) & 1u;
                if (id == (int32_t)panel.btn[B_FX]) glow = (fm1_led_dim[1][c] >> r) & 1u;
            }
        bad |= check("LEDs: the bytes the scan lights", home && glow && fm1_led_dim_mask[1] == 3u);
    }
    {                                                     /* the scale on the keys (C major from F: F G A B C ..) */
        TSEL->p[P_SCALE] = 1; TSEL->p[P_ROOT] = 0; TSEL->p[P_QUANT] = Q_OFF;
        frame();
        bad |= check("LEDs: the scale on the keys", ui_led_lv[14u + 7u] == 1u && ui_led_lv[14u + 0u] == 1u &&
                     ui_led_lv[14u + 1u] == 0u && ui_led_lv[14u + 6u] == 1u && fm1_led_dim_mask[0] == 3u);
        settings_lights = LIGHTS_OFF;
        frame();
        bad |= check("LIGHTS OFF: no glow", ui_led_lv[14u] == 0u && ui_led_lv[panel.btn[B_FX]] == 0u &&
                     ui_led_lv[panel.btn[B_HOME]] == 3u);
        settings_lights = LIGHTS_MID;
        TSEL->p[P_SCALE] = 0;
    }
    TSEL->p[P_AMODE] = 0;

    /* PRESETS browses, the sound loads at rest; ALGORITHM changes the engine and its colour */
    {
        uint32_t p0 = TSEL->preset;
        char tag[4];
        fmt3(tag, (int32_t)p0 + 3);
        turn(EN_PRESET, 3);
        bad |= check("PRESETS: the list", uix.br.on && lint_has("P5") && lint_has(tag) && lint_check("browse") == 0u);
        ui_png("browse");
        frames(400);
        bad |= check(".. loads at rest", TSEL->preset == p0 + 3u);
        frames(1600);
        bad |= check(".. and closes", !uix.br.on && uix.place == PL_HOME);
        turn(EN_ALGO, 1);
        frames(400);
        bad |= check("ALGORITHM: the next engine", TSEL->eng_req == ENGI_FM6 && T_THEME == T_ENG(1));
        frames(1600);
        ui_png("home_fm6");
    }

    /* the engines' own pages: every native value on one; an edit is the editor's (the patch, the switches); a value
     * that does nothing as the sound is shown dim */
    bad |= check("every native value is on a page", coverage());
    engine_load_of(TSEL, ENGI_PROPHET, 1);
    frames(800);
    p5_patch_of(TSEL)->raw[P5_PULSE_A] = 0;
    {
        ui_field_t w = FP(P5_PW_A, 0), fa = FP(P5_FREQ_A, 0);
        bad |= check("P5: a width with the pulse off is dim", !field_active(&w));
        show(PL_EDIT, stop_named(PL_EDIT, "OSC A", 0));
        int32_t f0 = field_get(&fa);
        turn(EN_K1, 1);
        bad |= check("P5: FREQ in semitones", field_get(&fa) == f0 + 1 && p5_patch_of(TSEL)->raw[P5_FREQ_A] == (f0 + 13) * 2);
        ui_png("p5_osc");
    }
    show(PL_EDIT, stop_named(PL_EDIT, "FILTER", "CUTOFF"));
    bad |= check("P5: the filter", lint_has("CUTOFF") && lint_check("p5 filter") == 0u);
    ui_png("p5_filter");
    show(PL_ENV, stop_named(PL_ENV, "AMP ENV", "SHAPE"));
    ui_png("p5_env");
    engine_load_of(TSEL, ENGI_FM6, 0);
    frames(800);
    {
        uint32_t gen = fm6_pgen[0], ol = fm6_patch[0][(5u - 1u) * FP_OP + FP_OL];
        show(PL_EDIT, stop_named(PL_EDIT, "OP2", "LEVEL"));
        turn(EN_K1, -1);
        bad |= check("FM6: OP2's level is the patch's", fm6_patch[0][4u * FP_OP + FP_OL] == ol - 1u && fm6_pgen[0] != gen);
        turn(EN_K2, -1);
        bad |= check("FM6: OP2 switched off, its values dim", !((fm6_on[0] >> 4) & 1u) && lint_has("OFF"));
        ui_png("fm6_op");
        turn(EN_K2, 1);
        show(PL_EDIT, stop_named(PL_EDIT, "VOICE", 0));
        bad |= check("FM6: the algorithm", lint_has("ALGO") && lint_check("fm6 voice") == 0u);
        ui_png("fm6_algo");
        for (uint32_t a = 0; a < 32u; a++) {              /* every algorithm draws inside its picture */
            fm6_patch[0][FP_ALG] = (uint8_t)a;
            frame();
            if (lint_check("algorithm")) { printf("  algorithm %u\n", (unsigned)a + 1u); bad |= check("FM6: an algorithm's picture", 0); break; }
            if (a == 18u) ui_png("fm6_algo19");
        }
        show(PL_EDIT, stop_named(PL_EDIT, "OP1", "RATES"));
        ui_png("fm6_eg");
    }
    engine_load_of(TSEL, ENGI_CZ, 1);
    frames(800);
    {
        ui_field_t lines = FC(LCZ_LINE, 0);
        int32_t l0 = field_get(&lines);
        show(PL_EDIT, stop_named(PL_EDIT, "TONE", "LINES"));
        turn(EN_K1, l0 < 3 ? 1 : -1);
        bad |= check("CZ-1: LINES is the tone's", field_get(&lines) == l0 + (l0 < 3 ? 1 : -1) && cz_patch_valid(cz_patch[0].raw));
        show(PL_ENV, stop_named(PL_ENV, "DCA 1", "RATES 1-4"));
        bad |= check("CZ-1: an 8-stage envelope", lint_has("DCA 1") && lint_check("cz env") == 0u);
        ui_png("cz_env");
    }
    engine_load_of(TSEL, ENGI_PROPHET, 1);
    frames(800);

    /* the library: lists, a sound at rest loads, the favourites (and HOME's on the keys) */
    memset(&lib, 0, sizeof lib);
    press(B_FX);
    uix.stop[PL_LIB] = LIB_FAV;
    frame();
    bad |= check("LIBRARY: no favourites yet", uix.place == PL_LIB && lint_has("NO FAVOURITES YET"));
    show(PL_LIB, LIB_P5);
    lib_open(LIB_P5);
    frame();
    {
        uint32_t p0 = TSEL->preset;
        char name[24];
        ui_sound_name(name);
        bad |= check("LIBRARY: P5, at the sound playing", lint_has(name) && lib.at[LIB_P5] == p0 && lint_check("lib") == 0u);
        turn(EN_K1, 2);
        frames(400);
        bad |= check(".. a turn loads at rest", TSEL->preset == p0 + 2u);
        turn(EN_K2, 1);
        bad |= check(".. FAV stars it", favorite_has(ENGI_PROPHET, p0 + 2u));
        ui_png("library");
        show(PL_LIB, LIB_FAV);
        bad |= check("FAVOURITES: the starred sound", lib_count(LIB_FAV) == 1u && lint_has("FAV"));
        press(B_HOME);
        engine_load_of(TSEL, ENGI_FM6, 0);
        frames(800);
        hold_down(B_HOME);
        frames(600);
        bad |= check("HOME held: the favourites", uix.reveal && lint_has("FAVOURITES") && ui_led_lv[14] == 1u);
        ui_png("home_favourites");
        key_down(0);
        frame();
        key_up(0);
        hold_up(B_HOME);
        frames(100);
        bad |= check(".. a key plays one", TSEL->eng_req == ENGI_PROPHET && TSEL->preset == p0 + 2u && uix.place == PL_HOME);
        favorite_set(ENGI_PROPHET, p0 + 2u, 0);
    }

    /* SAVE: the slot, the name letter by letter, SAVE again stores; the sound is that slot's then */
    press(B_SAVE);
    bad |= check("SAVE: the screen", uix.place == PL_SAVE && lint_has("PRESS SAVE TO STORE") && lint_has("EMPTY SLOT") &&
                 lint_check("save") == 0u);
    {
        uint32_t slot = lib.slot;
        char b[13];
        turn(EN_K3, 1);                                   /* the first letter one on */
        turn(EN_K2, 1);
        turn(EN_K4, 1);                                   /* starred */
        ui_png("save");
        press(B_SAVE);
        up_name(slot, b);
        bad |= check(".. SAVE again stores", up_used(slot) && b[0] == lib.name[0] && user_of(TSEL) == slot &&
                     up_favorite(slot));
        press(B_HOME);
        frames(1600);                                     /* (the message "SAVED" gone) */
        {
            char want[16] = "USER ";
            up_slot_label(want + 5, slot);
            bad |= check(".. HOME: the user sound", lint_has(want));
        }
        press(B_FX);
        show(PL_LIB, LIB_USER);
        {
            lib_item_t it;
            char n[24], tag[6];
            up_slot_label(tag, slot);
            bad |= check("USER: the sound saved", lib_count(LIB_USER) == 1u && lib_item(LIB_USER, 0, &it) && it.user &&
                         it.idx == slot && (lib_item_name(&it, n), str_eq(n, b)) && lint_has(tag));
        }
    }

    /* a SysEx dump: SAVE shows it, a press imports it into the free slots (DX7 bank of 32, one voice; a CZ-1 tone) */
    {
        static uint8_t bank[4104];
        uint8_t v[FP_SIZE + 1u], pk[FM6_PACKED];
        char name[13], want[13];
        uint32_t k, first, used0 = lib_count(LIB_USER);
        bank[0] = 0xF0; bank[1] = 0x43; bank[2] = 0x00; bank[3] = 0x09; bank[4] = 0x20; bank[5] = 0x00;
        for (k = 0; k < 32u; k++) fm6_factory(k, bank + 6 + k * FM6_PACKED);
        bank[4102] = fm6_chk(bank + 6, 4096); bank[4103] = 0xF7;
        press(B_HOME);
        fm6_sysex(bank, sizeof bank);
        frame();
        bad |= check("SYSEX: a bank waits in SAVE", uix.place == PL_SAVE && lint_has("32 SOUNDS") &&
                     lint_has("PRESS SAVE TO IMPORT") && lint_check("import") == 0u);
        ui_png("import");
        first = lib.first;
        press(B_SAVE);
        up_name(first + 5u, name);
        fm6_factory(5, pk);
        for (k = 0; k < 10u; k++) want[k] = (char)pk[118 + k];
        for (want[10] = 0; k && want[k - 1u] == ' '; k--) want[k - 1u] = 0;   /* (the DX7 pads its names) */
        for (k = str_len(name); k && name[k - 1u] == ' '; k--) name[k - 1u] = 0;
        bad |= check(".. SAVE imports the 32", !sx_staged() && lib_count(LIB_USER) == used0 + 32u && up_used(first + 31u) &&
                     str_eq(name, want));
        memcpy(v, fm6_patch[0], sizeof v);
        bank[0] = 0xF0; bank[1] = 0x43; bank[2] = 0x00; bank[3] = 0x00; bank[4] = 0x01; bank[5] = 0x1B;
        fm6_factory(9, pk); fm6_unpack(pk, v);
        memcpy(bank + 6, v, 155); bank[161] = fm6_chk(bank + 6, 155); bank[162] = 0xF7;
        fm6_sysex(bank, 163);
        frame();
        bad |= check(".. one voice", uix.place == PL_SAVE && lint_has("1 SOUND"));
        press(B_SAVE);
        bad |= check(".. imported and playing", !sx_staged() && user_of(TSEL) == lib.first && TSEL->eng_req == ENGI_FM6);
        {
            uint8_t raw[CZ_BYTES];
            cz_factory(3, raw);
            sysex_stage(ENGI_CZ, raw, 1, CZ_BYTES);
            frame();
            press(B_LFO);
            bad |= check(".. leaving SAVE drops it", !sx_staged() && uix.place == PL_LFO);
            sysex_stage(ENGI_CZ, raw, 1, CZ_BYTES);
            frame();
            press(B_SAVE);
            bad |= check(".. a CZ-1 tone", !sx_staged() && TSEL->eng_req == ENGI_CZ && user_of(TSEL) == lib.first);
        }
    }

    /* REC with the arp off: the notes let go keep sounding until the next keys, or REC again */
    engine_load_of(TSEL, ENGI_PROPHET, 1);
    settle();
    press(B_HOME);
    TSEL->p[P_AMODE] = 0;
    press(B_REC);
    key_down(0); frame(); key_up(0);
    for (uint32_t i = 0; i < 50; i++) tick();
    bad |= check("REC: a note let go keeps sounding", gates() == 1u);
    key_down(4); frame(); key_up(4);
    for (uint32_t i = 0; i < 50; i++) tick();
    bad |= check(".. the next key replaces it", gates() == 1u && TSEL->v[0].gate + TSEL->v[1].gate >= 1u);
    press(B_REC);
    for (uint32_t i = 0; i < 50; i++) tick();
    bad |= check(".. REC again lets go", gates() == 0u);

    /* ARP held is the sustain pedal: the keys play, the notes let go keep sounding and the next join them; one struck
     * again ends first; letting go ends them, or REC latches them; the arp keeps its notes; no list, no tap */
    hold_down(B_ARP);
    key_down(0); frame(); key_up(0);
    key_down(4); frame(); key_up(4);
    for (uint32_t i = 0; i < 50; i++) tick();
    frame();
    bad |= check("ARP held: the notes let go keep sounding, together", gates() == 2u && lint_has("SUSTAIN"));
    key_down(0); frame(); key_up(0);
    for (uint32_t i = 0; i < 50; i++) tick();
    bad |= check(".. one struck again sounds once", gates() == 2u);
    frames(600);
    bad |= check(".. no list", !uix.reveal && uix.place == PL_HOME);
    hold_up(B_ARP);
    for (uint32_t i = 0; i < 50; i++) tick();
    bad |= check(".. letting go ends them, ARP stays shut", gates() == 0u && uix.place == PL_HOME && !lint_has("SUSTAIN"));
    hold_down(B_ARP);
    frames(600);
    hold_up(B_ARP);
    bad |= check(".. held long alone: no tap", uix.place == PL_HOME);
    press(B_REC);
    hold_down(B_ARP);
    key_down(0); frame(); key_up(0);
    hold_up(B_ARP);
    for (uint32_t i = 0; i < 50; i++) tick();
    bad |= check(".. let go with REC on: latched", gates() == 1u);
    key_down(4); frame(); key_up(4);
    for (uint32_t i = 0; i < 50; i++) tick();
    bad |= check(".. until the next keys", gates() == 1u);
    press(B_REC);
    for (uint32_t i = 0; i < 50; i++) tick();
    TSEL->p[P_AMODE] = 1;
    hold_down(B_ARP);
    key_down(0); frame(); key_up(0);
    key_down(4); frame(); key_up(4);
    key_down(0); frame(); key_up(0);
    for (uint32_t i = 0; i < 50; i++) tick();
    bad |= check(".. the arp keeps the notes let go", TSEL->nheld == 2u && TSEL->arp_phys == 2u);
    hold_up(B_ARP);
    for (uint32_t i = 0; i < 50; i++) tick();
    bad |= check(".. letting go ends them", TSEL->nheld == 0u && TSEL->arp_phys == 0u);
    TSEL->p[P_AMODE] = 0;
    for (uint32_t i = 0; i < 400; i++) tick();

    /* HOME's key and chord: the chord the notes make (a slash for another bass), its notes; let go: faded, still
     * there; one note: its name; nothing played: the key */
    {
        uint32_t r = 99;
        const char *q;
        q = chord_of(0x091u, 0, &r);
        bad |= check("chord: C E G is C", q && !q[0] && r == 0u);
        q = chord_of((1u << 11) | (1u << 2) | (1u << 5) | (1u << 7), 11, &r);
        bad |= check("chord: B D F G is G7/B", q && str_eq(q, "7") && r == 7u);
        q = chord_of((1u << 0) | (1u << 3) | (1u << 7) | (1u << 10), 0, &r);
        bad |= check("chord: C Eb G Bb is Cm7", q && str_eq(q, "m7") && r == 0u);
        q = chord_of((1u << 0) | (1u << 4) | (1u << 10), 0, &r);
        bad |= check("chord: C E Bb (no 5th) is C7", q && str_eq(q, "7"));
        bad |= check("chord: C C# D is none", !chord_of(0x007u, 0, &r));
    }
    power_on();
    TSEL->p[P_QUANT] = Q_OFF; TSEL->p[P_AMODE] = 0; TSEL->p[P_CHRD] = 0; TSEL->p[P_SCALE] = 3; TSEL->p[P_ROOT] = 2;
    memset(live_last, 0, sizeof live_last); memset(live_held, 0, sizeof live_held);
    press(B_HOME);
    frames(1600);
    bad |= check("HOME: nothing played, the key", lint_has("D Dorian") && lint_check("home key") == 0u);
    ui_png("home_key");
    key_down(6); key_down(9); key_down(12); key_down(14);         /* B3 D4 F4 G4 */
    frame();
    bad |= check("HOME: the chord held", lint_has("G") && lint_has("7") && lint_has("/") && lint_has("B") &&
                 lint_has("B3 D4 F4 G4") && lint_check("home chord") == 0u);
    ui_png("home_chord");
    key_up(6); key_up(9); key_up(12); key_up(14);
    frame();
    bad |= check(".. let go: still there", lint_has("B3 D4 F4 G4"));
    ui_png("home_letgo");
    key_down(16); key_up(16);                                       /* A4 */
    frame();
    bad |= check("HOME: one note, its name", lint_has("A4") && !lint_has("B3 D4 F4 G4") && lint_check("home note") == 0u);
    frames(500);

    /* the system's own screens: the update countdown, startup maintenance */
    frames(1600);
    ui.uboot = 3; ui.force = 1;
    frame();
    bad |= check("countdown", lint_n() == 3u && lint_has("Update mode in") && lint_check("countdown") == 0u);
    ui_png("uboot");
    ui.uboot = 0; ui.force = 1;
    frame();
    bad |= check("countdown gone: the page again", !lint_has("Update mode in") && lint_has(ENG_TAG[eng_tint(TSEL->eng_req)]));
    static const char *const BOOT[3] = {"boot_progress", "boot_failed", "boot_wipe"};
    for (uint32_t s = 0; s < 3u; s++) {
        boot_work.title = "Updating user presets"; boot_work.presets = 1; boot_work.done = 37; boot_work.total = 128;
        boot_work.state = (uint8_t)(s ? s + 1u : 1u);
        boot_work_draw();
        bad |= check(BOOT[s], lint_check(BOOT[s]) == 0u && lint_n() >= 3u);
        ui_png(BOOT[s]);
    }
    bad |= check("the wipe question names both buttons", lint_has("BACK") && lint_has("WIPE"));
    boot_work.state = 0; ui.force = 1;
    frame();

    /* the lint finds what it is for: an overlap, a cut; a fill clears */
    cv_begin(120, 24, T_BG);
    cv_text(0, 0, TY_RW, "OVERLAP A", T_TEXT);
    cv_text(20, 4, TY_RW, "OVERLAP B", T_TEXT);
    cv_blit(10, 100);
    bad |= check("lint: an overlap", lint_check("probe") >= 1u);
    gfx_fill(0, 0, 240, 240, T_BG);
    bad |= check("lint: a fill clears", lint_n() == 0u);
    cv_begin(40, 24, T_BG);
    cv_text(0, 0, TY_RW, "A LONG LINE", T_TEXT);
    cv_blit(0, 0);
    bad |= check("lint: a cut", lint_check("probe") == 1u);
    (void)name;

    printf(bad ? "UI TEST FAILED\n" : "UI test passed\n");
    return bad;
}
