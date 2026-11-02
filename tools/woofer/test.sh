#!/usr/bin/env bash
# Tests van de Woofer-brug op Linux.
#   tools/woofer/test.sh          snel: alleen woofer_tests (zonder de emulatorkern)
#   tools/woofer/test.sh --full   het hele Azahar-testdoel, eerst [woofer], dan alles
# Bouwmap: build/woofer-host (of $WOOFER_BUILD_DIR).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="${WOOFER_BUILD_DIR:-$ROOT/build/woofer-host}"
JOBS="${JOBS:-$(nproc)}"

if [ ! -f "$ROOT/externals/catch2/CMakeLists.txt" ]; then
    echo "Submodules ontbreken: git submodule update --init --recursive" >&2
    exit 1
fi

if [ ! -f "$BUILD/build.ninja" ]; then
    cmake -S "$ROOT" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DENABLE_QT=OFF -DENABLE_SDL2=OFF -DENABLE_TESTS=ON -DENABLE_ROOM=OFF \
        -DENABLE_WEB_SERVICE=OFF -DENABLE_CUBEB=OFF -DENABLE_OPENAL=OFF \
        -DENABLE_LIBUSB=OFF -DENABLE_LTO=OFF -DENABLE_VULKAN=OFF -DENABLE_SCRIPTING=OFF
fi

if [ "${1:-}" = "--full" ]; then
    cmake --build "$BUILD" --target tests -j "$JOBS"
    "$BUILD/bin/Release/tests" "[woofer]"
    "$BUILD/bin/Release/tests"
else
    cmake --build "$BUILD" --target woofer_tests -j "$JOBS"
    "$BUILD/bin/Release/woofer_tests"
fi
