#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""The release site (tools/make_site.py) from the base port's package: its files, release.json, the bundled
installer page, and the packages it refuses. Needs platform/build/saadet.fwsc, the pinned fm1-base and node."""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import make_site as S  # noqa: E402

PKG = ROOT / "platform/build/saadet.fwsc"


class SiteTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.out = Path(cls.tmp.name) / "site"
        S.main(["--dev", str(PKG), "dev", str(cls.out)])
        cls.meta = json.loads((cls.out / "install/release.json").read_text())

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_files_and_checksums(self):
        o, m = self.out, self.meta
        for f in (".nojekyll", "index.html", "editor.html", "manual/index.html", "install/fakefm1.js", "install/fm1ota.js", "install/fm1ports.js",
                  "firmware/m-bootloader-LICENSE.txt", "firmware/SHA256SUMS", "firmware/LICENSE",
                  "firmware/LICENSING.md", "firmware/SOURCES.txt", "firmware/LICENSES/MPL-2.0.txt",
                  "firmware/LICENSES/GPL-3.0-only.txt", "firmware/LICENSES/upstream/fm1-base/LICENSE",
                  "firmware/LICENSES/upstream/fm1-base/Barlow-OFL.txt",
                  "firmware/LICENSES/upstream/felucca-bsp/NOTICE"):
            self.assertTrue((o / f).is_file(), f)
        fw = (o / "install" / m["pkg"]).resolve().read_bytes()
        boot = (o / "install" / m["boot"]["pkg"]).resolve().read_bytes()
        self.assertEqual(hashlib.sha256(fw).hexdigest(), m["sha256"])
        self.assertEqual(fw, PKG.read_bytes())
        self.assertEqual(hashlib.sha256(boot).hexdigest(), m["boot"]["sha256"])
        self.assertEqual((m["product"], m["dev"], m["boot"]["version"]), ("FM-1_900", True, "1.2"))
        sums = (o / "firmware/SHA256SUMS").read_text()
        for p in (o / "firmware").rglob("*"):
            if p.is_file() and p.name != "SHA256SUMS":
                self.assertIn(f"{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.relative_to(o / 'firmware').as_posix()}", sums)
        self.assertEqual((o / "firmware/LICENSE").read_bytes(), (ROOT / "LICENSE").read_bytes())
        self.assertEqual((o / "firmware/LICENSING.md").read_bytes(), (ROOT / "LICENSING.md").read_bytes())
        manual = (o / "manual/index.html").read_text()
        self.assertNotIn("../../docs/UI.md", manual)
        self.assertIn("https://github.com/keremimo/saadet/blob/", manual)

    def test_page_is_one_bundled_module(self):
        page = (self.out / "install/index.html").read_text()
        self.assertNotIn('src="installer.js"', page)
        self.assertIn('href="../firmware/LICENSING.md"', page)
        self.assertIn('href="../firmware/SOURCES.txt"', page)
        self.assertIn("Firmware: MPL-2.0", page)
        scripts = re.findall(r'<script type="module">\n(.*?)</script>', page, re.S)
        self.assertEqual(len(scripts), 1)
        for name in S.MODULES:
            self.assertIn(f"/* ---- {name} ---- */", scripts[0])
        js = Path(self.tmp.name) / "bundle.mjs"
        js.write_text(scripts[0])
        subprocess.run(["node", "--check", str(js)], check=True)

    def test_refusals(self):
        with tempfile.TemporaryDirectory() as d:
            with self.assertRaisesRegex(SystemExit, "expected FM-1_9010000"):
                S.main([str(PKG), "1.0", str(Path(d) / "a")])          # a development package as a release
            with self.assertRaisesRegex(SystemExit, "X.Y"):
                S.main([str(PKG), "one", str(Path(d) / "b")])
            (Path(d) / "c").mkdir()
            (Path(d) / "c/keep.txt").write_text("not a site")
            with self.assertRaisesRegex(SystemExit, "not a site"):
                S.main(["--dev", str(PKG), "dev", str(Path(d) / "c")])
            self.assertTrue((Path(d) / "c/keep.txt").exists())

    def test_release_identity(self):
        self.assertEqual(S.release_product("1.0"), "FM-1_9010000")
        self.assertEqual(S.release_product("1.2.3-rc1"), "FM-1_9010203")
        self.assertEqual(S.release_product("0.1.0"), "FM-1_9000100")


if __name__ == "__main__":
    unittest.main()
