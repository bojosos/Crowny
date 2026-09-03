import _bootstrap  # noqa: F401

import tempfile
import unittest
from pathlib import Path

from crowny import env, hashing, locks, stamps


class LockNameTests(unittest.TestCase):
    def test_deterministic_and_distinct(self):
        with tempfile.TemporaryDirectory() as directory:
            first = locks.lock_name(Path(directory), "project-generation")
            second = locks.lock_name(Path(directory), "project-generation")
            other = locks.lock_name(Path(directory), "managed-assemblies")
            self.assertEqual(first, second)
            self.assertNotEqual(first, other)
            self.assertEqual(len(first), 20)

    def test_shared_scope_ignores_repository_root(self):
        import os

        with tempfile.TemporaryDirectory() as directory:
            root_a = Path(directory) / "a"
            root_b = Path(directory) / "b"
            root_a.mkdir()
            root_b.mkdir()
            coordination = Path(directory) / "coordination"
            os.environ["CROWNY_BUILD_COORDINATION_ROOT"] = str(coordination)
            try:
                shared_a = locks.lock_name(root_a, "compiler-scheduler", scope="Shared")
                shared_b = locks.lock_name(root_b, "compiler-scheduler", scope="Shared")
                self.assertEqual(shared_a, shared_b)
                self.assertNotEqual(
                    locks.lock_name(root_a, "compiler-scheduler"),
                    locks.lock_name(root_b, "compiler-scheduler"),
                )
            finally:
                os.environ.pop("CROWNY_BUILD_COORDINATION_ROOT", None)

    def test_project_gate_file_uses_projects_suffix(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            gate = locks._gate_path(root, "projects", ".projects.gate")
            self.assertTrue(gate.name.endswith(".projects.gate"))
            output = locks._gate_path(root, "output-Release")
            self.assertTrue(output.name.endswith(".gate"))
            self.assertFalse(output.name.endswith(".projects.gate"))


class ExclusiveLockTests(unittest.TestCase):
    def test_acquire_and_release(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with locks.exclusive_lock(root, "test-operation"):
                with self.assertRaises(RuntimeError):
                    locks.exclusive_lock(root, "test-operation", wait=False).__enter__()

    def test_reacquire_after_release(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            lock = locks.exclusive_lock(root, "test-operation")
            lock.__enter__()
            lock.__exit__(None, None, None)
            with locks.exclusive_lock(root, "test-operation", wait=False):
                pass


class GateTests(unittest.TestCase):
    def test_multiple_readers_single_writer(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            reader_a = locks.project_read_lock(root, wait=False)
            reader_a.__enter__()
            reader_b = locks.project_read_lock(root, wait=False)
            reader_b.__enter__()
            with self.assertRaises(RuntimeError):
                locks.project_write_lock(root, wait=False).__enter__()
            reader_a.__exit__(None, None, None)
            reader_b.__exit__(None, None, None)
            writer = locks.project_write_lock(root, wait=False)
            writer.__enter__()
            with self.assertRaises(RuntimeError):
                locks.project_read_lock(root, wait=False).__enter__()
            writer.__exit__(None, None, None)


class StampTests(unittest.TestCase):
    def test_text_stamp_roundtrip(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "stamp.txt"
            stamps.write_text_stamp(path, "a=1\nb=2")
            self.assertTrue(stamps.text_stamp_matches(path, "a=1\nb=2"))
            self.assertFalse(stamps.text_stamp_matches(path, "a=1\nb=3"))

    def test_fingerprint_stamp_roundtrip(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            stamps.fingerprint_stamp("test-projects.json", "deadbeef", root=root)
            self.assertTrue(stamps.fingerprint_matches("test-projects.json", "deadbeef", root=root))
            self.assertFalse(stamps.fingerprint_matches("test-projects.json", "feedface", root=root))
            stamp = stamps.read_stamp(env.stamps_root(root) / "test-projects.json")
            self.assertEqual(stamp["schema"], 2)


class ContentHashParityTests(unittest.TestCase):
    def test_hash_format_is_uppercase_hex(self):
        digest = hashing.content_hash(values=["parity"])
        self.assertEqual(len(digest), 64)
        self.assertEqual(digest, digest.upper())


if __name__ == "__main__":
    unittest.main()
