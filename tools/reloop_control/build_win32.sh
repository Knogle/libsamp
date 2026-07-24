#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD_DIR="${ROOT_DIR}/${RELOOP_CONTROL_BUILD_DIR:-build-reloop-control}"
TOOLCHAIN_FILE="${ROOT_DIR}/reimpl/cmake/toolchains/i686-w64-mingw32.cmake"

cmake -S "${ROOT_DIR}/tools/reloop_control" -B "${BUILD_DIR}" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}" -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD_DIR}" --target reloop_control
if command -v i686-w64-mingw32-strip >/dev/null 2>&1; then
  i686-w64-mingw32-strip -s "${BUILD_DIR}/reloop_control.asi"
fi
echo "Built: ${BUILD_DIR}/reloop_control.asi"
