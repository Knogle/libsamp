#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 || $# -gt 3 ]]; then
  echo "Usage: $0 <reference-trace.log> <candidate-trace.log> [out-dir]" >&2
  exit 1
fi

REF_LOG="$1"
CAND_LOG="$2"
OUT_DIR="${3:-/tmp/samp-runtime-trace-diff}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TRIM_SCRIPT="${SCRIPT_DIR}/trim_wine_trace.sh"

require_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "Missing dependency: $1" >&2
    exit 2
  fi
}

for dep in rg awk sed sort diff wc mkdir cut; do
  require_cmd "$dep"
done

if [[ ! -f "$REF_LOG" ]]; then
  echo "Reference trace not found: $REF_LOG" >&2
  exit 3
fi
if [[ ! -f "$CAND_LOG" ]]; then
  echo "Candidate trace not found: $CAND_LOG" >&2
  exit 3
fi
if [[ ! -x "$TRIM_SCRIPT" ]]; then
  echo "Missing executable helper: $TRIM_SCRIPT" >&2
  exit 4
fi

mkdir -p "$OUT_DIR"

EVENT_PATTERN='Connecting to|Connected to|Failed to connect|Lost connection|Reconnecting|accepted the connection|BOOT_PHASE_[0-9]+|RakNet|Call [^ ]+\.(WSAStartup|WSACleanup|bind|connect|listen|sendto|recvfrom)\(|Ret  [^ ]+\.(WSAStartup|WSACleanup|bind|connect|listen|sendto|recvfrom)\('

