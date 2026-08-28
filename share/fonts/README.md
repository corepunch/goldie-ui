# Orion fonts

Orion's proportional UI text uses the hinted Noto Sans faces in this directory:

- `NotoSans-Medium.ttf` at 14 px for window chrome and controls
- `NotoSans-Regular.ttf` at 13 px for content and 12 px for compact labels

Noto Sans is distributed under the SIL Open Font License 1.1 in
`NotoSans-OFL.txt`. Its Latin, Greek, and Cyrillic coverage is suitable for the
framework's general UI text. Glyphs are rasterized from the TTF files into empty
runtime atlases on first use; generated bitmap font sheets are not source
assets.

`monoid.ttf` remains the fixed-width face for the terminal/VGA text renderer.
