#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
  echo "Usage: $0 <wine-trace.log> [out-dir]" >&2
  exit 1
fi

IN_FILE="$1"
OUT_DIR="${2:-analysis/generated/trace_trim}"

if [[ ! -f "$IN_FILE" ]]; then
  echo "Input file not found: $IN_FILE" >&2
  exit 2
fi

mkdir -p "$OUT_DIR"

FOCUS_FILE="${OUT_DIR}/focus_lines.log"
RELAY_FILE="${OUT_DIR}/relay_focus.log"
CALL_FILE="${OUT_DIR}/call_focus.log"
CALL_REDUCED_FILE="${OUT_DIR}/call_focus_reduced.log"
CALL_API_COUNTS_FILE="${OUT_DIR}/call_api_counts.tsv"
LOADDLL_FILE="${OUT_DIR}/loaddll_relevant.log"
EVENT_COUNTS_FILE="${OUT_DIR}/focus_event_counts.tsv"
SAMP_WINDOW_FILE="${OUT_DIR}/samp_lifecycle_window.log"
TOP_EVENTS_FILE="${OUT_DIR}/top_events.log"
TEXTDRAW_FILE="${OUT_DIR}/textdraw_focus.log"
SUMMARY_FILE="${OUT_DIR}/SUMMARY.txt"

# Broad high-signal focus view (lifecycle + module load + key SA-MP DLLs + net status text).
rg -n -i \
  -e 'trace:module:(process_attach|MODULE_InitDLL|load_dll|build_module|LdrGetDllHandleEx|GetModuleFileNameW)' \
  -e 'trace:loaddll:build_module' \
  -e 'samp\.dll|gta_sa\.exe|samp\.exe|WSOCK32\.dll|WS2_32\.dll|d3dx9_25\.dll|BASS\.dll' \
  -e 'Connecting to|Lost connection|Reconnecting|Server has accepted the connection|Connected to' \
  "$IN_FILE" >"$FOCUS_FILE" || true

# Function-level relay focus for net/audio/render calls.
rg -n -i \
  -e 'trace:relay:.*(WSAStartup|WSACleanup|socket|connect|bind|sendto|recvfrom|send|recv|gethostbyname|inet_addr|inet_ntoa|select|ioctlsocket)' \
  -e 'trace:relay:.*(BASS_|Direct3DCreate9|D3DX|CreateWindowEx|SetWindowLong|CallWindowProc)' \
  "$IN_FILE" >"$RELAY_FILE" || true

# Relay often appears as "Call ..."/"Ret ..." lines; capture these explicitly.
rg -n -i \
  -e 'Call .*?\.(WSAStartup|WSACleanup|socket|connect|bind|sendto|recvfrom|send|recv|gethostbyname|inet_addr|inet_ntoa|select|ioctlsocket)' \
  -e 'Call .*?\.(BASS_|Direct3DCreate9|D3DX|CreateWindowEx|SetWindowLong|CallWindowProc)' \
  -e 'Ret  .*?\.(WSAStartup|WSACleanup|socket|connect|bind|sendto|recvfrom|send|recv|gethostbyname|inet_addr|inet_ntoa|select|ioctlsocket)' \
  -e 'Ret  .*?\.(BASS_|Direct3DCreate9|D3DX|CreateWindowEx|SetWindowLong|CallWindowProc)' \
  "$IN_FILE" >"$CALL_FILE" || true

# Smaller call stream focused on SA-MP lifecycle + known ABI-relevant modules/APIs.
rg -n -i \
  -e 'Call PE DLL \(proc=.*module=02380000 L"samp\.dll",reason=' \
  -e 'Ret  PE DLL \(proc=.*module=02380000 L"samp\.dll",reason=' \
  -e 'Call (WSOCK32|WS2_32|BASS|WINMM|D3DX9_25|D3D9|USER32|KERNEL32)\.' \
  -e 'Ret  (WSOCK32|WS2_32|BASS|WINMM|D3DX9_25|D3D9|USER32|KERNEL32)\.' \
  -e 'Call .*?\.(connect|sendto|recvfrom|gethostbyname|inet_addr|inet_ntoa|WSAStartup|WSACleanup)' \
  -e 'Ret  .*?\.(connect|sendto|recvfrom|gethostbyname|inet_addr|inet_ntoa|WSAStartup|WSACleanup)' \
  "$CALL_FILE" >"$CALL_REDUCED_FILE" || true

