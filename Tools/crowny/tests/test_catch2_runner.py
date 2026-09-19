import _bootstrap  # noqa: F401

from pathlib import Path
import unittest
from unittest import mock

from crowny import catch2, cmd


class Catch2RunnerTests(unittest.TestCase):
    def run_lane(self, isolated_status=0, suite_status=0):
        root = Path("checkout")
        executable = root / "Crowny-Tests.exe"

        def execute(arguments, **kwargs):
            status = isolated_status if len(arguments) > 1 else suite_status
            if status:
                raise cmd.CommandError("test process failed", status, arguments)

        with mock.patch.object(catch2.build_module, "build"), mock.patch.object(
            catch2.managed, "ensure"
        ), mock.patch.object(catch2.locks, "output_read_lock"), mock.patch.object(
            catch2, "test_executable", return_value=(executable, root)
        ), mock.patch.object(catch2, "_runtime_environment", return_value={}), mock.patch.object(
            catch2, "_list_tests", return_value=["optional CoreCLR test"]
        ), mock.patch.object(catch2.cmd, "run_checked", side_effect=execute) as run_checked:
            catch2.run(root=root, process_isolated=True)
            self.assertEqual(run_checked.call_count, 2)

    def test_skipped_isolated_test_continues_to_full_suite(self):
        self.run_lane(isolated_status=4)

    def test_passing_isolated_test_continues_to_full_suite(self):
        self.run_lane()

    def test_isolated_failures_and_crashes_are_not_ignored(self):
        for status in (1, 2, 3, 5, 124, 137, 139, -1073741819):
            with self.subTest(status=status), self.assertRaises(cmd.CommandError):
                self.run_lane(isolated_status=status)

    def test_full_suite_must_pass(self):
        for status in (1, 4, 139):
            with self.subTest(status=status), self.assertRaises(cmd.CommandError):
                self.run_lane(suite_status=status)
