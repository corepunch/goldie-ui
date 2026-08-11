#!/usr/bin/env python3
"""One-shot script: normalise all .orion control tags to exact CLASS_DESC names.

Maps lowercase substitutes (e.g. "stack") to their canonical form ("StackView")
so orionc no longer needs alias logic and the codebase has single-canon names.
"""

import re
import sys
from pathlib import Path

# Map from lowercase substitute to canonical CLASS_DESC name.
TAG_MAP = {
    'button':    'Button',
    'checkbox':  'CheckBox',
    'combobox':  'ComboBox',
    'gradient':  'Gradient',
    'image':     'Image',
    'label':     'Label',
    'multiedit': 'MultiEdit',
    'separator': 'Separator',
    'slider':    'Slider',
    'space':     'Space',
    'stack':     'StackView',
    'textedit':  'TextEdit',
}


def tag_re(tag):
    """Return two compiled regexes for a tag: one for opening/self-closing
    (<tag followed by space, /, or >), one for closing (</tag)."""
    # opening/self-closing: <tagname(?=[\s/>])
    # Uses negative lookahead to avoid matching inside longer tag names.
    open_re = re.compile(r'(<)(' + tag + r')(?=[\s/>])')
    close_re = re.compile(r'(</)(' + tag + r')')
    return open_re, close_re


def process_file(path):
    with open(path, 'r') as f:
        content = f.read()

    original = content
    changes = []

    for tag, canonical in sorted(TAG_MAP.items(), key=lambda x: -len(x[0])):
        open_re, close_re = tag_re(tag)

        # Count before
        n_open = len(open_re.findall(content))
        n_close = len(close_re.findall(content))
        if n_open + n_close == 0:
            continue

        content = open_re.sub(rf'\1{canonical}', content)
        content = close_re.sub(rf'\1{canonical}', content)

        n = n_open + n_close
        changes.append(f'  {tag:12s} → {canonical:12s}  ({n_open} open, {n_close} close)')

    if content == original:
        return

    with open(path, 'w') as f:
        f.write(content)

    rel = path.relative_to(Path(__file__).resolve().parent.parent)
    print(f'{rel}')
    for ch in changes:
        print(ch)


def main():
    root = Path(__file__).resolve().parent.parent / 'apps'
    orion_files = sorted(root.rglob('*.orion'))
    for path in orion_files:
        process_file(path)
    print(f'\nDone — scanned {len(orion_files)} .orion files.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
