#!/usr/bin/env python3
"""Count physical and nonblank lines in first-party source, including comments.

Git supplies tracked and non-ignored untracked files. Assets, documentation,
vendored code and generated outputs are excluded. This is not a lexer-based
count of statements or comment-free code. Run with -a to audit every file.
"""

import argparse
from collections import defaultdict
from datetime import datetime
from pathlib import Path
import subprocess


ROOT = Path(__file__).resolve().parents[1]
SOURCE_EXTENSIONS = {
    ".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inl",
    ".cs", ".py", ".lua", ".sh", ".bat", ".cmd", ".ps1",
    ".glsl", ".glslinc", ".vert", ".frag", ".geom", ".comp", ".hlsl",
    ".cmake", ".props", ".targets", ".csproj",
}
EXCLUDED_DIRECTORIES = {
    "dependencies", "3rdparty", "vendor", "thirdparty", "third_party",
    "external", "bin", "bin-int", "obj", "cache", "artifacts",
    ".deps", ".scratch", "__pycache__",
}
GENERATED_FILES = {"Crowny/Source/Crowny/Common/UnicodeGraphemeData.inl"}
SOURCE_NAMES = {"Scripts/crowny", "CMakeLists.txt"}


def is_source(relative):
    path = Path(relative)
    if any(part.lower() in EXCLUDED_DIRECTORIES for part in path.parts[:-1]):
        return False
    # GeneratedMetadataBackend is handwritten, despite its directory name.
    if relative in GENERATED_FILES or path.name.lower().endswith(".g.cs"):
        return False
    return (path.suffix.lower() in SOURCE_EXTENSIONS
            or relative in SOURCE_NAMES or path.name == "CMakeLists.txt")


def source_files(root):
    result = subprocess.run(
        ["git", "-C", str(root), "ls-files", "--cached", "--others",
         "--exclude-standard", "-z"],
        check=True, stdout=subprocess.PIPE,
    )
    for relative in sorted(set(result.stdout.decode("utf-8").split("\0"))):
        path = root / relative
        if relative and is_source(relative) and path.is_file() and not path.is_symlink():
            yield relative


def count_file(path):
    with path.open(encoding="utf-8-sig") as source:
        lines = source.readlines()
    return len(lines), sum(bool(line.strip()) for line in lines), sum(map(len, lines))


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("-a", "--all", action="store_true", help="list every included file")
    parser.add_argument("--nowrite", action="store_true", help="do not append to count.log")
    args = parser.parse_args(argv)
    records = [(name, *count_file(ROOT / name)) for name in source_files(ROOT)]
    groups = defaultdict(lambda: [0, 0, 0])
    for name, lines, nonblank, characters in records:
        group = name.split("/")[0] if "/" in name else "(root)"
        groups[group][0] += 1
        groups[group][1] += lines
        groups[group][2] += nonblank
        if args.all:
            print(f"{name}: {lines} physical lines, {nonblank} nonblank lines")
    print("First-party source; comments included. Dependencies and generated outputs excluded.")
    for group, (files, lines, nonblank) in sorted(groups.items()):
        print(f"{group}: {files} files, {lines} physical lines, {nonblank} nonblank lines")
    lines = sum(record[1] for record in records)
    nonblank = sum(record[2] for record in records)
    characters = sum(record[3] for record in records)
    summary = (f"{lines} physical lines, {nonblank} nonblank lines in {len(records)} files, "
               f"{characters} characters")
    print(summary)
    if records:
        largest = max(records, key=lambda record: record[1])
        print(f"Largest file by lines: {largest[0]} ({largest[1]} lines)")
    if not args.nowrite:
        with (ROOT / "Scripts/count.log").open("a", encoding="utf-8") as output:
            output.write(datetime.now().strftime("%d/%m/%Y %H:%M:%S") + "\n")
            output.write(summary + "\n")


if __name__ == "__main__":
    main()
