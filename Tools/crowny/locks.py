import ctypes
import ctypes.wintypes
import json
import os
import sys
import time
import uuid
from contextlib import contextmanager
from pathlib import Path

from . import env, hashing, log

if sys.platform == "win32":
    import msvcrt
else:
    import fcntl

_LOCK_RANGE_BYTES = 64
_OWNER_POLL_SECONDS = 1.0


if sys.platform == "win32":

    class _OVERLAPPED(ctypes.Structure):
        _fields_ = [
            ("Internal", ctypes.c_void_p),
            ("InternalHigh", ctypes.c_void_p),
            ("Offset", ctypes.wintypes.DWORD),
            ("OffsetHigh", ctypes.wintypes.DWORD),
            ("hEvent", ctypes.wintypes.HANDLE),
        ]

else:
    _OVERLAPPED = None


def _is_windows():
    return sys.platform == "win32"


def pid_alive(pid):
    try:
        pid = int(pid)
    except (TypeError, ValueError):
        return False
    if pid <= 0:
        return False
    if pid == os.getpid():
        return True
    if _is_windows():
        synchronize = 0x00100000
        handle = ctypes.windll.kernel32.OpenProcess(synchronize, False, pid)
        if handle:
            ctypes.windll.kernel32.CloseHandle(handle)
            return True
        return ctypes.GetLastError() == 5
    try:
        os.kill(pid, 0)
        return True
    except ProcessLookupError:
        return False
    except PermissionError:
        return True


def _owner_record():
    return {
        "pid": os.getpid(),
        "command": " ".join(sys.argv),
        "startedUtc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
    }


def _write_owner(path):
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        json.dump(_owner_record(), handle, separators=(",", ":"))


def _describe_owner(owner_path):
    try:
        with open(owner_path, "r", encoding="utf-8-sig") as handle:
            record = json.load(handle)
    except (OSError, ValueError):
        return "owner details unavailable"
    if pid_alive(record.get("pid")):
        return f"PID {record.get('pid')}: {record.get('command')}"
    try:
        os.unlink(owner_path)
    except OSError:
        pass
    return None


class _LockedFile:
    def __init__(self, fd, path, owner_path):
        self.fd = fd
        self.path = Path(path)
        self.owner_path = Path(owner_path)

    def release(self):
        if self.fd is None:
            return
        try:
            try:
                os.unlink(self.owner_path)
            except OSError:
                pass
            if _is_windows():
                self._unlock_windows()
            else:
                fcntl.flock(self.fd, fcntl.LOCK_UN)
        finally:
            os.close(self.fd)
            self.fd = None

    def _unlock_windows(self):
        handle = msvcrt.get_osfhandle(self.fd)
        ctypes.windll.kernel32.UnlockFileEx(
            handle, 0, _LOCK_RANGE_BYTES, 0, ctypes.byref(_OVERLAPPED())
        )

    def __enter__(self):
        return self

    def __exit__(self, *exc_info):
        self.release()


def _lock_windows(fd, exclusive):
    handle = msvcrt.get_osfhandle(fd)
    lockfile_fail_immediately = 0x00000001
    lockfile_exclusive_lock = 0x00000002
    flags = lockfile_fail_immediately
    if exclusive:
        flags |= lockfile_exclusive_lock
    if not ctypes.windll.kernel32.LockFileEx(
        handle, flags, 0, _LOCK_RANGE_BYTES, 0, ctypes.byref(_OVERLAPPED())
    ):
        raise OSError("LockFileEx failed")


def _open_lock_file(path):
    path.parent.mkdir(parents=True, exist_ok=True)
    return os.open(str(path), os.O_RDWR | os.O_CREAT, 0o666)


@contextmanager
def _file_gate(path, owner_path, exclusive, wait, blocked_message):
    reported = False
    while True:
        fd = _open_lock_file(path)
        try:
            if _is_windows():
                _lock_windows(fd, exclusive)
            else:
                mode = fcntl.LOCK_EX if exclusive else fcntl.LOCK_SH
                fcntl.flock(fd, mode | fcntl.LOCK_NB)
        except OSError:
            os.close(fd)
            description = _describe_owner(owner_path)
            if not wait:
                raise RuntimeError(f"{blocked_message} {description or 'owner details unavailable'}")
            if not reported:
                log.info(f"{blocked_message} {description or 'owner details unavailable'}; waiting...")
                reported = True
            time.sleep(_OWNER_POLL_SECONDS)
            continue
        except BaseException:
            os.close(fd)
            raise
        _write_owner(owner_path)
        lock = _LockedFile(fd, path, owner_path)
        try:
            yield lock
        finally:
            lock.release()
        return


