import os
import sys
from pathlib import Path

from .. import env, log
from . import fetch

DEFAULT_VERSION = "1.4.357.0"
PINNED_INSTALLER_SHA256 = "81F474711E9042F4CD22B31B2F7A8870DB2E428B21586FB43DD80150BE97310D"
VMA_SHA256 = "8487B7995AD3B263EB73BC5B9A77D71AA69B6BEF5D58A715C02D2663AFD81F1A"
VMA_URL = (
    "https://raw.githubusercontent.com/GPUOpen-LibrariesAndSDKs/"
    "VulkanMemoryAllocator/v3.4.0/include/vk_mem_alloc.h"
)


def vulkan_root(root=None):
    return env.deps_root(root) / "VulkanSDK"


def _required_files(root):
    runtime_dir = "Lib" if sys.platform == "win32" else "lib"
    library = "vulkan-1.lib" if sys.platform == "win32" else "libvulkan.so.1"
    return [
        root / "Include" / "vulkan" / "vulkan.h",
        root / "Include" / "vma" / "vk_mem_alloc.h",
        root / runtime_dir / library,
    ]


def ensure(repository_root=None, version=DEFAULT_VERSION):
    repository_root = repository_root or env.repo_root()
    root = vulkan_root(repository_root)
    if all(path.is_file() for path in _required_files(root)):
        log.info(f"Vulkan SDK {version} is already installed in {root}.")
        os.environ["VULKAN_SDK"] = str(root)
        return root

    if sys.platform != "win32":
        system_sdk = os.environ.get("VULKAN_SDK", "")
        if system_sdk and (Path(system_sdk) / "Include" / "vulkan" / "vulkan.h").is_file():
            log.info(f"Using system Vulkan SDK at {system_sdk}.")
            return Path(system_sdk)
        raise RuntimeError(
            "No Vulkan SDK found. Install it with your system package manager "
            "or point VULKAN_SDK at an existing installation."
        )

    download_root = env.downloads_root(repository_root)
    download_root.mkdir(parents=True, exist_ok=True)
    installer = download_root / f"vulkansdk-windows-X64-{version}.exe"
    if not installer.exists():
        url = f"https://sdk.lunarg.com/sdk/download/{version}/windows/vulkansdk-windows-X64-{version}.exe"
        log.info(f"Downloading Vulkan SDK {version}...")
        fetch.download(url, installer, sha256=PINNED_INSTALLER_SHA256 if version == DEFAULT_VERSION else None)

    _extract_installer(repository_root, installer, root)
    _install_vma(root)

    missing = [str(path) for path in _required_files(root) if not path.is_file()]
    if missing:
        raise RuntimeError(f"Vulkan SDK setup is missing: {', '.join(missing)}")

    os.environ["VULKAN_SDK"] = str(root)
    log.info(f"Vulkan SDK {version} is ready at {root}.")
    return root


def _extract_installer(repository_root, installer, root):
    dependency_root = env.deps_root(repository_root)
    container_root = dependency_root / "vulkan-package"
    stream_root = container_root / "streams"
    fetch.remove_tree(container_root, guard_root=dependency_root)
    container_root.mkdir(parents=True, exist_ok=True)
    stream_root.mkdir(parents=True, exist_ok=True)
    try:
        fetch.extract_7z(installer, container_root, switches=["-tPE"], masks=["[0]"])
        payload = container_root / "[0]"
        fetch.extract_7z(payload, stream_root, switches=["-t#"])
        root.mkdir(parents=True, exist_ok=True)
        for stream in sorted(stream_root.glob("*.7z")):
            fetch.extract_7z(stream, root)
    finally:
        fetch.remove_tree(container_root, guard_root=dependency_root)


def _install_vma(root):
    vma_header = root / "Include" / "vma" / "vk_mem_alloc.h"
    if vma_header.is_file() and fetch.sha256_file(vma_header) == VMA_SHA256:
        return
    vma_header.parent.mkdir(parents=True, exist_ok=True)
    fetch.download(VMA_URL, vma_header, sha256=VMA_SHA256)
