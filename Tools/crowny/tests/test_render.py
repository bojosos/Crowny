import _bootstrap  # noqa: F401

import tempfile
import unittest
from pathlib import Path
from unittest import mock

from crowny import cli, render


class SpriteBenchmarkTests(unittest.TestCase):
    def test_cli_forwards_benchmark_mode(self):
        with tempfile.TemporaryDirectory() as directory, mock.patch.object(render, "run") as run:
            self.assertEqual(cli.main(["--root", directory, "render-tests", "--benchmark-sprites", "smoke", "--no-build"]), 0)
            self.assertEqual(run.call_args.kwargs["benchmark_sprites"], "smoke")
            self.assertTrue(run.call_args.kwargs["no_build"])

    def test_benchmarks_run_both_backends_without_image_comparison(self):
        root = Path("benchmark-workspace")
        for mode, flag in (("full", "--benchmark-sprites"), ("smoke", "--benchmark-sprites-smoke")):
            with self.subTest(mode=mode), mock.patch.object(render.locks, "output_read_lock"), mock.patch.object(
                render, "exclusive_lock"
            ), mock.patch.object(render, "_executable", return_value=root / "RenderTests"), mock.patch.object(
                render.cmd, "run_checked"
            ) as command, mock.patch.object(render.build_module, "build") as build:
                render.run(root=root, backend="All", benchmark_sprites=mode, no_build=True)
                build.assert_not_called()
                self.assertEqual(command.call_count, 2)
                for call, backend in zip(command.call_args_list, ("vulkan", "opengl")):
                    arguments = call.args[0]
                    self.assertEqual(arguments[arguments.index("--backend") + 1], backend)
                    self.assertIn(flag, arguments)
                    self.assertNotIn("--compare-backends", arguments)

    def test_incompatible_options_fail_before_build_or_execution(self):
        for options in ({"update_references": True}, {"filter": "persistent-sprites"}, {"benchmark_sprites": "invalid"}):
            with self.subTest(options=options), mock.patch.object(render.build_module, "build") as build, mock.patch.object(
                render.cmd, "run_checked"
            ) as command:
                with self.assertRaises(ValueError):
                    render.run(root=Path("benchmark-workspace"), **{"benchmark_sprites": "full", **options})
                build.assert_not_called()
                command.assert_not_called()


if __name__ == "__main__":
    unittest.main()
