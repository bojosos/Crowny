import shutil


def find_git():
    git = shutil.which("git")
    if not git:
        raise RuntimeError("Git is required.")
    return git


def rev_parse(path):
    if not (path / ".git").exists():
        return ""
    try:
        import subprocess

        result = subprocess.run(
            ["git", "-C", str(path), "rev-parse", "HEAD"],
            capture_output=True,
            text=True,
            check=False,
        )
        if result.returncode != 0:
            return ""
        return result.stdout.strip()
    except OSError:
        return ""
