# Engine improvement backlog

Captured on 2026-08-24 from the full queued request stream. This is the durable working checklist: do not drop an item because a previous task stopped or usage ran out. Inspect existing work before starting, keep useful `vk-rt` changes, and mark an item complete only after implementation and verification.

## Working rules and repository setup

- Work from the latest `vk-rt` line (`codex/vk-rt-integration` in this workspace), preserve unrelated dirty-tree changes, and use parallel agents on non-overlapping ownership areas.
- Bootstrap Mono, OpenAL, Vulkan, and physics dependencies from repository scripts. Remove hard-coded Vulkan-version assumptions, retain environment overrides, and vendor dependencies consistently as submodules. Document when an upstream fork is actually required.
- Keep the installed global `unslop` skill in use. Use `Scripts/format.sh` for formatting and line-ending normalization.
- Regenerate once per coherent batch, build Release, run Catch2 when present, and launch/smoke-test the editor. Do not rebuild after every line change.

## Current batch status

- Implemented, awaiting the coordinated build/test: image loading and bounded asset previews; console snapshots/search/order; camera invariants and managed properties; safe single-pixel viewport picking; packed ImGui fonts; physics materials and versioned serialization across the compiled 2D/3D backends; isolated play/simulate scenes and managed scene lifecycle; runtime Assets API migration and its regression checker; `PixelData`/`PixelUtils` hardening; OpenAL/EFX capability and lifecycle handling; and configurable procedural mesh primitives.
- In progress: integration with the concurrent lifetime, editor-inspector, asset-database, and scene-codec refactors already running in this worktree, followed by one clean generated Release build and full Catch2 pass.
- Everything else below remains queued unless its existing implementation is inspected and verified against the acceptance criteria.

## Crash, diagnostics, and memory safety

- Reproduce and eliminate the `Crowny-Tests.exe` null-write shutdown errors and Crowny Runtime startup crashes. Preserve crash reports and symbolized stacks.
- Diagnose component-menu search crashes, Asset Browser search/layout regressions after the ImGui rebase, and the Vulkan/Intel startup hang or driver deadlock. Audit allocation and destruction order around render-thread and swapchain teardown.
- Keep ASan support working on supported toolchains, provide a Windows leak-detection fallback, and fix reported leaks rather than merely suppressing them.

## Allocation and string performance

- Profile per-frame allocations first. Reuse transient containers, add frame/scratch/pool allocators where measurements justify them, and reference B3DFramework's allocator designs without copying unsuitable assumptions.
- Use `StringID`, `HashedString`, and CityHash-backed heterogeneous lookup for repeated identifiers, events, shader/resource keys, ECS names, and hot-path maps. Do not hash one-off UI text or replace user-facing strings that need ownership.

## Physics and collider visualization

- Polish 2D physics, callbacks, filtering, queries, joints, materials, lifecycle, and teardown; refactor it behind a backend-neutral interface.
- Integrate backend-selectable Box2D for 2D and Jolt/Bullet for 3D, with the intended default recorded in project settings. Expose the common feature set without erasing backend-specific capability queries.
- Cover all collider shapes, rigid-body controls, constraints, triggers, contact data, queries, layers/masks, sleeping, CCD, and debug drawing. Draw collider shapes as viewport overlays when enabled.
- Expand C# physics coverage for bodies, colliders, materials, assets, callbacks, queries, settings, and backend selection. Keep it safe across scene and assembly reloads.

## Renderer backends, shaders, and startup

- Polish Vulkan for correctness and performance; finish incomplete paths and fix bugs without using this pass for unrelated feature growth. Verify Intel-driver behavior explicitly.
- Finish and stabilize OpenGL with feature/capability fallbacks, backend parity, clean startup/shutdown, and cross-platform validation.
- Rework shader sources, built-in shaders, parsing, includes, reflection, variations/permutations, compilation cache, dependency invalidation, serialization, diagnostics, and hot reload as one coherent pipeline.
- Pack cooked built-in shaders, textures, fonts, icons, and meshes into versioned assets. Prefer the pack in Release/Dist while allowing newer loose sources during editor development.
- Make Release editor startup nearly immediate: profile project scanning, shader work, assembly loading, asset manifest loading, and renderer initialization; defer nonessential work without hiding failures.

