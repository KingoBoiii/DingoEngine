#!/usr/bin/env python3
"""Compares captures pixel by pixel: the differing-pixel count and the largest channel delta.

    pixel-diff.py A B [--crop x,y,w,h] [--threshold N] [--match REGEX] [--out diff.png]

A and B are PNG files, or two directories whose PNGs are paired by file name. Each pair prints
one line, "<name> diff_px=<n> max_delta=<d> size=<w>x<h>", and a missing or mismatched file
prints an error line instead; --match keeps only the files whose name matches a regex. A pixel
differs when any of its R, G or B channels differs by more than --threshold (default 0). --crop
compares only that rectangle (for the test app, its 3D viewport is 300,24,1005,872). --out
writes a mask of the differing pixels (one file; with directories, a folder of masks). The exit
code is 0 when every pair is identical, 1 when any differs, 2 on an error.

Only the standard library: 8-bit RGB or RGBA, non-interlaced PNGs, which is what
scripts/verify/capture.ps1 writes.
"""

import argparse
import os
import re
import struct
import sys
import zlib


def read_png(path):
    with open(path, "rb") as f:
        data = f.read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"{path}: not a PNG")
    pos = 8
    width = height = channels = None
    idat = []
    while pos < len(data):
        length, kind = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if kind == b"IHDR":
            width, height, depth, color, _, _, interlace = struct.unpack(">IIBBBBB", body)
            if depth != 8 or color not in (2, 6) or interlace != 0:
                raise ValueError(f"{path}: only 8-bit RGB/RGBA non-interlaced PNGs are supported")
            channels = 3 if color == 2 else 4
        elif kind == b"IDAT":
            idat.append(body)
        elif kind == b"IEND":
            break
    raw = zlib.decompress(b"".join(idat))
    return width, height, channels, raw


def unfilter(width, height, channels, raw):
    stride = width * channels
    out = bytearray(stride * height)
    previous = bytearray(stride)
    pos = 0
    for y in range(height):
        kind = raw[pos]
        line = bytearray(raw[pos + 1:pos + 1 + stride])
        pos += 1 + stride
        if kind == 1:
            for i in range(channels, stride):
                line[i] = (line[i] + line[i - channels]) & 0xFF
        elif kind == 2:
            line = bytearray((a + b) & 0xFF for a, b in zip(line, previous))
        elif kind == 3:
            for i in range(stride):
                left = line[i - channels] if i >= channels else 0
                line[i] = (line[i] + ((left + previous[i]) >> 1)) & 0xFF
        elif kind == 4:
            for i in range(stride):
                a = line[i - channels] if i >= channels else 0
                b = previous[i]
                c = previous[i - channels] if i >= channels else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                predictor = a if pa <= pb and pa <= pc else (b if pb <= pc else c)
                line[i] = (line[i] + predictor) & 0xFF
        elif kind != 0:
            raise ValueError(f"unknown PNG filter {kind}")
        out[y * stride:(y + 1) * stride] = line
        previous = line
    return out


def write_mask(path, width, height, mask):
    rows = b"".join(b"\x00" + bytes(mask[y * width:(y + 1) * width]) for y in range(height))

    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 0, 0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(rows, 6)))
        f.write(chunk(b"IEND", b""))


def compare(path_a, path_b, crop, threshold, out):
    wa, ha, ca, raw_a = read_png(path_a)
    wb, hb, cb, raw_b = read_png(path_b)
    if (wa, ha) != (wb, hb):
        raise ValueError(f"sizes differ: {wa}x{ha} and {wb}x{hb}")
    x0, y0, w, h = crop if crop else (0, 0, wa, ha)
    w = min(w, wa - x0)
    h = min(h, ha - y0)

    if ca == cb and raw_a == raw_b:
        if out:
            write_mask(out, w, h, bytearray(w * h))
        return 0, 0, w, h

    pixels_a = unfilter(wa, ha, ca, raw_a)
    pixels_b = unfilter(wb, hb, cb, raw_b)
    differing = 0
    max_delta = 0
    mask = bytearray(w * h) if out else None
    for y in range(y0, y0 + h):
        row_a = (y * wa + x0) * ca
        row_b = (y * wb + x0) * cb
        span_a = pixels_a[row_a:row_a + w * ca]
        span_b = pixels_b[row_b:row_b + w * cb]
        if ca == cb == 3 and span_a == span_b:
            continue
        for x in range(w):
            delta = max(abs(span_a[x * ca + k] - span_b[x * cb + k]) for k in range(3))
            if delta > threshold:
                differing += 1
                if mask is not None:
                    mask[(y - y0) * w + x] = 255
            max_delta = max(max_delta, delta)
    if out:
        write_mask(out, w, h, mask)
    return differing, max_delta, w, h


def main():
    parser = argparse.ArgumentParser(description="Count differing pixels between two captures (or two folders of them).")
    parser.add_argument("a")
    parser.add_argument("b")
    parser.add_argument("--crop", help="x,y,w,h")
    parser.add_argument("--threshold", type=int, default=0, help="ignore channel deltas up to this (default 0)")
    parser.add_argument("--match", help="with folders, compare only the files whose name matches this regex")
    parser.add_argument("--out", help="write a mask of the differing pixels (a folder when comparing folders)")
    args = parser.parse_args()
    crop = tuple(int(v) for v in args.crop.split(",")) if args.crop else None

    if os.path.isdir(args.a) and os.path.isdir(args.b):
        names = sorted(n for n in os.listdir(args.a) if n.lower().endswith(".png"))
        if args.match:
            names = [n for n in names if re.search(args.match, n)]
        pairs = [(n, os.path.join(args.a, n), os.path.join(args.b, n)) for n in names]
        if args.out:
            os.makedirs(args.out, exist_ok=True)
    else:
        pairs = [(os.path.basename(args.b), args.a, args.b)]

    status = 0
    for name, path_a, path_b in pairs:
        out = None
        if args.out:
            out = os.path.join(args.out, name) if len(pairs) > 1 or os.path.isdir(args.out) else args.out
        try:
            differing, max_delta, w, h = compare(path_a, path_b, crop, args.threshold, out)
            print(f"{name} diff_px={differing} max_delta={max_delta} size={w}x{h}")
            if differing:
                status = max(status, 1)
        except (OSError, ValueError, zlib.error) as error:
            print(f"{name} error={error}")
            status = 2
    sys.exit(status)


if __name__ == "__main__":
    main()
