"""Writes the raw RGBA images from tools/iconbake as a macOS .iconset
folder (PNG files with Apple's names), for iconutil. Standard library only.

usage: iconbake 16 32 64 128 256 512 1024 | python3 -I tools/mkicns.py OUT.iconset
"""
import os
import struct
import sys
import zlib

NAMES = {16: ["icon_16x16.png"], 32: ["icon_16x16@2x.png", "icon_32x32.png"], 64: ["icon_32x32@2x.png"],
         128: ["icon_128x128.png"], 256: ["icon_128x128@2x.png", "icon_256x256.png"],
         512: ["icon_256x256@2x.png", "icon_512x512.png"], 1024: ["icon_512x512@2x.png"]}


def png(size, rgba):
    def chunk(kind, data):
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    rows = b"".join(b"\x00" + rgba[y * size * 4:(y + 1) * size * 4] for y in range(size))
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b""))


def main():
    data = sys.stdin.buffer.read()
    os.makedirs(sys.argv[1], exist_ok=True)
    pos = 0
    while pos < len(data):
        size = struct.unpack("<I", data[pos:pos + 4])[0]
        pos += 4
        blob = png(size, data[pos:pos + size * size * 4])
        pos += size * size * 4
        for name in NAMES.get(size, []):
            with open(os.path.join(sys.argv[1], name), "wb") as f:
                f.write(blob)


if __name__ == "__main__":
    main()
