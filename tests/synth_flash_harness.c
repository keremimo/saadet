/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* NOR model used by the single-track UI, persistence and editor tests. */
static uint8_t nor[0x100000], flash_ok = 1;
static int erase_error, fail_after = -1;
static int erase_skip, erase_fail_at = -1;
static uint32_t erases;
static int st_read(uint32_t off, void *dst, uint32_t n)
{ assert(off + n <= sizeof nor); memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off)
{
    assert(off % 4096u == 0 && off <= sizeof nor - 4096u);
    if (erase_error || (erase_fail_at >= 0 && erases == (uint32_t)erase_fail_at)) return -1;
    erases++;
    if (!erase_skip) memset(nor + off, 255, 4096);
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    if (!fail_after) return -1;
    if (fail_after > 0) fail_after--;
    for (uint32_t i = 0; i < n; i++) nor[off + i] &= ((const uint8_t *)src)[i];
    return 0;
}
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
#include "../firmware/src/store_map.h"
static uint32_t fl_jedec_ram(void) { return SD_FLASH_ID; }
static void fl_plain_window_init(void) {}
#define FL_FAR(fn) (fn)
#include "../firmware/src/storage.c"
