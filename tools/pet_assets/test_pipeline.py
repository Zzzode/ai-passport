#!/usr/bin/env python3
"""Host tests for the PasPet asset pipeline (PNG IO + RLE + build)."""

from __future__ import annotations

import json
import os
import struct
import sys
import tempfile
import unittest
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import build_parts  # noqa: E402
import gen_placeholders  # noqa: E402
from png_io import PngError, read_gray_png, write_gray_png  # noqa: E402


class PngTests(unittest.TestCase):
    def test_gray_roundtrip(self) -> None:
        px = bytes((x * 3 + y * 7) & 0xFF for y in range(16) for x in range(11))
        with tempfile.TemporaryDirectory() as td:
            p = os.path.join(td, "a.png")
            write_gray_png(p, 11, 16, px)
            w, h, got = read_gray_png(p)
            self.assertEqual((w, h), (11, 16))
            self.assertEqual(bytes(got), px)

    def test_rejects_rgb_png(self) -> None:
        # Hand-craft an 8-bit RGB (color type 2) 1x1 image.
        raw = b"\x00\xff\x00\x00"
        ihdr = struct.pack(">IIBBBBB", 1, 1, 8, 2, 0, 0, 0)
        def chunk(t, d):
            import struct as s
            return s.pack(">I", len(d)) + t + d + s.pack(
                ">I", zlib.crc32(t + d) & 0xFFFFFFFF)
        blob = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr)
                + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))
        with tempfile.TemporaryDirectory() as td:
            p = os.path.join(td, "rgb.png")
            open(p, "wb").write(blob)
            with self.assertRaises(PngError):
                read_gray_png(p)

    def test_bad_crc(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            p = os.path.join(td, "a.png")
            write_gray_png(p, 2, 2, b"\x01\x02\x03\x04")
            blob = bytearray(open(p, "rb").read())
            blob[-10] ^= 0xFF
            open(p, "wb").write(bytes(blob))
            with self.assertRaises(PngError):
                read_gray_png(p)


class RleTests(unittest.TestCase):
    def test_roundtrip(self) -> None:
        px = bytes((i * 13) & 0xFF for i in range(4096))
        enc = build_parts.rle_encode(px)
        self.assertLessEqual(len(enc), len(px) * 2)
        dec = bytearray()
        for i in range(0, len(enc), 2):
            dec.extend([enc[i + 1]] * enc[i])
        self.assertEqual(bytes(dec), px)

    def test_long_run_splits_at_255(self) -> None:
        enc = build_parts.rle_encode(b"\x2a" * 600)
        self.assertEqual(enc, bytes([255, 0x2A, 255, 0x2A, 90, 0x2A]))


class BuildTests(unittest.TestCase):
    def test_generate_and_build(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            root = os.path.join(td, "pet")
            os.makedirs(root)
            with open(os.path.join(
                    os.path.dirname(gen_placeholders.__file__),
                    "..", "..", "assets", "pet", "manifest.json"),
                    encoding="utf-8") as fh:
                manifest = json.load(fh)
            with open(os.path.join(root, "manifest.json"), "w",
                      encoding="utf-8") as fh:
                json.dump(manifest, fh)
            import subprocess
            repo = os.path.normpath(os.path.join(HERE, "..", ".."))
            subprocess.run(
                [sys.executable,
                 os.path.join(repo, "tools", "pet_assets",
                              "gen_placeholders.py"),
                 "--root", root], check=True, capture_output=True)
            out = os.path.join(td, "gen")
            subprocess.run(
                [sys.executable,
                 os.path.join(repo, "tools", "pet_assets", "build_parts.py"),
                 "--root", root, "--out", out], check=True,
                capture_output=True)
            h = open(os.path.join(out, "pet_parts_data.h"),
                     encoding="utf-8").read()
            c = open(os.path.join(out, "pet_parts_data.c"),
                     encoding="utf-8").read()
            self.assertIn("PET_PARTS_CANVAS 128", h)
            frames_cfg = manifest["frames"]
            for name, count in (("body", 8), ("eyes", 12), ("face", 8),
                                ("head", 12), ("back", 8)):
                slot_dir = os.path.join(root, "parts", name)
                part_dirs = sorted(n for n in os.listdir(slot_dir)
                                   if os.path.isdir(os.path.join(slot_dir, n)))
                self.assertEqual(len(part_dirs), count)
                for pd in part_dirs:
                    pngs = [n for n in os.listdir(os.path.join(slot_dir, pd))
                            if n.endswith(".png")]
                    self.assertEqual(len(pngs), frames_cfg[name])
            self.assertIn("case 0: return 8;", c)
            # 占位资产远小于 1.2 MiB 部件预算。
            self.assertLess(os.path.getsize(
                os.path.join(out, "pet_parts_data.c")), 200_000)

    def test_byte_stable_generation(self) -> None:
        import subprocess
        repo = os.path.normpath(os.path.join(HERE, "..", ".."))
        r = subprocess.run(
            [sys.executable,
             os.path.join(repo, "tools", "pet_assets",
                          "gen_placeholders.py"), "--check"],
            capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)


if __name__ == "__main__":
    unittest.main()
