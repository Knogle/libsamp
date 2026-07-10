#!/usr/bin/env python3
"""Offline SA-MP asset archive/IDE probe helper.

Reads the local SAMP asset sidecars without extracting files permanently:
SAMP.ide, CUSTOM.ide, SAMP.img, CUSTOM.img and SAMPCOL.img.  Optionally folds
in the latest original-probe log observations for model_info/col_model data.
"""

from __future__ import annotations

import argparse
import collections
import dataclasses
import os
import re
import struct
import sys
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Sequence, Tuple


SECTOR_SIZE = 2048
IMG_ENTRY_SIZE = 32
IMG_NAME_SIZE = 24
COL_MAGICS = {b"COLL", b"COL2", b"COL3", b"COL4"}


RW_CHUNK_NAMES = {
    0x01: "Struct",
    0x02: "String",
    0x03: "Extension",
    0x06: "Texture",
    0x07: "Material",
    0x08: "MaterialList",
    0x0E: "FrameList",
    0x0F: "Geometry",
    0x10: "Clump",
    0x14: "Atomic",
    0x15: "TextureNative",
    0x16: "TextureDictionary",
    0x1A: "GeometryList",
    0x253F2F8: "BinMeshPlugin",
    0x253F2FC: "NativeDataPlugin",
    0x253F2FE: "MaterialEffects",
    0x253F2FF: "SkinPlugin",
    0x253F31F: "HAnimPlugin",
    0x253F33E: "UserDataPlugin",
    0x253F340: "CollisionPlugin",
}


@dataclasses.dataclass
class ImgEntry:
    archive: str
    name: str
    offset_sectors: int
    size_sectors: int

    @property
    def offset(self) -> int:
        return self.offset_sectors * SECTOR_SIZE

    @property
    def size(self) -> int:
        return self.size_sectors * SECTOR_SIZE


@dataclasses.dataclass
class IdeEntry:
    source: str
    section: str
    model_id: int
    model_name: str
    txd_name: str
    draw_distance: float = 0.0
    flags: int = 0


@dataclasses.dataclass
class ColEntry:
    name: str
    magic: str
    offset: int
    payload_size: int
    total_size: int


@dataclasses.dataclass
class ProbeObservation:
    cphysical_add_begin: int = 0
    cphysical_add_end: int = 0
    model_infos: collections.Counter[str] = dataclasses.field(default_factory=collections.Counter)
    txd_indexes: collections.Counter[int] = dataclasses.field(default_factory=collections.Counter)
    draw_distances: collections.Counter[str] = dataclasses.field(default_factory=collections.Counter)
    col_models: collections.Counter[str] = dataclasses.field(default_factory=collections.Counter)
    stack_rvas: collections.Counter[str] = dataclasses.field(default_factory=collections.Counter)


def norm_name(name: str) -> str:
    return name.strip().lower()


def ext_name(base: str, ext: str) -> str:
    base = norm_name(base)
    if "." in base:
        return base
    return f"{base}.{ext}"


def parse_img(path: Path, archive: str) -> List[ImgEntry]:
    data = path.read_bytes()
    if len(data) < 8 or data[:4] != b"VER2":
        raise ValueError(f"{path}: unsupported IMG header")
    count = struct.unpack_from("<I", data, 4)[0]
    if 8 + count * IMG_ENTRY_SIZE > len(data):
        raise ValueError(f"{path}: short IMG directory count={count}")

    entries: List[ImgEntry] = []
    pos = 8
    for _ in range(count):
        offset_sectors, size_sectors = struct.unpack_from("<II", data, pos)
        raw_name = data[pos + 8 : pos + 8 + IMG_NAME_SIZE]
        name = raw_name.split(b"\0", 1)[0].decode("latin-1", errors="replace")
        if name:
            entries.append(ImgEntry(archive, norm_name(name), offset_sectors, size_sectors))
        pos += IMG_ENTRY_SIZE
    return entries


def read_img_entry(path: Path, entry: ImgEntry) -> bytes:
    with path.open("rb") as handle:
        handle.seek(entry.offset)
        return handle.read(entry.size)


def strip_comment(line: str) -> str:
    hash_at = line.find("#")
    slash_at = line.find("//")
    cut = len(line)
    if hash_at >= 0:
        cut = min(cut, hash_at)
    if slash_at >= 0:
        cut = min(cut, slash_at)
    return line[:cut].strip()


