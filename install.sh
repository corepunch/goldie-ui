#!/bin/sh
set -eu

ORION_REPOSITORY="${ORION_REPOSITORY:-corepunch/orion-ui}"
ORION_PREFIX="${ORION_PREFIX:-/opt/orion}"
ORION_RELEASE="${ORION_RELEASE:-latest}"
ORION_RELEASE_BASE="${ORION_RELEASE_BASE:-}"

case "$(uname -s)" in Darwin) os=macos ;; Linux) os=linux ;; *) echo "Unsupported operating system" >&2; exit 1 ;; esac
case "$(uname -m)" in x86_64|amd64) arch=x86_64 ;; arm64|aarch64) arch=arm64 ;; *) echo "Unsupported architecture" >&2; exit 1 ;; esac
platform="$os-$arch"
if [ -n "$ORION_RELEASE_BASE" ]; then
  base="$ORION_RELEASE_BASE"
elif [ "$ORION_RELEASE" = latest ]; then
  base="https://github.com/$ORION_REPOSITORY/releases/latest/download"
else
  base="https://github.com/$ORION_REPOSITORY/releases/download/$ORION_RELEASE"
fi

tmp=$(mktemp -d "${TMPDIR:-/tmp}/orion-bootstrap.XXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
curl -fsSL "$base/packages-$platform.tsv" -o "$tmp/index.tsv"
record=$(awk -F '\t' '$1 == "orion-core" { print; exit }' "$tmp/index.tsv")
[ -n "$record" ] || { echo "No Orion core package for $platform" >&2; exit 1; }
asset=$(printf '%s\n' "$record" | cut -f4)
expected=$(printf '%s\n' "$record" | cut -f5)
curl -fsSL "$base/$asset" -o "$tmp/core.tar.gz"
if command -v sha256sum >/dev/null 2>&1; then actual=$(sha256sum "$tmp/core.tar.gz" | awk '{print $1}')
else actual=$(shasum -a 256 "$tmp/core.tar.gz" | awk '{print $1}'); fi
[ "$actual" = "$expected" ] || { echo "Orion package checksum mismatch" >&2; exit 1; }
if tar -tzf "$tmp/core.tar.gz" | grep -Eq '(^/|(^|/)\.\.(/|$))'; then
  echo "Unsafe paths in Orion package" >&2
  exit 1
fi
mkdir -p "$ORION_PREFIX" 2>/dev/null || {
  echo "Cannot create $ORION_PREFIX. Re-run with sudo or set ORION_PREFIX=\$HOME/.local" >&2
  exit 1
}
tar -xzf "$tmp/core.tar.gz" -C "$ORION_PREFIX"
mkdir -p "$ORION_PREFIX/.orion/installed"
tar -tzf "$tmp/core.tar.gz" | sed '/\/$/d' > "$ORION_PREFIX/.orion/installed/orion-core.files"
printf '%s\n' "$(printf '%s\n' "$record" | cut -f2)" > "$ORION_PREFIX/.orion/installed/orion-core.version"
printf 'Orion installed in %s\nAdd %s/bin to PATH, then run: orion search\n' "$ORION_PREFIX" "$ORION_PREFIX"