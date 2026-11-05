/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Host test of firmware/src/storage.c against a simulated NOR flash:
 * erase -> 0xFF, program can only clear bits, page writes must not wrap. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef SAADET_SMK37
/* the board's own windows: cc -DSAADET_SMK37=1 -Ibuild/fm1-base-smk37-platform/bsp/boards/smk37 */
#define FL_MAP_ONLY
#include "fm1_flash.h"
#endif
static uint8_t nor[0x100000];
static int fail_after = -1;            /* torn-write injection: stop after N programs */
static int fail_bytes = -1;            /* power loss after any programmed byte */
static int erase_error, write_protected;
static int pages_split;                /* the real fl_write splits pages: a compact object's writes may cross one */
static uint32_t fail_erase_off = 0xFFFFFFFFu;
static uint32_t io_calls;

static int st_read(uint32_t off, void *dst, uint32_t n) { io_calls++; memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off)
{
    io_calls++;
    if (erase_error || off == fail_erase_off) return -8;
    if (!write_protected) memset(nor + off, 0xFF, 4096);
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    io_calls++;
    if (fail_after == 0)
        return -9;
    if (fail_after > 0)
        fail_after--;
    if (!pages_split && (off & 0xFFu) + n > 256u) {
        printf("page wrap at %#x\n", off);
        exit(1);
    }
    for (i = 0; i < n; i++) {
        if (fail_bytes == 0) return -9;
        if (fail_bytes > 0) fail_bytes--;
        if (!write_protected) nor[off + i] &= s[i];
    }
    return 0;
}
#include "../firmware/src/storage.c"
#include "../platform/app.h"                     /* APP_STORE_MAP: the platform's pairs */
#define T_OBJ OBJ_CZBANK0                        /* (an ordinary one-sector object for the A/B tests) */

