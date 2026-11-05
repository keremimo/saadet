#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Retired firmware commands must reject requests before building or installing anything."""
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
SMK_PACKAGES = (("pro", "saadet-smk37.fwsc", "SMK-37 Pro_900", "Saadet Pro"),
                ("elite", "saadet-smk37-elite.fwsc", "SMK-37 Elite_900", "Saadet Elite"),
                ("mke", "saadet-smk37-mke.fwsc", "MKE-P37_900", "Saadet MKE"))


class BuildTargetsTest(unittest.TestCase):
    def test_legacy_build_is_unavailable(self):
        self.assertFalse((ROOT / "tools/build.py").exists())
        for command in (["sh", "build.sh", "--legacy"],
                        [sys.executable, "tools/build_base.py", "--legacy"]):
            with self.subTest(command=command):
                result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("retired", result.stdout + result.stderr)

    def test_legacy_flash_option_is_unavailable(self):
        result = subprocess.run(["sh", "tools/flash.sh", "--legacy"], cwd=ROOT, text=True, capture_output=True)
        self.assertEqual(result.returncode, 2)
        self.assertIn("unknown option", result.stdout + result.stderr)

    def test_old_package_and_release_commands_cannot_write(self):
        for tool in ("tools/fm1pkg_make.py", "tools/prepare_release.py"):
            with self.subTest(tool=tool):
                result = subprocess.run([sys.executable, tool], cwd=ROOT, text=True, capture_output=True)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("retired", result.stdout + result.stderr)


class BoardArgumentsTest(unittest.TestCase):
    def fails(self, *args):
        result = subprocess.run([sys.executable, "tools/build_base.py", *args], cwd=ROOT, text=True, capture_output=True)
        self.assertNotEqual(result.returncode, 0)
        return result.stdout + result.stderr

    def test_a_board_needs_its_model(self):
        self.assertIn("--model", self.fails("--board", "smk37"))
        self.assertIn("--board smk37", self.fails("--model", "elite"))
        self.assertIn("invalid choice", self.fails("--board", "nope"))
        self.assertIn("invalid choice", self.fails("--board", "smk37", "--model", "nope"))

    def test_the_fm1_pin_stays_the_one_the_site_reads(self):
        rev = re.search(r'REV="([0-9a-f]{12})', (ROOT / "tools/build_base.py").read_text())[1]
        self.assertEqual(rev, "3c4a57eea659")


class BluetoothStackTest(unittest.TestCase):
    def test_the_build_asks_for_the_own_stack(self):
        toml = (ROOT / "platform/fm1.toml").read_text()
        self.assertIn('ble_stack = "own"', toml)
        self.assertIn("boot_pack = true", toml)

    def test_the_last_build_links_nothing_of_jielis(self):
        for symbols in (ROOT / "platform/build/saadet.symbols", *(ROOT / "platform/build").glob("smk37-*/saadet.symbols")):
            if not symbols.exists():
                continue
            text = symbols.read_text()
            with self.subTest(file=str(symbols.relative_to(ROOT))):
                for name in ("btstack_task", "ble_op_disconnect", "wf_rf_common_init", "bt_ble_init"):
                    self.assertNotRegex(text, r"\s" + name + r"\n")


class OverlayTest(unittest.TestCase):
    def test_a_patch_that_no_longer_matches_fails(self):
        from base_overlay import replace, require, smk37_linker, smk37_toml
        for call in (lambda: replace("text", "absent", "x"), lambda: require("text", "absent"),
                     lambda: smk37_linker("no such script"), lambda: smk37_toml("no such toml", "elite", "Elite")):
            with self.assertRaises(ValueError):
                call()

    def test_the_toml_variant_is_the_models(self):
        from base_overlay import smk37_toml
        text = smk37_toml((ROOT / "platform/fm1.toml").read_text(), "elite", "Elite")
        self.assertTrue(text.startswith('board = "smk37"\nmodel = "elite"\n'))
        self.assertIn('usb_product = "Saadet Elite"', text)
        self.assertIn('-DSAADET_SMK37=1', text)


class SmkLightsTest(unittest.TestCase):
    def test_the_led_cells_are_the_uis(self):
        app = (ROOT / "platform/app.c").read_text()
        instrument = (ROOT / "platform/instrument.c").read_text()
        cells = [int(x) for x in re.search(r"sd_btn_cell\[14\]=\{([^}]+)\}", app)[1].split(",")]
        ids = [int(x) for x in re.search(r"bm\[14\]=\{([^}]+)\}", app)[1].split(",")]
        keymap = [[int(x) for x in row.split(",")] for row in
                  re.findall(r"\{(-?\d+(?:,-?\d+){10})\}", instrument[instrument.index("FM1_KEYMAP"):])[:6]]
        self.assertEqual(len(keymap), 6)
        for label, (cell, id_) in enumerate(zip(cells, ids)):
            self.assertEqual(keymap[cell & 7][cell >> 3], id_, f"label {label}")


@unittest.skipUnless(os.environ.get("SAADET_REQUIRE_SMK37") or (ROOT / "build/saadet-smk37-elite.fwsc").exists(),
                     "SMK-37 packages not built (tests/run_smk37_tests.sh builds and requires them)")
class SmkPackagesTest(unittest.TestCase):
    def test_each_model_has_its_package(self):
        from fm1_install import product_of
        for model, name, identity, product in SMK_PACKAGES:
            with self.subTest(model=model):
                out = ROOT / "platform/build" / f"smk37-{model}"
                package = (ROOT / "build" / name).read_bytes()
                self.assertEqual(package, (out / name).read_bytes())
                self.assertEqual(product_of(package), identity)
                self.assertLessEqual(len(identity), 20)
                image = (out / "saadet.bin").read_bytes()
                self.assertLessEqual(len(image), 0x97FBC)
                self.assertIn(f"{product} SMK-37".encode(), image)
                self.assertIn(f"SAADET {product.split()[1].upper()}".encode(), image)
                self.assertIn(product.encode(), image)
                self.assertNotIn(b"Saadet FM-1", image)
                receipt = json.loads((out / (Path(name).stem + ".build.json")).read_text())
                self.assertEqual(receipt["dependencies"]["fm1-base"], "0868ac55e03896d0fae56f5fbeb0ba11a28174d9")
                self.assertEqual(receipt["dependencies"]["felucca-bsp"], "33878ab51d452beacf9268edc2f6a6a7bec29a79")
                self.assertEqual(receipt["product"], identity)

    def test_the_instrument_was_built_for_the_smk_map(self):
        # instrument.c is its own unit: built without SAADET_SMK37 it would hold the FM-1's addresses, silently.
        for model, _, _, _ in SMK_PACKAGES:
            with self.subTest(model=model):
                ir = (ROOT / "platform/build" / f"smk37-{model}/o2/instrument.ll").read_text()
                self.assertIn(f"i32 {0xEB000}", ir)          # the SMK's settings
                self.assertNotIn(f"i32 {0xFC000}", ir)       # the FM-1's, inside the SMK's protected USR window

    def test_a_package_never_passes_as_the_fm1s(self):
        from fm1_install import product_of
        for _, name, _, _ in SMK_PACKAGES:
            self.assertFalse(product_of((ROOT / "build" / name).read_bytes()).startswith("FM-1"))


if __name__ == "__main__":
    unittest.main()
