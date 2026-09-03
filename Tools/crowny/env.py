import os
import subprocess
import sys
from pathlib import Path

CONFIGURATIONS = ("Debug", "Release", "Dist")
SANITIZERS = ("None", "Address")
SIMD_LEVELS = ("sse4.1", "avx2")


def repo_root():
    override = os.environ.get("CROWNY_REPO_ROOT")
    if override:
        return Path(os.path.abspath(override))
    return Path(os.path.abspath(__file__)).parents[2]


def git_common_root(root=None):
    root = Path(root or repo_root())
    dot_git = root / ".git"
    if dot_git.is_dir():
        return root
    if dot_git.is_file():
        try:
            first_line = dot_git.read_text(encoding="utf-8-sig").splitlines()[0].strip()
        except (OSError, IndexError):
            first_line = ""
        if first_line.startswith("gitdir:"):
            worktree_git_directory = first_line[len("gitdir:"):].strip()
            if not os.path.isabs(worktree_git_directory):
                worktree_git_directory = str(root / worktree_git_directory)
            worktree_admin_root = os.path.dirname(os.path.abspath(worktree_git_directory))
            if os.path.basename(worktree_admin_root) == "worktrees":
                common_directory = os.path.dirname(worktree_admin_root)
                if os.path.basename(common_directory.rstrip("\\/")) == ".git":
                    return Path(os.path.dirname(common_directory.rstrip("\\/")))

    git = os.environ.get("CROWNY_GIT") or "git"
    try:
        result = subprocess.run(
            [git, "-C", str(root), "rev-parse", "--path-format=absolute", "--git-common-dir"],
            capture_output=True,
            text=True,
            check=False,
        )
        if result.returncode == 0:
            common_directory = result.stdout.strip()
            if common_directory:
                if not os.path.isabs(common_directory):
                    common_directory = str(root / common_directory)
                normalized = common_directory.rstrip("\\/")
                if os.path.basename(normalized) == ".git":
                    return Path(os.path.dirname(normalized))
    except OSError:
        pass
    return root


def build_roots(root=None):
    root = Path(root or repo_root())
    common = git_common_root(root)
    dependency_override = os.environ.get("CROWNY_DEPS_ROOT", "")
    coordination_override = os.environ.get("CROWNY_BUILD_COORDINATION_ROOT", "")
    return {
        "repository_root": root,
        "common_repository_root": common,
        "local_dependency_root": root / ".deps",
        "dependency_root": Path(os.path.abspath(dependency_override))
        if dependency_override
        else common / ".deps",
        "coordination_root": Path(os.path.abspath(coordination_override))
        if coordination_override
        else common / ".deps" / "build-coordination",
    }


def deps_root(root=None):
    return build_roots(root)["dependency_root"]


def coordination_root(root=None):
    return build_roots(root)["coordination_root"]


def dependency_path(relative, ready_relative=None, root=None):
    root = root or repo_root()
    roots = build_roots(root)
    if os.environ.get("CROWNY_DEPS_ROOT"):
        return roots["dependency_root"] / relative
    ready = ready_relative or relative
    local_ready = roots["local_dependency_root"] / ready
    if local_ready.exists():
        return roots["local_dependency_root"] / relative
    return roots["dependency_root"] / relative


def downloads_root(root=None):
    return deps_root(root) / "downloads"


def stamps_root(root=None):
    return (root or repo_root()) / ".deps" / "stamps"


def locks_root(root=None):
    return coordination_root(root) / "locks"


def host_platform():
    if sys.platform == "win32":
        return "windows"
    if sys.platform == "darwin":
        return "macos"
    return "linux"


def host_arch():
    machine = os.environ.get("PROCESSOR_ARCHITECTURE", "").upper()
    if machine in ("ARM64", "AARCH64"):
        return "arm64"
    return "x86_64"


def platform_tag():
    return f"{host_platform()}-{host_arch()}"


def validate_configuration(configuration):
    if configuration not in CONFIGURATIONS:
        raise ValueError(f"Unsupported configuration: {configuration}")
    return configuration


def workspace_configuration(configuration, sanitizer):
    validate_configuration(configuration)
    if sanitizer == "Address":
        if configuration == "Dist":
            raise ValueError("Dist does not have an AddressSanitizer configuration.")
        return f"{configuration}ASan"
    if sanitizer not in SANITIZERS:
        raise ValueError(f"Unsupported sanitizer: {sanitizer}")
    return configuration


def output_configuration(configuration, sanitizer):
    validate_configuration(configuration)
    if sanitizer == "Address":
        return f"{configuration}-address"
    return configuration


def build_output_configurations(workspace_config):
    configurations = [workspace_config]
    if workspace_config == "DebugASan":
        configurations.append("Debug")
    elif workspace_config == "ReleaseASan":
        configurations.append("Release")
    return sorted(set(configurations))


def managed_define(configuration):
    return {"Debug": "CW_DEBUG", "Release": "CW_RELEASE", "Dist": "CW_DIST"}[configuration]


def default_mono_root(root=None):
    if host_platform() == "windows":
        return Path(os.environ.get("ProgramFiles", r"C:\Program Files")) / "Mono"
    return Path("/usr")


def configure_default_environment(root=None):
    root = root or repo_root()

    def set_default(name, value):
        if not os.environ.get(name):
            os.environ[name] = str(value)

    set_default("CROWNY_MONO_ROOT", default_mono_root(root))
    set_default(
        "VULKAN_SDK",
        dependency_path("VulkanSDK", "VulkanSDK/Include/vulkan/vulkan.h", root),
    )

    vma_header = None
    if os.environ.get("CROWNY_VMA_INCLUDE"):
        candidate = Path(os.environ["CROWNY_VMA_INCLUDE"]) / "vma" / "vk_mem_alloc.h"
        if candidate.is_file():
            vma_header = candidate
    if vma_header is None:
        vulkan_include = Path(os.environ["VULKAN_SDK"]) / "Include"
        if (vulkan_include / "vma" / "vk_mem_alloc.h").is_file():
            set_default("CROWNY_VMA_INCLUDE", vulkan_include)
        else:
            set_default(
                "CROWNY_VMA_INCLUDE",
                dependency_path("VulkanSDK/Include", "VulkanSDK/Include/vma/vk_mem_alloc.h", root),
            )
    set_default("CROWNY_OPENAL_ROOT", dependency_path("openal", "openal/include/AL/al.h", root))
    set_default("CROWNY_PHYSICS_ROOT", dependency_path("physics/install", "physics/install", root))
    set_default(
        "CROWNY_SPIRV_CROSS_ROOT",
        dependency_path("spirv-cross/install", "spirv-cross/install", root),
    )
