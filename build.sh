#!/usr/bin/env bash
# Cross-compiles LiteTorrent for Windows x64 from Linux into one static exe.
#
# Requirements (Debian/Ubuntu package names):
#   g++-mingw-w64-x86-64-posix  cmake  ninja-build  git
# plus a "sysroot" directory holding, built for mingw-w64 x86_64:
#   - Boost headers            (include/boost/...)
#   - OpenSSL static libraries (lib/libssl.a, lib/libcrypto.a, include/openssl/...)
# The published binary used the MSYS2 packages mingw-w64-x86_64-boost 1.92.0-4
# and mingw-w64-x86_64-openssl 3.6.5-1, unpacked so that SYSROOT is the
# "mingw64" directory inside them.
#
# Usage:  SYSROOT=/path/to/mingw64 ./build.sh
set -euo pipefail

: "${SYSROOT:?Set SYSROOT to the mingw64 directory containing Boost headers and OpenSSL static libs}"
ROOT="$(cd "$(dirname "$0")" && pwd)"
LT_TAG=v2.0.11
CXX=x86_64-w64-mingw32-g++
JOBS="${JOBS:-$(nproc)}"

# Features compiled out of libtorrent. The app must be compiled with the
# very same macros, otherwise the two disagree about class layouts.
LT_EXTRA="-DTORRENT_DISABLE_ALERT_MSG -DTORRENT_DISABLE_PREDICTIVE_PIECES -DTORRENT_DISABLE_SHARE_MODE -DTORRENT_DISABLE_SUPERSEEDING"

mkdir -p "$ROOT/deps" "$ROOT/build/obj"

# 1. libtorrent, trimmed and static -----------------------------------------
if [ ! -d "$ROOT/deps/libtorrent" ]; then
  git clone --depth 1 --branch "$LT_TAG" --recurse-submodules --shallow-submodules \
    https://github.com/arvidn/libtorrent.git "$ROOT/deps/libtorrent"
fi

cat > "$ROOT/build/toolchain.cmake" <<TC
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH $SYSROOT)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
TC

if [ ! -f "$ROOT/build/libtorrent/libtorrent-rasterbar.a" ]; then
  cmake -G Ninja -S "$ROOT/deps/libtorrent" -B "$ROOT/build/libtorrent" \
    -DCMAKE_TOOLCHAIN_FILE="$ROOT/build/toolchain.cmake" \
    -DCMAKE_BUILD_TYPE=MinSizeRel -DBUILD_SHARED_LIBS=OFF \
    -Ddeprecated-functions=OFF -Dlogging=OFF -Di2p=OFF \
    -Dmutable-torrents=OFF -Dstreaming=OFF \
    -DOPENSSL_ROOT_DIR="$SYSROOT" -DOPENSSL_USE_STATIC_LIBS=ON -DBOOST_ROOT="$SYSROOT" \
    -DCMAKE_CXX_FLAGS="-ffunction-sections -fdata-sections $LT_EXTRA"
  ninja -C "$ROOT/build/libtorrent" -j"$JOBS"
fi

# 2. The app -----------------------------------------------------------------
LT_DEFS="-DBOOST_ASIO_ENABLE_CANCELIO -DBOOST_ASIO_HAS_STD_CHRONO -DBOOST_ASIO_NO_DEPRECATED \
-DBOOST_EXCEPTION_DISABLE -DOPENSSL_NO_SSL2 -DTORRENT_DISABLE_LOGGING \
-DTORRENT_DISABLE_MUTABLE_TORRENTS -DTORRENT_DISABLE_STREAMING -DTORRENT_NO_DEPRECATE \
-DTORRENT_SSL_PEERS -DTORRENT_USE_I2P=0 -DTORRENT_USE_LIBCRYPTO -DTORRENT_USE_OPENSSL \
-DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=0x0A00 $LT_EXTRA"

for f in "$ROOT"/src/*.cpp; do
  $CXX -std=c++17 -Os -ffunction-sections -fdata-sections -DUNICODE -D_UNICODE $LT_DEFS \
    -I"$ROOT/src" -I"$ROOT/deps/libtorrent/include" -I"$SYSROOT/include" \
    -c "$f" -o "$ROOT/build/obj/$(basename "$f" .cpp).o"
done

# Icon, version info and the common-controls manifest.
( cd "$ROOT/resources" && x86_64-w64-mingw32-windres app.rc -O coff -o "$ROOT/build/obj/app_rc.o" )

# 3. Link: everything static, unused code dropped, symbols stripped -----------
$CXX -municode -mwindows -static -s -Wl,--gc-sections -o "$ROOT/build/litetorrent.exe" \
  "$ROOT"/build/obj/*.o \
  "$ROOT/build/libtorrent/libtorrent-rasterbar.a" "$SYSROOT/lib/libssl.a" "$SYSROOT/lib/libcrypto.a" \
  -lbcrypt -lmswsock -lws2_32 -liphlpapi -lcrypt32 \
  -lcomctl32 -lcomdlg32 -lole32 -luuid -lshell32 -lshlwapi -ladvapi32 -lgdi32 -luser32

ls -l "$ROOT/build/litetorrent.exe"
