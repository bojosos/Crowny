import _bootstrap  # noqa: F401

import os
import unittest

from crowny import env


class ConfigurationMappingTests(unittest.TestCase):
    def test_stamps_local_deps_and_coordination_shared(self):
        import tempfile
        from pathlib import Path

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            os.environ.pop("CROWNY_DEPS_ROOT", None)
            os.environ.pop("CROWNY_BUILD_COORDINATION_ROOT", None)
            try:
                roots = env.build_roots(root)
                self.assertEqual(roots["common_repository_root"], root)
                self.assertEqual(roots["dependency_root"], root / ".deps")
                self.assertEqual(
                    roots["coordination_root"], root / ".deps" / "build-coordination"
                )
                self.assertEqual(env.stamps_root(root), root / ".deps" / "stamps")
                self.assertEqual(env.locks_root(root), roots["coordination_root"] / "locks")
            finally:
                os.environ.pop("CROWNY_DEPS_ROOT", None)
                os.environ.pop("CROWNY_BUILD_COORDINATION_ROOT", None)

    def test_root_overrides_are_honored(self):
        import tempfile
        from pathlib import Path

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            shared = root / "shared-deps"
            coordination = root / "coordination"
            os.environ["CROWNY_DEPS_ROOT"] = str(shared)
            os.environ["CROWNY_BUILD_COORDINATION_ROOT"] = str(coordination)
            try:
                self.assertEqual(env.deps_root(root), shared)
                self.assertEqual(env.coordination_root(root), coordination)
                self.assertEqual(env.locks_root(root), coordination / "locks")
                self.assertEqual(env.stamps_root(root), root / ".deps" / "stamps")
            finally:
                os.environ.pop("CROWNY_DEPS_ROOT", None)
                os.environ.pop("CROWNY_BUILD_COORDINATION_ROOT", None)

    def test_plain_configurations(self):
        self.assertEqual(env.workspace_configuration("Release", "None"), "Release")
        self.assertEqual(env.workspace_configuration("Debug", "None"), "Debug")
        self.assertEqual(env.workspace_configuration("Dist", "None"), "Dist")

    def test_asan_configurations(self):
        self.assertEqual(env.workspace_configuration("Debug", "Address"), "DebugASan")
        self.assertEqual(env.workspace_configuration("Release", "Address"), "ReleaseASan")

    def test_dist_asan_is_rejected(self):
        with self.assertRaises(ValueError):
            env.workspace_configuration("Dist", "Address")

    def test_output_configurations(self):
        self.assertEqual(env.output_configuration("Release", "Address"), "Release-address")
        self.assertEqual(env.output_configuration("Dist", "None"), "Dist")

    def test_build_output_sets(self):
        self.assertEqual(env.build_output_configurations("DebugASan"), ["Debug", "DebugASan"])
        self.assertEqual(env.build_output_configurations("Release"), ["Release"])

    def test_managed_defines(self):
        self.assertEqual(env.managed_define("Debug"), "CW_DEBUG")
        self.assertEqual(env.managed_define("Dist"), "CW_DIST")


if __name__ == "__main__":
    unittest.main()
