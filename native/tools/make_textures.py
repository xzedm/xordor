#!/usr/bin/env python3
"""Generates the sample textures in assets/textures/.

The engine ships its own textures so the repository has no licensing
questions and needs no downloads. They are made from code with plain
Python (zlib and struct only), so anyone can tweak and regenerate them:

    python3 tools/make_textures.py

Output (all RGBA PNGs):
    assets/textures/crate.png     a wooden crate face, 128x128
    assets/textures/checker.png   a gray checkerboard for floors, 256x256, 8x8 cells
    assets/textures/barrel.png    wooden staves with two metal bands, 128x128
"""

import math
import os
import random
import struct
import zlib

SIZE = 128


def write_png(path, width, height, rgba_rows):
    """Writes an 8-bit RGBA PNG. rgba_rows is a list of bytearrays, top row first."""

    def chunk(kind, data):
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    # Each row starts with a filter byte; 0 means "no filter".
    raw = b"".join(b"\x00" + bytes(row) for row in rgba_rows)
    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)  # 8 bits, RGBA
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def mix(a, b, t):
    return tuple(int(round(x + (y - x) * t)) for x, y in zip(a, b))


def crate_pixel(x, y, grain):
    light_wood = (196, 148, 92)
    dark_wood = (122, 84, 46)
    frame_wood = (150, 104, 60)
    gap = (52, 34, 20)

    border = 14
    in_frame = x < border or y < border or x >= SIZE - border or y >= SIZE - border
    # The diagonal brace from bottom-left to top-right, 16 pixels wide.
    on_brace = abs((SIZE - 1 - y) - x) < 11 and not in_frame

    if in_frame:
        color = frame_wood
        # Dark seams where frame boards meet the panel.
        if x in (border - 1, SIZE - border) or y in (border - 1, SIZE - border):
            return gap
    elif on_brace:
        color = mix(frame_wood, light_wood, 0.3)
        if abs((SIZE - 1 - y) - x) >= 10:
            return gap
    else:
        # Horizontal planks inside the frame, separated by thin gaps.
        plank = (y - border) // 25
        if (y - border) % 25 == 0:
            return gap
        color = mix(light_wood, dark_wood, 0.15 * (plank % 2))

    # Wood grain: stretched noise along x, plus a few darker streaks.
    streak = 0.5 + 0.5 * math.sin(y * 0.9 + math.sin(x * 0.05) * 3.0)
    t = 0.18 * grain[y][x] + 0.10 * streak
    color = mix(color, dark_wood, t)
    # Nails in the frame corners.
    for cx, cy in ((7, 7), (SIZE - 8, 7), (7, SIZE - 8), (SIZE - 8, SIZE - 8)):
        if (x - cx) ** 2 + (y - cy) ** 2 <= 5:
            return (70, 70, 74)
    return color


def make_crate(path):
    rng = random.Random(42)  # fixed seed: the same file every run
    # Grain noise: random per row, smoothed along x so it looks like fibers.
    grain = []
    for _ in range(SIZE):
        row, value = [], rng.random()
        for _ in range(SIZE):
            value = value * 0.92 + rng.random() * 0.08
            row.append(value)
        grain.append(row)

    rows = []
    for y in range(SIZE):
        row = bytearray()
        for x in range(SIZE):
            r, g, b = crate_pixel(x, y, grain)
            row += bytes((r, g, b, 255))
        rows.append(row)
    write_png(path, SIZE, SIZE, rows)


def make_checker(path, size=256, cells=8):
    light, dark = (140, 140, 140), (107, 107, 107)  # 0.55 and 0.42 of 255
    rows = []
    for y in range(size):
        row = bytearray()
        for x in range(size):
            c = light if ((x * cells // size) + (y * cells // size)) % 2 == 0 else dark
            row += bytes((*c, 255))
        rows.append(row)
    write_png(path, size, size, rows)


def make_barrel(path):
    """u runs around the barrel, v from bottom to top (image top = barrel top)."""
    rng = random.Random(7)
    staves = 12
    shades = [0.85 + 0.3 * rng.random() for _ in range(staves)]
    wood = (150, 98, 52)
    metal = (78, 80, 86)
    rows = []
    for y in range(SIZE):
        row = bytearray()
        for x in range(SIZE):
            stave = x * staves // SIZE
            if 18 <= y < 28 or 100 <= y < 110:  # metal bands
                c = metal if y not in (18, 27, 100, 109) else (50, 50, 55)
            elif x % (SIZE // staves) == 0:  # dark gap between staves
                c = (60, 38, 20)
            else:
                k = shades[stave] * (0.92 + 0.08 * math.sin(y * 0.7 + stave))
                c = tuple(min(255, int(v * k)) for v in wood)
            row += bytes((*c, 255))
        rows.append(row)
    write_png(path, SIZE, SIZE, rows)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out_dir = os.path.join(here, "..", "assets", "textures")
    os.makedirs(out_dir, exist_ok=True)
    for name, maker in (("crate.png", make_crate), ("checker.png", make_checker), ("barrel.png", make_barrel)):
        path = os.path.normpath(os.path.join(out_dir, name))
        maker(path)
        print("wrote", path)


if __name__ == "__main__":
    main()
