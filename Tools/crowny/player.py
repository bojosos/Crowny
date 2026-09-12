"""Stage the native player and its redistributable runtime beside editor outputs."""

import os
import shutil
import sys
from pathlib import Path

from . import env, log, msbuild, resources


def stage_template(root, configuration, output_configuration):
    if configuration == "Debug":
        log.info("Build a Release player for distribution; the Debug CRT is not redistributable.")
        return
    if sys.platform != "win32":
        log.info("Automatic player runtime staging currently supports Windows x64.")
        return
    family = root / "bin" / f"{output_configuration}-{env.platform_tag()}"
    source = family / "Crowny-Player"
    template = family / "PlayerTemplate"
    mono = Path(os.environ.get("CROWNY_MONO_ROOT") or env.default_mono_root(root))
    required = [source / "Crowny-Player.exe", mono / "lib/mono/4.5/mscorlib.dll",
                root / "Crowny-Editor/Resources/Builtin.cwpack",
                root / ".deps/generated/managed" / configuration / "CrownySharp.dll"]
    for path in required:
        if not path.is_file():
            raise RuntimeError(f"Player runtime input is missing: {path}")
    if not (mono / "lib/mono/gac").is_dir():
        raise RuntimeError(f"Mono runtime assembly cache is missing: {mono / 'lib/mono/gac'}")
    if resources._builtins_need_cooking(root):
        raise RuntimeError("Player rendering resources are incomplete or stale. Build the Release editor to refresh them before exporting a game.")

    # Ship the release CRT beside the executable. Debug CRTs are not redistributable.
    compiler = msbuild.find_msvc_compiler(root)
    vc = next((parent for parent in compiler.parents if parent.name == "VC"), None)
    crt_dirs = sorted((vc / "Redist/MSVC").glob("*/x64/Microsoft.VC*.CRT")) if vc else []
    if not crt_dirs:
        raise RuntimeError("The Visual C++ redistributable DLL directory was not found.")

    def copy(source_path, destination):
        destination.parent.mkdir(parents=True, exist_ok=True)
        if not destination.is_file() or source_path.stat().st_mtime_ns != destination.stat().st_mtime_ns or source_path.stat().st_size != destination.stat().st_size:
            shutil.copy2(source_path, destination)

    copy(required[0], template / "Game.exe")
    for dll in source.glob("*.dll"):
        copy(dll, template / dll.name)
    for dll in (mono / "bin").glob("*.dll"):
        copy(dll, template / dll.name)
    copy(required[2], template / "Resources/Builtin.cwpack")
    copy(required[3], template / "Managed/CrownySharp.dll")
    for relative in ("lib/mono/4.5", "lib/mono/gac", "etc/mono"):
        for path in (mono / relative).rglob("*"):
            if path.is_file():
                copy(path, template / "Mono" / path.relative_to(mono))
    for dll in crt_dirs[-1].glob("*.dll"):
        copy(dll, template / dll.name)
    log.info(f"Standalone player template: {template}")
