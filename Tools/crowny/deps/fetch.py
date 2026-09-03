import hashlib
import os
import shutil
import sys
import tempfile
import time
import urllib.error
import urllib.request
from pathlib import Path

from .. import cmd, log


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def _move_window_safe(partial, destination):
    for attempt in range(10):
        try:
            os.replace(partial, destination)
            return
        except PermissionError:
            time.sleep(0.2 * (attempt + 1))
    os.replace(partial, destination)


def download(url, destination, sha256=None, resume=True, retries=3):
    destination = Path(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)
    partial = Path(str(destination) + ".part")

    last_error = None
    for attempt in range(1, retries + 1):
        try:
            _download_once(url, destination, partial, resume)
            break
        except (urllib.error.URLError, OSError) as error:
            last_error = error
            if attempt < retries:
                log.warn(f"Download failed ({error}); retrying ({attempt}/{retries})...")
                time.sleep(2 * attempt)
    else:
        raise RuntimeError(f"Downloading {url} failed: {last_error}")

    if sha256:
        actual = sha256_file(destination)
        if actual != sha256.upper():
            destination.unlink(missing_ok=True)
            raise RuntimeError(
                f"{destination.name} failed checksum validation "
                f"(expected {sha256.upper()}, got {actual})."
            )
    return destination


def _download_once(url, destination, partial, resume):
    offset = partial.stat().st_size if resume and partial.exists() else 0
    if offset and not partial.exists():
        offset = 0
    request = urllib.request.Request(url)
    if offset:
        request.add_header("Range", f"bytes={offset}-")
    log.info(f"Downloading {url}...")
    with urllib.request.urlopen(request) as response:
        if offset and response.status != 206:
            offset = 0
        mode = "ab" if offset else "wb"
        with open(partial, mode) as handle:
            while True:
                chunk = response.read(1024 * 1024)
                if not chunk:
                    break
                handle.write(chunk)
    if destination.exists():
        destination.unlink()
    _move_window_safe(partial, destination)


def find_7z():
    candidate = shutil.which("7z") or shutil.which("7zz")
    if candidate:
        return candidate
    if sys.platform == "win32":
        program_files = os.environ.get("ProgramFiles", r"C:\Program Files")
        bundled = Path(program_files) / "7-Zip" / "7z.exe"
        if bundled.is_file():
            return str(bundled)
    raise RuntimeError("7-Zip is required to extract this payload.")


def extract_7z(archive, output_dir, switches=()):
    args = [find_7z(), "x", "-y", f"-o{output_dir}"]
    args.extend(switches)
    args.append(str(archive))
    cmd.run_checked(args)


def assert_child_path(root, candidate):
    root = Path(root).resolve()
    candidate = Path(candidate).resolve()
    root_string = str(root).rstrip("\\/") + os.sep
    if not str(candidate).startswith(root_string) and candidate != root:
        raise RuntimeError(f"Refusing to modify a path outside {root}: {candidate}")


def remove_tree(path, guard_root=None):
    if guard_root is not None:
        assert_child_path(guard_root, path)
    shutil.rmtree(path, ignore_errors=True)


def safe_extract_dir(prefix_root, *parts):
    target = Path(prefix_root).joinpath(*parts)
    target.mkdir(parents=True, exist_ok=True)
    return target


def temporary_directory(prefix_root, name):
    assert_child_path(prefix_root, Path(prefix_root) / name)
    return tempfile.mkdtemp(prefix=name, dir=str(prefix_root))
