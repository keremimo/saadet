/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Two parts: one part costs and changes nothing; MULTI, SPLIT and LAYER reach the parts from the keys and MIDI and end what
 * they started; neither part starves the other; the state and performances keep both; an older state and backup load. */
#include <assert.h>
#define MELODEE_FLASH 1
#define MELODEE_USB_AUDIO 0
#include "synth_harness.c"
#define SXQ 1024u
static uint32_t so_w, so_r, wire_n;
static uint8_t wire[2048];
static void ota_wire_send(const uint8_t *p, uint32_t n) { assert(n <= sizeof wire); memcpy(wire, p, n); wire_n = n; }
#define ED_BK_FLASH_PTR(off) (nor + (off))
static int ota_frame_get(const uint8_t **p, uint32_t *n) { (void)p; (void)n; return 0; }
static void ota_frame_done(void) {}
#include "../firmware/src/editor.c"

static int bad;
static void ok(const char *what, int pass) { printf("parts: %s: %s\n", what, pass ? "ok" : "FAIL"); bad += !pass; }
static void tick(void) { int32_t out[CTL * 2]; mix_block(out, CTL); }
static void ticks(uint32_t n) { while (n--) tick(); }
static void msg(uint32_t st, uint32_t ch, uint32_t a, uint32_t b) { midi_event(st, ch, a, b); }
static uint32_t held(uint32_t p)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < NVOICE; i++)
        n += trk[p].v[i].active && trk[p].v[i].gate;
    return n;
}
static int plays(uint32_t p, uint32_t note)
{
    for (uint32_t i = 0; i < NVOICE; i++)
        if (trk[p].v[i].active && trk[p].v[i].gate && trk[p].v[i].note == note)
            return 1;
    return 0;
}
static uint32_t voices(uint32_t p)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < NVOICE; i++)
        n += trk[p].v[i].active && trk[p].v[i].stage != 4u;
    return n;
}
static void start(uint32_t mode)
{
    power_on();
    host_preset(&trk[0], ENGI_J106, 1);
    host_preset(&trk[1], ENGI_J106, 2);
    song.g[G_PMODE] = (int16_t)mode;
    ticks(8);
    panic_req = 0;
}
static void off_all(void) { panic_req |= 3u; ticks(600); }

static void one_part(void)
{
    start(PT_ONE);
    ok("one part: part B not in use", trk_n() == 1u);
    msg(0x90, 0, 60, 100); msg(0x90, 9, 64, 100); tick();
    ok("every channel plays part A", plays(0, 60) && plays(0, 64) && !voices(1));
    msg(0x80, 0, 60, 0); msg(0x80, 9, 64, 0); key_down(7); tick();
    ok("the keys play part A", held(0) == 1u && !voices(1));
    key_up(7);
    ok("part B holds no memory", !resource[RES_ENGINE0 + 1u].size);
    track_select(1);
    ok("part B cannot be selected", song.sel == 0u);
    song.g[G_ROUTE] = 3;
    off_all();
    msg(0x90, 0, 60, 100); msg(0x90, 2, 62, 100); tick();
    ok("IN CH still filters", !plays(0, 60) && plays(0, 62));
    song.g[G_ROUTE] = 0;
}

