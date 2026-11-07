#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Saadet's brand assets: the mark, the wordmark, the icon and the 1.0 graphics (assets/brand, assets/readme).

  gen_brand.py [--fonts DIR] [--out assets]

Every word is outlined from Sora and Manrope (OFL), so the SVGs and PNGs need no installed font. DIR holds the
fontsource .woff files (default: saadet-studio's node_modules); PNGs are rendered with rsvg-convert. The waveforms
are drawings of each engine's character, not measured audio.
"""
import argparse
import math
import shutil
import subprocess
from pathlib import Path

from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.ttLib import TTFont

STUDIO_FONTS = Path.home() / "code/personal/saadet-studio/node_modules/.pnpm"

# Saadet Studio's tokens (packages/saadet-ui/src/tokens.css): dark first.
INK, PANEL, LINE, LINE2 = "#12161d", "#151a22", "#2d3644", "#3a4555"
CREAM, SECONDARY, MID, QUIET = "#efe9d8", "#c9cfd9", "#a3acba", "#8590a0"
PAPER, PAPER_LINE, PAPER_LINE2, PAPER_INK, PAPER_MID = "#f6f3ea", "#ddd7c8", "#cbc4b3", "#1b1f26", "#59626f"
ENGINES = [  # firmware order: name, dark colour, light colour
    ("PROPHET", "#f2876b", "#c2512f"),
    ("FM6", "#86bfe8", "#2f7fb8"),
    ("CZ-1", "#e8cd78", "#9a7a12"),
    ("JUNO-106", "#b2dc86", "#4f8a22"),
    ("HYDRA", "#c5a5f0", "#7b55c4"),
    ("PPG", "#6fd3ea", "#1a86a3"),
]
WORD_TRACK = 0.06  # the wordmark: Sora 300, uppercase, 6 % of the size between letters


class Fonts:
    def __init__(self, root):
        self.root, self.cache = Path(root), {}

    def get(self, family, weight):
        k = (family, weight)
        if k not in self.cache:
            hits = list(self.root.glob(f"@fontsource+{family}@*/node_modules/@fontsource/{family}/files/"
                                       f"{family}-latin-{weight}-normal.woff"))
            if not hits:
                raise SystemExit(f"{family} {weight} not found under {self.root}: pass --fonts")
            self.cache[k] = TTFont(hits[0])
        return self.cache[k]


F = None


def text(s, family, weight, size, x, y, fill, track=0.0, anchor="start"):
    """s outlined at baseline y. Returns (svg element, advance in px)."""
    ft = F.get(family, weight)
    gs, cmap, upm = ft.getGlyphSet(), ft.getBestCmap(), ft["head"].unitsPerEm
    sc, adv = size / upm, 0.0
    names = [cmap[ord(c)] for c in s]
    for i, g in enumerate(names):
        adv += gs[g].width + (track * upm if i < len(names) - 1 else 0)
    width = adv * sc
    x0 = x - (width if anchor == "end" else width / 2 if anchor == "middle" else 0)
    pen, cx = SVGPathPen(gs), 0.0
    for g in names:
        gs[g].draw(TransformPen(pen, (sc, 0, 0, -sc, x0 + cx * sc, y)))
        cx += gs[g].width + track * upm
    return f'<path d="{pen.getCommands()}" fill="{fill}"/>', width


def cap(family, weight, size):
    ft = F.get(family, weight)
    return ft["OS/2"].sCapHeight * size / ft["head"].unitsPerEm


def poly(pts):
    return "M" + " L".join(f"{x:.2f},{y:.2f}" for x, y in pts)


# The mark: one cycle of a sine stood on its end, so it reads as an S. In the wordmark it is the first letter.
def sine_s(cx, top, h, amp, stroke, sw, n=160):
    pts = [(cx - amp * math.sin(2 * math.pi * i / n), top + sw / 2 + (h - sw) * i / n) for i in range(n + 1)]
    return f'<path d="{poly(pts)}" fill="none" stroke="{stroke}" stroke-width="{sw:.2f}" stroke-linecap="round" stroke-linejoin="round"/>'


MARK_W = 0.62  # the mark alone: width over height
def mark(x, top, h, stroke, weight=0.1):
    sw = weight * h
    return sine_s(x + MARK_W * h / 2, top, h, (MARK_W * h - sw) / 2, stroke, sw)


def wordmark(x, baseline, size, fg, s_colour=None):
    """SAADET in Sora 300, its S the sine. Returns (svg, width)."""
    ch = cap("sora", 300, size)
    ft = F.get("sora", 300)
    k = size / ft["head"].unitsPerEm
    sw = 0.104 * ch                     # Sora 300's stem
    h = 1.05 * ch                       # with the overshoot of Sora's S
    w = 0.68 * ch                       # a little narrower than Sora's S (0.77), stroke included
    lsb, adv = 65 * k, 697 * k          # Sora's S: its side bearing and advance
    s = sine_s(x + lsb + 0.04 * ch + w / 2, baseline - ch - 0.025 * ch, h, (w - sw) / 2, s_colour or fg, sw)
    t, tw = text("AADET", "sora", 300, size, x + adv + WORD_TRACK * size, baseline, fg, WORD_TRACK)
    return s + t, adv + WORD_TRACK * size + tw


def wave(kind, n=240):
    """One engine's drawing as y in -1..1 over two cycles (the Prophet's saw is drawn in wave_tile)."""
    out = []
    for i in range(n + 1):
        t = 2 * i / n  # cycles
        p = t % 1.0
        if kind == "FM6":       # a modulator at three times the carrier
            y = math.sin(2 * math.pi * p + 1.1 * math.sin(6 * math.pi * p))
        elif kind == "CZ-1":      # phase distortion: the cosine's phase bent at 0.15
            d = 0.15
            q = 0.5 * p / d if p < d else 0.5 + 0.5 * (p - d) / (1 - d)
            y = math.cos(2 * math.pi * q)
        elif kind == "JUNO-106":  # pulse, 35 %
            y = 1 if p < 0.35 else -1
        elif kind == "HYDRA":     # a wave list scanned: a sine turning square over the two cycles
            m = t / 2
            y = (1 - m) * math.sin(2 * math.pi * p) + m * math.tanh(6 * math.sin(2 * math.pi * p)) / math.tanh(6)
        else:                     # PPG: a stepped wavetable
            y = round(math.sin(2 * math.pi * p) * 4) / 4
        out.append((t / 2, y))
    if kind == "JUNO-106" or kind == "PPG":  # vertical edges
        edged = [out[0]]
        for a, b in zip(out, out[1:]):
            if a[1] != b[1]:
                edged.append((b[0], a[1]))
            edged.append(b)
        out = edged
    return out


def wave_tile(kind, x, y, w, h, color, axis, sw):
    """A drawing on its zero line."""
    lo, hi = y + h / 2 + h * 0.46, y + h / 2 - h * 0.46
    if kind == "PROPHET":  # saw: a ramp up, a straight drop
        pts = [(x, lo), (x + w / 2, hi), (x + w / 2, lo), (x + w, hi)]
    else:
        pts = [(x + u * w, y + h / 2 - v * h * 0.46) for u, v in wave(kind)]
    return (f'<line x1="{x}" y1="{y + h / 2}" x2="{x + w}" y2="{y + h / 2}" stroke="{axis}" stroke-width="1.5"/>'
            f'<path d="{poly(pts)}" fill="none" stroke="{color}" stroke-width="{sw}" stroke-linejoin="round" stroke-linecap="round"/>')


def strands(cx, top, h, colours, sw, gap):
    """The six engines as six parallel S strands (the 1.0 graphic)."""
    out, n = "", 200
    for k, col in enumerate(colours):
        off = (k - (len(colours) - 1) / 2) * gap
        pts = [(cx + off - 0.26 * h * math.sin(2 * math.pi * i / n), top + 0.1 * h + 0.8 * h * i / n) for i in range(n + 1)]
        out += f'<path d="{poly(pts)}" fill="none" stroke="{col}" stroke-width="{sw:.2f}" stroke-linecap="round"/>'
    return out


def svg(w, h, body, bg=None):
    b = f'<rect width="{w}" height="{h}" fill="{bg}"/>' if bg else ""
    return f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}">{b}{body}</svg>\n'


def write(path, s):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(s)
    print(path)


def png(src, dst, w=None):
    cmd = ["rsvg-convert", str(src), "-o", str(dst)] + (["-w", str(w)] if w else [])
    subprocess.run(cmd, check=True)
    print(dst)


def main():
    global F
    ap = argparse.ArgumentParser()
    ap.add_argument("--fonts", default=str(STUDIO_FONTS))
    ap.add_argument("--out", default=str(Path(__file__).resolve().parent.parent / "assets"))
    a = ap.parse_args()
    if not shutil.which("rsvg-convert"):
        raise SystemExit("rsvg-convert is needed for the PNGs (brew install librsvg)")
    F = Fonts(a.fonts)
    out = Path(a.out)
    b = out / "brand"

    # the mark alone (avatars, the icon, a favicon), on dark and on light grounds
    for name, fg in (("saadet-mark", CREAM), ("saadet-mark-ink", PAPER_INK)):
        write(b / f"{name}.svg", svg(round(MARK_W * 100), 100, mark(0, 0, 100, fg)))

    # the wordmark
    size = 96
    ch = cap("sora", 300, size)
    pad = 0.06 * ch
    for name, fg in (("saadet-wordmark", CREAM), ("saadet-wordmark-ink", PAPER_INK)):
        body, w = wordmark(pad, pad + 1.025 * ch, size, fg)
        write(b / f"{name}.svg", svg(round(w + 2 * pad), round(1.05 * ch + 2 * pad), body))

    # the icon: the mark on ink, corners at 22 %
    icon = f'<rect width="512" height="512" rx="112" fill="{INK}"/>' + mark(256 - MARK_W * 344 / 2, 84, 344, CREAM, 0.13)
    write(b / "saadet-icon.svg", svg(512, 512, icon))
    for px in (512, 180, 32):
        png(b / "saadet-icon.svg", b / f"saadet-icon-{px}.png", px)

    # the six strands
    write(b / "saadet-strands.svg", svg(240, 300, strands(120, 0, 300, [c for _, c, _ in ENGINES], 9, 12)))

    # GitHub social preview, 1280 x 640
    W, H = 1280, 640
    s = ""
    s += text("ELLIC STUDIO / SAADET", "manrope", 600, 17, 72, 84, QUIET, 0.16)[0]
    lk, lw = wordmark(72, 300, 112, CREAM)
    s += lk
    s += text("1.0", "sora", 300, 112, 72 + lw + 40, 300, QUIET, 0.02)[0]
    s += text("Six engines. Two parts. A new editor.", "sora", 400, 36, 72, 384, CREAM)[0]
    s += text("Synthesizer firmware for the M-VAVE FM-1", "manrope", 400, 25, 72, 430, MID)[0]
    x = 72
    for name, col, _ in ENGINES:
        t, w = text(name, "manrope", 700, 16, x, 540, col, 0.12)
        s += t
        x += w + 30
    s += text("github.com/keremimo/saadet", "manrope", 500, 18, 72, 588, QUIET)[0]
    s += strands(1095, 40, 560, [c for _, c, _ in ENGINES], 11, 15)
    write(b / "social-preview.svg", svg(W, H, s, INK))
    png(b / "social-preview.svg", b / "social-preview.png")

    # the README's introduction, 1600 x 1000 (also the release post's picture)
    W, H = 1600, 1000
    s = ""
    s += text("ELLIC STUDIO / SAADET", "manrope", 600, 20, 82, 70, QUIET, 0.16)[0]
    s += text("M-VAVE FM-1 · SMK-37", "manrope", 600, 20, W - 80, 70, QUIET, 0.16, "end")[0]
    lk, lw = wordmark(82, 270, 150, CREAM)
    s += lk
    s += text("1.0", "sora", 300, 150, 82 + lw + 56, 270, QUIET, 0.02)[0]
    s += text("Six engines. Two parts. A new editor.", "sora", 400, 46, 84, 368, CREAM)[0]
    s += text("Synthesizer firmware for the M-VAVE FM-1, and the SMK-37 in beta.", "manrope", 400, 29, 84, 420, MID)[0]
    tw, gap, ty = 206, 40, 540
    for k, (name, col, _) in enumerate(ENGINES):
        x = 84 + k * (tw + gap)
        s += text(name, "manrope", 700, 20, x, 500, col, 0.12)[0]
        s += wave_tile(name, x, ty, tw, 112, col, LINE, 3.5)
    s += text("TWO PARTS / SPLIT OR LAYERED BY MIDI CHANNEL / NAMED PERFORMANCES", "manrope", 600, 18, 84, 722, QUIET, 0.12)[0]
    for k, (big, small) in enumerate((("100", "PPG Wave 2.2 programs"), ("8", "named macros on every sound"),
                                      ("384", "shared user sound slots"))):
        x = 84 + k * 520
        s += text(big, "sora", 400, 46, x, 820, CREAM)[0]
        s += text(small, "manrope", 400, 25, x, 862, MID)[0]
    s += f'<line x1="80" y1="917" x2="{W - 80}" y2="917" stroke="{LINE2}" stroke-width="1.5"/>'
    s += text("github.com/keremimo/saadet", "manrope", 500, 24, 82, 962, MID)[0]
    s += text("saadet.ellicstudio.com", "manrope", 500, 24, W - 80, 962, MID, 0, "end")[0]
    write(b / "introducing-saadet.svg", svg(W, H, s, INK))
    png(b / "introducing-saadet.svg", out / "readme" / "introducing-saadet.png")


if __name__ == "__main__":
    main()
