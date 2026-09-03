import hashlib
from pathlib import Path


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def content_hash(files=(), values=()):
    records = []
    for file in sorted(set(str(f) for f in files)):
        path = Path(file)
        if not path.is_file():
            records.append(f"missing|{file}")
            continue
        records.append(f"file|{path.resolve()}|{sha256_file(path)}")
    for value in values:
        records.append(f"value|{value}")
    payload = "\n".join(records).encode("utf-8")
    return hashlib.sha256(payload).hexdigest().upper()
