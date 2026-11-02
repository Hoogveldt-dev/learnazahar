#!/usr/bin/env bash
# Bouwt de debug-APK (arm64-v8a) en zet hem in dist/.
#   tools/woofer/build_apk.sh [naam.apk]
# Nodig: JDK 17 of nieuwer, Android SDK in $ANDROID_HOME met NDK 27.3.13750724 en
# CMake 3.31.6 (zie docs/woofer/README.md).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
: "${ANDROID_HOME:?Zet ANDROID_HOME op de Android SDK}"
NAME="${1:-woofer-azahar-debug.apk}"
ABIS="${ABIS:-arm64-v8a}"

cd "$ROOT/src/android"
./gradlew assembleVanillaDebug -PabiFilters="$ABIS"
APK="$(ls -t app/build/outputs/apk/vanilla/debug/*.apk | head -n 1)"
mkdir -p "$ROOT/dist"
cp "$APK" "$ROOT/dist/$NAME"
echo "APK: $ROOT/dist/$NAME"
