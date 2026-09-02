#!/usr/bin/env python3
"""Check that CrownySharp bindings use the shared managed host contract."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

from generate_managed_interop import expand_host_functions


ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "Scripts" / "managed" / "managed-interop.json"
SHARP_SOURCE = ROOT / "Crowny-Sharp" / "Source"
HOST_IMPLEMENTATION = (
    ROOT / "Crowny" / "Source" / "Crowny" / "Scripting" / "Managed" / "Interop" / "ManagedHostBindings.cpp"
)
SCRIPTING_SOURCE = ROOT / "Crowny" / "Source" / "Crowny" / "Scripting"
CORECLR_BACKEND = (
    ROOT / "Crowny" / "Source" / "Crowny" / "Scripting" / "Backends" / "CoreCLR" / "CoreClrBackend.cpp"
)

RUNTIME_HELPERS = {
    "AddComponent",
    "CreateAsset",
    "GetComponent",
    "HasComponent",
    "Push",
    "RemoveComponent",
    "ResolveRegisteredScriptComponent",
    "ReleaseAsset",
    "SetNativeHostApi",
    "SetScriptResolver",
}

ALLOWED_INTERNAL_CALLS = {
    Path("ScriptObject.cs"): 1,
    Path("Runtime/ManagedRuntimeContext.cs"): 1,
    Path("Runtime/Mono/ManagedRuntimeAdapter.Mono.cs"): 1,
}

TEST_ONLY_INTERNAL_CALLS = {
    # Registered only by the opt-in native benchmark test to compare direct
    # Mono dispatch with the shared host table. No engine runtime registers it.
    Path("Runtime/ManagedBindingBenchmark.cs"): 1,
}

ALLOWED_NATIVE_INTERNAL_CALLS = {
    Path("ScriptObject.cpp"): 1,
    Path("Backends/Mono/MonoBackend.cpp"): 2,
}

HOST_CALL = re.compile(r"\bManagedRuntimeContext\.([A-Z][A-Za-z0-9_]*)\s*\(")
INTERNAL_CALL = re.compile(r"\[MethodImpl\(MethodImplOptions\.InternalCall\)\]")
NATIVE_INTERNAL_CALL = re.compile(r"->AddInternalCall\s*\(")
RUNTIME_CONDITIONAL = re.compile(r"^\s*#if\s+!?CROWNY_MONO\b", re.MULTILINE)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--allow-legacy", action="store_true", help="report legacy internal calls without failing")
    arguments = parser.parse_args()

    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    functions = set(expand_host_functions(manifest))
    failures: list[str] = []
    legacy: list[tuple[Path, int]] = []

    if "hostBindings" in manifest:
        failures.append("the obsolete numeric hostBindings dispatcher is present in the manifest")

    native_text = HOST_IMPLEMENTATION.read_text(encoding="utf-8")
    for function in sorted(functions):
        if re.search(rf"\b{re.escape(function)}\b", native_text) is None:
            failures.append(f"manifest function has no shared native implementation: {function}")

    for path in sorted(SCRIPTING_SOURCE.rglob("*.cpp")):
        relative = path.relative_to(SCRIPTING_SOURCE)
        registrations = len(NATIVE_INTERNAL_CALL.findall(path.read_text(encoding="utf-8")))
        allowed = ALLOWED_NATIVE_INTERNAL_CALLS.get(relative, 0)
        if registrations < allowed:
            failures.append(f"required native runtime adapter registration is missing: {relative}")
        elif registrations > allowed:
            failures.append(f"feature InternalCall registrations remain in native scripting code: {relative}:{registrations - allowed}")

    coreclr_text = CORECLR_BACKEND.read_text(encoding="utf-8")
    for marker in ("InvokeHostBinding", "ForwardTypedBinding", "CW_MANAGED_BINDING_"):
        if marker in coreclr_text:
            failures.append(f"obsolete CoreCLR feature dispatcher remains: {marker}")

    for path in sorted(SHARP_SOURCE.rglob("*.cs")):
        relative = path.relative_to(SHARP_SOURCE)
        text = path.read_text(encoding="utf-8")
        if relative.parts[:2] != ("Runtime", "Generated"):
            for call in HOST_CALL.findall(text):
                if call not in functions and call not in RUNTIME_HELPERS:
                    failures.append(f"CrownySharp calls an undeclared host function: {relative}:{call}")

        internal_calls = len(INTERNAL_CALL.findall(text))
        allowed = ALLOWED_INTERNAL_CALLS.get(relative, TEST_ONLY_INTERNAL_CALLS.get(relative, 0))
        if internal_calls < allowed:
            failures.append(f"required runtime adapter call is missing: {relative}")
        elif internal_calls > allowed:
            legacy.append((relative, internal_calls - allowed))

        if relative.parts[0] != "Runtime" and relative != Path("ScriptObject.cs") and RUNTIME_CONDITIONAL.search(text):
            failures.append(f"runtime conditional leaked into public CrownySharp code: {relative}")

    if legacy:
        count = sum(value for _, value in legacy)
        print(f"legacy managed internal calls: {count} in {len(legacy)} files")
        for path, value in legacy:
            print(f"  {path}: {value}")
        if not arguments.allow_legacy:
            failures.append("legacy feature bindings remain outside the runtime adapters")

    for failure in failures:
        print(f"managed binding parity error: {failure}")
    if failures:
        return 1
    print(f"managed binding parity check passed: {len(functions)} shared host functions")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
