#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 || $# -gt 3 ]]; then
  echo "Usage: $0 <samp-dll-path> <installer-exe-path> [out-dir]" >&2
  exit 1
fi

DLL_PATH="$1"
INSTALLER_PATH="$2"
OUT_DIR="${3:-analysis/metadata}"
EXTRACT_DIR="${OUT_DIR}/installer_extract"

for dep in file sha256sum objdump rz-bin strings 7z rg; do
  if ! command -v "$dep" >/dev/null 2>&1; then
    echo "Missing dependency: $dep" >&2
    exit 2
  fi
done

mkdir -p "$OUT_DIR" "$EXTRACT_DIR"

echo "[*] Writing baseline metadata to: ${OUT_DIR}"

{
  echo "# Artifact Hashes"
  sha256sum "$DLL_PATH" "$INSTALLER_PATH"
  echo
  echo "# File Signatures"
  file "$DLL_PATH" "$INSTALLER_PATH"
} >"${OUT_DIR}/01_artifacts_overview.txt"

objdump -x "$DLL_PATH" >"${OUT_DIR}/02_dll_header_sections_objdump.txt"
objdump -p "$DLL_PATH" >"${OUT_DIR}/03_dll_imports_objdump.txt"
rz-bin -I "$DLL_PATH" >"${OUT_DIR}/04_dll_info_rzbin.txt"
rz-bin -i "$DLL_PATH" >"${OUT_DIR}/05_dll_imports_rzbin.txt"
strings -a -n 8 "$DLL_PATH" >"${OUT_DIR}/06_dll_strings_all.txt"

strings -a -n 8 "$DLL_PATH" | rg -n -i \
  "ipv4|ipv6|getaddrinfo|gethostbyname|inet_addr|inet_ntoa|socket|connect|bind|sendto|recvfrom|raknet|SAMP/|Connecting to|Server has accepted" \
  >"${OUT_DIR}/07_dll_strings_network_focus.txt" || true

objdump -p "$DLL_PATH" | sed -n '/DLL Name: WSOCK32.dll/,+120p' \
  >"${OUT_DIR}/08_dll_wsock32_imports.txt"

7z l "$INSTALLER_PATH" >"${OUT_DIR}/09_installer_listing.txt"
7z x -y "$INSTALLER_PATH" "-o${EXTRACT_DIR}" >/dev/null

{
  echo "# Extracted payload hashes"
  sha256sum "${EXTRACT_DIR}/samp.dll" "${EXTRACT_DIR}/samp.exe" "${EXTRACT_DIR}/samp.saa" 2>/dev/null || true
  echo
  echo "# Bitwise compare (input dll vs installer dll)"
  if cmp -s "$DLL_PATH" "${EXTRACT_DIR}/samp.dll"; then
    echo "MATCH: DLL files are bit-identical"
  else
    echo "DIFF: DLL files are different"
  fi
} >"${OUT_DIR}/10_installer_payload_compare.txt"

echo "[+] Done."
