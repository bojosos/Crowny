import os
import shutil
import sys
from pathlib import Path

from . import cmd, env, log
from .deps import fetch
from .premake import find_premake


def _status(ok):
    return "OK     " if ok else "MISSING"


def _check(label, path, required=True):
    path = Path(path)
    ok = path.exists()
    marker = "" if required else " (optional)"
    log.info(f"  [{_status(ok)}] {label}: {path}{marker}")
    return ok or not required


def _check_tool(label, command, hint=""):
    found = shutil.which(command)
    log.info(f"  [{_status(bool(found))}] {label}: {found or 'not found on PATH'}{hint}")
    return bool(found)


def doctor():
    root = env.repo_root()
    log.info(f"Repository root: {root}")
    log.info(f"Host platform: {env.platform_tag()}, Python {sys.version.split()[0]}")
    log.info("Tools:")

    problems = 0
    if not _check_tool("git", "git"):
        problems += 1
    if not _check_tool("cmake", "cmake", hint=" (winget install Kitware.CMake)"):
        problems += 1
    premake = None
    try:
        premake = find_premake(root)
        log.info(f"  [OK     ] premake5: {premake}")
    except RuntimeError as error:
        log.info(f"  [MISSING] premake5: {error}")
        problems += 1

    if sys.platform == "win32":
        vswhere = root / "3rdparty" / "vswhere" / "vswhere.exe"
        if vswhere.is_file():
            try:
                result = cmd.run_checked(
                    [
                        str(vswhere),
                        "-latest",
                        "-products",
                        "*",
                        "-requires",
                        "Microsoft.Component.MSBuild",
                        "-find",
                        "MSBuild\\**\\Bin\\MSBuild.exe",
                    ],
                    capture=True,
                )
                msbuild = result.stdout.strip().splitlines()
                if msbuild:
                    log.info(f"  [OK     ] MSBuild: {msbuild[0]}")
                else:
                    log.info("  [MISSING] MSBuild: VS 2022 Build Tools with C++ required")
                    problems += 1
            except cmd.CommandError:
                log.info("  [MISSING] MSBuild: vswhere query failed")
                problems += 1
        else:
            log.info(f"  [MISSING] vswhere: {vswhere} (initialize submodules)")
            problems += 1
        if not _check_tool("winget", "winget"):
            problems += 1
        try:
            fetch.find_7z()
            log.info("  [OK     ] 7z")
        except RuntimeError:
            log.info("  [MISSING] 7z (winget install 7zip.7zip)")
            problems += 1

    log.info("Dependency roots:")
    dependency = env.deps_root(root)
    checks = [
        ("VULKAN_SDK", Path(os.environ.get("VULKAN_SDK", dependency / "VulkanSDK"))),
        ("CROWNY_MONO_ROOT", Path(env.default_mono_root(root))),
        ("CROWNY_OPENAL_ROOT", dependency / "openal"),
        ("CROWNY_PHYSICS_ROOT", dependency / "physics" / "install"),
        ("CROWNY_SPIRV_CROSS_ROOT", dependency / "spirv-cross" / "install"),
    ]
    for label, path in checks:
        if not _check(label, path, required=False):
            problems += 1

    physics_install = dependency / "physics" / "install"
    spirv_install = dependency / "spirv-cross" / "install"
    for config in ("Debug", "Release"):
        log.info(f"  {config} physics libraries: {_count_libs(physics_install / config / 'lib')} found")
        log.info(f"  {config} spirv-cross libraries: {_count_libs(spirv_install / config / 'lib')} found")

    if problems:
        log.warn(f"{problems} problem(s) found. Run `crowny setup` to bootstrap.")
        return 1
    log.info("All checks passed.")
    return 0


def _count_libs(directory):
    if not directory.is_dir():
        return "0"
    return str(sum(1 for path in directory.iterdir() if path.suffix.lower() in (".lib", ".a")))
