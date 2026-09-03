import sys

from . import cmd, env, log
from .deps import git, openal, physics, spirvcross, vulkan, winget
from .premake import ensure_projects


def update_submodules(root):
    git_executable = git.find_git()
    log.info("Initializing Git submodules...")
    cmd.run_checked([git_executable, "submodule", "sync", "--recursive"], cwd=root)
    cmd.run_checked([git_executable, "submodule", "update", "--init", "--recursive"], cwd=root)


def setup(
    root=None,
    build=False,
    test=False,
    coreclr=False,
    configuration="Release",
    sanitizer="None",
    simd="avx2",
    vulkan_version=vulkan.DEFAULT_VERSION,
):
    root = root or env.repo_root()
    if test and not build:
        raise RuntimeError("--test requires --build.")
    env.validate_configuration(configuration)

    update_submodules(root)

    if coreclr:
        from .deps import dotnet

        dotnet.ensure(root)

    if sys.platform == "win32":
        for package_id in winget.PACKAGES.values():
            winget.ensure_package(package_id)

    vulkan.ensure(root, version=vulkan_version)

    physics_configuration = "Debug" if configuration == "Debug" else "Release"
    openal.ensure(root, configuration=physics_configuration, simd=simd)
    physics.ensure(root, configuration=physics_configuration, simd=simd)
    spirvcross.ensure(root, configuration=physics_configuration, version=vulkan_version, simd=simd)

    _verify_install(root, physics_configuration)
    env.configure_default_environment(root)
    ensure_projects(root, simd=simd, force=True)

    if build:
        from . import build as build_module
        from . import catch2

        if sanitizer == "None":
            build_module.build(
                root=root, target="Editor", configuration=configuration, sanitizer=sanitizer, simd=simd
            )

        if test:
            catch2.run(root=root, configuration=configuration, sanitizer=sanitizer, simd=simd)
        else:
            build_module.build(
                root=root, target="Tests", configuration=configuration, sanitizer=sanitizer, simd=simd
            )

    log.info("Crowny setup completed successfully.")


def _verify_install(root, physics_configuration):
    if sys.platform != "win32":
        return
    debug_postfix = "d" if physics_configuration == "Debug" else ""
    dependency = env.deps_root(root)
    mono_root = env.default_mono_root(root)
    required = [
        mono_root / "include" / "mono-2.0" / "mono" / "jit" / "jit.h",
        mono_root / "lib" / "mono-2.0-sgen.lib",
        dependency / "VulkanSDK" / "Include" / "vulkan" / "vulkan.h",
        dependency / "VulkanSDK" / "Include" / "vma" / "vk_mem_alloc.h",
        dependency / "VulkanSDK" / "Lib" / "vulkan-1.lib",
        dependency / "openal" / "lib" / "OpenAL32.lib",
        dependency / "openal" / "bin" / "OpenAL32.dll",
        dependency / "spirv-cross" / "install" / physics_configuration / "lib" / f"spirv-cross-core{debug_postfix}.lib",
        dependency / "spirv-cross" / "install" / physics_configuration / "lib" / f"spirv-cross-glsl{debug_postfix}.lib",
    ]
    missing = [str(path) for path in required if not path.exists()]
    if missing:
        raise RuntimeError("Missing dependency files: " + ", ".join(missing))
