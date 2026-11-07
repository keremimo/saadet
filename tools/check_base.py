#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Additional invariants specific to Saadet's base port, after upstream checks."""
from pathlib import Path
import hashlib
import json
import re
import sys
import os
ROOT=Path(__file__).resolve().parents[1]
OUT=Path(os.environ.get('SAADET_OUT',ROOT/'platform/build'))
BASE=Path(os.environ.get('SAADET_BASE',ROOT/'build/fm1-base-platform'))
syms=(OUT/'saadet.symbols').read_text()
def sym(name):
    m=re.search(r'^([0-9a-f]+) .*\s'+re.escape(name)+r'$',syms,re.M)
    assert m,f'missing required target symbol {name}'
    return int(m[1],16)
assert sym('_start')==0x02000120
# SRAM placement alone is insufficient: boot itself calls some resident P33
# helpers. Initialize code and pointer data before the first platform call.
boot=(BASE/'device/main_fm1.c').read_text().split('void fm1_cstart(void)',1)[1]
first=boot.index('    fm1_time_init();')
for marker in ('d = _bss_start','boot_image(_rt_load','at, _data_start','at, _sd_audio_start','d = _pool_start','d = _sd_audio_end'):
    assert boot.index(marker)<first,f'boot uses resident helpers before initialization: {marker}'
# .audio_text lies in the pool: the clear goes around it, and only after the decoder's tables at the pool's start are done.
assert boot.index('at, _sd_audio_start')<boot.index('d = _pool_start'),'POOL clear leaves the decoder tables behind'
assert 'd < _sd_audio_start' in boot and 'd < _pool_end' in boot.split('d = _sd_audio_end',1)[1][:40],'POOL clear wipes resident code'
# CPU1 executes mask-ROM before our entry. PC limits may be armed only once
# app_init's bounded handshake has returned (ready, skipped, or held timeout).
main=(BASE/'device/main_fm1.c').read_text()
assert 'fm1_guard_enable(FM1_GUARD_STACK | FM1_GUARD_WRITE | FM1_GUARD_BUS);' in boot
assert 'FM1_GUARD_PC' not in boot,'PC limits trap CPU1 mask-ROM bootstrap'
app_init=main.index('    app_init();')
pc_guard=main.index('    fm1_guard_enable(FM1_GUARD_PC);')
assert app_init<pc_guard<main.index('    audio_init();',app_init),'PC limits must follow CPU1 handshake and precede audio'
instrument=(ROOT/'platform/instrument.c').read_text().split('void saadet_init(void)',1)[1].split('void saadet_render',1)[0]
assert 'fm1_multicore_start();' in instrument,'PC guard ordering assumes CPU1 starts inside app_init'
irq=(BASE/'bsp/chip/fm1_irq.h').read_text()
assert 'FM1_EMU_CON &= ~((1u << 2) | (31u << 16));' in irq,'CPU0 must use Melodee fp32 exception policy'
power=main.index('        sd_power_init();')
assert power<main.index('    lcd_init();',power)<app_init,'stock power setup must precede peripherals and CPU1'
app=(ROOT/'platform/app.c').read_text()
assert '#include "../firmware/hal/fm1_power.h"' in app,'base must reuse Melodee stock power initializer'
assert 'int sd_power_ready(void){return fm1_power_ready;}' in app and 'int sd_boot_failed(void){return bootguard.failed!=0u;}' in app \
    and 'fm1_core1_boot_allowed((uint32_t)sd_power_ready(),(uint32_t)sd_boot_failed())' in instrument,\
    'CPU1 must require the full verified supply setup and a boot that did not fail, by Melodee\'s gate (main.c)'
assert 'void sd_reset_intended(void){bootguard.pending=0;}' in app,'a reset on purpose must clear the base\'s own boot guard'
assert sym('sd_worker_stop')>=0x02000120,'early CPU1 stop must execute from flash before SRAM initialization'
lo,hi=sym('_sd_audio_start'),sym('_sd_audio_end')
assert 0x1c20000<=lo<hi<=sym('_pool_end')<=sym('_resource_end')==0x1c74000
assert sym('_resource_end')-sym('_resource_start')>=16384
assert sym('_cpu1_stacks_hi')-sym('_cpu1_stacks_lo')==8192
for name in ('saadet_render','audio_worker_loop','fm1_core1_main'):
    assert lo<=sym(name)<hi,f'{name} must be resident before the worker starts'
# CPU1 must poll shared SRAM using inlined helpers; only a checked job may call.
dis=(OUT/'saadet.dis').read_text()
body=dis.split('\naudio_worker_loop:',1)[1].split('\n\n',1)[0]
assert not re.search(r'\bcall -?\d+',body),'CPU1 idle loop contains an outlined helper'
# Every load image must actually be part of the install image, and unpack to what was linked.
image=(OUT/'saadet.bin').read_bytes()
sys.path.insert(0,str(BASE/'tools'))
import boot_pack
import lzma
assert boot_pack.SECTIONS==('ramtext.bin','data.bin','audio.bin'),'the boot unpacks three images'
records=boot_pack.unpack_images(image[sym('_rt_load')-sym('_start'):],3)
for section,(size,stream) in zip(boot_pack.SECTIONS,records):
    raw=(OUT/section).read_bytes()
    assert size==len(raw) and lzma.decompress(stream,format=lzma.FORMAT_RAW,filters=boot_pack.FILTERS)==raw,f'{section} does not unpack'
assert records[2][0]==hi-lo,'.audio_text unpacks to its whole section'
# The decoder's tables lie at the pool's start while it writes .audio_text further up.
assert lo-sym('_pool_start')>=8192*2,'the boot decoder tables overlap .audio_text'
assert len(image)<=sym('_app_xip_end')-sym('_start'),'the packed image overruns the app slot'
assert sym('_bss_end')<=0x1c20000
# The USB recording's ring: the deep one (app.h FM1B_UAC_DEEP), 1024 native frames, in POOL and not in low RAM.
ring=re.search(r'^[0-9a-f]+ l\s+\.pool\s+([0-9a-f]+) ua_ring$',syms,re.M)
assert ring and int(ring[1],16)==1024*8,'USB recording ring must be the deep one, in POOL'
for retired in ('fm1_stub_start','crstub_main'):
    assert not re.search(r'\s'+retired+r'$',syms,re.M),f'legacy boot/update linked: {retired}'
package=Path(sys.argv[1]) if len(sys.argv)>1 else OUT/'saadet.fwsc'
assert b'CRSTUB' not in image,'legacy boot-stub signature present'
report={'app_bytes':len(image),'low_ram_bytes':sym('_bss_end')-0x1c08000,
        'resident_audio_bytes':hi-lo,'pool_bytes':sym('_pool_end')-0x1c20000,
        'engine_arena_bytes':sym('_resource_end')-sym('_resource_start'),
        'package_sha256':hashlib.sha256(package.read_bytes()).hexdigest()}
(OUT/(package.stem+'-validation.json')).write_text(json.dumps(report,indent=2)+'\n')
print('Saadet target checks passed: CPU1, bootstrap PC guards, SRAM code, packed image, arena, boot layout')
print(json.dumps(report))
# Licences: every repository file compiled into this build must be MPL-2.0 or permissive; no GPL exceptions.
sys.path.insert(0,str(ROOT/'tools'))
from licence_check import check as licence_check
licence_check()
