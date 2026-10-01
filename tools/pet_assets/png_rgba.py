"""Minimal dependency-free RGBA PNG reader.

Only the subset produced by pet-redesign/export-icons.mjs is supported:
- 8-bit RGBA (color type 6), no interlace; all five adaptive filters.
Returned pixels are straight (non-premultiplied) RGBA bytes, 4 per pixel.
"""

from __future__ import annotations

import struct
import zlib

from png_io import PNG_SIG, PngError, _chunks  # noqa: E402


def read_rgba_png(path: str) -> tuple[int, int, bytearray]:
    """Return (width, height, rgba) for an 8-bit RGBA, non-interlaced PNG."""
    with open(path, "rb") as fh:
        blob = fh.read()
    width = height = bit_depth = color_type = interlace = None
    idat = bytearray()
    for ctype, payload in _chunks(blob):
        if ctype == b"IHDR":
            (width, height, bit_depth, color_type, comp, filt,
             interlace) = struct.unpack(">IIBBBBB", payload)
            if comp != 0 or filt != 0:
                raise PngError(f"{path}: unsupported compression/filter")
        elif ctype == b"IDAT":
            idat.extend(payload)
    if width is None:
        raise PngError(f"{path}: missing IHDR")
    if bit_depth != 8 or color_type != 6 or interlace != 0:
        raise PngError(
            f"{path}: only 8-bit RGBA non-interlaced PNG is supported "
            f"(got depth={bit_depth} color={color_type} interlace={interlace})"
        )
    raw = zlib.decompress(bytes(idat))
    bpp = 4
    stride = width * bpp
    expected = (stride + 1) * height
    if len(raw) != expected:
        raise PngError(f"{path}: decompressed size mismatch")
    pixels = bytearray(width * height * 4)
    prev = bytearray(stride)
    src = 0

    def paeth(a: int, b: int, c: int) -> int:
        p = a + b - c
        pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
        return a if pa <= pb and pa <= pc else b if pb <= pc else c

    for y in range(height):
        ftype = raw[src]
        src += 1
        line = bytearray(raw[src:src + stride])
        src += stride
        if ftype == 1:      # Sub
            for i in range(stride):
                a = line[i - bpp] if i >= bpp else 0
                line[i] = (line[i] + a) & 0xFF
        elif ftype == 2:    # Up
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 0xFF
        elif ftype == 3:    # Average
            for i in range(stride):
                a = line[i - bpp] if i >= bpp else 0
                line[i] = (line[i] + ((a + prev[i]) // 2)) & 0xFF
        elif ftype == 4:    # Paeth
            for i in range(stride):
                a = line[i - bpp] if i >= bpp else 0
                b = prev[i]
                c = prev[i - bpp] if i >= bpp else 0
                line[i] = (line[i] + paeth(a, b, c)) & 0xFF
        elif ftype != 0:
            raise PngError(f"{path}: unsupported filter {ftype}")
        pixels[y * stride:(y + 1) * stride] = line
        prev = line
    return width, height, pixels
