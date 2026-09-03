import os
import sys
import time
from pathlib import Path

from . import cmd, env, hashing, locks, log, stamps

_FASTNOISE_UNSUPPORTED = "private const short OPTIMISE = 512;"
_FASTNOISE_COMPATIBLE = "private const short OPTIMISE = 0;"

_MANAGED_DEFINES = {"Debug": "CW_DEBUG", "Release": "CW_RELEASE", "Dist": "CW_DIST"}


def _fastnoise_source(root):
    source = root / "Crowny" / "Dependencies" / "FastNoiseLite" / "CSharp" / "FastNoiseLite.cs"
    contents = source.read_text(encoding="utf-8")
    if _FASTNOISE_UNSUPPORTED not in contents:
        return source
    generated_root = root / ".deps" / "generated"
    generated_root.mkdir(parents=True, exist_ok=True)
    generated = generated_root / "FastNoiseLite.Mono.cs"
    compatible = contents.replace(_FASTNOISE_UNSUPPORTED, _FASTNOISE_COMPATIBLE)
    if not generated.is_file() or generated.read_text(encoding="utf-8") != compatible:
        generated.write_text(compatible, encoding="utf-8", newline="")
    return generated


def _collect_sources(source_root):
    return sorted(path for path in source_root.rglob("*.cs") if path.is_file())


def _mcs(mono_root):
    mcs = Path(mono_root) / "bin" / ("mcs.bat" if sys.platform == "win32" else "mcs")
    if not mcs.is_file():
        raise RuntimeError(f"Mono C# compiler was not found: {mcs}")
    return mcs


def _fingerprint(configuration, managed_define, emit_debug_symbols, mcs, mono_root, engine_sources, game_sources):
    return hashing.content_hash(
        files=[mcs, Path(mono_root) / "bin" / "mono.exe"] + engine_sources + game_sources,
        values=[
            f"configuration={configuration}",
            f"defines=CROWNY_MONO,{managed_define}",
            "langversion=7.2",
            "unsafe=true",
            f"debug={emit_debug_symbols}",
            f"optimize={not emit_debug_symbols}",
        ],
    )


def ensure(root=None, configuration="Release"):
    root = root or env.repo_root()
    if configuration not in _MANAGED_DEFINES:
        raise ValueError(f"Unsupported managed configuration: {configuration}")
    mono_root = os.environ.get("CROWNY_MONO_ROOT") or env.default_mono_root(root)
    mcs = _mcs(mono_root)

    engine_sources = _collect_sources(root / "Crowny-Sharp" / "Source")
    game_sources = _collect_sources(root / "Crowny-Sandbox" / "Source")
    fastnoise = _fastnoise_source(root)
    engine_sources.append(fastnoise)

    output_root = root / ".deps" / "generated" / "managed" / configuration
    engine_assembly = output_root / "CrownySharp.dll"
    game_assembly = output_root / "GameAssembly.dll"
    managed_define = _MANAGED_DEFINES[configuration]
    emit_debug_symbols = configuration == "Debug"

    outputs = [engine_assembly, game_assembly]
    if emit_debug_symbols:
        outputs += [Path(str(engine_assembly) + ".mdb"), Path(str(game_assembly) + ".mdb")]
    stale_symbols = [
        Path(str(engine_assembly) + suffix)
        for suffix in (".mdb", ".pdb")
    ] + [Path(str(game_assembly) + suffix) for suffix in (".mdb", ".pdb")]

    os.environ["CROWNY_MANAGED_ASSEMBLY_ROOT"] = str(output_root)

    fingerprint = _fingerprint(
        configuration, managed_define, emit_debug_symbols, mcs, mono_root, engine_sources, game_sources
    )
    stamp_name = f"managed-{configuration.lower()}.json"
    outputs_missing = [path for path in outputs if not path.is_file()]
    if not outputs_missing and stamps.fingerprint_matches(stamp_name, fingerprint, root):
        log.info("Managed assemblies are current.")
        return engine_assembly

    with locks.exclusive_lock(root, "managed-assemblies"):
        fingerprint = _fingerprint(
            configuration, managed_define, emit_debug_symbols, mcs, mono_root, engine_sources, game_sources
        )
        outputs_missing = [path for path in outputs if not path.is_file()]
        if not outputs_missing and stamps.fingerprint_matches(stamp_name, fingerprint, root):
            log.info("Managed assemblies are current.")
            return engine_assembly

        output_root.mkdir(parents=True, exist_ok=True)
        common = [
            str(mcs),
            "-debug+" if emit_debug_symbols else "-debug-",
            "-optimize-" if emit_debug_symbols else "-optimize+",
            "-langversion:7.2",
        ]
        log.info("Building CrownySharp.dll...")
        cmd.run_checked(
            common
            + ["-unsafe", f"-define:CROWNY_MONO,{managed_define}", "-target:library", f"-out:{engine_assembly}"]
            + [str(path) for path in engine_sources]
        )
        log.info("Building GameAssembly.dll...")
        cmd.run_checked(
            common
            + [
                f"-define:{managed_define}",
                "-target:library",
                f"-lib:{engine_assembly.parent}",
                "-reference:CrownySharp.dll",
                f"-out:{game_assembly}",
            ]
            + [str(path) for path in game_sources]
        )
        if not emit_debug_symbols:
            for path in stale_symbols:
                try:
                    path.unlink()
                except FileNotFoundError:
                    pass
        stamps.fingerprint_stamp(
            stamp_name,
            fingerprint,
            root,
            extra={"builtUtc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())},
        )
    return engine_assembly
