import _bootstrap  # noqa: F401

import unittest

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


if __name__ == "__main__":
    unittest.main()
