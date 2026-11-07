#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
# Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""tools/fm1_install.py against a simulated FM-1 (no hardware, no mido).
The fake device models the update protocol: identity on the handshake,
then "device asks, host answers" reads of the logical image. Run from the repo root:
  python3 tests/install_test.py
Also checks logical_image/product_of against web/fm1pkg.js (needs node and
build/saadet.fwsc, skipped otherwise)."""
import io
import queue
import shutil
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import fm1_install as I  # noqa: E402

I.DELAY.update(open=0, start=0.01, reply=0, loader=0.01, reboot=0.01, retry=0.02, poll=0.05, hs=0.1, hs_full=0.05,
               idle_check=0.4, idle_write=0.4, wait_loader=2, wait_reboot=2)
failed = 0


def ok(cond, what):
    global failed
    print(f"{what:<64} {'ok' if cond else 'FAIL'}")
    failed += not cond


# ---------------------------------------------------------- simulated FM-1 ---

class FakeLink:
    def __init__(self, dev, gen):
        self.dev, self.gen, self.q, self.closed = dev, gen, queue.Queue(), False

    @property
    def lost(self):
        return self.gen != self.dev.gen or not self.dev.connected

    def send(self, pkt):
        if self.lost:
            return False
        self.dev.sent.append(bytes(pkt))
        self.dev.rx(bytes(pkt))
        return True

    def read(self, timeout):
        try:
            return self.q.get(timeout=max(timeout, 0))
        except queue.Empty:
            return None

    def drain(self):
        while not self.q.empty():
            self.q.get_nowait()

    def close(self):
        self.closed = True


def ident_frame(text, flash=0):
    """a handshake answer as the firmware sends it (00 59 11, 27 bytes, checksum): the text, zeros and, from a
    firmware on felucca-bsp#29's chip/ota.c, the flash's JEDEC ID in bytes 24..26"""
    body = text.encode("latin-1").ljust(24, b"\0")[:24] + flash.to_bytes(3, "big")
    return b"\xF0" + I.pack7(bytes([0, 0x59, 0x11, len(body), 0, 0]) + body + bytes([~sum(body) & 0xFF])) + b"\xF7"


class FakeFM1:
    """a MIDI backend with one FM-1 on it. base: it runs firmware on felucca-bsp#29's chip/ota.c (Melodee on fm1-base):
    HS_QUERY gets the name, '_' and three digits, HS_FULL the whole identity (not from the loader), both with the
    flash's JEDEC ID; otherwise HS_FULL goes unanswered (classic Melodee, M-VAVE's firmware, Saadet 1.0)"""

    def __init__(self, image, identity="FM-1_015", name="FM-1", unplug_after=None, after_write="FM-1_900",
                 stall_after=None, bad_addr=None, loader=I.MELODEE_LOADER, base=False, flash=0x856014):
        self.image, self.unplug_after, self.after_write = image, unplug_after, after_write
        self.loader = loader        # the loader step 1 starts (the package's own)
        self.stall_after, self.bad_addr = stall_after, bad_addr
        self.base, self.flash = base, flash
        self.served = self.bad = self.upgrades = self.full_asked = 0
        self.sent, self.links, self.gen, self.lock = [], [], 0, threading.Lock()
        self.boot(identity, name)

    def boot(self, identity, name):
        with self.lock:
            self.gen += 1
            self.identity, self.name, self.connected = identity, name, True
            self.waiting, self.queue = None, []

    def input_names(self):
        return [self.name] if self.connected else []

    output_names = input_names

    def open(self, in_name, out_name):
        if in_name != self.name or not self.connected:
            raise IOError("no such port")
        link = FakeLink(self, self.gen)
        self.links.append(link)
        return link

    def tx(self, pkt):
        for link in self.links:
            if link.gen == self.gen and not link.closed:
                link.q.put(bytes(pkt))

    def answer(self, full):
        t, ota = self.identity, self.identity.startswith("ota-")
        if not self.base:
            return ident_frame(t)
        i = t.find("_")
        if not full and not ota and 0 <= i and i + 4 < len(t):
            t = t[:i + 4]               # "FM-1_9010000" -> "FM-1_901"; "FM-1_900" stays
        return ident_frame(t, self.flash)

    def rx(self, d):
        if d == I.HS_QUERY:
            self.tx(self.answer(False))
        elif d == I.HS_FULL:
            self.full_asked += 1
            if self.base and not self.identity.startswith("ota-"):
                self.tx(self.answer(True))
        elif d == I.UPGRADE:
            self.upgrades += 1
            first = self.bad_addr if self.bad_addr is not None else 0
            self.queue = ([(first + k * 512, 512) for k in range(6)] + [(I.FINISH_WRITE, 8)]
                          if self.identity.startswith("ota-")
                          else [(0, 64), (0x40, 160), (0x1000, 512), (I.FINISH_CHECK, 8)])
            self.next()
        elif self.waiting:
            u = I.unpack7(d[1:-1])
            addr, n = self.waiting
            fin = addr >= I.FINISH_CHECK
            want = b"success\0" if fin else self.image[addr:addr + n]
            if u[14:14 + len(want)] != want or len(u) != 15 + len(want):
                self.bad += 1
            self.waiting = None
            self.served += 1
            if self.unplug_after and self.served >= self.unplug_after:
                self.connected = False
                return
            if self.stall_after and self.served >= self.stall_after:
                return
            if addr == I.FINISH_CHECK:
                threading.Timer(0.05, self.boot, (self.loader, "Melodee Update")).start()
            elif addr == I.FINISH_WRITE:
                threading.Timer(0.05, self.boot, (self.after_write, "Melodee")).start()
            else:
                self.next()

    def next(self):
        if not self.queue:
            return
        self.waiting = addr, n = self.queue.pop(0)
        u = bytearray([0, 0x59, 0x30, 0, 0, 0, 0]) + addr.to_bytes(4, "little") + bytes([n & 0xFF, n >> 8, 0])
        u.append(~sum(u[6:14]) & 0xFF)
        self.tx(b"\xF0" + I.pack7(u) + b"\xF7")


