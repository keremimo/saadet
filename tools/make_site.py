#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Make Saadet's release site from a release package (static files for any web host):

  index.html                    a redirect to the installer
  install/index.html            web/install/index.html, its modules bundled into the page: one file, so a cached
                                or half-loaded page can never mix two releases' code during an install
  install/release.json          the version, the package identity, file names and SHA-256s the page checks
  install/fakefm1.js, fm1ota.js, fm1ports.js the simulated FM-1 for ?demo (loaded only there)
  firmware/saadet-VERSION.fwsc  the package; also the download
  firmware/saadet-smk37-pro-VERSION.fwsc, -smk37-elite-, -mke-p37-   the SMK-37 packages given with --smk (beta)
  firmware/m-bootloader-X.Y.fwsc, m-bootloader-LICENSE.txt   the pinned m-bootloader (MIT), offered by the installer
  firmware/SHA256SUMS           every file under firmware/
  firmware/saadet-VERSION-source.zip, SOURCES.txt, LICENSE, LICENSING.md, LICENSES/   matching source and notices
  editor.html, fm1backup.js, fukiai.ttf, FUKIAI-LICENSE.txt, manual/   the editor and the manual, as in web/

  tools/prepare_public_release.py 0.1.0    (build and prepare all public artifacts)
  tools/make_site.py PACKAGE VERSION SITE --source SOURCE.zip --build-info BUILD_INFO.json [--smk SMK.fwsc ...]
  tools/make_site.py --dev platform/build/saadet.fwsc dev build/site     (a development package, for testing)

A release package comes from ./build.sh --release X.Y[.Z]: its identity is FM-1_9 and XXYYZZ, which the page
checks the download against and the FM-1 reports after the install. An SMK-37 package (./build.sh --board smk37
--model pro|elite|mke --release X.Y[.Z], its receipt beside it) has its model's name, _9 and the major and minor
version one digit each ("SMK-37 Elite_910": three digits at most, for compatibility); the page installs the package
whose model the connected device's identity names. The site refuses a package without the base's
update loader, installer modules that differ from the pinned fm1-base's, and m-bootloader files that differ
from the pinned release. Publishing the BLE build is a separate decision: see docs/BLE_MIGRATION.md.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import html
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from base_bootloader import ARTIFACTS, HASHES, PACKAGE as BOOT_PACKAGE, VERSION as BOOT_VERSION, checked_files  # noqa: E402
from fm1_install import product_of  # noqa: E402
from release_support import validate_build, validate_source_archive  # noqa: E402

INSTALL = ROOT / "web/install"
# the installer's modules, each after those it imports; fakefm1.js stays a file of its own (the demo's)
MODULES = ("fm1pkg.js", "fm1ports.js", "fm1ota.js", "mboot.js", "flow.js", "transfer.js", "keepalive.js", "installer.js")
VENDORED = ("fm1pkg.js", "fm1ports.js", "fm1ota.js")          # fm1-base's, unchanged
WEB = ("editor.html", "fm1backup.js", "fukiai.ttf", "FUKIAI-LICENSE.txt")

IMPORT = re.compile(r'^import\s*\{([^}]*)\}\s*from\s*"\./([\w.]+)";[ \t]*\n', re.M)
EXPORT = re.compile(r"^export\s+(?=(?:async\s+)?function|class|const|let)", re.M)
EXPORTED = re.compile(r"^export\s+(?:async\s+)?(?:function\*?|class|const|let)\s+([A-Za-z_$][\w$]*)", re.M)


def sha256(data):
    return hashlib.sha256(data).hexdigest()


# the SMK-37 packages a site carries: the identity's model -> the download's name (saadet-NAME-VERSION.fwsc)
SMK_FILES = {"SMK-37 Pro": "smk37-pro", "SMK-37 Elite": "smk37-elite", "MKE-P37": "mke-p37"}


