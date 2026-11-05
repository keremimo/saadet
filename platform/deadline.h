/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Saadet's overload policy: measure, never cut. An overloaded render plays every voice late rather than losing one.
 * Polyphony is bounded before a note starts, by the shared budget (voice.c voice_budget). The shed counters stay 0,
 * for editor command 72's layout. */
static const uint32_t shed_count=0,shed_part[NPART]={0};

void saadet_deadline(uint32_t us,uint32_t budget){
    if(us>sd_render_max_us)sd_render_max_us=us;
    song.cpu_q8=(song.cpu_q8*31u+(budget?us*256u/budget:0u))/32u;
}
