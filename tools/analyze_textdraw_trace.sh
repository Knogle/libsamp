#!/usr/bin/env bash
set -euo pipefail

usage() {
  echo "Usage: $0 [-s server-log] [-o out-dir] <client-or-probe-log>" >&2
}

SERVER_LOG=""
OUT_DIR="analysis/generated/textdraw_trace"

while getopts ":s:o:h" opt; do
  case "$opt" in
    s)
      SERVER_LOG="$OPTARG"
      ;;
    o)
      OUT_DIR="$OPTARG"
      ;;
    h)
      usage
      exit 0
      ;;
    :)
      echo "Missing argument for -$OPTARG" >&2
      usage
      exit 1
      ;;
    \?)
      echo "Unknown option: -$OPTARG" >&2
      usage
      exit 1
      ;;
  esac
done
shift $((OPTIND - 1))

if [[ $# -ne 1 ]]; then
  usage
  exit 1
fi

CLIENT_LOG="$1"

if [[ ! -f "$CLIENT_LOG" ]]; then
  echo "Client/probe log not found: $CLIENT_LOG" >&2
  exit 2
fi
if [[ -n "$SERVER_LOG" && ! -f "$SERVER_LOG" ]]; then
  echo "Server log not found: $SERVER_LOG" >&2
  exit 2
fi

mkdir -p "$OUT_DIR"

CLIENT_EVENTS_FILE="${OUT_DIR}/client_textdraw_events.log"
RPC_EVENTS_FILE="${OUT_DIR}/textdraw_rpc_events.log"
RUNTIME_EVENTS_FILE="${OUT_DIR}/runtime_textdraw_events.log"
FONT_SEQUENCE_FILE="${OUT_DIR}/font_sequence.log"
FONT_ORDER_FILE="${OUT_DIR}/font_order.log"
FONT_CALL_COUNTS_FILE="${OUT_DIR}/font_call_counts.tsv"
FONT_CALLER_COUNTS_FILE="${OUT_DIR}/font_caller_counts.tsv"
PRINT_STRINGS_FILE="${OUT_DIR}/print_strings.log"
FONT5_DISPATCH_FILE="${OUT_DIR}/font5_dispatch.log"
FONT5_RTT_FILE="${OUT_DIR}/font5_rtt.log"
SERVER_EVENTS_FILE="${OUT_DIR}/server_events.log"
SUMMARY_FILE="${OUT_DIR}/SUMMARY.txt"

rg -n -i \
  -e 'textdraw_font: seq=' \
  -e 'textdraw-decode:' \
  -e 'textdraw: (show|hide|edit|select|click)' \
  -e 'rpc-state id=(83|105|134|135|136)\b' \
  -e 'rpc-(user|auto)-out id=83\b' \
  "$CLIENT_LOG" >"$CLIENT_EVENTS_FILE" || true

rg -n -i \
  -e 'textdraw-decode:' \
  -e 'rpc-state id=(83|105|134|135|136)\b' \
  -e 'rpc-(user|auto)-out id=83\b' \
  "$CLIENT_LOG" >"$RPC_EVENTS_FILE" || true

rg -n -i \
  -e 'textdraw: (show|hide|edit|select|click)' \
  "$CLIENT_LOG" >"$RUNTIME_EVENTS_FILE" || true

rg -n 'textdraw_font: seq=' "$CLIENT_LOG" >"$FONT_SEQUENCE_FILE" || true

sed -n 's/.*textdraw_font: seq=[0-9][0-9]* .* name=\([^ ]*\).*/\1/p' "$FONT_SEQUENCE_FILE" \
  | nl -ba >"$FONT_ORDER_FILE"

sed -n 's/.*textdraw_font: seq=[0-9][0-9]* .* name=\([^ ]*\).*/\1/p' "$FONT_SEQUENCE_FILE" \
  | sort \
  | uniq -c \
  | awk '{ print $1 "\t" $2 }' \
  | sort -nr >"$FONT_CALL_COUNTS_FILE"

sed -n 's/.*textdraw_font: seq=[0-9][0-9]* .*caller_samp_rva=\(0x[0-9a-fA-F]*\).* name=\([^ ]*\).*/\1\t\2/p' "$FONT_SEQUENCE_FILE" \
  | sort \
  | uniq -c \
  | awk '{ print $1 "\t" $2 "\t" $3 }' \
  | sort -nr >"$FONT_CALLER_COUNTS_FILE"

rg -n 'name=CFont\.PrintString' "$FONT_SEQUENCE_FILE" >"$PRINT_STRINGS_FILE" || true

rg -n -e 'font5_code_hook:' -e 'font5_dispatch:' "$CLIENT_LOG" >"$FONT5_DISPATCH_FILE" || true
rg -n -e 'textdraw_rtt:' -e 'textdraw_render: (IDirect3D(Device9|Texture9)|ID3DXSprite)' \
  "$CLIENT_LOG" >"$FONT5_RTT_FILE" || true

if [[ -n "$SERVER_LOG" ]]; then
  rg -n -i \
    -e '\[tdprobe\]' \
    -e 'OnPlayerClick(TextDraw|PlayerTextDraw)' \
    "$SERVER_LOG" >"$SERVER_EVENTS_FILE" || true
else
  rg -n -i \
    -e '\[tdprobe\]' \
    -e 'OnPlayerClick(TextDraw|PlayerTextDraw)' \
    "$CLIENT_LOG" >"$SERVER_EVENTS_FILE" || true
fi

{
  echo "client_log: $CLIENT_LOG"
  if [[ -n "$SERVER_LOG" ]]; then
    echo "server_log: $SERVER_LOG"
  fi
  echo "out_dir: $OUT_DIR"
  echo
  for f in \
    client_textdraw_events.log \
    textdraw_rpc_events.log \
    runtime_textdraw_events.log \
    font_sequence.log \
    font_order.log \
    font_call_counts.tsv \
    font_caller_counts.tsv \
    print_strings.log \
    font5_dispatch.log \
    font5_rtt.log \
    server_events.log; do
    printf '%-32s %s\n' "$f" "$(wc -l < "${OUT_DIR}/${f}" 2>/dev/null || echo 0) lines"
  done
  echo
  echo "top_font_calls:"
  sed -n '1,20p' "$FONT_CALL_COUNTS_FILE"
  echo
  echo "top_font_callers:"
  sed -n '1,20p' "$FONT_CALLER_COUNTS_FILE"
} >"$SUMMARY_FILE"

cat "$SUMMARY_FILE"