static void multi(void)
{
    start(PT_MULTI);
    msg(0x90, 0, 60, 100); msg(0x90, 1, 72, 100); msg(0x90, 5, 65, 100); tick();
    ok("MULTI: part B on its channel, part A on the others", plays(0, 60) && plays(1, 72) && plays(0, 65) && !plays(1, 60));
    msg(0x80, 0, 60, 0); msg(0x80, 1, 72, 0); msg(0x80, 5, 65, 0); tick();
    ok(".. each note-off ends its own", !held(0) && !held(1));
    song.g[G_ROUTE] = 1;
    msg(0x90, 5, 65, 100); msg(0x90, 0, 61, 100); tick();
    ok(".. IN CH narrows part A", !plays(0, 65) && plays(0, 61));
    msg(0x80, 0, 61, 0);
    song.g[G_ROUTE] = 0;
    msg(0xB0, 1, 1, 90); msg(0xB0, 0, 1, 20);
    ok(".. a channel's mod wheel is its part's", trk[1].mw == 90u && trk[0].mw == 20u);
    msg(0xE0, 1, 0, 127);
    ok(".. a channel's bend is its part's", trk[1].bend_raw > 8000 && trk[0].bend_raw == 0);
    msg(0xE0, 1, 0, 64);
    msg(0xB0, 1, 64, 127); msg(0x90, 1, 70, 100); msg(0x80, 1, 70, 0); tick();
    ok(".. the pedal keeps its channel's note", plays(1, 70));
    msg(0xB0, 1, 64, 0); tick();
    ok(".. and lets it go", !plays(1, 70));
    track_select(1); key_down(7); tick();
    ok(".. the keys play the selected part", held(1) == 1u && !held(0));
    track_select(0); key_up(7); tick();
    ok(".. a key's release follows it, wherever the selection went", !held(1));
    ok(".. part B's keys go out on its channel", trk_midi_ch(1) == 1u && trk_midi_ch(0) == (uint32_t)song.g[G_DRUMCH] - 1u);
}

static void split(void)
{
    start(PT_SPLIT);
    song.g[G_PSPLIT] = 60;
    msg(0x90, 0, 59, 100); msg(0x90, 0, 60, 100); tick();
    ok("SPLIT: below the point part B, from it part A", plays(1, 59) && plays(0, 60) && !plays(0, 59) && !plays(1, 60));
    msg(0x80, 0, 59, 0); msg(0x80, 0, 60, 0); tick();
    ok(".. their note-offs find them", !held(0) && !held(1));
    song.octave = 1;                               /* key 0 is F3: an octave up it sounds F4 and stays below the split */
    key_down(0); key_down(7); tick();
    ok(".. the keys split where they are, whatever the octave", plays(1, 65) && plays(0, 72) && held(0) == 1u && held(1) == 1u);
    key_up(0); key_up(7); tick();
    ok(".. and end what they started", !held(0) && !held(1));
    song.octave = 0;
    key_down(0); song.g[G_PSPLIT] = 40; key_up(0); tick();
    ok(".. a split moved under a held key leaves nothing hanging", !held(0) && !held(1));
    song.g[G_PSPLIT] = 60;
    msg(0xB0, 0, 1, 77);
    ok(".. the mod wheel reaches both", trk[0].mw == 77u && trk[1].mw == 77u);
    msg(0x90, 0, 50, 100); song.g[G_PSPLIT] = 40; msg(0x80, 0, 50, 0); tick();
    ok(".. a note-off finds its part after the split moved", !plays(1, 50) && !plays(0, 50));
}

static void layer(void)
{
    start(PT_LAYER);
    key_down(7); tick();
    ok("LAYER: a key plays both", plays(0, 60) && plays(1, 60));
    key_up(7); tick();
    ok(".. its release ends both", !held(0) && !held(1));
    msg(0x90, 0, 64, 100); tick();
    ok(".. a MIDI note plays both", plays(0, 64) && plays(1, 64));
    msg(0x90, 0, 64, 90); tick();
    ok(".. a repeat restarts on both", plays(0, 64) && plays(1, 64) && held(0) == 1u && held(1) == 1u);
    msg(0x80, 0, 64, 0); tick();
    ok(".. one note-off ends both", !held(0) && !held(1));
    msg(0xB0, 0, 64, 127); msg(0x90, 0, 65, 100); msg(0x80, 0, 65, 0); tick();
    ok(".. the pedal keeps both", plays(0, 65) && plays(1, 65));
    msg(0xB0, 0, 64, 0); tick();
    ok(".. and lets both go", !held(0) && !held(1));
    trk[1].p[P_CHRD] = CH_MAJ;
    trk[0].p[P_TRANS] = 12;
    key_down(7); tick();
    ok(".. each part builds its own notes from one key", plays(0, 72) && held(0) == 1u && plays(1, 60) && plays(1, 64) && plays(1, 67));
    key_up(7); tick();
    ok(".. and one release ends them all", !held(0) && !held(1));
    trk[1].p[P_CHRD] = CH_OFF;
    trk[0].p[P_TRANS] = 0;
    msg(0x90, 0, 70, 100); tick();
    msg(0xB0, 0, 120, 0); tick();
    ok(".. All Sound Off stops both", !voices(0) && !voices(1));
    trk[0].p[P_AMODE] = trk[1].p[P_AMODE] = 1;
    key_down(7); ticks(4);
    ok(".. each part's arp runs", trk[0].arp_note == 60u && trk[1].arp_note == 60u);
    key_up(7);
    trk[0].p[P_AMODE] = trk[1].p[P_AMODE] = 0;
    ticks(8);
    ok(".. and stops", !trk[0].arp_note && !trk[1].arp_note);
}

