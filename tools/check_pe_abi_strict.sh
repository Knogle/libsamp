#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "Usage: $0 <reference-dll> <candidate-dll>" >&2
  exit 1
fi

require_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "Missing dependency: $1" >&2
    exit 2
  fi
}

for dep in objdump rz-bin file sha256sum awk sed sort uniq comm grep tr wc mktemp; do
  require_cmd "$dep"
done

REF="$1"
CAND="$2"

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

objdump -p "$REF" >"$tmpdir/ref.p"
objdump -p "$CAND" >"$tmpdir/cand.p"
objdump -x "$REF" >"$tmpdir/ref.x"
objdump -x "$CAND" >"$tmpdir/cand.x"
rz-bin -i "$REF" >"$tmpdir/ref.imports"
rz-bin -i "$CAND" >"$tmpdir/cand.imports"

extract_scalar() {
  local file="$1"
  local key="$2"
  awk -v key="$key" '$1 == key { print $2; exit }' "$file"
}

extract_subsystem_text() {
  local file="$1"
  awk '$1 == "Subsystem" { print $2 " " $3 " " $4 " " $5; exit }' "$file"
}

extract_export_dir() {
  local file="$1"
  grep -m1 "Entry 0 .*Export Directory" "$file" | sed -E 's/.*Entry 0 //'
}

extract_sections() {
  local file="$1"
  sed -n '/Sections:/,/SYMBOL TABLE:/p' "$file" | awk '/^  [0-9]+ / { print $2 }'
}

extract_import_dlls() {
  local file="$1"
  grep "DLL Name:" "$file" | sed 's/^.*DLL Name: //g' | sort -u
}

extract_import_symbols() {
  local file="$1"
  awk 'NR > 2 && $1 ~ /^[0-9]+$/ { print $5 "\t" $6 }' "$file" | sort -u
}

extract_import_counts() {
  local file="$1"
  awk 'NR > 2 && $1 ~ /^[0-9]+$/ { cnt[$5]++ } END { for (dll in cnt) print dll "\t" cnt[dll] }' "$file" | sort
}

status_line() {
  local severity="$1"
  local key="$2"
  local status="$3"
  local ref_value="$4"
  local cand_value="$5"
  local description="$6"
  printf "%s\t%s\t%s\t%s\t%s\t%s\n" "$severity" "$key" "$status" "$ref_value" "$cand_value" "$description"
}

compare_equal() {
  local severity="$1"
  local key="$2"
  local description="$3"
  local ref_value="$4"
  local cand_value="$5"
  local status="PASS"

  if [[ "$ref_value" != "$cand_value" ]]; then
    status="FAIL"
  fi
  status_line "$severity" "$key" "$status" "$ref_value" "$cand_value" "$description"
}

compare_boolean() {
  local severity="$1"
  local key="$2"
  local description="$3"
  local ref_value="$4"
  local cand_value="$5"
  local status="PASS"

  if [[ "$ref_value" != "$cand_value" ]]; then
    status="FAIL"
  fi
  status_line "$severity" "$key" "$status" "$ref_value" "$cand_value" "$description"
}

list_to_csv() {
  tr '\n' ',' | sed 's/,$//'
}

diff_list() {
  local ref_file="$1"
  local cand_file="$2"
  local only_ref="$3"
  local only_cand="$4"
  local ref_sorted="$tmpdir/$(basename "$ref_file").sorted"
  local cand_sorted="$tmpdir/$(basename "$cand_file").sorted"

  sort -u "$ref_file" >"$ref_sorted"
  sort -u "$cand_file" >"$cand_sorted"
  comm -23 "$ref_sorted" "$cand_sorted" >"$only_ref"
  comm -13 "$ref_sorted" "$cand_sorted" >"$only_cand"
}

has_section() {
  local file="$1"
  local section="$2"
  if grep -Fxq "$section" "$file"; then
    echo yes
  else
    echo no
  fi
}

sha256sum "$REF" | awk '{print $1}' >"$tmpdir/ref.sha"
sha256sum "$CAND" | awk '{print $1}' >"$tmpdir/cand.sha"
extract_sections "$tmpdir/ref.x" >"$tmpdir/ref.sections"
extract_sections "$tmpdir/cand.x" >"$tmpdir/cand.sections"
extract_import_dlls "$tmpdir/ref.p" >"$tmpdir/ref.dlls"
extract_import_dlls "$tmpdir/cand.p" >"$tmpdir/cand.dlls"
extract_import_symbols "$tmpdir/ref.imports" >"$tmpdir/ref.symbols"
extract_import_symbols "$tmpdir/cand.imports" >"$tmpdir/cand.symbols"
extract_import_counts "$tmpdir/ref.imports" >"$tmpdir/ref.import_counts"
extract_import_counts "$tmpdir/cand.imports" >"$tmpdir/cand.import_counts"

