# Managed scripting implementation toolchain verification

Verified on 2026-08-25 against current first-party documentation, pinned .NET runtime source, and the Windows toolchain installed in this worktree environment.

## Decision

The research-backed split is implementable with public toolchains:

| Crowny role | Supported implementation |
| --- | --- |
| Desktop editor and ordinary desktop player | Crowny-owned CoreCLR host through `nethost` and `hostfxr`; one process-lifetime managed bootstrap; collectible `AssemblyLoadContext` instances for reloadable game code |
| Browser development | .NET 10 Mono WebAssembly interpreter or Jiterpreter; the .NET WebAssembly SDK relinks the runtime with a Crowny Emscripten archive |
| Browser shipping | The same .NET 10 Mono WebAssembly project with publish-time AOT and trimming; the .NET SDK owns the final Emscripten link |
| Closed-world desktop player experiment | Native AOT shared library with C exports; loaded once for the player process |

Use .NET 10 as the baseline. It is the current LTS release, and the support policy listed runtime 10.0.11 on the verification date. Pin the exact SDK and workload set in `global.json`, then service the bundled runtime within the .NET 10 patch line. Do not base a shipping path on a .NET 11 preview. [Microsoft's .NET support policy](https://dotnet.microsoft.com/en-us/platform/support/policy/dotnet-core)

## Desktop CoreCLR host

The supported startup route is:

1. Build `Crowny.ManagedHost` as a framework-dependent `net10.0` component. Set `GenerateRuntimeConfigurationFiles=true`, `SelfContained=false`, and a deliberate runtime roll-forward policy such as `LatestPatch`. Ship its DLL, `.deps.json`, and `.runtimeconfig.json`. The generated runtime config must contain a `Microsoft.NETCore.App` framework reference. `hostfxr_initialize_for_runtime_config` processes framework dependency manifests, not the component's `.deps.json`; the later component-load helper uses `AssemblyDependencyResolver` and the adjacent component `.deps.json`. [Runtime configuration file format](https://github.com/dotnet/sdk/blob/main/documentation/specs/runtime-configuration-file.md), [SDK runtime-config property](https://learn.microsoft.com/en-us/dotnet/core/project-sdk/msbuild-props#generateruntimeconfigurationfiles), [component runtime-config constraints](https://github.com/dotnet/runtime/blob/b41b63973bf49ad7a2d0d2ef37f08cf5df622ea1/docs/design/features/native-hosting.md#L264-L284), [component dependency loading](https://github.com/dotnet/runtime/blob/b41b63973bf49ad7a2d0d2ef37f08cf5df622ea1/docs/design/features/native-hosting.md#L408-L424)
2. Link or load `nethost` from the matching `Microsoft.NETCore.DotNetAppHost` package. Call `get_hostfxr_path`, load `hostfxr`, then resolve `hostfxr_initialize_for_runtime_config`, `hostfxr_get_runtime_delegate`, and `hostfxr_close`. Request `hdt_load_assembly_and_get_function_pointer` and use it once to enter the stable bootstrap through an `UnmanagedCallersOnly` method, passing `UNMANAGEDCALLERSONLY_METHOD` as the delegate-type argument and `null` for the reserved argument. This is Microsoft's supported native-host sequence. [Custom .NET host tutorial](https://learn.microsoft.com/en-us/dotnet/core/tutorials/netcore-hosting), [native-hosting design](https://github.com/dotnet/runtime/blob/b41b63973bf49ad7a2d0d2ef37f08cf5df622ea1/docs/design/features/native-hosting.md#L408-L430)
3. Keep every native-callable function pointer in the noncollectible bootstrap. The component loader returns process-lifetime pointers and has no component-unload operation. Do not use it to load each game rebuild. [Component pointer lifetime](https://github.com/dotnet/runtime/blob/b41b63973bf49ad7a2d0d2ef37f08cf5df622ea1/docs/design/features/native-hosting.md#L408-L430)
4. Let the bootstrap create one collectible `AssemblyLoadContext` per game-program generation. Use `AssemblyDependencyResolver` for the game assembly and its private dependencies, while explicitly sharing the stable Crowny API and bootstrap contract assemblies. Native code sees only the versioned Crowny C ABI and opaque handles. It must never retain a game object, reflection object, delegate, or strong `GCHandle`.
5. Reload cooperatively. Stop script work, release handles and callbacks, call `Unload`, drop all strong references, and verify collection through a `WeakReference` plus bounded GC and finalizer passes. Threads with game frames, external references, and strong or pinned handles prevent unload. [Assembly unloadability rules](https://learn.microsoft.com/en-us/dotnet/standard/assembly/unloadability)

CoreCLR itself remains loaded. Only one runtime can be loaded in a process, and closing a host context does not turn runtime shutdown into a reload mechanism. [Custom-host limitation](https://learn.microsoft.com/en-us/dotnet/core/tutorials/netcore-hosting#limitations), [first host-context lifetime](https://github.com/dotnet/runtime/blob/b41b63973bf49ad7a2d0d2ef37f08cf5df622ea1/docs/design/features/native-hosting.md#L511-L525)

### Private-runtime packaging

`hostfxr_initialize_for_runtime_config` accepts framework-dependent components only. A managed bootstrap published with `--self-contained` is not a supported component input. [Hosting API deployment limitation](https://learn.microsoft.com/en-us/dotnet/core/tutorials/netcore-hosting#hosting-apis)

Crowny can still ship a product that needs no machine-wide .NET install. Package a complete, per-RID .NET runtime layout beside the engine, keep the bootstrap framework-dependent, and pass that private root in both `get_hostfxr_parameters.dotnet_root` and `hostfxr_initialize_parameters.dotnet_root`. The root must preserve the official install layout, including `host/fxr`, `shared/Microsoft.NETCore.App`, `hostpolicy`, CoreCLR, runtime libraries, and notices. Do not copy a few DLLs out of a self-contained publish and call that a private runtime. `nethost` and `hostfxr` define `dotnet_root` as the root of a .NET installation and use it to find the host and shared frameworks. [Private `dotnet_root` lookup](https://github.com/dotnet/runtime/blob/b41b63973bf49ad7a2d0d2ef37f08cf5df622ea1/docs/design/features/native-hosting.md#L140-L161), [initialization root](https://github.com/dotnet/runtime/blob/b41b63973bf49ad7a2d0d2ef37f08cf5df622ea1/docs/design/features/native-hosting.md#L219-L226)

This distinction should appear in build terminology: the Crowny player bundle is self-contained as a product, while `Crowny.ManagedHost` remains a framework-dependent component hosted against Crowny's private runtime root.

## .NET Mono WebAssembly

Use a final browser application based on `Microsoft.NET.Sdk.WebAssembly` targeting `net10.0`. The stable .NET 10 browser template uses that SDK, and its output loads the Mono runtime through `dotnet.js`. [Pinned .NET 10 browser project template](https://github.com/dotnet/runtime/blob/v10.0.0/src/mono/wasm/templates/templates/browser/browser.0.csproj), [pinned .NET 10 WebAssembly guide](https://github.com/dotnet/runtime/blob/v10.0.0/src/mono/wasm/features.md)

Both browser configurations consume the same Crowny archive:

```xml
<ItemGroup>
  <NativeFileReference Include="$(CrownyWebArchive)" />
</ItemGroup>
```

Microsoft supports Emscripten `.a` files as `NativeFileReference` inputs and warns that prebuilt inputs normally need the same Emscripten version as the .NET runtime. Build the Crowny archive with the Emscripten toolchain and compile flags from the pinned `wasm-tools` workload. Keep the `NativeFileReference` in the final browser application project, or inject it there with package build props; WebAssembly-specific native dependencies are not referenced automatically through an ordinary managed dependency. [WebAssembly native dependencies](https://learn.microsoft.com/en-us/aspnet/core/blazor/webassembly-native-dependencies?view=aspnetcore-10.0)

Make archive creation an MSBuild dependency of `PrepareForWasmBuildNative`. The .NET 10 browser targets resolve and version-check the workload toolchain before that extension point, then expose its Emscripten paths and environment. A Crowny target can run its CMake archive build there and fail if the expected `.a` was not produced. Keep this integration pinned to the SDK because those MSBuild properties are toolchain details, not a separate stable Emscripten contract. [Pinned native-build extension point](https://github.com/dotnet/runtime/blob/v10.0.0/src/mono/browser/build/BrowserWasmApp.targets#L14-L24), [pinned workload toolchain setup](https://github.com/dotnet/runtime/blob/v10.0.0/src/mono/browser/build/BrowserWasmApp.targets#L188-L210)

For development, leave `RunAOTCompilation=false`. The managed code runs under the Mono interpreter or Jiterpreter. The archive still causes a native relink, so this configuration also needs `wasm-tools`. For shipping, set `RunAOTCompilation=true`, `PublishTrimmed=true`, and publish Release. The .NET 10 targets reject AOT without trimming and reject ordinary Debug AOT builds. [Interpreter and AOT behavior](https://learn.microsoft.com/en-us/aspnet/core/blazor/webassembly-build-tools-and-aot?view=aspnetcore-10.0), [pinned AOT checks](https://github.com/dotnet/runtime/blob/v10.0.0/src/mono/wasm/build/WasmApp.Common.targets#L594-L606)

The .NET targets set `WasmBuildNative` when a native reference is present, add that reference to the native link inputs, and perform the final link that emits `dotnet.native.wasm`. Crowny supplies an archive, not a separately linked Emscripten main module. [Pinned native-relink rules](https://github.com/dotnet/runtime/blob/v10.0.0/src/mono/wasm/build/WasmApp.Common.targets#L503-L528), [pinned native inputs](https://github.com/dotnet/runtime/blob/v10.0.0/src/mono/browser/build/BrowserWasmApp.targets#L364-L430), [pinned browser final link](https://github.com/dotnet/runtime/blob/v10.0.0/src/mono/browser/build/BrowserWasmApp.targets#L475-L525)

This single-final-link rule avoids an unnecessary dynamic-linking design. Emscripten expects exactly one main module, recommends static linking for performance, and still labels dynamic linking combined with pthreads experimental. [Emscripten dynamic linking](https://emscripten.org/docs/compiling/Dynamic-Linking.html)

The shipping browser program is closed-world. It does not support runtime game-assembly reload. Generate reflection, serialization, callback, trimming-root, and AOT-generic metadata before publish. Treat threaded and single-threaded browser builds as separate artifacts because Emscripten cannot make one binary fall back between them. Threaded deployment also needs cross-origin isolation headers. [Emscripten pthreads](https://emscripten.org/docs/porting/pthreads.html)

## Native AOT desktop player

Native AOT is a viable optional desktop player only:

- Publish a `net10.0` class-library root project with `PublishAot=true` for a concrete desktop RID. The result is a self-contained `.dll`, `.so`, or `.dylib`. Official support covers shared libraries; unloading through `FreeLibrary` or `dlclose` is unsupported. [Building Native AOT libraries](https://learn.microsoft.com/en-us/dotnet/core/deploying/native-aot/libraries)
- Put each C export stub in the published root assembly and mark it `UnmanagedCallersOnly` with a fixed `EntryPoint`. Native AOT does not export marked methods from referenced projects or packages. Keep arguments blittable, catch every exception inside the export, and return a Crowny ABI status. [Native AOT exports](https://learn.microsoft.com/en-us/dotnet/core/deploying/native-aot/interop#native-exports), [`UnmanagedCallersOnly` restrictions](https://learn.microsoft.com/en-us/dotnet/api/system.runtime.interopservices.unmanagedcallersonlyattribute?view=net-10.0#remarks)
- Compile the API, game scripts, generated registration, serialization thunks, reflection roots, and required generic instantiations into that one publish closure. Native AOT has no dynamic assembly loading or runtime code generation and requires trimming. [Native AOT limitations](https://learn.microsoft.com/en-us/dotnet/core/deploying/native-aot/#limitations-of-native-aot-deployment)
- Load the library once for the player process. Do not expose this preset in the editor, reloadable players, or mod-capable players. Enable it only after zero Crowny-owned trim and AOT warnings plus measurements against the CoreCLR player.

The public Native AOT matrix covers Windows, Linux, and macOS desktop targets. It does not establish console support. Named consoles still require vendor SDK, runtime, licensing, and certification evidence.

## Local toolchain findings

These are machine facts, not architecture limits:

- `dotnet` is absent from `PATH` and from the standard machine and user install roots. No .NET SDK, runtime, workload manifests, `nethost` package, or `wasm-tools` workload can be used locally yet. This blocks all three prototypes on this machine until a pinned .NET 10 SDK is installed.
- No standalone `emcc`, `em++`, `emar`, `EMSDK`, or `EM_CONFIG` is present. A separate Emscripten install is not required by the design because `wasm-tools` supplies the matched toolchain, but the missing workload currently blocks creation of the Crowny archive and final WebAssembly link.
- Visual Studio Build Tools 2022 version 17.14.37 is installed with MSVC 14.44, x64 C++ tools, MSBuild, and Windows SDK 10.0.26100. Those tools are not on the default shell `PATH`, but their binaries are present under the Build Tools installation. This removes the main native-compiler prerequisite for a Windows Native AOT prototype once the .NET SDK is installed.
- Python 3.12.10, Node 24.19.0, Chrome, and Edge are installed. Browser execution is available after the .NET workload and build artifacts exist.
- The repository has no `global.json` or SDK-style managed project yet. `Crowny-Sharp` and `Crowny-Sandbox` still come from Premake projects targeting .NET Framework 4.7.2. SDK migration and a pinned workload set are implementation prerequisites, not external blockers.
- This Windows environment cannot verify macOS or Linux packaging. Run the CoreCLR private-runtime, WebAssembly publish, and Native AOT gates in per-platform CI before claiming those outputs.

No public source or installed vendor toolchain supports a named-console claim.