def lock_name(root, name, scope="Worktree"):
    scope_path = (
        env.coordination_root(root) if scope == "Shared" else (root or env.repo_root())
    )
    identity = hashing.content_hash(values=[os.path.abspath(str(scope_path)).lower(), name])
    return identity[:20]


def exclusive_lock(root, name, wait=True, scope="Worktree"):
    lock_root = env.locks_root(root)
    lock_path = lock_root / f"{lock_name(root, name, scope)}.lock"
    owner_path = Path(str(lock_path) + ".owner.json")
    return _file_gate(
        lock_path,
        owner_path,
        exclusive=True,
        wait=wait,
        blocked_message=f"A Crowny {name} operation is already running.",
    )


def _gate_path(root, name, suffix=".gate"):
    gate_root = env.locks_root(root)
    gate_root.mkdir(parents=True, exist_ok=True)
    return gate_root / f"{lock_name(root, name)}{suffix}"


def project_read_lock(root, wait=True):
    gate = _gate_path(root, "projects", ".projects.gate")
    return _file_gate(
        gate,
        gate.with_name(f"{gate.name}.reader.{os.getpid()}-{uuid.uuid4().hex}.owner.json"),
        exclusive=False,
        wait=wait,
        blocked_message="Crowny project generation is currently running.",
    )


def project_write_lock(root, wait=True):
    gate = _gate_path(root, "projects", ".projects.gate")
    return _file_gate(
        gate,
        gate.with_name(gate.name + ".writer.owner.json"),
        exclusive=True,
        wait=wait,
        blocked_message="Cannot regenerate projects while a Crowny build is running.",
    )


def _output_gate(root, configuration):
    return _gate_path(root, f"output-{configuration}")


def output_write_lock(root, configuration, wait=True):
    gate = _output_gate(root, configuration)
    return _file_gate(
        gate,
        gate.with_name(gate.name + ".writer.owner.json"),
        exclusive=True,
        wait=wait,
        blocked_message=f"Crowny {configuration} outputs are in use.",
    )


def output_read_lock(root, configuration, wait=True):
    gate = _output_gate(root, configuration)
    owner = gate.with_name(f"{gate.name}.reader.{os.getpid()}-{uuid.uuid4().hex}.owner.json")
    return _file_gate(
        gate,
        owner,
        exclusive=False,
        wait=wait,
        blocked_message=f"Crowny {configuration} outputs are being updated.",
    )


def auto_jobs(requested_jobs=0):
    budget = max(1, os.cpu_count() or 1)
    reserved = min(4, budget // 3)
    automatic = max(1, budget - reserved)
    if requested_jobs == 0:
        wanted = automatic
        minimum = max(1, min(reserved, wanted))
    else:
        wanted = min(requested_jobs, budget)
        minimum = wanted
    return budget, wanted, minimum


@contextmanager
def compiler_lease(root, requested_jobs=0):
    budget, wanted, minimum = auto_jobs(requested_jobs)
    lease_root = env.coordination_root(root) / "compiler-leases"
    lease_root.mkdir(parents=True, exist_ok=True)
    reported = False

    while True:
        with exclusive_lock(root, "compiler-scheduler", scope="Shared"):
            active = []
            for lease_path in sorted(lease_root.glob("*.json")):
                try:
                    with open(lease_path, "r", encoding="utf-8-sig") as handle:
                        record = json.load(handle)
                    if pid_alive(record.get("pid")):
                        active.append((lease_path, record))
                    else:
                        try:
                            os.unlink(lease_path)
                        except OSError:
                            pass
                except (OSError, ValueError):
                    try:
                        os.unlink(lease_path)
                    except OSError:
                        pass

            used = sum(int(record.get("jobs", 0)) for _, record in active)
            available = max(0, budget - used)
            if available >= minimum:
                granted = min(wanted, available)
                lease_path = lease_root / f"{os.getpid()}-{uuid.uuid4().hex}.json"
                with open(lease_path, "w", encoding="utf-8", newline="\n") as handle:
                    json.dump(
                        {
                            "pid": os.getpid(),
                            "jobs": granted,
                            "command": " ".join(sys.argv),
                            "startedUtc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
                        },
                        handle,
                        indent=2,
                    )
                break

            if not reported:
                owners = "; ".join(
                    f"PID {record.get('pid')} using {record.get('jobs')} worker(s)"
                    for _, record in active
                )
                log.info(
                    f"Waiting for compiler capacity ({used}/{budget} workers active). {owners}"
                )
                reported = True
        time.sleep(_OWNER_POLL_SECONDS)

    try:
        yield {"path": lease_path, "jobs": granted, "requested_jobs": requested_jobs, "budget": budget}
    finally:
        with exclusive_lock(root, "compiler-scheduler", scope="Shared"):
            try:
                os.unlink(lease_path)
            except OSError:
                pass
