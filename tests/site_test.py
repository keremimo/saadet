#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""The release site (tools/make_site.py) from the base port's package: its files, release.json, the bundled
installer page, and the packages it refuses. Needs platform/build/saadet.fwsc, the pinned fm1-base and node; the
SMK-37 packages (build/saadet-smk37[-elite|-mke].fwsc, tests/run_smk37_tests.sh) when they are there."""
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
SMK = {"SMK-37 Pro": ROOT / "build/saadet-smk37.fwsc", "SMK-37 Elite": ROOT / "build/saadet-smk37-elite.fwsc",
       "MKE-P37": ROOT / "build/saadet-smk37-mke.fwsc"}
SMK_HERE = {m: p for m, p in SMK.items() if p.is_file()}


class SiteTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.out = Path(cls.tmp.name) / "site"
        S.main(["--dev", str(PKG), "dev", str(cls.out), *[a for p in SMK_HERE.values() for a in ("--smk", str(p))]])
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
        self.assertEqual((m["product"], m["dev"], m["boot"]["version"]), ("FM-1_900", True, "2.2"))
        sums = (o / "firmware/SHA256SUMS").read_text()
        for p in (o / "firmware").rglob("*"):
            if p.is_file() and p.name != "SHA256SUMS":
                self.assertIn(f"{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.relative_to(o / 'firmware').as_posix()}", sums)
        self.assertEqual((o / "firmware/LICENSE").read_bytes(), (ROOT / "LICENSE").read_bytes())
        self.assertEqual((o / "firmware/LICENSING.md").read_bytes(), (ROOT / "LICENSING.md").read_bytes())
        manual = (o / "manual/index.html").read_text()
        self.assertNotIn("../../docs/UI.md", manual)
        self.assertIn("https://github.com/keremimo/saadet/blob/", manual)

    def test_packages_per_model(self):
        """release.json lists a package per model, the FM-1's first and the same as its top-level fields"""
        m, o = self.meta, self.out
        self.assertEqual(m["packages"][0], {"model": "FM-1", "product": m["product"], "pkg": m["pkg"],
                                            "sha256": m["sha256"], "size": m["size"]})
        self.assertEqual([p["model"] for p in m["packages"][1:]], sorted(SMK_HERE))
        for p in m["packages"][1:]:
            raw = (o / "install" / p["pkg"]).resolve().read_bytes()
            self.assertEqual(raw, SMK_HERE[p["model"]].read_bytes())
            self.assertEqual((p["product"], p["sha256"], p["size"], p["beta"]),
                             (p["model"] + "_900", hashlib.sha256(raw).hexdigest(), len(raw), True))
            self.assertRegex(p["pkg"], r"^\.\./firmware/saadet-(smk37-pro|smk37-elite|mke-p37)-dev\.fwsc$")
        if not SMK_HERE:
            self.skipTest("no SMK-37 packages: tests/run_smk37_tests.sh builds them")

    def test_smk_refusals(self):
        if "SMK-37 Elite" not in SMK_HERE:
            self.skipTest("no SMK-37 Elite package")
        elite = str(SMK_HERE["SMK-37 Elite"])
        with tempfile.TemporaryDirectory() as d:
            with self.assertRaisesRegex(SystemExit, "not an SMK-37 package"):
                S.main(["--dev", str(PKG), "dev", str(Path(d) / "a"), "--smk", str(PKG)])
            with self.assertRaisesRegex(SystemExit, "a second package for the SMK-37 Elite"):
                S.main(["--dev", str(PKG), "dev", str(Path(d) / "b"), "--smk", elite, "--smk", elite])

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
        self.assertEqual(S.smk_product("SMK-37 Elite", "1.0.0"), "SMK-37 Elite_910")      # three digits at most
        self.assertEqual(S.smk_product("MKE-P37", "1.2.3-rc1"), "MKE-P37_912")
        self.assertEqual(S.smk_product("SMK-37 Pro", "dev"), "SMK-37 Pro_900")
        with self.assertRaisesRegex(SystemExit, "one digit each"):
            S.smk_product("SMK-37 Elite", "1.10")


if __name__ == "__main__":
    unittest.main()
