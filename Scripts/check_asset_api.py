#!/usr/bin/env python3
"""Reject runtime asset reads that bypass Crowny's asset and packed-resource APIs."""

from __future__ import annotations

import argparse
import fnmatch
import re
import sys
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class Rule:
    name: str
    pattern: re.Pattern[str]
    guidance: str


@dataclass(frozen=True)
class Allow:
    path: str
    rules: frozenset[str]
    reason: str
    contains: str | None = None


RULES = (
    Rule(
        "direct-import",
        re.compile(r"\bImporter::Get\(\)\.Import(?:All|Deferred)?(?:<[^>]+>)?\s*\("),
        "Load runtime assets through AssetManager/ProjectLibrary; import only at source-ingestion boundaries.",
    ),
    Rule(
        "raw-read",
        re.compile(r"\bFileSystem::(?:OpenFile|ReadFile|ReadTextFile|ReadBinaryFile)\s*\("),
        "Use AssetManager/ProjectLibrary, or centralize non-asset built-ins behind a pack-aware resource API.",
    ),
    Rule(
        "physical-stream",
        re.compile(r"\bstd::(?:ifstream|ofstream|fstream)\b|\bfopen\s*\("),
        "Use FileSystem/DataStream so built-in packs and platform behavior remain available.",
    ),
    Rule(
        "physical-metadata",
        re.compile(r"\b(?:std::filesystem|fs)::(?:file_size|last_write_time)\s*\("),
        "Use FileSystem metadata APIs or the already-open DataStream.",
    ),
    Rule(
        "raw-builtin-source",
        re.compile(
            r"[\"']Resources[/\\][^\"']+\.(?:glsl|glslinc|png|jpe?g|hdr|fbx|obj|ttf|cs|dll|pdb)[\"']",
            re.IGNORECASE,
        ),
        "Reference a cooked .asset, UUID, or a resource exposed by the built-in pack catalog.",
    ),
)


# These are boundaries, not general-purpose exemptions. Add an exact path and the
# smallest applicable rule set when a new boundary is unavoidable.
ALLOWLIST = (
    Allow("Crowny/Source/Crowny/Import/**", frozenset({"direct-import", "raw-read"}), "source importer boundary"),
    Allow("Crowny/Source/Crowny/Assets/AssetManager.cpp", frozenset({"raw-read"}), "asset residency I/O boundary"),
    Allow("Crowny/Source/Crowny/Assets/AssetCodecs.cpp", frozenset({"raw-read", "physical-metadata"}), "domain asset serialization boundary"),
    Allow("Crowny/Source/Crowny/Common/DataStream.*", frozenset({"physical-stream"}), "platform stream implementation"),
    Allow("Crowny/Source/Crowny/Common/BuiltInResourcePack.cpp", frozenset({"physical-stream", "physical-metadata"}), "built-in pack implementation"),
    Allow("Crowny/Source/Crowny/Common/VirtualFileSystem.cpp", frozenset({"raw-read"}), "virtual filesystem implementation"),
    Allow("Crowny/Source/Crowny/Utils/BuiltInShaderCompiler.cpp", frozenset({"raw-read", "physical-stream", "physical-metadata", "raw-builtin-source"}), "build-time shader compiler"),
    Allow("Crowny/Source/Crowny/Utils/ShaderCompiler.cpp", frozenset({"raw-read"}), "shader include compiler boundary"),
    Allow("Crowny/Source/Crowny/Serialization/SceneSerializer.cpp", frozenset({"raw-read"}), "scene serialization boundary"),
    Allow("Crowny/Source/Crowny/Serialization/PrefabSerializer.cpp", frozenset({"raw-read"}), "prefab serialization boundary"),
    Allow("Crowny/Source/Crowny/Serialization/NodeGraphSerializer.cpp", frozenset({"raw-read"}), "node-graph serialization boundary"),
    Allow("Crowny/Source/Crowny/Serialization/MaterialSerializer.cpp", frozenset({"raw-read"}), "material serialization boundary"),
    Allow("Crowny/Source/Crowny/Serialization/FileEncoder.h", frozenset({"raw-read"}), "file conversion utility boundary"),
    Allow("Crowny/Source/Crowny/ImGui/ImGuiLayer.cpp", frozenset({"raw-read", "raw-builtin-source"}), "font-atlas input is opened through the built-in pack"),
    Allow("Crowny/Source/Crowny/Scripting/Mono/MonoAssembly.cpp", frozenset({"raw-read"}), "managed assembly loader boundary"),
    Allow("Crowny/Source/Crowny/Scripting/Mono/MonoManager.cpp", frozenset({"raw-read"}), "managed runtime image boundary"),
    Allow("Crowny/Source/Crowny/Scene/ScriptRuntime.cpp", frozenset({"raw-builtin-source"}), "legacy managed assembly path; scripting lane owns migration"),
    Allow("Crowny/Source/Crowny/Scene/SceneRenderer.cpp", frozenset({"direct-import", "raw-builtin-source"}), "compile-time-disabled legacy ray tracing prototype", "RayTrace.glsl"),
    Allow("Crowny-Editor/Source/Editor/ProjectLibrary.cpp", frozenset({"direct-import", "raw-read", "physical-metadata"}), "project asset catalog and import boundary"),
    Allow("Crowny-Editor/Source/Editor/AssetLibraryServices.cpp", frozenset({"direct-import", "raw-read"}), "project metadata and import scheduling boundary"),
    Allow("Crowny-Editor/Source/Editor/EditorBuiltInAssetCompiler.cpp", frozenset({"direct-import", "raw-read", "physical-stream", "physical-metadata", "raw-builtin-source"}), "editor build tooling"),
    Allow("Crowny-Editor/Source/Editor/EditorAssets.cpp", frozenset({"raw-read", "raw-builtin-source"}), "central built-in resource catalog using pack-aware FileSystem"),
    Allow("Crowny-Editor/Source/Editor/AssetPreviewService.cpp", frozenset({"raw-read"}), "source preview decode boundary"),
    Allow("Crowny-Editor/Source/Panels/ComponentRenderer.cpp", frozenset({"direct-import"}), "explicit user file-dialog import"),
)


