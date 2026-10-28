"""Packs the raw RGBA images from tools/iconbake into a Windows .ico,
each stored as a PNG (Windows Vista and later). Standard library only.

usage: iconbake | python3 -I tools/mkico.py OUT.ico
"""
import struct
import sys
import zlib


def png(size, rgba):
    def chunk(kind, data):
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    rows = b"".join(b"\x00" + rgba[y * size * 4:(y + 1) * size * 4] for y in range(size))
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b""))


def main():
    data = sys.stdin.buffer.read()
    images, pos = [], 0
    while pos < len(data):
        size = struct.unpack("<I", data[pos:pos + 4])[0]
        pos += 4
        images.append((size, png(size, data[pos:pos + size * size * 4])))
        pos += size * size * 4
    out = struct.pack("<HHH", 0, 1, len(images))
    offset = 6 + 16 * len(images)
    for size, blob in images:
        dim = 0 if size >= 256 else size
        out += struct.pack("<BBBBHHII", dim, dim, 0, 0, 1, 32, len(blob), offset)
        offset += len(blob)
    out += b"".join(blob for _, blob in images)
    with open(sys.argv[1], "wb") as f:
        f.write(out)


if __name__ == "__main__":
    main()
