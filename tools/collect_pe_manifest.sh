#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
  echo "Usage: $0 <dll-path> [out-dir]" >&2
  exit 1
fi

require_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "Missing dependency: $1" >&2
    exit 2
  fi
}

for dep in objdump rz-bin file sha256sum awk sed sort uniq grep stat mkdir mktemp; do
  require_cmd "$dep"
done

DLL_PATH="$1"
OUT_DIR="${2:-analysis/metadata/pe_manifest}"

if [[ ! -f "$DLL_PATH" ]]; then
  echo "DLL not found: $DLL_PATH" >&2
  exit 3
fi

mkdir -p "$OUT_DIR"
tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

objdump -x "$DLL_PATH" >"$tmpdir/objdump_x.txt"
objdump -p "$DLL_PATH" >"$tmpdir/objdump_p.txt"
rz-bin -i "$DLL_PATH" >"$tmpdir/rz_imports.txt"

HEADERS_TSV="${OUT_DIR}/headers.tsv"
DIRECTORIES_TSV="${OUT_DIR}/data_directories.tsv"
SECTIONS_TSV="${OUT_DIR}/sections.tsv"
IMPORTS_TSV="${OUT_DIR}/imports.tsv"
IMPORT_COUNTS_TSV="${OUT_DIR}/import_counts.tsv"
SUMMARY_MD="${OUT_DIR}/MANIFEST.md"

{
  printf "key\tvalue\n"
  for key in AddressOfEntryPoint ImageBase SectionAlignment FileAlignment MajorSubsystemVersion MinorSubsystemVersion \
    Subsystem DllCharacteristics SizeOfStackReserve SizeOfStackCommit SizeOfHeapReserve SizeOfHeapCommit; do
    awk -v key="$key" '$1 == key { print key "\t" $2; exit }' "$tmpdir/objdump_x.txt"
  done
  printf "file_type\t%s\n" "$(file "$DLL_PATH" | sed 's/^.*: //')"
  printf "sha256\t%s\n" "$(sha256sum "$DLL_PATH" | awk '{print $1}')"
  printf "size_bytes\t%s\n" "$(stat -c%s "$DLL_PATH")"
} >"$HEADERS_TSV"

awk '
  /^Entry [0-9a-f]/ {
    idx = $2
    rva = $3
    size = $4
    $1 = ""; $2 = ""; $3 = ""; $4 = ""
    sub(/^    */, "", $0)
    print idx "\t" rva "\t" size "\t" $0
  }
' "$tmpdir/objdump_x.txt" \
  | {
      printf "index\trva\tsize\tdescription\n"
      cat
    } >"$DIRECTORIES_TSV"

sed -n '/^Sections:/,/^SYMBOL TABLE:/p' "$tmpdir/objdump_x.txt" | awk '
  /^  [0-9]+ / {
    name = $2
    size = $3
    vma = $4
    lma = $5
    file_off = $6
    align = $7
    flags = ""
    if (getline > 0) {
      flags = $0
      sub(/^ +/, "", flags)
    }
    print name "\t" size "\t" file_off "\t" vma "\t" lma "\t" file_off "\t" align "\t" flags
  }
' \
  | {
      printf "name\traw_size\traw_ptr\tvma\tlma\tfile_off\talign\tflags\n"
      cat
    } >"$SECTIONS_TSV"

awk 'NR > 2 && $1 ~ /^[0-9]+$/ { print $5 "\t" $6 }' "$tmpdir/rz_imports.txt" | sort -u \
  | {
      printf "dll\tsymbol\n"
      cat
    } >"$IMPORTS_TSV"

awk 'NR > 2 && $1 ~ /^[0-9]+$/ { cnt[$5]++ } END { for (dll in cnt) print dll "\t" cnt[dll] }' "$tmpdir/rz_imports.txt" | sort \
  | {
      printf "dll\tcount\n"
      cat
    } >"$IMPORT_COUNTS_TSV"

{
  echo "# PE Manifest"
  echo
  echo "Input: \`${DLL_PATH}\`"
  echo
  echo "## Core Files"
  echo
  echo "- [headers.tsv](headers.tsv)"
  echo "- [data_directories.tsv](data_directories.tsv)"
  echo "- [sections.tsv](sections.tsv)"
  echo "- [imports.tsv](imports.tsv)"
  echo "- [import_counts.tsv](import_counts.tsv)"
  echo
  echo "## Header Snapshot"
  echo
  echo '```text'
  sed -n '1,20p' "$HEADERS_TSV"
  echo '```'
  echo
  echo "## Sections"
  echo
  echo '```text'
  sed -n '1,12p' "$SECTIONS_TSV"
  echo '```'
  echo
  echo "## Import Counts"
  echo
  echo '```text'
  sed -n '1,20p' "$IMPORT_COUNTS_TSV"
  echo '```'
} >"$SUMMARY_MD"

echo "Manifest written to: $OUT_DIR"
echo "- $SUMMARY_MD"
