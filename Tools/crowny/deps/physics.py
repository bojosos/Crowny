import re
import shutil
import sys

from .. import cmd, env, log, stamps
from . import cmake, fetch, git

DEPENDENCIES = [
    {
        "name": "box3d",
        "repository": "https://github.com/erincatto/box3d.git",
        "commit": "8441b4a06d6d09dcfb0b0f704df4d847d1437b92",
        "required": "include/box3d/box3d.h",
    },
    {
        "name": "jolt",
        "repository": "https://github.com/jrouwe/JoltPhysics.git",
        "commit": "e77f175595e64cb44218cc9d9d56fc365ad0e36a",
        "required": "Jolt/Jolt.h",
    },
    {
        "name": "bullet3",
        "repository": "https://github.com/bulletphysics/bullet3.git",
        "commit": "2c204c49e56ed15ec5fcfa71d199ab6d6570b3f5",
        "required": "src/btBulletDynamicsCommon.h",
    },
]

_BOX_STATIC_RUNTIME = 'set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")'
_BOX_DYNAMIC_RUNTIME = 'set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")'


def physics_root(repository_root=None):
    return env.deps_root(repository_root) / "physics"


def _stamp_text():
    lines = [f"{dep['name']}={dep['commit']}" for dep in DEPENDENCIES]
    return "\n".join(lines)


def _required_libraries(install_root):
    if sys.platform == "win32":
        names = ["box3d.lib", "Jolt.lib", "BulletDynamics.lib", "BulletCollision.lib", "LinearMath.lib"]
    else:
        names = ["libbox3d.a", "libJolt.a", "libBulletDynamics.a", "libBulletCollision.a", "libLinearMath.a"]
    return [install_root / "lib" / name for name in names]


def ensure(repository_root=None, configuration="Release", simd="avx2", force=False):
    repository_root = repository_root or env.repo_root()
    root = physics_root(repository_root)
    build_root = root / "build"
    install_root = root / "install" / configuration
    root.mkdir(parents=True, exist_ok=True)
    build_root.mkdir(parents=True, exist_ok=True)
    install_root.mkdir(parents=True, exist_ok=True)

    expected = _stamp_text() + f"\nsimd={simd}-v1"
    stamp = install_root / ".crowny-physics-version"
    required = _required_libraries(install_root)
    if (
        not force
        and stamps.text_stamp_matches(stamp, expected)
        and all(path.is_file() for path in required)
    ):
        log.info(f"Physics dependencies are already built for {configuration}.")
        return install_root

    sources = {}
    for dependency in DEPENDENCIES:
        sources[dependency["name"]] = _ensure_source(root, dependency)

    suffix = cmake.simd_cache_entries(simd, "msvc" if sys.platform == "win32" else "gnu")
    simd_flags = suffix.get("CMAKE_CXX_FLAGS", "")

    _build_box3d(sources["box3d"], build_root, install_root, configuration, simd, simd_flags)
    _build_jolt(sources["jolt"], build_root, install_root, configuration, simd_flags)
    _build_bullet(sources["bullet3"], build_root, install_root, configuration, simd_flags)

    if sys.platform == "win32" and configuration == "Debug":
        for library_name in ("BulletDynamics", "BulletCollision", "LinearMath"):
            debug_library = install_root / "lib" / f"{library_name}_Debug.lib"
            canonical = install_root / "lib" / f"{library_name}.lib"
            if not debug_library.is_file():
                raise RuntimeError(f"Bullet did not install its Debug library: {debug_library}")
            shutil.copyfile(debug_library, canonical)

    missing = [str(path) for path in required if not path.is_file()]
    if missing:
        raise RuntimeError(f"Missing physics libraries after build: {', '.join(missing)}")
    stamps.write_text_stamp(stamp, expected)
    log.info(f"Physics dependencies are ready in {install_root}.")
    return install_root


