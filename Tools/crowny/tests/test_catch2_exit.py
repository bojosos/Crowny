import subprocess
import sys
import unittest
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[3] / "Scripts" / "check_catch2_exit.py"


class Catch2ExitTests(unittest.TestCase):
    def check_status(self, status, allow_skipped=False):
        command = [sys.executable, str(SCRIPT), str(status)]
        if allow_skipped:
            command.append("--allow-all-skipped")
        return subprocess.run(command, input="SKIPPED: optional benchmark\n", text=True, capture_output=True)

    def test_pass_succeeds(self):
        self.assertEqual(self.check_status(0).returncode, 0)

    def test_all_skipped_requires_explicit_opt_in(self):
        self.assertNotEqual(self.check_status(4).returncode, 0)
        self.assertEqual(self.check_status(4, allow_skipped=True).returncode, 0)

    def test_skipped_output_never_masks_failures_or_timeouts(self):
        for status in (1, 2, 3, 5, 124, 137, 139, -1073741819):
            with self.subTest(status=status):
                self.assertNotEqual(self.check_status(status, allow_skipped=True).returncode, 0)


if __name__ == "__main__":
    unittest.main()
