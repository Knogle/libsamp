#!/usr/bin/env python3
import argparse
import os
import shutil
import struct
import sys

import lief


def import_key(name: str) -> str:
    return name.upper()


def entry_key(entry) -> tuple[str, int]:
    if entry.is_ordinal:
        return ("", int(entry.ordinal))
    return (entry.name or "", -1)


def get_candidate_import(binary, dll_name: str):
    target = import_key(dll_name)
    for imp in binary.imports:
        if import_key(imp.name) == target:
            return imp
    return None


def ensure_import_entry(candidate_import, reference_entry):
    wanted = entry_key(reference_entry)
    for entry in candidate_import.entries:
        if entry_key(entry) == wanted:
            return False

    if reference_entry.is_ordinal:
        new_entry = lief.PE.ImportEntry(reference_entry.data, lief.PE.PE_TYPE.PE32)
        candidate_import.add_entry(new_entry)
    else:
        candidate_import.add_entry(reference_entry.name)
    return True


def read_preserved_string_table(path: str) -> bytes:
    with open(path, "rb") as f:
        data = f.read()

    if len(data) < 0x40:
        return b""

    pe_offset = struct.unpack_from("<I", data, 0x3C)[0]
    file_header_offset = pe_offset + 4
    if file_header_offset + 20 > len(data):
        return b""

    pointer_to_symbol_table = struct.unpack_from("<I", data, file_header_offset + 8)[0]
    number_of_symbols = struct.unpack_from("<I", data, file_header_offset + 12)[0]
    if number_of_symbols != 0 or pointer_to_symbol_table == 0 or pointer_to_symbol_table >= len(data):
        return b""

    tail = data[pointer_to_symbol_table:]
    if len(tail) < 4:
        return b""

    declared_size = struct.unpack_from("<I", tail, 0)[0]
    if declared_size < 4 or declared_size > len(tail):
        return b""

    return tail[:declared_size]


def graft_string_table(path: str, string_table: bytes) -> None:
    if not string_table:
        return

    with open(path, "rb") as f:
        data = bytearray(f.read())

    if len(data) < 0x40:
        return

    pe_offset = struct.unpack_from("<I", data, 0x3C)[0]
    file_header_offset = pe_offset + 4
    if file_header_offset + 20 > len(data):
        return

    new_ptr = len(data)
    data.extend(string_table)
    struct.pack_into("<I", data, file_header_offset + 8, new_ptr)
    struct.pack_into("<I", data, file_header_offset + 12, 0)

    with open(path, "wb") as f:
        f.write(data)


def shape_imports(reference_path: str, candidate_path: str, output_path: str, extend_existing: bool) -> tuple[int, int, int]:
    reference = lief.PE.parse(reference_path)
    candidate = lief.PE.parse(candidate_path)

    if reference is None:
        raise RuntimeError(f"failed to parse reference PE: {reference_path}")
    if candidate is None:
        raise RuntimeError(f"failed to parse candidate PE: {candidate_path}")

    preserved_string_table = read_preserved_string_table(candidate_path)
    renamed = 0
    added_dlls = 0
    added_entries = 0

    for ref_import in reference.imports:
        cand_import = get_candidate_import(candidate, ref_import.name)
        if cand_import is None:
            cand_import = candidate.add_import(ref_import.name)
            added_dlls += 1
        elif cand_import.name != ref_import.name:
            cand_import.name = ref_import.name
            renamed += 1
            if not extend_existing:
                continue
        elif not extend_existing:
            continue

        for ref_entry in ref_import.entries:
            if ensure_import_entry(cand_import, ref_entry):
                added_entries += 1

    config = lief.PE.Builder.config_t()
    config.imports = True
    candidate.write(output_path, config)
    graft_string_table(output_path, preserved_string_table)
    return renamed, added_dlls, added_entries


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Post-link PE import shaper: copy reference import surface into a candidate DLL."
    )
    parser.add_argument("reference", help="reference PE/DLL path")
    parser.add_argument("candidate", help="candidate PE/DLL path")
    parser.add_argument(
        "output",
        nargs="?",
        help="output PE/DLL path; defaults to in-place rewrite of candidate",
    )
    parser.add_argument(
        "--backup",
        action="store_true",
        help="create <output>.bak before writing when output already exists",
    )
    parser.add_argument(
        "--extend-existing-libraries",
        action="store_true",
        help="also add missing imports into already-existing libraries; more invasive and can disturb the current IAT layout",
    )
    args = parser.parse_args()

    output = args.output or args.candidate

    if not os.path.isfile(args.reference):
        print(f"reference not found: {args.reference}", file=sys.stderr)
        return 2
    if not os.path.isfile(args.candidate):
        print(f"candidate not found: {args.candidate}", file=sys.stderr)
        return 2

    if args.backup and os.path.exists(output):
        shutil.copy2(output, output + ".bak")

    renamed, added_dlls, added_entries = shape_imports(
        args.reference,
        args.candidate,
        output,
        args.extend_existing_libraries,
    )
    print(f"wrote: {output}")
    print(f"renamed_dlls: {renamed}")
    print(f"added_dlls: {added_dlls}")
    print(f"added_entries: {added_entries}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
