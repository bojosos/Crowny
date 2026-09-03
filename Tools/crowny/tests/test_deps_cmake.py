import _bootstrap  # noqa: F401

import unittest

from crowny.deps import cmake


class SimdCacheTests(unittest.TestCase):
    def test_msvc_avx2(self):
        entries = cmake.simd_cache_entries("avx2", "msvc")
        self.assertEqual(entries["CMAKE_CXX_FLAGS"], "/arch:AVX2")

    def test_gnu_avx2_includes_auxiliary_isa_flags(self):
        entries = cmake.simd_cache_entries("avx2", "gnu")
        self.assertEqual(
            entries["CMAKE_CXX_FLAGS"],
            "-mavx2 -mbmi -mpopcnt -mlzcnt -mf16c",
        )

    def test_gnu_sse41(self):
        entries = cmake.simd_cache_entries("sse4.1", "gnu")
        self.assertEqual(entries["CMAKE_CXX_FLAGS"], "-msse4.1")

    def test_invalid_level_rejected(self):
        with self.assertRaises(ValueError):
            cmake.simd_cache_entries("avx512", "gnu")


if __name__ == "__main__":
    unittest.main()