static int check(const char *what, int ok)
{
    printf("%-46s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

/* where a sector may lie: the board's data windows, the same test the firmware's flash calls make */
static int sector_allowed(uint32_t off)
{
#ifdef SAADET_SMK37
    return !FL_PROTECTED(off, ST_SECTOR) && !FL_IN(off, ST_SECTOR, FL_OTA_LO, FL_OTA_HI) && off != FL_STOCK_MARK &&
           off >= FL_APP_HI && (FL_STORE_OK(off, ST_SECTOR) || FL_XDATA_OK(off, ST_SECTOR)) &&
           !SD_SECTOR_BAD(off, ST_SECTOR);
#else
    return !SD_SECTOR_BAD(off, ST_SECTOR);
#endif
}

/* every live object's two sectors and the platform's pairs: allowed, none twice, the count the plan has */
static int map_test(void)
{
    static const uint32_t platform[OBJ_NOBJ] = APP_STORE_MAP;
    static uint8_t used[0x100000 / ST_SECTOR];
    uint32_t obj, copy, live = 0, pairs = 0;
    int bad = 0, ok = 1;
    memset(used, 0, sizeof used);
    for (obj = 0; obj < OBJ_COUNT; obj++) {
        if (st_retired(obj)) continue;
        for (copy = 0; copy < 2u; copy++) {
            uint32_t off = st_sector(obj, copy);
            if (!sector_allowed(off) || used[off / ST_SECTOR]) {
                printf("object %u copy %u at %#x: not allowed or used twice\n", obj, copy, off);
                ok = 0;
                continue;
            }
            used[off / ST_SECTOR] = 1;
            live++;
#ifdef SAADET_SMK37
            /* the update loader reads bytes 0xF00.. of an XDATA sector as a possible record: only compact objects */
            if (FL_XDATA_OK(off, ST_SECTOR) && st_payload_off(obj) + st_capacity(obj) > ST_SECTOR - 256u) {
                printf("object %u copy %u at %#x: reaches 0xF00 in an XDATA window\n", obj, copy, off);
                ok = 0;
            }
#endif
        }
    }
    bad += check("every live object's sectors allowed, none used twice", ok);
    bad += check("60 live object sectors", live == 60u);
    ok = 1;
    for (obj = 0; obj < OBJ_NOBJ; obj++)
        for (copy = 0; copy < 2u; copy++) {
            uint32_t off = platform[obj] + copy * ST_SECTOR;
            if (!sector_allowed(off) || used[off / ST_SECTOR]) {
                printf("platform pair %u copy %u at %#x: not allowed or used twice\n", obj, copy, off);
                ok = 0;
                continue;
            }
            used[off / ST_SECTOR] = 1;
            pairs++;
#ifdef SAADET_SMK37
            if (!FL_STORE_OK(off, ST_SECTOR)) {            /* device/unity.c st_erase, st_prog refuse the rest */
                printf("platform pair %u copy %u at %#x: outside the base's store windows\n", obj, copy, off);
                ok = 0;
            }
#endif
        }
    bad += check("the platform's pairs allowed and apart from the objects", ok && pairs == 6u);
    return bad;
}

/* every live object written full, twice (both copies): nothing lands outside the windows, and in an XDATA window the
 * loader's record bytes stay erased */
static int fill_test(void)
{
    static uint8_t full[ST_SECTOR], back[ST_SECTOR];
    uint32_t obj, off;
    int bad = 0, ok = 1;
    memset(nor, 0xFF, sizeof nor);
    pages_split = 1;
    for (off = 0; off < sizeof full; off++) full[off] = (uint8_t)(off * 13u + 1u);
    for (obj = 0; obj < OBJ_COUNT; obj++) {
        if (st_retired(obj)) continue;
        uint32_t cap = st_capacity(obj);
        ok &= st_save(obj, full, cap) == 0 && st_save(obj, full, cap) == 0;
        ok &= st_load(obj, back, sizeof back) == (int)cap && !memcmp(back, full, cap);
    }
    pages_split = 0;
    bad += check("every live object saves and loads at its capacity", ok);
    ok = 1;
    for (off = 0; off < sizeof nor; off += ST_SECTOR) {
        uint32_t i, written = 0;
        for (i = 0; i < ST_SECTOR; i++) written |= nor[off + i] != 0xFFu;
        if (written && !sector_allowed(off)) {
            printf("written outside the windows: %#x\n", off);
            ok = 0;
        }
#ifdef SAADET_SMK37
        if (FL_XDATA_OK(off, ST_SECTOR))
            for (i = 0xF00u; i < ST_SECTOR; i++) ok &= nor[off + i] == 0xFFu;
#endif
    }
    bad += check("nothing written outside the windows", ok);
    return bad;
}

int main(void)
{
    char a[600], b[600], got[600];
    int bad = 0, n;
    bad += map_test();
    bad += fill_test();
    memset(nor, 0xFF, sizeof nor);
    memset(a, 'A', sizeof a);
    memset(b, 'B', sizeof b);
    bad += check("empty flash loads nothing", st_load(T_OBJ, got, sizeof got) < 0);
    bad += check("save A", st_save(T_OBJ, a, sizeof a) == 0);
    n = st_load(T_OBJ, got, sizeof got);
    bad += check("load returns A", n == (int)sizeof a && !memcmp(got, a, sizeof a));
    bad += check("save B", st_save(T_OBJ, b, sizeof b) == 0);
    n = st_load(T_OBJ, got, sizeof got);
    bad += check("load returns B (newer seq)", n == (int)sizeof b && !memcmp(got, b, sizeof b));
    fail_after = 1;                                  /* payload page 1 written, the rest torn */
    st_save(T_OBJ, a, sizeof a);
    fail_after = -1;
    n = st_load(T_OBJ, got, sizeof got);
    bad += check("torn save keeps B", n == (int)sizeof b && !memcmp(got, b, sizeof b));
    fail_after = 3;                                  /* all payload pages, header torn */
    st_save(T_OBJ, a, sizeof a);
    fail_after = -1;
    n = st_load(T_OBJ, got, sizeof got);
    bad += check("header not written keeps B", n == (int)sizeof b && !memcmp(got, b, sizeof b));
    bad += check("save A again", st_save(T_OBJ, a, sizeof a) == 0);
    n = st_load(T_OBJ, got, sizeof got);
    bad += check("load returns A", n == (int)sizeof a && !memcmp(got, a, sizeof a));
    nor[st_sector(T_OBJ, 0) + ST_PAYLOAD_OFF + 10] ^= 0x01;   /* bit rot in one copy */
    nor[st_sector(T_OBJ, 1) + ST_PAYLOAD_OFF + 10] ^= 0x01;
    n = st_load(T_OBJ, got, sizeof got);
    bad += check("both copies corrupt -> nothing", n < 0);
    bad += check("other objects untouched", st_load(T_OBJ + 1, got, sizeof got) < 0);
    bad += check("settings save/load",
                 st_save(OBJ_SETTINGS, "hello", 5) == 0 && st_load(OBJ_SETTINGS, got, 5) == 5 && !memcmp(got, "hello", 5));
    bad += check("CRC-32 is zlib's (check value 0xCBF43926)", st_crc32("123456789", 9) == 0xCBF43926u);
    memset(nor, 0xFF, sizeof nor);
    st_save(T_OBJ + 2, a, sizeof a);
    st_save(T_OBJ + 2, b, sizeof b);              /* B is newer, in the other copy */
    {
        st_hdr_t h;
        int cur = st_current(T_OBJ + 2, &h);
        nor[st_sector(T_OBJ + 2, (uint32_t)cur) + ST_PAYLOAD_OFF + 300] ^= 0x10;   /* rot in the newer copy */
    }
    n = st_load(T_OBJ + 2, got, sizeof got);
    bad += check("newer copy rotten -> the older one loads", n == (int)sizeof a && !memcmp(got, a, sizeof a));
    bad += check("save over the rotten copy", st_save(T_OBJ + 2, b, sizeof b) == 0);
    n = st_load(T_OBJ + 2, got, sizeof got);
    bad += check("... loads the new data", n == (int)sizeof b && !memcmp(got, b, sizeof b));
    nor[st_sector(T_OBJ + 2, 0) + 8] ^= 0x01;    /* both headers broken */
    nor[st_sector(T_OBJ + 2, 1) + 8] ^= 0x01;
    bad += check("both headers broken -> nothing", st_load(T_OBJ + 2, got, sizeof got) < 0);
#ifndef SAADET_SMK37
    bad += check("data stays in the Melodee regions",
                 st_sector(OBJ_SETTINGS, 1) + 4096 <= 0xFF000 && st_sector(T_OBJ + 3, 1) + 4096 <= 0xE0000 &&
                     st_sector(OBJ_UPRESET0, 0) >= 0xDC000 && st_sector(OBJ_UPRESET0 + 1, 1) + 4096 <= 0xE0000);
    /* the FM6 patch bank: the two free sectors, 0x9F000 (after the projects) and 0xFE000 (after the settings) */
    bad += check("FM6 bank in the free sectors 0x9F000 / 0xFE000",
                 OBJ_FM6BANK + 1 == OBJ_BANK0 && st_sector(OBJ_FM6BANK, 0) == 0x9F000u &&
                     st_sector(OBJ_FM6BANK, 1) == 0xFE000u && st_sector(OBJ_SETTINGS, 1) + 4096 == 0xFE000u);
#endif
    {   /* the legacy single-pattern projects: retired, nothing loads or saves them (the Prophet banks' place) */
        uint32_t k, calls = io_calls;
        int ok = 1;
        for (k = 0; k < 4u; k++)
            ok &= st_load(OBJ_PROJECT0 + k, got, sizeof got) < 0 && st_save(OBJ_PROJECT0 + k, a, sizeof a) < 0;
        bad += check("legacy project objects retired: no load, no save, no flash access", ok && io_calls == calls);
    }
    {
        static uint8_t bank[3612], back[3612];                 /* (any record up to a sector) */
        uint32_t i;
        for (i = 0; i < sizeof bank; i++) bank[i] = (uint8_t)(i * 7u);
        bad += check("FM6 bank save / load (A then B)", st_save(OBJ_FM6BANK, bank, sizeof bank) == 0 &&
                     st_load(OBJ_FM6BANK, back, sizeof back) == (int)sizeof back && !memcmp(bank, back, sizeof bank) &&
                     (bank[0] ^= 1, st_save(OBJ_FM6BANK, bank, sizeof bank) == 0) &&
                     st_load(OBJ_FM6BANK, back, sizeof back) == (int)sizeof back && back[0] == bank[0]);
        {
            int erased = 1;
            for (i = 256u + sizeof bank; i < 4096u; i++)
                erased &= nor[st_sector(OBJ_FM6BANK, 0) + i] == 0xFFu && nor[st_sector(OBJ_FM6BANK, 1) + i] == 0xFFu;
            bad += check("FM6 bank: the sector tails stay erased", erased);
        }
    }
    {
        uint8_t copies[2 * ST_SECTOR];
        uint32_t off = st_sector(T_OBJ, 0), cut;
        int ok = 1;
        memset(nor, 0xFF, sizeof nor);
        st_save(T_OBJ, a, sizeof a);
        st_save(T_OBJ, b, sizeof b);
        memcpy(copies, nor + off, sizeof copies);
        for (cut = 0; cut < sizeof a + sizeof(st_hdr_t); cut++) {
            memcpy(nor + off, copies, sizeof copies);
            fail_bytes = (int)cut;
            ok &= st_save(T_OBJ, a, sizeof a) != 0;
            fail_bytes = -1;
            ok &= st_load(T_OBJ, got, sizeof got) == (int)sizeof b && !memcmp(got, b, sizeof b);
        }
        bad += check("every payload/header byte cut keeps old data", ok);
        erase_error = 1;
        ok = st_save(T_OBJ, a, sizeof a) != 0;
        erase_error = 0;
        bad += check("erase failure keeps old data", ok && st_load(T_OBJ, got, sizeof got) == (int)sizeof b &&
                                                      !memcmp(got, b, sizeof b));
        write_protected = 1;
        ok = st_save(T_OBJ, a, sizeof a) != 0;
        write_protected = 0;
        bad += check("write protection is a save error", ok && st_load(T_OBJ, got, sizeof got) == (int)sizeof b &&
                                                            !memcmp(got, b, sizeof b));
    }
    {
        st_hdr_t h;
        uint32_t copy;
        for (copy = 0; copy < 2u; copy++) {
            uint32_t off = st_sector(T_OBJ, copy);
            memcpy(&h, nor + off, sizeof h);
            h.seq = 0xFFFFFFFEu + copy;
            h.hcrc = st_crc32(&h, sizeof h - 4u);
            memcpy(nor + off, &h, sizeof h);
        }
        bad += check("save sequence wraps", st_save(T_OBJ, a, sizeof a) == 0);
        n = st_load(T_OBJ, got, sizeof got);
        bad += check("wrapped sequence loads new data", n == (int)sizeof a && !memcmp(got, a, sizeof a));
        bad += check("save after sequence wrap", st_save(T_OBJ, b, sizeof b) == 0 &&
                                                st_load(T_OBJ, got, sizeof got) == (int)sizeof b &&
                                                !memcmp(got, b, sizeof b));
    }
    {
        uint32_t calls = io_calls;
        bad += check("invalid object load does no flash access", st_load(OBJ_COUNT, got, sizeof got) < 0 && io_calls == calls);
        bad += check("invalid object save does no flash access", st_save(OBJ_COUNT, a, sizeof a) < 0 && io_calls == calls);
        bad += check("wrapped object rejected", st_save(0xFFFFFFFFu, a, sizeof a) < 0 && io_calls == calls);
        memset(got, 'X', sizeof got);
        bad += check("small destination rejects whole object", st_load(T_OBJ, got, sizeof got - 1u) < 0 && got[0] == 'X');
        bad += check("oversized save rejected", st_save(T_OBJ, a, ST_PAYLOAD_MAX + 1u) < 0);
    }
    for(uint32_t i=0;i<4;i++){
        uint32_t calls=io_calls;
        bad+=check("retired project bank rejected without flash access",st_save(OBJ_BANK0+i,a,sizeof a)<0 && st_load(OBJ_BANK0+i,got,sizeof got)<0 && io_calls==calls);
    }
    printf("%s\n", bad ? "STORAGE TEST FAILED" : "storage test passed");
    bad += check("CZ banks have disjoint A/B sectors outside project and preset data",
        st_sector(OBJ_CZBANK0,0)==0xC8000u && st_sector(OBJ_CZBANK0+7,1)+ST_SECTOR==0xD8000u &&
        st_capacity(OBJ_CZBANK0)==ST_PAYLOAD_MAX && st_sector(OBJ_UPRESET0,0)==0xDC000u);
    return bad != 0;
}
