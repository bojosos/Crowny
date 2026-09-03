import os
import shutil
import sys
import time
from pathlib import Path

from . import cmd, env, hashing, locks, log, stamps

PROJECT_DIRECTORIES = (
    "Crowny",
    "Crowny-Editor",
    "Crowny-Builder",
    "Crowny-RenderTests",
    "Crowny-Tests",
    "Crowny-Sharp",
    "Crowny-Sandbox",
)


def premake_executable_name():
    return "premake5.exe" if sys.platform == "win32" else "premake5"


def find_premake(root=None):
    root = root or env.repo_root()
    bundled = root / "3rdparty" / "premake" / "bin" / premake_executable_name()
    if bundled.is_file():
        return bundled
    on_path = shutil.which("premake5")
    if on_path:
        return Path(on_path)
    raise RuntimeError(
        "Premake was not found in 3rdparty/premake/bin or PATH. Run `crowny setup` first."
    )


def premake_action():
    return "vs2022" if sys.platform == "win32" else "gmake2"


def premake_flags(simd):
    return ["--with-nodes", f"--simd={simd.lower()}"]


def generation_environment_values():
    values = []
    for name in (
        "VULKAN_SDK",
        "CROWNY_VMA_INCLUDE",
        "CROWNY_OPENAL_ROOT",
        "CROWNY_PHYSICS_ROOT",
        "CROWNY_SPIRV_CROSS_ROOT",
        "CROWNY_MONO_ROOT",
    ):
        value = os.environ.get(name, "").strip()
        if not value:
            values.append(f"{name}=<unset>")
            continue
        values.append(f"{name}={Path(value).resolve().as_posix().lower().rstrip('/')}")
    return values


def project_fingerprint(root=None, simd="avx2"):
    root = root or env.repo_root()
    files = [root / "premake5.lua"]
    for directory in PROJECT_DIRECTORIES:
        files.append(root / directory / "premake5.lua")
    scripts = root / "Scripts"
    if scripts.is_dir():
        files.extend(sorted(scripts.glob("premake*.lua")))
    dependencies = root / "Crowny" / "Dependencies"
    if dependencies.is_dir():
        files.extend(sorted(dependencies.rglob("premake*.lua")))
    files.append(root / "3rdparty" / "premake" / "premake5.lua")

    source_layout = []
    for directory in PROJECT_DIRECTORIES:
        source_root = root / directory / "Source"
        if not source_root.is_dir():
            continue
        for path in source_root.rglob("*"):
            if path.is_file():
                source_layout.append(str(path))

    values = (
        [premake_action(), "with-nodes", simd.lower()]
        + generation_environment_values()
        + sorted(source_layout)
    )
    return hashing.content_hash(files=[str(f) for f in files], values=values)


def ensure_projects(root=None, simd="avx2", force=False):
    root = root or env.repo_root()
    env.configure_default_environment(root)
    action = premake_action()
    stamp_name = f"{action}-projects.json"
    solution_path = root / "Crowny.sln"
    fingerprint = project_fingerprint(root, simd)

    if (
        not force
        and solution_path.is_file()
        and stamps.fingerprint_matches(stamp_name, fingerprint, root)
    ):
        log.info("Generated projects are current.")
        return

    with locks.exclusive_lock(root, "project-generation"), locks.project_write_lock(root):
        fingerprint = project_fingerprint(root, simd)
        if (
            not force
            and solution_path.is_file()
            and stamps.fingerprint_matches(stamp_name, fingerprint, root)
        ):
            log.info("Generated projects are current.")
            return
        log.info(f"Generating the {action} workspace...")
        cmd.run_checked(
            [find_premake(root), action] + premake_flags(simd),
            cwd=root,
        )
        stamps.fingerprint_stamp(
            stamp_name,
            fingerprint,
            root,
            extra={
                "generatedUtc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
                "command": f"premake5 {action} " + " ".join(premake_flags(simd)),
            },
        )
