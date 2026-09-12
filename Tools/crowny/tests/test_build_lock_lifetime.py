import _bootstrap  # noqa: F401

from contextlib import contextmanager
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from crowny import build as build_module


class BuildLockLifetimeTests(unittest.TestCase):
    def test_output_locks_cover_compilation_and_release_on_failure(self):
        for fail in (False, True):
            with self.subTest(fail=fail), tempfile.TemporaryDirectory() as directory:
                active = set()

                @contextmanager
                def output_lock(root, configuration):
                    active.add(configuration)
                    try:
                        yield mock.Mock()
                    finally:
                        active.remove(configuration)

                def compile(*args, **kwargs):
                    self.assertEqual(active, {"Release"})
                    if fail:
                        raise RuntimeError("compiler failed")
                    return {"exit_code": 0, "peak_compiler_working_set_bytes": 0}

                with mock.patch.object(build_module.sys, "platform", "win32"), mock.patch.object(
                    build_module.env, "configure_default_environment"
                ), mock.patch.object(build_module.premake, "ensure_projects"), mock.patch.object(
                    build_module.locks, "output_write_lock", side_effect=output_lock
                ), mock.patch.object(build_module.locks, "project_read_lock"), mock.patch.object(
                    build_module.locks, "compiler_lease"
                ) as lease, mock.patch.object(
                    build_module.msbuild, "find_msbuild", return_value="MSBuild.exe"
                ), mock.patch.object(build_module, "run_msbuild", side_effect=compile), mock.patch.dict(
                    "os.environ", {"CROWNY_OUTPUT_WRITE_LOCK": ""}
                ):
                    lease.return_value.__enter__.return_value = {"jobs": 2, "budget": 12}
                    if fail:
                        with self.assertRaisesRegex(RuntimeError, "compiler failed"):
                            build_module.build(root=Path(directory), target="Engine", jobs=2)
                    else:
                        build_module.build(root=Path(directory), target="Engine", jobs=2)
                self.assertEqual(active, set())


if __name__ == "__main__":
    unittest.main()
