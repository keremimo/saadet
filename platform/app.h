/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
#pragma once
#include <stdint.h>
#include "bridge.h"
#if !defined(SAADET_SMK37)
#define APP_NAME "SAADET"
#elif PLAT_SMK37_MODEL == 1
#define APP_NAME "SAADET PRO"
#elif PLAT_SMK37_MODEL == 2
#define APP_NAME "SAADET ELITE"
#else
#define APP_NAME "SAADET MKE"
#endif
#define APP_BLOCK 128u
#define APP_SPLASH_RGB 0xFBA0u
#define APP_CANVAS_PX (240u*24u)
#define APP_SYSEX
#define APP_HOST_EXPECT
#define APP_BTDEV
/* Separate pairs outside all 24 live Saadet objects. IDs are base-local. */
enum { OBJ_PANEL, OBJ_BLE, OBJ_BT_LAST, OBJ_NOBJ };
#ifndef SAADET_SMK37
#define APP_STORE_MAP {0xA2000u,0xA4000u,0xA6000u}
#else
#define APP_STORE_MAP {0xC2000u,0xC4000u,0xC6000u} /* the base's store refuses all but FL_DATA, FL_GLOB and FL_AUTO */
#endif
void app_init(void);
void app_render(int32_t *lr,uint32_t n,uint32_t gain);
void app_panic(void);
void app_ui_init(void);
void app_frame(void);
void app_input_only(void);
void app_say(const char *a,const char *b);
int app_sysex(const uint8_t *p,uint32_t n);
int app_expect(const char *what,int *value);

#ifndef PLAT_HOST
#define APP_CSTART
#define APP_FAULT
void app_cstart(void);
void app_fault(const void *crash);
#endif
