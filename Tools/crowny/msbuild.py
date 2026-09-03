import subprocess
import sys
import threading
import time
from pathlib import Path

from . import cmd, env, log

_VSWHERE_QUERY = [
    "-latest",
    "-products",
    "*",
    "-requires",
    "Microsoft.Component.MSBuild",
    "-find",
    "MSBuild\\**\\Bin\\MSBuild.exe",
]


def _vswhere(root):
    vswhere = root / "3rdparty" / "vswhere" / "vswhere.exe"
    if not vswhere.is_file():
        raise RuntimeError(f"vswhere was not found at {vswhere}. Initialize the repository submodules first.")
    return vswhere


def find_msbuild(root=None):
    root = root or env.repo_root()
    if sys.platform != "win32":
        raise RuntimeError("MSBuild is only available on Windows; use the gmake2 flow elsewhere.")
    result = cmd.run_checked([str(_vswhere(root))] + _VSWHERE_QUERY, capture=True)
    candidates = [line.strip() for line in result.stdout.splitlines() if line.strip()]
    if not candidates:
        raise RuntimeError("Visual Studio 2022 Build Tools with C++ support is required.")
    return Path(candidates[0])


def find_msvc_compiler(root=None):
    root = root or env.repo_root()
    if sys.platform != "win32":
        raise RuntimeError("The MSVC compiler is only available on Windows.")
    result = cmd.run_checked(
        [
            str(_vswhere(root)),
            "-latest",
            "-products",
            "*",
            "-requires",
            "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
            "-find",
            "VC\\Tools\\MSVC\\**\\bin\\Hostx64\\x64\\cl.exe",
        ],
        capture=True,
    )
    candidates = sorted(
        (line.strip() for line in result.stdout.splitlines() if line.strip()), reverse=True
    )
    if not candidates:
        raise RuntimeError("The x64 MSVC compiler was not found.")
    return Path(candidates[0])


def _peak_working_set_sampler(build_started_epoch, peak_holder, stop_event):
    query = (
        "$start = [DateTimeOffset]::FromUnixTimeSeconds(%d).LocalDateTime; "
        "$sample = Get-Process -Name cl, c1xx -ErrorAction SilentlyContinue | "
        "Where-Object { $_.StartTime -ge $start } | "
        "Measure-Object WorkingSet64 -Sum; "
        "if ($sample.Count) { $sample.Sum } else { 0 }" % int(build_started_epoch)
    )
    while not stop_event.is_set():
        try:
            result = subprocess.run(
                ["powershell", "-NoProfile", "-Command", query],
                capture_output=True,
                text=True,
                check=False,
                creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
            )
            if result.returncode == 0:
                value = int(float(result.stdout.strip() or 0))
                if value > peak_holder[0]:
                    peak_holder[0] = value
        except (OSError, ValueError):
            pass
        stop_event.wait(0.5)


def build(
    msbuild,
    solution,
    targets,
    configuration,
    jobs,
    platform="Win64",
    clean=False,
    binlog_path=None,
    sccache=False,
    scripts_dir=None,
    collect_profile=False,
    build_project_references=True,
):
    target_actions = [f"{t}:Rebuild" if clean else t for t in targets]
    arguments = [
        str(msbuild),
        str(solution),
        "/nologo",
        "/v:minimal",
        "/m:1",
        "/nodeReuse:false",
        "/p:UseMultiToolTask=false",
        f"/p:CL_MPCount={jobs}",
        f"/p:Configuration={configuration}",
        f"/p:Platform={platform}",
        f"/t:{';'.join(target_actions)}",
    ]
    if not build_project_references and not clean:
        arguments.append("/p:BuildProjectReferences=false")
    if binlog_path:
        arguments += [f"/bl:{binlog_path}", "/clp:PerformanceSummary"]
    if sccache:
        arguments += [
            "/p:CLToolExe=sccache-cl.cmd",
            f"/p:CLToolPath={scripts_dir}",
            "/p:DebugInformationFormat=OldStyle",
        ]

    printable = " ".join(arguments[1:])
    log.info(f"> {Path(msbuild).name} {printable}")

    if not collect_profile:
        result = subprocess.run(arguments, check=False)
        if result.returncode != 0:
            raise cmd.CommandError(
                f"MSBuild failed with exit code {result.returncode}.", result.returncode, arguments
            )
        return {"exit_code": 0, "peak_compiler_working_set_bytes": 0}

    build_started = time.time() - 1
    peak = [0]
    stop = threading.Event()
    sampler = threading.Thread(
        target=_peak_working_set_sampler, args=(build_started, peak, stop), daemon=True
    )
    sampler.start()
    try:
        process = subprocess.Popen(arguments)
        exit_code = process.wait()
    finally:
        stop.set()
        sampler.join(timeout=2)
    if exit_code != 0:
        raise cmd.CommandError(f"MSBuild failed with exit code {exit_code}.", exit_code, arguments)
    return {"exit_code": exit_code, "peak_compiler_working_set_bytes": peak[0]}
