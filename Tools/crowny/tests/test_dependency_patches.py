import _bootstrap  # noqa: F401

import unittest
from pathlib import Path
from subprocess import CompletedProcess
from unittest.mock import patch

from crowny import bootstrap


class DependencyPatchTests(unittest.TestCase):
    def test_clean_checkout_applies_patch(self):
        with patch.object(bootstrap.subprocess, "run", return_value=CompletedProcess([], 0)) as check, patch.object(
            bootstrap.cmd, "run_checked"
        ) as apply:
            bootstrap.apply_dependency_patches(Path("checkout"), "git")
            self.assertEqual(check.call_count, 1)
            apply.assert_called_once()
            self.assertNotIn("--reverse", apply.call_args.args[0])

    def test_patched_checkout_is_unchanged(self):
        results = [CompletedProcess([], 1, stderr="already applied"), CompletedProcess([], 0)]
        with patch.object(bootstrap.subprocess, "run", side_effect=results) as check, patch.object(
            bootstrap.cmd, "run_checked"
        ) as apply:
            bootstrap.apply_dependency_patches(Path("checkout"), "git")
            self.assertIn("--reverse", check.call_args.args[0])
            apply.assert_not_called()

    def test_conflicting_checkout_is_preserved(self):
        with patch.object(bootstrap.subprocess, "run", return_value=CompletedProcess([], 1, stderr="conflict")), patch.object(
            bootstrap.cmd, "run_checked"
        ) as apply:
            with self.assertRaisesRegex(RuntimeError, "neither applies nor is already applied"):
                bootstrap.apply_dependency_patches(Path("checkout"), "git")
            apply.assert_not_called()