## Rendering validation and statistics

- Maintain a rendering-test harness with deterministic scenes, reference images, tolerances, diff artifacts, and Vulkan/OpenGL comparison. Design it so future backends use the same cases.
- Report FPS, frame CPU/GPU milliseconds, draw/dispatch counts, vertices, triangles, instances, memory, and other useful engine statistics without adding material per-frame overhead.

## Geometry, meshes, and animation

- Polish the geometry node editor: add useful nodes, flexible typed connections, caching, invalidation, evaluator performance, serialization, diagnostics, undo/redo, and stable editor adapters.
- Expand mesh import, reading, parsing, validation, submeshes, vertex layouts, tangents, skin data, morph targets, bounds, and malformed-input handling.
- Provide mesh primitive creation for cubes, spheres, planes, capsules, cylinders, cones, and other common primitives with configurable dimensions and subdivisions.
- Complete morph animation, then implement skeletal animation, skeletons, bones, skinning, blending, sampling, root motion, import, serialization, runtime evaluation, editor inspection, and tests; use bs::framework only as a design reference.

## Scripting and managed API

- Finish and stabilize Mono/C# scripting across platforms. The editor must build the game assembly, reload DLLs safely, recreate instances, and transfer compatible serialized parameters without stale native or managed references.
- Expand managed coverage across the engine, preserve useful exceptions/diagnostics, and test repeated compile/reload/play/scene transitions.

## Audio

- Restore and extend the OpenAL work: clips, streaming, spatialization, attenuation, listener/source controls, devices, buses, effects, filters, sends, Doppler, capture where supported, serialization, previews, and managed bindings.
- Treat missing EFX entry points as a capability fallback with one clear diagnostic; do not crash or spam logs. Verify Windows, Linux, and other supported OpenAL configurations.

## ECS, hierarchy, and selection

- Profile ECS iteration, component storage, transform propagation, callbacks, and allocations. Prefer data-oriented layouts where they improve measured performance without forcing an impractical rewrite.
- Replace fragile entity parenting with a cache-friendly, cycle-safe hierarchy that supports fast traversal, reparenting, sibling order, destruction, prefab synchronization, serialization, and stable UUID lookup.
- Polish the hierarchy UI and add Ctrl-toggle, Shift-range, and combined multi-selection. Implement coherent multi-entity/component editing, mixed-value display, common-component views, gizmo placement, and safe bulk operations.

## Editor architecture and user experience

- Clean up panel and menu registration so panels self-describe their menu path, shortcut, default visibility, and construction metadata instead of being registered backwards in a central menu.
- Polish the editor UI, panel lifecycle, import/editor code quality, Asset Browser search and properties layout, component search, cameras, and viewport tools.
- Make undo/redo cover entity/component creation and deletion, parenting, transforms/gizmos, multi-edit, asset edits, and drag/slider changes as one logical transaction per interaction.

## Importers and code quality

- Refactor importers behind consistent probe/load/import contracts with structured diagnostics, cancellation, dependency tracking, deterministic output, and clean failure rollback.
- Remove duplicated, misleading, dead, and needlessly clever code across engine and editor. Keep refactors reviewable, preserve behavior unless a change is intentional, and add regression tests around fragile code before rewriting it.

## Fonts, text, pixels, windows, and cameras

- Finish the font/text system, Unicode shaping/fallback behavior, atlas lifecycle, serialization, batching, and editing workflow.
- Implement full horizontal/vertical alignment, wrapping, clipping, overflow, spacing, decoration, auto-size, and layout bounds. Use compact alignment icons/quick tabs rather than dropdowns.
- Finish and test `PixelUtils`/`PixelData`: formats, pitches, ownership, copies/moves, bounds, conversion, compression blocks, mip helpers, and overflow-safe sizing.
- Finish the windowing layer: modes, monitors, DPI, resize, focus, input, cursor/grab, drag/drop, multiple windows, Vulkan/OpenGL surfaces, and cross-platform teardown.

