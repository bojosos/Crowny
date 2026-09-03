import json
import os
import time

from . import build as build_module
from . import env, log

SCENARIOS = ("Clean", "NoOp", "TouchedSource")

REPRESENTATIVE_SOURCES = {
    "Engine": "Crowny/Source/Platform/Windows/WindowsPlatformUtils.cpp",
    "Editor": "Crowny-Editor/Source/UI/UIUtils.cpp",
    "Tests": "Crowny-Tests/Source/StringUtilsTests.cpp",
    "RenderTests": "Crowny-RenderTests/Source/RenderTestRunner.cpp",
    "All": "Crowny/Source/Platform/Windows/WindowsPlatformUtils.cpp",
}


def measure(
    root=None,
    scenarios=SCENARIOS,
    target="Tests",
    configuration="Release",
    sanitizer="None",
    jobs=0,
    simd="avx2",
    compiler_cache="None",
):
    root = root or env.repo_root()
    for scenario in scenarios:
        if scenario not in SCENARIOS:
            raise ValueError(f"Unsupported measurement scenario: {scenario}")

    representative = root / REPRESENTATIVE_SOURCES[target]
    if not representative.is_file():
        raise RuntimeError(f"Representative source file was not found: {representative}")

    def run_measurement(name, clean=False):
        started = time.time()
        metrics = build_module.build(
            root=root,
            target=target,
            configuration=configuration,
            sanitizer=sanitizer,
            jobs=jobs,
            simd=simd,
            compiler_cache=compiler_cache,
            profile=True,
            clean=clean,
        )
        return {
            "scenario": name,
            "wallSeconds": round(time.time() - started, 3),
            "binlog": metrics.get("binlog"),
            "peakCompilerWorkingSetBytes": metrics.get("peakCompilerWorkingSetBytes", 0),
            "phases": metrics.get("phases", {}),
        }

    results = []
    original_write_time = representative.stat().st_mtime
    try:
        for scenario in scenarios:
            if scenario == "Clean":
                results.append(run_measurement("Clean", clean=True))
            elif scenario == "NoOp":
                log.info("Priming the no-op scenario...")
                build_module.build(
                    root=root,
                    target=target,
                    configuration=configuration,
                    sanitizer=sanitizer,
                    jobs=jobs,
                    simd=simd,
                    compiler_cache=compiler_cache,
                )
                results.append(run_measurement("NoOp"))
            elif scenario == "TouchedSource":
                log.info("Priming the touched-source scenario...")
                build_module.build(
                    root=root,
                    target=target,
                    configuration=configuration,
                    sanitizer=sanitizer,
                    jobs=jobs,
                    simd=simd,
                    compiler_cache=compiler_cache,
                )
                os.utime(representative, (time.time() + 2, time.time() + 2))
                results.append(run_measurement("TouchedSource"))
    finally:
        os.utime(representative, (original_write_time, original_write_time))

    stamp = (
        time.strftime("%Y%m%d-%H%M%S", time.gmtime())
        + f"-{int((time.time() % 1) * 1000):03d}-measurement"
    )
    summary_root = root / "artifacts" / "build-metrics" / stamp
    summary_root.mkdir(parents=True, exist_ok=True)
    summary = {
        "target": target,
        "configuration": configuration,
        "sanitizer": sanitizer,
        "jobs": jobs,
        "compilerCache": compiler_cache,
        "representativeSource": str(representative),
        "results": results,
    }
    summary_path = summary_root / "summary.json"
    summary_path.write_text(json.dumps(summary, indent=2), encoding="utf-8")
    for result in results:
        log.info(
            f"{result['scenario']}: {result['wallSeconds']}s "
            f"(peak compiler working set {result['peakCompilerWorkingSetBytes']} bytes)"
        )
    log.info(f"Measurement summary: {summary_path}")
    return summary
