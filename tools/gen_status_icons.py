#!/usr/bin/env python3
"""Generate the anti-aliased VibeOffice status and count circles."""

import argparse
import math
import struct
import zlib
from pathlib import Path

COLORS = {
    "available": (48, 160, 96),
    "busy":      (208, 144, 32),
    "pending":   (128, 128, 128),
    "error":     (224, 32, 32),
}


def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))


def write_circle(path, size, color):
    center = radius = (size - 1) / 2
    rows = bytearray()
    for y in range(size):
        rows.append(0)  # PNG filter: None
        for x in range(size):
            coverage = max(0.0, min(1.0, radius + 0.5 - math.hypot(x - center, y - center)))
            rows.extend((*color, int(coverage * 255)))
    header = struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0)
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b""))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--size", type=int, default=18)
    parser.add_argument("--count-badge-size", type=int, default=26)
    parser.add_argument("--output-dir", type=Path, default=Path(__file__).parents[1] / "examples/vibeoffice/share/artifacts")
    args = parser.parse_args()
    if args.size < 1 or args.count_badge_size < 1:
        parser.error("circle sizes must be positive")
    args.output_dir.mkdir(parents=True, exist_ok=True)
    for name, color in COLORS.items():
        write_circle(args.output_dir / f"status-{name}.png", args.size, color)
    write_circle(args.output_dir / "count-badge.png", args.count_badge_size, COLORS["error"])


if __name__ == "__main__":
    main()
