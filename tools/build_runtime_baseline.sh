#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
  echo "Usage: $0 <reference-trace.log> [out-dir]" >&2
  exit 1
fi

IN_FILE="$1"
OUT_DIR="${2:-analysis/generated/runtime_baseline}"

if [[ ! -f "$IN_FILE" ]]; then
  echo "Reference trace not found: $IN_FILE" >&2
  exit 2
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TRIM_SCRIPT="${SCRIPT_DIR}/trim_wine_trace.sh"

if [[ ! -x "$TRIM_SCRIPT" ]]; then
  echo "Missing executable helper: $TRIM_SCRIPT" >&2
  exit 3
fi

mkdir -p "$OUT_DIR"
TRIM_DIR="${OUT_DIR}/trim"
mkdir -p "$TRIM_DIR"

"$TRIM_SCRIPT" "$IN_FILE" "$TRIM_DIR" >/dev/null

MODULES_TSV="${OUT_DIR}/baseline_modules.tsv"
APIS_TSV="${OUT_DIR}/baseline_api_counts.tsv"
LIFECYCLE_TSV="${OUT_DIR}/baseline_lifecycle.tsv"
SIGNALS_TSV="${OUT_DIR}/baseline_signals.tsv"
SUMMARY_MD="${OUT_DIR}/BASELINE.md"

# Module surface actually loaded in runtime.
awk '
  match($0, /Loaded L".*\\([^\\"]+\.dll)"/, m) {
    dll = tolower(m[1])
    cnt[dll]++
  }
  END {
    for (k in cnt) print cnt[k] "\t" k
  }
' "${TRIM_DIR}/loaddll_relevant.log" | sort -nr >"${MODULES_TSV}"

# API call frequencies from reduced call stream.
cp "${TRIM_DIR}/call_api_counts.tsv" "${APIS_TSV}"

# DLL lifecycle reasons and attach markers for samp.dll and BASS.dll.
{
  printf "key\tcount\n"
  printf "samp_process_attach_call\t%s\n" "$(rg -n -i -c -e 'Call PE DLL \(proc=.*module=02380000 L"samp\.dll",reason=PROCESS_ATTACH' "${TRIM_DIR}/samp_lifecycle_window.log" || echo 0)"
  printf "samp_process_detach_call\t%s\n" "$(rg -n -i -c -e 'Call PE DLL \(proc=.*module=02380000 L"samp\.dll",reason=PROCESS_DETACH' "${TRIM_DIR}/samp_lifecycle_window.log" || echo 0)"
  printf "samp_thread_attach_call\t%s\n" "$(rg -n -i -c -e 'Call PE DLL \(proc=.*module=02380000 L"samp\.dll",reason=THREAD_ATTACH' "${TRIM_DIR}/samp_lifecycle_window.log" || echo 0)"
  printf "samp_thread_detach_call\t%s\n" "$(rg -n -i -c -e 'Call PE DLL \(proc=.*module=02380000 L"samp\.dll",reason=THREAD_DETACH' "${TRIM_DIR}/samp_lifecycle_window.log" || echo 0)"
  printf "samp_process_attach_marker_start\t%s\n" "$(rg -n -i -c -e 'process_attach \(L"samp\.dll",00000000\) - START' "${TRIM_DIR}/samp_lifecycle_window.log" || echo 0)"
  printf "samp_process_attach_marker_end\t%s\n" "$(rg -n -i -c -e 'process_attach \(L"samp\.dll",00000000\) - END' "${TRIM_DIR}/samp_lifecycle_window.log" || echo 0)"
  printf "bass_process_attach_call\t%s\n" "$(rg -n -i -c -e 'Call PE DLL \(proc=.*module=11000000 L"BASS\.dll",reason=PROCESS_ATTACH' "${TRIM_DIR}/focus_lines.log" || echo 0)"
  printf "bass_process_detach_call\t%s\n" "$(rg -n -i -c -e 'Call PE DLL \(proc=.*module=11000000 L"BASS\.dll",reason=PROCESS_DETACH' "${TRIM_DIR}/focus_lines.log" || echo 0)"
} >"${LIFECYCLE_TSV}"

# High-level behavior anchors.
{
  printf "key\tcount\n"
  printf "connecting_banner\t%s\n" "$(rg -n -i -c -e 'Connecting to ' "${TRIM_DIR}/focus_lines.log" || echo 0)"
  printf "connected_banner\t%s\n" "$(rg -n -i -c -e 'Connected to ' "${TRIM_DIR}/focus_lines.log" || echo 0)"
  printf "reconnect_banner\t%s\n" "$(rg -n -i -c -e 'Lost connection to the server\. Reconnecting' "${TRIM_DIR}/focus_lines.log" || echo 0)"
} >"${SIGNALS_TSV}"

{
  echo "# Runtime Baseline"
  echo
  echo "Input trace: \`${IN_FILE}\`"
  echo
  echo "## Artifacts"
  echo "- [trim/SUMMARY.txt](trim/SUMMARY.txt)"
  echo "- [baseline_modules.tsv](baseline_modules.tsv)"
  echo "- [baseline_api_counts.tsv](baseline_api_counts.tsv)"
  echo "- [baseline_lifecycle.tsv](baseline_lifecycle.tsv)"
  echo "- [baseline_signals.tsv](baseline_signals.tsv)"
  echo
  echo "## Core Signals"
  echo
  echo '```text'
  sed -n '1,40p' "${LIFECYCLE_TSV}"
  echo
  sed -n '1,20p' "${SIGNALS_TSV}"
  echo '```'
  echo
  echo "## Top APIs"
  echo
  echo '```text'
  sed -n '1,20p' "${APIS_TSV}"
  echo '```'
  echo
  echo "## Top Modules"
  echo
  echo '```text'
  sed -n '1,20p' "${MODULES_TSV}"
  echo '```'
} >"${SUMMARY_MD}"

echo "Runtime baseline written to: ${OUT_DIR}"
echo "- ${SUMMARY_MD}"
echo "- ${MODULES_TSV}"
echo "- ${APIS_TSV}"
echo "- ${LIFECYCLE_TSV}"
echo "- ${SIGNALS_TSV}"