# ----------------------------------------------------------------- helpers ---

def package(product="FM-1_900", marker=True, size=0x2000):
    raw = bytearray((i * 7) & 0xFF for i in range(size + I.BLOCKS))
    for i in range(I.BLOCKS):
        raw[i * I.BLK + I.KEEP] = (ord(product[i]) + i + 1) & 0xFF if i < len(product) else 0x7D
    raw[0x1800:0x1800 + 16] = I.LOADER_MARK if marker else bytes(16)
    return bytes(raw)


TMP = Path(tempfile.mkdtemp(prefix="melodee-install-"))


def pkgfile(name, raw):
    p = TMP / name
    p.write_bytes(raw)
    return str(p)


def cli(args, dev, answer=True):
    out, err = io.StringIO(), io.StringIO()
    old, sys.stderr = sys.stderr, err
    try:
        rc = I.main(args, backend=dev, out=out, ask=lambda _p: answer)
    finally:
        sys.stderr = old
    return rc, out.getvalue(), err.getvalue()


# ------------------------------------------------------------------- tests ---

def wire():
    data = bytes(range(256)) * 3
    ok(I.unpack7(I.pack7(data))[:len(data)] == data, "pack7 / unpack7 round trip")
    pkt = I.response(0x12345, b"\x01\x02\x03", fl=5)
    u = I.unpack7(pkt[1:-1])
    ok(u[:3] == b"\x00\x59\x30" and int.from_bytes(u[7:11], "little") == 0x12345 and u[14:17] == b"\x01\x02\x03"
       and ~sum(u[6:17]) & 0xFF == u[17], "response: header, address, data, checksum")
    raw = package("FM-1_906")
    ok(I.product_of(raw) == "FM-1_906" and len(I.logical_image(raw)) == len(raw) - 20, "product_of / logical_image (synthetic)")


def installs():
    raw = package()
    image = I.logical_image(raw)
    p = pkgfile("ok.fwsc", raw)

    dev = FakeFM1(image)
    rc, out, err = cli([p, "--yes"], dev)
    ok(rc == 0 and dev.bad == 0 and dev.upgrades == 2 and dev.served == 11 and "done: the FM-1 runs FM-1_900" in out
       and "100%" in out, f"install: running -> loader -> Melodee ({dev.served} reads)")

    dev = FakeFM1(image, identity="ota-FM-1_900", name="Melodee Update")
    rc, out, err = cli([p, "--yes"], dev)
    ok(rc == 0 and dev.bad == 0 and dev.upgrades == 1 and "update mode" in out, "install: device already in update mode -> finishes the write")

    dev = FakeFM1(image)
    rc, out, err = cli([p], dev, answer=False)
    ok(rc == 1 and dev.upgrades == 0 and "cancelled" in out, "install: answer no -> nothing sent but the handshake")

    dev = FakeFM1(image, identity="FM-1_905", name="Melodee")
    rc, out, err = cli(["--info"], dev)
    ok(rc == 0 and "FM-1_905" in out and "running" in out and dev.upgrades == 0, "--info: identity of the connected FM-1")

    dev = FakeFM1(image, identity="ota-FM-1_900", name="Melodee Update")
    rc, out, err = cli(["--info"], dev)
    ok(rc == 0 and "update loader" in out, "--info: device in update mode")


