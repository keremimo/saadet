#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Prepare/check m-bootloader, or explicitly install it on running Saadet.

Uses fm1-base's MIDI transport and the bootloader-only staging sequence
documented by its Updater.installBoot. The bootloader itself is unmodified
MIT code by Julius Vitkauskas, https://github.com/jvitkauskas/m-bootloader:
its release files are kept in platform/m-bootloader/ (fm1-base's pin carries 1.2).
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import shutil
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "platform/m-bootloader"
ARTIFACTS = ROOT / "platform/build/bootloader"
BASE = ROOT / "build/fm1-base-platform"
VERSION = "2.2"
COMMIT = "64f7c9cad1580bea870fc4a9e9bd61583fee3aef"
PACKAGE = f"fm1-uboot-{VERSION}.fwsc"
BOOT = f"fm1-uboot-{VERSION}.boot"
# Saadet's identities: the development build and releases (fm1-base: id_prefix FM-1_9, then XXYYZZ).
SAADET = re.compile(r"FM-1_9(00|\d{6})")
# The v2.2 release's own SHA256SUMS (fm1-uboot.fwsc, fm1-uboot.boot).
HASHES = {
    PACKAGE: "cb6128ee753a1661e6399002317464959231854024f259520755621954438981",
    BOOT: "cedc9c17648bdeecda5c8d0ffffe2211849f8a36ab615fe49d6cc46d249baf44",
}


def checked_files(directory):
    result = {}
    for name, want in HASHES.items():
        data = (directory / name).read_bytes()
        if hashlib.sha256(data).hexdigest() != want:
            raise ValueError(f"{name}: m-bootloader v{VERSION} checksum mismatch")
        result[name] = data
    # Keep the actual third-party licence beside every prepared copy.
    licence = (directory / "LICENSE").read_text()
    if "MIT License" not in licence or "Copyright (c) 2026 Julius Vitkauskas" not in licence:
        raise ValueError("m-bootloader licence/attribution is missing")
    return result


def prepare(destination, source=SOURCE):
    checked_files(source)
    destination.mkdir(parents=True, exist_ok=True)
    for stale in destination.glob("fm1-uboot-*"):   # an earlier pin's files
        if stale.name not in HASHES:
            stale.unlink()
    for name in (*HASHES, "LICENSE"):
        shutil.copyfile(source / name, destination / name)
    (destination / "manifest.json").write_text(json.dumps({
        "version": VERSION,
        "source": f"https://github.com/jvitkauskas/m-bootloader/releases/tag/v{VERSION}", "commit": COMMIT,
        "licence": "MIT", "copyright": "2026 Julius Vitkauskas",
        "sha256": HASHES,
    }, indent=2) + "\n")


def load_updater():
    path = BASE / "tools/fm1_install.py"
    spec = importlib.util.spec_from_file_location("saadet_base_updater", path)
    module = importlib.util.module_from_spec(spec)
    sys.path.insert(0, str(BASE / "tools"))   # its own modules (fm1ports)
    spec.loader.exec_module(module)
    return module


def install_bootloader(api, up, dev, image, say=print):
    """One staging transaction, then the original application must return.

    A bootloader-only installer has no MIDI update-loader phase: using
    Updater.install() would incorrectly wait for that loader and FM-1_099.
    """
    product = dev.id.text
    if dev.id.loader or dev.id.open or not SAADET.fullmatch(product):
        dev.link.close()
        raise ValueError(f"Bootloader installation requires running Saadet (FM-1_900 or a release), not {product}; "
                         "update Saadet first")
    say(f"Staging m-bootloader {VERSION}. Keep the FM-1 powered and connected.")
    try:
        if not dev.link.send(api.UPGRADE):
            raise ValueError("Could not start bootloader staging")
        time.sleep(api.DELAY["start"])
        served, finished, lost = up.serve(dev.link, image, api.FINISH_CHECK,
                                        api.DELAY["idle_check"], lambda *_: None)
    finally:
        dev.link.close()
    if not finished:
        raise ValueError(f"Bootloader staging did not complete ({served} reads, disconnected={lost}). "
                         "Do not assume it was installed; check the device before retrying.")
    say("Staging accepted; waiting for the bootloader installer to restart Saadet.")
    # Return of the app verifies restart, not the boot area's contents.
    result = up.verify(product, lambda *_: None)
    say(f"Application returned: {result}. Check PLAY at power-on to verify the recovery menu.")
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    action = parser.add_mutually_exclusive_group()
    action.add_argument("--check", action="store_true", help="check prepared files only (the default; no MIDI)")
    action.add_argument("--install", action="store_true", help="install the pinned bootloader over USB MIDI")
    parser.add_argument("--port", default="FM-1", help="USB MIDI port (default FM-1)")
    parser.add_argument("--yes", action="store_true", help="skip the boot-area write confirmation")
    args = parser.parse_args(argv)
    try:
        checked_files(ARTIFACTS)
        print(f"m-bootloader {VERSION}: FM-1 package and boot image checksums verified")
        if not args.install:
            return 0
        api = load_updater()
        product, image, _ = api.load_package(ARTIFACTS / PACKAGE, True)
        if product != "FM-1_099":
            raise ValueError("Wrong bootloader installer identity")
        # The exact pinned hash is required above; there is no arbitrary --force package path.
        up = api.Updater(api.MidoBackend(), args.port)
        dev = up.find()
        if not dev:
            raise api.not_found(up)
        try:
            if not args.yes and input("Replace the FM-1 bootloader? Keep power connected. [y/N] ").lower() not in ("y", "yes"):
                print("Cancelled")
                return 1
            install_bootloader(api, up, dev, image)
        finally:
            dev.link.close()
        return 0
    except EOFError:
        print("Cancelled: no confirmation supplied", file=sys.stderr)
        return 1
    except Exception as error:
        print(f"error: {error}", file=sys.stderr)
        return 2
    except KeyboardInterrupt:
        print("Interrupted; check the FM-1 before any retry.", file=sys.stderr)
        return 130


if __name__ == "__main__":
    sys.exit(main())
