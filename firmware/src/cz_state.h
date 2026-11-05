/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Runtime state for native CZ tones. No PHASE oscillator or envelope path. */
#pragma once
#include "cz_patch.h"

typedef struct { float level; uint8_t stage, gate; } cz_native_env_t;
typedef struct { float note[2]; int32_t fine[2]; uint32_t a4[2], inc[2], valid; } cz_native_freq_t;
typedef struct {
    cz_native_env_t native_eg[2][3];
    uint32_t vib_phase, vib_ticks, noise;
    float native_dc[2];
    cz_native_freq_t native_freq;
} cz_voice_t;
typedef struct { float rate[8], level[8]; uint8_t sustain, end; } cz_native_def_t;
typedef struct {
    cz_voice_t v[NPOLY + 1u];   /* the cap and engine_t.spare */
    cz_native_def_t native_def[2][3];
    uint8_t native_raw[128], native_valid;
} cz_part_t;
static cz_patch_t cz_patch[NTRK] __attribute__((section(".pool")));
static void cz_init(void)
{
    for (uint32_t i = 0; i < NTRK; i++) cz_patch_init(cz_patch[i].raw);
}
static cz_part_t *cz_part(uint32_t part);
static cz_voice_t *cz_voice(track_t *t, voice_t *v)
{
    return &cz_part((uint32_t)(t - trk) % NPART)->v[(uint32_t)(v - t->v) % (NPOLY + 1u)];
}