def _ensure_source(root, dependency):
    name = dependency["name"]
    target = root / name
    commit = git.rev_parse(target)
    if commit == dependency["commit"] and (target / dependency["required"]).is_file():
        return target

    staging = root / f"staging-{name}"
    fetch.remove_tree(staging, guard_root=root)
    log.info(f"Fetching {name} at {dependency['commit']}...")
    git_executable = git.find_git()
    cmd.run_checked(
        [git_executable, "clone", "--filter=blob:none", "--no-checkout", dependency["repository"], str(staging)]
    )
    cmd.run_checked([git_executable, "-C", str(staging), "fetch", "--depth", "1", "origin", dependency["commit"]])
    cmd.run_checked([git_executable, "-C", str(staging), "checkout", "--detach", dependency["commit"]])
    if git.rev_parse(staging) != dependency["commit"] or not (staging / dependency["required"]).is_file():
        raise RuntimeError(f"Dependency validation failed for {name}.")

    fetch.remove_tree(target, guard_root=root)
    staging.rename(target)
    return target


def _patch_box3d_runtime(source_root):
    cmake_lists = source_root / "CMakeLists.txt"
    text = cmake_lists.read_text(encoding="utf-8")
    if _BOX_STATIC_RUNTIME in text:
        cmake_lists.write_text(text.replace(_BOX_STATIC_RUNTIME, _BOX_DYNAMIC_RUNTIME), encoding="utf-8", newline="")
    elif _BOX_DYNAMIC_RUNTIME not in text:
        raise RuntimeError(
            "Box3D's runtime setting changed upstream; update crowny.deps.physics before building."
        )


def _build_box3d(source_root, build_root, install_root, configuration, simd, simd_flags):
    if sys.platform == "win32":
        _patch_box3d_runtime(source_root)
        cache = {
            "BOX3D_SAMPLES": "OFF",
            "BOX3D_UNIT_TESTS": "OFF",
            "BOX3D_BENCHMARKS": "OFF",
            "BOX3D_DOCS": "OFF",
            "BOX3D_VALIDATE": "OFF",
            "BUILD_SHARED_LIBS": "OFF",
        }
        cache.update(cmake.simd_cache_entries(simd, "msvc"))
        build_dir = build_root / "box3d"
        cmake.configure(source_root, build_dir, cache)
        cmake.build(build_dir, configuration=configuration)
        pattern = re.compile(r".*\\%s\\.*box3d.*\.lib$" % re.escape(configuration), re.IGNORECASE)
        candidates = [
            path for path in build_dir.rglob("box3d*.lib") if path.is_file() and pattern.match(str(path))
        ]
        if not candidates:
            raise RuntimeError("The Box3D library was not produced.")
        (install_root / "include" / "box3d").mkdir(parents=True, exist_ok=True)
        (install_root / "lib").mkdir(parents=True, exist_ok=True)
        shutil.copytree(source_root / "include" / "box3d", install_root / "include" / "box3d", dirs_exist_ok=True)
        shutil.copyfile(candidates[0], install_root / "lib" / "box3d.lib")
    else:
        cache = {
            "CMAKE_BUILD_TYPE": configuration,
            "CMAKE_C_FLAGS": simd_flags,
            "CMAKE_CXX_FLAGS": simd_flags,
            "BOX3D_SAMPLES": "OFF",
            "BOX3D_UNIT_TESTS": "OFF",
            "BOX3D_BENCHMARKS": "OFF",
            "BOX3D_DOCS": "OFF",
            "BOX3D_VALIDATE": "OFF",
            "BUILD_SHARED_LIBS": "OFF",
        }
        build_dir = build_root / f"box3d-{configuration.lower()}"
        cmake.configure(source_root, build_dir, cache, multi_config=False)
        cmake.build(build_dir=build_dir)
        candidates = list(build_dir.rglob("libbox3d.a"))
        if not candidates:
            raise RuntimeError("The Box3D library was not produced.")
        (install_root / "include" / "box3d").mkdir(parents=True, exist_ok=True)
        (install_root / "lib").mkdir(parents=True, exist_ok=True)
        shutil.copytree(source_root / "include" / "box3d", install_root / "include" / "box3d", dirs_exist_ok=True)
        shutil.copyfile(candidates[0], install_root / "lib" / "libbox3d.a")