def parse_ide(path: Path, source: str) -> List[IdeEntry]:
    if not path.exists() or path.stat().st_size == 0:
        return []

    entries: List[IdeEntry] = []
    section = ""
    for raw_line in path.read_text(encoding="latin-1", errors="replace").splitlines():
        line = strip_comment(raw_line)
        if not line:
            continue
        lower = norm_name(line)
        if lower in {"objs", "tobj", "anim", "peds", "txdp", "path", "2dfx"}:
            section = lower
            continue
        if lower == "end":
            section = ""
            continue
        if section not in {"objs", "tobj", "anim"}:
            continue

        parts = [part.strip() for part in line.split(",")]
        if len(parts) < 3:
            continue
        try:
            model_id = int(parts[0], 10)
        except ValueError:
            continue
        model_name = norm_name(parts[1])
        txd_name = norm_name(parts[2])
        draw_distance = 0.0
        flags = 0
        if section in {"objs", "tobj"}:
            if len(parts) >= 4:
                try:
                    draw_distance = float(parts[3])
                except ValueError:
                    draw_distance = 0.0
            if len(parts) >= 5:
                try:
                    flags = int(parts[4], 0)
                except ValueError:
                    flags = 0
        elif section == "anim":
            if len(parts) >= 5:
                try:
                    draw_distance = float(parts[4])
                except ValueError:
                    draw_distance = 0.0
            if len(parts) >= 6:
                try:
                    flags = int(parts[5], 0)
                except ValueError:
                    flags = 0
        entries.append(IdeEntry(source, section, model_id, model_name, txd_name, draw_distance, flags))
    return entries


def parse_col_stream(data: bytes) -> Tuple[List[ColEntry], int]:
    entries: List[ColEntry] = []
    pos = 0
    bad_at = -1
    while pos + 8 <= len(data):
        magic = data[pos : pos + 4]
        if magic not in COL_MAGICS:
            if data[pos:] and any(byte != 0 for byte in data[pos:]):
                bad_at = pos
            break
        payload_size = struct.unpack_from("<I", data, pos + 4)[0]
        total_size = payload_size + 8
        if total_size <= 8 or pos + total_size > len(data):
            bad_at = pos
            break
        raw_name = data[pos + 8 : min(pos + 8 + 22, pos + total_size)]
        name = raw_name.split(b"\0", 1)[0].decode("latin-1", errors="replace")
        entries.append(ColEntry(norm_name(name), magic.decode("ascii"), pos, payload_size, total_size))
        pos += total_size
        if pos & 3:
            pos = (pos + 3) & ~3
    return entries, bad_at


def walk_rw_chunks(data: bytes, start: int = 0, end: Optional[int] = None, depth: int = 0) -> collections.Counter[int]:
    if end is None or end > len(data):
        end = len(data)
    counts: collections.Counter[int] = collections.Counter()
    pos = start
    while pos + 12 <= end:
        chunk_id, length, _version = struct.unpack_from("<III", data, pos)
        next_pos = pos + 12 + length
        if length > len(data) or next_pos > end or chunk_id == 0:
            break
        counts[chunk_id] += 1
        # Recursing into all chunks is noisy but safe with the bounds above; cap
        # depth to avoid false positives inside compressed/native payloads.
        if depth < 3 and chunk_id in {0x10, 0x16, 0x1A, 0x0F, 0x08, 0x07, 0x06, 0x03}:
            counts.update(walk_rw_chunks(data, pos + 12, next_pos, depth + 1))
        pos = next_pos
    return counts


def summarize_rw(data: bytes) -> str:
    counts = walk_rw_chunks(data)
    if not counts:
        return "rw=unparsed"
    parts = []
    for chunk_id, count in counts.most_common(8):
        name = RW_CHUNK_NAMES.get(chunk_id, f"0x{chunk_id:08x}")
        parts.append(f"{name}:{count}")
    return "rw=" + ",".join(parts)


