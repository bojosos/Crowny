"""Convert Bistro DDS textures to PNG, reconstructing BC5 tangent-space normals.

Requires Pillow. Existing project textures can be repaired with --existing-only
--normals-only without adding the unused interior textures to the project.
"""

import argparse
import math
from pathlib import Path
import shutil
import struct

from PIL import Image


def is_bc5(path):
    with path.open("rb") as source:
        header = source.read(148)
    fourcc = header[84:88]
    return fourcc == b"ATI2" or (
        fourcc == b"DX10" and len(header) >= 148 and struct.unpack_from("<I", header, 128)[0] == 83
    )


def normal_blue_lookup():
    return bytes(
        round((math.sqrt(max(0.0, 1.0 - (r / 127.5 - 1.0) ** 2 - (g / 127.5 - 1.0) ** 2)) + 1.0) * 127.5)
        for r in range(256)
        for g in range(256)
    )


def convert(source, destination, maximum_size, blue_lookup):
    normal = is_bc5(source)
    with Image.open(source) as decoded:
        pixels = decoded.convert("RGB" if normal else "RGBA")
        pixels.thumbnail((maximum_size, maximum_size), Image.Resampling.LANCZOS)
    if normal:
        data = bytearray(pixels.tobytes())
        for offset in range(0, len(data), 3):
            data[offset + 2] = blue_lookup[(data[offset] << 8) | data[offset + 1]]
        pixels = Image.frombytes("RGB", pixels.size, bytes(data))
    pixels.save(destination)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    parser.add_argument("--max-size", type=int, default=512)
    parser.add_argument("--existing-only", action="store_true")
    parser.add_argument("--normals-only", action="store_true")
    parser.add_argument("--backup-dir", type=Path)
    args = parser.parse_args()
    if args.max_size < 1:
        parser.error("--max-size must be positive")
    args.destination.mkdir(parents=True, exist_ok=True)
    lookup = normal_blue_lookup()
    count = 0
    for source in sorted(args.source.glob("*.dds")):
        destination = args.destination / (source.stem + ".png")
        if args.existing_only and not destination.exists():
            continue
        if args.normals_only and not is_bc5(source):
            continue
        if args.backup_dir and destination.exists():
            args.backup_dir.mkdir(parents=True, exist_ok=True)
            backup = args.backup_dir / destination.name
            if not backup.exists():
                shutil.copy2(destination, backup)
        convert(source, destination, args.max_size, lookup)
        count += 1
    print(f"Converted {count} DDS textures to {args.destination}")


if __name__ == "__main__":
    main()
