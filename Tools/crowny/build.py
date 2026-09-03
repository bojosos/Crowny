import json
import os
import shutil
import sys
import time
from pathlib import Path

from . import cmd, env, locks, log, msbuild, premake
from .msbuild import build as run_msbuild

SOLUTION_TARGETS = {
    "Engine": ["Crowny"],
    "Editor": ["Crowny-Editor"],
    "Tests": ["Crowny-Tests"],
    "RenderTests": ["Crowny-RenderTests"],
    "All": ["Crowny", "Crowny-Editor", "Crowny-Builder", "Crowny-Tests", "Crowny-RenderTests"],
}

TARGETS = tuple(SOLUTION_TARGETS)
COMPILER_CACHES = ("None", "Sccache")


def validate_target(target):
    if target not in TARGETS:
        raise ValueError(f"Unsupported target: {target}")
    return target


def validate_compiler_cache(compiler_cache):
    if compiler_cache not in COMPILER_CACHES:
        raise ValueError(f"Unsupported compiler cache: {compiler_cache}")
    return compiler_cache


def output_dir(root, output_configuration):
    return root / "bin" / f"{output_configuration}-{env.platform_tag()}"


def _sccache_ready(root):
    sccache = Path(os.environ["SCCACHE_EXE"]) if os.environ.get("SCCACHE_EXE") else shutil.which("sccache")
    probe = root / ".deps" / "stamps" / "sccache-msbuild.json"
    if not sccache or not probe.is_file():
        raise RuntimeError(
            "sccache is not enabled. Install it and run Scripts/probe-sccache-windows.ps1 first."
        )
    try:
        with open(probe, "r", encoding="utf-8-sig") as handle:
            if not json.load(handle).get("passed"):
                raise RuntimeError("The recorded sccache MSBuild feasibility probe did not pass.")
    except ValueError:
        raise RuntimeError("The sccache probe stamp is corrupted; rerun the probe.")
    return sccache


def _gmake_config(configuration):
    return f"{configuration.lower()}_linux64"


def _build_posix(root, target, configuration, jobs, clean, metrics):
    targets = SOLUTION_TARGETS[target]
    arguments = ["make"] + targets + [f"config={_gmake_config(configuration)}"]
    if clean:
        arguments.insert(1, "clean")
    if jobs:
        arguments.append(f"-j{jobs}")
    log.info(f"Building {target} ({_gmake_config(configuration)})...")
    started = time.time()
    cmd.run_checked(arguments, cwd=root)
    metrics["phases"]["nativeBuildSeconds"] = round(time.time() - started, 3)


