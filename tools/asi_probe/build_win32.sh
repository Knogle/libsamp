#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD_DIR="${ROOT_DIR}/${SAMP_PROBE_BUILD_DIR:-build-asi-probe}"
TOOLCHAIN_FILE="${ROOT_DIR}/reimpl/cmake/toolchains/i686-w64-mingw32.cmake"
BUILD_TYPE="${SAMP_PROBE_BUILD_TYPE:-Release}"
STRIP_OUTPUT="${SAMP_PROBE_STRIP:-1}"

if ! command -v i686-w64-mingw32-gcc >/dev/null 2>&1; then
  echo "Missing compiler: i686-w64-mingw32-gcc" >&2
  echo "Run through toolboxes/reverse-engineering/run.sh or create the reverse-engineering toolbox." >&2
  exit 2
fi

cmake -S "${ROOT_DIR}/tools/asi_probe" -B "${BUILD_DIR}" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}" \
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"

cmake --build "${BUILD_DIR}" --target samp_probe

if [[ "${STRIP_OUTPUT}" != "0" ]] && command -v i686-w64-mingw32-strip >/dev/null 2>&1; then
  i686-w64-mingw32-strip -s "${BUILD_DIR}/samp_probe.asi" || true
fi

echo "Built: ${BUILD_DIR}/samp_probe.asi"
if [[ -f "${BUILD_DIR}/samp_probe.map" ]]; then
  echo "Map: ${BUILD_DIR}/samp_probe.map"
fi