## Asset previews

- Finish previews for meshes, textures, audio clips, materials, fonts, scenes, and prefabs.
- Use one preview service with cached results, cancellation, invalidation after reimport, and bounded background work.
- Mesh and material previews need an isolated preview scene and camera. Texture previews must handle channels, alpha, HDR, cubemaps, and mip selection. Audio previews need transport, waveform, duration, channel, and sample-rate information.
- Preview failures must show a useful placeholder and must not block or crash the asset browser.

## Physics materials and asset serialization

- Make collider materials reusable assets in both 2D and 3D. Define defaults, overrides, live runtime updates, and backend-neutral combine rules.
- Serialize physics materials, collider references, prefabs, scene YAML, binary assets, and managed wrappers without losing fields.
- Audit asset serializers for versioning, missing-field defaults, unknown-field tolerance, stable UUID references, round trips, and failed-load cleanup.

## Editor play and simulation

- Entering Play must clone the edit scene into a temporary runtime scene. Exiting Play restores the untouched edit scene and selection.
- Simulate must use a separate temporary scene too, run physics without game scripting, and restore the edit scene when stopped.
- Define pause, step, reload, save-during-play, asset reload, and script assembly reload behavior. Repeated transitions must not leak scenes or runtime systems.

## Console behavior

- Improve search with severity, source, and text filtering without rebuilding lowercase strings every frame.
- Cache local-time formatting per message. Keep collapsed counts, timestamps, and source information correct.
- Verify stable chronological ordering while messages arrive from several threads. Preserve selection and scroll behavior when filters or collapse state change.

## Scene lifecycle and C# scene API

- Make scene load, unload, replace, and reload explicit operations with deterministic teardown of entities, scripts, physics bodies, renderer state, audio state, and asset references.
- Add asynchronous and deferred scene changes where an immediate change would invalidate an update callback.
- Expose a Unity-like C# API for loading by asset reference, unloading, querying loaded scenes, selecting the active scene, and receiving lifecycle events. Do not expose raw filesystem paths.

## Asset API adoption

- Find engine and editor code that opens project or built-in assets through raw paths.
- Route runtime resource access through `AssetHandle`, asset UUIDs, the manifest, virtual paths, or the built-in resource pack.
- Raw paths remain valid only at import boundaries, user file dialogs, build tooling, and platform filesystem code. Add checks for accidental runtime path access.

## Cameras

- Consolidate editor and runtime camera math, projection updates, viewport changes, reverse-Z handling, frustum data, and screen/world conversion.
- Fix aspect-ratio, orthographic, near/far plane, resize, jitter, and camera-cut behavior. Avoid redundant matrix inversions during a frame.
- Add camera serialization, C# coverage, editor controls, and tests for projection and coordinate conversions.

## Entity picking

- Make viewport picking asynchronous or otherwise stall-free. Validate coordinates, target size, attachment availability, entity IDs, and scene lifetime before readback.
- Move object identification out of hand-maintained shader declarations. Prefer a standard renderer-owned object-ID pass or injected variation that works across Vulkan and OpenGL.
- Keep picking correct for opaque, transparent, skinned, instanced, procedural, and 2D renderables. Define behavior for hidden and locked entities.

## Image loading

- Move image decoding behind the importer/loader contract with format probing, clear diagnostics, metadata-only reads, cancellation, and consistent `PixelData` output.
- Audit PNG, JPEG, TGA, BMP, PSD, GIF, HDR, EXR, DDS, KTX/KTX2, and Basis support, including 16-bit, floating-point, compressed, cubemap, array, mip, color-space, orientation, and ICC metadata.
- Keep stb for simple raster formats unless measured gaps justify another dependency. Prefer focused libraries such as tinyexr and Basis/KTX tooling over FreeImage unless FreeImage provides a tested portability and maintenance win.

## Completion rules

- Keep changes cross-platform and backend-neutral.
- Add focused Catch2 coverage and serialization round-trip tests for each completed item.
- Run the Release build and full tests after each coherent batch, not after each edit.
- Record migrations or asset-format version bumps before changing persisted data.