# API frequency map from reduced call stream.
awk '
  match($0, /(Call|Ret  ) ([A-Za-z0-9_]+)\.([A-Za-z0-9_@]+)/, m) {
    key = m[2] "." m[3]
    cnt[key]++
  }
  match($0, /(Call|Ret  ) PE DLL \(proc=.*module=02380000 L"samp\.dll",reason=([A-Z_]+)/, p) {
    key = "PE_DLL_REASON." p[2]
    cnt[key]++
  }
  END {
    for (k in cnt) print cnt[k] "\t" k
  }
' "$CALL_REDUCED_FILE" | sort -nr >"$CALL_API_COUNTS_FILE"

# Compact relevant module-load sequence.
rg -n -i 'trace:loaddll:build_module Loaded' "$IN_FILE" \
  | rg -i 'samp\.dll|gta_sa\.exe|samp\.exe|wsock32\.dll|ws2_32\.dll|d3dx9_25\.dll|bass\.dll|user32\.dll|kernel32\.dll|comctl32\.dll|winmm\.dll|shell32\.dll|psapi\.dll' \
  >"$LOADDLL_FILE" || true

# Coarse event frequency map to quickly see dominant trace classes.
awk -F'trace:' 'NF>1 { print $2 }' "$FOCUS_FILE" \
  | sed 's/[0-9A-Fa-f]\{6,\}/<hex>/g' \
  | sed 's/[0-9][0-9]*/<n>/g' \
  | cut -d'(' -f1 \
  | awk '{ cnt[$0]++ } END { for (k in cnt) print cnt[k] "\t" k }' \
  | sort -nr >"$EVENT_COUNTS_FILE"

# Extract only the samp.dll lifetime window.
START_LINE="$(rg -n 'process_attach \(L"samp\.dll"' "$IN_FILE" | head -n1 | cut -d: -f1 || true)"
END_LINE="$(rg -n 'MODULE_InitDLL \(02380000 L"samp\.dll",PROCESS_DETACH' "$IN_FILE" | head -n1 | cut -d: -f1 || true)"
if [[ -n "${START_LINE:-}" && -n "${END_LINE:-}" && "$END_LINE" -ge "$START_LINE" ]]; then
  sed -n "${START_LINE},${END_LINE}p" "$IN_FILE" >"$SAMP_WINDOW_FILE"
else
  rg -n -i 'samp\.dll|MODULE_InitDLL \(02380000' "$IN_FILE" >"$SAMP_WINDOW_FILE" || true
fi

# Small deterministic event list for quick checks in CI or manual review.
rg -n -i \
  -e 'process_attach \(L"samp\.dll"' \
  -e 'MODULE_InitDLL \(02380000 L"samp\.dll",PROCESS_ATTACH' \
  -e 'MODULE_InitDLL \(02380000 L"samp\.dll",PROCESS_DETACH' \
  -e 'Loaded L".*samp\.dll"' \
  -e 'Loaded L".*d3dx9_25\.dll"' \
  -e 'Loaded L".*WSOCK32\.dll"' \
  -e 'Loaded L".*WS2_32\.dll"' \
  -e 'Loaded L".*BASS\.dll"' \
  "$IN_FILE" >"$TOP_EVENTS_FILE" || true

# TextDraw-specific focus for mixed Wine/probe/server logs.
rg -n -i \
  -e 'textdraw_font: seq=' \
  -e 'textdraw-decode:' \
  -e 'textdraw: (show|hide|edit|select|click)' \
  -e 'rpc-state id=(83|105|134|135|136)\b' \
  -e 'rpc-(user|auto)-out id=83\b' \
  -e '\[tdprobe\]' \
  -e 'OnPlayerClick(TextDraw|PlayerTextDraw)' \
  "$IN_FILE" >"$TEXTDRAW_FILE" || true

{
  echo "input: $IN_FILE"
  wc -l "$IN_FILE"
  echo
  for f in \
    focus_lines.log \
    relay_focus.log \
    call_focus.log \
    call_focus_reduced.log \
    call_api_counts.tsv \
    loaddll_relevant.log \
    focus_event_counts.tsv \
    samp_lifecycle_window.log \
    top_events.log \
    textdraw_focus.log; do
    printf '%-30s %s\n' "$f" "$(wc -l < "${OUT_DIR}/${f}" 2>/dev/null || echo 0) lines"
  done
} >"$SUMMARY_FILE"

cat "$SUMMARY_FILE"