def release_product(version):
    m = re.fullmatch(r"(\d{1,2})\.(\d{1,2})(?:\.(\d{1,2}))?(-[A-Za-z0-9]+)?", version)
    if not m:
        raise SystemExit(f"version {version!r}: use X.Y[.Z][-suffix], up to two digits each")
    return "FM-1_9%02d%02d%02d" % (int(m[1]), int(m[2]), int(m[3] or 0))


def smk_product(model, version):
    """an SMK-37 release's identity: three digits at most after the underscore, 9 and X and Y ("SMK-37 Elite_910" for
    1.0 and 1.0.1); "dev": the development build's (MODEL_900)"""
    if version == "dev":
        return model + "_900"
    m = re.fullmatch(r"(\d{1,2})\.(\d{1,2})(?:\.(\d{1,2}))?(-[A-Za-z0-9]+)?", version)
    if not m or int(m[1]) > 9 or int(m[2]) > 9:
        raise SystemExit(f"version {version!r}: an SMK-37 identity has one digit each for the major and minor")
    return "%s_9%d%d" % (model, int(m[1]), int(m[2]))


def pinned_base():
    rev = re.search(r'REV="([0-9a-f]{12})', (ROOT / "tools/build_base.py").read_text())[1]
    path = ROOT / "build/deps" / f"fm1-base-{rev}" / "web"
    if not path.is_dir():
        raise SystemExit(f"no {path}: run ./build.sh once to fetch the pinned fm1-base")
    return rev, path


def bundle(names):
    """the modules as one script: each in its own scope, its imports taken from the ones before it"""
    var = lambda name: "__" + re.sub(r"\W", "_", name)  # noqa: E731
    out, done = [], set()
    for name in names:
        src = (INSTALL / name).read_text(encoding="utf-8")
        head = []
        for wanted, dep in IMPORT.findall(src):
            if dep not in done:
                raise SystemExit(f"web/install/{name} imports {dep} before it is bundled")
            head.append(f"const {{ {' '.join(wanted.split())} }} = {var(dep)};\n")
        body = EXPORT.sub("", IMPORT.sub("", src))
        if re.search(r"^\s*(import|export)\b", body, re.M):
            raise SystemExit(f"web/install/{name}: an import or export the bundler does not handle")
        exported = ", ".join(EXPORTED.findall(src))
        out.append(f"/* ---- {name} ---- */\nconst {var(name)} = (() => {{\n{''.join(head)}{body}\n"
                   f"return {{ {exported} }};\n}})();\n")
        done.add(name)
    text = "\n".join(out)
    if "</script" in text.lower():
        raise SystemExit("a module contains </script>")
    return text


