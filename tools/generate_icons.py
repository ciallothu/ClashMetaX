#!/usr/bin/env python3
"""Generate ClashMetaX shield tray/app icons without committing binary assets."""

from __future__ import annotations
import os
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ICON_DIR = ROOT / "src" / "res" / "icons"


def _build_ico(path: Path, fg_rgb: tuple[int, int, int]) -> None:
    width = height = 32
    bg = (0, 0, 0, 0)

    pixels: list[list[int]] = []
    for y in range(height):
        _ = y
        for _x in range(width):
            pixels.append([bg[2], bg[1], bg[0], bg[3]])

    def set_px(x: int, y: int, color: tuple[int, int, int]) -> None:
        if 0 <= x < width and 0 <= y < height:
            idx = (height - 1 - y) * width + x
            pixels[idx] = [color[2], color[1], color[0], 255]

    polygon = [(16, 3), (26, 8), (24, 19), (16, 28), (8, 19), (6, 8)]
    for y in range(height):
        xs: list[float] = []
        for i in range(len(polygon)):
            x1, y1 = polygon[i]
            x2, y2 = polygon[(i + 1) % len(polygon)]
            if y1 == y2:
                continue
            if min(y1, y2) <= y < max(y1, y2):
                t = (y - y1) / (y2 - y1)
                xs.append(x1 + t * (x2 - x1))
        xs.sort()
        for i in range(0, len(xs), 2):
            if i + 1 >= len(xs):
                break
            for x in range(int(xs[i]), int(xs[i + 1]) + 1):
                set_px(x, y, fg_rgb)

    outline = (240, 240, 240)
    for i in range(len(polygon)):
        x1, y1 = polygon[i]
        x2, y2 = polygon[(i + 1) % len(polygon)]
        steps = max(abs(x2 - x1), abs(y2 - y1))
        for s in range(steps + 1):
            x = round(x1 + (x2 - x1) * s / steps)
            y = round(y1 + (y2 - y1) * s / steps)
            set_px(x, y, outline)

    xor = b"".join(bytes(px) for px in pixels)
    row_bytes = ((width + 31) // 32) * 4
    and_mask = b"\x00" * (row_bytes * height)

    bmp_header = struct.pack(
        "<IIIHHIIIIII",
        40,
        width,
        height * 2,
        1,
        32,
        0,
        len(xor) + len(and_mask),
        0,
        0,
        0,
        0,
    )
    image = bmp_header + xor + and_mask

    icon_header = struct.pack("<HHH", 0, 1, 1)
    entry = struct.pack("<BBBBHHII", width, height, 0, 0, 1, 32, len(image), 6 + 16)
    path.write_bytes(icon_header + entry + image)


def main() -> int:
    ICON_DIR.mkdir(parents=True, exist_ok=True)
    _build_ico(ICON_DIR / "proxy-on.ico", (59, 130, 246))
    _build_ico(ICON_DIR / "proxy-off.ico", (120, 120, 120))
    print("Generated icons:")
    print(f"- {ICON_DIR / 'proxy-on.ico'}")
    print(f"- {ICON_DIR / 'proxy-off.ico'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
