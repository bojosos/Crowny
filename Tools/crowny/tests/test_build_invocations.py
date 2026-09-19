import _bootstrap  # noqa: F401

import tempfile
import unittest
from pathlib import Path
from unittest import mock

from crowny import build as build_module
from crowny import cli, msbuild


def _run_build(root, target, recorded, resource_error=None, **build_options):
    with mock.patch.object(build_module.sys, "platform", "win32"), mock.patch.object(build_module.premake, "ensure_projects"), mock.patch.object(
        build_module.env, "configure_default_environment"
    ), mock.patch.object(build_module.locks, "output_write_lock"), mock.patch.object(
        build_module.locks, "project_read_lock"
    ), mock.patch.object(
        build_module.locks, "compiler_lease"
    ) as lease, mock.patch.object(
        msbuild, "find_msbuild", return_value=root / "MSBuild.exe"
    ), mock.patch.object(
        build_module,
        "run_msbuild",
        side_effect=lambda msbuild_path, solution, targets, config, jobs, **kwargs: (
            recorded.append((targets, kwargs)),
            {"exit_code": 0, "peak_compiler_working_set_bytes": 0},
        )[1],
    ), mock.patch(
        "crowny.managed.ensure"
    ) as managed_build, mock.patch("crowny.player.stage_template") as stage_template, mock.patch(
        "crowny.resources.update", side_effect=resource_error
    ) as resource_update, mock.patch.dict(
        "os.environ", {"CROWNY_OUTPUT_WRITE_LOCK": ""}
    ):
        lease.return_value.__enter__.return_value = {"jobs": 8, "budget": 12}
        build_module.build(root=root, target=target, jobs=8, **build_options)
        return managed_build, resource_update, stage_template


class AllTargetSplitTests(unittest.TestCase):
    def test_headless_all_build_keeps_native_and_managed_builds(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            recorded = []
            managed_build, resource_update, stage_template = _run_build(
                root, "All", recorded,
                resource_error=RuntimeError("No Vulkan-capable physical device was found"),
                skip_editor_resources=True,
            )

            self.assertEqual([targets for targets, _ in recorded], [
                ["Crowny", r"Dependencies\ImNodeFlow", r"Dependencies\imgui-node-editor", r"Dependencies\catch2"],
                build_module.ALL_FOLLOW_UP_TARGETS,
            ])
            managed_build.assert_called_once_with(root, "Release")
            resource_update.assert_not_called()
            stage_template.assert_not_called()

    def test_default_all_build_stages_player_template(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            _, resource_update, stage_template = _run_build(root, "All", [])
            resource_update.assert_called_once()
            stage_template.assert_called_once_with(root, "Release", "Release")

    def test_default_all_build_still_cooks_editor_resources(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(RuntimeError, "No Vulkan-capable physical device"):
                _run_build(
                    Path(directory), "All", [],
                    resource_error=RuntimeError("No Vulkan-capable physical device was found"),
                )

    def test_cli_forwards_headless_build_option(self):
        with tempfile.TemporaryDirectory() as directory, mock.patch.object(build_module, "build") as run_build:
            self.assertEqual(cli.main(["--root", directory, "build", "All", "--skip-editor-resources"]), 0)
            self.assertEqual(run_build.call_args.kwargs["target"], "All")
            self.assertTrue(run_build.call_args.kwargs["skip_editor_resources"])

    def test_all_builds_crowny_first_then_apps_without_references(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "Crowny.sln").write_text("", encoding="utf-8")
            recorded = []
            _run_build(root, "All", recorded)

            self.assertEqual(len(recorded), 2)
            first_targets, first_kwargs = recorded[0]
            # Editor node libraries and Catch2 are not engine dependencies.
            # They must exist before application reference builds are disabled.
            self.assertEqual(first_targets, [
                "Crowny", r"Dependencies\ImNodeFlow", r"Dependencies\imgui-node-editor", r"Dependencies\catch2"
            ])
            self.assertTrue(first_kwargs["build_project_references"])
            self.assertEqual(first_kwargs["nodes"], 1)

            second_targets, second_kwargs = recorded[1]
            self.assertEqual(second_targets, build_module.ALL_FOLLOW_UP_TARGETS)
            self.assertFalse(second_kwargs["build_project_references"])
            self.assertEqual(second_kwargs["nodes"], 1)

    def test_engine_keeps_reference_building(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "Crowny.sln").write_text("", encoding="utf-8")
            recorded = []
            _run_build(root, "Engine", recorded)

            self.assertEqual(len(recorded), 1)
            targets, kwargs = recorded[0]
            self.assertEqual(targets, ["Crowny"])
            self.assertTrue(kwargs["build_project_references"])


if __name__ == "__main__":
    unittest.main()