def strip_comments(source: str) -> str:
    """Remove C/C++ comments while preserving strings and line numbers."""
    output: list[str] = []
    index = 0
    block_comment = False
    quote = ""
    escaped = False
    while index < len(source):
        char = source[index]
        following = source[index + 1] if index + 1 < len(source) else ""
        if block_comment:
            if char == "*" and following == "/":
                output.extend("  ")
                index += 2
                block_comment = False
            else:
                output.append("\n" if char == "\n" else " ")
                index += 1
            continue
        if quote:
            output.append(char)
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == quote:
                quote = ""
            index += 1
            continue
        if char in {'"', "'"}:
            quote = char
            output.append(char)
            index += 1
        elif char == "/" and following == "/":
            while index < len(source) and source[index] != "\n":
                output.append(" ")
                index += 1
        elif char == "/" and following == "*":
            output.extend("  ")
            index += 2
            block_comment = True
        else:
            output.append(char)
            index += 1
    return "".join(output)


def is_allowed(relative_path: str, rule: str, line: str) -> bool:
    return any(
        rule in entry.rules
        and fnmatch.fnmatchcase(relative_path, entry.path)
        and (entry.contains is None or entry.contains in line)
        for entry in ALLOWLIST
    )


def scan(repo_root: Path) -> list[tuple[str, int, Rule, str]]:
    violations: list[tuple[str, int, Rule, str]] = []
    roots = (repo_root / "Crowny/Source/Crowny", repo_root / "Crowny-Editor/Source")
    for root in roots:
        for path in sorted(candidate for candidate in root.rglob("*") if candidate.suffix in {".cpp", ".cc", ".h", ".hpp"}):
            relative_path = path.relative_to(repo_root).as_posix()
            source = strip_comments(path.read_text(encoding="utf-8", errors="replace"))
            for line_number, line in enumerate(source.splitlines(), 1):
                for rule in RULES:
                    if rule.pattern.search(line) and not is_allowed(relative_path, rule.name, line):
                        violations.append((relative_path, line_number, rule, line.strip()))
    return violations


def self_test() -> None:
    rules = {rule.name: rule for rule in RULES}
    assert rules["physical-metadata"].pattern.search("std::filesystem::file_size(path)")
    assert rules["direct-import"].pattern.search('Importer::Get().Import<Texture>("source.png")')
    assert not is_allowed("Crowny/Source/Crowny/Import/ImageLoader.cpp", "physical-metadata", "std::filesystem::file_size(path)")
    assert is_allowed("Crowny/Source/Crowny/Import/ImageLoader.cpp", "raw-read", "FileSystem::OpenFile(path)")
    assert is_allowed("Crowny-Editor/Source/Editor/ProjectLibrary.cpp", "physical-metadata", "fs::file_size(path)")
    assert is_allowed("Crowny/Source/Crowny/Scene/SceneRenderer.cpp", "direct-import", "Importer::Get().Import(\"RayTrace.glsl\")")
    assert not is_allowed("Crowny/Source/Crowny/Scene/SceneRenderer.cpp", "direct-import", "Importer::Get().Import(path)")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--self-test", action="store_true", help="run checker regression assertions before scanning")
    args = parser.parse_args()
    if args.self_test:
        self_test()

    repo_root = args.repo_root.resolve()
    violations = scan(repo_root)
    if not violations:
        print("Asset API check passed.")
        return 0

    print("Runtime asset access bypasses detected:", file=sys.stderr)
    for path, line_number, rule, source in violations:
        print(f"{path}:{line_number}: [{rule.name}] {source}", file=sys.stderr)
        print(f"  {rule.guidance}", file=sys.stderr)
    print(f"Found {len(violations)} violation(s). Keep exceptions exact and documented in ALLOWLIST.", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
