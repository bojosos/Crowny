import _bootstrap  # noqa: F401

import os
import tempfile
import unittest
from pathlib import Path

from crowny import build as build_module
from crowny import locks


class AutoJobsTests(unittest.TestCase):
    def test_auto_reserves_capacity_for_a_second_build(self):
        budget, wanted, minimum = locks.auto_jobs(0)
        self.assertEqual(budget, max(1, os.cpu_count() or 1))
        reserved = min(4, budget // 3)
        self.assertEqual(wanted, budget - reserved)
        self.assertEqual(minimum, max(1, min(reserved, wanted)))

    def test_fixed_request_gets_exact_reservation(self):
        budget, wanted, minimum = locks.auto_jobs(64)
        self.assertEqual(wanted, budget)
        self.assertEqual(minimum, budget)

    def test_fixed_request_above_budget_is_capped(self):
        _, wanted, minimum = locks.auto_jobs(999)
        self.assertEqual(wanted, max(1, os.cpu_count() or 1))
        self.assertEqual(minimum, wanted)


class CompilerLeaseTests(unittest.TestCase):
    def _lease_dir(self, root):
        from crowny import env

        return env.coordination_root(root) / "compiler-leases"

    def test_grant_and_release(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with locks.compiler_lease(root, 2) as lease:
                self.assertGreaterEqual(lease["jobs"], 2)
                leases = list(self._lease_dir(root).glob("*.json"))
                self.assertEqual(len(leases), 1)
            leases = list(self._lease_dir(root).glob("*.json"))
            self.assertEqual(len(leases), 0)

    def test_two_small_leases_coexist(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with locks.compiler_lease(root, 1):
                with locks.compiler_lease(root, 1):
                    leases = list(self._lease_dir(root).glob("*.json"))
                    self.assertEqual(len(leases), 2)


class BuildTargetTests(unittest.TestCase):
    def test_solution_target_mapping(self):
        self.assertEqual(build_module.SOLUTION_TARGETS["Engine"], ["Crowny"])
        self.assertIn("Crowny-Builder", build_module.SOLUTION_TARGETS["All"])

    def test_invalid_target_rejected(self):
        with self.assertRaises(ValueError):
            build_module.validate_target("Shaders")

    def test_invalid_cache_rejected(self):
        with self.assertRaises(ValueError):
            build_module.validate_compiler_cache("Incredibuild")

    def test_output_dir_layout(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = build_module.output_dir(root, "Release-address")
            self.assertTrue(str(path).endswith("bin\\Release-address-windows-x86_64") or "Release-address" in str(path))


if __name__ == "__main__":
    unittest.main()
