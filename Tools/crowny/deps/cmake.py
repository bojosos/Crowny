import os
import shutil
import sys
from pathlib import Path

from .. import cmd

_LINUX_AVX2 = ("-mavx2", "-mbmi", "-mpopcnt", "-mlzcnt", "-mf16c")
_LINUX_SSE41 = ("-msse4.1",)


def find_cmake():
    candidate = shutil.which("cmake")
    if candidate:
        return candidate
    if sys.platform == "win32":
        program_files = os.environ.get("ProgramFiles", r"C:\Program Files")
        candidates = [
            Path(program_files) / "CMake" / "bin" / "cmake.exe",
            Path(program_files)
            / "Microsoft Visual Studio"
            / "2022"
            / "BuildTools"
            / "Common7"
            / "IDE"
            / "CommonExtensions"
            / "Microsoft"
            / "CMake"
            / "CMake"
            / "bin"
            / "cmake.exe",
            Path(program_files)
            / "Microsoft Visual Studio"
            / "2022"
            / "Community"
            / "Common7"
            / "IDE"
            / "CommonExtensions"
            / "Microsoft"
            / "CMake"
            / "CMake"
            / "bin"
            / "cmake.exe",
        ]
        for bundled in candidates:
            if bundled.is_file():
                return str(bundled)
    raise RuntimeError("CMake 3.22 or newer is required.")


def simd_cache_entries(simd, compiler):
    if simd == "avx2":
        flags = ["/arch:AVX2"] if compiler == "msvc" else list(_LINUX_AVX2)
    elif simd == "sse4.1":
        flags = ["/arch:SSE2"] if compiler == "msvc" else list(_LINUX_SSE41)
    else:
        raise ValueError(f"Unsupported SIMD level: {simd}")
    joined = " ".join(flags)
    return {"CMAKE_C_FLAGS": joined, "CMAKE_CXX_FLAGS": joined}


def _configure_cache(cache):
    arguments = []
    for key, value in cache.items():
        arguments.append(f"-D{key}={value}")
    return arguments


def configure(source, build, cache=None, multi_config=None):
    source = Path(source)
    build = Path(build)
    build.mkdir(parents=True, exist_ok=True)
    arguments = [find_cmake(), "-S", str(source), "-B", str(build)]
    if multi_config is None:
        multi_config = sys.platform == "win32"
    if multi_config:
        arguments += ["-A", "x64"]
    else:
        build_type = cache.get("CMAKE_BUILD_TYPE") if cache else None
        if not build_type:
            raise RuntimeError("Single-configuration generators need CMAKE_BUILD_TYPE.")
    if cache:
        arguments += _configure_cache(cache)
    cmd.run_checked(arguments)


def build(build_dir, configuration=None, target=None):
    arguments = [find_cmake(), "--build", str(build_dir), "--parallel"]
    if configuration:
        arguments += ["--config", configuration]
    if target:
        arguments += ["--target", target]
    cmd.run_checked(arguments)


def install(build_dir, configuration, prefix):
    cmd.run_checked(
        [
            find_cmake(),
            "--install",
            str(build_dir),
            "--config",
            configuration,
            "--prefix",
            str(prefix),
        ]
    )


def build_and_install(source, build_dir, cache, configuration, prefix):
    configure(source, build_dir, cache)
    if sys.platform == "win32":
        build(build_dir, configuration=configuration, target="install")
    else:
        build(build_dir)
        install(build_dir, configuration, prefix)