def _jolt_cache(configuration, simd_flags):
    cache = {
        "INTERPROCEDURAL_OPTIMIZATION": "OFF",
        "ENABLE_ALL_WARNINGS": "OFF",
        "ENABLE_OBJECT_STREAM": "OFF",
        "DEBUG_RENDERER_IN_DEBUG_AND_RELEASE": "OFF",
        "PROFILER_IN_DEBUG_AND_RELEASE": "OFF",
        "FLOATING_POINT_EXCEPTIONS_ENABLED": "OFF",
        "USE_SSE4_1": "ON" if simd_flags.startswith("-msse4.1") else "OFF",
        "USE_SSE4_2": "OFF",
        "USE_AVX": "OFF",
        "USE_AVX2": "ON" if "-mavx2" in simd_flags or "/arch:AVX2" in simd_flags else "OFF",
        "USE_AVX512": "OFF",
        "USE_LZCNT": "OFF",
        "USE_TZCNT": "OFF",
        "USE_F16C": "OFF",
        "USE_FMADD": "OFF",
        "JPH_USE_DX12": "OFF",
        "JPH_USE_VK": "OFF",
        "JPH_USE_MTL": "OFF",
        "JPH_USE_CPU_COMPUTE": "OFF",
        "TARGET_UNIT_TESTS": "OFF",
        "TARGET_HELLO_WORLD": "OFF",
        "TARGET_PERFORMANCE_TEST": "OFF",
        "TARGET_SAMPLES": "OFF",
        "TARGET_VIEWER": "OFF",
        "CMAKE_C_FLAGS": simd_flags,
        "CMAKE_CXX_FLAGS": simd_flags,
    }
    if sys.platform == "win32":
        cache["USE_STATIC_MSVC_RUNTIME_LIBRARY"] = "OFF"
        cache["CMAKE_C_FLAGS"] = "/arch:AVX2" if "/arch:AVX2" in simd_flags else ""
        cache["CMAKE_CXX_FLAGS"] = cache["CMAKE_C_FLAGS"]
    else:
        cache["CMAKE_BUILD_TYPE"] = configuration
    return cache


def _build_jolt(source_root, build_root, install_root, configuration, simd_flags):
    cache = _jolt_cache(configuration, simd_flags)
    cache["CMAKE_INSTALL_PREFIX"] = str(install_root)
    build_dir = build_root / ("jolt" if sys.platform == "win32" else f"jolt-{configuration.lower()}")
    cmake.build_and_install(source_root / "Build", build_dir, cache, configuration, install_root)


def _build_bullet(source_root, build_root, install_root, configuration, simd_flags):
    cache = {
        "CMAKE_INSTALL_PREFIX": str(install_root),
        "CMAKE_C_FLAGS": simd_flags,
        "CMAKE_CXX_FLAGS": simd_flags,
        "BUILD_SHARED_LIBS": "OFF",
        "BUILD_BULLET2_DEMOS": "OFF",
        "BUILD_CPU_DEMOS": "OFF",
        "BUILD_OPENGL3_DEMOS": "OFF",
        "BUILD_EXTRAS": "OFF",
        "BUILD_UNIT_TESTS": "OFF",
        "BUILD_PYBULLET": "OFF",
        "INSTALL_LIBS": "ON",
    }
    if sys.platform == "win32":
        cache["CMAKE_POLICY_VERSION_MINIMUM"] = "3.5"
        cache["USE_MSVC_RUNTIME_LIBRARY_DLL"] = "ON"
    else:
        cache["CMAKE_POLICY_VERSION_MINIMUM"] = "3.5"
        cache["CMAKE_BUILD_TYPE"] = configuration
    build_dir = build_root / ("bullet3" if sys.platform == "win32" else f"bullet3-{configuration.lower()}")
    cmake.build_and_install(source_root, build_dir, cache, configuration, install_root)
