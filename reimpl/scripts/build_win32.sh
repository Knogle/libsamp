#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD_DIR="${ROOT_DIR}/${SAMPDLL_BUILD_DIR:-build-win32}"
TOOLCHAIN_FILE="${ROOT_DIR}/reimpl/cmake/toolchains/i686-w64-mingw32.cmake"
RAKNET_KNOGLE_FLAG="${SAMPDLL_ENABLE_RAKNET_KNOGLE:-ON}"

if ! command -v i686-w64-mingw32-gcc >/dev/null 2>&1; then
  echo "Missing compiler: i686-w64-mingw32-gcc" >&2
  exit 2
fi

if ! command -v i686-w64-mingw32-g++ >/dev/null 2>&1; then
  echo "Missing compiler: i686-w64-mingw32-g++" >&2
  exit 2
fi

cmake -S "${ROOT_DIR}/reimpl" -B "${BUILD_DIR}" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}" \
  -DSAMPDLL_BUILD_TESTS=OFF \
  -DSAMPDLL_ENABLE_RAKNET_KNOGLE="${RAKNET_KNOGLE_FLAG}" \
  -DCMAKE_BUILD_TYPE=Release

cmake --build "${BUILD_DIR}" --target samp

if command -v i686-w64-mingw32-strip >/dev/null 2>&1; then
  i686-w64-mingw32-strip -s "${BUILD_DIR}/samp.dll" || true
fi

echo "Built: ${BUILD_DIR}/samp.dll"
