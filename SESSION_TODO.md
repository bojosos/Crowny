# Session TODO (Claude, 2026-09-01)

Own working list for the current editor polish pass. Not part of the engine backlogs.

## Fixed
- [x] Recent projects list rows overlapped vertically (cursor not restored after overlay text).
- [x] Removed "Renderer test values" section from Settings.

## Open
- [x] Opening a project does not clear the scene from the previously opened project.
- [x] Build window scene selector: forced save on select, then duplicate entries (with and without `.cwscene`).
- [x] Project selection search bar starts a few pixels tall and expands over several frames.
- [x] Wireframe rendering hard to reach; add Unity-style viewport overlay toolbar (top-right): render mode toggle (solid/wireframe/...), GPU stats toggle. GPU stats overlay itself anchored on the left.
- [x] Code editor picker (Settings > Code editor) broken: ImGui ID conflict.
- [x] Mesh Filter asset search popup: ID conflicts, same mesh listed many times.
- [x] Asset browser mesh previews too dark; `.glb` has no preview. (`.gltf` viewport drop fixed: entries without .meta are reimported on drop.)
- [x] Drag and drop files from Windows Explorer into asset browser / viewport. (Viewport side done: WindowFileDropEvent + import + spawn; asset browser side pending.)
- [x] Asset browser panel does not refresh when files are added externally (asset tree does); needs manual Reload.
- [x] Asset browser: breadcrumb path misaligned with toolbar; List/Grid toggle selection state hard to see.
- [x] Console: category (Info/...) selector needs left padding.

## Open (batch 2)
- [x] Hierarchy: "Create > Light" (directional/point/spot) and "Create > 3D Object" primitives (cube, sphere, plane, cylinder, cone, capsule) using MeshFactory.
- [x] Material editing workflow: after creating a material, inspector lets you pick a shader (engine built-ins + project shaders) and shows its parameters.
- [x] Viewport gizmo toolbar icons (top-left) are stretched/squished.
- [x] Notifications (top-right) never disappear and have no close (X) button.
- [x] Text component inspector: group advanced options into collapsible rollouts (Layout, Rendering, ...).
- [x] Replace `(void)var;` with CW_MAYBE_UNUSED everywhere except ManagedHostBindings.cpp (5 sites, user WIP).
- [x] Entity inspector header takes far too much vertical space.

## Open (batch 3)
- [x] Move toon presets out of Material.cpp into data-driven MaterialPreset assets (built-ins shipped in Builtin.cwpack, user presets in project, enumerated in inspector; keep ToonMaterialPreset enum as ABI shim).
- [x] `return {};` for Entity -> `return Entity::Invalid;` (static const). All sites converted except ManagedHostBindings.cpp (user WIP).
- [x] Script lifecycle parity with Unity: add LateUpdate, verify OnDestroy fires on entity destroy / scene stop; audit Awake/OnEnable/OnDisable.
- [x] Inspector file reorganisation: ComponentEditor -> EntityInspector; script widget + new-script flow -> ScriptComponentInspector.cpp/.h; ComponentRenderer.cpp deleted (it was 100% script code); ComponentInspector.cpp keeps built-in widgets; `if constexpr` script cases replaced by ComponentInspectorTraits<T> + explicit specialisations. SelectionProperty.h overload/Member bugs fixed (these caused the cl.exe 'hangs').
- [x] Primitive meshes: EngineRuntime::StartRenderer registers PrimitiveMeshLibrary (and shuts it down) for editor and player; asset reference buttons fall back to the primitive name ("Cube (built-in)") via UIUtils::GetAssetDisplayName.

