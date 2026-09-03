import sys

from .. import cmd, env, log, stamps
from . import cmake, fetch, git

DEFAULT_VULKAN_VERSION = "1.4.357.0"


def _tag(version):
    return f"vulkan-sdk-{version}"


def ensure(repository_root=None, configuration="Release", version=DEFAULT_VULKAN_VERSION, simd="avx2", force=False):
    repository_root = repository_root or env.repo_root()
    dependency_root = env.deps_root(repository_root) / "spirv-cross"
    source_root = dependency_root / "source"
    build_root = dependency_root / "build" / configuration
    install_root = dependency_root / "install" / configuration
    tag = _tag(version)

    _ensure_source(dependency_root, source_root, tag)

    commit = git.rev_parse(source_root)
    debug_postfix = "d" if configuration == "Debug" else ""
    library_names = [f"spirv-cross-core{debug_postfix}", f"spirv-cross-glsl{debug_postfix}"]
    if sys.platform == "win32":
        required = [install_root / "lib" / f"{name}.lib" for name in library_names]
    else:
        required = [install_root / "lib" / f"lib{name}.a" for name in library_names]

    expected = f"tag={tag}\ncommit={commit}\nconfiguration={configuration}\nruntime=static-v1\nsimd={simd}-v1"
    stamp = install_root / ".crowny-spirv-cross-version"
    if (
        not force
        and stamps.text_stamp_matches(stamp, expected)
        and all(path.is_file() for path in required)
    ):
        log.info(f"SPIRV-Cross is already built for {configuration}.")
        return install_root

    cache = {
        "CMAKE_INSTALL_PREFIX": str(install_root),
        "SPIRV_CROSS_STATIC": "ON",
        "SPIRV_CROSS_SHARED": "OFF",
        "SPIRV_CROSS_CLI": "OFF",
        "SPIRV_CROSS_ENABLE_TESTS": "OFF",
        "SPIRV_CROSS_ENABLE_GLSL": "ON",
        "SPIRV_CROSS_ENABLE_HLSL": "OFF",
        "SPIRV_CROSS_ENABLE_MSL": "OFF",
        "SPIRV_CROSS_ENABLE_CPP": "OFF",
        "SPIRV_CROSS_ENABLE_REFLECT": "OFF",
        "SPIRV_CROSS_ENABLE_C_API": "OFF",
        "SPIRV_CROSS_ENABLE_UTIL": "OFF",
    }
    if sys.platform == "win32":
        cache["CMAKE_DEBUG_POSTFIX"] = "d"
        cache["CMAKE_MSVC_RUNTIME_LIBRARY"] = "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
        cache.update(cmake.simd_cache_entries(simd, "msvc"))
        cmake.configure(source_root, build_root, cache)
        cmake.build(build_root, configuration=configuration, target="install")
    else:
        cache["CMAKE_DEBUG_POSTFIX"] = "d"
        cache["CMAKE_BUILD_TYPE"] = configuration
        cache.update(cmake.simd_cache_entries(simd, "gnu"))
        cmake.configure(source_root, build_root, cache, multi_config=False)
        cmake.build(build_dir=build_root)
        cmake.install(build_root, configuration, install_root)

    missing = [str(path) for path in required if not path.is_file()]
    if missing:
        raise RuntimeError(f"Missing SPIRV-Cross libraries after build: {', '.join(missing)}")
    stamps.write_text_stamp(stamp, expected)
    log.info(f"SPIRV-Cross is ready in {install_root}.")
    return install_root


def _ensure_source(dependency_root, source_root, tag):
    def source_matches():
        if not (source_root / ".git").exists() or not (source_root / "spirv_glsl.cpp").is_file():
            return False
        commit = git.rev_parse(source_root)
        if not commit:
            return False
        try:
            cmd.run_checked(
                [git.find_git(), "-C", str(source_root), "rev-parse", f"{tag}^{{commit}}"],
                capture=True,
            )
        except cmd.CommandError:
            return False
        result = cmd.run_checked(
            [git.find_git(), "-C", str(source_root), "rev-parse", "HEAD"], capture=True
        )
        return result.stdout.strip() == commit

    if source_matches():
        return

    staging = dependency_root / "staging-source"
    fetch.remove_tree(staging, guard_root=dependency_root)
    log.info(f"Fetching SPIRV-Cross {tag}...")
    cmd.run_checked(
        [
            git.find_git(),
            "clone",
            "--depth",
            "1",
            "--branch",
            tag,
            "https://github.com/KhronosGroup/SPIRV-Cross.git",
            str(staging),
        ]
    )
    fetch.remove_tree(source_root, guard_root=dependency_root)
    staging.rename(source_root)
    if not source_matches():
        raise RuntimeError("SPIRV-Cross source validation failed.")
