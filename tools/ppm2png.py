#!/usr/bin/env python3
"""Convert binary PPM (P6) images to PNG using only the standard library.

Usage: python3 tools/ppm2png.py OUT_DIR file.ppm [file.ppm ...]
"""
import os
import struct
import sys
import zlib


def read_ppm(path):
    with open(path, "rb") as f:
        data = f.read()
    fields, pos = [], 0
    while len(fields) < 4:
        while data[pos:pos + 1].isspace():
            pos += 1
        start = pos
        while not data[pos:pos + 1].isspace():
            pos += 1
        fields.append(data[start:pos])
    if fields[0] != b"P6" or fields[3] != b"255":
        raise SystemExit(f"{path}: not an 8-bit P6 PPM")
    width, height = int(fields[1]), int(fields[2])
    pixels = data[pos + 1:pos + 1 + width * height * 3]
    if len(pixels) != width * height * 3:
        raise SystemExit(f"{path}: truncated")
    return width, height, pixels


def chunk(kind, body):
    return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)


def write_png(path, width, height, pixels):
    stride = width * 3
    raw = b"".join(b"\x00" + pixels[y * stride:(y + 1) * stride] for y in range(height))
    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(raw, 9))
           + chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(png)


def main(argv):
    if len(argv) < 2:
        raise SystemExit(__doc__)
    out_dir = argv[0]
    os.makedirs(out_dir, exist_ok=True)
    for src in argv[1:]:
        w, h, px = read_ppm(src)
        dst = os.path.join(out_dir, os.path.splitext(os.path.basename(src))[0] + ".png")
        write_png(dst, w, h, px)
        print(dst)


if __name__ == "__main__":
    main(sys.argv[1:])
