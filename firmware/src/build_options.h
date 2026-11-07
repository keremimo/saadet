/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Shared by the application and assembly startup. Explicit build overrides win. */
#ifndef MELODEE_BUILD_OPTIONS_H
#define MELODEE_BUILD_OPTIONS_H
#include "performance_options.h"
#ifndef MELODEE_DUAL_CORE
#define MELODEE_DUAL_CORE 1     /* paired FM6/Prophet kernels; serial fallback if CPU1 is unavailable */
#endif
#ifndef MELODEE_CORE1_FALLBACK
#define MELODEE_CORE1_FALLBACK 1 /* 0: diagnose CPU1 failures by halting for the watchdog, never render alone */
#endif
#ifndef SAADET_PPG
#define SAADET_PPG 0            /* PPG, the PPG Wave 2.2 engine (eng_ppg.c). It needs the local wave image
                                 * (private/ppg/ppg22-waves.bin, tools/gen_ppg_waves.py), which is never committed. Here
                                 * too: the panel's colours (gfx.c) are built before core.h is read */
#endif
#if MELODEE_CORE1_FALLBACK != 0 && MELODEE_CORE1_FALLBACK != 1
#error "MELODEE_CORE1_FALLBACK must be 0 or 1"
#endif
#endif
