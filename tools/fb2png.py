#!/usr/bin/env python3
"""Turns a Kindle framebuffer dump into a gray PNG: a screenshot of the real screen.

    ssh root@KINDLE 'cd /sys/class/graphics/fb0; echo "$(head -1 modes) $(cat bits_per_pixel) $(cat stride)"; cat /dev/fb0' \
        | python3 tools/fb2png.py shot.png [--half]

Input: one text line "U:1272x1696p-0 8 1280" (mode, bits per pixel, bytes per row), then the raw bytes.
--half writes the picture at half width and height (a quarter of the pixels).
"""
import re
import struct
import sys
import zlib


def to_png(header: str, raw: bytes, half: bool = False) -> bytes:
    mode, bpp, stride = header.split()
    w, h = map(int, re.search(r"(\d+)x(\d+)", mode).groups())
    bpp, stride = int(bpp), int(stride)
    if bpp not in (8, 32):
        raise SystemExit(f"fb2png: {bpp} bits per pixel is not supported")
    if len(raw) < stride * h:
        raise SystemExit(f"fb2png: got {len(raw)} bytes, need {stride * h} for {w}x{h}")
    rows = []
    for y in range(h):
        row = raw[y * stride:y * stride + w * bpp // 8]
        if bpp == 32:  # BGRA: green is close enough to gray on an e-ink screen
            row = row[1::4]
        rows.append(row)
    if half:  # mean of each 2x2 block, so small text stays readable
        w, h = w // 2, h // 2
        rows = [bytes((a + b + c + d) >> 2 for a, b, c, d in zip(r0[0:2 * w:2], r0[1:2 * w:2], r1[0:2 * w:2], r1[1:2 * w:2]))
                for r0, r1 in zip(rows[0:2 * h:2], rows[1:2 * h:2])]
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 0, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(b"".join(b"\0" + r for r in rows), 6)) + chunk(b"IEND", b""))


if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if a != "--half"]
    if len(args) != 1:
        raise SystemExit(__doc__)
    data = sys.stdin.buffer.read()
    line, _, raw = data.partition(b"\n")
    with open(args[0], "wb") as f:
        f.write(to_png(line.decode(), raw, "--half" in sys.argv))