static void chord(uint32_t ch, uint32_t root, uint32_t n)
{
    static const uint8_t IV[8] = {0, 2, 4, 5, 7, 9, 11, 12};
    for (uint32_t i = 0; i < n; i++)
        msg(0x90, ch, root + IV[i], 100);
    ticks(VOICE_FADE_BLOCKS + 2u);                   /* the notes that took a voice start when it has faded */
}
static void fair(void)
{
    uint32_t share;
    start(PT_MULTI);
    share = voice_budget() / 2u / voice_units(&trk[0]);
    chord(0, 48, 6);
    ok("fair share: alone a part borrows what the other is not using", voices(0) * voice_units(&trk[0]) > voice_budget() / 2u);
    chord(1, 72, 6);
    ok(".. the other gets its share back", voices(1) >= share && voices(0) >= share);
    ok(".. within the budget", voices_busy() <= voice_budget());
    chord(0, 49, 6);
    ok(".. a part over its share takes only from itself", voices(1) >= share);
    off_all();
    host_preset(&trk[0], ENGI_PROPHET, 1); host_preset(&trk[1], ENGI_PROPHET, 2); ticks(8);
    share = voice_budget() / 2u / voice_units(&trk[0]);
    chord(0, 48, 8); chord(1, 72, 8);
    ok(".. a Prophet part keeps more than its bass", voices(0) >= share && voices(1) >= share && share >= 2u);
    off_all();
    host_preset(&trk[0], ENGI_FM6, 1); host_preset(&trk[1], ENGI_J106, 2); ticks(8);
    chord(0, 48, 8); chord(1, 72, 6);
    ok(".. mixed weights share by budget, not by voice",
       part_busy(&trk[1]) >= voice_budget() / 2u - voice_units(&trk[1]) + 1u && voices_busy() <= voice_budget());
    off_all();
    msg(0xB0, 0, 121, 0);
}

static void mode_change(void)
{
    start(PT_LAYER);
    track_select(1);
    key_down(7); msg(0x90, 0, 72, 100); ticks(3);
    ok("mode change: both parts sound before it", held(0) && held(1) && resource[RES_ENGINE0 + 1u].size);
    song.g[G_PMODE] = PT_ONE; ticks(2);
    ok(".. one part: part B is silent and gives its memory back", !voices(1) && !resource[RES_ENGINE0 + 1u].size);
    ok(".. part A is selected", song.sel == 0u);
    ok(".. nothing is held on", !held(0));
    key_up(7); msg(0x80, 0, 72, 0); ticks(2);
    ok(".. the release afterwards changes nothing", !held(0) && !held(1));
    song.g[G_PMODE] = PT_MULTI; ticks(2);
    msg(0x90, 1, 60, 100); tick();
    ok(".. back on, part B sounds", plays(1, 60));
    msg(0x80, 1, 60, 0);
}

