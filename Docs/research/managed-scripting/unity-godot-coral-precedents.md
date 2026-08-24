# Unity, Godot, and Coral as managed-scripting precedents

Research date: 2026-08-25  
Issue: [#4 Study Unity, Godot, and Coral as managed-scripting precedents](https://github.com/bojosos/Crowny/issues/4)

## Decision summary

Crowny should copy the separation of concerns, not another project's runtime stack.

Unity has the best user-facing model in this comparison. A player backend is a per-target build setting, unsupported choices do not form part of the platform contract, and the C# API stays nearly the same when the player moves between JIT and AOT. Unity's editor continues to optimize for managed-code iteration even when a player uses IL2CPP. This is the right product model for Crowny.

Godot has the most useful open implementation precedent. Godot 4 owns its native host, managed bootstrap, callback tables, generated bindings, project build integration, and reload protocol. It lets the .NET SDK provide CoreCLR, the modern Mono runtime, and NativeAOT. This is much closer to what Crowny should build than creating an IL-to-C++ compiler like IL2CPP.

Coral is useful source material for a small hostfxr bootstrap and a C++ facade over managed reflection. Crowny should not adopt it as a dependency. Coral's current `main` branch supports only Windows x64, Linux x64, and macOS arm64, requires .NET 9 on the machine, manually discovers hostfxr, and does not cover AOT, mobile, browser, or console players. Its API also mixes runtime hosting, reflection, invocation, object handles, assembly identity, and build execution. Crowny would still own every engine-specific compatibility and deployment problem after adopting it.

The resulting direction is:

1. Crowny owns a runtime-neutral scripting contract and serialization identity model.
2. The editor uses a JIT-capable backend with collectible assembly contexts, initially Mono for compatibility and then CoreCLR when parity gates pass.
3. Player build profiles choose from only the backends that the target supports. The likely roles are CoreCLR or Mono for desktop JIT, NativeAOT for native restricted targets, and a WebAssembly AOT path for browser builds.
4. Source generation produces registration, callback, serialization, and AOT-root metadata. Reflection remains an editor and discovery tool, not the per-frame invocation path.

## Source and version boundary

This report describes these revisions rather than relying on recollection from older engine versions:

| Project | Revision examined | Why it matters |
| --- | --- | --- |
| Unity | Unity 6.5, documentation build dated 2026-08-24 | The current manual identifies the supported Mono and IL2CPP targets and the current WebAssembly toolchain. |
| Godot | [Godot 4.7.2 stable](https://github.com/godotengine/godot/releases/tag/4.7.2-stable), released 2026-08-18 | The tagged source resolves runtime details that the higher-level C# documentation leaves implicit. |
| Coral | [`d53b268`](https://github.com/StudioCherno/Coral/commit/d53b2685725f7535bc4d1deaa8a22bf16d112fe2), current `main` on the research date | Coral has no stable release contract. Pinning the commit avoids treating a moving branch as a versioned SDK. |

Public console information is necessarily incomplete. Unity documents the general IL2CPP requirement but not every vendor-specific configuration. Godot explicitly leaves console ports to licensed developers and third parties. No conclusion below assumes access to a console SDK or NDA material.

## Platform and backend matrix

| Role | Unity 6.5 | Godot 4.7.2 | Coral at `d53b268` |
| --- | --- | --- | --- |
| Editor | Mono-oriented managed domain and reload workflow. Player backend choice does not turn the editor into IL2CPP. | Separate .NET-enabled editor. It loads the installed compatible .NET runtime through hostfxr and runs project code in a collectible `AssemblyLoadContext`. | CoreCLR through hostfxr. Collectible `AssemblyLoadContext` instances load user assemblies. |
| Windows player | Mono JIT or IL2CPP AOT. Mono is the default where both are supported. | Self-contained .NET publish. CoreCLR is the ordinary desktop path; the loader also accepts MonoVM or a NativeAOT library. | Windows x64 is listed as supported. No player packaging or AOT path is supplied. |
| macOS player | Mono JIT or IL2CPP AOT on x64 and arm64. | Same desktop model as Windows. | macOS arm64 is listed as supported. x64 is not in the supported-platform list. |
| Linux player | Mono JIT or IL2CPP AOT on x64. | Same desktop model as Windows. | Linux x64 is listed as supported. |
| Android player | Mono is documented only for Armv7. IL2CPP supports all Unity targets, so other Android architectures use IL2CPP. | Modern .NET Mono runtime is the default unless the project requests `PublishAot`; the export and loader code also handle a NativeAOT shared library. C# Android support remains documented as experimental. | Not supported. |
| iOS player | IL2CPP AOT. | The Godot SDK forces NativeAOT for iOS. C# iOS support remains documented as experimental. | Not supported. |
| Browser player | IL2CPP converts CIL to C++, then Emscripten compiles it to Wasm. The platform is AOT, has no `Reflection.Emit`, and does not support managed threads. | Godot 4.7 cannot export C# projects to the web. | Not supported. |
| Console player | IL2CPP has the widest support and is required for most consoles. Exact platform details are outside public documentation. | No Foundation-maintained console ports. Licensed developers use private ports or middleware, so there is no public, general C# runtime matrix. | Not supported or discussed. |
| Selection UX | `Player Settings > Configuration > Scripting Backend`, scoped to a build target, plus a scripting API. | No runtime selector in the export options. Platform props and the project file drive runtime publication. | No engine-facing selection model. The application embeds one CoreCLR host. |

Unity sources: [scripting backend overview](https://docs.unity3d.com/6000.5/Documentation/Manual/scripting-backends-intro.html), [Web IL2CPP and Emscripten pipeline](https://docs.unity3d.com/6000.5/Documentation/Manual/webgl-native-plugins-with-emscripten.html), and [Web technical limitations](https://docs.unity3d.com/6000.5/Documentation/Manual/webgl-technical-overview.html).

Godot sources: [C# platform support](https://docs.godotengine.org/en/4.7/tutorials/scripting/c_sharp/index.html), [`gd_mono.cpp` runtime loader](https://github.com/godotengine/godot/blob/4.7.2-stable/modules/mono/mono_gd/gd_mono.cpp), [Android runtime default](https://github.com/godotengine/godot/blob/4.7.2-stable/modules/mono/editor/Godot.NET.Sdk/Godot.NET.Sdk/Sdk/Android.props), [iOS NativeAOT configuration](https://github.com/godotengine/godot/blob/4.7.2-stable/modules/mono/editor/Godot.NET.Sdk/Godot.NET.Sdk/Sdk/iOS.props), and [export implementation](https://github.com/godotengine/godot/blob/4.7.2-stable/modules/mono/editor/GodotTools/GodotTools/Export/ExportPlugin.cs).

Coral sources: [README and supported platforms](https://github.com/StudioCherno/Coral/blob/d53b2685725f7535bc4d1deaa8a22bf16d112fe2/README.md) and [`HostInstance`](https://github.com/StudioCherno/Coral/blob/d53b2685725f7535bc4d1deaa8a22bf16d112fe2/Coral.Native/Source/Coral/HostInstance.cpp).

## Unity precedent

### What Unity exposes

Unity calls the choice a scripting backend. Unity 6.5 offers Mono and IL2CPP in a per-target Player Settings dropdown and through `PlayerSettings.SetScriptingBackend`. Mono JIT is available on Windows, macOS, Linux x64, and Android Armv7. IL2CPP is available on every Unity platform. When a target supports both, Mono is the default. The same manual says the two backends expose nearly identical C# and .NET APIs and use the same base class library and API compatibility levels. AOT restrictions remain visible where they cannot be hidden, notably dynamic code generation, generic instantiation, and stripping. [Unity scripting backend overview](https://docs.unity3d.com/6000.5/Documentation/Manual/scripting-backends-intro.html)

This is a build-target decision, not a project-wide switch. A studio can use Mono for quick desktop iteration and IL2CPP for an iOS, console, or shipping desktop build without maintaining two gameplay APIs. That is the important lesson. The labels identify execution technology users can reason about, while the platform decides which labels are valid.

Crowny should go one step further in its first implementation. An unsupported backend should be absent or visibly disabled with the reason, rather than allowing a late build failure. The chosen backend belongs in the build profile and should also have a command-line and project-file representation so CI produces the same result as the editor.

### Editor and reload behavior

Unity keeps the editor workflow separate from the player backend. Its current backend comparison explicitly associates Mono with faster editor and desktop iteration. Entering Play mode normally reloads the scripting domain and scene. Unity can skip either reload to shorten iteration, but user code must then reset static fields and handlers itself. [Backend comparison](https://docs.unity3d.com/6000.5/Documentation/Manual/scripting-backends-intro.html) [Domain and scene reload sequence](https://docs.unity3d.com/6000.5/Documentation/Manual/configurable-enter-play-mode-details.html)

The useful mechanism is the state handoff. During a code reload Unity serializes eligible fields from loaded scripting objects, reloads the managed code, and restores those fields. Static state is not restored. [How Unity uses serialization](https://docs.unity3d.com/6000.5/Documentation/Manual/script-serialization-how-unity-uses.html)

Crowny needs the same explicit reload transaction:

1. Stop script callbacks and reject new managed work.
2. Capture backend-neutral state for live script instances.
3. Release native and managed handles, unsubscribe callbacks, and stop script-owned work.
4. Unload the user-code context and prove that it was collected.
5. Load and validate the new assembly metadata.
6. Rebind instances by stable script identity and restore compatible fields.
7. Resume callbacks only after the whole scene is consistent.

An `AssemblyLoadContext.Unload` call alone is not a hot-reload feature. State transfer and lifetime diagnostics are part of the feature.

### AOT, metadata, and serialization

IL2CPP first compiles C# to managed assemblies, strips them, converts the remaining CIL to C++, and invokes the platform compiler. Unity's linker cannot always discover members reached through reflection, so users must preserve them with attributes or `link.xml`. [Managed code stripping](https://docs.unity3d.com/6000.5/Documentation/Manual/managed-code-stripping.html) [Backend restrictions comparison](https://docs.unity3d.com/6000.5/Documentation/Manual/scripting-backends-intro.html)

Unity therefore demonstrates two separate compatibility contracts:

- Source compatibility keeps the gameplay API similar across Mono and IL2CPP.
- Build metadata tells the AOT pipeline which types, methods, generic instantiations, and reflected members must survive.

Crowny should generate that metadata from the same script schema that drives the editor inspector. Asking users to maintain a second hand-written preservation file would make AOT failures easy to introduce and hard to review.

Serialized scene data must not use a runtime pointer, `System.Type` handle, metadata token, or backend-specific assembly handle as identity. The durable key should contain a stable script or type identifier plus stable field identifiers and declared data types. Assembly-qualified names can assist migration and diagnostics, but they are too easy to change to be the sole saved identity.

### Performance and maintenance lesson

Unity documents broad tradeoffs rather than a universal performance winner. Mono builds faster and pays JIT costs. IL2CPP takes longer to build, commonly produces larger native code, removes JIT warmup, and has stricter AOT behavior. Those statements do not predict Crowny's callback cost or frame time. Crowny still needs measurements.

Unity also owns an IL-to-C++ compiler, linker integration, runtime support library, debugger integration, and every platform port. That investment is far beyond this engine's sensible maintenance budget. Crowny should adopt Unity's backend boundary and UX, but reject an in-house IL2CPP equivalent.

## Godot precedent

### What Godot 4.7.2 actually runs

The directory and module names still contain `mono`, but Godot 4 uses modern .NET hosting. The .NET-enabled editor locates hostfxr, initializes a compatible runtime from a runtime configuration, loads the managed bootstrap, and uses a collectible project `AssemblyLoadContext`. Desktop exports use `dotnet publish` and normally carry CoreCLR. The native loader can also attach to a MonoVM with CoreCLR-compatible hosting exports or load a NativeAOT game library. [`gd_mono.cpp`](https://github.com/godotengine/godot/blob/4.7.2-stable/modules/mono/mono_gd/gd_mono.cpp) [`GodotPlugins/Main.cs`](https://github.com/godotengine/godot/blob/4.7.2-stable/modules/mono/glue/GodotSharp/GodotPlugins/Main.cs)

The platform SDK props make the mobile split concrete. Android sets `UseMonoRuntime=true` unless `PublishAot=true`. iOS sets `PublishAot`, `PublishAotUsingRuntimePack`, and `UseNativeAOTRuntime` to true. [Android props](https://github.com/godotengine/godot/blob/4.7.2-stable/modules/mono/editor/Godot.NET.Sdk/Godot.NET.Sdk/Sdk/Android.props) [iOS props](https://github.com/godotengine/godot/blob/4.7.2-stable/modules/mono/editor/Godot.NET.Sdk/Godot.NET.Sdk/Sdk/iOS.props)

This is strong evidence for a Crowny-owned host over a third-party wrapper. Godot's native side owns runtime discovery and boot, but the managed bootstrap owns the operations that .NET represents naturally, such as assembly dependency resolution, load contexts, script type discovery, exception handling, and generated callbacks.

### Backend selection UX

Godot's runtime choice is less discoverable than Unity's. The 4.7.2 export panel exposes debug symbols, source inclusion, embedding, and one Android libc option. It does not expose CoreCLR, Mono, or NativeAOT as a backend setting. Runtime choice comes from the target and MSBuild properties. [`ExportPlugin.cs`](https://github.com/godotengine/godot/blob/4.7.2-stable/modules/mono/editor/GodotTools/GodotTools/Export/ExportPlugin.cs)

Crowny should adopt Godot's use of ordinary .NET publish artifacts, runtime identifiers, and target-specific project properties. It should reject the hidden selection. The editor should translate a visible build-profile choice into pinned publish properties, then show the resolved runtime, target framework, runtime identifier, AOT mode, trimming mode, and unavailable-backend reason before building.

### Bindings and interop

Godot uses generated code on both sides of the managed boundary. The managed bootstrap exposes native entry points with `UnmanagedCallersOnly`. A generated callback table lets managed code call native engine functions without resolving each call by name. C# source generators produce script registration, property access, method, signal, and serialization helpers. [Managed game bootstrap generator](https://github.com/godotengine/godot/blob/4.7.2-stable/modules/mono/editor/Godot.NET.Sdk/Godot.SourceGenerators/GodotPluginsInitializerGenerator.cs) [Native callback table](https://github.com/godotengine/godot/blob/4.7.2-stable/modules/mono/glue/GodotSharp/GodotSharp/Core/NativeInterop/NativeFuncs.cs) [Script property generator](https://github.com/godotengine/godot/blob/4.7.2-stable/modules/mono/editor/Godot.NET.Sdk/Godot.SourceGenerators/ScriptPropertiesGenerator.cs) [Script serialization generator](https://github.com/godotengine/godot/blob/4.7.2-stable/modules/mono/editor/Godot.NET.Sdk/Godot.SourceGenerators/ScriptSerializationGenerator.cs)

Some metadata work still uses reflection, especially script lookup and invoking generated metadata helpers. The source marks several paths for future source-generation work. That division is reasonable: reflection during load and editor inspection may be acceptable, but a per-frame callback should resolve to a cached, typed thunk or generated function pointer.

Godot's documentation also warns that property access on managed wrappers crosses into the C++ engine and that arrays and strings incur marshalling cost. [Godot C# performance notes](https://docs.godotengine.org/en/4.7/tutorials/scripting/c_sharp/c_sharp_basics.html#performance-of-c-in-godot) This is more relevant to Crowny's performance than a broad CoreCLR-versus-Mono comparison. A fast runtime cannot rescue a binding layer that performs repeated lookup, allocation, string conversion, boxing, or generic reflection on every callback.

### Reload and state

Godot creates the project load context as collectible in the editor. Its unload code drops the strong context reference, repeatedly forces collection and finalization, and reports likely roots such as strong GC handles or running threads if the context remains alive. [`GodotPlugins/Main.cs`](https://github.com/godotengine/godot/blob/4.7.2-stable/modules/mono/glue/GodotSharp/GodotPlugins/Main.cs) The custom [`PluginLoadContext`](https://github.com/godotengine/godot/blob/4.7.2-stable/modules/mono/glue/GodotSharp/GodotPlugins/PluginLoadContext.cs) uses `AssemblyDependencyResolver` and treats engine API assemblies as shared.

That unload diagnostic is worth copying. The user-visible behavior is weaker. Godot documents that hot reload preserves exported variables but not other state. [Godot C# current gotchas](https://docs.godotengine.org/en/4.7/tutorials/scripting/c_sharp/c_sharp_basics.html#current-gotchas-and-known-issues)

Crowny's acceptance gate should be higher because the requested transition is seamless. It should preserve declared serialized script fields, scene references, and stable type identities across Mono and CoreCLR. Transient private runtime state can reset unless Crowny explicitly defines a reload-state annotation or interface. Failed unloads must identify leaked handles, active threads, static event roots, and unresolved tasks instead of silently accumulating old assemblies.

### Web and consoles

Godot 4.7 documents desktop, experimental Android, and experimental iOS C# export support. It explicitly says C# projects cannot export to Web. [Godot C# platform support](https://docs.godotengine.org/en/4.7/tutorials/scripting/c_sharp/index.html#c-platform-support)

This gap is instructive. A clean CoreCLR host abstraction does not automatically produce a browser backend. Web requires its own publish toolchain, engine interop ABI, trimming and metadata rules, JavaScript integration, exception behavior, threading policy, download-size budget, and tests under Emscripten.

Godot does not maintain official closed-console ports. Approved developers must build private ports or use certified middleware. [Godot console support](https://godotengine.org/consoles/) Its public source therefore cannot answer which managed backend is best for a named console. Crowny should keep console constraints out of this ticket's first implementation while avoiding assumptions that require JIT, writable executable memory, dynamic assembly loading, or unrestricted reflection in player code.

### License and ownership

Godot is MIT-licensed and maintained by its contributors. Its .NET integration is part of the engine source, not a separate wrapper project. [Godot licensing](https://docs.godotengine.org/en/4.7/about/complying_with_licenses.html) This makes it valuable implementation reference material, subject to preserving required notices if Crowny copies code. The better course is to learn from its boundaries and write Crowny-specific code around Crowny's existing script system.

## Coral precedent

### What Coral contains

Coral describes itself as a C++ wrapper around hostfxr with a Mono-like native API. A native `HostInstance` locates and loads hostfxr, initializes `Coral.Managed.dll` from a runtime config, acquires `load_assembly_and_get_function_pointer`, and caches a large table of managed entry points. The managed half implements assembly loading, reflection, method and field access, object handles, internal calls, marshalling, garbage collection helpers, and MSBuild invocation. [Coral README](https://github.com/StudioCherno/Coral/blob/d53b2685725f7535bc4d1deaa8a22bf16d112fe2/README.md) [`HostInstance.cpp`](https://github.com/StudioCherno/Coral/blob/d53b2685725f7535bc4d1deaa8a22bf16d112fe2/Coral.Native/Source/Coral/HostInstance.cpp) [`AssemblyLoader.cs`](https://github.com/StudioCherno/Coral/blob/d53b2685725f7535bc4d1deaa8a22bf16d112fe2/Coral.Managed/Source/AssemblyLoader.cs)

The useful ideas are small and clear:

- Keep hostfxr boot code on the native side and expose a small managed bootstrap.
- Use `UnmanagedCallersOnly` entry points and function pointers for the stable boundary.
- Put user assemblies in collectible load contexts.
- Route managed exceptions to an engine callback.
- Centralize handle and marshalling rules instead of scattering runtime calls through engine systems.

These ideas are already available in the platform APIs and are straightforward to implement in a Crowny-specific layer.

### Why Crowny should not adopt it

Coral's supported list is Windows x64, Linux x64, and macOS arm64. Its build targets .NET 9, and `HostInstance.cpp` manually searches conventional installation folders for a hostfxr directory whose first character is `9`. The README says the .NET SDK must be installed. This is unsuitable for a self-contained editor distribution and brittle under servicing, app-local runtimes, nonstandard installation paths, and future major versions. [Supported platforms](https://github.com/StudioCherno/Coral/blob/d53b2685725f7535bc4d1deaa8a22bf16d112fe2/README.md#supported-platforms) [Runtime discovery](https://github.com/StudioCherno/Coral/blob/d53b2685725f7535bc4d1deaa8a22bf16d112fe2/Coral.Native/Source/Coral/HostInstance.cpp) [Managed target framework](https://github.com/StudioCherno/Coral/blob/d53b2685725f7535bc4d1deaa8a22bf16d112fe2/Coral.Managed/Coral.Managed-Static.csproj)

The code is also a generic reflection facade rather than an engine scripting architecture:

- Load-context and assembly identifiers derive from string hash codes.
- Process-wide static function tables and callbacks limit isolation.
- Unload clears Coral caches and calls `AssemblyLoadContext.Unload`, but does not verify collection as Godot does.
- Native callers perform broad type, method, field, property, attribute, and invocation operations through managed reflection.
- There is no platform publish matrix, AOT preservation generator, WebAssembly bridge, console policy, serialized scene schema, editor state handoff, or backend capability model.

These are not reasons to dismiss the project. Coral has a deliberately smaller goal. They are reasons not to make it Crowny's critical runtime boundary. A fork would transfer maintenance ownership to Crowny while keeping abstractions designed for another engine. A clean-room, Crowny-owned host can stay smaller because it only needs Crowny's actual operations.

Coral is MIT-licensed. [Coral license](https://github.com/StudioCherno/Coral/blob/d53b2685725f7535bc4d1deaa8a22bf16d112fe2/LICENSE) The license permits reuse, but license permissiveness does not change the platform and maintenance findings.

## What Crowny should adopt and reject

| Decision | Adopt | Reject |
| --- | --- | --- |
| Product model | Unity's per-build-target backend setting with target-aware availability and one shared C# API. | One global runtime choice that must work on every platform. |
| Editor/player split | A JIT editor optimized for build, debug, exception, inspector, and reload workflows; separately published player backends. | Requiring the editor to execute the same AOT artifact as a shipping player. |
| Host ownership | Godot's engine-owned native host plus managed bootstrap, using standard .NET publish outputs. | Adopting or forking Coral as the public scripting architecture. |
| Runtime implementation | Use maintained .NET runtimes and toolchains behind Crowny adapters. | Building a Crowny IL-to-C++ compiler to imitate IL2CPP. |
| Interop | Generated registration and typed callbacks, cached handles, explicit ownership, and measured marshalling. | Reflection lookup and string dispatch in per-frame callbacks. |
| Reload | Collectible user-code context, state checkpoint and restore, unload verification, and useful leak reports. | Treating `Unload()` as proof that code and objects were released. |
| Serialization | Backend-neutral script and field identifiers with migration aliases and generated metadata. | Persisting runtime handles, metadata tokens, or a backend-specific type object. |
| AOT | Generate roots and generic instantiations from the same schema used by inspectors and serialization. | Requiring users to discover missing AOT metadata only through player crashes. |
| Browser | A dedicated Wasm AOT backend and capability profile. | Assuming desktop hostfxr code can be compiled with Emscripten and become a browser backend. |
| Consoles | Keep the contract compatible with AOT and restricted dynamic features. Validate named consoles only with SDK access. | Claiming console support from desktop, mobile, or public Godot evidence. |

## Concrete requirements for the Crowny plan

The architecture and implementation tickets that follow this research should include these requirements:

1. Define runtime capabilities in code. At minimum: JIT, AOT, dynamic assembly load, collectible load context, managed debugger, reflection level, threads, native dynamic libraries, and WebAssembly.
2. Resolve backend availability from target platform, architecture, build type, installed toolchain, and runtime package. The editor shows only valid choices and explains missing prerequisites.
3. Keep player backend selection in a build profile. Do not make it a scene property or a managed-project-only setting.
4. Pin the target framework, runtime version, runtime identifier, publish flags, and managed package versions. Do not search for any convenient machine-wide runtime in production builds.
5. Preserve existing CrownySharp source APIs where practical. Recompiling scripts is acceptable; rewriting gameplay code for each backend is not.
6. Define durable script type and field identities before adding CoreCLR. The Mono adapter and new adapter must read and write the same scene data.
7. Generate managed registration, native and managed callback declarations, field schemas, serialization thunks, exception boundaries, and AOT roots from one description.
8. Use typed direct calls for lifecycle callbacks and common property access. Confine reflection to loading, validation, inspector discovery, and uncommon editor operations.
9. Build reload as a transaction with state restore and unload diagnostics. Gate CoreCLR editor-default status on reload, debugging, exception reporting, inspectors, and serialization parity.
10. Benchmark the whole boundary. Required cases include empty lifecycle dispatch, primitive and vector calls, strings, arrays, object references, exception paths, assembly load, reload, startup, memory after repeated reloads, AOT build time, binary size, and first-frame latency.
11. Treat WebAssembly as a separate backend project. Reuse the C# API and generated schema, but give the browser its own publish, interop, threading, networking, and test rules.
12. Leave named consoles uncommitted until Crowny can test vendor SDKs. Keep player-facing APIs free of mandatory runtime code generation and dynamic loading so an AOT console adapter remains possible.

## Final assessment

The best combined precedent is Unity outside and Godot inside.

Crowny's UI should resemble Unity's per-target backend model. Its implementation should resemble Godot's owned host and generated bridge, adapted to Crowny's existing Mono API and serialized scenes. Coral should remain a compact code reference for hostfxr bootstrapping and collectible load contexts. It is not the dependency or abstraction on which Crowny should base its long-term scripting system.
