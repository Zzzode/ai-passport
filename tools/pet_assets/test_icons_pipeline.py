#!/usr/bin/env python3
"""Host tests for the PasPet UI icon pipeline (RGBA IO + BGRA build)."""

from __future__ import annotations

import os
import struct
import sys
import tempfile
import unittest
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import build_icons  # noqa: E402
from png_rgba import PngError, read_rgba_png  # noqa: E402

ASSET_ROOT = os.path.normpath(os.path.join(HERE, "..", "..", "assets", "pet"))


def _chunk(ctype: bytes, payload: bytes) -> bytes:
    return (struct.pack(">I", len(payload)) + ctype + payload
            + struct.pack(">I", zlib.crc32(ctype + payload) & 0xFFFFFFFF))


def write_rgba_png(path: str, w: int, h: int, rgba: bytes,
                   color_type: int = 6, bit_depth: int = 8) -> None:
    ihdr = struct.pack(">IIBBBBB", w, h, bit_depth, color_type, 0, 0, 0)
    raw = bytearray()
    ch = {0: 1, 2: 3, 6: 4}[color_type]
    for y in range(h):
        raw.append(0)
        raw.extend(rgba[y * w * ch:(y + 1) * w * ch])
    blob = (b"\x89PNG\r\n\x1a\n" + _chunk(b"IHDR", ihdr)
            + _chunk(b"IDAT", zlib.compress(bytes(raw), 9))
            + _chunk(b"IEND", b""))
    with open(path, "wb") as fh:
        fh.write(blob)


class PngRgbaTests(unittest.TestCase):
    def test_rgba_roundtrip_filters(self) -> None:
        # Distinct R,G,B,A so byte order mistakes fail loudly.
        flat = bytes(v for y in range(5) for x in range(3)
                     for v in ((x * 40) & 0xFF, (y * 70) & 0xFF,
                               (x + y * 3) & 0xFF, 255 - ((x + y) & 0xFF)))
        with tempfile.TemporaryDirectory() as td:
            p = os.path.join(td, "a.png")
            write_rgba_png(p, 3, 5, flat)
            w, h, got = read_rgba_png(p)
            self.assertEqual((w, h), (3, 5))
            self.assertEqual(bytes(got), flat)

    def test_rejects_gray_png(self) -> None:
        raw = b"\x00\x80"
        ihdr = struct.pack(">IIBBBBB", 1, 1, 8, 0, 0, 0, 0)
        blob = (b"\x89PNG\r\n\x1a\n" + _chunk(b"IHDR", ihdr)
                + _chunk(b"IDAT", zlib.compress(raw)) + _chunk(b"IEND", b""))
        with tempfile.TemporaryDirectory() as td:
            p = os.path.join(td, "gray.png")
            open(p, "wb").write(blob)
            with self.assertRaises(PngError):
                read_rgba_png(p)


class BuildIconsTests(unittest.TestCase):
    def test_real_assets_build_and_swap_bgra(self) -> None:
        # The shipped manifest + PNGs must build cleanly.
        images, total, dock, small, deco = build_icons.load_icons(ASSET_ROOT)
        with tempfile.TemporaryDirectory() as td:
            build_icons.emit(images, total, dock, small, deco, td)
            header = open(os.path.join(td, "pet_ui_icons_data.h"),
                          encoding="utf-8").read()
            source = open(os.path.join(td, "pet_ui_icons_data.c"),
                          encoding="utf-8").read()
        self.assertIn("pet_ui_dock_feed_20", header)
        self.assertIn("pet_ui_small_moon", header)
        self.assertIn("pet_ui_deco_sun30", header)
        self.assertIn("pet_ui_deco_moon26", header)
        self.assertIn("1600u", header)  # 20x20x4
        self.assertIn("3600u", header)  # 30x30x4 decal
        self.assertIn("2704u", header)  # 26x26x4 moon decal
        # 12 dock icons x 2 sizes + 8 small + 3 deco = 35 bitmaps.
        self.assertEqual(source.count("] = {"), 35)

    def test_bgra_byte_order(self) -> None:
        # Synthetic 1x1 red opaque RGBA must land as B,G,R,A = 00,00,ff,ff.
        with tempfile.TemporaryDirectory() as td:
            os.makedirs(os.path.join(td, "ui-icons", "dock"))
            os.makedirs(os.path.join(td, "ui-icons", "small"))
            os.makedirs(os.path.join(td, "ui-icons", "deco"))
            import json
            with open(os.path.join(td, "ui-icons", "manifest.json"),
                      "w") as fh:
                json.dump({"format_version": 1, "dock": ["red"],
                           "small": [], "deco": []}, fh)
            write_rgba_png(os.path.join(td, "ui-icons", "dock", "red-20.png"),
                           20, 20, bytes([255, 0, 0, 255]) * 400)
            write_rgba_png(os.path.join(td, "ui-icons", "dock", "red-25.png"),
                           25, 25, bytes([0, 255, 0, 128]) * 625)
            images, total, _, _, _ = build_icons.load_icons(td)
        feed20 = next(px for sym, _, _, px in images
                      if sym == "pet_ui_dock_red_20")
        feed25 = next(px for sym, _, _, px in images
                      if sym == "pet_ui_dock_red_25")
        self.assertEqual(feed20[:4], b"\x00\x00\xff\xff")
        self.assertEqual(feed25[:4], b"\x00\xff\x00\x80")
        self.assertEqual(total, 20 * 20 * 4 + 25 * 25 * 4)

    def test_wrong_size_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            os.makedirs(os.path.join(td, "ui-icons", "dock"))
            os.makedirs(os.path.join(td, "ui-icons", "small"))
            os.makedirs(os.path.join(td, "ui-icons", "deco"))
            import json
            with open(os.path.join(td, "ui-icons", "manifest.json"),
                      "w") as fh:
                json.dump({"format_version": 1, "dock": ["bad"],
                           "small": [], "deco": []}, fh)
            write_rgba_png(os.path.join(td, "ui-icons", "dock", "bad-20.png"),
                           21, 20, bytes(21 * 20 * 4))
            write_rgba_png(os.path.join(td, "ui-icons", "dock", "bad-25.png"),
                           25, 25, bytes(25 * 25 * 4))
            with self.assertRaises(SystemExit):
                build_icons.load_icons(td)


if __name__ == "__main__":
    unittest.main()
