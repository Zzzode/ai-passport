#!/usr/bin/env python3
"""Deterministically generate placeholder grayscale part art for PasPet.

Shades: 0 = transparent, ~24 = outline, 160 = shaded fill, 200 = body fill,
255 = highlight/belly. Runtime LUT maps the ramp onto the pet's PALETTE
(edge -> main -> belly), so this art carries shape only, never color.

Regenerating is idempotent: identical bytes every run. Real art replaces these
PNGs one file at a time without touching firmware code.

Usage: python3 gen_placeholders.py [--check]
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from png_io import write_gray_png  # noqa: E402

G = 64
TRANS, OUT, SHADOW, FILL, HI = 0, 24, 160, 190, 255


class Canvas:
    def __init__(self) -> None:
        self.px = bytearray(G * G)

    def set(self, x: int, y: int, v: int) -> None:
        if 0 <= x < G and 0 <= y < G:
            # Outline never gets overwritten by fill, so composite helpers can
            # draw fill first and outline on top in any order.
            if v != TRANS and not (v != OUT and self.px[y * G + x] == OUT):
                self.px[y * G + x] = v

    def ellipse(self, cx, cy, rx, ry, v, fill=True):
        for y in range(cy - ry - 1, cy + ry + 2):
            for x in range(cx - rx - 1, cx + rx + 2):
                d = ((x - cx) / max(rx, 1)) ** 2 + ((y - cy) / max(ry, 1)) ** 2
                if fill and d <= 1.0:
                    self.set(x, y, v)
                elif not fill and 1.0 < d <= 1.45:
                    self.set(x, y, v)

    def disc(self, cx, cy, rx, ry, value=OUT, inner=None):
        """Filled shape with a 2px outline ring."""
        self.ellipse(cx, cy, rx, ry, OUT, fill=False)
        self.ellipse(cx, cy, rx, ry, value, fill=True)
        if inner is not None:
            self.ellipse(cx, cy, max(rx - 2, 1), max(ry - 2, 1), inner,
                         fill=True)

    def tri(self, x0, y0, x1, y1, x2, y2, v):
        lo, hi = min(y0, y1, y2), max(y0, y1, y2)
        for y in range(lo, hi + 1):
            xs = []
            for (ax, ay, bx, by) in ((x0, y0, x1, y1), (x1, y1, x2, y2),
                                     (x0, y0, x2, y2)):
                if (ay <= y <= by) or (by <= y <= ay):
                    if ay == by:
                        xs += [ax, bx]
                    else:
                        xs.append(ax + (bx - ax) * (y - ay) / (by - ay))
            if xs:
                for x in range(int(min(xs)), int(max(xs)) + 1):
                    self.set(x, y, v)

    def hline(self, x0, x1, y, v):
        for x in range(min(x0, x1), max(x0, x1) + 1):
            self.set(x, y, v)

    def vline(self, x, y0, y1, v):
        for y in range(min(y0, y1), max(y0, y1) + 1):
            self.set(x, y, v)


def body(i: int) -> Canvas:
    c = Canvas()
    # feet (dark stubs peeking below the blob)
    c.disc(22, 59, 5, 3), c.disc(42, 59, 5, 3)
    shapes = [
        (32, 38, 20, 21),   # 0 round
        (32, 36, 16, 24),   # 1 tall
        (32, 40, 24, 18),   # 2 wide
        (32, 39, 21, 20),   # 3 pear base
        (32, 40, 18, 18),   # 4 small chubby
        (33, 39, 20, 22),   # 5 bean (leans right)
        (32, 38, 21, 20),   # 6 droopy
        (32, 38, 23, 21),   # 7 plump
    ]
    cx, cy, rx, ry = shapes[i]
    if i == 3:
        c.ellipse(cx, 34, 16, 14, OUT, fill=False)
        c.ellipse(cx, 34, 16, 14, FILL)
    c.ellipse(cx, cy, rx, ry, OUT, fill=False)
    c.ellipse(cx, cy, rx, ry, FILL)
    # bottom shading crescent
    c.ellipse(cx, cy + ry - 5, max(rx - 4, 2), 6, SHADOW)
    # belly patch
    brx = {0: 11, 1: 9, 2: 13, 3: 11, 4: 9, 5: 11, 6: 12, 7: 13}[i]
    c.ellipse(cx, 47, brx, 7, OUT, fill=False)
    c.ellipse(cx, 47, brx, 7, HI)
    if i == 6:   # droopy: little tuft shadow stays in head slot; add stub arms
        c.ellipse(cx - rx + 1, 44, 4, 6, OUT, fill=False)
        c.ellipse(cx + rx - 1, 44, 4, 6, OUT, fill=False)
    return c


def eye_block(c: Canvas, cx, cy, kind: int):
    if kind == 0:      # round dots 2x3
        for dx in range(2):
            for dy in range(3):
                c.set(cx + dx - 1, cy + dy - 1, OUT)
    elif kind == 1:    # big dark eyes
        c.disc(cx, cy, 3, 3)
        c.set(cx - 1, cy - 1, HI)
    elif kind == 2:    # vertical ovals
        c.disc(cx, cy, 2, 3)
    elif kind == 3:    # arches (curious)
        c.hline(cx - 2, cx + 2, cy, OUT)
        c.set(cx - 2, cy - 1, OUT)
        c.set(cx + 2, cy - 1, OUT)
    elif kind == 4:    # happy ^ ^
        for dx in (-2, 0, 2):
            c.set(cx + dx, cy - abs(dx) // 2 + 1, OUT)
    elif kind == 5:    # sparkle
        c.disc(cx, cy, 3, 3)
        c.set(cx, cy, HI)
        c.set(cx - 1, cy, HI)
        c.set(cx + 1, cy, HI)
    elif kind == 6:    # tiny dots
        c.vline(cx, cy - 1, cy + 1, OUT)
    elif kind == 7:    # wide ovals
        c.disc(cx, cy, 3, 2)
    elif kind == 8:    # plus stars
        c.vline(cx, cy - 2, cy + 2, OUT)
        c.hline(cx - 2, cx + 2, cy, OUT)
    elif kind == 9:    # sleepy lower arcs
        c.hline(cx - 2, cx + 2, cy, OUT)
        c.set(cx - 2, cy + 1, OUT)
        c.set(cx + 2, cy + 1, OUT)
    elif kind == 10:  # doubletons
        c.set(cx, cy - 1, OUT)
        c.set(cx, cy + 1, OUT)
    else:             # 11 hollow rings
        c.ellipse(cx, cy, 3, 3, OUT, fill=False)


def eyes(i: int) -> Canvas:
    c = Canvas()
    eye_block(c, 25, 27, i)
    eye_block(c, 39, 27, i)
    return c


def face(i: int) -> Canvas:
    c = Canvas()
    my = 43
    if i in (0, 4):    # straight mouths (0 long, 4 short)
        c.hline(27 if i == 0 else 29, 37 if i == 0 else 35, my, OUT)
    elif i in (1, 5):  # smile, 5 adds blush
        for dx in range(-5, 6):
            c.set(32 + dx, my + abs(dx) // 3, OUT)
        if i == 5:
            for bx in (13, 45):
                c.ellipse(bx, my + 2, 5, 2, FILL)
                c.ellipse(bx, my + 2, 4, 2, HI)
    elif i in (2, 6):  # open mouth, 6 with fang
        c.ellipse(32, my + 2, 3, 4, OUT, fill=False)
        c.ellipse(32, my + 2, 3, 4, OUT)
        c.ellipse(32, my + 2, 2, 3, SHADOW)
        if i == 6:
            c.set(31, my, HI)
    elif i == 3:      # omega / cat mouth
        c.hline(27, 37, my, OUT)
        c.set(29, my + 1, OUT)
        c.set(35, my + 1, OUT)
        c.set(30, my + 2, OUT)
        c.set(34, my + 2, OUT)
    elif i == 7:      # whiskers + tiny mouth
        for side in (-1, 1):
            for k in range(3):
                y = my - 2 + k * 3
                c.hline(32 + side * 16, 32 + side * 26, y, OUT)
        c.set(31, my + 1, OUT)
        c.set(33, my + 1, OUT)
    return c


def head(i: int) -> Canvas:
    c = Canvas()
    if i == 0:      # bear round ears
        c.disc(14, 10, 7, 7, FILL), c.disc(50, 10, 7, 7, FILL)
        c.disc(14, 10, 3, 3, HI), c.disc(50, 10, 3, 3, HI)
    elif i == 1:    # cat triangles
        c.tri(8, 18, 14, 0, 24, 14, OUT), c.tri(40, 14, 50, 0, 56, 18, OUT)
        c.tri(12, 14, 15, 5, 20, 13, HI), c.tri(44, 13, 49, 5, 52, 14, HI)
    elif i == 2:    # floppy ears
        c.disc(10, 18, 5, 10, FILL), c.disc(54, 18, 5, 10, FILL)
    elif i == 3:    # bunny tall ears
        c.disc(18, 8, 4, 12, FILL), c.disc(46, 8, 4, 12, FILL)
        c.ellipse(18, 6, 2, 7, HI), c.ellipse(46, 6, 2, 7, HI)
    elif i == 4:    # little horns
        c.tri(16, 14, 20, 2, 25, 13, OUT), c.tri(39, 13, 44, 2, 48, 14, OUT)
    elif i == 5:    # ahoge
        pts = [(32, 0), (33, 3), (31, 6), (33, 9), (31, 12)]
        for x, y in pts:
            c.set(x, y, OUT)
    elif i in (6, 9):  # beanie, 9 with star badge
        c.ellipse(32, 20, 21, 12, OUT, fill=False)
        c.ellipse(32, 20, 21, 12, SHADOW)
        c.hline(12, 52, 18, OUT)
        c.disc(32, 8, 4, 4, SHADOW)
        if i == 9:
            c.set(32, 12, HI)
            c.vline(32, 10, 14, HI)
            c.hline(30, 34, 12, HI)
    elif i == 7:    # side bow
        c.tri(8, 8, 20, 4, 20, 14, OUT), c.tri(20, 4, 20, 14, 26, 9, OUT)
        c.set(20, 9, HI)
    elif i == 8:    # sprout
        c.vline(32, 4, 14, OUT)
        c.ellipse(26, 4, 6, 3, OUT, fill=False), c.ellipse(26, 4, 6, 3, FILL)
        c.ellipse(38, 4, 6, 3, OUT, fill=False), c.ellipse(38, 4, 6, 3, FILL)
    elif i == 10:   # feather
        for k in range(6):
            c.set(40 + k // 2, 4 + k, OUT)
            c.set(44 + k // 2, 6 + k, SHADOW)
    else:           # 11 legendary crown (AFF_TAG_SPECIAL)
        c.tri(18, 18, 18, 6, 26, 16, OUT)
        c.tri(26, 16, 32, 4, 38, 16, OUT)
        c.tri(38, 16, 46, 6, 46, 18, OUT)
        c.hline(18, 46, 18, OUT)
        for jx in (22, 32, 42):
            c.set(jx, 13, HI)
        c.set(32, 8, HI)
    return c


def back(i: int) -> Canvas:
    c = Canvas()
    if i == 0:      # membrane wings
        c.disc(6, 30, 8, 15, FILL), c.disc(58, 30, 8, 15, FILL)
    elif i == 1:    # feathered angel wings
        for side in (-1, 1):
            bx = 32 + side * 26
            for k in range(3):
                c.ellipse(bx, 24 + k * 8, 7 - k, 7, OUT, fill=False)
                c.ellipse(bx, 24 + k * 8, 7 - k, 7, HI if k == 0 else FILL)
    elif i == 2:    # curled tail (lower right)
        for r, v in ((10, OUT), (8, FILL), (5, OUT), (3, FILL)):
            c.ellipse(50, 50, r, r, OUT, fill=False) if v == OUT else \
                c.ellipse(50, 50, r, r, v)
    elif i == 3:    # ribbon bow at top
        c.tri(20, 4, 32, 8, 20, 14, OUT), c.tri(32, 8, 44, 4, 44, 14, OUT)
        c.disc(32, 9, 3, 3, HI)
    elif i == 4:    # shell on upper back
        c.ellipse(32, 14, 10, 8, OUT, fill=False)
        c.ellipse(32, 14, 10, 8, FILL)
        for k in range(-2, 3):
            c.vline(32 + k * 4, 8, 20, SHADOW)
    elif i == 5:    # bat wings
        for side in (-1, 1):
            bx = 32 + side * 20
            c.ellipse(bx, 22, 12, 8, OUT, fill=False)
            c.ellipse(bx, 22, 12, 8, SHADOW)
            for k in range(3):
                c.tri(bx - 10 + k * 7, 26, bx - 6 + k * 7, 36,
                      bx - 2 + k * 7, 26, OUT)
    elif i == 6:    # back spikes
        for k, x in enumerate((20, 32, 44)):
            c.tri(x - 5, 18, x, 2 + (k % 2) * 3, x + 5, 18, OUT)
    else:           # 7 halo ring
        c.ellipse(32, 6, 8, 4, OUT, fill=False)
        c.ellipse(32, 6, 8, 4, HI)
        c.ellipse(32, 6, 6, 2, TRANS)
    return c


def shift_y(src: Canvas, dy: int) -> Canvas:
    """Whole-frame vertical translation (breath/jump 共用静止层，05 §5.1）。"""
    out = Canvas()
    for y in range(G):
        sy = y - dy
        if 0 <= sy < G:
            out.px[y * G:(y + 1) * G] = src.px[sy * G:(sy + 1) * G]
    return out


def eyes_frame(_idx: int, frame: int) -> Canvas:
    # f0 由各眼型 builder 产出；表情帧是与具体眼型无关的占位通用眉/眼
    # （真美术逐部件补帧，运行期契约不变）。
    c = Canvas()
    for cx in (25, 39):
        if frame == 1:      # blink：短横线
            c.hline(cx - 2, cx + 2, 27, OUT)
        elif frame == 2:    # happy ^_^
            for dx in (-2, 0, 2):
                c.set(cx + dx, 27 - abs(dx) // 2 + 1, OUT)
        elif frame == 3:    # sleep 下弧
            c.hline(cx - 2, cx + 2, 26, OUT)
            c.set(cx - 2, 27, OUT)
            c.set(cx + 2, 27, OUT)
        else:               # 4 sick 加号
            c.vline(cx, 25, 29, OUT)
            c.hline(cx - 2, cx + 2, 27, OUT)
    return c


def face_frame(frame: int) -> Canvas:
    c = Canvas()
    if frame == 1:         # eating：张开的嘴
        c.ellipse(32, 44, 3, 4, OUT, fill=False)
        c.ellipse(32, 44, 3, 4, OUT)
        c.ellipse(32, 45, 2, 3, SHADOW)
    elif frame == 2:       # crying：下撇嘴 + 两行泪
        for dx in range(-4, 5):
            c.set(32 + dx, 47 - abs(dx) // 3, OUT)
        c.vline(23, 43, 48, SHADOW)
        c.vline(41, 43, 48, SHADOW)
        c.set(23, 49, SHADOW)
        c.set(41, 49, SHADOW)
    return c


def frames_for(name: str, idx: int, base: Canvas, count: int) -> list[Canvas]:
    if name == "body":
        return [base, shift_y(base, 1), shift_y(base, -3)]
    if name == "eyes":
        return [base] + [eyes_frame(idx, f) for f in range(1, count)]
    if name == "face":
        return [base] + [face_frame(f) for f in range(1, count)]
    return [base]


BUILDERS = {"body": body, "eyes": eyes, "face": face, "head": head,
            "back": back}


def generate(root: str) -> list[str]:
    written = []
    with open(os.path.join(root, "manifest.json"), encoding="utf-8") as fh:
        manifest = json.load(fh)
    frame_counts = manifest["frames"]
    for slot in manifest["slots"]:
        name = slot["name"]
        nframes = frame_counts[name]
        builder = BUILDERS[name]
        for idx in range(slot["count"]):
            base = builder(idx)
            frames = frames_for(name, idx, base, nframes)
            assert len(frames) == nframes
            d = os.path.join(root, "parts", name, f"{idx:02d}")
            os.makedirs(d, exist_ok=True)
            for f, canvas in enumerate(frames):
                path = os.path.join(d, f"{f}.png")
                write_gray_png(path, G, G, bytes(canvas.px))
                written.append(path)
    return written


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true",
                    help="fail if generated files would change")
    ap.add_argument("--root", default=os.path.dirname(os.path.abspath(__file__))
                    + "/../../assets/pet")
    args = ap.parse_args()
    paths = generate(os.path.abspath(args.root))
    if args.check:
        ok = True
        for path in paths:
            with open(path, "rb") as fh:
                data = fh.read()
            # Rewriting must be byte-stable.
            before = hashlib.sha256(data).hexdigest()
            generate(os.path.abspath(args.root))
            with open(path, "rb") as fh:
                after = hashlib.sha256(fh.read()).hexdigest()
            if before != after:
                print(f"{path}: not byte-stable")
                ok = False
        if not ok:
            return 1
    print(f"generated {len(paths)} part PNGs under {args.root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
