import sys

from .. import env, log, stamps
from . import cmake, git

RUNTIME_SCHEMA = "dynamic-v1"
SIMD_SCHEMA = "-v1"


def openal_root(repository_root=None):
    return env.deps_root(repository_root) / "openal"


def _stamp_text(commit, configuration, simd):
    return (
        f"commit={commit}\n"
        f"configuration={configuration}\n"
        f"runtime={RUNTIME_SCHEMA}\n"
        f"simd={simd}{SIMD_SCHEMA}"
    )


def _artifacts(root):
    header = root / "include" / "AL" / "al.h"
    if sys.platform == "win32":
        return [root / "lib" / "OpenAL32.lib", root / "bin" / "OpenAL32.dll", header]
    library = root / "lib"
    candidates = []
    if library.is_dir():
        candidates = [path for path in library.iterdir() if path.name.startswith("libopenal")]
    return candidates + [header]


def ensure(repository_root=None, configuration="Release", simd="avx2", force=False):
    repository_root = repository_root or env.repo_root()
    source_root = repository_root / "Crowny" / "Dependencies" / "openal-soft"
    if not (source_root / "CMakeLists.txt").is_file():
        raise RuntimeError(
            "The OpenAL Soft submodule is missing. Run: git submodule update --init --recursive"
        )

    root = openal_root(repository_root)
    commit = git.rev_parse(source_root)
    if not commit:
        raise RuntimeError("Could not identify the OpenAL Soft source revision.")

    stamp = root / ".crowny-openal-version"
    expected = _stamp_text(commit, configuration, simd)
    if not force and stamps.text_stamp_matches(stamp, expected) and all(
        path.exists() for path in _artifacts(root)
    ):
        log.info(f"OpenAL Soft is already built for {configuration} with {simd}.")
        return root

    build_root = root / "build"
    cache = {
        "CMAKE_INSTALL_PREFIX": str(root),
        "LIBTYPE": "SHARED",
        "ALSOFT_UTILS": "OFF",
        "ALSOFT_EXAMPLES": "OFF",
        "ALSOFT_NO_CONFIG_UTIL": "ON",
        "ALSOFT_INSTALL_CONFIG": "OFF",
        "ALSOFT_INSTALL_HRTF_DATA": "OFF",
        "ALSOFT_INSTALL_AMBDEC_PRESETS": "OFF",
        "ALSOFT_REQUIRE_SSE4_1": "ON",
    }
    if sys.platform == "win32":
        cache["CMAKE_POLICY_VERSION_MINIMUM"] = "3.5"
        cache["CMAKE_MSVC_RUNTIME_LIBRARY"] = "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
        cache.update(cmake.simd_cache_entries(simd, "msvc"))
        cmake.configure(source_root, build_root, cache)
        cmake.build(build_root, configuration=configuration, target="install")
    else:
        cache["CMAKE_BUILD_TYPE"] = configuration
        cache.update(cmake.simd_cache_entries(simd, "gnu"))
        cmake.configure(source_root, build_root, cache, multi_config=False)
        cmake.build(build_dir=build_root)
        cmake.install(build_root, configuration, root)

    missing = [str(path) for path in _artifacts(root) if not path.exists()]
    if missing:
        raise RuntimeError(f"Missing OpenAL Soft files after build: {', '.join(missing)}")

    stamps.write_text_stamp(stamp, expected)
    log.info(f"OpenAL Soft is ready in {root}.")
    return root
