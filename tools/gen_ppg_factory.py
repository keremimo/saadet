#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""The PPG Wave 2.2's factory programs as PPG presets, from a local copy of the bank (never committed).

Reads the gitignored `private/ppg/ppg22-factory.syx` (Virtual Music's factory22_vm.syx, a V8 bank dump) and, if
present, `private/ppg/ppg22-factory-names.txt` (NN|wavetable|name, from the owner's manual). Writes
build/gen/ppg_factory.h: the 100 programs in order (preset k is program k) in ppg_patch.h's byte order, one a program
("17 PIANO"), whole: both groups, KEYB and SPLIT. As the 2.2 recalls one (its OS, load mode 0): group A where KEYB 0
plays, group B beside it for the layers and splits (and for the GROUP button, the 2.2's B-into-A load). The six on
WAVETABLE 31 keep it: the engine plays its natural sounds. Without the bank the header has none.

The bank: F0 29 01 0D, 10,240 nibbles (pairs, high nibble first: 5,120 bytes of the 2.2's program memory), F7. Program
k is 51 bytes at 51 k, read as one little-endian bit string. The layout is the 2.2 OS's own (its unpack loop and the
field table it walks, the same in the V3 and V5 OS): bits 0..14 the program's WAVETABLE, KEYB and SPLIT; group A's
fields from bit 15, group B's from bit 211, each as GROUP below (width in bits, the factor the OS scales the knobs by;
the panel shows a knob as the scaled value halved, 0..63). The knobs' meanings follow the ANALOG display, which names
each by its place in the scaled array; the codes follow the DIGITAL and TUNING displays and their flag bits.
"""
import hashlib
import os
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parents[1]
BANK = Path(os.environ.get("SAADET_PPG_FACTORY", SRC / "private/ppg/ppg22-factory.syx"))
NAMES = BANK.with_name("ppg22-factory-names.txt")
SHA256 = "a542383740bd33a6fd66df15834c469083da2e399c3dd0ead5522fab78ed159e"
REC, NPROG = 51, 100
# a group's fields in the OS's order: (name, bits, scale); the knobs first (0..21), then the codes
GROUP = [("RATE", 5, 4), ("OSC", 6, 2), ("CUT", 6, 2), ("E1F", 5, 4), ("E1W", 5, 4), ("EMPH", 4, 9), ("LOUD", 5, 4),
         ("A1", 4, 8), ("A2", 4, 8), ("D1", 5, 4), ("D2", 5, 4), ("S1", 6, 2), ("S2", 6, 2), ("R1", 5, 4), ("R2", 5, 4),
         ("DELAY", 4, 8), ("A3", 4, 8), ("SHAPE", 2, 0), ("D3", 5, 4), ("WHEEL", 5, 4), ("E3AMT", 5, 4), ("SUB", 6, 2),
         ("FLAGS1", 8, 0), ("FLAGS2", 8, 0), ("KF", 3, 0), ("KL", 3, 0), ("KW", 3, 0), ("DETU", 3, 0), ("SW", 2, 0),
         ("BD", 3, 0), ("BI", 2, 0)] + [(f"SEMIT{v}", 6, 0) for v in range(1, 9)]
GROUP_AT = (15, 211)
# ppg_patch.h's PG_* order
ORDER = ["TABLE", "OSC", "SUB", "SW", "E1W", "KW", "UW", "SEMI", "DETU", "BI", "BD", "CUT", "EMPH", "E1F", "KF",
         "A1", "D1", "S1", "R1", "A2", "D2", "S2", "R2", "LOUD", "KL", "A3", "D3", "E3AMT", "E3TO",
         "RATE", "SHAPE", "DELAY", "WHEEL", "MP", "MW", "MF", "ML", "TW", "TF", "TL", "TM", "VF", "VL",
         "SEMI2", "SEMI3", "SEMI4", "SEMI5", "SEMI6", "SEMI7", "SEMI8"]


def memory(raw):
    assert raw[:4] == bytes([0xF0, 0x29, 0x01, 0x0D]) and raw[-1] == 0xF7, "not a V8 bank dump"
    d = raw[4:-1]
    assert len(d) == 2 * NPROG * REC + 40 and max(d) < 16
    return bytes(d[i] << 4 | d[i + 1] for i in range(0, len(d), 2))


def fields(rec, at):
    v, out, p = int.from_bytes(rec, "little"), {}, at
    for name, bits, scale in GROUP:
        x = (v >> p) & ((1 << bits) - 1)
        p += bits
        out[name] = x if not scale else min(63, x * 9 // 2) if name == "EMPH" else ((x * scale) & 0x7F) >> 1
    return out, v


def patch(table, f):
    f1, f2 = f["FLAGS1"], f["FLAGS2"]                # (VL VF TF MW TW UW - MS), (MO MF ML - TL TM EO ES)
    bit = lambda b, n: (b >> n) & 1
    p = dict(f)
    p.update({("SEMI" if v == 1 else f"SEMI{v}"): max(-24, min(24, f[f"SEMIT{v}"] - 12)) + 24 for v in range(1, 9)})
    p.update(TABLE=table, UW=bit(f1, 5), E3TO=bit(f2, 6) | bit(f2, 7) << 1,
             MP=bit(f2, 0) | bit(f1, 7) << 1, MW=bit(f1, 3), MF=bit(f2, 1), ML=bit(f2, 2), TW=bit(f1, 4), TF=bit(f1, 2),
             TL=bit(f2, 4), TM=bit(f2, 5), VF=bit(f1, 1), VL=bit(f1, 0))
    return [p[k] for k in ORDER]


def names():
    out = {}
    if NAMES.exists():
        for line in NAMES.read_text().splitlines():
            if line and not line.startswith("#"):
                k, wt, name = line.split("|", 2)
                out[int(k)] = (int(wt), name)
    return out


def presets():
    if not BANK.exists():
        return None, []
    raw = BANK.read_bytes()
    if hashlib.sha256(raw).hexdigest() != SHA256:
        raise SystemExit(f"gen_ppg_factory: {BANK} is not the expected PPG Wave 2.2 factory bank")
    mem, given, out = memory(raw), names(), []
    for k in range(NPROG):
        rec = mem[REC * k:REC * (k + 1)]
        bits = int.from_bytes(rec, "little")
        table, keyb, split = bits & 31, bits >> 5 & 15, bits >> 9 & 63
        if keyb > 8:
            keyb = 0                                 # (90's KEYB 9: the V3 OS deals it no voice; read as 0)
        wt, name = given.get(k, (None, ""))
        title = name if name and wt == table else f"PROGRAM {k:02d}"   # (the bank's program, not the manual's)
        groups = [fields(rec, at)[0] for at in GROUP_AT]
        a, b = (patch(table, f) for f in groups)
        out.append((f"{k:02d} {title}"[:20], a, b, keyb, split, 0))
    return BANK, out


def header(src, rows):
    out = ["/* Generated by tools/gen_ppg_factory.py from a local copy of the PPG Wave 2.2 factory bank. The programs are",
           " * PPG's (Palm Products GmbH), not covered by this repository's licence; keep them out of version control. */",
           "#pragma once", f"#define PPG_FACTORY_N {len(rows)}u"]
    if rows:
        out += [f"_Static_assert({len(ORDER)} == PG_RAW, \"ppg_patch.h and tools/gen_ppg_factory.py disagree\");",
                "static const struct { uint8_t raw[PG_RAW], rawb[PG_RAW], keyb, split, sel; char name[21]; }"
                " PPG_FACTORY[PPG_FACTORY_N] = {"]
        out += [f"    {{{{{','.join(map(str, a))}}}, {{{','.join(map(str, b))}}}, {keyb}, {split}, {sel}, \"{n}\"}},"
                for n, a, b, keyb, split, sel in rows]
        out.append("};")
    return "\n".join(out) + "\n"


def main(argv):
    if len(argv) != 2:
        raise SystemExit("usage: gen_ppg_factory.py OUT.h")
    src, rows = presets()
    Path(argv[1]).write_text(header(src, rows))
    print(f"gen_ppg_factory: {len(rows)} presets from {src}" if src else f"gen_ppg_factory: no factory bank ({BANK}), none")


if __name__ == "__main__":
    main(sys.argv)
