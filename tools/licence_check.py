#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Check the licences of the repository's files that the fm1-base build compiles.

Every such file must carry an SPDX line with MPL-2.0 or a permissive third-party licence. There are no remaining
GPL exceptions in the base build. The empty KNOWN_GPL registry retains validation for explicitly documented
exceptions: a new GPL file fails, as does a stale exception.

The compiled files are found with the host compiler's dependency output (cc -MM) on platform/*.c, with the base
port's include directories and the Bluetooth build's options, so headers and the sources seq.c and instrument.c
include are covered. Files under build/ (the generated tables, fm1-base and felucca-bsp) are not this repository's.

  tools/licence_check.py          check (also run by tools/check_base.py after every base build)
"""
import os
from pathlib import Path
import re
import subprocess
import sys
import tomllib

ROOT = Path(__file__).resolve().parents[1]
BASE = Path(os.environ.get("SAADET_BASE", ROOT / "build/fm1-base-platform"))
SMK37 = os.environ.get("SAADET_BOARD") == "smk37"     # tools/build_base.py --board smk37: that build's sources and defines
BOARD_DEFINES = ["-DPLAT_SMK37=1", "-DPLAT_SMK37_MODEL=2", "-DSAADET_SMK37=1"] if SMK37 else []

# All four former exceptions have independent replacements; see docs/cleanroom/preferences-process.md.
KNOWN_GPL = {}
ALLOWED = {"MPL-2.0", "MPL-2.0 AND Apache-2.0", "MIT", "BSD-3-Clause", "Apache-2.0"}
SPDX = re.compile(r"SPDX-License-Identifier:\s*(.+?)\s*(?:\*/)?$", re.M)


def spdx(path):
    """the file's SPDX expression (within its first 12 lines), or None"""
    with open(path, encoding="utf-8", errors="replace") as f:
        head = "".join(f.readline() for _ in range(12))
    m = SPDX.search(head)
    return m[1].strip() if m else None


def compiled():
    """the repository files (relative paths) the base build compiles"""
    inc = ["platform", "firmware/src", "build/gen"] + [str(BASE / d) for d in ("device", "include", "lib", "bsp/chip", "ble")]
    cc = os.environ.get("HOST_CC", "cc")
    files = set()
    for src in sorted((ROOT / "platform").glob("*.c")):
        out = subprocess.run([cc, "-MM", "-MG", "-std=gnu99", "-w", "-DFM1B_BLE=1", "-DFM1B_BLE_CENTRAL=1",
                              "-DMELODEE_RAM_AUDIO=1", *BOARD_DEFINES, *(f"-I{d}" for d in inc), str(src.relative_to(ROOT))],
                             cwd=ROOT, capture_output=True, text=True, check=True).stdout
        for dep in out.replace("\\\n", " ").split(":", 1)[1].split():
            path = Path(os.path.normpath(ROOT / dep))
            if path.is_relative_to(ROOT) and not path.is_relative_to(ROOT / "build") and path.exists():
                files.add(str(path.relative_to(ROOT)))
    return files


def problems(files, licence=spdx):
    """-> (problems, known GPL files found)"""
    bad, known = [], []
    for f in sorted(files):
        lic = licence(ROOT / f)
        if lic is None:
            bad.append(f"{f}: no SPDX-License-Identifier")
        elif lic in ALLOWED:
            continue
        elif f in KNOWN_GPL and lic == "GPL-3.0-only":
            known.append(f)
        else:
            bad.append(f"{f}: {lic} in the base build (not MPL-2.0 or a permissive licence)")
    for f in sorted(KNOWN_GPL):
        if f not in files:
            bad.append(f"{f}: listed in KNOWN_GPL but no longer compiled in: remove it from the list")
        elif licence(ROOT / f) != "GPL-3.0-only":
            bad.append(f"{f}: listed in KNOWN_GPL but now {licence(ROOT / f)}: remove it from the list")
    return bad, known


def check():
    files = compiled()
    bad, known = problems(files)
    bad.extend(manifest_problems())
    bad.extend(board_layer_problems())
    if bad:
        raise SystemExit("licence check:\n  " + "\n  ".join(bad))
    message = f"licence check: {len(files)} files, all MPL-2.0 or permissive"
    if known:
        message += f" except {len(known)} known GPL-3.0-only: " + ", ".join(Path(f).name for f in known)
    print(message)


def board_layer_problems():
    """The SMK-37 board layer is fm1-base's and felucca-bsp's, under build/: new work, and it must stay free of GPL."""
    if not SMK37:
        return []
    files = [*sorted((BASE / "bsp/boards/smk37").glob("*.h")), BASE / "device/plat_smk37.c", BASE / "device/midi_trs_out.c",
             BASE / "device/app_smk37.ld"]
    return [f"{f.relative_to(BASE)}: {spdx(f)} in the SMK-37 board layer" for f in files if spdx(f) not in ALLOWED]


def manifest_problems():
    """The app's declared licence must agree with the completed source migration."""
    with (ROOT / "platform/fm1.toml").open("rb") as source:
        licence = tomllib.load(source).get("licence")
    return [] if licence == "MPL-2.0" else [f"platform/fm1.toml: app licence is {licence!r}, expected MPL-2.0"]


if __name__ == "__main__":
    sys.exit(check())
