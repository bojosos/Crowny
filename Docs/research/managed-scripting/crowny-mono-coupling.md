# Crowny's Mono coupling and the managed-host seam

Research ticket: [#2](https://github.com/bojosos/Crowny/issues/2)

Audited revision: [`07a0945322f4d45d77fee1c871fac6a96c81957e`](https://github.com/bojosos/Crowny/tree/07a0945322f4d45d77fee1c871fac6a96c81957e)

Date: 2026-08-25

## Decision

Put the public seam above managed scripting as a whole, not between Crowny and a renamed set of reflection wrappers. Engine and editor callers should use one deep `ManagedScripting` module expressed in Crowny terms: program loading, script discovery, instance lifetime, event dispatch, state capture, state application, reload, diagnostics, and backend capabilities. An internal adapter interface should select Mono, CoreCLR, or an AOT implementation.

The existing `Scripting/Mono` directory is a useful starting implementation for the Mono adapter, but its `MonoClass`, `MonoMethod`, `MonoObject*`, `MonoReflectionType*`, and thunk types must stay behind the internal seam. Copying that interface as `ManagedClass`, `ManagedMethod`, and `ManagedObject` would preserve nearly all current coupling and would fit reflection-heavy JIT runtimes much better than AOT targets.

Do not rename serialized `MonoScript` concepts during the first migration. Preserve their numeric and text identities as compatibility aliases while code moves behind the new module. Crowny already persists scripts by the runtime-neutral triple of assembly, namespace, and type name, and it retains missing script data. That is the right foundation to keep.

## Scope and counting method

I inspected the clean research worktree at the revision above. I also inspected the user's dirty main checkout read-only because it contains ongoing serialization work. None of the dirty checkout's changes were copied into this report branch, and none are included in the counts below.

The runtime-type count searched C++ headers and implementations for these identifiers, then excluded `Crowny/Source/Crowny/Scripting/Mono` itself:

```text
MonoManager MonoAssembly MonoClass MonoMethod MonoField MonoProperty
MonoArray MonoObject MonoString MonoReflectionType MonoClassField
MonoDomain MonoImage MonoException MonoPrimitiveType CrownyMonoVisibility
```

This produced 998 occurrences in 101 files. The count does not include the domain class names `MonoScript` and `MonoScriptComponent`; those are measured separately. The audit also found 72 direct `Scripting/Mono` includes in 32 files and 65 direct `mono_*` C interface uses in 9 files outside the Mono adapter.

| Area outside `Scripting/Mono` | Files | Runtime-type occurrences | What is coupled |
| --- | ---: | ---: | --- |
| Native bindings | 64 | 386 | Managed call ABI, strings, arrays, objects, reflection types |
| Scripting core | 14 | 214 | Wrapper lifetime, type registration, object lookup |
| Script serialization | 8 | 167 | Schema, member access, boxing, collections |
| Engine outside scripting | 6 | 127 | Startup, scene execution, ECS data, collisions |
| Editor | 7 | 84 | Compilation, reload, inspector, reflection debug UI |
| Tests | 2 | 20 | Mono-specific utilities and reload assertions |
| Total | 101 | 998 | |

The high count is partly caused by binding declarations, but the coupling is not confined there. [`Common/Types.h`](https://github.com/bojosos/Crowny/blob/07a0945322f4d45d77fee1c871fac6a96c81957e/Crowny/Source/Crowny/Common/Types.h#L1-L24) includes `Mono.h` and declares Mono wrapper types. This makes Mono definitions available through one of the engine's most widely used headers.

The names `MonoScript` and `MonoScriptComponent` appear another 203 times in 23 files. Some of these are implementation details, while others are serialized names or public ECS terminology. Treating all 203 as a rename job would create churn without removing runtime coupling.

## Where Mono leaks today

### Process startup and program loading

`EngineRuntime` resolves Mono installation paths, chooses a debugger port, starts `MonoManager`, registers bindings, loads `CrownySharp` and `GameAssembly`, and asks `ScriptInfoManager` to reflect their types. The entire order is spelled in Mono terms in [`EngineRuntime.cpp`](https://github.com/bojosos/Crowny/blob/07a0945322f4d45d77fee1c871fac6a96c81957e/Crowny/Source/Crowny/Application/EngineRuntime.cpp#L91-L106) and again in its runtime-service startup path ([lines 276-323](https://github.com/bojosos/Crowny/blob/07a0945322f4d45d77fee1c871fac6a96c81957e/Crowny/Source/Crowny/Application/EngineRuntime.cpp#L276-L323)). `ApplicationDesc` has debug and profiling switches plus two assembly paths, but no backend selection or capability model ([`Application.h`, lines 22-44](https://github.com/bojosos/Crowny/blob/07a0945322f4d45d77fee1c871fac6a96c81957e/Crowny/Source/Crowny/Application/Application.h#L22-L44)).

The current Mono implementation creates an app domain on the first assembly load, then initializes registered native wrappers from reflected classes ([`MonoManager.cpp`, lines 171-198](https://github.com/bojosos/Crowny/blob/07a0945322f4d45d77fee1c871fac6a96c81957e/Crowny/Source/Crowny/Scripting/Mono/MonoManager.cpp#L171-L198), [lines 261-280](https://github.com/bojosos/Crowny/blob/07a0945322f4d45d77fee1c871fac6a96c81957e/Crowny/Source/Crowny/Scripting/Mono/MonoManager.cpp#L261-L280)). Domain lifetime is therefore mixed with program lifetime. That distinction matters for CoreCLR and AOT implementations, which will not reproduce Mono app domains exactly.

### ECS script data and hot callbacks

`ScriptTypeIdentity` is already runtime-neutral. It stores assembly, namespace, and type name ([`Components.h`, lines 569-578](https://github.com/bojosos/Crowny/blob/07a0945322f4d45d77fee1c871fac6a96c81957e/Crowny/Source/Crowny/Ecs/Components.h#L569-L578)). The class immediately below it is not. `MonoScript` stores `MonoObject*`, `MonoClass*`, `MonoReflectionType*`, Mono exception thunks, and native function pointers for every lifecycle and physics callback ([lines 586-688](https://github.com/bojosos/Crowny/blob/07a0945322f4d45d77fee1c871fac6a96c81957e/Crowny/Source/Crowny/Ecs/Components.h#L586-L688)).

Script creation performs Mono class lookup and reflection, creates either the requested object or `MissingEntityBehaviour`, and attaches it through `ScriptSceneObjectManager` ([`Components.cpp`, lines 924-965](https://github.com/bojosos/Crowny/blob/07a0945322f4d45d77fee1c871fac6a96c81957e/Crowny/Source/Crowny/Ecs/Components.cpp#L924-L965)). Callback discovery walks base classes and caches Mono unmanaged thunks ([lines 984-1097](https://github.com/bojosos/Crowny/blob/07a0945322f4d45d77fee1c871fac6a96c81957e/Crowny/Source/Crowny/Ecs/Components.cpp#L984-L1097)). Collision conversion also calls Mono's raw array functions directly from the ECS implementation ([lines 1172-1217](https://github.com/bojosos/Crowny/blob/07a0945322f4d45d77fee1c871fac6a96c81957e/Crowny/Source/Crowny/Ecs/Components.cpp#L1172-L1217), [lines 1263-1316](https://github.com/bojosos/Crowny/blob/07a0945322f4d45d77fee1c871fac6a96c81957e/Crowny/Source/Crowny/Ecs/Components.cpp#L1263-L1316)).

`ScriptRuntime` is otherwise close to a useful caller. It snapshots script identities to tolerate scene changes during callbacks, dispatches start, update, and destroy, and clears runtime instances. Its remaining runtime check and assembly unloading still call `MonoManager` directly ([`ScriptRuntime.cpp`, lines 22-50](https://github.com/bojosos/Crowny/blob/07a0945322f4d45d77fee1c871fac6a96c81957e/Crowny/Source/Crowny/Scene/ScriptRuntime.cpp#L22-L50), [lines 58-83](https://github.com/bojosos/Crowny/blob/07a0945322f4d45d77fee1c871fac6a96c81957e/Crowny/Source/Crowny/Scene/ScriptRuntime.cpp#L58-L83), [lines 137-157](https://github.com/bojosos/Crowny/blob/07a0945322f4d45d77fee1c871fac6a96c81957e/Crowny/Source/Crowny/Scene/ScriptRuntime.cpp#L137-L157)).

### Reflection, serialization, and inspection

`SerializableTypeInfo` looks like neutral schema data but requires every subtype to return `::MonoClass*`. Lists and dictionaries store raw Mono classes, object metadata stores a `MonoClass*`, and member metadata exposes `MonoObject*`, `MonoField*`, and `MonoProperty*` ([`SerializableObjectInfo.h`, lines 65-170](https://github.com/bojosos/Crowny/blob/07a0945322f4d45d77fee1c871fac6a96c81957e/Crowny/Source/Crowny/Scripting/Serialization/SerializableObjectInfo.h#L65-L170), [lines 172-240](https://github.com/bojosos/Crowny/blob/07a0945322f4d45d77fee1c871fac6a96c81957e/Crowny/Source/Crowny/Scripting/Serialization/SerializableObjectInfo.h#L172-L240)). Schema and runtime access are one module today. This is the hardest leak because scene persistence, reload, and the editor all depend on it.

`ScriptInfoManager` also publishes a large `BuiltinScriptClasses` table made entirely of `MonoClass*`, keys component and asset maps by `MonoReflectionType*`, and returns reflected entity behaviour classes ([`ScriptInfoManager.h`, lines 13-85](https://github.com/bojosos/Crowny/blob/07a0945322f4d45d77fee1c871fac6a96c81957e/Crowny/Source/Crowny/Scripting/ScriptInfoManager.h#L13-L85), [lines 87-176](https://github.com/bojosos/Crowny/blob/07a0945322f4d45d77fee1c871fac6a96c81957e/Crowny/Source/Crowny/Scripting/ScriptInfoManager.h#L87-L176)).

The editor inspector reads boxed Mono values, queries attribute objects, converts Mono strings, and invokes collection methods itself. Even its header accepts `MonoObject*` getters and instances ([`ScriptInspector.h`, lines 5-30](https://github.com/bojosos/Crowny/blob/07a0945322f4d45d77fee1c871fac6a96c81957e/Crowny-Editor/Source/UI/ScriptInspector.h#L5-L30)). The list editor is a representative example of direct reflection and invocation in UI code ([`ScriptInspector.cpp`, lines 159-190](https://github.com/bojosos/Crowny/blob/07a0945322f4d45d77fee1c871fac6a96c81957e/Crowny-Editor/Source/UI/ScriptInspector.cpp#L159-L190)). This must become schema plus value editing. AOT metadata can then come from generated data instead of runtime reflection.

### Native bindings

`ScriptBindings::Register` initializes 42 wrapper types in one fixed list ([`ScriptBindings.cpp`, lines 45-89](https://github.com/bojosos/Crowny/blob/07a0945322f4d45d77fee1c871fac6a96c81957e/Crowny/Source/Crowny/Scripting/Bindings/ScriptBindings.cpp#L45-L89)). Across the binding implementations, 466 calls register native functions through `MonoClass::AddInternalCall`. That method is a direct one-line call to `mono_add_internal_call` ([`MonoClass.cpp`, line 107](https://github.com/bojosos/Crowny/blob/07a0945322f4d45d77fee1c871fac6a96c81957e/Crowny/Source/Crowny/Scripting/Mono/MonoClass.cpp#L107)).

The native operation behind a binding can usually be shared, but its managed call ABI cannot. String ownership, array layout, object handles, exception transport, and how a native entry point is exposed differ by backend. The new design should keep a shared logical binding manifest and native engine operations, while each adapter owns marshalling and entry-point registration. Do not put arbitrary `void*` calls or `AddInternalCall` in the public managed-scripting interface.

### Reload, compiler, diagnostics, and build setup

Reload currently lives in `ScriptObjectManager`. It validates assemblies in a temporary Mono domain, snapshots persistent wrappers, unloads the script domain, loads assemblies, and restores the last working program on failure ([`ScriptObjectManager.cpp`, lines 24-118](https://github.com/bojosos/Crowny/blob/07a0945322f4d45d77fee1c871fac6a96c81957e/Crowny/Source/Crowny/Scripting/ScriptObjectManager.cpp#L24-L118)). This is good user-visible behavior, but the caller knows Mono's domain mechanism. Reload should be one operation of the deep module. Each adapter can use an app domain, a collectible `AssemblyLoadContext`, process replacement, or no reload at all.

The editor compiles by reflecting `Crowny.ScriptCompiler`, passing Mono strings and arrays, then calling `ScriptObjectManager::RefreshAssemblies` ([`EditorLayerProject.cpp`, lines 429-506](https://github.com/bojosos/Crowny/blob/07a0945322f4d45d77fee1c871fac6a96c81957e/Crowny-Editor/Source/Editor/EditorLayerProject.cpp#L429-L506)). Generated projects target .NET Framework 4.8 ([`ScriptProjectGenerator.cpp`, lines 39-87](https://github.com/bojosos/Crowny/blob/07a0945322f4d45d77fee1c871fac6a96c81957e/Crowny-Editor/Source/Editor/Script/ScriptProjectGenerator.cpp#L39-L87)). Compilation is a build concern and should select output for the chosen backend. It should not be implemented by invoking a runtime-specific managed helper inside the active game program.

Build setup is likewise global. Premake resolves a Mono root, links Mono on Windows and Linux, and copies the Mono runtime DLL on Windows ([`premake5.lua`, lines 126-130](https://github.com/bojosos/Crowny/blob/07a0945322f4d45d77fee1c871fac6a96c81957e/premake5.lua#L126-L130), [lines 181-210](https://github.com/bojosos/Crowny/blob/07a0945322f4d45d77fee1c871fac6a96c81957e/premake5.lua#L181-L210), [lines 246-253](https://github.com/bojosos/Crowny/blob/07a0945322f4d45d77fee1c871fac6a96c81957e/premake5.lua#L246-L253)). The Windows setup script installs Mono and uses `mcs` for both engine and game assemblies ([`setup-windows.ps1`, lines 159-183](https://github.com/bojosos/Crowny/blob/07a0945322f4d45d77fee1c871fac6a96c81957e/Scripts/setup-windows.ps1#L159-L183), [lines 228-252](https://github.com/bojosos/Crowny/blob/07a0945322f4d45d77fee1c871fac6a96c81957e/Scripts/setup-windows.ps1#L228-L252)). Premake skips the Mono link for the Web platform, but the engine sources and common headers still depend on Mono. The build filter alone does not make scripting portable to WebAssembly.

### Serialized compatibility

The persistence format is better separated than the runtime implementation. YAML and binary scenes write assembly, namespace, type name, and serialized fields. Older scenes still map their short script names to `GameAssembly` and `Sandbox` ([`SceneComponentCodec.cpp`, lines 626-710](https://github.com/bojosos/Crowny/blob/07a0945322f4d45d77fee1c871fac6a96c81957e/Crowny/Source/Crowny/Serialization/SceneComponentCodec.cpp#L626-L710), [lines 713-749](https://github.com/bojosos/Crowny/blob/07a0945322f4d45d77fee1c871fac6a96c81957e/Crowny/Source/Crowny/Serialization/SceneComponentCodec.cpp#L713-L749)). `SceneComponentId::MonoScript` is numeric ID 8, and the YAML codec name is `MonoScriptComponent` ([`SceneComponentCodec.h`, lines 18-32](https://github.com/bojosos/Crowny/blob/07a0945322f4d45d77fee1c871fac6a96c81957e/Crowny/Source/Crowny/Serialization/SceneComponentCodec.h#L18-L32), [`SceneComponentCodec.cpp`, lines 1618-1626](https://github.com/bojosos/Crowny/blob/07a0945322f4d45d77fee1c871fac6a96c81957e/Crowny/Source/Crowny/Serialization/SceneComponentCodec.cpp#L1618-L1626)). Keep both values readable even if the C++ type later becomes `ManagedScriptComponent`.

Existing tests already cover missing-script retention, legacy scene loading, and state restoration after an assembly returns ([`SceneSerializationTests.cpp`, lines 189-211](https://github.com/bojosos/Crowny/blob/07a0945322f4d45d77fee1c871fac6a96c81957e/Crowny-Tests/Source/SceneSerializationTests.cpp#L189-L211), [lines 211-315](https://github.com/bojosos/Crowny/blob/07a0945322f4d45d77fee1c871fac6a96c81957e/Crowny-Tests/Source/SceneSerializationTests.cpp#L211-L315), [lines 315-367](https://github.com/bojosos/Crowny/blob/07a0945322f4d45d77fee1c871fac6a96c81957e/Crowny-Tests/Source/SceneSerializationTests.cpp#L315-L367)). These are valuable compatibility tests, but most runtime behavior tests still construct or assert Mono types directly.

## Seam candidates

| Candidate | Advantage | Cost | Verdict |
| --- | --- | --- | --- |
| Generic reflection wrappers replacing `MonoClass`, `MonoMethod`, and friends | Small first diff; much existing code can be renamed | Large shallow interface; callers still own boxing, reflection, invocation, and GC rules; poor fit for generated AOT metadata | Reject as the public seam. These wrappers may exist privately inside an adapter. |
| Seam at `ScriptInfoManager` | Removes some reflection from serialization and the inspector | Startup, instances, reload, bindings, callbacks, and build logic still bypass it | Too narrow. |
| Seam at `ScriptRuntime` | Scene update code becomes backend-neutral | The editor, serialization, native wrappers, assets, and compiler still talk to Mono | Too high and too narrow. |
| Domain-level `ManagedScripting` module | One place owns program and instance lifetime, schema, state, events, reload, diagnostics, and capabilities | Requires staged movement of metadata, bindings, and editor code | Recommended. Callers get more behavior through one interface, and backend knowledge stays local. |

Place the public seam in `Crowny/Source/Crowny/Scripting/Managed`, or an equivalent runtime-neutral directory. Put adapters below it, for example `Scripting/Backends/Mono`, `Scripting/Backends/CoreCLR`, and later AOT-specific directories. The exact names matter less than the include rule: files outside an adapter must not include its runtime headers or mention its runtime types.

## The smallest useful deep interface

The public `ManagedScripting` module should expose use cases, not reflection machinery. The following is the smallest set supported by current callers:

1. Module startup and shutdown with a `ManagedBackendId`, backend paths, debugging settings, and a capability result.
2. `LoadProgram` and `ReloadProgram` using engine assembly artifacts, game assembly artifacts, generated metadata when present, and symbols. Reload owns validation, backup, rollback, and restoration.
3. `GetScriptCatalog`, returning runtime-neutral type and field schemas for creation, serialization, and the inspector.
4. `CreateScript` and `DestroyScript`, using `ScriptTypeIdentity`, an entity identity, an optional persisted state, and an opaque generational `ScriptInstanceHandle`.
5. `Dispatch`, using a `ScriptEvent` value for lifecycle, collision, and trigger callbacks.
6. `CaptureState` and `ApplyState`, using a runtime-neutral `ScriptValue` tree rather than a managed object pointer.
7. `Update`, which pumps finalization and other backend work, and returns structured diagnostics.

`ManagedCapabilities` should at least say whether a backend supports dynamic program loading, reload, runtime reflection, managed debugging, profiling, and AOT-only execution. The editor and build presets can then hide unsupported choices without testing backend names throughout the UI.

The data that crosses this seam should be Crowny-owned:

- `ScriptTypeIdentity` remains the persisted assembly, namespace, and type name.
- `ScriptInstanceHandle` is opaque and invalidated by generation. An adapter may map it to a Mono GC handle, CoreCLR handle, or AOT table entry.
- `ScriptTypeSchema` and `ScriptFieldSchema` contain normalized flags and attribute values. They contain no runtime class or member pointer.
- `ScriptValue` represents supported primitives, strings, math types, entity and asset identities, enums, arrays, lists, dictionaries, and nested objects.
- `ScriptEvent` contains a closed event kind and a typed native payload. It does not contain a managed array or object.
- `ManagedDiagnostic` contains severity, message, managed stack text, and backend name. Mono exceptions are currently unwrapped and logged inside `MonoUtils` ([`MonoUtils.h`, lines 63-68](https://github.com/bojosos/Crowny/blob/07a0945322f4d45d77fee1c871fac6a96c81957e/Crowny/Source/Crowny/Scripting/Mono/MonoUtils.h#L63-L68), [`MonoUtils.cpp`, lines 42-64](https://github.com/bojosos/Crowny/blob/07a0945322f4d45d77fee1c871fac6a96c81957e/Crowny/Source/Crowny/Scripting/Mono/MonoUtils.cpp#L42-L64)).

The adapter interface is internal to the `ManagedScripting` implementation. Engine, scene, serialization, and editor code should not depend on it. This keeps one adapter's extra features from expanding the public interface.

## Safe migration order

1. Land and protect type identity work first. The dirty main checkout currently adds assembly-qualified nested enum and object identities, exact nested names, legacy leaf-name matching, scene-format versioning, and compatibility tests. Rebase this effort after that work lands. Do not recreate or overwrite it.
2. Freeze compatibility with characterization tests. Cover the existing YAML and binary formats, missing types, `FormerlySerializedAs`, reload rollback, lifecycle order, collision payloads, exception text, inspector edits, and native wrapper lifetime. Run these tests against every adapter.
3. Add runtime-neutral data beside the old types. Start with `ScriptTypeIdentity`, schema records, `ScriptValue`, `ScriptEvent`, opaque handles, diagnostics, and capabilities. Keep `MonoScript` and `MonoScriptComponent` as names or aliases so serialized scenes and broad caller code do not change at the same time.
4. Introduce `ManagedScripting` with only the Mono adapter. Move `EngineRuntime`, `ScriptRuntime`, and scene script creation to use-case operations. Replace the old path as each operation moves instead of retaining two permanent orchestration layers.
5. Move reflection and state ownership behind the module. Split `SerializableObjectInfo` into runtime-neutral schema and adapter-private access data. Change the inspector to edit schema plus `ScriptValue`. This removes the most expensive cross-cutting dependency before adding another backend.
6. Split binding semantics from marshalling. Keep native engine operations shared. Generate or declare a logical binding manifest and implement Mono marshalling inside the Mono adapter. Preserve managed class, member, and internal-call names during this phase.
7. Move compilation out of the active managed program. Build CrownySharp and game scripts through a backend-aware build module. Keep the current compile, validate, reload, and last-good rollback workflow for the Mono editor preset.
8. Add the second desktop adapter and run the same contract tests. This turns the seam from a hypothetical one into a real one. Only then remove remaining direct Mono includes from non-adapter code and add a source check that prevents them from returning.
9. Add AOT adapters through generated metadata and binding tables. Do not require reflection or runtime assembly loading from the public interface. Backends without reload or debugging report that through capabilities, and the editor hides those presets where they cannot work.
10. Consider source-level renames last. Keep readers for `SceneComponentId::MonoScript` and `MonoScriptComponent` indefinitely unless a deliberate scene migration changes the persisted format.

## Completion criteria for the seam

The seam is real when all of these conditions hold:

- Engine, scene, editor, and serialization code include no Mono headers and use no Mono runtime types.
- `Common/Types.h` no longer includes `Mono.h`.
- Only the Mono adapter and its adapter-specific tests call `mono_*`.
- The editor inspector operates on runtime-neutral schema and values.
- Reload rollback and missing-script retention pass through the public module interface.
- Mono and one non-Mono adapter pass the same contract suite.
- A no-reflection fake or generated-metadata adapter can load a catalog, create a script, dispatch an event, and round-trip state. That test is the practical proof that the interface does not assume JIT reflection.

This route costs more than renaming the current wrappers, but it pays for the second adapter and the later AOT work. More importantly, it concentrates runtime-specific ownership, invocation, reflection, and marshalling in one place instead of making every future backend imitate Mono's object model.