static void state(void)
{
    memset(nor, 255, sizeof nor);
    start(PT_SPLIT);
    song.g[G_PSPLIT] = 55; song.g[G_PCH2] = 9; track_select(1);
    trk[1].p[P_LEVEL] = 77; trk[1].p[P_PAN] = -20;
    j106_patch_t tone = *j106_patch_of(&trk[1]);
    assert(state_autosave());
    power_on(); syn_saved = 0; state_boot();
    ok("state: mode, split, channel, selection", parts_mode() == PT_SPLIT && song.g[G_PSPLIT] == 55 && song.g[G_PCH2] == 9 && song.sel == 1u);
    ok(".. part B's sound and mix", trk[1].eng_req == ENGI_J106 && trk[1].p[P_LEVEL] == 77 && trk[1].p[P_PAN] == -20 &&
       !memcmp(j106_patch_of(&trk[1]), &tone, sizeof tone));
    {   /* a state from before the parts: its sound becomes part A */
        static synth_state1_t o;
        synth_state_t s, up;
        const synth_part_t *a = &s.part[0];
        power_on();
        host_preset(&trk[0], ENGI_FM6, 3);
        state_capture(&s);
        memset(&o, 0, sizeof o);
        o.magic = SYN_MAGIC1; o.size = sizeof o;
        memcpy(o.g, s.g, sizeof o.g);
        o.g[G_PMODE] = o.g[G_PCH2] = o.g[G_PSPLIT] = 0;        /* (reserved then: zeros) */
        o.engine = a->engine; o.preset = a->preset; o.user = a->user;
        memcpy(o.p, a->p, sizeof o.p); memcpy(o.fm6, a->fm6, sizeof o.fm6); memcpy(o.fm6_fn, a->fm6_fn, sizeof o.fm6_fn);
        o.cz = a->cz; o.p5 = a->p5; o.j106 = a->j106;
        o.sum = syn_hash(&o, sizeof o - 4u);
        memcpy(&up, &o, sizeof o);
        ok(".. an older state upgrades", state_upgrade(&up, sizeof o) && state_valid(&up));
        ok(".. to one part, its sound part A", up.part[0].engine == ENGI_FM6 && up.g[G_PMODE] == PT_ONE && up.g[G_PSPLIT] == 60 && up.g[G_PCH2] == 2);
        assert(!st_save(OBJ_SYNTH, &o, sizeof o));
        power_on(); syn_saved = 0; state_boot();
        ok(".. and boots", trk[0].eng_req == ENGI_FM6 && parts_mode() == PT_ONE && song.sel == 0u);
        memcpy(ED_BK_RAW, &o, sizeof o); ed_bk_id = 32; ed_bk_len = ed_bk_pos = sizeof o; ed_bk_crc = st_crc32(ED_BK_RAW, ed_bk_len);
        host_preset(&trk[0], ENGI_J106, 4);
        ok(".. and an older backup restores", !ed_bk_commit_inner() && trk[0].eng_req == ENGI_FM6);
    }
}

