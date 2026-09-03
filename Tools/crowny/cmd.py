import subprocess
import sys

from . import log


class CommandError(RuntimeError):
    def __init__(self, message, exit_code, command):
        super().__init__(message)
        self.exit_code = exit_code
        self.command = command


def run_checked(args, cwd=None, env=None, capture=False):
    printable = " ".join(str(a) for a in args)
    log.info(f"> {printable}" + (f"  (in {cwd})" if cwd else ""))
    result = subprocess.run(
        [str(a) for a in args],
        cwd=str(cwd) if cwd else None,
        env=env,
        check=False,
        text=True,
        capture_output=capture,
    )
    if result.returncode != 0:
        if capture:
            if result.stdout:
                print(result.stdout, end="")
            if result.stderr:
                print(result.stderr, end="", file=sys.stderr)
        raise CommandError(
            f"Command failed with exit code {result.returncode}: {printable}",
            result.returncode,
            list(args),
        )
    return result
