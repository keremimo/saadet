/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* PPG and the editor (SAADET_PPG): a program in a user slot stores and loads whole (the panel's SAVE, commands 78 and 80),
 * command 102 reads, plays, edits and names a part's program and refuses what leaves its ranges, INFO announces it, PUSH
 * sees its edits, and its user namespace takes favourites. Needs the local wave image; without it, says so and passes. */
#include "ppg_rom.h"
#if !PPG_HAVE_ROM
#include <stdio.h>
int main(void) { printf("ppg editor: no PPG wave image (private/ppg/ppg22-waves.bin): skipped\n"); return 0; }
#else
#define SAADET_PPG 1
#include <assert.h>
#define MELODEE_FLASH 1
#define MELODEE_USB_AUDIO 0
#include "synth_harness.c"
#define SXQ 1024u
static uint32_t so_w, so_r, wire_n;
static uint8_t wire[2048];
static int ota_wire_send(const uint8_t *p, uint32_t n) { assert(n <= sizeof wire); memcpy(wire, p, n); wire_n = n; return 0; }
#define ED_BK_FLASH_PTR(off) (nor + (off))
static int ota_frame_get(const uint8_t **p, uint32_t *n) { (void)p; (void)n; return 0; }
static void ota_frame_done(void) {}
#include "../firmware/src/editor.c"

static int bad;
static void ok(const char *what, int pass) { printf("ppg editor: %s: %s\n", what, pass ? "ok" : "FAIL"); bad += !pass; }
static void request(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    uint8_t f[1024] = {0x7D, 0x46, 0x4C};
    f[3] = (uint8_t)cmd; if (n) memcpy(f + 4, a, n);
    wire_n = 0; ed_handle(f, n + 4);
}
static const uint8_t *reply(void) { return wire + 5; }   /* (after F0 7D 46 4C cmd) */
static track_t *ppg_part0(uint32_t preset)
{
    power_on();
    host_preset(&trk[0], ENGI_PPG, preset);
    trk[0].engine = trk[0].eng_req;
    return &trk[0];
}

static void slots(void)
{
    track_t *t = ppg_part0(50);                          /* 50 STRINGS 1: KEYB 1, both groups */
    ppg_patch_t want = *ppg_patch_of(t);
    ok("store a program in slot 300", !up_store_to(299, 0, "MY STRINGS") && up_used(299) && up_engine(299) == ENGI_PPG &&
       native_used(ENGI_PPG, 299));
    char name[13]; up_name(299, name);
    ok(".. under its name", !strcmp(name, "MY STRINGS"));
    host_preset(t, ENGI_FM6, 0);
    ok(".. and load it back whole", !up_load_to(299, 0) && t->eng_req == ENGI_PPG &&
       !memcmp(ppg_patch_of(t)->raw, want.raw, PG_RAW) && !memcmp(ppg_patch_of(t)->rawb, want.rawb, PG_RAW) &&
       ppg_patch_of(t)->keyb == want.keyb && !memcmp(ppg_patch_of(t)->name, "MY STRINGS  ", PG_NAME_LEN));
    /* command 78: a raw program into a slot, its name in the list */
    {
        static uint8_t put[3u + 2u * sizeof(ppg_patch_t)];
        ppg_patch_t p; ppg_tone(17, &p);
        const uint8_t *r = (const uint8_t *)&p;
        put[0] = ENGI_PPG; put[1] = 2; put[2] = 10;
        for (uint32_t i = 0; i < sizeof p; i++) { put[3u + 2u * i] = r[i] & 15u; put[4u + 2u * i] = r[i] >> 4; }
        request(ED_NATIVE, put, sizeof put);
        ok("78 PUT a program into slot 11", reply()[3] == 0 && native_used(ENGI_PPG, 10));
        uint8_t list[] = {ENGI_PPG, 0, 10, 1};
        request(ED_NATIVE, list, sizeof list);
        ok(".. and 78 lists it by its name", memmem(wire, wire_n, "17 PIANO", 8) != 0);
        p.keyb = 9;
        r = (const uint8_t *)&p;
        for (uint32_t i = 0; i < sizeof p; i++) { put[3u + 2u * i] = r[i] & 15u; put[4u + 2u * i] = r[i] >> 4; }
        put[2] = 11;
        request(ED_NATIVE, put, sizeof put);
        ok(".. refuses one past its ranges", reply()[3] != 0 && !native_used(ENGI_PPG, 11));
    }
    /* command 80: the slot is native kind 6 */
    {
        uint8_t get[] = {1, 299u & 127u, 299u >> 7};
        request(ED_LIBRARY, get, sizeof get);
        uint32_t at = 5u + 4u + 1u + 1u + (uint32_t)strlen("MY STRINGS") + 1u + 2u * P_COUNT + 32u;   /* (its name 0-ended) */
        ok("80 reads the slot as a PPG program (kind 6, 115 bytes)", wire_n > at + 3u && wire[at] == 6u && wire[at + 1u] == 115u);
    }
    /* favourites: the user namespace */
    ok("its user namespace takes favourites", prefs_capacity(USER_NATIVE_PPG) == UP_SLOTS && prefs_native_engine(USER_NATIVE_PPG) == ENGI_PPG);
    up_put(299, 0); up_put(10, 0);
}

static void command(void)
{
    track_t *t = ppg_part0(17);
    const uint8_t *raw = (const uint8_t *)ppg_patch_of(t);
    uint8_t get[] = {0, 0};
    request(ED_PPG, get, sizeof get);
    int same = wire_n == 5u + 3u + 2u * sizeof(ppg_patch_t) + 1u && reply()[0] == 1 && reply()[1] == 0;
    for (uint32_t i = 0; same && i < sizeof(ppg_patch_t); i++) same = (reply()[3u + 2u * i] | reply()[4u + 2u * i] << 4) == raw[i];
    ok("102 GET: the program as nibble pairs", same);
    {
        static uint8_t put[2u + 2u * sizeof(ppg_patch_t)];
        ppg_patch_t p; ppg_tone(97, &p);
        const uint8_t *r = (const uint8_t *)&p;
        put[0] = 1; put[1] = 0;
        for (uint32_t i = 0; i < sizeof p; i++) { put[2u + 2u * i] = r[i] & 15u; put[3u + 2u * i] = r[i] >> 4; }
        host_preset(t, ENGI_FM6, 0);
        request(ED_PPG, put, sizeof put);
        ok("102 PUT plays a program in the part", reply()[1] == 0 && t->eng_req == ENGI_PPG && !memcmp(ppg_patch_of(t), &p, sizeof p));
        put[2u + 2u * PG_TABLE] = 0; put[3u + 2u * PG_TABLE] = 2;   /* TABLE 32 */
        request(ED_PPG, put, sizeof put);
        ok(".. refuses one past its ranges", reply()[1] != 0 && !memcmp(ppg_patch_of(t), &p, sizeof p));
    }
    {
        uint32_t h = ed_native_hash(0);
        uint8_t edit[] = {2, 0, PG_RAW + PG_CUT, 12, 0};
        request(ED_PPG, edit, sizeof edit);
        ok("102 edits a byte of group B", reply()[1] == 0 && ppg_patch_of(t)->rawb[PG_CUT] == 12u);
        ok(".. and PUSH sees the change", ed_native_hash(0) != h);
        uint8_t keyb[] = {2, 0, 2u * PG_RAW, 9, 0};
        request(ED_PPG, keyb, sizeof keyb);
        ok(".. refuses KEYB 9", reply()[1] != 0 && ppg_patch_of(t)->keyb == 3u);
        uint8_t semi[] = {2, 0, PG_SEMI4, 48, 0}, over[] = {2, 0, PG_SEMI4, 49, 0};
        request(ED_PPG, semi, sizeof semi);
        request(ED_PPG, over, sizeof over);
        ok(".. a voice's SEMIT to +24, not past it", ppg_patch_of(t)->raw[PG_SEMI4] == 48u);
        uint8_t name[] = {3, 0, 'C', 'H', 'O', 'R', 'D', 'S'};
        request(ED_PPG, name, sizeof name);
        ok("102 names the program", !memcmp(ppg_patch_of(t)->name, "CHORDS      ", PG_NAME_LEN));
        uint8_t namebyte[] = {2, 0, (uint8_t)PPG_NAME, 'X', 0}, send[] = {4, 0};
        request(ED_PPG, namebyte, sizeof namebyte);
        ok(".. the name only by op 3", reply()[1] != 0);
        request(ED_PPG, send, sizeof send);
        ok(".. and has no native message to send", reply()[1] != 0);
    }
    {
        int found = 0;
        request(ED_INFO, 0, 0);
        for (uint32_t i = 0; i + 6u < wire_n; i++)
            found |= wire[i] == 0x50u && wire[i + 1u] == 1u && wire[i + 2u] == ENGI_PPG && wire[i + 3u] == USER_NATIVE_PPG &&
                     wire[i + 4u] == ED_PPG && wire[i + 5u] == 115u && wire[i + 6u] == 0u;
        ok("INFO announces PPG: engine 29, namespace 30, command 102, 115 bytes", found);
    }
}

int main(void)
{
    slots();
    command();
    if (bad) { printf("PPG EDITOR TESTS FAILED: %d\n", bad); return 1; }
    printf("ALL PPG EDITOR TESTS PASSED\n");
    return 0;
}
#endif
