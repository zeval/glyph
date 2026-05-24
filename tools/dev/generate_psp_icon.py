#!/usr/bin/env python3
"""Generate PSP EBOOT menu art for glyph."""

from __future__ import annotations

import os
import struct
import zlib


WIDTH = 144
HEIGHT = 80
OUT_PATH = os.path.join("assets", "psp", "ICON0.PNG")

WHITE = (255, 255, 255)
INK = (31, 34, 36)
ACCENT = (74, 151, 154)
MUTED = (204, 216, 216)

FONT = {
    "g": ("01110", "10001", "10000", "10111", "10001", "01111", "00001", "01110"),
    "l": ("11000", "01000", "01000", "01000", "01000", "01000", "11100", "00000"),
    "y": ("10001", "10001", "01010", "00100", "00100", "01000", "10000", "00000"),
    "p": ("11110", "10001", "10001", "11110", "10000", "10000", "10000", "00000"),
    "h": ("10000", "10000", "10000", "11110", "10001", "10001", "10001", "00000"),
}


def set_pixel(pixels: bytearray, x: int, y: int, color: tuple[int, int, int]) -> None:
    if x < 0 or x >= WIDTH or y < 0 or y >= HEIGHT:
        return
    offset = (y * WIDTH + x) * 3
    pixels[offset : offset + 3] = bytes(color)


def fill_rect(
    pixels: bytearray, x: int, y: int, w: int, h: int, color: tuple[int, int, int]
) -> None:
    for yy in range(y, y + h):
        for xx in range(x, x + w):
            set_pixel(pixels, xx, yy, color)


def stroke_rect(
    pixels: bytearray, x: int, y: int, w: int, h: int, color: tuple[int, int, int]
) -> None:
    fill_rect(pixels, x, y, w, 1, color)
    fill_rect(pixels, x, y + h - 1, w, 1, color)
    fill_rect(pixels, x, y, 1, h, color)
    fill_rect(pixels, x + w - 1, y, 1, h, color)


def draw_letter(
    pixels: bytearray, letter: str, x: int, y: int, scale: int, color: tuple[int, int, int]
) -> int:
    pattern = FONT[letter]
    for row, bits in enumerate(pattern):
        for col, bit in enumerate(bits):
            if bit == "1":
                fill_rect(pixels, x + col * scale, y + row * scale, scale, scale, color)
    return x + (len(pattern[0]) + 1) * scale


def draw_text(pixels: bytearray, text: str, x: int, y: int, scale: int) -> None:
    cursor = x
    for letter in text:
        cursor = draw_letter(pixels, letter, cursor, y, scale, INK)


def png_chunk(kind: bytes, data: bytes) -> bytes:
    crc = zlib.crc32(kind)
    crc = zlib.crc32(data, crc)
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", crc & 0xFFFFFFFF)


def write_png(path: str, pixels: bytearray) -> None:
    rows = []
    stride = WIDTH * 3
    for y in range(HEIGHT):
        start = y * stride
        rows.append(b"\x00" + bytes(pixels[start : start + stride]))

    data = b"".join(
        [
            b"\x89PNG\r\n\x1a\n",
            png_chunk(b"IHDR", struct.pack(">IIBBBBB", WIDTH, HEIGHT, 8, 2, 0, 0, 0)),
            png_chunk(b"IDAT", zlib.compress(b"".join(rows), 9)),
            png_chunk(b"IEND", b""),
        ]
    )

    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)


def main() -> int:
    pixels = bytearray(WHITE * WIDTH * HEIGHT)
    stroke_rect(pixels, 6, 6, WIDTH - 12, HEIGHT - 12, MUTED)
    fill_rect(pixels, 16, 58, 112, 4, ACCENT)
    fill_rect(pixels, 118, 54, 10, 12, ACCENT)
    draw_text(pixels, "glyph", 16, 22, 4)
    write_png(OUT_PATH, pixels)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
