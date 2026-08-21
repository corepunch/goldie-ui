#!/usr/bin/env bash
# Download iconoir SVGs needed by orion-ui into share/icons/.
#
# Usage:
#   ./tools/download_iconoir.sh          (downloads to share/icons/)
#   ./tools/download_iconoir.sh <dir>    (downloads to <dir>)
#
# Requires: curl (macOS/Linux standard)
# Iconoir SVGs live at:
#   https://raw.githubusercontent.com/iconoir-icons/iconoir/main/icons/regular/<name>.svg

set -euo pipefail

OUTDIR="${1:-share/icons}"
BASE_URL="https://raw.githubusercontent.com/iconoir-icons/iconoir/main/icons/regular"

mkdir -p "$OUTDIR"

# All iconoir SVG names referenced by orion-ui (sysicons + picker + imageeditor).
# Run this script once after cloning; re-run to refresh.
ICONS=(
  # ── navigation & arrows ──────────────────────────────────────────────────
  arrow-up
  arrow-down
  arrow-left
  arrow-right
  arrows-horizontal
  arrows-vertical
  arrow-separate-vertical
  fast-arrow-left
  fast-arrow-right
  nav-arrow-up
  nav-arrow-down
  nav-arrow-left
  nav-arrow-right
  rotate-camera-left
  rotate-camera-right
  refresh
  switch
  data-transfer-both
  log-in
  log-out
  git-fork
  submit
  expand
  collapse

  # ── toolbar / actions ────────────────────────────────────────────────────
  check
  xmark
  plus
  minus
  plus-square
  minus-square
  trash
  copy
  paste-clipboard
  scissors
  undo
  redo
  search
  zoom-in
  zoom-out

  # ── files & folders ──────────────────────────────────────────────────────
  folder
  folder-open
  folder-plus
  folder-arrow-up
  file-plus
  file-edit
  page
  notes
  export
  floppy-disk

  # ── media & images ───────────────────────────────────────────────────────
  media-image
  add-media-image
  camera
  camera-plus
  layers
  frame
  mirror
  crop
  expand

  # ── drawing & editing tools ──────────────────────────────────────────────
  brush
  paint-brush-medium
  paint-bucket
  eraser
  pipette
  pencil
  pen
  text
  ruler
  magic-wand
  lasso-pointer
  select-window
  fingerprint
  spray-can
  droplet
  triangle
  circle
  circle-plus
  square
  square-dashed
  cursor
  crosshair
  move-ruler
  half-cookie
  health-shield
  fire
  brightness
  clock-rotate-right
  mountain

  # ── system & UI ──────────────────────────────────────────────────────────
  settings
  wrench
  menu
  grid
  view-grid
  link
  link-slash
  attachment
  tag
  book
  book-stack
  palette
  color-filter
  color-swatch
  puzzle
  package
  shapes

  # ── media controls ───────────────────────────────────────────────────────
  play
  pause
  square
  fast-forward
  rewind

  # ── people & communication ───────────────────────────────────────────────
  user
  user-plus
  edit-user
  chat-bubble
  chat-bubble-text
  lock
  lock-slash
  shield-check

  # ── status & alerts ──────────────────────────────────────────────────────
  info-circle
  warning-triangle
  warning-hexagon
  question-mark
  question-mark-circle

  # ── text ─────────────────────────────────────────────────────────────────
  align-center
  list
  numbered-list-left
  input-field
  font
  code
  code-brackets
  language

  # ── time & location ──────────────────────────────────────────────────────
  clock
  hourglass
  map
  map-pin
  compass

  # ── misc ─────────────────────────────────────────────────────────────────
  heart
  heart-plus
  star
  coins
  eye
  eye-off
  flag
  door
  globe
  joystick
  keyboard
  mouse-button-left
  computer
  terminal
  music-note-beamed
  sound-high
  sun-light
  brightness-off
  hand-gesture
  drag-hand-gesture
  cube
  discord
  linux
  apple-mac
  windows
  twitter
  magnet
  printer
  download
  upload
)

OK=0
FAIL=0
SKIP=0

for name in "${ICONS[@]}"; do
  dest="$OUTDIR/$name.svg"
  if [ -f "$dest" ]; then
    SKIP=$((SKIP + 1))
    continue
  fi
  url="$BASE_URL/$name.svg"
  if curl -fsS -o "$dest" "$url"; then
    OK=$((OK + 1))
  else
    echo "MISSING: $name"
    FAIL=$((FAIL + 1))
  fi
done

echo ""
echo "Done: $OK downloaded, $SKIP already present, $FAIL not found in iconoir."
if [ $FAIL -gt 0 ]; then
  echo "Icons listed as MISSING above have no iconoir equivalent."
  echo "Add custom SVGs to $OUTDIR/ with the same name to fill the gaps."
fi
