#!/usr/bin/env python3
"""Build Crowny-Editor's deterministic built-in resource pack."""

from __future__ import annotations

import argparse
import glob
import struct
import sys
from pathlib import Path


MAGIC = b"CWPACK01"
VERSION = 1
HEADER = struct.Struct("<8sIIQ")
ENTRY = struct.Struct("<HHQQ")
RESOURCE_GLOBS = (
    "Resources/Shaders/*.asset",
    "Resources/Fonts/Roboto/roboto-thin.ttf.asset",
    "Resources/Fonts/Roboto/Roboto-Regular.ttf",
    "Resources/Fonts/Roboto/Roboto-Bold.ttf",
    "Resources/Icons/*.asset",
    "Resources/Textures/*.asset",
    "Resources/Default/*",
)
COOKED_SOURCES = (
    "Resources/Icons/Play.png", "Resources/Icons/Pause.png", "Resources/Icons/Stop.png",
    "Resources/Icons/File.png", "Resources/Icons/Folder.png", "Resources/Icons/ArrowPointerIcon.png",
    "Resources/Icons/ArrowsIcon.png", "Resources/Icons/RotateIcon.png", "Resources/Icons/MaximizeIcon.png",
    "Resources/Icons/GlobeIcon.png", "Resources/Icons/SearchIcon.png", "Resources/Icons/ConsoleInfo.png",
    "Resources/Icons/ConsoleWarn.png", "Resources/Icons/ConsoleError.png", "Resources/Icons/AlignLeft.png",
    "Resources/Icons/AlignCenter.png", "Resources/Icons/AlignRight.png",
    "Resources/Fonts/Roboto/roboto-thin.ttf",
)
ASSET_MAGIC = struct.pack("<I", 0x43574E59)


def has_asset_header(path: Path) -> bool:
    with path.open("rb") as stream:
        return ASSET_MAGIC in stream.read(512)


def collect_files(source_root: Path) -> list[Path]:
    source_root = source_root.resolve()
    files: set[Path] = set()
    for pattern in RESOURCE_GLOBS:
        for match in glob.glob(str(source_root / pattern)):
            path = Path(match).resolve()
            if path.is_file() and path.is_relative_to(source_root):
                files.add(path)
    return sorted(files, key=lambda path: path.relative_to(source_root).as_posix())


def cooked_errors(source_root: Path) -> list[str]:
    errors: list[str] = []
    for relative_source in COOKED_SOURCES:
        source = source_root / relative_source
        asset = Path(f"{source}.asset") if source.suffix.lower() == ".ttf" else source.with_suffix(".asset")
        relative_asset = asset.relative_to(source_root).as_posix()
        if not asset.is_file():
            errors.append(f"missing {relative_asset}")
        elif not has_asset_header(asset):
            errors.append(f"legacy {relative_asset}")
        elif source.stat().st_mtime_ns > asset.stat().st_mtime_ns:
            errors.append(f"stale {relative_asset}")
    brdf_asset = source_root / "Resources/Textures/Brdf.asset"
    if not brdf_asset.is_file():
        errors.append("missing Resources/Textures/Brdf.asset")
    elif not has_asset_header(brdf_asset):
        errors.append("legacy Resources/Textures/Brdf.asset")
    for source in (source_root / "Resources/Shaders").glob("*.glsl"):
        if "#lang" not in source.read_text(encoding="utf-8"):
            continue
        asset = source.with_suffix(".asset")
        relative_asset = asset.relative_to(source_root).as_posix()
        if not asset.is_file():
            errors.append(f"missing {relative_asset}")
        elif not has_asset_header(asset):
            errors.append(f"legacy {relative_asset}")
        elif source.stat().st_mtime_ns > asset.stat().st_mtime_ns:
            errors.append(f"stale {relative_asset}")
    return errors


def align(buffer: bytearray, alignment: int = 8) -> None:
    buffer.extend(b"\0" * (-len(buffer) % alignment))


def build_pack(source_root: Path) -> bytes:
    files = collect_files(source_root)
    if not files:
        raise RuntimeError(f"no built-in resources found below {source_root}")

    output = bytearray(HEADER.size)
    entries: list[tuple[bytes, int, int]] = []
    for path in files:
        align(output)
        logical_path = path.relative_to(source_root).as_posix().encode("utf-8")
        if not logical_path or len(logical_path) > 4096:
            raise RuntimeError(f"invalid resource path: {path}")
        offset = len(output)
        contents = path.read_bytes()
        output.extend(contents)
        entries.append((logical_path, offset, len(contents)))

    align(output)
    index_offset = len(output)
    for logical_path, offset, size in entries:
        output.extend(ENTRY.pack(len(logical_path), 0, offset, size))
        output.extend(logical_path)

    output[: HEADER.size] = HEADER.pack(MAGIC, VERSION, len(entries), index_offset)
    return bytes(output)


def is_current(output_path: Path, source_root: Path, files: list[Path]) -> bool:
    if not output_path.is_file() or not files:
        return False
    try:
        with output_path.open("rb") as stream:
            header = stream.read(HEADER.size)
            if len(header) != HEADER.size:
                return False
            magic, version, entry_count, index_offset = HEADER.unpack(header)
            if magic != MAGIC or version != VERSION or entry_count != len(files):
                return False
            stream.seek(index_offset)
            packed_paths: list[str] = []
            for _ in range(entry_count):
                record = stream.read(ENTRY.size)
                if len(record) != ENTRY.size:
                    return False
                path_length, _, _, _ = ENTRY.unpack(record)
                packed_paths.append(stream.read(path_length).decode("utf-8"))
    except (OSError, UnicodeDecodeError, struct.error):
        return False

    expected_paths = [path.relative_to(source_root).as_posix() for path in files]
    newest_source = max(path.stat().st_mtime_ns for path in files)
    return packed_paths == expected_paths and output_path.stat().st_mtime_ns >= newest_source


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--configuration", default="Release")
    parser.add_argument("--check", action="store_true", help="fail if the existing pack is stale")
    args = parser.parse_args()

    repo_root = args.repo_root.resolve()
    source_root = repo_root / "Crowny-Editor"
    output_path = source_root / "Resources" / "Builtin.cwpack"
    errors = cooked_errors(source_root)
    if errors:
        message = "Cooked built-ins are incomplete: " + ", ".join(errors) + ". Run a Release editor once with --cook-builtins."
        if args.check or args.configuration == "Dist":
            print(message, file=sys.stderr)
            return 1
        print("warning: " + message, file=sys.stderr)
    files = collect_files(source_root)
    if is_current(output_path, source_root, files):
        print(f"Built-in resource pack is current: {output_path}")
        return 0
    packed = build_pack(source_root)
    current = output_path.read_bytes() if output_path.is_file() else None

    if current == packed:
        print(f"Built-in resource pack is current: {output_path} ({len(packed)} bytes)")
        return 0
    if args.check:
        print(f"Built-in resource pack is stale: {output_path}", file=sys.stderr)
        return 1

    output_path.parent.mkdir(parents=True, exist_ok=True)
    temporary_path = output_path.with_suffix(output_path.suffix + ".tmp")
    temporary_path.write_bytes(packed)
    temporary_path.replace(output_path)
    print(f"Packed {len(collect_files(source_root))} resources into {output_path} ({len(packed)} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
