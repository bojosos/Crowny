import _bootstrap  # noqa: F401

import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from crowny import cli, env, osl


class OslToolchainTests(unittest.TestCase):
    def test_commands_accept_portable_paths(self):
        args = cli.build_parser().parse_args(["deps", "osl", "--deps-prefix", "/opt/osl deps", "--jobs", "3"])
        self.assertEqual(args.deps_prefix, "/opt/osl deps")
        args = cli.build_parser().parse_args(["osl", "compile", "shader with spaces.osl", "output dir", "--output", "result"])
        self.assertEqual(args.output, "result")

    def test_host_paths_and_arm_detection(self):
        for host, platform_name, suffix in (("win32", "windows", ".exe"), ("linux", "linux", ""), ("darwin", "macos", "")):
            with self.subTest(host=host), patch.object(osl.sys, "platform", host), patch.object(osl.platform, "machine", return_value="aarch64"):
                self.assertEqual(osl.host_tag(), platform_name + "-arm64")
                self.assertEqual(osl.executable("llc"), "llc" + suffix)

    def test_compiler_environment_is_private_and_removes_external_options(self):
        with patch.dict(os.environ, {"PATH": "engine-tools", "OSL_OPTIONS": "debug_output_cpp=3"}), patch.object(osl.sys, "platform", "linux"):
            original = os.environ.copy()
            result = osl.compiler_environment(Path("/deps"), Path("/osl"))
            self.assertEqual(dict(os.environ), original)
            self.assertNotIn("OSL_OPTIONS", result)
            self.assertIn(str(Path("/osl/lib")), result["LD_LIBRARY_PATH"])
            self.assertIn("engine-tools", result["PATH"])

    def test_missing_or_foreign_toolchain_requires_explicit_setup(self):
        with tempfile.TemporaryDirectory() as directory, patch.dict(os.environ, {"CROWNY_OSL_ROOT": directory}):
            with self.assertRaisesRegex(RuntimeError, "deps osl"):
                osl.load_toolchain(env.repo_root())
            (Path(directory) / "toolchain.json").write_text(json.dumps({"schema": 1, "host": "wrong-host"}))
            with self.assertRaisesRegex(RuntimeError, "does not match"):
                osl.load_toolchain(env.repo_root())

    def test_normal_compile_keeps_spaces_as_arguments_and_excludes_test_generator(self):
        with tempfile.TemporaryDirectory(prefix="osl tools ") as temporary:
            directory = Path(temporary)
            (directory / "toolchain.json").write_text(json.dumps({"deps_prefix": str(directory / "dependencies")}))
            with patch.object(osl, "load_toolchain", return_value=(directory, {"PATH": "compiler only"})), \
                    patch.object(osl, "spirv_directory", return_value=directory / "spirv tools"):
                command, process_env = osl.compile_command(env.repo_root(), directory / "source with spaces.osl", directory / "output dir")
            self.assertIn((directory / "source with spaces.osl").resolve(), command)
            self.assertNotIn("--test-reference-generator", command)
            self.assertEqual(process_env, {"PATH": "compiler only"})

    def test_vulkan_tools_are_checked_before_compilation(self):
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaisesRegex(RuntimeError, "Missing glslangValidator"):
                osl.spirv_directory(temporary)
            for name in ("glslangValidator", "spirv-as", "spirv-dis", "spirv-link", "spirv-opt", "spirv-val"):
                (Path(temporary) / osl.executable(name)).touch()
            self.assertEqual(osl.spirv_directory(temporary), Path(temporary).resolve())

    def test_source_checkout_is_pinned_patchable_and_preserves_edits(self):
        # Exercise real local Git, including repeat setup and refusing a different HEAD.
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            upstream, checkout = root / "upstream", root / "checkout"
            upstream.mkdir()
            def git(*args):
                return subprocess.run(["git", "-C", str(upstream), *args], capture_output=True, text=True, check=True).stdout.strip()
            git("init")
            git("config", "user.email", "test@example.invalid")
            git("config", "user.name", "Test")
            (upstream / "value.txt").write_text("before\n")
            git("add", ".")
            git("commit", "-m", "initial")
            revision = git("rev-parse", "HEAD")
            patches = root / "Tools/osl"
            patches.mkdir(parents=True)
            (patches / "change.patch").write_text("diff --git a/value.txt b/value.txt\n--- a/value.txt\n+++ b/value.txt\n@@ -1 +1 @@\n-before\n+after\n")
            specification = {"repository": str(upstream), "revision": revision, "patches": ["change.patch"]}
            osl.acquire_source(root, checkout, specification)
            osl.acquire_source(root, checkout, specification)
            self.assertEqual((checkout / "value.txt").read_text(), "after\n")
            (checkout / "keep.txt").write_text("user edit")
            with self.assertRaisesRegex(RuntimeError, "not pinned"):
                osl.acquire_source(root, checkout, {**specification, "revision": "0" * 40})
            self.assertEqual((checkout / "keep.txt").read_text(), "user edit")

    def test_windows_lock_contains_exact_packages_and_hashes(self):
        lock = (env.repo_root() / "Tools/osl/locks/windows-x86_64.txt").read_text()
        self.assertIn("@EXPLICIT", lock)
        packages = [line for line in lock.splitlines() if line.startswith("https:")]
        self.assertGreater(len(packages), 20)
        for package in packages:
            self.assertRegex(package, r"^https://conda.anaconda.org/conda-forge/(win-64|noarch)/.+#[a-f0-9]{32}$")
        self.assertTrue(any("llvmdev-20.1.8" in package for package in packages))


if __name__ == "__main__":
    unittest.main()