## Resume notes (paused 2026-09-02, usage limit)
Agents were stopped mid-work on these; check the files for partial edits before continuing:
- Mesh Filter asset search duplicates + Console category padding + Text component rollouts: ComponentEditor.cpp, ComponentInspector.cpp, ConsolePanel.cpp, UIUtils search widget, new UI/AssetSearchCandidates.h.
- Asset browser (previews too dark, .glb thumbnails, auto-refresh, breadcrumb alignment, Grid/List state, WindowFileDropEvent handler): AssetBrowserPanel.cpp/.h; agent had a Crowny-Tests build pending.
- Material workflow (shader picker, compact inspector header, MaterialPreset assets replacing toon presets in Material.cpp): InspectorPanel.cpp, MaterialInspectorSchemaCache, Material.*; likely barely started.
- Script lifecycle (LateUpdate, OnDestroy coverage, Awake/OnEnable/OnDisable audit): ScriptRuntime.*, Scene.cpp, Scripting/**, Crowny-Sharp; likely barely started.
Untracked new files from agents: Editor/SelectionComponentOperations.h, Editor/SelectionProperty.h, UI/SelectionProperties.h, UI/SelectionPropertyLayout.h and their Crowny-Tests (from the mesh-search agent's refactor) — verify they are registered in the vcxproj and compile.
Build tip: msbuild /p:SelectedFiles= accepts ONE file per invocation ("%3B" joins compile nothing). Full build blocked by user's WIP errors in ManagedHostBindings.cpp.
- Mesh-search/console/Text-rollouts agent finished; BUT ComponentInspector.cpp does not compile: 41 errors at lines 44-216 from an unknown earlier refactor (untracked UI/SelectionProperties.h: `Property` template parameter shadows `UI::Property`, C2365). Not from any agent in this session; fix first thing tomorrow.
- [x] ComponentInspector.cpp compile fix: template params `Property`/`Member` shadowed functions in UI/SelectionProperties.h and Editor/SelectionProperty.h (renamed to Binding/MemberType). Remaining ComponentInspector (void) sites converted.
- [x] FixedUpdate: Play loop now runs ScriptRuntime::OnFixedUpdate + Scene::OnFixedUpdate (physics) per fixed step from Time::AdvanceSimulation; physics was previously not stepped in Play at all. OnEnable/OnDisable intentionally skipped (no enabled flag). Remaining: app shutdown while playing skips OnShutdown. Managed ABI bumped 14->15: republish CoreCLR test packages. Pre-existing: ManagedScriptingTests.cpp lines 815-821 `tooltip` redefinition; Crowny-Sharp Conditional.cs CS0273 breaks dotnet build.
- [x] Builtin.cwpack regenerated with Resources/Presets/Toon/*.cwpreset (64 resources). Notes: the in-repo pirate.glb uses EXT_meshopt_compression which Assimp rejects (tooltip now shows reason); built-in shaders now have deterministic UUIDs via BuiltInShaderCatalog.

## Mesh colliders (2026-09-05)
Plan: C:\Users\bo.ivanov\.claude\plans\make-a-plan-to-wondrous-brooks.md
- [x] Foundation: `Physics/PhysicsMesh.h/.cpp` (asset, Build/weld, resolver), `Asset::OnDependentAssigned`, `Mesh::CollisionMeshUuid` (mesh format v5), `MeshCollider3DComponent`, PhysicsMesh codec + cereal registration, vcxproj entries.
- [x] A: Scene hooks + `CreatePhysics3DShapes` mesh path, Physics3D/PhysicsMaterial loops, Jolt static guard, backend desc shrink, PrimitiveMeshLibrary physics meshes.
- [x] B: MeshImportOptions (GenerateCollision, CollisionMaxConvexPoints) + serializer, MeshImporter dependent, ProjectLibrary `OnDependentAssigned` hook, SceneComponentCodec id 21, PrefabSync.
- [x] C: Editor inspector widget + warnings + pre-fill on add, ColliderOverlay wireframe, asset browser labels, mesh import inspector checkboxes.
- [x] D: managed-interop.json + generator, ManagedHostBindings, ManagedComponentTypes, Mono wrapper, C# `MeshCollider3D`.
- [x] E: Tests (backend contract, scene, PhysicsMesh build/codec, serialization, prefab) + Docs/Physics.md.
- [ ] Full build + tests + clang-format. Formatting, 853 native tests, and Vulkan/OpenGL render checks passed. Initial All build passed; final All verification remains pending after concurrent material edits. Run `Scripts\crowny.bat build All` with the editor closed. Generated vcxproj files come from premake globs; do not hand-add entries.

## Viewport renders nothing (2026-09-05)
- [x] Root cause: `VulkanUtils::GetCompareOp` had no case for `CompareFunction::EQUAL` and fell through to `VK_COMPARE_OP_ALWAYS`. `Sky.glsl` uses `#pragma depth_compare equal` (since c97d945d, Aug 24) to paint only untouched pixels, so on Vulkan the sky pass repainted over every mesh each frame. The depth prepass still wrote object IDs (picking worked) and the grid/2D overlays draw after the sky (they showed). Fixed in VulkanUtils.cpp.
- [x] Material-less meshes (Create > 3D Object, FBX imports with `Materials: []`) used GPU material record 0 from a table that was only built after the first material change; the graph then bound an empty fallback buffer and no bindless textures. GpuScene now builds the table on first use and fills record 0 with the standard white material.
- [x] New render test `primitive-lit-sphere` (Crowny-RenderTests) mirrors the editor flow: primitive sphere, no material, EntityFactory light, EditorCamera, two frames. Golden generated on Vulkan (Intel Iris Xe).
- [x] Removed the temporary `[diag]` logging/PPM dump from SceneRenderer.cpp, EditorLayer.cpp, ViewportPanel.cpp.
- Open: default lighting looks overexposed (100000 lux directional + exposure 1 saturates to white); toon material without a light renders black. Scene loaded from disk with primitive UUIDs before `PrimitiveMeshLibrary::EnsureRegistered` gets unloaded placeholder handles (see LoadAssetReference) — check project-open ordering.


## Managed scripting hardening (2026-09-05, from the CoreCLR/Mono review)
Review summary: architecture sound (ADR-0001/0002 followed), two real Mono bugs, CI gate that cannot fail, unmeasured hot path. Work fanned out to five parallel agents with disjoint file ownership. Shared prerequisite done by hand: `UUID::Word(i)` accessor in Common/Uuid.h.

### WP1 Mono adapter (Scripting/Mono, Backends/Mono, Script*Object*, Bindings, Runtime/Mono C#)
- [x] C1: `MonoMethod::Invoke` swallows exceptions; TryPrepare/TryApply/Dispatch report success. Return the exception, fail with a ManagedDiagnostic (message + managed stack).
- [x] C2: `ScriptSceneObjectBase::FreeManagedInstance` leaves the C# `m_InternalPtr` pointing at the deleted native wrapper; finalizer queues a dangling pointer (UAF). Clear the cached ptr field like ScriptEntityBehaviour does. Resolving a freed (zero) GC handle now returns null.
- [x] H1: active Mono script calls resolve their wrapper through its GC handle; temporary roots protect construction, state calls, and collision payload allocation. The unused legacy ScriptArray cache remains under H5 cleanup.
- [ ] H2: hand-coded callback name table + Collision*Interop layouts duplicate CrownySharp `ScriptCallbacks`; route dispatch through one shared C# `ScriptCallbackDispatcher`.
- [x] H4: `MonoBackend::Update` returns no diagnostics; Dispatch always Success; `MonoUtils::CheckException` stack parsing is OOB-unsafe.
- [ ] M3: domain unload failure leaves state inconsistent; `mono_jit_init` failure unchecked; pathological `--gc-debug` flags in debug builds.
- [x] M4: `FromMonoString` UTF-16 -> wstring -> UTF-8 (surrogates broken on Linux).
- [x] Transport staleness: call `ManagedRuntimeContext.ClearNativeHostApi()` on Mono shutdown.
- [ ] H5: delete ~3.5k lines of dead wrapper layer (Bindings/** except ScriptEntity/ScriptEntityBehaviour/ScriptSceneManager, ScriptAssetManager, MonoBindingRegistry maps, MonoArray/MonoProperty/MonoProfiler/MonoVisibility), drop `Mono.h` from Common/Types.h.
- [x] Tests: exception -> failure, destroy -> finalizer safe, diagnostics surfaced.

### WP2 Managed C# host + CrownySharp (Crowny-Managed, Crowny-Sharp except generated/Mono/Mesh/ArrayInterop/Physics)
- [x] H1: `SceneManager` static events root game types past unload -> ReloadLeak; clear on unload + test.
- [x] H2: `Transform` uses EntityId directly; entity/component owners cache native wrappers. Warm transform reads skip repeated presence calls and type encoding. Optional components still check native presence; removal, transport reset, and scene replacement invalidate caches. Managed scripts remain uncached.
- [x] M1: Guid<->UUID via strings on event path; byte-level `UUID.FromBytes`, drop `FromGuid`.
- [x] M2: `UUID.ToString` hex table has uppercase 'B' (Asset.cs:93).
- [x] M3: CoreCLR indexes live scripts by entity, preserves base-type lookup and attachment order, and clears index references on destruction/unload. Reload and rollback recreate in backend-handle order so reused public handle slots cannot reorder matches.
- [x] M4: CoreCLR diagnostics use a typed queue and Utf8JsonWriter, with a static empty-array payload for empty polls.
- [ ] M5: AOT posture: enable trim analyzer on host or remove inert descriptors.
- [x] Add `ManagedRuntimeContext.ClearNativeHostApi()` (Mono shutdown hook, WP1 calls it).
- [ ] L1-L7 hygiene (nullable, codec substring allocations, remaining cleanup). Partial: UUID.Empty is readonly, UUID implements typed equality for dictionary keys, and m_InternalPtr exists only in Mono builds.

### WP3 ABI / generator / bindings (Scripts/managed, Interop/**, generated files, Mesh.cs, ManagedArrayInterop.cs, Physics*.cs)
- [ ] H1: `Execute` drains asset leases (mutex + hash) on all 533 calls; atomic pending flag fast path.
- [x] H2: `ToAbiUuid` string round trip -> shifts via `UUID::Word`.
- [x] H3: generated CoreCLR calls use `delegate* unmanaged[Cdecl]`; CROWNY_MONO builds retain marshaled delegates. Both transports clear all 533 callbacks on reset and retain the same ABI table layout.
- [ ] H4: untyped `pointer` params with hand-mirrored structs; add struct/array kinds, generate structs + size asserts both sides.
- [ ] H5/M7: strengthen manifest schema validation and parity parsing. Partial: four generator regressions cover every callback signature, Mono/CoreCLR transport selection, table layout, and reset behavior.
- [ ] M3: `Execute` swallows C++ exception messages; log them.
- [ ] M4: bare int enums unchecked; M5: document borrowed string lifetime; L: static_asserts for remaining structs; SetApi(default) nulls statics; ManagedArrayInterop closures/GCHandle -> fixed. Partial: borrowed string lifetime documented and SetApi(default) clears cached delegates.
- [ ] Benchmark: measure a representative call (TransformGetPosition) on both backends, record numbers.

### WP4 CoreCLR hosting + ScriptRuntime (Backends/CoreCLR, Managed/*.cpp, Scene/ScriptRuntime, Scene.cpp, physics dispatch)
- [x] H1: hostfxr diagnostics discarded; install `hostfxr_set_error_writer`, include status codes.
- [ ] H2: add fake program-api seam and rollback-failure matrix. Partial: real CoreCLR replacement-load failure now verifies successful rollback, preserved state, and continued use of the original public handle.
- [x] H3: replace raw backend host context with revocable registry tokens and owner-thread guards. Worker logging and queued finalizer releases remain supported; tests cover stale callbacks and synchronous reentry.
- [ ] M1: per-frame diagnostics JSON round trip; count-first/push-only.
- [ ] M4: vendor official nethost/hostfxr/coreclr_delegates headers; M5: std::map handle table -> flat vector; M6: ToAbiUuid via `UUID::Word`; L: DynamicLibrary destructor, manifest size bound, DestroyScript JSON capture. Partial: M6 complete.
- [ ] M3: CoreCLR naming in ManagedProgramPackage; backend fetches scene events itself.
- [ ] ScriptRuntime H1: cache event mask + handle on ManagedScript, skip phases, one validity check per dispatch.
- [x] ScriptRuntime H2: physics/destroy dispatch iterate script vector by reference; snapshot them.
- [ ] M5: shared static snapshot buffer; low: INFO log on destroy, empty Init, repeated DeferSceneChanges. Partial: snapshots are local and destroy INFO logging removed.
- [x] Reload failure leaves stale scene-side state (M3): validate runtime ownership before reuse; clear invalid handles and Awake flags, retain saved state, and recreate the scene occurrence with a fresh handle.

### WP5 Editor / tests / CI / build / docs
- [x] CI C1: Windows lane accepts any nonzero exit when `SKIPPED:` appears; gate on Catch2 semantics only.
- [x] CI: wire `check_managed_binding_parity.py`; CoreCLR publish + package smoke.
- [ ] Editor H3: inspector captures state as JSON every frame per script; dirty-flag capture.
- [ ] Editor H5: synchronous `dotnet build` on main thread; background rebuild with progress.
- [ ] Editor M4: `[RunInEditor]` scripts never get Update; edit-scene close skips OnShutdown.
- [ ] Tests: coverage map gaps (repeated unload, exception stacks, collision ordering to scripts, format rejection). Partial: repeated unload, exception stacks, UUID byte order, and callback mutation covered.
- [ ] Build M7/M8: csproj sprawl + three language versions; publish-coreclr.ps1 Windows-only paths; FastNoiseLite double patch. Partial: publisher now uses Tools/crowny instead of deleted setup helpers.
- [x] Docs: format 11/12 inconsistency, AOT wording, borrowed-view convention, parity claim.

### Phase 2 (after the above lands)
- [ ] Batch per-phase dispatch (one managed transition per phase) - new ABI function.
- [ ] Component-kind enum instead of type-name strings.
- [ ] Slim `ScriptValue` (variant + small vector) and stop JSON in inspector/undo/save paths.
- [ ] Neutral `ManagedBackendBase`; move `MonoRuntimePaths` out of ManagedReload.h.
- [ ] Flip editor default to CoreCLR once lifecycle tests run on both backends.

## Parallel hardening pass (2026-09-05)

Implemented and checked on Windows:
- WP1: explicit Mono invocation/callback failures with managed stacks, queued diagnostics returned by Update, stale finalizer pointer clearing, UTF-16 surrogate conversion, and transport clearing on shutdown.
- WP2: scene-event subscribers released before collectible unload, direct ABI UUID decoding, lowercase UUID formatting, and cleared generated delegate caches on transport reset.
- WP3: native host UUID encoding uses UUID::Word instead of formatting and reparsing text.
- WP4: hostfxr error writer and status messages, physics/destroy dispatch snapshots, script removal after callback mutation, and local lifecycle snapshots.
- WP5: CI checks Catch2 exit codes, runs binding parity and tooling tests, publishes a CoreCLR package, and runs its smoke test. Repaired the publisher's references to deleted build helpers. Updated scene-format, AOT, parity, and borrowed-string documentation.

- Build coordination: keep the output lock context alive through compilation, preventing concurrent builds from overwriting PCH outputs. Regression tests cover lock lifetime and release after failure.
- Validation fixes: corrected rotation extraction across quaternion trace branches, shader picker ordering, and ImGui property-layout test expectations. Rotation tests cover boundary angles, reflected scales, and shear.
- [x] Follow-up: AssetBrowserPanel.cpp now uses strict case-insensitive name ordering with folders first and deterministic case/path tie breaks. Tests cover prefixes, duplicate names, and enumeration order.

Verification: 853 native test cases (89,390 assertions), all 10 process-isolated tests, 47 Python tooling tests, and the CoreCLR smoke test (46 assertions, including three collectible reloads) passed. Generated interop, binding parity for 533 functions, asset API, and header checks passed. Mono assemblies and the CoreCLR package at `artifacts/coreclr-hardening-20260905/managed-program.json` built successfully. After the rotation fix, all 9 Vulkan render tests, 9 OpenGL render tests, and 9 backend comparisons passed on Intel Iris Xe. No interactive editor workflow has been tested in this pass.

The initial All build passed. A later All build stopped at editor linking with LNK1104 while Crowny-Editor.exe was running. After it closed, another All attempt picked up concurrent material edits and was stopped to prioritize the requested CI sanitizer check. Final All verification remains pending. Logs: `artifacts/session-todo-final-build.log`, `artifacts/session-todo-full-native-tests.log`, and `artifacts/session-todo-final-render-tests.log`.

## GitHub CI scripting failures (2026-09-05)

- [x] Confirmed the Windows Build failure in run 33914803400 and ASan failure in run 33951249192: the lifecycle test called `destroyed.GetUuid()` after destroying the entity. The test now saves the UUID first. LifecycleProbe also keeps its sink reference because callbacks rename that entity.
- [x] Linux Build run 33914803400 mixed hidden lifecycle tests into the shared `[Mono]` process, leaving live managed instances between tests. The focused filter now excludes `[.ProcessIsolated]`, and each isolated test gets its own process.
- [x] Windows CI uses `build All --skip-editor-resources` to compile native and managed targets without launching a Vulkan editor on a runner without a GPU. Normal local builds still cook resources. Regression tests cover both modes and CLI forwarding.
- [x] Executed the actual Windows workflow test step locally under Git Bash: isolated tests, CoreCLR smoke, and all 853 native tests passed. The focused Mono filter passed 12 tests with the failing Linux run's seed; verified all Mono lifecycle tests belong only to the isolated lane. Tooling tests: 50 passed.
- [x] Rebuilt Release with AddressSanitizer and ran the exact failing lifecycle test: all 28 assertions passed with exit 0 and no sanitizer errors. Logs: `artifacts/ci-lifecycle-asan-validation-retry.log`, `artifacts/ci-windows-test-step-validation.log`, and `artifacts/ci-tooling-validation.log`.

GitHub has not run the uncommitted workspace changes yet. Linux test selection was checked using the native Windows executable; a Linux runner was not available for a full local Linux build.

## Lifetime risks and asset browser sorting (2026-09-05)

- Mono script dispatch now resolves the current wrapper through its GC handle. Temporary roots protect managed constructors, state calls, and 2D/3D collision payloads. The GC regression forces collections, replaces a wrapper under the same scene instance ID, and verifies dispatch reaches the replacement.
- CoreCLR host tables use revocable tokens instead of backend pointers. Tests exercise copied callbacks after destruction, owner-thread guards, worker diagnostics, finalizer releases, and synchronous callback reentry without deadlock.
- Scene scripts discard invalid runtime handles and Awake flags while retaining saved state. The regression reproduced four failures before the fix and now passes 70 assertions. A real CoreCLR replacement-load failure also verifies rollback restores state and preserves public handles; the rollback-failure matrix remains open.
- Asset browser name sorting uses a strict comparator: folders first, case-insensitive names, then deterministic case and path ties. Tests cover prefixes, duplicates, and all permutations of equal basenames.

Windows verification: `Scripts\crowny.bat test --process-isolated` passed all 12 isolated tests and the full 864-test suite (89,478 assertions). AddressSanitizer passed all 12 isolated tests and 9 focused lifetime/sorting tests (68 assertions), with no sanitizer errors. The private CoreCLR package smoke/rollback test passed 51 assertions in both Release and AddressSanitizer. Managed assemblies rebuilt, scoped native formatting and header checks passed, and the concurrent Bistro task confirmed successful Editor compilation/linking (`artifacts/bistro/build-empty-transparency.log`).

Logs: `artifacts/lifetime-full-validation.log`, `artifacts/lifetime-asan-validation.log`, `artifacts/lifetime-coreclr-rollback.log`, and `artifacts/lifetime-coreclr-rollback-asan.log`. No interactive asset browser or scripting editor workflow was exercised in this pass. Rendering validation belongs to the concurrent Bistro task; GitHub has not run these uncommitted changes.

## C# lookup and transport pass (2026-09-05)

- Closed WP2 H2, M3, M4 and WP3 H3: cached native component wrappers, entity-indexed CoreCLR script lookup, explicit diagnostic JSON writing, and generated unmanaged function pointers for CoreCLR with the Mono delegate path retained.
- Added typed UUID equality to avoid boxing dictionary keys and excluded the unused native wrapper pointer from CoreCLR objects.
- The cache regression passed 44 assertions. Across 513 transform position reads, it observed one native component-presence check and 513 native position calls. It also verified optional component removal/replacement, destroyed-entity rejection, and cache renewal when a new scene reused an entity UUID.
- The expanded CoreCLR integration test passed 166 assertions, including zero managed bytes allocated across 512 warm transform reads, escaped diagnostic messages and stacks, duplicate/base-type lookup, failed construction, three reloads, and rollback. This test exposed reordered lookup after public handle-slot reuse; reload now recreates scripts in original backend-handle order while preserving public-handle mapping.

Windows verification: Mono C# 7.2 assemblies and the fresh .NET package at `artifacts/coreclr-csharp-20260905/managed-program.json` built successfully. `Scripts\crowny.bat test --process-isolated` passed all 13 isolated cases and all 864 regular cases (89,593 assertions). All 54 tooling tests, generated-output consistency, 533-function parity, header checks, and scoped formatting/diff checks passed. The fresh managed package also passed the existing 51-assertion AddressSanitizer smoke harness before the native reload-order adjustment; final expanded regressions ran in Release.

Logs: `artifacts/csharp-full-validation.log`, `artifacts/csharp-coreclr-integration-green.log`, `artifacts/csharp-component-cache-test.log`, `artifacts/csharp-managed-build.log`, `artifacts/csharp-coreclr-publish.log`, and `artifacts/csharp-tooling-tests.log`. No interactive scripting editor workflow was exercised. Remaining C# work includes shared callback dispatch, broader ABI/schema validation, AOT analysis, codec cleanup, and the editor script workflows listed above.