diff_list "$tmpdir/ref.sections" "$tmpdir/cand.sections" "$tmpdir/ref.sections.only" "$tmpdir/cand.sections.only"
diff_list "$tmpdir/ref.dlls" "$tmpdir/cand.dlls" "$tmpdir/ref.dlls.only" "$tmpdir/cand.dlls.only"
diff_list "$tmpdir/ref.symbols" "$tmpdir/cand.symbols" "$tmpdir/ref.symbols.only" "$tmpdir/cand.symbols.only"
diff_list "$tmpdir/ref.import_counts" "$tmpdir/cand.import_counts" "$tmpdir/ref.import_counts.only" "$tmpdir/cand.import_counts.only"

ref_size="$(stat -c%s "$REF")"
cand_size="$(stat -c%s "$CAND")"
if [[ "$ref_size" -gt 0 ]]; then
  size_ratio="$(awk "BEGIN { printf \"%.2f\", (${cand_size} * 100.0) / ${ref_size} }")"
else
  size_ratio="n/a"
fi

{
  echo "== Strict PE ABI Parity =="
  echo "Reference: $(file "$REF")"
  echo "Candidate: $(file "$CAND")"
  echo "Reference SHA256: $(cat "$tmpdir/ref.sha")"
  echo "Candidate SHA256: $(cat "$tmpdir/cand.sha")"
  echo "Reference Size: ${ref_size}"
  echo "Candidate Size: ${cand_size} (${size_ratio}% of reference)"
  echo
  echo "severity	key	status	ref	candidate	description"
  compare_equal critical address_of_entry_point "AddressOfEntryPoint must match" \
    "$(extract_scalar "$tmpdir/ref.x" AddressOfEntryPoint)" \
    "$(extract_scalar "$tmpdir/cand.x" AddressOfEntryPoint)"
  compare_equal critical image_base "ImageBase must match" \
    "$(extract_scalar "$tmpdir/ref.p" ImageBase)" \
    "$(extract_scalar "$tmpdir/cand.p" ImageBase)"
  compare_equal critical section_alignment "SectionAlignment must match" \
    "$(extract_scalar "$tmpdir/ref.x" SectionAlignment)" \
    "$(extract_scalar "$tmpdir/cand.x" SectionAlignment)"
  compare_equal critical file_alignment "FileAlignment must match" \
    "$(extract_scalar "$tmpdir/ref.x" FileAlignment)" \
    "$(extract_scalar "$tmpdir/cand.x" FileAlignment)"
  compare_equal critical subsystem "Subsystem must match" \
    "$(extract_subsystem_text "$tmpdir/ref.x")" \
    "$(extract_subsystem_text "$tmpdir/cand.x")"
  compare_equal high dll_characteristics "DllCharacteristics should match" \
    "$(extract_scalar "$tmpdir/ref.x" DllCharacteristics)" \
    "$(extract_scalar "$tmpdir/cand.x" DllCharacteristics)"
  compare_equal high stack_reserve "SizeOfStackReserve should match" \
    "$(extract_scalar "$tmpdir/ref.x" SizeOfStackReserve)" \
    "$(extract_scalar "$tmpdir/cand.x" SizeOfStackReserve)"
  compare_equal high stack_commit "SizeOfStackCommit should match" \
    "$(extract_scalar "$tmpdir/ref.x" SizeOfStackCommit)" \
    "$(extract_scalar "$tmpdir/cand.x" SizeOfStackCommit)"
  compare_equal high heap_reserve "SizeOfHeapReserve should match" \
    "$(extract_scalar "$tmpdir/ref.x" SizeOfHeapReserve)" \
    "$(extract_scalar "$tmpdir/cand.x" SizeOfHeapReserve)"
  compare_equal high heap_commit "SizeOfHeapCommit should match" \
    "$(extract_scalar "$tmpdir/ref.x" SizeOfHeapCommit)" \
    "$(extract_scalar "$tmpdir/cand.x" SizeOfHeapCommit)"
  compare_equal medium export_directory "Export directory shape should match" \
    "$(extract_export_dir "$tmpdir/ref.p")" \
    "$(extract_export_dir "$tmpdir/cand.p")"
  compare_equal critical section_count "Section count must match" \
    "$(wc -l < "$tmpdir/ref.sections" | tr -d ' ')" \
    "$(wc -l < "$tmpdir/cand.sections" | tr -d ' ')"
  compare_equal critical import_dll_count "Imported DLL count must match" \
    "$(wc -l < "$tmpdir/ref.dlls" | tr -d ' ')" \
    "$(wc -l < "$tmpdir/cand.dlls" | tr -d ' ')"
  compare_equal critical import_symbol_count "Imported symbol count must match" \
    "$(wc -l < "$tmpdir/ref.symbols" | tr -d ' ')" \
    "$(wc -l < "$tmpdir/cand.symbols" | tr -d ' ')"
  compare_boolean critical has_rsrc "Resource section presence must match" \
    "$(has_section "$tmpdir/ref.sections" .rsrc)" \
    "$(has_section "$tmpdir/cand.sections" .rsrc)"
  compare_boolean high has_tls "TLS section presence should match" \
    "$(has_section "$tmpdir/ref.sections" .tls)" \
    "$(has_section "$tmpdir/cand.sections" .tls)"
  compare_boolean high has_eh_frame "Unexpected unwind section should be absent when reference lacks it" \
    "$(has_section "$tmpdir/ref.sections" .eh_frame)" \
    "$(has_section "$tmpdir/cand.sections" .eh_frame)"
  echo
  echo "== Section Order =="
  echo "Reference: $(list_to_csv < "$tmpdir/ref.sections")"
  echo "Candidate: $(list_to_csv < "$tmpdir/cand.sections")"
  if cmp -s "$tmpdir/ref.sections" "$tmpdir/cand.sections"; then
    echo "Status: PASS"
  else
    echo "Status: FAIL"
    echo "Only in reference: $(list_to_csv < "$tmpdir/ref.sections.only")"
    echo "Only in candidate: $(list_to_csv < "$tmpdir/cand.sections.only")"
  fi
  echo
  echo "== Imported DLL Set =="
  echo "Reference: $(list_to_csv < "$tmpdir/ref.dlls")"
  echo "Candidate: $(list_to_csv < "$tmpdir/cand.dlls")"
  if cmp -s "$tmpdir/ref.dlls" "$tmpdir/cand.dlls"; then
    echo "Status: PASS"
  else
    echo "Status: FAIL"
    echo "Missing from candidate: $(list_to_csv < "$tmpdir/ref.dlls.only")"
    echo "Unexpected in candidate: $(list_to_csv < "$tmpdir/cand.dlls.only")"
  fi
  echo
  echo "== Imported Symbol Set =="
  echo "Reference symbol entries: $(wc -l < "$tmpdir/ref.symbols" | tr -d ' ')"
  echo "Candidate symbol entries: $(wc -l < "$tmpdir/cand.symbols" | tr -d ' ')"
  if cmp -s "$tmpdir/ref.symbols" "$tmpdir/cand.symbols"; then
    echo "Status: PASS"
  else
    echo "Status: FAIL"
    echo "Missing symbol entries: $(wc -l < "$tmpdir/ref.symbols.only" | tr -d ' ')"
    echo "Unexpected symbol entries: $(wc -l < "$tmpdir/cand.symbols.only" | tr -d ' ')"
    echo "Top missing symbol entries:"
    sed -n '1,20p' "$tmpdir/ref.symbols.only"
    echo "Top unexpected symbol entries:"
    sed -n '1,20p' "$tmpdir/cand.symbols.only"
  fi
  echo
  echo "== Import Counts Per DLL =="
  if cmp -s "$tmpdir/ref.import_counts" "$tmpdir/cand.import_counts"; then
    echo "Status: PASS"
  else
    echo "Status: FAIL"
    echo "Reference:"
    sed -n '1,40p' "$tmpdir/ref.import_counts"
    echo "Candidate:"
    sed -n '1,40p' "$tmpdir/cand.import_counts"
  fi
} >"$tmpdir/report.txt"

cat "$tmpdir/report.txt"

if awk -F'\t' '$1 == "critical" && $3 == "FAIL" { found = 1 } END { exit(found ? 0 : 1) }' "$tmpdir/report.txt"; then
  exit 3
fi

if awk -F'\t' '$1 == "high" && $3 == "FAIL" { found = 1 } END { exit(found ? 0 : 1) }' "$tmpdir/report.txt"; then
  exit 4
fi

exit 0