normalize_trace() {
  local in_file="$1"
  local out_prefix="$2"

  rg -n -i -e "$EVENT_PATTERN" "$in_file" >"${out_prefix}.raw" || true

  awk '
    {
      line = $0
      sub(/^[0-9]+:/, "", line)
      gsub(/\r/, "", line)
      gsub(/[0-9]{2}:[0-9]{2}:[0-9]{2}(\.[0-9]+)?/, "<ts>", line)
      gsub(/0x[0-9A-Fa-f]+/, "<hex>", line)
      gsub(/[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+:[0-9]+/, "<hostport>", line)
      gsub(/[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+/, "<ipv4>", line)
      gsub(/[[:space:]]+/, " ", line)
      sub(/^ /, "", line)
      if (length(line) > 0) {
        print line
      }
    }
  ' "${out_prefix}.raw" >"${out_prefix}.norm"

  awk '
    { cnt[$0]++ }
    END {
      for (k in cnt) {
        print cnt[k] "\t" k
      }
    }
  ' "${out_prefix}.norm" | sort -nr >"${out_prefix}.counts"
}

REF_PREFIX="${OUT_DIR}/reference"
CAND_PREFIX="${OUT_DIR}/candidate"

normalize_trace "$REF_LOG" "$REF_PREFIX"
normalize_trace "$CAND_LOG" "$CAND_PREFIX"

diff -u "${REF_PREFIX}.norm" "${CAND_PREFIX}.norm" >"${OUT_DIR}/sequence.diff" || true
diff -u "${REF_PREFIX}.counts" "${CAND_PREFIX}.counts" >"${OUT_DIR}/counts.diff" || true

count_matches() {
  local file="$1"
  local pattern="$2"
  local v
  v="$(rg -n -i -c -e "$pattern" "$file" 2>/dev/null || true)"
  if [[ -z "${v}" ]]; then
    echo 0
  else
    echo "${v}"
  fi
}

emit_checks() {
  local trim_dir="$1"
  local out_tsv="$2"
  local loaddll_file="${trim_dir}/loaddll_relevant.log"
  local samp_window_file="${trim_dir}/samp_lifecycle_window.log"
  local call_file="${trim_dir}/call_focus.log"
  local focus_file="${trim_dir}/focus_lines.log"

  {
    printf "key\tseverity\tmin\tcount\tdescription\n"
    printf "samp_dll_loaded_native\tcritical\t1\t%s\tsamp.dll loaded as native module\n" "$(count_matches "$loaddll_file" 'Loaded L".*samp\.dll".*: native')"
    printf "samp_process_attach_start\tcritical\t1\t%s\tsamp.dll process_attach START marker\n" "$(count_matches "$samp_window_file" 'process_attach \(L"samp\.dll",00000000\) - START')"
    printf "samp_process_attach_end\tcritical\t1\t%s\tsamp.dll process_attach END marker\n" "$(count_matches "$samp_window_file" 'process_attach \(L"samp\.dll",00000000\) - END')"
    printf "samp_process_attach_call\tcritical\t1\t%s\tsamp.dll DllMain PROCESS_ATTACH call\n" "$(count_matches "$samp_window_file" 'Call PE DLL \(proc=.*module=02380000 L"samp\.dll",reason=PROCESS_ATTACH')"
    printf "samp_process_detach_call\tcritical\t1\t%s\tsamp.dll DllMain PROCESS_DETACH call\n" "$(count_matches "$samp_window_file" 'Call PE DLL \(proc=.*module=02380000 L"samp\.dll",reason=PROCESS_DETACH')"
    printf "ws2_32_dll_load\thigh\t1\t%s\tWS2_32.dll loaded\n" "$(count_matches "$loaddll_file" 'Loaded L".*WS2_32\.dll"')"
    printf "wsock32_dll_load\thigh\t1\t%s\tWSOCK32.dll loaded\n" "$(count_matches "$loaddll_file" 'Loaded L".*WSOCK32\.dll"')"
    printf "d3dx9_25_dll_load\tmedium\t0\t%s\td3dx9_25.dll loaded (external dependency, non-blocking)\n" "$(count_matches "$loaddll_file" 'Loaded L".*d3dx9_25\.dll"')"
    printf "bass_dll_load\tmedium\t0\t%s\tBASS.dll loaded (external dependency, non-blocking)\n" "$(count_matches "$loaddll_file" 'Loaded L".*BASS\.dll"')"
    printf "net_wsa_startup\thigh\t1\t%s\tWSAStartup call present\n" "$(count_matches "$call_file" 'Call (ws2_32|wsock32)\.WSAStartup')"
    printf "net_wsa_cleanup\thigh\t1\t%s\tWSACleanup call present\n" "$(count_matches "$call_file" 'Call (ws2_32|wsock32)\.WSACleanup')"
    printf "net_host_resolve\thigh\t1\t%s\tgethostbyname call present\n" "$(count_matches "$call_file" 'Call (ws2_32|wsock32)\.gethostbyname')"
    printf "net_sendto\tcritical\t1\t%s\tsendto call present\n" "$(count_matches "$call_file" 'Call (ws2_32|wsock32)\.sendto')"
    printf "net_recvfrom\tcritical\t1\t%s\trecvfrom call present\n" "$(count_matches "$call_file" 'Call (ws2_32|wsock32)\.recvfrom')"
    printf "net_inet_ntoa\thigh\t1\t%s\tinet_ntoa call present\n" "$(count_matches "$call_file" 'Call (ws2_32|wsock32)\.inet_ntoa')"
    printf "net_inet_addr\thigh\t1\t%s\tinet_addr call present\n" "$(count_matches "$call_file" 'Call (ws2_32|wsock32)\.inet_addr')"
    printf "ui_callwindowproc\tmedium\t1\t%s\tCallWindowProc(A/W) call present\n" "$(count_matches "$call_file" 'Call user32\.CallWindowProc(A|W)')"
    printf "ui_sendmessage\tmedium\t1\t%s\tSendMessage(A/W/TimeoutW) call present\n" "$(count_matches "$call_file" 'Call user32\.(SendMessageA|SendMessageW|SendMessageTimeoutW)')"
    printf "ui_setwindowlong\tmedium\t1\t%s\tSetWindowLong(A/W) call present\n" "$(count_matches "$call_file" 'Call user32\.SetWindowLong(A|W)')"
    printf "ui_createwindowex\tmedium\t1\t%s\tCreateWindowEx(A/W) call present\n" "$(count_matches "$call_file" 'Call user32\.CreateWindowEx(A|W)')"
    printf "d3d_create9\tmedium\t0\t%s\tDirect3DCreate9 call present (external dependency, non-blocking)\n" "$(count_matches "$call_file" 'Call d3d9\.Direct3DCreate9')"
    printf "connect_banner\tmedium\t1\t%s\t\"Connecting to ...\" banner observed\n" "$(count_matches "$focus_file" 'Connecting to ')"
    printf "connected_banner\tmedium\t1\t%s\t\"Connected to ...\" banner observed\n" "$(count_matches "$focus_file" 'Connected to ')"
    printf "reconnect_banner\tmedium\t0\t%s\t\"Lost connection ... Reconnecting\" banner observed\n" "$(count_matches "$focus_file" 'Lost connection to the server\. Reconnecting')"
  } >"$out_tsv"
}

compare_checks() {
  local ref_tsv="$1"
  local cand_tsv="$2"
  local out_tsv="$3"

  awk -F'\t' '
    function sev_rank(s) {
      if (s == "critical") return 1;
      if (s == "high") return 2;
      return 3;
    }
    NR == FNR {
      if (FNR == 1) next
      ref[$1] = $4 + 0
      sev[$1] = $2
      min[$1] = $3 + 0
      desc[$1] = $5
      next
    }
    FNR == 1 { next }
    {
      cand[$1] = $4 + 0
      sev[$1] = $2
      min[$1] = $3 + 0
      desc[$1] = $5
    }
    END {
      print "0\tseverity\tkey\tref_count\tcand_count\tmin_required\tdelta\tstatus\tdescription"
      for (k in sev) {
        r = ref[k] + 0
        c = cand[k] + 0
        req = min[k] + 0
        delta = c - r
        status = "PASS"
        if (c < req) {
          if (sev[k] == "critical" || sev[k] == "high") status = "FAIL"
          else status = "WARN"
        }
        printf "%d\t%s\t%s\t%d\t%d\t%d\t%d\t%s\t%s\n",
          sev_rank(sev[k]), sev[k], k, r, c, req, delta, status, desc[k]
      }
    }
  ' "$ref_tsv" "$cand_tsv" \
    | sort -t$'\t' -k1,1n -k2,2 -k3,3 \
    | cut -f2- >"$out_tsv"
}

REF_TRIM_DIR="${OUT_DIR}/reference_trim"
CAND_TRIM_DIR="${OUT_DIR}/candidate_trim"
mkdir -p "$REF_TRIM_DIR" "$CAND_TRIM_DIR"

"$TRIM_SCRIPT" "$REF_LOG" "$REF_TRIM_DIR" >/dev/null
"$TRIM_SCRIPT" "$CAND_LOG" "$CAND_TRIM_DIR" >/dev/null

emit_checks "$REF_TRIM_DIR" "${OUT_DIR}/reference_checks.tsv"
emit_checks "$CAND_TRIM_DIR" "${OUT_DIR}/candidate_checks.tsv"
compare_checks "${OUT_DIR}/reference_checks.tsv" "${OUT_DIR}/candidate_checks.tsv" "${OUT_DIR}/check_report.tsv"

REF_EVENTS="$(wc -l < "${REF_PREFIX}.norm" | tr -d " ")"
CAND_EVENTS="$(wc -l < "${CAND_PREFIX}.norm" | tr -d " ")"
FAIL_COUNT="$(awk -F'\t' 'NR>1 && $7=="FAIL" {c++} END {print c+0}' "${OUT_DIR}/check_report.tsv")"
WARN_COUNT="$(awk -F'\t' 'NR>1 && $7=="WARN" {c++} END {print c+0}' "${OUT_DIR}/check_report.tsv")"

echo "== Runtime Trace Diff =="
echo "Reference trace: $REF_LOG"
echo "Candidate trace: $CAND_LOG"
echo "Output dir: $OUT_DIR"
echo "Reference matched events: $REF_EVENTS"
echo "Candidate matched events: $CAND_EVENTS"
echo "ABI checks FAIL: $FAIL_COUNT"
echo "ABI checks WARN: $WARN_COUNT"
echo
echo "Files:"
echo "- ${REF_PREFIX}.norm"
echo "- ${CAND_PREFIX}.norm"
echo "- ${OUT_DIR}/sequence.diff"
echo "- ${OUT_DIR}/counts.diff"
echo "- ${OUT_DIR}/reference_checks.tsv"
echo "- ${OUT_DIR}/candidate_checks.tsv"
echo "- ${OUT_DIR}/check_report.tsv"
