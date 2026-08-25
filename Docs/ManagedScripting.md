# Managed scripting

Crowny keeps the serialized `MonoScriptComponent` name and numeric component ID while moving runtime work behind the
runtime-neutral `ManagedScripting` module. Mono remains the editor default during the transition. CoreCLR is an opt-in
desktop preset until the shared Mono/CoreCLR parity gates pass.

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
and validates the replacement catalog, migrates renamed fields, recreates instances, and restores state. If replacement
fails, the adapter reloads the previous program and restores the same snapshots. If rollback also fails, it clears the
invalid runtime state and reports both failures.

Unknown serialized members stay attached to the native instance and are written back after capture so an older build does
not erase data it cannot interpret. `[FormerlySerializedAs]` migrates a field or property rename. Assembly-qualified nested
type names use `Outer+Inner`; the old short nested name remains readable only when it is unambiguous within the assembly and
namespace.

## Compatibility policy

- Existing gameplay source is recompiled for the selected backend. Scene component IDs and serialized script identities do
  not change.
- The CoreCLR catalog supports public fields and properties, or non-public members marked `[SerializeField]`. It excludes
  static, indexed, `[DontSerializeField]`, and unsupported member types.
- Script callbacks use exact signatures. A same-named overload does not become a lifecycle or collision callback.
- CoreCLR is not the editor default while public engine-call binding parity, inspector workflows, debugging, and the shared
  contract suite remain incomplete.
- Browser builds use the dedicated .NET WebAssembly interpreter or AOT presets. Native AOT remains an evidence-gated,
  closed-world desktop player option. Neither preset is a substitute for the CoreCLR editor host.
- Crowny makes no named-console support claim without vendor SDK, runtime, licensing, and certification evidence.

## Required verification

Before promoting CoreCLR to the editor default, run the generated-ABI check, managed publishes, native contract tests,
legacy scene round trips, missing-script retention, repeated unload checks, exception-stack checks, inspector edits,
lifecycle and collision ordering, public binding marshalling, and private-runtime package launch. Record startup, callback,
allocation, reload, memory, build-time, and artifact-size measurements against Mono.
