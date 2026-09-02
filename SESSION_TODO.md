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
