import os
import subprocess
import sys
from pathlib import Path

from . import build as build_module
from . import cmd, env, locks, log, managed
from .build import output_dir


def test_executable(root, output_configuration):
    test_output = output_dir(root, output_configuration) / "Crowny-Tests"
    executable = test_output / ("Crowny-Tests.exe" if sys.platform == "win32" else "Crowny-Tests")
    if not executable.is_file():
        raise RuntimeError(f"Test executable was not found: {executable}")
    return executable, test_output


def _runtime_environment(root, test_output):
    runtime_env = os.environ.copy()
    prepend = [
        str(Path(os.environ["VULKAN_SDK"]) / ("Bin" if sys.platform == "win32" else "lib")),
        str(Path(os.environ.get("CROWNY_MONO_ROOT", env.default_mono_root(root))) / "bin"),
        str(test_output),
    ]
    runtime_env["PATH"] = os.pathsep.join(prepend + [runtime_env.get("PATH", "")])
    return runtime_env


def _list_tests(executable, runtime_env, tag, cwd):
    result = subprocess.run(
        [str(executable), "--list-tests", tag, "--verbosity", "quiet"],
        cwd=str(cwd),
        env=runtime_env,
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        raise RuntimeError("Failed to enumerate the hidden process-isolated test lane.")
    return [line for line in result.stdout.splitlines() if line.strip()]


def run(
    root=None,
    configuration="Release",
    sanitizer="None",
    jobs=0,
    clean=False,
    profile=False,
    compiler_cache="None",
    simd="avx2",
    process_isolated=False,
    filter="",
):
    root = root or env.repo_root()
    build_module.build(
        root=root,
        target="Tests",
        configuration=configuration,
        sanitizer=sanitizer,
        jobs=jobs,
        clean=clean,
        profile=profile,
        compiler_cache=compiler_cache,
        simd=simd,
    )

    if process_isolated:
        managed.ensure(root, configuration)

    workspace_config = env.workspace_configuration(configuration, sanitizer)
    output_configuration = env.output_configuration(configuration, sanitizer)

    original_asan = os.environ.get("ASAN_OPTIONS")
    with locks.output_read_lock(root, workspace_config):
        executable, test_output = test_executable(root, output_configuration)
        runtime_env = _runtime_environment(root, test_output)
        try:
            if sanitizer == "Address":
                os.environ["ASAN_OPTIONS"] = "abort_on_error=1:halt_on_error=1:strict_string_checks=1"
                runtime_env["ASAN_OPTIONS"] = os.environ["ASAN_OPTIONS"]

            if process_isolated:
                log.info("Running the hidden process-isolated test lane...")
                for isolated_test in _list_tests(
                    executable, runtime_env, "[.ProcessIsolated]~[Benchmark]", root
                ):
                    log.info(f"Running isolated test: {isolated_test}")
                    cmd.run_checked([str(executable), isolated_test], cwd=root, env=runtime_env)

            arguments = [str(executable)]
            if filter:
                arguments.append(filter)
            log.info("Running Crowny-Tests from the repository root...")
            cmd.run_checked(arguments, cwd=root, env=runtime_env)
        finally:
            if sanitizer == "Address":
                if original_asan is None:
                    os.environ.pop("ASAN_OPTIONS", None)
                else:
                    os.environ["ASAN_OPTIONS"] = original_asan
