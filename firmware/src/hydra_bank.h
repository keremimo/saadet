/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* HYDRA's user waves U1..HY_UWAVES (wave ids from HY_USER0, hydra_patch.h): one cycle of HY_PTS points each, a name of up
 * to HY_UNAME letters, and whether one was ever written (a free wave holds the factory sine and is named USER n).
 *
 * The bank stays in RAM, laid out as its two flash objects (hydra_bank.c, storage.c OBJ_HYWAVE0, +1): the parts copy
 * their waves from it (eng_hydra.c hy_want), so an edit is heard at once and nothing reads flash through XIP just after
 * writing it. Only the main loop changes it, with the audio held off for the copy (hy_uset); hy_ugen then moves on and
 * every part copies its user waves again. */
#pragma once
#define HY_UNAME 7u
#define HY_UOBJ 7u                                     /* waves a flash object holds */
typedef struct {
    int16_t pts[HY_PTS];
    char name[HY_UNAME + 1u];                          /* (0-terminated) */
    uint8_t used, rsv[3];
} hy_uwave_t;
typedef struct {
    uint32_t magic;
    uint8_t first, count, rsv[2];                      /* (the bank's first wave in this object, HY_UOBJ) */
    hy_uwave_t w[HY_UOBJ];
} hy_uobj_t;
_Static_assert(HY_UWAVES == 2u * HY_UOBJ, "two objects hold the bank");
_Static_assert(sizeof(hy_uwave_t) % 4u == 0u, "a wave copies by words");
static hy_uobj_t hy_ubank[2] __attribute__((section(".pool")));
static volatile uint32_t hy_ugen;                      /* moves on at every change the parts must copy again */
#define hy_uwave(k) (&hy_ubank[(k) / HY_UOBJ].w[(k) % HY_UOBJ])
/* the user waves' names, for the panel's wave lists (hydra_edit.h HY_ALL_NAMES) */
#define HY_UNAMES                                                                                                       \
    hy_ubank[0].w[0].name, hy_ubank[0].w[1].name, hy_ubank[0].w[2].name, hy_ubank[0].w[3].name, hy_ubank[0].w[4].name, \
    hy_ubank[0].w[5].name, hy_ubank[0].w[6].name, hy_ubank[1].w[0].name, hy_ubank[1].w[1].name, hy_ubank[1].w[2].name, \
    hy_ubank[1].w[3].name, hy_ubank[1].w[4].name, hy_ubank[1].w[5].name, hy_ubank[1].w[6].name
_Static_assert(HY_UWAVES == 14u, "HY_UNAMES lists the bank");
