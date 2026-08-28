#!/usr/bin/env python3
"""Generate compact Unicode 17.0 grapheme-property tables for Crowny."""

from __future__ import annotations

import argparse
import hashlib
import pathlib
import sys
import urllib.request


UNICODE_VERSION = "17.0.0"
REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parents[1]
OUTPUT_PATH = REPOSITORY_ROOT / "Crowny/Source/Crowny/Common/UnicodeGraphemeData.inl"
SOURCES = {
    "GraphemeBreakProperty.txt": (
        f"https://www.unicode.org/Public/{UNICODE_VERSION}/ucd/auxiliary/GraphemeBreakProperty.txt",
        "d6b51d1d2ae5c33b451b7ed994b48f1f4dc62b2272a5831e7fd418514a6bae89",
    ),
    "emoji-data.txt": (
        f"https://www.unicode.org/Public/{UNICODE_VERSION}/ucd/emoji/emoji-data.txt",
        "2cb2bb9455cda83e8481541ecf5b6dfda66a3bb89efa3fa7c5297eccf607b72b",
    ),
    "DerivedCoreProperties.txt": (
        f"https://www.unicode.org/Public/{UNICODE_VERSION}/ucd/DerivedCoreProperties.txt",
        "24c7fed1195c482faaefd5c1e7eb821c5ee1fb6de07ecdbaa64b56a99da22c08",
    ),
    "DerivedGeneralCategory.txt": (
        f"https://www.unicode.org/Public/{UNICODE_VERSION}/ucd/extracted/DerivedGeneralCategory.txt",
        "d62e5bab70ca74f099343f71224fa051cb1fdd61a1ab45c0488c44cfc0b6102e",
    ),
}

GRAPHEME_PROPERTY_NAMES = {
    "CR": "CR",
    "LF": "LF",
    "Control": "Control",
    "Extend": "Extend",
    "ZWJ": "ZWJ",
    "Regional_Indicator": "RegionalIndicator",
    "Prepend": "Prepend",
    "SpacingMark": "SpacingMark",
    "L": "L",
    "V": "V",
    "T": "T",
    "LV": "LV",
    "LVT": "LVT",
}
INDIC_PROPERTY_NAMES = {"Consonant": "Consonant", "Extend": "Extend", "Linker": "Linker"}


def read_source(name: str, data_directory: pathlib.Path | None) -> str:
    url, expected_hash = SOURCES[name]
    data = (data_directory / name).read_bytes() if data_directory else urllib.request.urlopen(url).read()
    actual_hash = hashlib.sha256(data).hexdigest()
    if actual_hash != expected_hash:
        raise RuntimeError(f"{name} SHA-256 mismatch: expected {expected_hash}, got {actual_hash}")
    return data.decode("utf-8")


def parse_range(value: str) -> tuple[int, int]:
    bounds = value.split("..")
    first = int(bounds[0], 16)
    return first, int(bounds[-1], 16)


def merge_ranges(ranges: list[tuple[int, int, str]]) -> list[tuple[int, int, str]]:
    merged: list[tuple[int, int, str]] = []
    for first, last, property_name in sorted(ranges):
        if merged and merged[-1][2] == property_name and merged[-1][1] + 1 == first:
            merged[-1] = (merged[-1][0], last, property_name)
        else:
            merged.append((first, last, property_name))
    return merged


def parse_grapheme_ranges(contents: str) -> list[tuple[int, int, str]]:
    ranges = []
    for raw_line in contents.splitlines():
        fields = [field.strip() for field in raw_line.split("#", 1)[0].split(";")]
        if len(fields) != 2 or fields[1] not in GRAPHEME_PROPERTY_NAMES:
            continue
        ranges.append((*parse_range(fields[0]), GRAPHEME_PROPERTY_NAMES[fields[1]]))
    return merge_ranges(ranges)


def parse_indic_ranges(contents: str) -> list[tuple[int, int, str]]:
    ranges = []
    for raw_line in contents.splitlines():
        fields = [field.strip() for field in raw_line.split("#", 1)[0].split(";")]
        if len(fields) != 3 or fields[1] != "InCB" or fields[2] not in INDIC_PROPERTY_NAMES:
            continue
        ranges.append((*parse_range(fields[0]), INDIC_PROPERTY_NAMES[fields[2]]))
    return merge_ranges(ranges)


