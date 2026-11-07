#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Prepare a local BLE release, matching source and uploadable installer site; never publish or flash.

The FM-1 package and the SMK-37's (Pro, Elite, MKE-P37: beta, built on the board layer that tools/build_base.py pins
as SMK_SOURCE) come from one commit; the site installs whichever the connected device's identity names."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile

from make_site import main as make_site, release_product, smk_product, SMK_FILES
from release_support import make_source_archive, sha256, validate_build, source_state

ROOT = Path(__file__).resolve().parents[1]
# the SMK-37 models: build_base.py's --model, the build's file suffix, the identity's model
SMK = (("pro", "", "SMK-37 Pro"), ("elite", "-elite", "SMK-37 Elite"), ("mke", "-mke", "MKE-P37"))


def smk_source():
    """the board layer's checkout, as tools/build_base.py pins it (SMK_SOURCE)"""
    m = re.search(r'^SMK_SOURCE=Path\.home\(\)/"([^"]+)"', (ROOT / "tools/build_base.py").read_text(), re.M)
    if not m:
        raise SystemExit("tools/build_base.py has no SMK_SOURCE")
    return Path.home() / m[1]


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("version")
    parser.add_argument("--out", type=Path, help="new output directory (default: build/releases/VERSION)")
    parser.add_argument("--no-build", action="store_true", help="reuse a matching versioned build and receipt")
    parser.add_argument("--checks", type=Path, help="completed verification results JSON to carry with the release")
    parser.add_argument("--no-smk", action="store_true", help="the FM-1 package alone, without the SMK-37's")
    args = parser.parse_args(argv)
    product = release_product(args.version)
    if source_state(ROOT)["dirty"]:
        raise SystemExit("Commit all source changes before preparing a public release.")
    if (ROOT / "RELEASE_NOTES.md").read_text().splitlines()[0] != f"# Saadet {args.version}":
        raise SystemExit("Update RELEASE_NOTES.md for the selected release version first.")
    output = (args.out or ROOT / "build/releases" / args.version).resolve()
    if output.exists():
        raise SystemExit(f"{output} already exists; choose a new release output directory.")
    if not args.no_build:
        env = dict(os.environ, PYTHON=sys.executable)
        subprocess.run([str(ROOT / "build.sh"), "--release", args.version], cwd=ROOT, env=env, check=True)
        for model, _, _ in () if args.no_smk else SMK:
            subprocess.run([str(ROOT / "build.sh"), "--board", "smk37", "--model", model, "--release", args.version],
                           cwd=ROOT, env=env, check=True)
    package = ROOT / "platform/build" / f"saadet-{args.version}.fwsc"
    receipt = validate_build(ROOT, package, args.version)
    if receipt["product"] != product:
        raise SystemExit("Release identity differs from the selected version.")
    smk = []                                            # (identity's model, package, receipt)
    for model, suffix, name in () if args.no_smk else SMK:
        path = ROOT / "platform/build" / f"smk37-{model}" / f"saadet-smk37{suffix}-{args.version}.fwsc"
        smk_receipt = validate_build(ROOT, path, args.version)
        if smk_receipt["product"] != smk_product(name, args.version):
            raise SystemExit(f"{path}: identity {smk_receipt['product']} differs from the selected version.")
        smk.append((name, path, smk_receipt))
    base = ROOT / "build/deps" / ("fm1-base-" + receipt["dependencies"]["fm1-base"][:12])
    checks = None
    if args.checks:
        checks = json.loads(args.checks.read_text())
        if not checks or any(item.get("exit_code") != 0 for item in checks):
            raise SystemExit("Release verification contains failed or incomplete checks.")
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".saadet-release-", dir=output.parent) as temporary:
        staging = Path(temporary) / "release"
        staging.mkdir()
        copied = staging / package.name
        shutil.copyfile(package, copied)
        build_info = staging / "BUILD_INFO.json"
        build_info.write_text(json.dumps(receipt, indent=2) + "\n")
        source = staging / f"saadet-{args.version}-source.zip"
        make_source_archive(ROOT, args.version, source, receipt, base, [r for _, _, r in smk], smk_source())
        smk_assets = []
        for name, path, smk_receipt in smk:
            asset = staging / f"saadet-{SMK_FILES[name]}-{args.version}.fwsc"
            shutil.copyfile(path, asset)
            (staging / f"BUILD_INFO-{SMK_FILES[name]}.json").write_text(json.dumps(smk_receipt, indent=2) + "\n")
            smk_assets.append({"model": name, "product": smk_receipt["product"], "firmware": asset.name})
        make_site([str(copied), args.version, str(staging / "site"),
                   "--source", str(source), "--build-info", str(build_info),
                   *[a for _, path, _ in smk for a in ("--smk", str(path))]])
        shutil.copyfile(ROOT / "RELEASE_NOTES.md", staging / "RELEASE_NOTES.md")
        for name in ("LICENSE", "LICENSING.md"):
            shutil.copyfile(ROOT / name, staging / name)
        shutil.copytree(staging / "site/firmware/LICENSES", staging / "LICENSES")
        if checks:
            (staging / "VERIFICATION.json").write_text(json.dumps(checks, indent=2) + "\n")
        site_zip = staging / f"saadet-{args.version}-site.zip"
        with zipfile.ZipFile(site_zip, "w", compression=zipfile.ZIP_DEFLATED) as archive:
            for path in sorted((staging / "site").rglob("*")):
                if path.is_file():
                    archive.write(path, path.relative_to(staging / "site"))
        release = {"version": args.version, "product": product, "commit": receipt["source"]["commit"],
                   "firmware": copied.name, "smk37": smk_assets, "source": source.name, "site": site_zip.name,
                   "published": False, "installed": False,
                   "hardware_acceptance": "pending; see RELEASE_NOTES.md"}
        (staging / "RELEASE_INFO.json").write_text(json.dumps(release, indent=2) + "\n")
        (staging / "SHA256SUMS").write_text("".join(
            f"{sha256(path.read_bytes())}  {path.relative_to(staging).as_posix()}\n"
            for path in sorted(staging.rglob("*")) if path.is_file() and path != staging / "SHA256SUMS"))
        validate_build(ROOT, package, args.version)
        for _, path, _ in smk:
            validate_build(ROOT, path, args.version)
        shutil.move(str(staging), output)
    print(f"Prepared Saadet {args.version}: {output}")
    return output


if __name__ == "__main__":
    main()
