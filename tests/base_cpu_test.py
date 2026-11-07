#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Exercise the generated CPU0 exception setup against register mocks."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
def function(source, signature):
    """the function that starts with signature, through its closing brace"""
    start = source.index(signature)
    opening = source.index('{', start)
    end, depth = opening + 1, 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


source = (ROOT / 'build/fm1-base-platform/bsp/chip/fm1_irq.h').read_text()
body = function(source, 'static void fm1_irq_init(void)').replace(
    '*(volatile uint32_t *)(0x1EEF100u + 4u * i)', 'host_icfg[i]')
# CPU1 at boot and at a crash, as Melodee's main.c has it: its gate (fm1_core1_bar.h) fed the base's supply and boot
# guard, a reset on purpose clearing the base's guard, and a crash barring CPU1 only once it was working
bar = (ROOT / 'firmware/hal/fm1_core1_bar.h').read_text()
app = (ROOT / 'platform/app.c').read_text()
glue = '\n'.join(next(line for line in app.splitlines() if line.startswith(s)) for s in (
    'int sd_power_ready(void)', 'int sd_boot_failed(void){return bootguard', 'void sd_reset_intended(void){bootguard'))
cpu1 = '\n'.join(function(bar, s) for s in (
    'static inline int fm1_core1_barred(void)', 'static inline void fm1_core1_bar_set(void)',
    'static inline int fm1_core1_boot_allowed(')) + '\n' + function(
    (ROOT / 'platform/hardware/cpu1.h').read_text(), 'void sd_worker_crash(void)')
instrument = (ROOT / 'platform/instrument.c').read_text()
assert 'fm1_core1_boot_allowed((uint32_t)sd_power_ready(),(uint32_t)sd_boot_failed())' in instrument
assert 'sd_reset_intended();' in function((ROOT / 'firmware/src/core.h').read_text(), 'static inline void reset_intended(void)')
code = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
static uint32_t host_icfg[32], vectors[128], ilat, emu_msg, unlock, dbg_clear, emu, etm;
static char fm1_fatal_stubs[128*6];
#define FM1_ICFG(n) host_icfg[(n)>>3]
#define FM1_VEC vectors
#define FM1_ILAT_CLR ilat
#define FM1_EMU_MSG emu_msg
#define FM1_DBG_WR_EN unlock
#define FM1_DBG_MSG_CLR dbg_clear
#define FM1_EMU_CON emu
#define FM1_ETM_CON etm
static uint8_t fm1_power_ready;
static struct { uint32_t failed, pending; } bootguard;
#define MELODEE_CORE1_FALLBACK 1
#define FM1_CORE1_BAR_MAGIC 0x52414231u
static struct { uint32_t magic, barred; } fm1_core1_bar;
static int audio_worker_online, stops;
static void fm1_core1_stop(void) { stops++; }
''' + body + '\n' + glue + '\n' + cpu1 + r'''
int main(void) {
    const uint32_t states[] = {0, 0xFFFFFFFFu, 0x001F000Cu, 0xA5A5A5A5u, 8u};
    for (unsigned i = 0; i < sizeof states / sizeof *states; i++) {
        emu = states[i]; etm = 0x24u; unlock = 1u;
        fm1_irq_init();
        /* The five FPU exception enables and div0 are cleared on every boot;
         * stack protection and all unrelated bits retain their input state. */
        assert(emu == (states[i] & ~0x001F0004u));
        assert((emu & 8u) == (states[i] & 8u));
        assert(etm == 0x25u && ilat == 0xFFu);
        assert(emu_msg == 0xFFFFFFFFu && dbg_clear == 0xFFFFFFFFu);
        assert((host_icfg[0] & 0xF0u) == 0xF0u);
    }
    for (unsigned ready = 0; ready < 2; ready++)
        for (unsigned failed = 0; failed < 4; failed++)
            for (unsigned barred = 0; barred < 2; barred++) {
                fm1_power_ready = ready; bootguard.failed = failed;
                fm1_core1_bar.magic = barred ? FM1_CORE1_BAR_MAGIC : 0u; fm1_core1_bar.barred = 1;
                assert(fm1_core1_boot_allowed(sd_power_ready(), sd_boot_failed()) == (ready && !failed && !barred));
            }
    bootguard.pending = 1;
    sd_reset_intended();                       /* Saadet's reset on purpose: the base's guard, not a copy */
    assert(!bootguard.pending);
    fm1_core1_bar.magic = 0; audio_worker_online = 0;
    sd_worker_crash();                         /* a crash with CPU1 never started: no bar, CPU1 held */
    assert(!fm1_core1_barred() && stops == 1);
    audio_worker_online = 1;
    sd_worker_crash();                         /* with CPU1 working: one core until power-off */
    assert(fm1_core1_barred() && stops == 2);
    puts("base CPU: Melodee fp32 trap policy, retained stack guard, exception vector and trace setup passed");
    puts("base CPU: unverified supplies, early failed boots and a retained bar keep CPU1 held, as Melodee's start does");
    puts("base CPU: a reset on purpose is not a failed boot; a crash bars CPU1 only once it was working");
}
'''
with tempfile.TemporaryDirectory() as tmp:
    path = Path(tmp)
    (path / 'test.c').write_text(code)
    subprocess.run([os.getenv('CC', 'cc'), '-O1', '-g', '-fsanitize=address,undefined',
                    str(path / 'test.c'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
