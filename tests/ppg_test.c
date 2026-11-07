/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* PPG (SAADET_PPG): the wave memory a table loads (key waves kept, blends between them, the standard waves at 60..63),
 * the voice (pitch, the filter, the loudness's envelope and its swell, a sweep that steps at half cycles), the fields'
 * signed values and the engine's place among the others. Needs the local wave image (tools/gen_ppg_waves.py); with the
 * stub header it says so and passes, so the suite runs on machines without it. Prints no ROM data. */
#include "ppg_rom.h"
#if !PPG_HAVE_ROM
#include <stdio.h>
int main(void) { printf("ppg: no PPG wave image (private/ppg/ppg22-waves.bin): skipped\n"); return 0; }
#else
#define SAADET_PPG 1
#include <assert.h>
#include "synth_harness.c"
static int failures;
static void ok(const char *what, int pass) { printf("ppg: %s: %s\n", what, pass ? "ok" : "FAIL"); failures += !pass; }
static ppg_patch_t *patch(void) { return ppg_patch_of(TSEL); }
static void ppg(void)
{
    power_on();
    host_preset(TSEL, ENGI_PPG, PPG_INIT);
    TSEL->p[P_VOICE] = V_POLY;
}
static int32_t block(int32_t *out)
{
    int32_t peak = 0;
    track_render(TSEL, out, CTL);
    for (uint32_t i = 0; i < CTL; i++) { int32_t a = out[i] < 0 ? -out[i] : out[i]; if (a > peak) peak = a; }
    return peak;
}
static uint32_t live(void) { uint32_t n = 0; for (uint32_t i = 0; i < NVOICE; i++) n += TSEL->v[i].active; return n; }
static ppg_voice_t *voiceg(uint32_t g) { for (uint32_t i = 0; i < NVOICE; i++) if (TSEL->v[i].active) return ppg_voice_g(TSEL, &TSEL->v[i], g); return 0; }
static ppg_voice_t *voice0(void) { return voiceg(0); }
static uint32_t s2g(uint32_t g) { ppg_voice_t *s = voiceg(g); return s ? s->s2 : 0u; }   /* group g sounding on the first key */
static voice_t *key_of(uint32_t note) { for (uint32_t i = 0; i < NVOICE; i++) if (TSEL->v[i].active && TSEL->v[i].note == note) return &TSEL->v[i]; return 0; }
static uint32_t held_of(uint32_t note)                 /* the 2.2 voices a sounding key holds (a mask) */
{
    voice_t *v = key_of(note);
    return v ? ppg_held(ppg_part(0), TSEL, (uint32_t)(v - TSEL->v)) : 0u;
}
static void keyb(uint32_t k, uint32_t split) { ppg(); patch()->keyb = (uint8_t)k; patch()->split = (uint8_t)split; }
static float crossings(float seconds)
{
    int32_t out[CTL], prev = 0; uint32_t n = 0, blocks = (uint32_t)(seconds * FS / CTL);
    for (uint32_t b = 0; b < 40u; b++) block(out);
    for (uint32_t b = 0; b < blocks; b++) {
        block(out);
        for (uint32_t i = 0; i < CTL; i++) { n += prev < 0 && out[i] >= 0; prev = out[i]; }
    }
    return (float)n / seconds;
}
static double energy(uint32_t blocks)
{
    int32_t out[CTL]; double e = 0;
    for (uint32_t b = 0; b < blocks; b++) { block(out); for (uint32_t i = 0; i < CTL; i++) e += (double)out[i] * out[i]; }
    return e;
}

static void tables(void)
{
    ppg(); int32_t out[CTL]; block(out);
    ppg_part_t *P = ppg_part(0);
    ok("a part's state fits the arena's 16 KiB", sizeof(ppg_part_t) <= 16384u);
    ok("table 0 loads at the first block", P && P->table == 1u);
    /* every ROM table: its key slots hold the key waves, a slot between keys lies between them sample for sample */
    int keys = 1, blends = 1;
    uint8_t w[64][64];
    for (uint32_t t = 0; t < PPG_ROM_TABLES; t++) {
        ppg_build(w, t < 28u ? t : PG_UPPER);
        for (uint32_t k = PPG_KEY_AT[t]; k + 1u < PPG_KEY_AT[t + 1u]; k++) {
            uint32_t s0 = PPG_KEY_SLOT[k], s1 = PPG_KEY_SLOT[k + 1u];
            keys &= !memcmp(w[s0], PPG_WAVES[PPG_KEY_WAVE[k]], 64);
            for (uint32_t s = s0 + 1u; s < s1 && s < 60u; s++)
                for (uint32_t i = 0; i < 64u; i++) {
                    uint32_t a = PPG_WAVES[PPG_KEY_WAVE[k]][i], b = PPG_WAVES[PPG_KEY_WAVE[k + 1u]][i];
                    blends &= w[s][i] >= (a < b ? a : b) && w[s][i] <= (a > b ? a : b);
                }
        }
    }
    ok("every table keeps its key waves at their slots", keys);
    ok("the slots between keys are blends of them", blends);
    ppg_build(w, 0);
    int sq = 1; for (uint32_t i = 0; i < 64u; i++) sq &= w[62][i] == 255u;
    ok("slot 62 is the square (its half all high)", sq);
    ok("slot 63 is the saw (rising through its half)", w[63][0] < w[63][32] && w[63][32] < w[63][63]);
    ok("slot 60 is the triangle (up, then back)", w[60][31] > w[60][0] && w[60][31] > w[60][63]);
    ppg_build(w, 29u);
    ok("table 29 narrows the pulse slot by slot", w[0][0] == 255u && w[59][0] < 160u && w[59][63] == 255u);
    patch()->raw[PG_TABLE] = 5; block(out);
    ok("a new TABLE loads at the next block", P->table == 6u);
}

static void voice(void)
{
    int32_t out[CTL];
    ppg(); trk_note_on(TSEL, 69, 100);
    float hz = crossings(0.5f);
    ok("INIT plays A4 near 440 Hz (the saw)", hz > 430.0f && hz < 450.0f);
    ppg(); patch()->raw[PG_SEMI] = PG_SEMI0 + 12; trk_note_on(TSEL, 57, 100);
    hz = crossings(0.5f);
    ok("SEMI +12 plays an octave up", hz > 430.0f && hz < 450.0f);
    ppg(); trk_note_on(TSEL, 60, 100);
    double open = energy(200);
    ppg(); patch()->raw[PG_CUT] = 12; trk_note_on(TSEL, 60, 100);
    double shut = energy(200);
    ok("a low CUTOFF takes most of the saw away", shut < open * 0.2);
    ppg(); patch()->raw[PG_R2] = 0; trk_note_on(TSEL, 60, 100);
    for (uint32_t b = 0; b < 50u; b++) block(out);
    trk_note_off(TSEL, 60);
    for (uint32_t b = 0; b < 400u && live(); b++) block(out);
    ok("a short RELEASE ends the voice", live() == 0u);
    ppg(); patch()->raw[PG_A2] = 60; patch()->raw[PG_R2] = 40; trk_note_on(TSEL, 60, 100);
    for (uint32_t b = 0; b < 5u; b++) block(out);
    trk_note_off(TSEL, 60);
    ppg_voice_t *s = voice0(); float e0 = s ? s->e2 : 0.0f;
    for (uint32_t b = 0; b < 200u; b++) block(out);
    s = voice0();
    ok("ATTACK above 52 keeps swelling after the key is up", s && s->e2 > e0 && live() > 0u);
    ppg(); patch()->raw[PG_A2] = 20; patch()->raw[PG_R2] = 40; trk_note_on(TSEL, 60, 100);
    for (uint32_t b = 0; b < 3u; b++) block(out);
    trk_note_off(TSEL, 60);
    for (uint32_t b = 0; b < 3u; b++) block(out);
    s = voice0();
    ok("an ATTACK of 48 or less releases with the key", s && s->s2 == 4u);
}

/* a sweep: ENV 1 walks OSC's wave up; the playing wave changes only as the phase passes a half cycle */
static void sweep(void)
{
    int32_t out[CTL];
    ppg(); patch()->raw[PG_TABLE] = 0; patch()->raw[PG_OSC] = 0; patch()->raw[PG_E1W] = 63;
    patch()->raw[PG_A1] = 40; patch()->raw[PG_S1] = 63; trk_note_on(TSEL, 48, 100);
    uint32_t moves = 0, last = 255u;
    for (uint32_t b = 0; b < 3000u; b++) {             /* (about 2 s: the 2.2's attack 40 takes about one) */
        block(out);
        ppg_voice_t *s = voice0(); if (!s) break;
        moves += s->cur[0] != last && last != 255u;
        last = s->cur[0];
    }
    ok("ENV 1 sweeps OSC through the table", moves > 20u && last > 40u);
    /* the half-cycle rule itself: a pending wave is taken up as the phase crosses 0.5, not before */
    static const uint8_t A[64] = {0}, B[64] = {0};
    const uint8_t *w = A; uint32_t ph = 0x7FFFFF00u; float carry = 0.0f;
    ppg_osc(&ph, &w, B, 0x10u, &carry);
    int before = w == A;
    ppg_osc(&ph, &w, B, 0x100u, &carry);
    ok("a pending wave waits for the half cycle", before && w == B);
}

static void fields(void)
{
    ppg();
    ppg_edit_value(TSEL, PG_SEMI, -7);
    ok("SEMI keeps -7 offset and reads it back", patch()->raw[PG_SEMI] == PG_SEMI0 - 7 && ppg_get(TSEL, PG_SEMI) == -7);
    ppg_edit_value(TSEL, PG_E3AMT, 31);
    ok("AMOUNT +31 is the knob's 63", patch()->raw[PG_E3AMT] == 63u && ppg_get(TSEL, PG_E3AMT) == 31);
    ppg_edit_value(TSEL, PG_TABLE, 99);
    ok("TABLE stops at SAMPLE, the 2.2's 31", patch()->raw[PG_TABLE] == PG_SAMPLE);
    ok("the INIT patch is valid", ppg_patch_valid(patch()));
    ok("engines 26..28 and PPG's user namespace 30 stay reserved, PPG is 29",
       !eng_ok(26) && !eng_ok(27) && !eng_ok(28) && !eng_ok(USER_NATIVE_PPG) && eng_ok(ENGI_PPG) && NENGINES == 31);
    ok("PPG is the sixth engine one can pick, colour 5", ENGINE_ORDER[5] == ENGI_PPG && eng_tint(ENGI_PPG) == 5u);
}

/* the program: KEYB 0 plays the selected group, KEYB 1..3 both on each key (and fewer keys), 4..8 B below SPLIT and A
 * from it; a voice ends when both its groups have; the pages edit the selected group, TABLE both */
static void program(void)
{
    int32_t out[CTL];
    ppg(); patch()->sel = 1; trk_note_on(TSEL, 60, 100); block(out);
    ok("KEYB 0 plays the selected group alone", s2g(1) && !s2g(0));
    keyb(0, 0);
    for (uint32_t n = 0; n < 8u; n++) trk_note_on(TSEL, (uint8_t)(60 + n), 100);
    block(out);
    ok("KEYB 0: eight keys, a 2.2 voice each", live() == 8u && held_of(60) == 0x01u && held_of(67) == 0x80u);
    keyb(1, 0); trk_note_on(TSEL, 60, 100); block(out);
    ok("KEYB 1 plays both groups on a key, a pair of voices", s2g(0) && s2g(1) && held_of(60) == 0x03u);
    for (uint32_t n = 0; n < 6u; n++) trk_note_on(TSEL, (uint8_t)(64 + n), 100);
    block(out);
    ok("KEYB 1 holds four keys, the oldest given up, the pairs in turn", live() == 4u && !key_of(60) && !key_of(64) && held_of(69) == 0x30u);
    keyb(2, 0);
    for (uint32_t n = 0; n < 3u; n++) trk_note_on(TSEL, (uint8_t)(60 + n), 100);
    block(out);
    ok("KEYB 2: two keys, four voices each (two A, two B)", live() == 2u && held_of(61) == 0xF0u && held_of(62) == 0x0Fu);
    keyb(3, 0); trk_note_on(TSEL, 60, 100); block(out); trk_note_on(TSEL, 64, 100); block(out);
    ok("KEYB 3: one key takes all eight, the newest", live() == 1u && held_of(64) == 0xFFu);
    keyb(3, 0); patch()->raw[PG_A2] = 0; patch()->rawb[PG_A2] = 0; trk_note_on(TSEL, 60, 100);
    for (uint32_t b = 0; b < 60u; b++) block(out);
    int16_t was = ppg_part(0)->hw[0].lv[1];
    trk_note_on(TSEL, 64, 100);
    ok("a voice taken by a new key goes on from where its envelope is", was > 100 && ppg_part(0)->hw[0].lv[1] == was &&
       ppg_part(0)->hw[0].s2 == 1u && live() == 1u);
    ppg_part_t *P = ppg_part(0);
    ok("KEYB 3: even voices play A, odd ones B", P->grp[0] == 0u && P->grp[1] == 1u && P->grp[6] == 0u && P->grp[7] == 1u);
    keyb(4, 24); trk_note_on(TSEL, 59, 100); block(out);
    ok("KEYB 4: below SPLIT plays B", s2g(1) && !s2g(0) && held_of(59) == 0x02u);
    keyb(4, 24); trk_note_on(TSEL, 60, 100); block(out);
    ok("KEYB 4: from SPLIT plays A", s2g(0) && !s2g(1) && held_of(60) == 0x01u);
    /* (the host's budget is 16 units: a split with a monophonic side claims more than it renders, so these hold one
     * B key fewer than the 2.2; the device's, with its second core, has the room) */
    keyb(5, 24); trk_note_on(TSEL, 70, 100);
    for (uint32_t n = 0; n < 3u; n++) trk_note_on(TSEL, (uint8_t)(40 + n), 100);
    trk_note_on(TSEL, 72, 100); block(out);
    ok("KEYB 5: A one key of four voices above (the newest), B a voice a key below",
       live() == 4u && !key_of(70) && held_of(72) == 0x55u && held_of(40) == 0x02u && held_of(42) == 0x20u);
    keyb(6, 24); trk_note_on(TSEL, 72, 100);
    for (uint32_t n = 0; n < 5u; n++) trk_note_on(TSEL, (uint8_t)(40 + n), 100);
    block(out);
    ok("KEYB 6: A one key of voices 0 and 2, B the other six", live() == 6u && held_of(72) == 0x05u && held_of(42) == 0x10u &&
       held_of(44) == 0x40u);
    keyb(7, 24); trk_note_on(TSEL, 40, 100); trk_note_on(TSEL, 41, 100); block(out);
    ok("KEYB 7: B one key of voices 1 and 3", live() == 1u && held_of(41) == 0x0Au);
    keyb(8, 24); trk_note_on(TSEL, 40, 100); trk_note_on(TSEL, 72, 100); block(out);
    ok("KEYB 8: one key a side, four voices each", live() == 2u && held_of(40) == 0xAAu && held_of(72) == 0x55u);
    /* each voice its own SEMIT: a chord on one key, as 97 and 98 play */
    keyb(3, 0);
    patch()->raw[PG_SEMI3] = PG_SEMI0 + 4; patch()->raw[PG_SEMI5] = PG_SEMI0 + 7; patch()->rawb[PG_SEMI2] = PG_SEMI0 + 12;
    trk_note_on(TSEL, 57, 100); block(out);
    P = ppg_part(0);
    ok("each 2.2 voice takes its own group's SEMIT", P->r[0].semi[2] == 4 && P->r[0].semi[4] == 7 && P->r[1].semi[1] == 12 &&
       P->r[0].semi[1] == 0 && P->r[1].semi[2] == 0);
    ppg(); patch()->raw[PG_SEMI4] = PG_SEMI0 + 5; ppg_edit_value(TSEL, PG_SEMI, 2);
    ok("SEMI on TUNE moves every voice together", patch()->raw[PG_SEMI] == PG_SEMI0 + 2u && patch()->raw[PG_SEMI4] == PG_SEMI0 + 7u &&
       patch()->raw[PG_SEMI8] == PG_SEMI0 + 2u);
    ppg_edit_value(TSEL, PGX_V1, -3);
    ok("V1 moves voice 1 alone", patch()->raw[PG_SEMI] == PG_SEMI0 - 3u && patch()->raw[PG_SEMI4] == PG_SEMI0 + 7u && ppg_get(TSEL, PGX_V1) == -3);
    ppg(); patch()->keyb = 1; patch()->raw[PG_R2] = 0; patch()->rawb[PG_R2] = 16;
    trk_note_on(TSEL, 60, 100);
    for (uint32_t b = 0; b < 100u; b++) block(out);
    trk_note_off(TSEL, 60);
    for (uint32_t b = 0; b < 40u; b++) block(out);
    int held = live() == 1u && !s2g(0) && s2g(1);
    for (uint32_t b = 0; b < 4000u && live(); b++) block(out);
    ok("a layered voice lasts until its longer group's release ends", held && !live());
    ppg(); ppg_edit_value(TSEL, PGX_SEL, 1); ppg_edit_value(TSEL, PG_CUT, 7); ppg_edit_value(TSEL, PG_TABLE, 9);
    ok("the pages edit the selected group", patch()->rawb[PG_CUT] == 7u && patch()->raw[PG_CUT] == 63u && ppg_get(TSEL, PG_CUT) == 7);
    ok("TABLE is both groups'", patch()->raw[PG_TABLE] == 9u && patch()->rawb[PG_TABLE] == 9u);
    ppg_edit_value(TSEL, PGX_SPLIT, 48); ppg_edit_value(TSEL, PGX_KEYB, 9);
    ok("SPLIT shows as a key, KEYB stops at 8", patch()->split == 12u && ppg_get(TSEL, PGX_SPLIT) == 48 && patch()->keyb == 8u);
}

/* the factory programs (with the local bank): all 100 in their own places, in range, loaded as stored, and heard */
static void factory(void)
{
#if PPG_FACTORY_N
    int valid = 1, loads = 1, heard = 1, named = 1, placed = 1, held = 1; uint32_t layered = 0, samples = 0;
    for (uint32_t k = 0; k < PPG_FACTORY_N; k++) {
        ppg_patch_t p; ppg_tone(k, &p);
        valid &= ppg_patch_valid(&p);
        loads &= !memcmp(p.raw, PPG_FACTORY[k].raw, PG_RAW) && !memcmp(p.rawb, PPG_FACTORY[k].rawb, PG_RAW)
                 && p.keyb == PPG_FACTORY[k].keyb && p.split == PPG_FACTORY[k].split && p.sel == PPG_FACTORY[k].sel;
        const char *n = PPG_FACTORY[k].name;         /* "17 PIANO": the program, recalled with A */
        named &= n[2] == ' ' && p.sel == 0u;
        placed &= (uint32_t)(n[0] - '0') * 10u + (uint32_t)(n[1] - '0') == k;
        layered += p.keyb >= 1u && p.keyb <= 3u;
        if (p.raw[PG_TABLE] == PG_SAMPLE) { samples++; held &= k == 10u || k == 40u || k == 59u || (k >= 90u && k <= 92u); }
    }
    for (uint32_t k = 1; k < PPG_FACTORY_N; k += 23u) {   /* a spread of them, each played for a moment */
        int32_t out[CTL];
        ppg(); host_preset(TSEL, ENGI_PPG, k); trk_note_on(TSEL, 60, 100);
        int32_t peak = 0;
        for (uint32_t b = 0; b < 300u; b++) { int32_t pk = block(out); if (pk > peak) peak = pk; }
        heard &= peak > 0;
    }
    ok("the engine offers the 100 programs, then INIT", PPG_FACTORY_N == 100u && ENG_PPG.npresets == 101u);
    char name[21]; preset_t pr = {0}; ppg_preset_make(PPG_INIT, &pr, name);
    ok("INIT comes after program 99", str_eq(name, "INIT WAVE"));
    ok("preset k is program k", placed);
    ok("every factory preset is in the patch's ranges", valid);
    ok("a factory preset loads as stored", loads);
    ok("factory presets sound", heard);
    ok("a preset is a program, named for it and recalled with group A", named);
    ok("the 25 layered programs are whole", layered == 25u);
    ok("the six sampled programs keep their places, on TABLE 31", samples == 6u && held);
#if PPG_HAVE_SAMPLE
    {   /* TABLE 31: the natural sounds over the wave memory, the run from OSC to the loop at ENV 1 WAVES x 2 + 3 */
        int32_t out[CTL], peak = 0;
        ppg(); host_preset(TSEL, ENGI_PPG, 10); trk_note_on(TSEL, 57, 100); block(out);
        ppg_part_t *P = ppg_part(0);
        int mem = !memcmp(&P->wave[0][0], PPG_SAMPLE, PPG_SAMPLE_BYTES);
        uint8_t up[64][64]; ppg_build(up, PG_UPPER);
        int keep = !memcmp(P->wave[124], up[60], 4u * 64u);
        uint32_t start = voice0()->cur[0];
        for (uint32_t b = 0; b < 2000u; b++) { int32_t pk = block(out); if (pk > peak) peak = pk; }
        ok("TABLE 31 loads the piano and the saxophone, the standard waves kept after them", mem && keep);
        ok("10 PIANO (FLUEGEL) starts at OSC's stretch and ends looping ENV 1 WAVES x 2 + 3",
           start == 2u && voice0()->cur[0] == 27u && P->r[0].loop == 27u && peak > 0);
        ppg(); host_preset(TSEL, ENGI_PPG, 40); trk_note_on(TSEL, 57, 100); block(out);
        ok("40 SAX starts at stretch 31 (the piano's last, then the saxophone's)", voice0()->cur[0] == 31u);
        ppg(); host_preset(TSEL, ENGI_PPG, 91); trk_note_on(TSEL, 34, 100);   /* A#1: a cycle a stretch, slowly */
        int held = 1;
        for (uint32_t b = 0; b < 1600u; b++) { block(out); held &= voice0()->cur[0] == 38u; }
        ok("91's group A (start 38, loop 23) holds its start: no run round the memory through the standard waves", held);
        ppg(); host_preset(TSEL, ENGI_PPG, 17); block(out);
        ok("leaving TABLE 31 builds the upper table again", !memcmp(ppg_part(0)->wave[64], up[0], sizeof up));
    }
#endif
    ok("97 keeps its chord (per-voice SEMIT)",
       PPG_FACTORY[97].keyb == 3u && PPG_FACTORY[97].raw[PG_SEMI3] != PPG_FACTORY[97].raw[PG_SEMI]);
#else
    printf("ppg: no factory bank: factory checks skipped\n");
#endif
}

int main(void)
{
    tables();
    voice();
    sweep();
    fields();
    program();
    factory();
    if (failures) { printf("PPG TESTS FAILED: %d\n", failures); return 1; }
    printf("ALL PPG TESTS PASSED\n");
    return 0;
}
#endif
