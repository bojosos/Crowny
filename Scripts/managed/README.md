# Managed binding workflow

`managed-interop.json` is the transport contract shared by every Crowny managed backend. The generator emits the native ABI, the managed-host ABI, the CrownySharp transport, AOT roots, and linker roots. Gameplay-facing C# never selects Mono or CoreCLR for an engine operation.

The current ABI exposes 519 typed feature functions. Mono and CoreCLR consume the same table; the parity check prevents a backend-specific feature path from being added beside it.

To add a managed binding:

1. Add the typed function to `hostFunctions` in `managed-interop.json`.
2. Implement a native function with the same PascalCase name under `Crowny/Scripting/Managed`. It receives stable entity or asset identities, plain values, strings, or explicit buffers. It must not accept a Mono or CoreCLR object.
3. Call the generated `ManagedRuntimeContext` method from the public CrownySharp member.
4. Run `python Scripts/managed/generate_managed_interop.py`, then run both checks below.

```powershell
python Scripts/managed/generate_managed_interop.py --check
python Scripts/managed/check_managed_binding_parity.py
```

The parity check rejects undeclared calls, missing native implementations, backend conditionals in public feature code, and feature-level `InternalCall` declarations. `--allow-legacy` is only for measuring the current migration. Do not use it as a release gate.

The three runtime hooks are separate from feature bindings. Mono acquires the host table and resolves managed script instances through small runtime adapters. `ScriptObject` also retains its Mono finalization hook while native wrapper ownership remains in the Mono backend. New engine features must not add code to those adapters.

## Shared scripting policy

`Crowny-Sharp/Source/Runtime` is the single implementation of managed member discovery, inspector visibility, lifecycle callback discovery, `RequireComponent`, script-catalog generation, and script-state serialization. Both runtime adapters call that code. A backend must not recreate those rules with Mono metadata or CoreCLR reflection.

Managed state JSON contains the raw `Fields` payload consumed by both runtime adapters plus one native `Metadata` map. The metadata makes nested kinds and declared types self-describing without changing the payload applied by C#. Decimal values are invariant strings; vectors, colors, quaternions, and matrices are numeric arrays; entity, component, asset, and UUID references are UUID strings. Runtime state is normalized against exact current catalog identities and member names; old schema aliases are outside the runtime contract.

Mono reflection is limited to `Backends/Mono/MonoBindingRegistry` and runtime wrapper dispatch. It is not used for managed-script discovery, scene state, undo, inspection, or reload snapshots. The retired `SerializableObject` graph and Mono-only inspector no longer exist. Previously compiled CrownySharp assemblies and pre-format-11 scenes are not supported; a future compatibility promise must be implemented as an explicit import adapter rather than folded back into the runtime-neutral model.
