#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"

if ! command -v toolbox >/dev/null 2>&1; then
  echo "Missing command: toolbox" >&2
  exit 2
fi

toolbox run -c devbuild bash -lc "
set -euo pipefail
cd '${ROOT_DIR}'
reimpl/scripts/build_win32.sh
"

tools/check_pe_abi_parity.sh "${ROOT_DIR}/samp.dll" "${ROOT_DIR}/build-win32/samp.dll"
tools/check_pe_abi_strict.sh "${ROOT_DIR}/samp.dll" "${ROOT_DIR}/build-win32/samp.dll" || true
