#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Write the contract an external editor vendors from this firmware into build/contract/. Run it after
tests/run_tests.sh, which writes the host fixtures it copies.

  tools/export_editor_contract.py

  fixtures/*.bin     what tests/run_tests.sh left in build/host/: INFO's reply (synth-info.bin), the synth state and
                     settings objects, every backup object (archive-<id>.bin, editor_backup.c ED_BK_IDS), HYDRA's user
                     waves LIST and a GET of SAW (hywave-list.bin, hywave-get-saw.bin); a reply's bytes between its
                     command and F7, an object's bytes as stored
  j106-factory.syx   assets/j106-factory/j106-factory.syx
  scales.json        the SCL catalogue in the firmware's order (its index is the scale's id): name, title, family, count,
                     from assets/scales/catalog.json as tools/gen_scales.py reads it
  hydra.json         HYDRA's factory waves (tools/gen_hy_waves.py: id 1 to 56, 0 is EMPTY), its macros' names (HY_NAMES)
                     and their targets (HY_TARGETS: the slot's 5-bit id, its key, its label, the patch byte it moves;
                     id 0, NONE, is a free slot), from firmware/src/hydra_patch.h
  manifest.json      the saadet commit and `git describe`, whether the tree had changes, the firmware's version string
                     (platform/instrument.c MELODEE_VERSION, what a ./build.sh image without --release names itself
                     after "SAADET "), the host fixtures' INFO name, and the sha256 of every other file
"""
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
HOST = ROOT / "build/host"
OUT = ROOT / "build/contract"
sys.path.insert(0, str(ROOT / "tools"))
import gen_hy_waves  # noqa: E402
import gen_scales  # noqa: E402


def source(path):
    """A C file without its comments."""
    text = (ROOT / path).read_text()
    return re.sub(r"//[^\n]*", "", re.sub(r"/\*.*?\*/", "", text, flags=re.S))


def macro(text, name):
    """The body of a #define spread over continued lines."""
    m = re.search(r"#define " + name + r"\s*\\\n((?:[^\n]*\\\n)*[^\n]*)", text)
    if not m:
        raise SystemExit(f"No {name} in firmware/src/hydra_patch.h")
    return m.group(1).replace("\\\n", " ")


def enums(text):
    """The values of every plain enum constant (NAME or NAME = expression over earlier ones)."""
    values = {}
    for body in re.findall(r"enum\s*\{(.*?)\}\s*;", text, flags=re.S):
        nxt = 0
        for item in body.split(","):
            m = re.fullmatch(r"\s*([A-Za-z_]\w*)\s*(?:=\s*([\w\s+*()-]+?))?\s*", item)
            if not m:
                continue
            nxt = evaluate(m.group(2), values) if m.group(2) else nxt
            values[m.group(1)] = nxt
            nxt += 1
    return values


def evaluate(expr, values):
    expr = re.sub(r"\b(\d+)u\b", r"\1", expr)
    if not re.fullmatch(r"[\w\s+*()-]+", expr):
        raise SystemExit(f"Not a constant expression: {expr}")
    return int(eval(expr, {"__builtins__": {}}, dict(values)))  # (names, digits and + - * only, checked above)


def scales():
    return [dict(name=r["name"], title=r["title"], family="Western" if r["group"] == "Original" else r["group"],
                 count=len(r["pitch"]) - 1) for r in gen_scales.catalogue()]


def hydra():
    text = source("firmware/src/hydra_patch.h")
    values = enums(text)
    waves = [dict(id=i + 1, name=n) for i, (n, _) in enumerate(gen_hy_waves.WAVES)]
    names = [dict(id=i, name=n) for i, n in enumerate(re.findall(r"X\((\w+)\)", macro(text, "HY_NAMES")))]
    targets = [dict(id=i, key=k, label=label, byte=evaluate(b, values))
               for i, (k, b, label) in enumerate(re.findall(r'X\((\w+),\s*([^,]+?),\s*"([^"]*)"\)', macro(text, "HY_TARGETS")))]
    count = int(re.search(r"#define HY_WAVE_COUNT (\d+)u", text).group(1))
    assert len(waves) == count, "gen_hy_waves.py and HY_WAVE_COUNT disagree"
    assert len(targets) == int(re.search(r"#define HY_NTGT (\d+)u", text).group(1)) + 1, "a target for every slot id"
    assert all(0 <= t["byte"] < values["HY_NAME"] for t in targets), "a target outside the patch's fields"
    assert len(names) == len({n["name"] for n in names}) and names[0]["name"] == "AUTO"
    return dict(waves=waves, macro_names=names, macro_targets=targets)


def git(*args):
    return subprocess.check_output(["git", "-C", str(ROOT), *args], text=True).strip()


def main():
    ids = [int(x) for x in re.search(r"ED_BK_IDS\[\]\s*=\s*\{([^}]*)\}", source("firmware/src/editor_backup.c")).group(1).split(",")]
    fixtures = ["synth-info.bin", "synth-state.bin", "synth-settings.bin", *(f"archive-{i}.bin" for i in ids),
                "hywave-list.bin", "hywave-get-saw.bin"]
    missing = [f for f in fixtures if not (HOST / f).is_file()]
    if missing:
        raise SystemExit(f"No {', '.join(missing)} in build/host: run tests/run_tests.sh first")
    if OUT.exists():
        shutil.rmtree(OUT)
    (OUT / "fixtures").mkdir(parents=True)
    for f in fixtures:
        shutil.copyfile(HOST / f, OUT / "fixtures" / f)
    shutil.copyfile(ROOT / "assets/j106-factory/j106-factory.syx", OUT / "j106-factory.syx")
    for name, data in [("scales.json", scales()), ("hydra.json", hydra())]:
        (OUT / name).write_text(json.dumps(data, indent=1) + "\n")
    info = (OUT / "fixtures/synth-info.bin").read_bytes()
    version = re.search(r'#else\s*\n\s*#define MELODEE_VERSION "([^"]+)"', (ROOT / "platform/instrument.c").read_text())
    if not version or 0 not in info:
        raise SystemExit("No MELODEE_VERSION in platform/instrument.c, or no name in INFO's reply")
    files = sorted(p.relative_to(OUT).as_posix() for p in OUT.rglob("*") if p.is_file())
    manifest = dict(format=1, commit=git("rev-parse", "HEAD"), describe=git("describe", "--tags", "--always", "--dirty"),
                    dirty=bool(git("status", "--porcelain")), firmware_version=version.group(1),
                    host_info_name=info[:info.index(0)].decode("ascii"),
                    files={f: hashlib.sha256((OUT / f).read_bytes()).hexdigest() for f in files})
    (OUT / "manifest.json").write_text(json.dumps(manifest, indent=1) + "\n")
    print(f"editor contract: {len(files)} files at {manifest['describe']} -> {OUT}")


if __name__ == "__main__":
    main()
