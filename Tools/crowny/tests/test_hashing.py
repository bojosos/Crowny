import _bootstrap  # noqa: F401

import unittest

from crowny import hashing


class ContentHashTests(unittest.TestCase):
    def test_value_order_matters(self):
        first = hashing.content_hash(values=["a", "b"])
        second = hashing.content_hash(values=["b", "a"])
        self.assertNotEqual(first, second)

    def test_stable_for_same_inputs(self):
        self.assertEqual(
            hashing.content_hash(values=["a", "b"]),
            hashing.content_hash(values=["a", "b"]),
        )

    def test_missing_file_is_marked(self):
        digest = hashing.content_hash(files=["Z:/definitely/missing/file.h"])
        self.assertEqual(digest, hashing.content_hash(files=["Z:/definitely/missing/file.h"]))
        self.assertNotEqual(digest, hashing.content_hash(values=[]))

    def test_real_file_changes_digest(self):
        import tempfile
        from pathlib import Path

        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "file.txt"
            path.write_text("hello", encoding="utf-8")
            with_file = hashing.content_hash(files=[str(path)])
            path.write_text("world", encoding="utf-8")
            after_change = hashing.content_hash(files=[str(path)])
            self.assertNotEqual(with_file, after_change)


if __name__ == "__main__":
    unittest.main()