def copy_licences(destination, base):
    """Keep application, dependency and font notices alongside the downloads."""
    for name in ("LICENSE", "LICENSING.md"):
        shutil.copyfile(ROOT / name, destination / name)
    shutil.copytree(ROOT / "LICENSES", destination / "LICENSES")
    for label, directory in (("fm1-base", base), ("felucca-bsp", base / "bsp")):
        target = destination / "LICENSES/upstream" / label
        target.mkdir(parents=True)
        for name in ("LICENSE", "LICENSING.md", "NOTICE"):
            if (directory / name).is_file():
                shutil.copyfile(directory / name, target / name)
        if (directory / "LICENSES").is_dir():
            shutil.copytree(directory / "LICENSES", target / "LICENSES")
    for name in ("Barlow-OFL.txt", "Terminus-LICENSE.txt"):
        shutil.copyfile(base / "assets/fonts" / name, destination / "LICENSES/upstream/fm1-base" / name)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("package", type=Path)
    ap.add_argument("version")
    ap.add_argument("out", type=Path)
    ap.add_argument("--dev", action="store_true", help="accept a development package (FM-1_900), for testing only")
    ap.add_argument("--source", type=Path, help="matching source ZIP (required for public releases)")
    ap.add_argument("--build-info", type=Path, help="firmware build receipt; defaults to PACKAGE.build.json")
    ap.add_argument("--smk", type=Path, action="append", default=[],
                    help="an SMK-37 package (Pro, Elite or MKE-P37) to carry too, its receipt as PACKAGE.build.json")
    a = ap.parse_args(argv)

    raw = a.package.read_bytes()
    product = product_of(raw)
    want = "FM-1_900" if a.dev else release_product(a.version)
    if product != want:
        hint = "" if a.dev else f" (build it with ./build.sh --release {a.version})"
        raise SystemExit(f"{a.package}: identity {product}, expected {want}{hint}")
    if b"FELUCCA-LOADER-1" not in raw:
        raise SystemExit(f"{a.package}: no update loader in it: not an fm1-base package")
    smk = {}                                    # model -> (identity, bytes, path)
    for path in a.smk:
        sraw = path.read_bytes()
        sprod = product_of(sraw)
        model = sprod.split("_")[0]
        if model not in SMK_FILES:
            raise SystemExit(f"{path}: identity {sprod}: not an SMK-37 package (SMK-37 Pro, SMK-37 Elite or MKE-P37)")
        swant = smk_product(model, "dev" if a.dev else a.version)
        if sprod != swant:
            raise SystemExit(f"{path}: identity {sprod}, expected {swant}")
        if b"FELUCCA-LOADER-1" not in sraw:
            raise SystemExit(f"{path}: no update loader in it: not an fm1-base package")
        if model in smk:
            raise SystemExit(f"{path}: a second package for the {model}")
        smk[model] = (sprod, sraw, path)
    rev, base = pinned_base()
    for name in VENDORED:
        if sha256((INSTALL / name).read_bytes()) != sha256((base / name).read_bytes()):
            raise SystemExit(f"web/install/{name} differs from fm1-base {rev}: copy it from {base}")
    boot = checked_files(ARTIFACTS)[BOOT_PACKAGE]
    if product_of(boot) != "FM-1_099":
        raise SystemExit("m-bootloader: wrong package identity")

    source_info = None
    if not a.dev:
        receipt = validate_build(ROOT, a.package, a.version, a.build_info)
        smk_receipts = [validate_build(ROOT, path, a.version) for _, _, path in smk.values()]
        if not a.source:
            raise SystemExit("A public release needs --source with its matching source archive.")
        source_info = validate_source_archive(a.source, a.version, receipt, smk_receipts)

    out = a.out
    if out.exists():
        if not (out / "install/release.json").exists():
            raise SystemExit(f"{out} exists and is not a site this tool made: choose another directory")
        shutil.rmtree(out)
    (out / "install").mkdir(parents=True)
    (out / "firmware").mkdir()

    fw = f"saadet-{re.sub(r'[^A-Za-z0-9.-]', '-', a.version)}.fwsc"
    bootname = f"m-bootloader-{BOOT_VERSION}.fwsc"
    (out / "firmware" / fw).write_bytes(raw)
    packages = [{"model": "FM-1", "product": product, "pkg": f"../firmware/{fw}", "sha256": sha256(raw), "size": len(raw)}]
    for model, (sprod, sraw, _) in sorted(smk.items()):
        name = f"saadet-{SMK_FILES[model]}-{re.sub(r'[^A-Za-z0-9.-]', '-', a.version)}.fwsc"
        (out / "firmware" / name).write_bytes(sraw)
        packages.append({"model": model, "product": sprod, "pkg": f"../firmware/{name}", "sha256": sha256(sraw),
                         "size": len(sraw), "beta": True})
    (out / "firmware" / bootname).write_bytes(boot)
    shutil.copyfile(ARTIFACTS / "LICENSE", out / "firmware/m-bootloader-LICENSE.txt")
    copy_licences(out / "firmware", base.parent)
    source_name = f"saadet-{a.version}-source.zip"
    if source_info:
        shutil.copyfile(a.source, out / "firmware" / source_name)
        source_notice = (f"Saadet {a.version}\nFirmware source: {source_name}\n"
                         f"Application commit: {source_info['commit']}\nSource SHA-256: {source_info['sha256']}\n"
                         "The ZIP includes matching Saadet, fm1-base and felucca-bsp source"
                         + (", and the SMK-37 board layer's (dependencies/fm1-base-smk37/)" if smk else "") + ".\n"
                         "MPL-covered files are available under MPL-2.0; other files retain their own terms.\n")
    else:
        source_notice = "Development preview. Public releases include a matching source ZIP here.\n"
    (out / "firmware/SOURCES.txt").write_text(source_notice + "Read LICENSE, LICENSING.md and LICENSES/.\n")
    (out / "firmware/SHA256SUMS").write_text("".join(
        f"{sha256(p.read_bytes())}  {p.relative_to(out / 'firmware').as_posix()}\n"
        for p in sorted((out / "firmware").rglob("*")) if p.is_file() and p.name != "SHA256SUMS"))

    commit = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True).stdout.strip()
    dirty = bool(subprocess.run(["git", "status", "--porcelain", "--", "firmware", "platform", "web", "tools"],
                                cwd=ROOT, capture_output=True, text=True).stdout.strip())
    meta = {
        "name": "Saadet", "version": a.version, "product": product, "dev": a.dev,
        "pkg": f"../firmware/{fw}", "sha256": sha256(raw), "size": len(raw), "packages": packages,
        "boot": {"version": BOOT_VERSION, "pkg": f"../firmware/{bootname}", "sha256": HASHES[BOOT_PACKAGE],
                 "licence": "MIT", "source": "https://github.com/jvitkauskas/m-bootloader"},
        "commit": commit + ("+dirty" if dirty else ""), "base": rev,
        "built": datetime.now(timezone.utc).replace(microsecond=0).isoformat(),
    }
    if source_info:
        meta["source"] = {**source_info, "zip": f"../firmware/{source_name}"}
    meta["licences"] = "../firmware/LICENSING.md"
    (out / "install/release.json").write_text(json.dumps(meta, indent=2) + "\n")

    page = (INSTALL / "index.html").read_text(encoding="utf-8")
    tag = '<script type="module" src="installer.js"></script>'
    if page.count(tag) != 1:
        raise SystemExit(f"web/install/index.html must load installer.js once: {tag}")
    downloads = '<p><a href="../firmware/LICENSING.md">Licences</a> · <a href="../firmware/SOURCES.txt">Source information</a>'
    if source_info:
        downloads += f' · <a href="../firmware/{html.escape(source_name, quote=True)}" download>Download source</a>'
    page = page.replace("<!-- RELEASE_DOWNLOADS -->", downloads + "</p>")
    (out / "install/index.html").write_text(page.replace(tag, f'<script type="module">\n{bundle(MODULES)}</script>'),
                                            encoding="utf-8")
    for name in ("fakefm1.js", "fm1ports.js", "fm1ota.js"):
        shutil.copyfile(INSTALL / name, out / "install" / name)

    for name in WEB:
        shutil.copyfile(ROOT / "web" / name, out / name)
    shutil.copytree(ROOT / "web/manual", out / "manual")
    # The manual moves from web/manual/ to manual/ in the published site.
    # Link to the matching public source instead of a repository-relative path.
    manual = out / "manual/index.html"
    manual.write_text(manual.read_text().replace("../../docs/UI.md",
                      f"https://github.com/keremimo/saadet/blob/{commit or 'main'}/docs/UI.md"))
    (out / ".nojekyll").touch()
    (out / "index.html").write_text('<!doctype html><meta charset="utf-8"><title>Saadet</title>'
                                    '<meta http-equiv="refresh" content="0; url=install/">'
                                    '<a href="install/">Install Saadet</a>\n')
    print(f"site: {out}: install/ ({', '.join(p['product'] for p in packages)}, {a.version}{', dev' if a.dev else ''}), "
          f"firmware/{fw} ({len(raw)} B){f' and {len(smk)} SMK-37' if smk else ''}, "
          f"m-bootloader {BOOT_VERSION}, editor, manual" + ("; uncommitted changes in the tree" if dirty else ""))


if __name__ == "__main__":
    main()
