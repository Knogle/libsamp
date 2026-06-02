#!/usr/bin/env python3
"""Build the SA-MP loading-screen TXD from the local JPG.

OLD_02X_REF:
The 0.2x client patches GTA SA's LOADSCS texture lookup to "title" and ships a
LOADSCS.txd with a single 1024x1024 DXT1 texture by that name. We keep that TXD
header/name/footer and only replace the raster payload.
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


RASTER_OFFSET = 0x90
DDS_HEADER_BYTES = 128
DXT1_1024_PAYLOAD_BYTES = 1024 * 1024 // 2


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def parse_args() -> argparse.Namespace:
    root = repo_root()
    parser = argparse.ArgumentParser(description="Build assets/LOADSCS.txd from loadingscreen.jpg")
    parser.add_argument("--source", type=Path, default=root / "loadingscreen.jpg")
    parser.add_argument(
        "--template",
        type=Path,
        default=root / "gta-samp-0.2x-source-code" / "archive" / "files" / "LOADSCS.txd",
    )
    parser.add_argument("--output", type=Path, default=root / "assets" / "LOADSCS.txd")
    return parser.parse_args()


def find_magick() -> str:
    magick = shutil.which("magick")
    if magick:
        return magick
    convert = shutil.which("convert")
    if convert:
        return convert
    raise RuntimeError("ImageMagick not found: expected 'magick' or 'convert' in PATH")


def build_dds(source: Path, dds: Path) -> None:
    cmd = [
        find_magick(),
        str(source),
        "-resize",
        "1024x1024",
        "-background",
        "black",
        "-gravity",
        "center",
        "-extent",
        "1024x1024",
        "-define",
        "dds:compression=dxt1",
        "-define",
        "dds:mipmaps=0",
        str(dds),
    ]
    subprocess.run(cmd, check=True)


def main() -> int:
    args = parse_args()
    source = args.source
    template = args.template
    output = args.output

    if not source.is_file():
        raise FileNotFoundError(source)
    if not template.is_file():
        raise FileNotFoundError(template)

    txd = bytearray(template.read_bytes())
    if b"title" not in txd[:0x80] or txd[0x80:0x84] != b"DXT1":
        raise RuntimeError(f"{template} does not look like the legacy SA-MP LOADSCS.txd template")
    if len(txd) < RASTER_OFFSET + DXT1_1024_PAYLOAD_BYTES:
        raise RuntimeError(f"{template} is too small for a 1024x1024 DXT1 raster")

    with tempfile.TemporaryDirectory(prefix="loadsctxd-") as temp:
        dds = Path(temp) / "title.dds"
        build_dds(source, dds)
        dds_bytes = dds.read_bytes()

    if len(dds_bytes) != DDS_HEADER_BYTES + DXT1_1024_PAYLOAD_BYTES:
        raise RuntimeError(f"Unexpected DDS size {len(dds_bytes)} bytes; expected 1024x1024 DXT1")
    if dds_bytes[84:88] != b"DXT1":
        raise RuntimeError("Generated DDS is not DXT1")

    txd[RASTER_OFFSET : RASTER_OFFSET + DXT1_1024_PAYLOAD_BYTES] = dds_bytes[DDS_HEADER_BYTES:]
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(txd)
    print(f"wrote {output} ({len(txd)} bytes)")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"build_loadscs_txd.py: {exc}", file=sys.stderr)
        raise SystemExit(1)