def parse_probe_log(path: Optional[Path]) -> Dict[int, ProbeObservation]:
    observations: Dict[int, ProbeObservation] = collections.defaultdict(ProbeObservation)
    if path is None or not path.exists():
        return observations

    lines = path.read_text(encoding="latin-1", errors="replace").splitlines()
    latest_attach = 0
    for index, line in enumerate(lines):
        if "probe: attached" in line:
            latest_attach = index
    for line in lines[latest_attach:]:
        if "gta_asset: CPhysical.Add.begin" in line:
            model = extract_int(line, r"model=(\d+)")
            if model is not None:
                observations[model].cphysical_add_begin += 1
            continue
        if "gta_asset: CPhysical.Add.end" in line:
            model = extract_int(line, r"model=(\d+)")
            if model is not None:
                observations[model].cphysical_add_end += 1
            continue
        if "gta_object_info: phase=CPhysical.Add.begin" in line:
            model = extract_int(line, r"model=(\d+)")
            if model is None:
                continue
            obs = observations[model]
            for key, pattern, counter in [
                ("model_info", r"model_info=([0-9a-fA-F]+)", obs.model_infos),
                ("col_model", r"col_model=0x([0-9a-fA-F]+)", obs.col_models),
                ("draw_distance", r"draw_distance=([0-9.]+)", obs.draw_distances),
            ]:
                match = re.search(pattern, line)
                if match:
                    counter[match.group(1).lower()] += 1
            txd = extract_int(line, r"txd_index=(\d+)")
            if txd is not None:
                obs.txd_indexes[txd] += 1
            continue
        if "custom_object_heavy: physical_add" in line:
            model = extract_int(line, r"model=(\d+)")
            if model is None:
                continue
            for rva in re.findall(r"0x[0-9a-fA-F]{6,8}", line):
                observations[model].stack_rvas[rva.lower()] += 1
    return observations


def extract_int(text: str, pattern: str) -> Optional[int]:
    match = re.search(pattern, text)
    if not match:
        return None
    try:
        return int(match.group(1), 10)
    except ValueError:
        return None


def first_counter(counter: collections.Counter) -> str:
    if not counter:
        return "-"
    value, count = counter.most_common(1)[0]
    return f"{value}({count})"


def collect_model_ids(range_args: Sequence[str], model_args: Sequence[str]) -> List[int]:
    result = set()
    for item in range_args:
        start_s, end_s = item.split(":", 1)
        start = int(start_s, 10)
        end = int(end_s, 10)
        if end < start:
            start, end = end, start
        result.update(range(start, end + 1))
    for item in model_args:
        for part in item.split(","):
            part = part.strip()
            if part:
                result.add(int(part, 10))
    return sorted(result)


def archive_path(samp_dir: Path, archive: str) -> Path:
    for name in [archive, archive.upper(), archive.lower()]:
        candidate = samp_dir / name
        if candidate.exists():
            return candidate
    return samp_dir / archive


