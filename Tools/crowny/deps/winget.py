import subprocess
import sys

from .. import cmd, log

PACKAGES = {
    "mono": "Mono.Mono",
    "7zip": "7zip.7zip",
    "cmake": "Kitware.CMake",
}


def _installed(package_id):
    result = subprocess.run(
        [
            "winget",
            "list",
            "--id",
            package_id,
            "--exact",
            "--accept-source-agreements",
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    return result.returncode == 0 and package_id in result.stdout


def ensure_package(package_id):
    if sys.platform != "win32":
        raise RuntimeError("winget is only available on Windows.")
    if _installed(package_id):
        log.info(f"{package_id} is already installed.")
        return
    cmd.run_checked(
        [
            "winget",
            "install",
            "--id",
            package_id,
            "--exact",
            "--silent",
            "--accept-package-agreements",
            "--accept-source-agreements",
            "--disable-interactivity",
        ]
    )
