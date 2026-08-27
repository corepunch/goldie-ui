#!/usr/bin/env python3
import argparse
import struct
from pathlib import Path

ICON_SIZE = 24
SOURCE_CELL_WIDTHS = {"TaskPanel_": 22}

ICONS = {
  "toolbar/undo":      ("Maintoolbar_24", 0),
  "toolbar/redo":      ("Maintoolbar_24", 2),
  "tools/select":      ("Maintoolbar_24", 10),
  "tools/move":        ("Maintoolbar_24", 20),
  "tools/rotate":      ("Maintoolbar_24", 22),
  "tools/scale":       ("Maintoolbar_24", 24),
  "view/frame":        ("ViewportNavigationControls_24", 6),
  "tabs/create":       ("TaskPanel_", 0),
  "tabs/modify":       ("TaskPanel_", 1),
  "tabs/hierarchy":    ("TaskPanel_", 2),
  "tabs/motion":       ("TaskPanel_", 3),
  "tabs/display":      ("TaskPanel_", 4),
  "tabs/utilities":    ("TaskPanel_", 5),
  "primitives/box":    ("Standard_24", 0),
  "primitives/sphere": ("Standard_24", 1),
  "primitives/cylinder": ("Standard_24", 2),
  "primitives/cone":   ("Standard_24", 5),
  "primitives/torus":  ("Standard_24", 3),
  "primitives/prism":  ("Standard_24", 8),
  "primitives/capsule": ("Standard_24", 7),
  "primitives/arch":   ("Standard_24", 0),
  "scene/point-light": ("Lights_24", 2),
  "scene/directional-light": ("Lights_24", 4),
  "scene/camera":      ("Cameras_24", 0),
  "modifiers/taper":   ("Standard_Modifiers_24", 1),
  "modifiers/twist":   ("Standard_Modifiers_24", 3),
  "modifiers/bend":    ("Standard_Modifiers_24", 0),
  "modifiers/stretch": ("Standard_Modifiers_24", 4),
  "modifiers/skew":    ("Standard_Modifiers_24", 2),
  "modifiers/extrude": ("Standard_Modifiers_24", 12),
  "modifiers/mirror":  ("Standard_Modifiers_24", 27),
  "modifiers/noise":   ("Standard_Modifiers_24", 6),
  "modifiers/shell":   ("Standard_Modifiers_24", 35),
  "modifiers/array":   ("Maintoolbar_24", 52),
}

def read_bmp(path):
  data = path.read_bytes()
  if data[:2] != b"BM":
    raise ValueError(f"{path}: not a BMP")
  offset = struct.unpack_from("<I", data, 10)[0]
  width, height = struct.unpack_from("<ii", data, 18)
  bits = struct.unpack_from("<H", data, 28)[0]
  compression = struct.unpack_from("<I", data, 30)[0]
  if compression or bits not in (1, 4, 8, 24) or width <= 0 or height == 0:
    raise ValueError(f"{path}: unsupported BMP encoding")
  rows = abs(height)
  stride = ((width * bits + 31) // 32) * 4
  palette = ()
  if bits in (1, 4, 8):
    palette = tuple((data[p + 2], data[p + 1], data[p]) for p in range(54, offset, 4))
  pixels = []
  for y in range(rows):
    source_y = rows - 1 - y if height > 0 else y
    row = data[offset + source_y * stride:offset + (source_y + 1) * stride]
    if bits == 1:
      values = tuple((byte >> bit) & 1 for byte in row for bit in range(7, -1, -1))
      pixels.append([palette[value] for value in values[:width]])
    elif bits == 4:
      values = tuple(value for byte in row for value in (byte >> 4, byte & 15))
      pixels.append([palette[value] for value in values[:width]])
    elif bits == 8:
      pixels.append([palette[value] for value in row[:width]])
    else:
      pixels.append([(row[x + 2], row[x + 1], row[x]) for x in range(0, width * 3, 3)])
  return width, rows, pixels

def read_strip(source, stem):
  width, height, color = read_bmp(source / f"{stem}i.bmp")
  alpha_width, alpha_height, alpha = read_bmp(source / f"{stem}a.bmp")
  if (width, height) != (alpha_width, alpha_height):
    raise ValueError(f"{stem}: color and alpha strips differ")
  return [[(*color[y][x], alpha[y][x][0]) for x in range(width)] for y in range(height)]

def source_cell_width(stem):
  return SOURCE_CELL_WIDTHS.get(stem, ICON_SIZE)

def pad_icon(icon):
  if len(icon) == ICON_SIZE and len(icon[0]) == ICON_SIZE:
    return icon
  if len(icon) > ICON_SIZE or len(icon[0]) > ICON_SIZE:
    raise ValueError("GMax icon exceeds the Scener icon size")
  left = (ICON_SIZE - len(icon[0])) // 2
  top = (ICON_SIZE - len(icon)) // 2
  result = [[(0, 0, 0, 0) for _ in range(ICON_SIZE)] for _ in range(ICON_SIZE)]
  for y, row in enumerate(icon):
    result[top + y][left:left + len(row)] = row
  return result

def write_bmp(path, pixels):
  height = len(pixels)
  width = len(pixels[0]) if height else 0
  if (width, height) != (ICON_SIZE, ICON_SIZE):
    raise ValueError(f"{path}: expected {ICON_SIZE}x{ICON_SIZE}, got {width}x{height}")
  image_size = width * height * 4
  dib_size = 108
  offset = 14 + dib_size
  header = struct.pack("<2sIHHI", b"BM", offset + image_size, 0, 0, offset)
  dib = struct.pack("<IiiHHIIiiII", dib_size, width, -height, 1, 32, 3,
                    image_size, 0, 0, 0, 0)
  masks = struct.pack("<IIII", 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000)
  rgba = b"".join(bytes((b, g, r, a)) for row in pixels for r, g, b, a in row)
  path.parent.mkdir(parents=True, exist_ok=True)
  path.write_bytes(header + dib + masks + bytes(dib_size - len(dib) - len(masks)) + rgba)

def extract(source, output):
  expected = {Path(f"{name}.bmp") for name in ICONS}
  for path in output.rglob("*.bmp"):
    if path.relative_to(output) not in expected:
      path.unlink()
  strips = {}
  for name, (stem, index) in ICONS.items():
    if stem not in strips:
      strips[stem] = read_strip(source, stem)
    strip = strips[stem]
    cell_width = source_cell_width(stem)
    start = index * cell_width
    icon = [row[start:start + cell_width] for row in strip]
    write_bmp(output / f"{name}.bmp", pad_icon(icon))
  print(f"wrote {len(ICONS)} transparent GMax BMP icons to {output}")

def main():
  parser = argparse.ArgumentParser(description="Export Scener's GMax icons as individual transparent BMPs")
  parser.add_argument("gmax", type=Path, help="GMax installation directory")
  parser.add_argument("output", type=Path, help="Scener icons directory")
  args = parser.parse_args()
  extract(args.gmax / "ui" / "Icons", args.output)

if __name__ == "__main__":
  main()