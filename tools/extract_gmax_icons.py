#!/usr/bin/env python3
import argparse
import re
import struct
import zlib
from pathlib import Path

TILE = 24
COLS = 32
GENERIC_WIDTHS = {
	"gMaterialED_": 24,
	"TaskPanel_": 22,
}
ALIASES = {
	"SELECT": ("Maintoolbar_24", 10),
	"MOVE": ("Maintoolbar_24", 20),
	"ROTATE": ("Maintoolbar_24", 22),
	"SCALE": ("Maintoolbar_24", 24),
	"BOX": ("Standard_24", 0),
	"SPHERE": ("Standard_24", 1),
	"CYLINDER": ("Standard_24", 2),
	"CONE": ("Standard_24", 5),
	"TORUS": ("Standard_24", 3),
	"PRISM": ("Standard_24", 8),
	"CAPSULE": ("Standard_24", 7),
	"ARCH": ("Standard_24", 0),
	"POINT_LIGHT": ("Lights_24", 2),
	"DIR_LIGHT": ("Lights_24", 4),
	"CAMERA": ("Cameras_24", 0),
	"TAPER": ("Standard_Modifiers_24", 1),
	"TWIST": ("Standard_Modifiers_24", 3),
	"BEND": ("Standard_Modifiers_24", 0),
	"STRETCH": ("Standard_Modifiers_24", 4),
	"SKEW": ("Standard_Modifiers_24", 2),
	"EXTRUDE": ("Standard_Modifiers_24", 12),
	"MIRROR": ("Standard_Modifiers_24", 27),
	"NOISE": ("Standard_Modifiers_24", 6),
	"SHELL": ("Standard_Modifiers_24", 28),
	"ARRAY": ("Standard_Modifiers_24", 31),
	"TAB_CREATE": ("TaskPanel_", 0),
	"TAB_MODIFY": ("TaskPanel_", 1),
	"TAB_HIERARCHY": ("TaskPanel_", 2),
	"TAB_MOTION": ("TaskPanel_", 3),
	"TAB_DISPLAY": ("TaskPanel_", 4),
	"TAB_UTILITIES": ("TaskPanel_", 5),
	"GRID": ("Helpers_24", 3),
	"FRAME": ("ViewportNavigationControls_24", 3),
}

