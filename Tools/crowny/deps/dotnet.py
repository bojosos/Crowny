import json
import subprocess
import sys
from pathlib import Path

from .. import cmd, env, log
from . import fetch

PINNED_INSTALLER_SHA256 = {
    "dotnet-install.ps1": "E8B873E18A81E5C4CD8AB69D84DAC8FEAD291D50B3C44633CD7FDDAD709A13D6",
}


def _platform_prefix():
    if sys.platform == "win32":
        return "win"
    if sys.platform == "darwin":
        return "osx"
    return "linux"


def _installer_name():
    return "dotnet-install.ps1" if sys.platform == "win32" else "dotnet-install.sh"


def _pinned_sha256(installer_name):
    return PINNED_INSTALLER_SHA256.get(installer_name)


def ensure(repository_root=None, version="", architecture="x64", install_directory=""):
    repository_root = repository_root or env.repo_root()
    global_json_path = repository_root / "global.json"
    if not global_json_path.is_file():
        raise RuntimeError(f"The repository SDK pin is missing: {global_json_path}")
    pinned_version = json.loads(global_json_path.read_text(encoding="utf-8"))["sdk"]["version"]
    if not version:
        version = pinned_version
    elif version != pinned_version:
        raise RuntimeError(f"Requested .NET SDK {version} does not match global.json ({pinned_version}).")

    if not install_directory:
        install_name = "dotnet" if architecture == "x64" else f"dotnet-{architecture}"
        install_root = env.deps_root(repository_root) / install_name
    else:
        install_root = Path(install_directory).resolve()

    dotnet_name = "dotnet.exe" if sys.platform == "win32" else "dotnet"
    dotnet = install_root / dotnet_name
    expected_rid = f"{_platform_prefix()}-{architecture}"

    if _installed_sdk_matches(dotnet, version, install_root, expected_rid):
        _report_runtime_versions(dotnet, install_root)
        return dotnet

    download_root = env.downloads_root(repository_root)
    download_root.mkdir(parents=True, exist_ok=True)
    installer = download_root / _installer_name()
    pinned = _pinned_sha256(installer.name)
    if installer.is_file() and pinned:
        if fetch.sha256_file(installer) != pinned:
            raise RuntimeError(
                f"The cached {_installer_name()} does not match the repository-pinned checksum. "
                f"Remove {installer} and review the new installer before updating the pin."
            )
    else:
        _download_installer(installer)

    log.info(f"Installing .NET SDK {version} ({architecture}) into {install_root}...")
    if sys.platform == "win32":
        cmd.run_checked(
            [
                "powershell",
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                str(installer),
                "-Version",
                version,
                "-InstallDir",
                str(install_root),
                "-Architecture",
                architecture,
                "-NoPath",
            ]
        )
    else:
        installer.chmod(installer.stat().st_mode | 0o111)
        cmd.run_checked(
            [
                str(installer),
                "--version",
                version,
                "--install-dir",
                str(install_root),
                "--architecture",
                architecture,
                "--no-path",
            ]
        )

    if not _installed_sdk_matches(dotnet, version, install_root, expected_rid):
        raise RuntimeError(f"The repository-local .NET SDK did not install correctly at {dotnet}.")
    _report_runtime_versions(dotnet, install_root)
    return dotnet


def _download_installer(installer):
    url = f"https://dot.net/v1/{installer.name}"
    fetch.download(url, installer, sha256=_pinned_sha256(installer.name))


def _installed_sdk_matches(dotnet, version, install_root, expected_rid):
    if not dotnet.is_file():
        return False
    result = subprocess.run(
        [str(dotnet), "--version"], capture_output=True, text=True, check=False
    )
    if result.returncode != 0 or result.stdout.strip() != version:
        return False
    version_metadata = install_root / "sdk" / version / ".version"
    if not version_metadata.is_file():
        return False
    return expected_rid in version_metadata.read_text(encoding="utf-8", errors="ignore").split()


def _report_runtime_versions(dotnet, install_root):
    runtime_root = install_root / "shared" / "Microsoft.NETCore.App"
    host_root = install_root / "host" / "fxr"
    runtime_versions = {path.name for path in runtime_root.iterdir()} if runtime_root.is_dir() else set()
    host_versions = {path.name for path in host_root.iterdir()} if host_root.is_dir() else set()
    shared_versions = sorted(runtime_versions & host_versions)
    if not shared_versions:
        raise RuntimeError("The SDK install has no matching host/fxr and Microsoft.NETCore.App runtime directories.")
    log.info(f"Repository .NET SDK ready: {dotnet}")
    log.info(f"Private runtime versions: {', '.join(shared_versions)}")
