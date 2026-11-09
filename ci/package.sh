#!/usr/bin/env bash
# Puts build/litetorrent.exe, a short README and the licenses into
# dist/LiteTorrent-Windows-x64.zip, and writes the exe's SHA-256 next to it.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/dist"
PKG="$OUT/LiteTorrent-Windows-x64"

rm -rf "$OUT"
mkdir -p "$PKG/licenses"
cp "$ROOT/build/litetorrent.exe" "$PKG/"
cp "$ROOT/packaging/README.txt" "$PKG/README.txt"
cp "$ROOT/LICENSE" "$ROOT/THIRD-PARTY-NOTICES.md" "$ROOT"/licenses/* "$PKG/licenses/"
(cd "$OUT" && zip -qr9 LiteTorrent-Windows-x64.zip LiteTorrent-Windows-x64)
(cd "$PKG" && sha256sum litetorrent.exe) | tee "$OUT/litetorrent.exe.sha256"
