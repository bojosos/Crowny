import os
import shutil
import sys
from pathlib import Path

from . import cmd, env, log

_REQUIRED_SOURCES = [
    "Icons/Play.png",
    "Icons/Pause.png",
    "Icons/Stop.png",
    "Icons/File.png",
    "Icons/Folder.png",
    "Icons/ArrowPointerIcon.png",
    "Icons/ArrowsIcon.png",
    "Icons/RotateIcon.png",
    "Icons/MaximizeIcon.png",
    "Icons/GlobeIcon.png",
    "Icons/SearchIcon.png",
    "Icons/ConsoleInfo.png",
    "Icons/ConsoleWarn.png",
    "Icons/ConsoleError.png",
    "Icons/AlignLeft.png",
    "Icons/AlignCenter.png",
    "Icons/AlignRight.png",
    "Fonts/Roboto/roboto-thin.ttf",
]


def _asset_for(source):
    if source.suffix.lower() == ".ttf":
        return Path(str(source) + ".asset")
    return source.with_suffix(".asset")


def _builtins_need_cooking(root):
    resource_root = root / "Crowny-Editor" / "Resources"
    sources = [resource_root / relative for relative in _REQUIRED_SOURCES]
    shaders = resource_root / "Shaders"
    if shaders.is_dir():
        for shader in sorted(shaders.glob("*.glsl")):
            if "#lang" in shader.read_text(encoding="utf-8", errors="ignore"):
                sources.append(shader)
    for source in sources:
        asset = _asset_for(source)
        if not asset.is_file():
            return True
        if source.stat().st_mtime > asset.stat().st_mtime:
            return True
    return False


def editor_executable(root, output_configuration):
    editor_output = root / "bin" / f"{output_configuration}-{env.platform_tag()}" / "Crowny-Editor"
    executable = editor_output / ("Crowny-Editor.exe" if sys.platform == "win32" else "Crowny-Editor")
    if not executable.is_file():
        raise RuntimeError(f"Editor executable was not found: {executable}")
    return executable


def update(root=None, configuration="Release", sanitizer="None"):
    root = root or env.repo_root()
    output_configuration = env.output_configuration(configuration, sanitizer)
    executable = editor_executable(root, output_configuration)

    original_asan = os.environ.get("ASAN_OPTIONS")
    try:
        if _builtins_need_cooking(root):
            log.info("Cooking changed editor built-ins...")
            if sanitizer == "Address":
                os.environ["ASAN_OPTIONS"] = "abort_on_error=1:halt_on_error=1:strict_string_checks=1"
            cmd.run_checked([str(executable), "--cook-builtins"], cwd=root / "Crowny-Editor")
        else:
            log.info("Cooked editor built-ins are current.")

        pack_script = root / "Scripts" / "pack-builtins.py"
        cmd.run_checked(
            [
                sys.executable,
                str(pack_script),
                "--repo-root",
                str(root),
                "--configuration",
                configuration,
            ]
        )
    finally:
        if sanitizer == "Address":
            if original_asan is None:
                os.environ.pop("ASAN_OPTIONS", None)
            else:
                os.environ["ASAN_OPTIONS"] = original_asan

    editor_output = executable.parent
    resource_output = editor_output / "Resources"
    resource_output.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(
        root / "Crowny-Editor" / "Resources" / "Builtin.cwpack",
        resource_output / "Builtin.cwpack",
    )
