#!/usr/bin/env bash
# Downloads the Boost headers and OpenSSL static libraries that LiteTorrent
# links against: the exact MSYS2 packages used for the released builds,
# verified by SHA-256 before they are unpacked.
#
# Usage: ci/fetch-sysroot.sh <dir>   -> SYSROOT is then <dir>/mingw64
set -euo pipefail

DEST="${1:?usage: $0 <dir>}"
MIRROR="https://repo.msys2.org/mingw/mingw64"
PKGS=(
  "mingw-w64-x86_64-boost-1.92.0-4-any.pkg.tar.zst b8f63d98cfccddfdc6bce426440b545ce084ea46a61afb3eab5b1f0ec5b39762"
  "mingw-w64-x86_64-openssl-3.6.5-1-any.pkg.tar.zst 389d1af21e99223d9e2cd32d4c073131b276da85be7636bd8d23571f6938864f"
)

mkdir -p "$DEST/download"
for entry in "${PKGS[@]}"; do
  read -r file sum <<<"$entry"
  curl -fsSL --retry 3 -o "$DEST/download/$file" "$MIRROR/$file"
  echo "$sum  $DEST/download/$file" | sha256sum -c -
  tar --zstd -xf "$DEST/download/$file" -C "$DEST"
done
ls "$DEST/mingw64/lib/libssl.a" "$DEST/mingw64/include/boost/version.hpp"