def base():
    """a firmware on felucca-bsp#29 (Melodee on fm1-base): the flash's JEDEC ID after the identity's zeros, read
    without spoiling the text; --info names the part; installs and resumes on the UC25HQ80 as on the Puya"""
    # Melodee's FM-1_900 on fm1-base answering HS_FULL on a real FM-1 (2026-11-06, Melodee tests/install_test.py)
    reply = bytes.fromhex("f000324558010000234d5a447915070c180000000000000000000000000000000000001404462207f7")
    ident = I.parse_identity(reply)
    ok(ident and ident.text == "FM-1_900" and ident.model == "FM-1" and ident.version == 900 and not ident.loader
       and ident.flash == 0x856014, "fm1-base FM-1's real reply: FM-1_900, flash 856014")
    ok(ident_frame("FM-1_900", 0x856014) == reply, "... the simulated fm1-base FM-1 answers those very bytes")
    ok(I.unpack7(I.HS_FULL[1:-1]) == bytes([0, 0x59, 0x11, 1, 0, 0, 1, 0xFE]), "HS_FULL: the handshake with the body 01")
    loader = I.parse_identity(ident_frame(I.MELODEE_LOADER, 0xB36014))
    ok(loader and loader.loader and loader.text == I.MELODEE_LOADER and loader.flash == 0xB36014,
       "fm1-base loader's reply with a flash ID: ota-FM-1_900, B36014")
    plain = I.parse_identity(ident_frame("FM-1_015"))
    ok(plain and plain.text == "FM-1_015" and plain.flash == 0, "stock reply: no flash ID")
    body = b"ABCDEFGHIJKLMNOPQRSTUVW_12\0"           # a text into bytes 24..26: no ID there
    long = I.parse_identity(b"\xF0" + I.pack7(bytes([0, 0x59, 0x11, 27, 0, 0]) + body + bytes([~sum(body) & 0xFF]))
                            + b"\xF7")
    ok(long and long.text == "ABCDEFGHIJKLMNOPQRSTUVW_12" and long.flash == 0,
       "a text over 23 bytes: its tail is not read as a flash ID")

    raw = package()
    image = I.logical_image(raw)
    p = pkgfile("ok.fwsc", raw)
    for flash, shown in ((0x856014, "flash: Puya P25Q80 (856014)"), (0xB36014, "flash: UC25HQ80 (B36014)"),
                         (0xEB6014, "flash: EB6014")):
        dev = FakeFM1(image, identity="FM-1_900", name="Melodee", base=True, flash=flash)
        rc, out, err = cli(["--info"], dev)
        ok(rc == 0 and out.startswith("FM-1_900  [running]") and shown in out and dev.full_asked == 1,
           f"--info on an fm1-base FM-1: {shown}")
    dev = FakeFM1(image, identity="FM-1_7001608", name="Melodee", base=True, flash=0xB36014)
    rc, out, err = cli(["--info"], dev)
    ok(rc == 0 and out.startswith("FM-1_7001608  [running]") and "UC25HQ80" in out,
       "--info: HS_FULL gives the whole FM-1_7001608 and its flash")

    dev = FakeFM1(image, identity="FM-1_7001608", name="Melodee", base=True, flash=0xB36014)
    rc, out, err = cli([p, "--yes"], dev)
    ok(rc == 0 and dev.bad == 0 and dev.upgrades == 2 and "done: the FM-1 runs FM-1_900" in out,
       "install over Melodee on fm1-base, UC25HQ80: loader, back as FM-1_900")
    dev = FakeFM1(image, identity=I.MELODEE_LOADER, name="Melodee Update", base=True, flash=0xB36014)
    rc, out, err = cli([p, "--yes"], dev)
    ok(rc == 0 and dev.bad == 0 and dev.upgrades == 1 and "update mode" in out and dev.full_asked == 1,
       "fm1-base loader with a flash ID: write finished, HS_FULL only after")

    for identity, name, what in (("FM-1_900", "Melodee", "classic FM-1_900: HS_FULL unanswered, no flash"),
                                 ("FM-1_015", "FM-1", "stock FM-1_015: no flash"),
                                 (I.MELODEE_LOADER, "Melodee Update", "update loader: no flash")):
        dev = FakeFM1(image, identity=identity, name=name)
        rc, out, err = cli(["--info"], dev)
        ok(rc == 0 and out.startswith(identity + "  [") and "flash:" not in out, what)


