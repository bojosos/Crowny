# .NET hosting and AOT options for Crowny

Research date: 2026-08-25  
Resolves: [#3](https://github.com/bojosos/Crowny/issues/3)

## Recommendation

Crowny should own a managed scripting interface and provide different adapters by platform and build profile. It should not try to find one runtime that covers the editor, desktop players, browsers, and unknown consoles equally well.

The first new adapter should host CoreCLR on Windows, macOS, and Linux through the supported `nethost` and `hostfxr` APIs. A small, stable Crowny bootstrap assembly should create a collectible `AssemblyLoadContext` for game code. This gives the editor modern .NET, managed debugging, broad reflection support, and assembly reload without depending on Coral or another hosting wrapper.

Keep the current Mono backend during migration, but treat it as a compatibility adapter. Original Mono is now in maintenance under WineHQ, Microsoft recommends that active users migrate to unified .NET, and its last listed release is 6.12.0.206. That makes it a poor long-term default despite its useful embedding API and Crowny's existing integration. See the [Mono project handoff notice](https://www.mono-project.com/download/stable/).

Add two player-only paths after the common interface works:

1. A .NET WebAssembly build using the modern Mono runtime and WebAssembly AOT for the Emscripten browser player.
2. A .NET Native AOT build for closed-world desktop players, provided a prototype passes compatibility, size, and frame-time gates.

Do not select a console backend from public information. Preserve an AOT-friendly C ABI and generated metadata, then test the available runtime and toolchain under each console SDK when that work is authorized.

## Keep runtime and compilation mode separate

A runtime family and a code execution mode are different choices.

| Runtime family | Relevant execution modes | Crowny role |
| --- | --- | --- |
| CoreCLR | JIT, tiered JIT, ReadyToRun | Desktop editor and default desktop player |
| Original Mono 6.12 | JIT, Mono AOT | Transition backend only |
| Modern Mono in unified .NET | Interpreter, JIT on supported native targets, Mono AOT | Stable browser runtime today |
| NativeAOT runtime | Native AOT only | Optional closed-world player backend |

ReadyToRun is CoreCLR precompiled code with JIT fallback. Mono AOT is Mono's compiler. Native AOT uses a separate, stripped-down runtime and has no JIT. Browser WebAssembly AOT uses the modern Mono runtime and its WebAssembly toolchain. These options are not interchangeable. The [.NET runtime glossary](https://github.com/dotnet/runtime/blob/main/docs/project/glossary.md) records these distinctions, and the [.NET MAUI runtime guide](https://learn.microsoft.com/en-us/dotnet/maui/deployment/runtimes-compilation) shows that Microsoft also chooses runtime and compilation strategy per target.

## Ranked candidates by platform role

| Role | First choice | Second choice | Do not use as the baseline |
| --- | --- | --- | --- |
| Desktop editor on Windows, macOS, Linux | Crowny-owned `hostfxr` and CoreCLR JIT adapter | Existing Mono while parity work is incomplete | Native AOT or WebAssembly AOT |
| Desktop development player and mod-capable player | `hostfxr` and CoreCLR JIT | Existing Mono for transition compatibility | Native AOT when runtime assembly loading is required |
| Closed-world desktop shipping player | CoreCLR JIT first; measure ReadyToRun separately | Native AOT as an opt-in profile after compatibility gates | Existing Mono as the future default |
| Browser development build | Modern .NET Mono interpreter for debugging and iteration | CoreCLR WebAssembly after its .NET 11 work is stable and measured | Original Mono embedding and desktop `hostfxr` |
| Browser shipping player | Modern .NET Mono WebAssembly AOT | Re-evaluate CoreCLR WebAssembly after stable AOT and native-link support exists | Desktop Native AOT |
| Future restricted platform | No winner without its SDK | Native AOT or modern Mono full AOT if the vendor-supported toolchain permits it | Any public desktop support claim treated as proof of console support |

The closed-world desktop row has two viable candidates, not a predetermined winner. CoreCLR is the safer default because it preserves normal .NET behavior and diagnostics. Native AOT may win for startup, memory, or deployment size on a specific game, but only measurements can establish that. Microsoft documents faster startup and a smaller memory footprint for Native AOT, while also documenting trimming, reflection, dynamic loading, diagnostics, and generic code-size limitations in the [Native AOT deployment guide](https://learn.microsoft.com/en-us/dotnet/core/deploying/native-aot/).

CoreCLR WebAssembly deserves a watch item rather than a Crowny commitment. As of .NET 11 Preview 7, Microsoft describes it as an interpreter plus ReadyToRun bring-up that has reached the runtime library test suite. Earlier preview notes state that the dedicated native toolchain and AOT paths still required Mono. .NET 11 preview work also mentions NativeAOT publish for WebAssembly, but it is outside the stable Native AOT support table. Neither preview path is a production baseline yet. See the [.NET 11 Preview 7 runtime notes](https://github.com/dotnet/core/blob/main/release-notes/11.0/preview/preview7/runtime.md), [Preview 5 notes](https://github.com/dotnet/core/blob/main/release-notes/11.0/preview/preview5/runtime.md), and [Preview 4 notes](https://github.com/dotnet/core/blob/main/release-notes/11.0/preview/preview4/runtime.md).

## Crowny's starting point

Crowny currently targets .NET Framework 4.7.2 in [`Crowny-Sharp/premake5.lua`](../../../Crowny-Sharp/premake5.lua). Its scripting interface is coupled to Mono at several layers:

- The managed API declares 471 Mono internal calls across 35 C# files. [`Time.cs`](../../../Crowny-Sharp/Source/Utils/Time.cs) is a small example.
- 114 engine or editor C++ files expose `MonoObject`, `MonoClass`, `MonoArray`, or related types. [`SerializableObjectInfo.h`](../../../Crowny/Source/Crowny/Scripting/Serialization/SerializableObjectInfo.h) and [`ScriptInspector.h`](../../../Crowny-Editor/Source/UI/ScriptInspector.h) show that serialization and inspector code depend directly on Mono handles.
- [`MonoManager.cpp`](../../../Crowny/Source/Crowny/Scripting/Mono/MonoManager.cpp) creates a script AppDomain, registers internal calls, resolves classes through Mono metadata, and unloads the domain during reload.

This has two consequences. Replacing runtime startup alone will not produce a CoreCLR backend. Crowny must also replace the Mono-shaped object, metadata, and interop contract. Second, keeping the public C# class names and serialized identities stable is possible, but the assemblies must be rebuilt for the selected backend.

The current AppDomain reload sequence already supplies a useful behavior specification. The CoreCLR implementation must serialize live fields, stop script-owned work, release native and managed handles, unload game code, load rebuilt code, reconstruct instances, and restore compatible fields. A runtime-neutral contract should describe that sequence before a second backend is added.

## Option 1: Crowny-owned hostfxr and CoreCLR adapter

### Embedding model

The supported native hosting route is `nethost` plus `hostfxr`, not direct calls to private CoreCLR exports. `get_hostfxr_path` locates `hostfxr`; `hostfxr_initialize_for_runtime_config` resolves a `.runtimeconfig.json`; and `hostfxr_get_runtime_delegate` obtains a managed entry point. Microsoft documents the flow in [Write a custom .NET runtime host](https://learn.microsoft.com/en-us/dotnet/core/tutorials/netcore-hosting) and maintains the matching [native host sample](https://github.com/dotnet/samples/blob/main/core/hosting/src/NativeHost/nativehost.cpp). The [native hosting design](https://github.com/dotnet/runtime/blob/main/docs/design/features/native-hosting.md) explicitly excludes directly loading CoreCLR and treats `hostfxr` as the supported boundary.

Crowny should use that boundary once to enter a stable `Crowny.ManagedHost` assembly. The bootstrap should expose a small C-compatible function table and load project assemblies itself. Crowny owns this code, so it can design reload, exception translation, logging, and version checks around engine needs without copying a third-party wrapper.

### Native interop

CoreCLR does not implement Mono's `mono_add_internal_call` contract. Keeping the 471 existing declarations unchanged internally would lock the common layer to Mono.

Use a versioned C ABI made from fixed-width integers, opaque handles, blittable structs, spans represented as pointer plus length, explicit UTF-8 buffers, and error codes. Pass the native engine function table to the managed bootstrap at startup. The bootstrap can expose native-callable static methods with `[UnmanagedCallersOnly]`. Microsoft recommends function pointers and `[UnmanagedCallersOnly]` for callbacks and `[LibraryImport]` for generated P/Invoke where it fits. See [.NET native interop best practices](https://learn.microsoft.com/en-us/dotnet/standard/native-interop/best-practices).

This ABI can also serve Native AOT and WebAssembly. Backend code still owns object handles, garbage collection roots, metadata lookup, and exception conversion. No public engine or editor header should contain a Mono or CLR object pointer.

### Unload and reload

Only one CoreCLR runtime can be loaded in a process, and closing a host context does not unload it. The hostfxr component-loading helper also returns process-lifetime function pointers and has no component unload operation. These limits are documented in the [native hosting design](https://github.com/dotnet/runtime/blob/main/docs/design/features/native-hosting.md).

Do not ask hostfxr to load each game assembly. Load the stable bootstrap once, then have it create a collectible `AssemblyLoadContext` for `CrownySharp`, the game assembly, and reloadable dependencies. `AssemblyLoadContext` unloading is cooperative. It finishes only after no thread, stack, static reference, strong GC handle, or external reference retains an object from the context. Microsoft lists the full conditions and debugging procedure in [How to use and debug assembly unloadability](https://learn.microsoft.com/en-us/dotnet/standard/assembly/unloadability).

Crowny therefore needs a reload audit that detects leaked contexts with a `WeakReference`, bounded garbage collection attempts, and a useful report of known native handles and script-owned workers. A failed unload should leave the previous scripts active or request an editor restart. It must not silently load unlimited copies.

### Reflection and trimming

Untrimmed CoreCLR keeps the reflection behavior that Crowny's inspector and serializer expect. That makes it the least disruptive editor target. The game assembly can still use a generated type manifest so the same source is ready for AOT, but editor correctness need not depend on the generator at first.

ReadyToRun is not a reload solution. The runtime ignores ReadyToRun code in collectible contexts, according to the [assembly unloadability guide](https://learn.microsoft.com/en-us/dotnet/standard/assembly/unloadability). It may still help startup for stable framework and bootstrap assemblies, but it should be measured separately.

### Debugging and diagnostics

CoreCLR has the richest option set here. Managed debuggers, EventPipe, dumps, traces, counters, and custom diagnostic clients are available on desktop. The [.NET diagnostics overview](https://learn.microsoft.com/en-us/dotnet/core/diagnostics/) summarizes those tools. The runtime diagnostic port uses named pipes on Windows and Unix domain sockets on Linux and macOS, as described in [Diagnostic ports](https://learn.microsoft.com/en-us/dotnet/core/diagnostics/diagnostic-port).

The Crowny adapter still has work to do. It must emit portable PDBs, expose the correct process and symbol paths to the IDE, convert unhandled managed exceptions into editor diagnostics, and verify that reload does not leave debugger state attached to an old load context.

### Distribution, platforms, and license

The documented hostfxr component API supports framework-dependent components, not the SDK's normal self-contained component model. Crowny may point `nethost` at a controlled runtime root, but an app-local private runtime layout needs a prototype and packaging test. It should not assume that publishing a self-contained managed component makes it hostable. See the hosting tutorial's [deployment limitation](https://learn.microsoft.com/en-us/dotnet/core/tutorials/netcore-hosting#limitations).

.NET supports the desktop targets Crowny wants. Current downloads cover Windows x64, x86, and Arm64, macOS x64 and Arm64, and several Linux architectures and distributions. See the official [Windows](https://learn.microsoft.com/en-us/dotnet/core/install/windows), [macOS](https://learn.microsoft.com/en-us/dotnet/core/install/macos), and [Linux](https://learn.microsoft.com/en-us/dotnet/core/install/linux) installation pages. Crowny should initially qualify only the engine architectures it builds and tests.

The .NET runtime source is MIT licensed. Product binary terms differ by OS, and distributions include third-party notices. Microsoft's [.NET license information](https://github.com/dotnet/core/blob/main/license-information.md) says Linux and macOS product distributions use MIT terms while Windows product distributions use the .NET Library License. Packaging must retain the applicable notices.

### Likely performance and size

CoreCLR has the best desktop steady-state performance potential in this set because it has a highly optimizing, tiered JIT and mature runtime diagnostics. That is a capability claim, not a Crowny benchmark. JIT compilation adds startup and warm-up work. A private runtime also costs more disk space than shipping only the game assembly.

Do not rank CoreCLR on reputation. Measure editor startup, first script callback, warmed callback throughput, allocation and collection time, reload latency, native-to-managed transition cost, and packaged size. Test debug and release separately.

### Main risks

- Cooperative load-context leaks can make editor reload unreliable.
- A CoreCLR bootstrap cannot reuse Mono object pointers or internal calls.
- Only one runtime version can own the process, so runtime selection occurs before managed startup.
- Private runtime packaging and servicing become Crowny responsibilities.
- Upgrading the target framework every year would add churn. Pin an LTS runtime and define a servicing policy.

## Option 2: Existing original Mono backend

### What it still does well

Mono was designed for embedding. A native application links `libmono`, initializes a root domain, creates application domains, invokes methods through metadata handles, and registers native internal calls. The official [embedding guide](https://www.mono-project.com/docs/advanced/embedding/) documents the same model Crowny uses.

Crowny already has assembly discovery, editor inspection, serialization, object lifetime tracking, a debugger agent, and AppDomain reload on this backend. Keeping it during the transition is the lowest-risk way to preserve current projects while the common interface is extracted.

### Costs and limits

The tight Mono API made the first implementation convenient but now spreads runtime objects through engine and editor code. Internal calls pass raw Mono heap objects without a marshalling layer. They cannot become the shared ABI for CoreCLR, Native AOT, or WebAssembly.

Original Mono's maintenance position is the larger problem. Microsoft completed the move of its modern Mono work to `dotnet/runtime`, handed stewardship of original Mono to WineHQ, and recommends migration to unified .NET. Its old platform page lists many systems, including consoles, but that page is not proof that Crowny can obtain a maintained toolchain or ship on a current console. See [Supported platforms](https://www.mono-project.com/docs/about-mono/supported-platforms/) and the [handoff notice](https://www.mono-project.com/download/stable/).

Mono's JIT and optional AOT can reduce startup or support no-JIT targets, but its own [AOT guide](https://www.mono-project.com/docs/advanced/aot/) warns that full AOT must find all required generic instantiations, some libraries generate code dynamically, and AOT code may be slower than JIT code in some programs. No source found justifies a Crowny performance claim without measurement.

Original Mono is generally MIT licensed, with several bundled third-party components under other licenses. The [Mono license file](https://github.com/mono/mono/blob/main/LICENSE) lists them. Crowny must audit the exact redistributed build rather than rely on the headline license.

### Proper role

Keep Mono as an explicitly selectable desktop compatibility backend until CoreCLR passes source, serialization, inspector, debugger, exception, and reload parity. Do not invest in making the original Mono 6.12 embedding layer Crowny's browser or console strategy.

## Option 3: .NET Native AOT

### Embedding and interop

Native AOT is not CoreCLR with an AOT switch. Publishing compiles the managed application and a reduced runtime into a target-specific native executable or library. It has no JIT. The official [Native AOT deployment guide](https://learn.microsoft.com/en-us/dotnet/core/deploying/native-aot/) describes the model.

For Crowny, the practical form is a shared native library. Static C# methods marked `[UnmanagedCallersOnly(EntryPoint = ...)]` become C exports. The native engine loads the library and calls those exports. Managed code can P/Invoke engine functions or receive the same versioned engine function table used by the CoreCLR bootstrap. Microsoft documents native exports and static native dependencies in [Native AOT interop](https://learn.microsoft.com/en-us/dotnet/core/deploying/native-aot/interop) and supplies a [native library sample](https://github.com/dotnet/samples/blob/main/core/nativeaot/NativeLibrary/README.md).

Exported methods must be static and use primitive or manually marshalled value types. Exceptions must not escape the unmanaged entry point. This fits the proposed C ABI and rejects the current Mono-shaped API.

### Reload, reflection, and compatibility

Native AOT cannot dynamically load game assemblies and does not support unloading its generated runtime library. All scripts and required dependencies must be inputs to publish. The official deployment guide lists dynamic loading and runtime code generation as unsupported. The native library sample states that the resulting library cannot be unloaded with `FreeLibrary` or `dlclose`.

Reflection still exists for metadata retained by the build, but unbounded reflection is unsafe under trimming. Crowny's inspector and serializer need generated metadata or precise preservation annotations. The trimmer documentation recommends analyzable reflection patterns, `DynamicallyAccessedMembers`, or source generators and explains why unresolved warnings can become runtime failures. See [Prepare .NET libraries for trimming](https://learn.microsoft.com/en-us/dotnet/core/deploying/trimming/prepare-libraries-for-trimming).

This means Native AOT can preserve C# source compatibility for ordinary game scripts, but not unrestricted runtime behavior. Mods loaded as arbitrary assemblies, `Reflection.Emit`, and libraries that depend on hidden reflection targets do not fit this backend.

### Debugging, distribution, platforms, and license

A published Native AOT binary uses native debug symbols and native debuggers. The managed debugger does not attach to the published result. Heap analysis is not supported, and several other diagnostic features are partial. Microsoft recommends debugging normal builds on the full runtime before Native AOT publish. See [Native AOT diagnostics](https://learn.microsoft.com/en-us/dotnet/core/deploying/native-aot/diagnostics).

The result is self-contained and target-specific. The currently documented production matrix covers Windows x64 and Arm64, Linux x64 and Arm64, and macOS x64 and Arm64. Native AOT requires a native compiler and platform libraries at publish time. Its stable support table does not list browser WebAssembly or consoles. .NET 11 preview work is experimenting with NativeAOT publish for WebAssembly, so recheck this after .NET 11 reaches general availability. The runtime licensing considerations are the same .NET source and product-distribution terms described above.

### Likely performance and size

Microsoft documents faster startup and a smaller memory footprint compared with JIT deployment. Native AOT avoids JIT stalls and can work where executable memory is prohibited. It does not guarantee better frame time. It loses dynamic profile-guided JIT decisions, interpreted expression trees can be slower, and pre-generated generic instantiations can increase file size. The self-contained binary includes the required runtime code.

Measure it against CoreCLR using an identical exported game and an identical binding ABI. A win in startup with a regression in package size, inspector compatibility, or a representative gameplay workload is not an overall win.

### Proper role

Treat Native AOT as an opt-in, player-only `DotNetNativeAOT` profile. It should stay hidden for the editor, mod-capable builds, browser targets, and any platform without an official or vendor-supported runtime pack. Require zero AOT and trim warnings in Crowny-owned managed code before enabling it for users.

## Option 4: .NET WebAssembly

### Runtime and build model

The stable browser path uses the modern Mono runtime from unified .NET, compiled to WebAssembly. It can interpret IL or AOT-compile managed methods into WebAssembly. The browser is hosted through `dotnet.js`; it does not use desktop `nethost` or `hostfxr`. The runtime's [WebAssembly feature guide](https://github.com/dotnet/runtime/blob/main/src/mono/wasm/features.md) documents the `wasmbrowser` template, JavaScript host API, runtime assets, AOT, trimming, debugging, and native files.

For Crowny's shipping browser player, use WebAssembly AOT. Keep an interpreter development configuration even if the exported player preset only exposes AOT. The current .NET build targets require AOT to publish in Release with trimming. They reject ordinary Debug AOT builds. See [`WasmApp.Common.targets`](https://github.com/dotnet/runtime/blob/main/src/mono/wasm/build/WasmApp.Common.targets).

### Integrating the Emscripten engine

The .NET WebAssembly SDK can compile C sources or link Emscripten object files and archives through `NativeFileReference`, then rebuild the runtime and application into `dotnet.native.wasm`. Microsoft notes that prebuilt native dependencies normally need the same Emscripten version as the .NET runtime build. See the [.NET WebAssembly native dependency guide](https://learn.microsoft.com/en-us/aspnet/core/blazor/webassembly-native-dependencies).

The first prototype should let the .NET WebAssembly SDK own the final link and feed it a minimal Crowny engine archive. Trying to combine two independently linked main modules adds risk. Emscripten expects exactly one main module, and its dynamic linking plus pthreads combination remains experimental. See [Emscripten dynamic linking](https://emscripten.org/docs/compiling/Dynamic-Linking.html).

This build should use the same C ABI as desktop. Managed code can P/Invoke statically linked Crowny functions. JavaScript-only browser services belong behind `[JSImport]` and `[JSExport]` in the browser adapter rather than in game scripts.

### Reload, reflection, debugging, and browser constraints

The AOT browser player is closed-world. Rebuild and reload the page to change scripts. Runtime assembly reload must not be part of its capability set.

AOT requires trimming, so inspector and serialization metadata must be generated or explicitly preserved. The .NET WebAssembly guide warns that reflection, serialization, and dependency injection can break under trimming.

.NET supports C# debugging through browser developer tools and Visual Studio in development builds, plus DWARF for native code. Shipping AOT has a different workflow and should rely on symbols, logs, and source maps. Crowny should not promise desktop debugger parity in the AOT player.

Web threads add deployment requirements. Emscripten implements pthreads with `SharedArrayBuffer`, and a threaded and non-threaded build cannot be one fallback binary. See [Emscripten pthreads](https://emscripten.org/docs/porting/pthreads.html). Crowny needs explicit single-threaded and threaded browser capabilities, suitable cross-origin isolation headers for the threaded build, and tests for main-thread browser APIs.

### Likely performance, download size, and maintenance

Microsoft states that WebAssembly AOT greatly improves managed execution performance but increases the application size, download time, and startup time. No fixed ratio applies to Crowny. Full AOT, selective AOT if supported by the chosen SDK, trimming roots, globalization data, engine size, compression, caching, and network conditions all affect the result.

This backend ties Crowny's Emscripten version and final native link to the .NET WebAssembly workload. That is more maintenance than adding a separate managed file to an existing web build. Pin the .NET SDK and workload together, cache them in CI, and upgrade them as one toolchain.

The .NET runtime keeps the .NET licensing terms described above. Emscripten offers its code under MIT and University of Illinois/NCSA terms, according to the [Emscripten repository](https://github.com/emscripten-core/emscripten). Its LLVM toolchain uses Apache 2.0 with LLVM exceptions and contains separately identified third-party code. See the [LLVM license](https://github.com/llvm/llvm-project/blob/main/llvm/LICENSE.TXT). Preserve the notices from the exact workload and native libraries shipped in the browser bundle.

### Proper role

Expose one shipping preset named for what it does, such as `DotNet WebAssembly AOT`, not `CoreCLR AOT` or `Native AOT`. Hide desktop backends for the browser target. Keep `DotNet WebAssembly Interpreter` as an internal or advanced development preset until its user experience is deliberate.

## Restricted platforms and consoles

No public source establishes a shippable Crowny console backend. Original Mono's public platform table names older and current console families, but it does not supply the SDK access, current binaries, licensing, certification status, or support contract Crowny would need. Native AOT's public support table names only Windows, Linux, and macOS. Desktop hostfxr assumes a supported desktop runtime layout and JIT-capable process.

The design can still prepare for restricted platforms:

- Keep the public C# API independent of runtime classes.
- Put all native calls behind a versioned, generated C ABI.
- Generate script type, field, method, attribute, and serialization metadata at build time.
- Make dynamic assembly loading, JIT, runtime code generation, threads, reflection, and debugger attachment explicit capabilities.
- Compile all player scripts into a closed set for AOT profiles.
- Keep backend selection in build configuration, and hide profiles that the selected platform adapter does not provide.

When a console is in scope, build a minimal vendor-SDK spike first. It must answer whether the approved runtime is CoreCLR, modern Mono full AOT, Native AOT, or a vendor fork; how native code is linked; what debugging works; and what redistribution terms apply. Rank options only after that evidence exists.

## Common architecture required before another backend

The runtime adapters should implement behavior, not leak runtime object models. A practical split is:

1. `ManagedRuntime` owns startup, shutdown, capability reporting, assembly contexts, managed invocation, garbage collection handles, exceptions, and debugger configuration.
2. `ManagedMetadata` exposes backend-neutral assembly, type, member, and attribute descriptions. Editor and serialization code consume these descriptions instead of `MonoClass*`.
3. `ManagedValue` carries primitives, strings, enums, object handles, arrays, lists, dictionaries, and serialized object values without exposing a GC object pointer.
4. `Crowny.ManagedApi` keeps the existing public C# names and behavior. A separate backend bootstrap implements its native bridge.
5. A source generator emits native binding declarations and the closed-world type and serialization manifest needed by AOT and WebAssembly.

The first implementation should not reproduce every Mono metadata function behind a generic virtual method. That would make the common interface as wide and shallow as Mono's API. Extract the operations Crowny actually needs for script lifecycle, field serialization, inspectors, entity and asset handles, and native calls.

Runtime capabilities should at least report:

| Capability | CoreCLR editor | Original Mono editor | Native AOT player | WebAssembly AOT player |
| --- | --- | --- | --- | --- |
| JIT | Yes | Yes | No | No |
| Dynamic assembly load | Yes | Yes | No | No |
| Script assembly unload | Cooperative ALC | AppDomain | No | No |
| Full editor reflection | Yes | Yes | Generated subset | Generated subset |
| Managed debugger | Yes | Existing Mono agent | No after publish | Development configuration only |
| Runtime code generation | Yes | Yes | No | Do not depend on it |
| Self-contained player | Private runtime packaging needs a hosting spike | Yes with bundled Mono | Yes | Yes as web assets |

The editor should show only compatible presets. It should also explain why a preset is unavailable, such as `requires closed-world scripts`, `browser target only`, or `runtime pack unavailable for this architecture`.

## Compatibility contract

"Seamless" should mean that existing game C# source recompiles without ordinary source edits and that scenes retain script type and field identity. It should not mean that a .NET Framework 4.7.2 binary runs unchanged on every backend.

Preserve these items across backends:

- Public namespace, class, member, attribute, and callback names in Crowny's C# API.
- Script identity based on stable assembly, namespace, and type identity, with existing rename attributes honored.
- Serialized primitive, enum, object, collection, entity, and asset field formats.
- Missing-script behavior and recovery after the matching type returns.
- Lifecycle ordering, exception reporting, and edit versus play behavior.

Move the user-facing API toward an SDK-style target that can compile for unified .NET. A useful migration experiment is a runtime-neutral `Crowny.ManagedApi` contract targeting `netstandard2.0`, with modern backend glue targeting the chosen .NET LTS. If an API cannot fit that contract, multi-target it in the glue rather than exposing backend conditionals to game scripts. Validate this against actual Crowny scripts before making it policy.

## Measurement and acceptance gates

There are no Crowny benchmarks in this research, so the performance ranking remains conditional. Use one representative script suite and one serialized scene for every backend.

Measure:

- Runtime and first-scene startup time.
- First call and warmed native-to-managed callback cost.
- Frame-time distribution under script-heavy work, not only average throughput.
- Allocations, collection pauses, and managed heap size.
- Editor rebuild and reload time, plus leaked load contexts.
- Release artifact size, compressed browser transfer size, and browser startup time.
- AOT publish time and incremental developer build time.

Require:

- Existing script source recompiles without routine changes.
- The same scene round-trips serialized fields through Mono and CoreCLR.
- CoreCLR supports inspector reflection, exception stacks, managed debugging, and repeated reload before it becomes the editor default.
- Native AOT and WebAssembly builds have zero Crowny-owned trim and AOT warnings.
- AOT profiles reject unsupported libraries and dynamic features during the build, not after launch.
- Every packaged runtime has a pinned version, license notices, symbols policy, and servicing procedure.

## Decision record candidates

The evidence supports these decisions now:

1. Crowny owns the scripting abstraction and hostfxr adapter. Coral is not a dependency.
2. CoreCLR through hostfxr is the first new desktop backend and the eventual editor default after parity.
3. Original Mono remains selectable only for migration and fallback.
4. The stable browser shipping candidate is modern .NET Mono WebAssembly AOT, with the .NET SDK owning the final Emscripten link.
5. Native AOT is an experimental closed-world desktop player profile, not an editor or browser backend.
6. Console backend selection waits for vendor SDK evidence, while the common ABI and generated metadata remain AOT-ready.

The open choices need prototypes and measurements: private CoreCLR packaging, collectible context reliability with Crowny's object graph, the generated metadata format, the WebAssembly final-link arrangement, and whether Native AOT provides a real benefit for a Crowny game.
