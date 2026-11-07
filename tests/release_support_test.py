#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""A public release must match committed source and omit private/generated files."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import release_support as R


def command(root, *args):
    return subprocess.check_output(["git", "-C", str(root), *args], stderr=subprocess.STDOUT)


def repository(path, files):
    path.mkdir(parents=True)
    command(path, "init", "-q")
    for name, text in files.items():
        (path / name).write_text(text)
    command(path, "add", ".")
    command(path, "-c", "user.name=Release Test", "-c", "user.email=release@example.invalid", "commit", "-qm", "fixture")
    return path


class ReleaseSupportTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.folder = Path(self.temp.name)
        self.root = repository(self.folder / "app", {"LICENSE": "MPL-2.0", "app.c": "original code\n",
                               ".gitignore": "build/\nprivate/\n"})
        self.base = repository(self.folder / "base", {"LICENSE": "MPL-2.0", "base.c": "base source\n", ".gitignore": "bsp/\n"})
        self.bsp = repository(self.base / "bsp", {"LICENSE": "MPL-2.0", "chip.c": "bsp source\n"})
        self.package = self.root / "build/saadet-0.1.0.fwsc"
        self.package.parent.mkdir()
        self.package.write_bytes(b"test package")
        self.receipt = R.record_build(self.root, self.package, R.source_state(self.root), "0.1.0", "FM-1_9000100", True,
                                      command(self.base, "rev-parse", "HEAD").decode().strip(),
                                      command(self.bsp, "rev-parse", "HEAD").decode().strip())

    def test_mismatches_are_refused(self):
        self.assertEqual(R.validate_build(self.root, self.package, "0.1.0"), self.receipt)
        with self.assertRaisesRegex(SystemExit, "version"):
            R.validate_build(self.root, self.package, "0.2.0")
        self.package.write_bytes(b"tampered package")
        with self.assertRaisesRegex(SystemExit, "differs"):
            R.validate_build(self.root, self.package, "0.1.0")
        self.package.write_bytes(b"test package")
        (self.root / "app.c").write_text("uncommitted change\n")
        with self.assertRaisesRegex(SystemExit, "Commit all"):
            R.validate_build(self.root, self.package, "0.1.0")

    def test_source_is_complete_and_private_outputs_are_absent(self):
        (self.root / "private").mkdir()
        (self.root / "private/device-capture.bin").write_bytes(b"not public")
        archive = self.folder / "source.zip"
        R.make_source_archive(self.root, "0.1.0", archive, self.receipt, self.base)
        info = R.validate_source_archive(archive, "0.1.0", self.receipt)
        self.assertEqual(info["commit"], self.receipt["source"]["commit"])
        with zipfile.ZipFile(archive) as source:
            names = source.namelist()
            self.assertFalse(any(".git/" in n or "/private/" in n or "/build/" in n for n in names))
            prefix = "saadet-0.1.0-source/"
            self.assertEqual(source.read(prefix + "app.c"), b"original code\n")
            self.assertEqual(source.read(prefix + "dependencies/fm1-base/base.c"), b"base source\n")
            self.assertEqual(source.read(prefix + "dependencies/fm1-base/bsp/chip.c"), b"bsp source\n")
            extracted = self.folder / "extracted"
            source.extractall(extracted)
        self.assertEqual(R.source_state(extracted / prefix), self.receipt["source"])
        with zipfile.ZipFile(archive) as source:
            entries = {name: source.read(name) for name in source.namelist()}
        entries[prefix + "app.c"] = b"wrong source"
        with zipfile.ZipFile(archive, "w") as source:
            for name, data in entries.items():
                source.writestr(name, data)
        with self.assertRaisesRegex(SystemExit, "does not match"):
            R.validate_source_archive(archive, "0.1.0", self.receipt)

    def test_smk37_board_layer_is_archived_with_each_receipt(self):
        smk_base = repository(self.folder / "smk", {"LICENSE": "MPL-2.0", "board.c": "smk board\n", ".gitignore": "bsp/\n"})
        smk_bsp = repository(smk_base / "bsp", {"LICENSE": "MPL-2.0", "smk37.c": "smk bsp\n"})
        revs = [command(r, "rev-parse", "HEAD").decode().strip() for r in (smk_base, smk_bsp)]
        receipts = []
        for name, product in (("saadet-smk37-elite-0.1.0.fwsc", "SMK-37 Elite_901"),
                              ("saadet-smk37-mke-0.1.0.fwsc", "MKE-P37_901")):
            package = self.root / "build" / name
            package.write_bytes(product.encode())
            receipts.append(R.record_build(self.root, package, R.source_state(self.root), "0.1.0", product, True, *revs))
        archive = self.folder / "source.zip"
        R.make_source_archive(self.root, "0.1.0", archive, self.receipt, self.base, receipts, smk_base)
        info = R.validate_source_archive(archive, "0.1.0", self.receipt, receipts)
        self.assertEqual(info["smk37_dependencies"], {"fm1-base": revs[0], "felucca-bsp": revs[1]})
        prefix = "saadet-0.1.0-source/"
        with zipfile.ZipFile(archive) as source:
            self.assertEqual(source.read(prefix + "dependencies/fm1-base-smk37/board.c"), b"smk board\n")
            self.assertEqual(source.read(prefix + "dependencies/fm1-base-smk37/bsp/smk37.c"), b"smk bsp\n")
            self.assertIn(b"fm1-base-smk37", source.read(prefix + "SOURCE_README.txt"))
            self.assertIn(prefix + "SOURCE_INFO-saadet-smk37-mke-0.1.0.json", source.namelist())
        with self.assertRaisesRegex(SystemExit, "different board layer"):
            R.make_source_archive(self.root, "0.1.0", self.folder / "b.zip", self.receipt, self.base,
                                  [receipts[0], {**receipts[1], "dependencies": {"fm1-base": "x", "felucca-bsp": "y"}}],
                                  smk_base)
        with self.assertRaisesRegex(SystemExit, "metadata differs"):
            R.validate_source_archive(archive, "0.1.0", self.receipt, [{**receipts[0], "product": "SMK-37 Pro_901"}])

    def test_source_changed_during_build_has_no_receipt(self):
        before = R.source_state(self.root)
        (self.root / "app.c").write_text("changed while compiling\n")
        R.receipt_path(self.package).unlink()
        with self.assertRaisesRegex(SystemExit, "Source changed"):
            R.record_build(self.root, self.package, before, "0.1.0", "FM-1_9000100", True, "base", "bsp")
        self.assertFalse(R.receipt_path(self.package).exists())


if __name__ == "__main__":
    unittest.main()