def errors():
    raw = package()
    image = I.logical_image(raw)
    p = pkgfile("ok.fwsc", raw)

    rc, out, err = cli([p, "--yes"], FakeFM1(image, name="IAC Driver Bus 1"))
    ok(rc == 3 and "not found" in err and "IAC Driver" in err, "no FM-1 port -> exit 3, lists the MIDI inputs")
    dev = FakeFM1(image)
    rc, out, err = cli([p, "--yes", "--port", "Melodee"], dev)
    ok(rc == 3 and dev.upgrades == 0, "--port that matches nothing -> exit 3")
    dev = FakeFM1(image, name="My Interface")
    rc, out, err = cli(["--info", "--port", "my int"], dev)
    ok(rc == 0 and "FM-1_015" in out, "--port picks a port the default match skips")

    t0 = time.monotonic()
    rc, out, err = cli([p, "--yes"], FakeFM1(image, unplug_after=2))
    ok(rc == 4 and "disconnected" in err and "nothing was written" in err and time.monotonic() - t0 < 3,
       "unplugged in step 1 -> exit 4 'lost', nothing written")
    t0 = time.monotonic()
    rc, out, err = cli([p, "--yes"], FakeFM1(image, identity="ota-FM-1_900", name="Melodee Update", unplug_after=3))
    ok(rc == 4 and "run the install again" in err and time.monotonic() - t0 < 3, "unplugged during the write -> exit 4 at once")
    rc, out, err = cli([p, "--yes"], FakeFM1(image, identity="ota-FM-1_900", name="Melodee Update", stall_after=3))
    ok(rc == 4 and "stopped answering" in err, "loader stops answering -> exit 4 after the idle time")
    rc, out, err = cli([p, "--yes"], FakeFM1(image, identity="ota-FM-1_900", name="Melodee Update", bad_addr=len(image)))
    ok(rc == 4 and "outside the package" in err, "read past the package -> exit 4")

    rc, out, err = cli([p, "--yes"], FakeFM1(image, after_write="FM-1_015"))
    ok(rc == 6 and "reports FM-1_015" in err, "another identity after the restart -> exit 6")
    dev = FakeFM1(image, identity="XY-9_001", name="usb-midi")
    rc, out, err = cli([p, "--yes"], dev)
    ok(rc == 6 and dev.upgrades == 0, "another model -> exit 6, nothing sent")

    class Stuck(FakeFM1):          # the loader never shows up
        def boot(self, identity, name):
            super().boot(identity, name)
            if identity.startswith("ota-"):
                self.connected = False
    rc, out, err = cli([p, "--yes"], Stuck(image))
    ok(rc == 5 and "loader did not appear" in err, "no loader after step 1 -> exit 5")

    plain = pkgfile("plain.fwsc", package(marker=False))
    dev = FakeFM1(image)
    rc, out, err = cli([plain, "--yes"], dev)
    ok(rc == 2 and "no Melodee update loader" in err and dev.sent == [], "package without the loader marker -> exit 2, no MIDI")
    dev = FakeFM1(I.logical_image(package(marker=False)))
    rc, out, err = cli([plain, "--yes", "--force"], dev)
    ok(rc == 0 and dev.bad == 0, "... installs with --force")
    dev = FakeFM1(image, identity=I.MELODEE_LOADER, name="Melodee Update")
    rc, out, err = cli([plain, "--yes", "--force"], dev)
    ok(rc == 6 and dev.upgrades == 0 and "nothing was sent" in err, "... but never resumes in an update loader")
    rc, out, err = cli([pkgfile("short.fwsc", b"\0" * 100), "--yes"], FakeFM1(image))
    ok(rc == 2 and "too short" in err, "not a package -> exit 2")
    rc, out, err = cli([str(TMP / "missing.fwsc"), "--yes"], FakeFM1(image))
    ok(rc == 2, "missing file -> exit 2")