static void performances(void)
{
    memset(nor, 255, sizeof nor);
    power_on(); persist_boot();
    ok("performances: none yet", !perf_used(0) && !perf_used(PERF_SLOTS - 1u));
    host_preset(&trk[0], ENGI_FM6, 2); host_preset(&trk[1], ENGI_J106, 3);
    song.g[G_PMODE] = PT_SPLIT; song.g[G_PSPLIT] = 57; song.g[G_PCH2] = 5; track_select(1);
    trk[1].p[P_LEVEL] = 80;
    ok("stored", !perf_store(3, "BASS PAD") && perf_used(3));
    char n[PERF_NAME];
    perf_name(3, n);
    ok(".. under its name", str_eq(n, "BASS PAD") && perf_engine(3) == ENGI_FM6);
    host_preset(&trk[0], ENGI_CZ, 1); host_preset(&trk[1], ENGI_PROPHET, 1);
    assert(!perf_store(2, "NEIGHBOUR"));           /* (slots 2 and 3 share a flash object) */
    power_on(); persist_boot();
    ok(".. the neighbour kept beside it, across a power cycle", perf_used(2) && perf_used(3));
    perf_name(3, n);
    ok(".. the name back", str_eq(n, "BASS PAD") && !perf_used(4));
    uint32_t before = up_gen;
    msg(0x90, 0, 60, 100); tick();
    ok("loaded", !perf_load(3));
    ok(".. the mode, the split, the channel, the selection",
       parts_mode() == PT_SPLIT && song.g[G_PSPLIT] == 57 && song.g[G_PCH2] == 5 && song.sel == 1u);
    ok(".. both sounds", trk[0].eng_req == ENGI_FM6 && trk[1].eng_req == ENGI_J106 && trk[1].p[P_LEVEL] == 80);
    ticks(16);
    ok(".. what sounded ended", !held(0) && !held(1));
    msg(0x90, 0, 40, 100); msg(0x90, 0, 70, 100); tick();
    ok(".. it plays", plays(1, 40) && plays(0, 70));
    msg(0x80, 0, 40, 0); msg(0x80, 0, 70, 0);
    song.playing = 1;
    ok("refused while the transport runs", perf_store(0, "X") == 2 && !perf_used(0) && perf_delete(3) == 2 && perf_used(3));
    song.playing = 0;
    ok("deleted", !perf_delete(3) && !perf_used(3) && perf_used(2) && up_gen != before);
    ok(".. an empty slot does not load", perf_load(3) != 0);
    {   /* the backup path: only valid banks go in */
        perf_bank_t *b = (perf_bank_t *)ED_BK_RAW;
        assert(!perf_store(0, "BACKED UP"));
        uint32_t len;
        const uint8_t *p = ed_bk_capture() ? 0 : ed_bk_object(ED_BK_PERF0, &len);
        ok("backup: the object reads", p && len == sizeof(perf_bank_t) && ed_bk_supported(ED_BK_PERF0));
        st_read_range(OBJ_PERF0, (uint32_t)ed_bk_source_copy, 0, ED_BK_RAW, sizeof *b);
        ed_bk_id = ED_BK_PERF0; ed_bk_len = ed_bk_pos = sizeof *b;
        b->r[0].part[1].engine = ENGI_DRUM;
        ed_bk_crc = st_crc32(ED_BK_RAW, ed_bk_len);
        ok(".. a bad sound is refused", ed_bk_commit_inner() == 2u);
        b->r[0].part[1].engine = ENGI_J106;
        str_cpy(b->r[0].name, "RESTORED", PERF_NAME);
        ed_bk_crc = st_crc32(ED_BK_RAW, ed_bk_len);
        ok(".. a good one commits", !ed_bk_commit_inner());
        perf_name(0, n);
        ok(".. and the library reads it", str_eq(n, "RESTORED"));
        memset(ED_BK_RAW, 0, 16); ed_bk_id = ED_BK_PERF0 + 1u; ed_bk_len = ed_bk_pos = 16; ed_bk_crc = st_crc32(ED_BK_RAW, 16);
        ok(".. a wrong size is refused", ed_bk_commit_inner() == 2u);
    }
    uint32_t first = st_sector(OBJ_PERF0, 0), last = st_sector(OBJ_PERF0 + PERF_OBJS - 1u, 1);
    ok("the flash map: inside the data window, clear of the last state", first > st_sector(OBJ_SYNTH, 1) && last + ST_SECTOR <= 0xC8000u);
}

/* a whole render of two parts with sanitizers on: nothing outside its buffers */
static void render(void)
{
    static const uint8_t ENG[4] = {ENGI_PROPHET, ENGI_FM6, ENGI_CZ, ENGI_J106};
    for (uint32_t a = 0; a < 4u; a++)
        for (uint32_t b = 0; b < 4u; b++) {
            int32_t out[CTL * 2];
            int64_t peak = 0;
            start(PT_LAYER);
            host_preset(&trk[0], ENG[a], 1); host_preset(&trk[1], ENG[b], 1);
            ticks(8);
            msg(0x90, 0, 60, 110); msg(0x90, 0, 64, 110);
            for (uint32_t i = 0; i < 800u; i++) {
                mix_block(out, CTL);
                for (uint32_t k = 0; k < 2u * CTL; k++) {
                    int64_t x = out[k] < 0 ? -(int64_t)out[k] : out[k];
                    if (x > peak) peak = x;
                }
            }
            if (!peak || resource_failures) {
                printf("  %s + %s: peak %lld, resource failures %u\n", ENGINES[ENG[a]]->name, ENGINES[ENG[b]]->name, (long long)peak, (unsigned)resource_failures);
                bad++;
            }
            off_all();
        }
    ok("every pair of engines renders together, within the arena", !resource_failures);
}

int main(void)
{
    one_part();
    multi();
    split();
    layer();
    fair();
    mode_change();
    state();
    performances();
    render();
    printf(bad ? "PARTS TEST FAILED (%d)\n" : "PARTS TEST PASSED\n", bad);
    return bad != 0;
}
