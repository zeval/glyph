#!/usr/bin/env python3
"""Generate PSP EBOOT menu art from the glyph mark."""

from __future__ import annotations

import os
import struct
import zlib


WIDTH = 144
HEIGHT = 80
LOGO_SIZE = 56
SOURCE_PATH = os.path.join("assets", "branding", "glyph-mark.png")
OUT_PATH = os.path.join("assets", "psp", "ICON0.PNG")

WHITE = (255, 255, 255)


def png_chunk(kind: bytes, data: bytes) -> bytes:
    crc = zlib.crc32(kind)
    crc = zlib.crc32(data, crc)
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", crc & 0xFFFFFFFF)


def read_be32(data: bytes, offset: int) -> int:
    return struct.unpack(">I", data[offset : offset + 4])[0]


def paeth(left: int, up: int, up_left: int) -> int:
    estimate = left + up - up_left
    distance_left = abs(estimate - left)
    distance_up = abs(estimate - up)
    distance_up_left = abs(estimate - up_left)
    if distance_left <= distance_up and distance_left <= distance_up_left:
        return left
    if distance_up <= distance_up_left:
        return up
    return up_left


def channels_for_color_type(color_type: int) -> int:
    if color_type == 2:
        return 3
    if color_type == 3:
        return 1
    if color_type == 6:
        return 4
    raise ValueError(f"unsupported PNG color type: {color_type}")


def unfilter(raw: bytes, width: int, height: int, channels: int) -> list[bytearray]:
    stride = width * channels
    rows: list[bytearray] = []
    previous = bytearray(stride)
    offset = 0
    for _ in range(height):
        filter_type = raw[offset]
        offset += 1
        encoded = raw[offset : offset + stride]
        offset += stride
        row = bytearray(stride)
        for i, value in enumerate(encoded):
            left = row[i - channels] if i >= channels else 0
            up = previous[i]
            up_left = previous[i - channels] if i >= channels else 0
            if filter_type == 0:
                row[i] = value
            elif filter_type == 1:
                row[i] = (value + left) & 0xFF
            elif filter_type == 2:
                row[i] = (value + up) & 0xFF
            elif filter_type == 3:
                row[i] = (value + ((left + up) // 2)) & 0xFF
            elif filter_type == 4:
                row[i] = (value + paeth(left, up, up_left)) & 0xFF
            else:
                raise ValueError(f"unsupported PNG filter: {filter_type}")
        rows.append(row)
        previous = row
    return rows


def read_png_rgba(path: str) -> tuple[int, int, list[tuple[int, int, int, int]]]:
    with open(path, "rb") as f:
        data = f.read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"{path} is not a PNG")

    width = 0
    height = 0
    bit_depth = 0
    color_type = 0
    palette: list[tuple[int, int, int]] = []
    alpha: list[int] = []
    compressed = bytearray()

    offset = 8
    while offset + 12 <= len(data):
        size = read_be32(data, offset)
        kind = data[offset + 4 : offset + 8]
        payload = data[offset + 8 : offset + 8 + size]
        offset += size + 12
        if kind == b"IHDR":
            width, height, bit_depth, color_type, _, _, interlace = struct.unpack(
                ">IIBBBBB", payload
            )
            if bit_depth != 8 or interlace != 0:
                raise ValueError("only 8-bit non-interlaced PNGs are supported")
        elif kind == b"PLTE":
            palette = [tuple(payload[i : i + 3]) for i in range(0, len(payload), 3)]
        elif kind == b"tRNS":
            alpha = list(payload)
        elif kind == b"IDAT":
            compressed.extend(payload)
        elif kind == b"IEND":
            break

    channels = channels_for_color_type(color_type)
    raw = zlib.decompress(bytes(compressed))
    rows = unfilter(raw, width, height, channels)
    pixels: list[tuple[int, int, int, int]] = []

    for row in rows:
        for x in range(width):
            i = x * channels
            if color_type == 2:
                pixels.append((row[i], row[i + 1], row[i + 2], 255))
            elif color_type == 3:
                index = row[i]
                r, g, b = palette[index]
                a = alpha[index] if index < len(alpha) else 255
                pixels.append((r, g, b, a))
            elif color_type == 6:
                pixels.append((row[i], row[i + 1], row[i + 2], row[i + 3]))

    return width, height, pixels


def alpha_bounds(
    width: int, height: int, pixels: list[tuple[int, int, int, int]]
) -> tuple[int, int, int, int]:
    xs: list[int] = []
    ys: list[int] = []
    for y in range(height):
        for x in range(width):
            if pixels[y * width + x][3] > 0:
                xs.append(x)
                ys.append(y)
    if not xs:
        raise ValueError("source logo has no visible pixels")
    return min(xs), min(ys), max(xs), max(ys)


def composite_over_white(pixel: tuple[int, int, int, int]) -> tuple[int, int, int]:
    r, g, b, a = pixel
    return (
        (r * a + WHITE[0] * (255 - a) + 127) // 255,
        (g * a + WHITE[1] * (255 - a) + 127) // 255,
        (b * a + WHITE[2] * (255 - a) + 127) // 255,
    )


def set_pixel(pixels: bytearray, x: int, y: int, color: tuple[int, int, int]) -> None:
    offset = (y * WIDTH + x) * 3
    pixels[offset : offset + 3] = bytes(color)


def draw_scaled_logo(canvas: bytearray) -> None:
    source_width, source_height, source_pixels = read_png_rgba(SOURCE_PATH)
    left, top, right, bottom = alpha_bounds(source_width, source_height, source_pixels)
    source_crop_width = right - left + 1
    source_crop_height = bottom - top + 1
    logo_width = LOGO_SIZE
    logo_height = max(1, round(LOGO_SIZE * source_crop_height / source_crop_width))
    if logo_height > LOGO_SIZE:
        logo_height = LOGO_SIZE
        logo_width = max(1, round(LOGO_SIZE * source_crop_width / source_crop_height))

    dst_x = (WIDTH - logo_width) // 2
    dst_y = (HEIGHT - logo_height) // 2
    for y in range(logo_height):
        if logo_height == 1:
            source_y = top
        else:
            source_y = top + round(y * (source_crop_height - 1) / (logo_height - 1))
        for x in range(logo_width):
            if logo_width == 1:
                source_x = left
            else:
                source_x = left + round(x * (source_crop_width - 1) / (logo_width - 1))
            color = composite_over_white(source_pixels[source_y * source_width + source_x])
            set_pixel(canvas, dst_x + x, dst_y + y, color)


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
    draw_scaled_logo(pixels)
    write_png(OUT_PATH, pixels)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
