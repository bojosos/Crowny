import json
import os
import re
import shutil
import subprocess
import time

from . import cmd, env, log, msbuild, stamps

_PROBE_PROJECT = """<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup Label="ProjectConfigurations">
    <ProjectConfiguration Include="Release|x64">
      <Configuration>Release</Configuration>
      <Platform>x64</Platform>
    </ProjectConfiguration>
  </ItemGroup>
  <PropertyGroup Label="Globals">
    <ProjectGuid>{7A67C4CA-B272-4AF7-A953-E93D6B591441}</ProjectGuid>
    <Keyword>Win32Proj</Keyword>
  </PropertyGroup>
  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.Default.props" />
  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Release|x64'" Label="Configuration">
    <ConfigurationType>StaticLibrary</ConfigurationType>
    <PlatformToolset>v143</PlatformToolset>
  </PropertyGroup>
  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.props" />
  <ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Release|x64'">
    <ClCompile>
      <DebugInformationFormat>OldStyle</DebugInformationFormat>
      <MultiProcessorCompilation>true</MultiProcessorCompilation>
    </ClCompile>
  </ItemDefinitionGroup>
  <ItemGroup><ClCompile Include="__SOURCE__" /></ItemGroup>
  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.targets" />
</Project>
"""

_ENV_KEYS = (
    "SCCACHE_DIR",
    "SCCACHE_SERVER_PORT",
    "CROWNY_REAL_CL",
    "SCCACHE_C_CUSTOM_CACHE_BUSTER",
)


def _cache_hits(sccache):
    result = subprocess.run([sccache, "--show-stats"], capture_output=True, text=True, check=False)
    found = re.findall(r"Cache hits\s+(\d+)", result.stdout)
    return max((int(hit) for hit in found), default=0)


def probe(root=None):
    root = root or env.repo_root()
    sccache = shutil.which("sccache")
    if not sccache:
        raise RuntimeError("sccache.exe is not installed or is not on PATH.")

    probe_root = (
        root
        / "artifacts"
        / "build-metrics"
        / ("sccache-probe-" + time.strftime("%Y%m%d-%H%M%S", time.gmtime()))
    )
    probe_root.mkdir(parents=True, exist_ok=True)
    source = probe_root / "probe.cpp"
    source.write_text("int CrownySccacheProbe() { return 42; }\n", encoding="utf-8", newline="")
    project = probe_root / "probe.vcxproj"
    project.write_text(
        _PROBE_PROJECT.replace("__SOURCE__", str(source)), encoding="utf-8", newline=""
    )

    real_compiler = msbuild.find_msvc_compiler(root)
    msbuild_exe = msbuild.find_msbuild(root)
    arguments = [
        str(msbuild_exe),
        str(project),
        "/nologo",
        "/v:minimal",
        "/m:1",
        "/nodeReuse:false",
        "/t:Rebuild",
        "/p:Configuration=Release",
        "/p:Platform=x64",
        "/p:CL_MPCount=1",
        "/p:CLToolExe=sccache-cl.cmd",
        f"/p:CLToolPath={root / 'Scripts'}",
    ]

    originals = {name: os.environ.get(name) for name in _ENV_KEYS}
    os.environ["SCCACHE_DIR"] = str(probe_root / "cache")
    os.environ["SCCACHE_SERVER_PORT"] = str(43000 + (os.getpid() % 1000))
    os.environ["CROWNY_REAL_CL"] = str(real_compiler)
    os.environ["SCCACHE_C_CUSTOM_CACHE_BUSTER"] = "Crowny-MSBuild-feasibility-probe"

    passed = False
    first_seconds = 0.0
    second_seconds = 0.0
    hits_after_first = 0
    hits_after_second = 0
    try:
        subprocess.run([sccache, "--zero-stats"], capture_output=True, check=False)

        started = time.time()
        cmd.run_checked(arguments)
        first_seconds = round(time.time() - started, 3)
        hits_after_first = _cache_hits(sccache)

        started = time.time()
        cmd.run_checked(arguments)
        second_seconds = round(time.time() - started, 3)
        hits_after_second = _cache_hits(sccache)
        passed = hits_after_second > hits_after_first
    finally:
        subprocess.run([sccache, "--stop-server"], capture_output=True, check=False)
        for name, value in originals.items():
            if value is None:
                os.environ.pop(name, None)
            else:
                os.environ[name] = value

    result = {
        "passed": passed,
        "firstBuildSeconds": first_seconds,
        "secondBuildSeconds": second_seconds,
        "hitsAfterFirst": hits_after_first,
        "hitsAfterSecond": hits_after_second,
        "compiler": str(real_compiler),
        "sccache": sccache,
        "completedUtc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
    }
    result_path = probe_root / "result.json"
    result_path.write_text(json.dumps(result, indent=2), encoding="utf-8")
    if not passed:
        raise RuntimeError(f"sccache did not produce a cache hit through MSBuild. Result: {result_path}")

    stamps.write_stamp(env.stamps_root(root) / "sccache-msbuild.json", result)
    log.info(f"sccache MSBuild feasibility probe passed: {result_path}")
    log.info("Keep it opt-in until a second clean rebuild reaches at least 70% hits and improves wall time by at least 30%.")
    return result
