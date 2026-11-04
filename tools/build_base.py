#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Build the experimental fm1-base port from exact dependency revisions.

  tools/build_base.py [--board fm1|smk37] [--model pro|elite|mke] [fm1-base build options]

The FM-1 build (the default) is unchanged. --board smk37 needs --model and makes platform/build/smk37-MODEL/ and
build/saadet-smk37[-elite|-mke].fwsc from the base with the SMK-37 board layer, which installs on nothing here.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
from release_support import source_state, record_build

if "--legacy" in sys.argv:
    raise SystemExit("The legacy platform is retired; only fm1-base targets are available.")

ROOT=Path(__file__).resolve().parents[1]
REV="6c0b47e4bfc56fd3cd5b34463210d0e7e0ce0431"
BSP_REV="0f08fcaa6610b7cabe8cfdcc5c50d6facdae0e7f"
# The SMK-37 board layer: Kerem's unpushed branches smk37-board-24705d (fm1-base) and smk37-board (felucca-bsp), used
# from their local worktree until they are pushed. The revisions pin it as REV and BSP_REV pin the FM-1's.
SMK_REV="c1082696f249c650a556871b397e3f2fb05cf0b3"
SMK_BSP_REV="af07023f1704214afdf971c278dba2e092a3e899"
SMK_SOURCE=Path.home()/"code/personal/fm1-base/.claude/worktrees/smk37-board-24705d"
SMK_MODELS={"pro":("Pro",""),"elite":("Elite","-elite"),"mke":("MKE","-mke")}  # the product's name, the file's suffix
parser=argparse.ArgumentParser(add_help=False)
parser.add_argument("--board",choices=("fm1","smk37"),default="fm1")
parser.add_argument("--model",choices=tuple(SMK_MODELS))
board_args,build_args=parser.parse_known_args()
board,model=board_args.board,board_args.model
if board=="smk37" and not model:
    raise SystemExit("--board smk37 needs --model pro, elite or mke")
if board=="fm1" and model:
    raise SystemExit("--model is the SMK-37's: use it with --board smk37")
smk=board=="smk37"
if smk:
    REV,BSP_REV=SMK_REV,SMK_BSP_REV
    source=SMK_SOURCE
    work=ROOT/"build/fm1-base-smk37-platform"
else:
    source=ROOT/"build/deps"/("fm1-base-"+REV[:12])
    work=ROOT/"build/fm1-base-platform"
def run(args,**kw):subprocess.run([str(x) for x in args],check=True,**kw)
if smk and not source.exists():
    raise SystemExit(f"No SMK-37 board layer at {source}")
if not source.exists():
    source.parent.mkdir(parents=True,exist_ok=True)
    run(["git","clone","--no-checkout","https://github.com/charlesvestal/fm1-base.git",source])
    run(["git","-C",source,"checkout","--detach",REV])
    run(["git","-C",source,"submodule","update","--init","--recursive"])
if subprocess.check_output(["git","-C",str(source),"rev-parse","HEAD"],text=True).strip()!=REV:
    raise SystemExit("fm1-base audit checkout is not at the pinned revision")
if subprocess.check_output(["git","-C",str(source/"bsp"),"rev-parse","HEAD"],text=True).strip()!=BSP_REV:
    raise SystemExit("felucca-bsp is not at the pinned revision")
if subprocess.check_output(["git","-C",str(source),"status","--porcelain"],text=True).strip():
    raise SystemExit("Pinned dependency has local changes; preserve them and use a clean checkout")
if work.exists():
    shutil.rmtree(work)  # disposable build overlay only; never the dependency checkout
shutil.copytree(source,work,ignore=shutil.ignore_patterns(".git","build","__pycache__"))
# The overlay is kept in the repository; dependency sources remain untouched.
from base_overlay import apply, smk37_linker, smk37_toml
if smk:
    name=SMK_MODELS[model][0]
    (work/"device/app_smk37.ld").write_text(smk37_linker((ROOT/"platform/app.ld").read_text()))
    (work/"saadet-fm1.toml").write_text(smk37_toml((ROOT/"platform/fm1.toml").read_text(),model,name))
    apply(work,board,model,"Saadet "+name)
else:
    shutil.copyfile(ROOT/"platform/app.ld",work/"device/app.ld")
    apply(work)
    from base_bootloader import prepare  # m-bootloader is the FM-1's: nothing of it on the SMK-37
    prepare(work, ROOT/"platform/build/bootloader")
# Asset generation is independent of the retired firmware compiler and packager.
from generate_assets import generate
generate()
env=os.environ.copy()
env.setdefault("JIELI_TOOLCHAIN",str(Path.home()/".jieli/toolchain"))
env.setdefault("AC79_SDK",str(Path.home()/"fw-AC79_AIoT_SDK"))
before = source_state(ROOT)
run([sys.executable,work/"tools/build.py",ROOT/"platform",*build_args],env=env,cwd=ROOT)

if "--host" not in build_args and "--web" not in build_args:
    out=ROOT/"platform/build"/f"smk37-{model}" if smk else ROOT/"platform/build"
    stem="saadet-smk37"+SMK_MODELS[model][1] if smk else "saadet"
    if "--no-ble" in build_args:
        stem+="-usb"
    if "--release" in build_args:
        stem+="-"+build_args[build_args.index("--release")+1]
    check_env=os.environ.copy()
    if smk:
        check_env.update(SAADET_OUT=str(out),SAADET_BASE=str(work),SAADET_BOARD=board)
    run([sys.executable,ROOT/"tools/check_base.py",out/(stem+".fwsc")],env=check_env)
    version = build_args[build_args.index("--release")+1] if "--release" in build_args else None
    if version:
        # The optimizer expands the short editor INFO string into byte stores,
        # so it need not survive as a contiguous literal in the linked image.
        editor_ir = (out/"o2/instrument.ll").read_text()
        image = (out/"saadet.bin").read_bytes()
        if ('c"SAADET v' + version.upper() + r'\00"') not in editor_ir:
            raise SystemExit("The compiled editor INFO does not advertise its requested version.")
        if ("SAADET " + version.upper()).encode() not in image:
            raise SystemExit("The release image announcement does not match its requested version.")
    from fm1_install import product_of
    package = out/(stem+".fwsc")
    record_build(ROOT, package, before, version, product_of(package.read_bytes()), "--no-ble" not in build_args,
                 REV, BSP_REV)
    if smk:
        shutil.copyfile(package,ROOT/"build"/package.name)
        print(f"package  {ROOT/'build'/package.name}")
