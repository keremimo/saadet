#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
# Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Generate Saadet's shared assets for the fm1-base build; no firmware compiler or packager.

The generation helper was extracted from the retired builder after checking its source history.
Its original Felucca portions and later Melodee/Saadet additions retain their attribution.
"""
import subprocess
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parents[1]
GEN = SRC / "build/gen"


def generate():
    """generated headers (UI fonts, palettes, tables)"""
    GEN.mkdir(parents=True, exist_ok=True)
    for old in ("melodee_font.h", "melodee_icons.h"):     # headers of the bitmap font and icon atlas
        (GEN / old).unlink(missing_ok=True)
    tools = SRC / "tools"
    subprocess.run([sys.executable, str(tools / "gen_scales.py"), "--check"], check=True)
    cmds = [[tools / "gen_aa_font.py", GEN / "ui_fonts.h", "--preset", "stage"],
            [tools / "gen_ui_palettes.py", GEN / "ui_palettes.h"],
            [tools / "gen_tables.py", GEN / "melodee_tables.h"],
            [tools / "gen_fm6_patches.py", GEN / "melodee_fm6.h"],
            [tools / "gen_cz1_factory.py", GEN / "melodee_cz1.h"],
            [tools / "gen_j106_factory.py", GEN / "melodee_j106.h"],
            [tools / "gen_hy_waves.py", GEN / "melodee_hy.h"],
            [tools / "gen_ppg_waves.py", GEN / "ppg_rom.h"],      # PPG's data: from the gitignored private/ppg
            [tools / "gen_ppg_factory.py", GEN / "ppg_factory.h"],
            [tools / "gen_prophet_factory.py", GEN / "melodee_prophet_factory.h", "--web", "check"]]
    procs = [subprocess.Popen([sys.executable, *map(str, c)], stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              text=True) for c in cmds]
    failed = []
    for c, p in zip(cmds, procs):
        sys.stdout.write(p.communicate()[0])
        if p.returncode:
            failed.append(c[0].name)
    if failed:
        raise SystemExit(f"build: {', '.join(failed)} failed")


if __name__ == "__main__":
    generate()