def loaders():
    """each package goes only to its own update loader: the official one would write a Melodee
    package's head over the boot data"""
    raw = package()
    image = I.logical_image(raw)
    p = pkgfile("ok.fwsc", raw)
    dev = FakeFM1(image, identity=I.STOCK_LOADER, name="FM-1")
    rc, out, err = cli([p, "--yes"], dev)
    ok(rc == 6 and dev.upgrades == 0 and "nothing was sent" in err and "official FM-1.fwsc" in err and "device:" not in out,
       "Melodee package, FM-1 in the official loader -> exit 6 before asking, nothing sent")
    dev = FakeFM1(image, identity="ota-FM-1_123", name="Melodee Update")
    rc, out, err = cli([p, "--yes"], dev)
    ok(rc == 6 and dev.upgrades == 0 and "report" in err, "an unknown loader -> exit 6, nothing sent")
    dev = FakeFM1(image, loader=I.STOCK_LOADER)
    rc, out, err = cli([p, "--yes"], dev)
    ok(rc == 6 and dev.upgrades == 1 and dev.served == 4 and dev.identity == I.STOCK_LOADER,
       "step 1 starts the official loader -> exit 6, nothing sent to it")


def against_js():
    clean = ROOT / "build/saadet.fwsc"
    if not shutil.which("node") or not clean.exists():
        print("logical image vs fm1pkg.js: skipped (needs node and build/saadet.fwsc)")
        return
    js = ("import { logicalImage, productOf } from %r; import { readFileSync } from 'node:fs';"
          "const p = readFileSync(process.argv[1]); process.stderr.write(productOf(p));"
          "process.stdout.write(logicalImage(p));") % str(ROOT / "web/fm1pkg.js")
    r = subprocess.run(["node", "--input-type=module", "-e", js, str(clean)], capture_output=True, check=True)
    raw = clean.read_bytes()
    ok(I.logical_image(raw) == r.stdout and I.product_of(raw) == r.stderr.decode(),
       f"logical_image / product_of == fm1pkg.js ({clean.name}, {I.product_of(raw)})")
    ok(I.LOADER_MARK in raw, f"{clean.name} carries the Melodee loader marker")


def official():
    """#32: back to the official V15 from Melodee, without --force (only the unmodified file)"""
    v15 = ROOT / "FM-1_v15.fwsc"
    if not v15.exists():
        print("official V15 restore: skipped (needs FM-1_v15.fwsc in the repo root)")
        return
    raw = v15.read_bytes()
    dev = FakeFM1(I.logical_image(raw), identity="FM-1_900", name="Melodee", after_write="FM-1_015", loader=I.STOCK_LOADER)
    rc, out, err = cli([str(v15), "--yes"], dev)
    ok(rc == 0 and dev.bad == 0 and "FM-1_015" in out and dev.identity == "FM-1_015",
       "official V15 (FM-1.fwsc) from Melodee: installed without --force, back as FM-1_015")
    dev = FakeFM1(I.logical_image(raw), identity=I.STOCK_LOADER, name="FM-1", after_write="FM-1_015")
    rc, out, err = cli([str(v15), "--yes"], dev)
    ok(rc == 0 and dev.upgrades == 1 and dev.identity == "FM-1_015", "official V15 resumes in the official loader")
    dev = FakeFM1(I.logical_image(raw), identity=I.MELODEE_LOADER, name="Melodee Update", after_write="FM-1_015")
    rc, out, err = cli([str(v15), "--yes"], dev)
    ok(rc == 6 and dev.upgrades == 0 and "Melodee package first" in err, "official V15 never resumes in Melodee's loader")
    dev = FakeFM1(I.logical_image(raw), identity="FM-1_900", name="Melodee", after_write="FM-1_015")
    rc, out, err = cli([str(v15), "--yes"], dev)
    ok(rc == 6 and dev.served == 4 and dev.identity == I.MELODEE_LOADER, "official V15 is not written when step 1 starts Melodee's loader")
    bad = bytearray(raw)
    bad[-1] ^= 1
    dev = FakeFM1(I.logical_image(bytes(bad)), identity="FM-1_900", name="Melodee")
    rc, out, err = cli([pkgfile("v15mod.fwsc", bytes(bad)), "--yes"], dev)
    ok(rc == 2 and "official V15" in err and dev.sent == [], "a modified V15 is still refused (no MIDI)")


wire()
installs()
base()
errors()
loaders()
against_js()
official()
shutil.rmtree(TMP, ignore_errors=True)
print(f"INSTALL TESTS FAILED ({failed})" if failed else "install tests passed")
sys.exit(1 if failed else 0)
