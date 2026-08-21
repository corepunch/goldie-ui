#!/usr/bin/env bash
# Download iconoir SVGs used by orion-ui into share/icons/.
#
# Usage:
#   ./tools/download_iconoir.sh          (downloads to share/icons/)
#   ./tools/download_iconoir.sh <dir>    (downloads to <dir>)
#
# Requires: curl (macOS/Linux standard)
# Source:   https://github.com/iconoir-icons/iconoir  (icons/regular/)

set -euo pipefail

OUTDIR="${1:-share/icons}"
BASE="https://raw.githubusercontent.com/iconoir-icons/iconoir/main/icons/regular"

mkdir -p "$OUTDIR"

ICONS=(
  # ── Navigation & arrows ──────────────────────────────────────────────────
  arrow-up
  arrow-down
  arrow-left
  arrow-right
  arrow-up-right
  arrow-separate-vertical
  fast-arrow-left
  fast-arrow-right
  fast-arrow-up
  fast-arrow-down
  nav-arrow-up
  nav-arrow-down
  nav-arrow-left
  nav-arrow-right
  rotate-camera-left
  rotate-camera-right
  refresh
  forward
  rewind
  log-in
  log-out
  git-fork
  expand
  collapse
  data-transfer-both

  # ── Core actions ─────────────────────────────────────────────────────────
  check
  check-circle
  xmark
  plus
  plus-circle
  plus-square
  minus
  minus-square
  trash
  copy
  paste-clipboard
  cut
  scissor
  scissor-alt
  undo
  redo
  search
  zoom-in
  zoom-out
  send-diagonal
  switch-off
  switch-on

  # ── Files & folders ──────────────────────────────────────────────────────
  folder
  folder-plus
  folder-settings
  page
  page-edit
  page-plus
  empty-page
  notes
  floppy-disk
  import

  # ── Media & images ───────────────────────────────────────────────────────
  media-image
  media-image-plus
  camera
  multiple-pages
  frame
  mirror
  crop
  expand

  # ── Drawing & editing tools ──────────────────────────────────────────────
  design-nib
  design-pencil
  edit-pencil
  fill-color
  erase
  color-picker
  color-wheel
  color-filter
  text
  text-size
  ruler
  ruler-arrows
  magic-wand
  selective-tool
  select-window
  fingerprint
  droplet
  droplet-half
  triangle
  circle
  square
  square-dashed
  cursor-pointer
  precision-tool
  drag-hand-gesture
  half-cookie
  health-shield
  fire-flame
  brightness
  clock-rotate-right
  hexagon
  pentagon
  palette

  # ── System & UI ──────────────────────────────────────────────────────────
  settings
  wrench
  menu
  view-grid
  dots-grid-3x3
  link
  link-slash
  attachment
  book
  book-stack
  puzzle
  package
  database-check
  database-script-plus
  user-circle
  user-plus
  group

  # ── Media controls ───────────────────────────────────────────────────────
  play
  pause
  square

  # ── Communication ─────────────────────────────────────────────────────────
  chat-bubble
  chat-bubble-empty
  chat-lines
  lock
  lock-slash
  shield-check

  # ── Status & alerts ──────────────────────────────────────────────────────
  info-circle
  warning-triangle
  warning-hexagon
  warning-circle
  help-circle
  question-mark

  # ── Text ─────────────────────────────────────────────────────────────────
  align-center
  list
  numbered-list-left
  input-field
  code
  code-brackets
  language

  # ── Time & location ──────────────────────────────────────────────────────
  clock
  hourglass
  map
  map-pin
  compass

  # ── Misc ─────────────────────────────────────────────────────────────────
  heart
  star
  coins
  eye
  eye-closed
  triangle-flag
  gamepad
  mouse-button-left
  computer
  terminal
  music-note
  music-double-note
  sound-high
  sun-light
  globe
  discord
  linux
  apple-mac
  windows
  twitter
  magnet
  printer
  download
  upload
  drag
  scale-frame-enlarge
  path-arrow
  import
  frame
  page-edit
  page-plus
)

OK=0; FAIL=0; SKIP=0

for name in "${ICONS[@]}"; do
  dest="$OUTDIR/$name.svg"
  [ -f "$dest" ] && { SKIP=$((SKIP+1)); continue; }
  if curl -fsS -o "$dest" "$BASE/$name.svg"; then
    OK=$((OK+1))
  else
    echo "MISSING: $name"
    FAIL=$((FAIL+1))
  fi
done

echo ""
echo "Done: $OK downloaded, $SKIP already present, $FAIL not found in iconoir."
[ $FAIL -gt 0 ] && echo "Add custom SVGs to $OUTDIR/ for any MISSING entries above."
true
