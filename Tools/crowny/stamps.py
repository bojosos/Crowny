import json
import os
import tempfile
from pathlib import Path

from . import env


def read_stamp(path):
    try:
        with open(path, "r", encoding="utf-8-sig") as handle:
            return json.load(handle)
    except (OSError, ValueError):
        return None


def write_stamp(path, payload):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    handle = tempfile.NamedTemporaryFile(
        "w", encoding="utf-8", dir=str(path.parent), delete=False, newline="\n"
    )
    try:
        json.dump(payload, handle, indent=2)
        handle.close()
        os.replace(handle.name, path)
    except BaseException:
        os.unlink(handle.name)
        raise


def fingerprint_stamp(name, fingerprint, root=None, extra=None):
    payload = {"schema": 2, "fingerprint": fingerprint}
    if extra:
        payload.update(extra)
    write_stamp(env.stamps_root(root) / name, payload)


def fingerprint_matches(name, fingerprint, root=None):
    stamp = read_stamp(env.stamps_root(root) / name)
    return bool(stamp) and stamp.get("fingerprint") == fingerprint


def text_stamp_matches(path, expected):
    try:
        with open(path, "r", encoding="utf-8-sig") as handle:
            return handle.read().strip() == expected.strip()
    except OSError:
        return False


def write_text_stamp(path, expected):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(expected + "\n")
