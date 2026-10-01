"""Minimal dependency-free grayscale PNG reader/writer.

Only the subset produced/consumed by the PasPet asset pipeline is supported:
- 8-bit grayscale (color type 0), no interlace, single IDAT (or merged).
Pixel value convention for part art: 0 = transparent, 1..255 = shade ramp.
"""

from __future__ import annotations

import struct
import zlib

PNG_SIG = b"\x89PNG\r\n\x1a\n"


class PngError(ValueError):
    pass


def _chunks(blob: bytes):
    if not blob.startswith(PNG_SIG):
        raise PngError("not a PNG file")
    pos = len(PNG_SIG)
    while pos + 8 <= len(blob):
        (length,) = struct.unpack(">I", blob[pos:pos + 4])
        ctype = blob[pos + 4:pos + 8]
        start = pos + 8
        end = start + length
        if end + 4 > len(blob):
            raise PngError("truncated PNG chunk")
        payload = blob[start:end]
        (crc,) = struct.unpack(">I", blob[end:end + 4])
        if (zlib.crc32(ctype + payload) & 0xFFFFFFFF) != crc:
            raise PngError("bad PNG chunk CRC")
        yield ctype, payload
        pos = end + 4
        if ctype == b"IEND":
            return


def read_gray_png(path: str) -> tuple[int, int, bytearray]:
    """Return (width, height, pixels) for an 8-bit grayscale, non-interlaced PNG."""
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
    if bit_depth != 8 or color_type != 0 or interlace != 0:
        raise PngError(
            f"{path}: only 8-bit grayscale non-interlaced PNG is supported "
            f"(got depth={bit_depth} color={color_type} interlace={interlace})"
        )
    raw = zlib.decompress(bytes(idat))
    stride = width
    expected = (stride + 1) * height
    if len(raw) != expected:
        raise PngError(f"{path}: decompressed size mismatch")
    pixels = bytearray(width * height)
    prev = bytearray(stride)
    src = 0
    for y in range(height):
        ftype = raw[src]
        src += 1
        line = bytearray(raw[src:src + stride])
        src += stride
        if ftype == 1:   # Sub
            for i in range(stride):
                a = line[i - stride] if i >= stride else 0
                line[i] = (line[i] + a) & 0xFF
        elif ftype == 2:  # Up
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 0xFF
        elif ftype == 3:  # Average
            for i in range(stride):
                a = line[i - stride] if i >= stride else 0
                line[i] = (line[i] + ((a + prev[i]) // 2)) & 0xFF
        elif ftype == 4:  # Paeth
            for i in range(stride):
                a = line[i - stride] if i >= stride else 0
                b = prev[i]
                c = prev[i - stride] if i >= stride else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pred = a if pa <= pb and pa <= pc else b if pb <= pc else c
                line[i] = (line[i] + pred) & 0xFF
        elif ftype != 0:
            raise PngError(f"{path}: unsupported filter {ftype}")
        pixels[y * stride:(y + 1) * stride] = line
        prev = line
    return width, height, pixels


def _chunk(ctype: bytes, payload: bytes) -> bytes:
    return (
        struct.pack(">I", len(payload))
        + ctype
        + payload
        + struct.pack(">I", zlib.crc32(ctype + payload) & 0xFFFFFFFF)
    )


def write_gray_png(path: str, width: int, height: int, pixels: bytes) -> None:
    """Write an 8-bit grayscale PNG using filter-0 scanlines."""
    if len(pixels) != width * height:
        raise PngError("pixel buffer size mismatch")
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 0, 0, 0, 0)
    raw = bytearray()
    for y in range(height):
        raw.append(0)
        raw.extend(pixels[y * width:(y + 1) * width])
    blob = PNG_SIG + _chunk(b"IHDR", ihdr)
    blob += _chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    blob += _chunk(b"IEND", b"")
    with open(path, "wb") as fh:
        fh.write(blob)
