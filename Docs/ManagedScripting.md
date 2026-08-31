# Managed scripting

Crowny exposes one backend-neutral `ManagedScriptComponent`, `ManagedScript`, and `ScriptState` model. Mono and CoreCLR
own their runtime instances, callbacks, loading, and reload mechanics behind `ManagedScripting`; neither runtime object
leaks into scene data or editor code. Mono remains the editor default during the transition. CoreCLR is an opt-in desktop
preset until the remaining editor workflow, debugging, and operational gates pass. Public engine calls use the same
generated 519-function contract on both backends.

## Desktop CoreCLR package

CoreCLR uses a process-lifetime `Crowny.ManagedHost` bootstrap and a collectible game `AssemblyLoadContext`. The player
bundle contains a complete private .NET installation; the managed host itself remains a framework-dependent component.
The package does not depend on a machine-wide .NET installation.

Bootstrap the SDK pinned by `global.json` into the ignored `.deps/dotnet` directory, then publish one runtime identifier at
a time. The publish script also runs this bootstrap automatically when the local SDK is missing:

```powershell
Scripts\setup-dotnet.ps1

Scripts\managed\publish-coreclr.ps1 `
    -RuntimeIdentifier win-x64 `
    -GameProject Crowny-Sandbox\GameAssembly.Managed.csproj `
    -GameAssemblyName GameAssembly `
    -OutputDirectory C:\packages\CrownyGame\managed
```

`Scripts\setup-windows.ps1 -CoreCLR` performs the same bootstrap during normal Windows setup. Pass `RuntimeRoot`,
`RuntimeVersion`, or `DotNetExecutable` to `publish-coreclr.ps1` only when packaging a separately serviced runtime.

The output directory must be empty. `managed-program.json` records the ABI version, private runtime root, and the six
required host and game artifacts. All paths are package-relative. The native loader canonicalizes every path and rejects
absolute paths, parent traversal, symlink escapes, missing artifacts, a mismatched ABI, and an incomplete private runtime.

`RuntimeVersion` must name a directory present under both `host/fxr` and `shared/Microsoft.NETCore.App` in the supplied
runtime root. The same version selects the exact `Microsoft.NETCore.App.Host.<RID>` package that supplies `nethost`; the
script restores it into `.deps/nuget-packages` when the SDK does not contain the matching pack. Cross-architecture
packaging requires an explicit matching `RuntimeRoot`. Service the runtime, host pack, and build input together.

## Reload and serialized data

A reload captures every live script through the runtime-neutral state model, unloads the old collectible context, loads
and validates the replacement catalog, retains exact-name fields with compatible kinds, recreates instances, and restores state. If replacement
fails, the adapter reloads the previous program and restores the same snapshots. If rollback also fails, it clears the
invalid runtime state and reports both failures.

The shared C# codec emits recursive kind and declared-type metadata for fields, object members, collection elements, and
dictionary entries. Assembly-qualified nested type names use `Outer+Inner`. Missing scripts retain their complete state
because no catalog normalization occurs until the script type is available. Once a type is loaded, unknown, renamed, or
kind-incompatible members are intentionally dropped rather than carried as an implicit compatibility layer.

Scene format 12 is based exclusively on `ScriptState`. YAML uses `ManagedScriptComponent`; binary scenes store the same
JSON state payload. It is also the only scene format accepted by this build. Per-component version gates, YAML aliases,
and legacy text, physics, and managed-script readers have been removed. Regenerate project scenes when adopting this
version; compatibility can return later as an explicit import tool without complicating the runtime scene codec.

## Compatibility policy

- Gameplay source is recompiled for the selected backend. Scene files must be regenerated as format 11. Script and member
  identities are exact; renames require updating or regenerating affected scene data. The managed
  component keeps its numeric scene ID, while its YAML name and payload intentionally changed.
- The CoreCLR catalog supports public fields and properties, or non-public members marked `[SerializeField]`. It excludes
  static, indexed, `[DontSerializeField]`, and unsupported member types.
- Script callbacks use exact signatures. A same-named overload does not become a lifecycle or collision callback.
- CoreCLR is not the editor default while inspector workflows, debugging, and the full shared contract suite remain
  incomplete. Public engine-call binding parity is enforced statically for both backends.
- Browser builds use the dedicated .NET WebAssembly interpreter or AOT presets. Native AOT remains an evidence-gated,
  closed-world desktop player option. Neither preset is a substitute for the CoreCLR editor host.
- Crowny makes no named-console support claim without vendor SDK, runtime, licensing, and certification evidence.

## Required verification

Before promoting CoreCLR to the editor default, run the generated-ABI check, managed publishes, native contract tests,
format-11 scene round trips, missing-script retention, repeated unload checks, exception-stack checks, inspector edits,
lifecycle and collision ordering, public binding marshalling, and private-runtime package launch. Record startup, callback,
allocation, reload, memory, build-time, and artifact-size measurements against Mono.