def read_bmp(path):
	data = path.read_bytes()
	if data[:2] != b"BM":
		raise ValueError(f"{path}: not a BMP")
	offset = struct.unpack_from("<I", data, 10)[0]
	width, height = struct.unpack_from("<ii", data, 18)
	bpp = struct.unpack_from("<H", data, 28)[0]
	compression = struct.unpack_from("<I", data, 30)[0]
	if compression or bpp not in (1, 4, 8, 24) or width <= 0 or height == 0:
		raise ValueError(f"{path}: unsupported BMP encoding")
	rows = abs(height)
	stride = ((width * bpp + 31) // 32) * 4
	palette = ()
	if bpp in (1, 4, 8):
		palette = tuple((data[p + 2], data[p + 1], data[p]) for p in range(54, offset, 4))
	pixels = []
	for y in range(rows):
		sy = rows - 1 - y if height > 0 else y
		row = data[offset + sy * stride:offset + (sy + 1) * stride]
		if bpp == 1:
			values = tuple((byte >> bit) & 1 for byte in row for bit in range(7, -1, -1))
			pixels.append([palette[value] for value in values[:width]])
		elif bpp == 4:
			values = tuple(value for byte in row for value in (byte >> 4, byte & 15))
			pixels.append([palette[value] for value in values[:width]])
		elif bpp == 8:
			pixels.append([palette[value] for value in row[:width]])
		else:
			pixels.append([(row[x + 2], row[x + 1], row[x]) for x in range(0, width * 3, 3)])
	return width, rows, pixels

def read_strip(directory, stem):
	w, h, color = read_bmp(directory / f"{stem}i.bmp")
	aw, ah, alpha = read_bmp(directory / f"{stem}a.bmp")
	if (w, h) != (aw, ah):
		raise ValueError(f"{stem}: color and alpha strips differ")
	return w, h, [[(*color[y][x], alpha[y][x][0]) for x in range(w)] for y in range(h)]

def source_stems(directory):
	stems = []
	available = {path.name[:-5] for path in directory.glob("*i.bmp") if path.with_name(path.name[:-5] + "a.bmp").exists()}
	for stem in sorted(available, key=str.casefold):
		if stem.endswith("_16") and stem[:-3] + "_24" in available:
			continue
		stems.append(stem)
	return stems

def source_width(stem, height):
	if stem.endswith("_24"):
		return 24
	if stem.endswith("_16"):
		return 16
	return GENERIC_WIDTHS.get(stem, 16)

def enum_name(stem, index):
	name = re.sub(r"[^A-Za-z0-9]+", "_", stem).strip("_").upper()
	return f"GMAX_ICON_{name}_{index:03d}"

def png_chunk(name, payload):
	return struct.pack(">I", len(payload)) + name + payload + struct.pack(">I", zlib.crc32(name + payload) & 0xffffffff)

def write_png(path, width, height, pixels):
	raw = b"".join(b"\0" + bytes(channel for pixel in row for channel in pixel) for row in pixels)
	header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
	path.parent.mkdir(parents=True, exist_ok=True)
	path.write_bytes(b"\x89PNG\r\n\x1a\n" + png_chunk(b"IHDR", header) +
		png_chunk(b"IDAT", zlib.compress(raw, 9)) + png_chunk(b"IEND", b""))

def write_header(path, entries, lookup, rows):
	lines = [
		"#ifndef GMAX_ICONS_H", "#define GMAX_ICONS_H", "", "enum {",
		f"\tGMAX_ICON_SIZE = {TILE},", f"\tGMAX_ICON_COLS = {COLS},",
		f"\tGMAX_ICON_SHEET_W = {COLS * TILE},", f"\tGMAX_ICON_SHEET_H = {rows * TILE},", "",
	]
	lines.extend(f"\t{name} = {index}," for index, name in enumerate(entries))
	lines.append(f"\tGMAX_ICON_COUNT = {len(entries)},")
	lines.append("")
	for alias, source in ALIASES.items():
		if source not in lookup:
			raise ValueError(f"alias {alias}: missing {source[0]} icon {source[1]}")
		lines.append(f"\tGMAX_ICON_{alias} = {lookup[source]},")
	lines.extend(["};", "", "#endif", ""])
	path.parent.mkdir(parents=True, exist_ok=True)
	path.write_text("\n".join(lines))

def extract(source, output, header):
	icons = []
	entries = []
	lookup = {}
	for stem in source_stems(source):
		strip_w, strip_h, pixels = read_strip(source, stem)
		cell_w = source_width(stem, strip_h)
		if strip_w % cell_w:
			raise ValueError(f"{stem}: width {strip_w} is not divisible by cell width {cell_w}")
		for source_index in range(strip_w // cell_w):
			name = enum_name(stem, source_index)
			lookup[(stem, source_index)] = name
			entries.append(name)
			icons.append((pixels, source_index * cell_w, cell_w, strip_h))
	rows = (len(icons) + COLS - 1) // COLS
	atlas = [[(0, 0, 0, 0) for _ in range(COLS * TILE)] for _ in range(rows * TILE)]
	for atlas_index, (pixels, sx, width, height) in enumerate(icons):
		dx = (atlas_index % COLS) * TILE + (TILE - width) // 2
		dy = (atlas_index // COLS) * TILE + (TILE - height) // 2
		for y in range(min(height, TILE)):
			for x in range(min(width, TILE)):
				atlas[dy + y][dx + x] = pixels[y][sx + x]
	write_png(output, COLS * TILE, rows * TILE, atlas)
	write_header(header, entries, lookup, rows)
	print(f"wrote {len(icons)} icons to {output} and {header}")

def main():
	parser = argparse.ArgumentParser(description="Build the complete 24 px GMax icon atlas and C enum")
	parser.add_argument("gmax", type=Path, help="GMax installation directory")
	parser.add_argument("output", type=Path, help="output PNG atlas")
	parser.add_argument("header", type=Path, help="output C enum header")
	args = parser.parse_args()
	extract(args.gmax / "ui" / "Icons", args.output, args.header)

if __name__ == "__main__":
	main()
