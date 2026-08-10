#!/usr/bin/env python3
"""Convert scene files from meters to centimeters.

Usage:
    python3 tools/convert_to_cm.py [--dry-run] [--revert]

Options:
    --dry-run   Show what would be changed without modifying files
    --revert    Convert from centimeters back to meters
"""

import os
import re
import sys
import glob
import argparse

# Attributes that represent spatial values (in meters)
SPATIAL_ATTRS = {
    'pos', 'size', 'radius', 'height', 'length', 'thickness', 'width', 'sill',
    'majorRadius', 'minorRadius', 'look', 'start', 'end', 'target',
    'radiusTop', 'depth', 'inset', 'weld', 'strength', 'amount',
    'translation', 'offset', 'pivotOffset'
}

# Attributes that should NOT be converted
SKIP_ATTRS = {
    'fov', 'intensity', 'castShadows', 'renderable', 'unlit', 'shininess',
    'color', 'background', 'ambient', 'dir', 'name', 'comment', 'type',
    'id', 'source', 'attach', 'closed', 'smooth', 'x', 'y', 'z',
    'segments', 'angle', 'curvature', 'amplify', 'axis', 'seed', 'count',
    'rings', 'slices', 'sides', 'majorSegments', 'minorSegments',
    'rotation', 'scale', 'pose', 'camera', 'target', 'ref'
}

def convert_value(value: str, factor: float) -> str:
    """Convert a single numeric value."""
    try:
        num = float(value)
        converted = num * factor
        # Format nicely: avoid trailing zeros for integers
        if converted == int(converted):
            return str(int(converted))
        else:
            # Keep reasonable precision
            return f"{converted:.4g}"
    except ValueError:
        return value

def convert_vec(value: str, factor: float) -> str:
    """Convert a vector value like '0.5 1.0 2.0'."""
    parts = value.split()
    converted = [convert_value(p, factor) for p in parts]
    return ' '.join(converted)

def process_attr(name: str, value: str, factor: float) -> str:
    """Process a single attribute, converting if it's spatial."""
    if name in SPATIAL_ATTRS:
        if ' ' in value:
            return convert_vec(value, factor)
        else:
            return convert_value(value, factor)
    return value

def convert_file(filepath: str, factor: float, dry_run: bool = False) -> bool:
    """Convert a single .blk or .blks file."""
    with open(filepath, 'r') as f:
        content = f.read()

    # Pattern to match XML attributes: name="value"
    pattern = r'(\w+)="([^"]*)"'

    def replacer(match):
        attr_name = match.group(1)
        attr_value = match.group(2)
        new_value = process_attr(attr_name, attr_value, factor)
        if new_value != attr_value:
            return f'{attr_name}="{new_value}"'
        return match.group(0)

    new_content = re.sub(pattern, replacer, content)

    if new_content == content:
        return False

    if dry_run:
        print(f"  Would modify: {filepath}")
        # Show a diff-like output
        old_lines = content.splitlines()
        new_lines = new_content.splitlines()
        for i, (old, new) in enumerate(zip(old_lines, new_lines)):
            if old != new:
                print(f"    Line {i+1}:")
                print(f"    - {old}")
                print(f"    + {new}")
    else:
        with open(filepath, 'w') as f:
            f.write(new_content)
        print(f"  Converted: {filepath}")

    return True

def find_scene_files(root_dir: str) -> list:
    """Find all .blk and .blks files."""
    patterns = [
        os.path.join(root_dir, '**', '*.blk'),
        os.path.join(root_dir, '**', '*.blks'),
    ]
    files = []
    for pattern in patterns:
        files.extend(glob.glob(pattern, recursive=True))
    return sorted(files)

def main():
    parser = argparse.ArgumentParser(description='Convert scene files between meters and centimeters')
    parser.add_argument('--dry-run', action='store_true', help='Show changes without modifying files')
    parser.add_argument('--revert', action='store_true', help='Convert from cm back to m')
    args = parser.parse_args()

    # Determine conversion factor
    if args.revert:
        factor = 0.01  # cm to m
        print("Converting from centimeters to meters...")
    else:
        factor = 100.0  # m to cm
        print("Converting from meters to centimeters...")

    # Find the scenes and prefabs directories
    script_dir = os.path.dirname(os.path.abspath(__file__))
    scener_dir = os.path.dirname(script_dir)

    scene_files = find_scene_files(scener_dir)

    if not scene_files:
        print("No .blk or .blks files found!")
        return 1

    print(f"Found {len(scene_files)} scene files to process")

    converted = 0
    for filepath in scene_files:
        if convert_file(filepath, factor, args.dry_run):
            converted += 1

    if args.dry_run:
        print(f"\nDry run: {converted} files would be modified")
    else:
        print(f"\nConverted {converted} files")

    return 0

if __name__ == '__main__':
    sys.exit(main())