def build(
    root=None,
    target="Engine",
    configuration="Release",
    sanitizer="None",
    jobs=0,
    clean=False,
    profile=False,
    compiler_cache="None",
    simd="avx2",
    inner_loop=False,
):
    root = root or env.repo_root()
    validate_target(target)
    validate_compiler_cache(compiler_cache)
    workspace_config = env.workspace_configuration(configuration, sanitizer)
    output_configs = env.build_output_configurations(workspace_config)

    env.configure_default_environment(root)
    from . import managed, resources

    metrics = {
        "target": target,
        "configuration": configuration,
        "sanitizer": sanitizer,
        "workspaceConfiguration": workspace_config,
        "requestedJobs": jobs,
        "jobs": 0,
        "compilerCache": compiler_cache,
        "clean": bool(clean),
        "outputLocks": output_configs,
        "startedUtc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "phases": {},
    }
    overall_started = time.time()

    lock_identity = f"{os.getpid()}|{','.join(output_configs)}"
    original_marker = os.environ.get("CROWNY_OUTPUT_WRITE_LOCK")
    owns_output_lock = original_marker != lock_identity
    acquired_locks = []
    profile_root = None

    try:
        if owns_output_lock:
            for output_config in output_configs:
                acquired_locks.append(
                    locks.output_write_lock(root, output_config).__enter__()
                )
            os.environ["CROWNY_OUTPUT_WRITE_LOCK"] = lock_identity

        started = time.time()
        premake.ensure_projects(root, simd=simd)
        metrics["phases"]["projectGenerationSeconds"] = round(time.time() - started, 3)

        with locks.project_read_lock(root), locks.compiler_lease(root, jobs) as lease:
            effective_jobs = lease["jobs"]
            metrics["jobs"] = effective_jobs
            job_mode = "auto" if jobs == 0 else f"requested {jobs}"

            original_real_cl = os.environ.get("CROWNY_REAL_CL")
            original_buster = os.environ.get("SCCACHE_C_CUSTOM_CACHE_BUSTER")
            using_sccache = compiler_cache == "Sccache"
            if using_sccache:
                _sccache_ready(root)
                os.environ["CROWNY_REAL_CL"] = str(msbuild.find_msvc_compiler(root))
                os.environ["SCCACHE_C_CUSTOM_CACHE_BUSTER"] = f"Crowny-{workspace_config}"

            binlog_path = None
            if profile:
                stamp = (
                    time.strftime("%Y%m%d-%H%M%S", time.gmtime())
                    + f"-{int((time.time() % 1) * 1000):03d}"
                )
                profile_root = root / "artifacts" / "build-metrics" / f"{stamp}-{target.lower()}-{workspace_config.lower()}"
                profile_root.mkdir(parents=True, exist_ok=True)
                binlog_path = profile_root / "msbuild.binlog"

            log.info(
                f"Building {target} ({workspace_config}|Win64) with {effective_jobs} compiler worker(s) "
                f"({job_mode}, {lease['budget']} total budget)..."
            )
            if sys.platform == "win32":
                started = time.time()
                result = run_msbuild(
                    msbuild.find_msbuild(root),
                    root / "Crowny.sln",
                    SOLUTION_TARGETS[target],
                    workspace_config,
                    effective_jobs,
                    clean=clean,
                    binlog_path=binlog_path,
                    sccache=using_sccache,
                    scripts_dir=root / "Scripts",
                    collect_profile=profile,
                    build_project_references=not inner_loop,
                )
                metrics["phases"]["nativeBuildSeconds"] = round(time.time() - started, 3)
                metrics["peakCompilerWorkingSetBytes"] = result["peak_compiler_working_set_bytes"]
            else:
                _build_posix(root, target, configuration, effective_jobs, clean, metrics)

            if using_sccache:
                os.environ["CROWNY_REAL_CL"] = original_real_cl or ""
                if not original_real_cl:
                    os.environ.pop("CROWNY_REAL_CL", None)
                os.environ["SCCACHE_C_CUSTOM_CACHE_BUSTER"] = original_buster or ""
                if not original_buster:
                    os.environ.pop("SCCACHE_C_CUSTOM_CACHE_BUSTER", None)

            if target in ("Editor", "All"):
                started = time.time()
                managed.ensure(root, configuration)
                metrics["phases"]["managedBuildSeconds"] = round(time.time() - started, 3)

                started = time.time()
                resources.update(root, configuration, sanitizer)
                metrics["phases"]["editorResourcesSeconds"] = round(time.time() - started, 3)

        metrics["totalSeconds"] = round(time.time() - overall_started, 3)
        metrics["completedUtc"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())

        if using_sccache:
            stats = cmd.run_checked(["sccache", "--show-stats"], capture=True).stdout
            print(stats, end="")
            if profile:
                (profile_root / "sccache-stats.txt").write_text(stats, encoding="utf-8")
                metrics["sccacheStats"] = str(profile_root / "sccache-stats.txt")

        if profile:
            metrics["binlog"] = str(binlog_path)
            metrics_path = profile_root / "metrics.json"
            metrics_path.write_text(json.dumps(metrics, indent=2), encoding="utf-8")
            log.info(f"Build metrics: {metrics_path}")

    finally:
        if owns_output_lock:
            os.environ["CROWNY_OUTPUT_WRITE_LOCK"] = original_marker or ""
            if not original_marker:
                os.environ.pop("CROWNY_OUTPUT_WRITE_LOCK", None)
            for lock in reversed(acquired_locks):
                lock.release()

    return metrics
