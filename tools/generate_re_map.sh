#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
  echo "Usage: $0 <samp-dll-path> [out-dir]" >&2
  exit 1
fi

DLL_PATH="$1"
OUT_DIR="${2:-analysis/generated}"

require_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "Missing dependency: $1" >&2
    exit 2
  fi
}

for dep in r2 rz-bin jq rg strings awk sed sort cut uniq wc; do
  require_cmd "$dep"
done

mkdir -p "$OUT_DIR"

IMPORTS_RAW="${OUT_DIR}/rz_imports.txt"
IMPORTS_TSV="${OUT_DIR}/imports_rz.tsv"
AFJ="${OUT_DIR}/r2_aflj.json"

echo "[*] Collecting import table..."
rz-bin -i "$DLL_PATH" >"$IMPORTS_RAW"
awk 'NR>2 && $1 ~ /^[0-9]+$/ {print $5"\t"$6}' "$IMPORTS_RAW" >"$IMPORTS_TSV"
awk -F'\t' '{cnt[$1]++} END{for (d in cnt) printf "%d\t%s\n", cnt[d], d}' "$IMPORTS_TSV" | sort -nr \
  >"${OUT_DIR}/import_dll_counts_rz.tsv"

echo "[*] Running function analysis (r2 aaa + aflj)..."
r2 -2 -e scr.color=0 -q -c 'aaa;aflj' "$DLL_PATH" >"$AFJ"
jq 'length' "$AFJ" >"${OUT_DIR}/function_count.txt"
jq -r 'sort_by(-.size)[:120][] | (.offset|tostring) + "\t" + (.size|tostring) + "\t" + (.nbbs|tostring) + "\t" + .name' \
  "$AFJ" | awk -F'\t' '{printf("0x%x\t%s\t%s\t%s\n", $1, $2, $3, $4)}' >"${OUT_DIR}/top_functions.tsv"
jq -r '.[] | (.offset|tostring) + "\t" + (.size|tostring)' "$AFJ" \
  | awk -F'\t' '{
      off=$1; sz=$2;
      bucket=int((off-0x10000000)/0x10000);
      start=0x10000000 + bucket*0x10000;
      end=start+0xFFFF;
      key=sprintf("0x%08x-0x%08x", start, end);
      cnt[key]++; sum[key]+=sz;
    }
    END {
      for (k in cnt) printf "%6d\t%10d\t%s\n", cnt[k], sum[k], k;
    }' | sort -k2,2nr >"${OUT_DIR}/function_range_hist.tsv"

collect_import_xrefs() {
  local lib="$1"
  local out_file="$2"
  local callers_file="$3"
  local usage_file="$4"
  local cmd="aaa;"
  local funcs=()

  mapfile -t funcs < <(awk -v lib="$lib" '$5 == lib {print $6}' "$IMPORTS_RAW" | sort -u)
  if [[ ${#funcs[@]} -eq 0 ]]; then
    : >"$out_file"
    : >"$callers_file"
    : >"$usage_file"
    return
  fi

  for f in "${funcs[@]}"; do
    cmd+=" ?e IMPORT:${f}; axt sym.imp.${lib}_${f};"
  done

  r2 -2 -e scr.color=0 -c "$cmd" "$DLL_PATH" >"$out_file"

  awk '
    /^IMPORT:/ { imp = substr($0, 8); next }
    /^(fcn\.|entry0)/ {
      if (imp != "") {
        seen[imp "\t" $1] = 1;
      }
    }
    END {
      for (k in seen) {
        split(k, a, "\t");
        cnt[a[1]]++;
      }
      for (i in cnt) print cnt[i] "\t" i;
    }' "$out_file" | sort -nr >"$usage_file"

  awk '
    /^(fcn\.|entry0)/ && /sym\.imp\./ { calls[$1]++ }
    END { for (c in calls) print calls[c] "\t" c }' "$out_file" | sort -nr >"$callers_file"
}

collect_wrapper_xrefs() {
  local wrapper_prefix="$1"
  local out_file="$2"
  local callers_file="$3"
  local cmd="aaa;"
  local wrappers=()

  mapfile -t wrappers < <(jq -r --arg p "$wrapper_prefix" '.[] | select(.name | startswith($p)) | .name' "$AFJ" | sort -u)
  if [[ ${#wrappers[@]} -eq 0 ]]; then
    : >"$out_file"
    : >"$callers_file"
    return
  fi

  for w in "${wrappers[@]}"; do
    cmd+=" ?e WRAP:${w}; axt ${w};"
  done

  r2 -2 -e scr.color=0 -c "$cmd" "$DLL_PATH" >"$out_file"
  awk '
    /^fcn\./ { cnt[$1]++ }
    END { for (c in cnt) print cnt[c] "\t" c }' "$out_file" | sort -nr >"$callers_file"
}

echo "[*] Collecting import xrefs..."
collect_import_xrefs "WSOCK32.dll" "${OUT_DIR}/wsock_xrefs.txt" "${OUT_DIR}/wsock_callers.tsv" \
  "${OUT_DIR}/wsock_import_usage_counts.tsv"

echo "[*] Collecting wrapper xrefs..."
collect_wrapper_xrefs "sub.d3dx9_25.dll_" "${OUT_DIR}/d3dx_wrapper_xrefs.txt" \
  "${OUT_DIR}/d3dx_wrapper_callers.tsv"
collect_wrapper_xrefs "sub.BASS.dll_" "${OUT_DIR}/bass_wrapper_xrefs.txt" \
  "${OUT_DIR}/bass_wrapper_callers.tsv"

echo "[*] Collecting string crosswalk signals..."
strings -a -n 8 "$DLL_PATH" >"${OUT_DIR}/strings_n8.txt"
awk '
  length($0) >= 10 &&
  length($0) <= 120 &&
  $0 ~ /[A-Za-z]/ &&
  $0 ~ /[[:space:][:punct:]]/ { print }' "${OUT_DIR}/strings_n8.txt" \
  | sort -u >"${OUT_DIR}/strings_anchor_candidates.txt"

{
  echo "# RE Map Summary"
  echo
  echo "- DLL: \`${DLL_PATH}\`"
  echo "- Function count: $(cat "${OUT_DIR}/function_count.txt")"
  echo
  echo "## Key Outputs"
  echo
  echo "- \`import_dll_counts_rz.tsv\`: import density per DLL."
  echo "- \`top_functions.tsv\`: largest functions by size."
  echo "- \`function_range_hist.tsv\`: function count + code size per 64 KB region."
  echo "- \`wsock_xrefs.txt\`, \`wsock_callers.tsv\`: Winsock call-site map."
  echo "- \`d3dx_wrapper_xrefs.txt\`, \`d3dx_wrapper_callers.tsv\`: render wrapper call map."
  echo "- \`bass_wrapper_xrefs.txt\`, \`bass_wrapper_callers.tsv\`: audio wrapper call map."
  echo "- \`strings_anchor_candidates.txt\`: retained string anchors for manual RE notes."
} >"${OUT_DIR}/RE_MAP_SUMMARY.md"

echo "[+] Done. Results written to: ${OUT_DIR}"