def parse_extended_pictographic_ranges(contents: str) -> list[tuple[int, int]]:
    ranges = []
    for raw_line in contents.splitlines():
        fields = [field.strip() for field in raw_line.split("#", 1)[0].split(";")]
        if len(fields) == 2 and fields[1] == "Extended_Pictographic":
            ranges.append(parse_range(fields[0]))
    merged: list[tuple[int, int]] = []
    for first, last in sorted(ranges):
        if merged and merged[-1][1] + 1 == first:
            merged[-1] = (merged[-1][0], last)
        else:
            merged.append((first, last))
    return merged


def parse_code_point_ranges(contents: str, properties: set[str]) -> list[tuple[int, int]]:
    ranges = []
    for raw_line in contents.splitlines():
        fields = [field.strip() for field in raw_line.split("#", 1)[0].split(";")]
        if len(fields) == 2 and fields[1] in properties:
            ranges.append(parse_range(fields[0]))
    merged: list[tuple[int, int]] = []
    for first, last in sorted(ranges):
        if merged and merged[-1][1] + 1 == first:
            merged[-1] = (merged[-1][0], last)
        else:
            merged.append((first, last))
    return merged


def generate(data_directory: pathlib.Path | None) -> str:
    grapheme_ranges = parse_grapheme_ranges(read_source("GraphemeBreakProperty.txt", data_directory))
    indic_ranges = parse_indic_ranges(read_source("DerivedCoreProperties.txt", data_directory))
    pictographic_ranges = parse_extended_pictographic_ranges(read_source("emoji-data.txt", data_directory))
    spacing_mark_ranges = parse_code_point_ranges(read_source("DerivedGeneralCategory.txt", data_directory), {"Mc"})

    lines = [
        "// Generated by Scripts/generate_unicode_grapheme_data.py. Do not edit.",
        f"// Unicode {UNICODE_VERSION}; source files are pinned by SHA-256 in the generator.",
        "// Data is used under the Unicode License: https://www.unicode.org/license.txt",
        "",
        "constexpr GraphemePropertyRange GRAPHEME_BREAK_RANGES[] = {",
    ]
    lines.extend(
        f"    {{ 0x{first:04X}, 0x{last:04X}, GraphemeBreakProperty::{property_name} }},"
        for first, last, property_name in grapheme_ranges
    )
    lines.extend(["};", "", "constexpr IndicPropertyRange INDIC_CONJUNCT_BREAK_RANGES[] = {"])
    lines.extend(
        f"    {{ 0x{first:04X}, 0x{last:04X}, IndicConjunctBreakProperty::{property_name} }},"
        for first, last, property_name in indic_ranges
    )
    lines.extend(["};", "", "constexpr CodePointRange EXTENDED_PICTOGRAPHIC_RANGES[] = {"])
    lines.extend(f"    {{ 0x{first:04X}, 0x{last:04X} }}," for first, last in pictographic_ranges)
    lines.extend(["};", "", "constexpr CodePointRange SPACING_MARK_RANGES[] = {"])
    lines.extend(f"    {{ 0x{first:04X}, 0x{last:04X} }}," for first, last in spacing_mark_ranges)
    lines.extend(["};", ""])
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="Fail if the generated table is not current")
    parser.add_argument("--data-dir", type=pathlib.Path, help="Read pinned Unicode source files from this directory")
    arguments = parser.parse_args()
    generated = generate(arguments.data_dir)
    if arguments.check:
        if not OUTPUT_PATH.exists() or OUTPUT_PATH.read_text(encoding="utf-8") != generated:
            print(f"Out of date: {OUTPUT_PATH}", file=sys.stderr)
            return 1
        return 0
    OUTPUT_PATH.write_text(generated, encoding="utf-8", newline="\n")
    print(f"Generated {OUTPUT_PATH.relative_to(REPOSITORY_ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
