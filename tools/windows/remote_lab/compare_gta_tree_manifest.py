#!/usr/bin/env python3
"""Compare a Windows GTA SHA-256 CSV manifest with a local GTA tree."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
from collections import Counter
from pathlib import Path


def normalized_path(value: str) -> str:
    return value.replace("/", "\\").casefold()


def hash_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_windows_manifest(path: Path) -> dict[str, dict[str, object]]:
    records: dict[str, dict[str, object]] = {}
    with path.open("r", encoding="utf-8-sig", newline="") as stream:
        for row in csv.DictReader(stream):
            records[normalized_path(row["Path"])] = {
                "path": row["Path"],
                "length": int(row["Length"]),
                "sha256": row["SHA256"].lower(),
            }
    return records


def load_local_tree(root: Path) -> dict[str, dict[str, object]]:
    records: dict[str, dict[str, object]] = {}
    for path in sorted(item for item in root.rglob("*") if item.is_file()):
        relative = path.relative_to(root).as_posix().replace("/", "\\")
        records[normalized_path(relative)] = {
            "path": relative,
            "length": path.stat().st_size,
            "sha256": hash_file(path),
        }
    return records


def top_level_counts(records: list[dict[str, object]]) -> dict[str, int]:
    counts = Counter(str(record["path"]).split("\\", 1)[0] for record in records)
    return dict(sorted(counts.items(), key=lambda item: item[0].casefold()))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("windows_manifest", type=Path)
    parser.add_argument("local_root", type=Path)
    parser.add_argument("--details-limit", type=int, default=200)
    args = parser.parse_args()

    windows = load_windows_manifest(args.windows_manifest)
    local = load_local_tree(args.local_root.resolve())
    windows_keys = set(windows)
    local_keys = set(local)
    common_keys = windows_keys & local_keys

    changed = []
    same = 0
    for key in sorted(common_keys):
        windows_record = windows[key]
        local_record = local[key]
        if (
            windows_record["length"] == local_record["length"]
            and windows_record["sha256"] == local_record["sha256"]
        ):
            same += 1
        else:
            changed.append({"windows": windows_record, "local": local_record})

    windows_only = [windows[key] for key in sorted(windows_keys - local_keys)]
    local_only = [local[key] for key in sorted(local_keys - windows_keys)]
    limit = max(0, args.details_limit)
    report = {
        "identical": not changed and not windows_only and not local_only,
        "counts": {
            "windows": len(windows),
            "local": len(local),
            "common": len(common_keys),
            "same": same,
            "changed": len(changed),
            "windows_only": len(windows_only),
            "local_only": len(local_only),
        },
        "windows_only_by_top_level": top_level_counts(windows_only),
        "local_only_by_top_level": top_level_counts(local_only),
        "changed": changed[:limit],
        "windows_only": windows_only[:limit],
        "local_only": local_only[:limit],
        "details_truncated": {
            "changed": max(0, len(changed) - limit),
            "windows_only": max(0, len(windows_only) - limit),
            "local_only": max(0, len(local_only) - limit),
        },
    }
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0 if report["identical"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