def main(argv: Sequence[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--samp-dir", default="SAMP", type=Path)
    parser.add_argument("--probe-log", type=Path)
    parser.add_argument("--range", action="append", default=["11682:11753"], dest="ranges")
    parser.add_argument("--models", action="append", default=["11682,11683,11684,11685,11686,11687,11692,11751,11752,11753"])
    parser.add_argument("--show-table", action="store_true")
    args = parser.parse_args(argv)

    samp_dir = args.samp_dir
    if not samp_dir.exists():
        print(f"missing samp dir: {samp_dir}", file=sys.stderr)
        return 2

    img_files = {
        "SAMP.img": archive_path(samp_dir, "SAMP.img"),
        "CUSTOM.img": archive_path(samp_dir, "custom.img"),
        "SAMPCOL.img": archive_path(samp_dir, "SAMPCOL.img"),
    }
    archives: Dict[str, List[ImgEntry]] = {}
    all_img: Dict[str, ImgEntry] = {}
    for label, path in img_files.items():
        entries = parse_img(path, label)
        archives[label] = entries
        for entry in entries:
            all_img[entry.name] = entry

    ide_entries = parse_ide(samp_dir / "SAMP.ide", "SAMP.ide")
    ide_entries.extend(parse_ide(samp_dir / "CUSTOM.ide", "CUSTOM.ide"))
    ide_by_model = {entry.model_id: entry for entry in ide_entries}

    sampcol_entries: List[ColEntry] = []
    col_bad_at = -1
    all_col_entry = all_img.get("allsampcols.col")
    if all_col_entry is not None:
        col_data = read_img_entry(img_files["SAMPCOL.img"], all_col_entry)
        sampcol_entries, col_bad_at = parse_col_stream(col_data)
    col_names = {entry.name for entry in sampcol_entries}

    observations = parse_probe_log(args.probe_log)
    model_ids = collect_model_ids(args.ranges, args.models)

    print("## SAMP Asset Offline Summary")
    print(f"samp_dir={samp_dir}")
    for label, entries in archives.items():
        by_ext = collections.Counter(Path(entry.name).suffix.lower() or "<none>" for entry in entries)
        ext_summary = " ".join(f"{ext}:{count}" for ext, count in sorted(by_ext.items()))
        print(f"archive {label}: entries={len(entries)} {ext_summary}")
    ide_by_range = collections.Counter()
    for entry in ide_entries:
        if 11682 <= entry.model_id <= 12799:
            ide_by_range["low_11682_12799"] += 1
        elif 15065 <= entry.model_id <= 15999:
            ide_by_range["custom_15065_15999"] += 1
        elif 18631 <= entry.model_id <= 19999:
            ide_by_range["high_18631_19999"] += 1
        else:
            ide_by_range["other"] += 1
    print(f"ide entries={len(ide_entries)} ranges={dict(sorted(ide_by_range.items()))}")
    print(f"all_sampcols chunks={len(sampcol_entries)} bad_at={col_bad_at}")

    missing_dff = []
    missing_txd = []
    missing_col = []
    for model_id in range(11682, 11754):
        entry = ide_by_model.get(model_id)
        if entry is None:
            continue
        dff_name = ext_name(entry.model_name, "dff")
        txd_name = ext_name(entry.txd_name or entry.model_name, "txd")
        if dff_name not in all_img:
            missing_dff.append((model_id, dff_name))
        if txd_name not in all_img:
            missing_txd.append((model_id, txd_name))
        if entry.model_name not in col_names:
            missing_col.append((model_id, entry.model_name))
    print(f"low_range_11682_11753 missing_dff={len(missing_dff)} missing_txd={len(missing_txd)} missing_col_name={len(missing_col)}")
    if missing_dff[:8]:
        print("missing_dff_sample=" + ", ".join(f"{mid}:{name}" for mid, name in missing_dff[:8]))
    if missing_txd[:8]:
        print("missing_txd_sample=" + ", ".join(f"{mid}:{name}" for mid, name in missing_txd[:8]))
    if missing_col[:8]:
        print("missing_col_sample=" + ", ".join(f"{mid}:{name}" for mid, name in missing_col[:8]))

    print("")
    print("## Target Models")
    print("model,name,txd,draw,flags,dff,txd_file,col,probe_add_begin,probe_model_info,probe_txd,probe_draw,probe_col,stack_rvas")
    for model_id in model_ids:
        entry = ide_by_model.get(model_id)
        obs = observations.get(model_id, ProbeObservation())
        if entry is None:
            print(f"{model_id},<missing_ide>,,,,,,,,,,,")
            continue
        dff_name = ext_name(entry.model_name, "dff")
        txd_name = ext_name(entry.txd_name or entry.model_name, "txd")
        dff_entry = all_img.get(dff_name)
        txd_entry = all_img.get(txd_name)
        col_state = "yes" if entry.model_name in col_names else "no"
        dff_state = f"{dff_entry.archive}:{dff_entry.size_sectors}" if dff_entry else "missing"
        txd_state = f"{txd_entry.archive}:{txd_entry.size_sectors}" if txd_entry else "missing"
        stack_rvas = ";".join(f"{rva}:{count}" for rva, count in obs.stack_rvas.most_common(6)) or "-"
        print(
            f"{model_id},{entry.model_name},{entry.txd_name},{entry.draw_distance:g},0x{entry.flags:08x},"
            f"{dff_state},{txd_state},{col_state},{obs.cphysical_add_begin},"
            f"{first_counter(obs.model_infos)},{first_counter(obs.txd_indexes)},"
            f"{first_counter(obs.draw_distances)},{first_counter(obs.col_models)},{stack_rvas}"
        )

    print("")
    print("## RW Samples")
    sample_ids = [model for model in model_ids if model in observations or model in {11682, 11683, 11684, 11685, 11686, 11687, 11692, 11753}]
    for model_id in sample_ids[:24]:
        entry = ide_by_model.get(model_id)
        if entry is None:
            continue
        dff_entry = all_img.get(ext_name(entry.model_name, "dff"))
        txd_entry = all_img.get(ext_name(entry.txd_name or entry.model_name, "txd"))
        parts = [f"model={model_id}", f"name={entry.model_name}"]
        if dff_entry:
            dff_data = read_img_entry(img_files[dff_entry.archive], dff_entry)
            parts.append(f"dff_size={dff_entry.size}")
            parts.append("dff_" + summarize_rw(dff_data))
        if txd_entry:
            txd_data = read_img_entry(img_files[txd_entry.archive], txd_entry)
            parts.append(f"txd_size={txd_entry.size}")
            parts.append("txd_" + summarize_rw(txd_data))
        print(" ".join(parts))

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
