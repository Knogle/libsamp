#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "Usage: $0 <reference-dll> <candidate-dll>" >&2
  exit 1
fi

REF="$1"
CAND="$2"

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

objdump -p "$REF" >"$tmpdir/ref.p"
objdump -p "$CAND" >"$tmpdir/cand.p"
objdump -x "$REF" >"$tmpdir/ref.x"
objdump -x "$CAND" >"$tmpdir/cand.x"

echo "== Identity =="
echo "Reference: $(file "$REF")"
echo "Candidate: $(file "$CAND")"
ref_size="$(stat -c%s "$REF")"
cand_size="$(stat -c%s "$CAND")"
if [[ "$ref_size" -gt 0 ]]; then
  ratio="$(awk "BEGIN { printf \"%.2f\", (${cand_size}*100.0)/${ref_size} }")"
else
  ratio="n/a"
fi
echo "Reference Size: ${ref_size} bytes"
echo "Candidate Size: ${cand_size} bytes (${ratio}% of reference)"

echo
echo "== Export Directory =="
ref_exp="$(grep -n "Entry 0 .*Export Directory" "$tmpdir/ref.p" | sed -E 's/.*Entry 0 //')"
cand_exp="$(grep -n "Entry 0 .*Export Directory" "$tmpdir/cand.p" | sed -E 's/.*Entry 0 //')"
echo "Reference: ${ref_exp}"
echo "Candidate: ${cand_exp}"

echo
echo "== ImageBase / Subsystem =="
echo "Reference ImageBase: $(grep "^ImageBase" "$tmpdir/ref.p" | awk '{print $2}')"
echo "Candidate ImageBase: $(grep "^ImageBase" "$tmpdir/cand.p" | awk '{print $2}')"
echo "Reference Subsystem: $(grep "^Subsystem" "$tmpdir/ref.p" | awk '{print $2,$3,$4,$5}')"
echo "Candidate Subsystem: $(grep "^Subsystem" "$tmpdir/cand.p" | awk '{print $2,$3,$4,$5}')"

echo
echo "== Section Names =="
echo "Reference:"
sed -n '/Sections:/,/SYMBOL TABLE:/p' "$tmpdir/ref.x" | grep -E "^  [0-9]+ " | awk '{print $2}'
echo "Candidate:"
sed -n '/Sections:/,/SYMBOL TABLE:/p' "$tmpdir/cand.x" | grep -E "^  [0-9]+ " | awk '{print $2}'

echo
echo "== Imported DLLs =="
echo "Reference:"
grep "DLL Name:" "$tmpdir/ref.p" | sed 's/^.*DLL Name: //g' | sort -u
echo "Candidate:"
grep "DLL Name:" "$tmpdir/cand.p" | sed 's/^.*DLL Name: //g' | sort -u

echo
echo "Done."
