/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Which flash the store may touch, per board. storage.c's st_sector holds the objects' addresses and platform/app.h's
 * APP_STORE_MAP the platform's pairs; this is the windows they must lie in and the part the boot expects.
 *
 * The SMK-37's windows are felucca-bsp's boards/smk37/fm1_flash.h (the stock v016 region table, as analysed by
 * M. Geisler; not yet checked on a unit by this port): platform/app.c asserts at compile time that they agree, and
 * tests/storage_test.c enumerates every live sector against that header. Saadet uses:
 *   0x9D000-0xA0FFF  FL_XDATA0  from the end of the 0x9D000-byte app area
 *   0xA1000-0xA2FFF  FL_AUTO    the autosave pair: the last state
 *   0xA3000-0xBAFFF  FL_XDATA1  (0xBB000-0xBFFFF is the update's staging)
 *   0xC2000-0xE9FFF  FL_DATA    (0xC0000-0xC1FFF, BTIF and USRTRIM, is never written; 0xEA000 is the stock marker)
 *   0xEB000-0xEDFFF  FL_GLOB    the settings pair (0xF4000-0xFDFFF, USR, and 0xFF000-0xFFFFF are never written)
 * The update loader looks for a record in bytes 0xF00.. of a sector in the XDATA windows too, so only objects that
 * leave their last 256 bytes erased (storage.c st_capacity) go there; tests/storage_test.c checks that as well. */
#pragma once
#ifdef SAADET_SMK37
#define SD_FLASH_ID 0xEB6014u
#define SD_SMK_LOW_LO 0x9D000u                     /* XDATA0, AUTO and XDATA1, contiguous */
#define SD_SMK_LOW_HI 0xBB000u
#define SD_SMK_DATA_LO 0xC2000u
#define SD_SMK_DATA_HI 0xEA000u
#define SD_SMK_GLOB_LO 0xEB000u
#define SD_SMK_GLOB_HI 0xEE000u
#define SD_RANGE_IN(off, n, lo, hi) ((off) >= (lo) && (off) <= (hi) && (n) <= (hi) - (off))
#define SD_DATA_OK(off, n) (SD_RANGE_IN(off, n, SD_SMK_LOW_LO, SD_SMK_LOW_HI) || \
                            SD_RANGE_IN(off, n, SD_SMK_DATA_LO, SD_SMK_DATA_HI) || \
                            SD_RANGE_IN(off, n, SD_SMK_GLOB_LO, SD_SMK_GLOB_HI))
#define SD_SECTOR_BAD(off, sector) ((off) % (sector) || !SD_DATA_OK(off, sector))
#else
#define SD_FLASH_ID 0x856014u
#define SD_DATA_OK(off, n) (((off) >= 0x93000u && (off) <= 0xE0000u && (n) <= 0xE0000u - (off)) || \
                            ((off) >= 0xE5000u && (off) <= 0xFF000u && (n) <= 0xFF000u - (off)))
#define SD_SECTOR_BAD(off, sector) ((off) < 0x93000u || (off) > 0x100000u - (sector) || (off) % (sector) || \
                                    ((off) < 0xE5000u && (off) + (sector) > 0xE0000u))
#endif
