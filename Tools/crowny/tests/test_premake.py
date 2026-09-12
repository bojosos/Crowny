import _bootstrap  # noqa: F401

import unittest
from contextlib import nullcontext
from pathlib import Path
from tempfile import TemporaryDirectory
from unittest.mock import patch

from crowny import env, premake


class ProjectFingerprintTests(unittest.TestCase):
    def test_stable_for_same_inputs(self):
        root = env.repo_root()
        first = premake.project_fingerprint(root, simd="avx2")
        second = premake.project_fingerprint(root, simd="avx2")
        self.assertEqual(first, second)

    def test_sensitive_to_simd(self):
        root = env.repo_root()
        avx2 = premake.project_fingerprint(root, simd="avx2")
        sse41 = premake.project_fingerprint(root, simd="sse4.1")
        self.assertNotEqual(avx2, sse41)

    def test_action_per_platform(self):
        import sys

        expected = "vs2022" if sys.platform == "win32" else "gmake2"
        self.assertEqual(premake.premake_action(), expected)

    def test_flags_include_nodes_and_simd(self):
        flags = premake.premake_flags("SSE4.1")
        self.assertIn("--with-nodes", flags)
        self.assertIn("--simd=sse4.1", flags)

    def test_finished_generation_does_not_wait_for_active_build_readers(self):
        with TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "Crowny.sln").touch()
            with (
                patch.object(premake.env, "configure_default_environment"),
                patch.object(premake, "project_fingerprint", return_value="current"),
                patch.object(premake.stamps, "fingerprint_matches", side_effect=[False, True]),
                patch.object(premake.locks, "exclusive_lock", return_value=nullcontext()),
                patch.object(premake.locks, "project_write_lock", side_effect=AssertionError("An active build still holds a reader lock")) as write_lock,
                patch.object(premake.cmd, "run_checked") as generate,
            ):
                premake.ensure_projects(root)
            write_lock.assert_not_called()
            generate.assert_not_called()


if __name__ == "__main__":
    unittest.main()
