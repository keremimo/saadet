#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Reuse Saadet's checked resident-audio transformation for the two base units."""
import json
from pathlib import Path
import re
import sys
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
import audio_ram
from jieli_toolchain import tc
src,dst=map(Path,sys.argv[1:3])
unit=sys.argv[3] if len(sys.argv)>3 else ''
optimized=src.with_suffix('.optimized.ll')
tc('cc','-mcpu=r3','-mfprev1','-ffp-contract=off','-fno-builtin','-O2' if unit else '-Os','-S','-emit-llvm',src,'-o',optimized)
roots=audio_ram.engine_roots(ROOT/'firmware/src')
BASE_RAM={'sd_ms','sd_ticks','sd_lock','sd_unlock','sd_watchdog','sd_midi_out'}   # the base's API, resident in SRAM
if unit:
    roots|={'saadet_render','saadet_deadline'}
    required='saadet_render'
else:
    roots|=BASE_RAM
    required='fm1_alnk0_irq'
s,selected,far=audio_ram.relocate(optimized.read_text(),roots,required_root=required,ram_externals=BASE_RAM if unit else ())
dst.write_text(s)
dst.with_suffix('.json').write_text(json.dumps({'resident':selected,'far_calls':far},indent=2))
print(f'{unit or "base"}: {len(selected)} resident functions, {len(far)} flash/RAM boundaries')
