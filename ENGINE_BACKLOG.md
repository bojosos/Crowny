# Crowny Engine Backlog

This is the durable record of the requested engine work. A checked item is merged into `origin/the-new-age`; verification notes state what was actually exercised. Keep implementation batches small enough to review, but preserve backend-agnostic interfaces and cross-platform behavior.

## Current priorities

- [ ] Eliminate steady-state per-frame allocations after warm-up. Start with scene extraction generation stamps, render-graph scratch reuse, draw/shadow preparation, editor selection copies, material lookup copies, and animation upload closures. Measure 1, 1,000, and 10,000 entities.
- [ ] Harden `TaskSystem`: continuation-based dependencies, exception propagation, cancellation, shutdown sealing/drain, scheduler generations, fair priority queues, bounded `ParallelFor`, and one shared CPU budget with Jolt.
- [ ] Finish the shippable-game pipeline around the committed build primitives: orchestrate validate, resolve, compile, pack, template stage, manifest, atomic publish, editor UI, CLI, progress, cancellation, incremental cache, and build-and-run.
- [ ] Keep startup and shutdown clean under ASan, Vulkan validation, and VMA leak reporting. The editor should open almost immediately in Release.

## Completed milestones

- [x] Managed script state survives assembly refresh and DLL reload identity changes (`7b3ff9a`).
- [x] Shared Vulkan geometry heaps, stable mesh bindings, transfer barriers, telemetry, and renderer tests (`8670f0f`).
- [x] Faster entity hierarchy mutation paths and hierarchy tests (`30add47`).
- [x] Expanded image loader formats and diagnostics (`d20e287`).
- [x] Central editor panel registry and menu registration (`47663ab`).
- [x] Bounded parallel editor import scheduling with deterministic publication (`7a54f32`).
- [x] Deterministic player-build primitives: profiles, manifests, content graph and pack, managed compilation, player templates, validation, security checks, and atomic publication (`f3db0af`).
- [x] Ranked multithreading audit and sequencing guidance (`67e60c3`).
- [x] Windows ASan full suite: 25,349 assertions in 346 cases. Build tests: 174 assertions in 37 cases. Real Mono/Roslyn compile, PE inspection, and dependency closure pass.
- [x] Vulkan and OpenGL reference-image harness passed 4/4 for each backend; Vulkan shutdown completed without the previously reported 15 VMA allocations.

## Physics and ECS

- [ ] Refactor 2D physics behind a backend-neutral interface. Box2D is the default. Expose bodies, all common collider shapes, materials, filters, triggers, joints, queries, contacts, callbacks, sleeping, CCD, and deterministic lifecycle rules.
- [ ] Complete 3D physics with Jolt as default and Bullet as an interchangeable backend. Keep common features backend-neutral; expose backend extensions explicitly instead of weakening the common API.
- [ ] Vendor physics libraries consistently with existing submodules. Record whether upstream can be pinned directly or requires a Crowny fork for build fixes or stable patches.
- [ ] Complete C# bindings for 2D and 3D bodies, colliders, materials, layers, queries, contacts, callbacks, joints, assets, and serialization.
- [ ] Serialize physics materials and collider overrides without raw paths. Define fallback/default material behavior and version migrations.
- [ ] Draw collider shapes in the viewport when physics overlays are enabled.
- [ ] Add Play, Simulate Physics, pause, and step behavior using an isolated temporary scene while preserving the edit scene.
- [ ] Continue ECS performance work: benchmark component access and iteration, remove accidental ownership churn, and preserve data-oriented storage.
- [ ] Replace recursive hierarchy invalidation with measured generation or topological transform propagation. Preserve physics/audio notification order and optimize parenting, reparenting, subtree deletion, and scene copying.

## Rendering, shaders, and GPU lifetime

- [ ] Finish Vulkan correctness, shutdown ordering, Intel-driver hang diagnosis, synchronization, descriptor/resource lifetimes, and performance. Prefer bug fixes and optimization over new Vulkan features.
- [ ] Finish the OpenGL backend to behavioral parity for supported features, including clean startup/shutdown and renderer harness coverage.
- [ ] Complete shader parsing, GLSL preprocessing, includes, stages, passes, variations, cache keys, reflection, diagnostics, hot reload, and backend parity. Parallelize independent variants only after TaskSystem hardening.
- [ ] Package built-in shaders, textures, fonts, icons, and fallback assets into a versioned built-in resource pack. Keep source development and hot reload convenient.
- [ ] Expand rendering statistics: FPS, CPU/GPU frame time, vertices, triangles, draw/dispatch calls, culled instances, shadow work, upload bytes, descriptor use, geometry-heap occupancy, and frame-allocation counts.
- [ ] Unify entity picking without adding an entity ID output to every material shader. Make readback asynchronous, bounds-safe, generation-checked, and fast for large scenes.
- [ ] Finish cameras: projection validation, physical/orthographic controls, viewport/aspect changes, clear modes, culling masks, camera selection, and C# parity.
- [ ] Add and retain reference-image regression tests across Vulkan and OpenGL, with tolerances, diff artifacts, headless CI policy, and future-backend comparison.
- [ ] Keep `RENDERER_BACKLOG.md` as the detailed renderer design and verification log.

## Geometry, meshes, and animation

- [ ] Expand mesh import and parsing: attributes, index widths, submeshes, topology, tangents, skin data, morph targets, bounds, LODs, meshlets, validation, corrupt-input diagnostics, and round-trip asset serialization.
- [ ] Add asset-backed and runtime mesh primitives: cube, plane, sphere, cylinder, cone, capsule, quad, and configurable tessellation with safe upper bounds.
- [ ] Finish morph animation, then implement skeletal animation: skeleton assets, bones, bind/inverse-bind poses, clips, channels, interpolation, blending, layers, masks, root motion, events, retarget-ready IDs, GPU/CPU skinning, bounds, serialization, import, editor inspection, and C# APIs.
- [ ] Improve geometry-node evaluation, node catalog, type conversion, caching, invalidation, diagnostics, previews, serialization, undo, and large-graph performance.

## Scripting and managed lifecycle

- [ ] Move compilation to a versioned background job with bounded output and timeout. Publish only the newest successful assembly; keep Mono domain unload, reflection, backup, restore, and callbacks on the managed owner thread.
- [ ] Complete reload state transfer across assembly saves, missing types, renamed or moved types, fields, collections, assets, entities, and user errors. Stress repeated full-suite runs because the earlier retained-state crash was intermittent.
- [ ] Expand C# API coverage across physics, scenes, assets, cameras, audio, animation, windowing, editor-safe operations, and lifecycle callbacks. Match Unity-like scene load/change semantics where useful without copying ambiguous behavior.
- [ ] Make scene change/reload callable from C#, leak-free, generation-safe, and seamless. Old scenes must release scripts, physics, audio, render resources, tasks, and assets in a defined order.

## Audio

- [ ] Complete OpenAL support for devices, contexts, sources, spatialization, streaming, listener state, attenuation, Doppler, distance models, filters, auxiliary sends, EFX effects, reverb presets, routing, and capability diagnostics.
- [ ] Treat missing EFX entry points as an expected capability fallback, with one clear message and no broken base audio.
- [ ] Restore per-source streaming decoders and move OpenAL ownership to a dedicated audio thread. Decode PCM on bounded workers and drain safely on scene/application shutdown.
- [ ] Expand audio assets, import, previews, waveform/duration metadata, serialization, editor controls, and C# parity.

## Editor experience

- [ ] Polish the Asset Browser search and properties layout after ImGui updates. Cover empty states, focus, keyboard navigation, filters, breadcrumbs, thumbnails, context actions, multi-selection, and no-stack-imbalance crashes.
- [ ] Add Shift range selection and Ctrl toggle selection to the hierarchy. Complete multi-entity/component editing, mixed-value display, common-component presentation, gizmo rules, and safe removal/addition across a selection.
- [ ] Finish undo/redo for entity/component creation and deletion, reparenting, transforms/gizmos, multi-edit, text edits, sliders, and drag transactions. One gesture must produce one understandable command.
- [ ] Polish the console: correct chronological order, cached local-time conversion, search/filter, collapsed duplicate counts, selection, copying, clearing, severity toggles, and bounded storage.
- [ ] Finish previews for meshes, materials, textures, audio, fonts, animations, scenes, and scripts. Cancel and drain preview jobs on project/scene shutdown.
- [ ] Improve panel/editor architecture beyond registration: lifecycle ownership, saved layout, visibility, focus, menus, shortcuts, services, test seams, and less coupling in editor-layer code.
- [ ] Clean up importers and editor code with focused refactors, explicit ownership, actionable diagnostics, stable asset identity, and no raw filesystem assumptions outside boundary modules.

## Assets, serialization, and loading

- [ ] Route engine and editor consumers through the Assets API. Raw physical paths belong only in import, build, VFS, and serialization boundary modules with explicit allowlist justification.
- [ ] Define one versioned asset serialization contract with codecs, migrations, dependency enumeration, stable UUIDs, atomic writes, corruption handling, and deterministic manifests.
- [ ] Finish asset preview, import scheduling, multi-output importer support, change coalescing, async scanning, load coalescing, and CPU-decode/GPU-finalize separation.
- [ ] Finish image loading as a loader backend with capability probing and actionable errors. Compare stb, FreeImage, and smaller dedicated codecs based on format coverage, maintenance, security, licensing, size, and threading.
- [ ] Keep CityHash-backed `StringID` or owned hashed strings for stable hot lookups where collision handling and lifetime are explicit. Do not use non-owning hashed strings for dynamic names that outlive their source.

## Text, fonts, pixels, and windows

- [ ] Finish font assets, import, glyph caching/atlases, shaping boundaries, fallback fonts, Unicode, kerning, wrapping, measurement, DPI, batching, serialization, and editor previews.
- [ ] Complete text layout and alignment for horizontal/vertical alignment, anchors, pivot, wrapping, overflow, line spacing, tabs, rich spans, bounds, hit testing, and stable results. Use icon quick-tabs for common alignment choices instead of dropdowns.
- [ ] Finish `PixelUtils` and `PixelData`: validated pitches/sizes, all declared conversions, compression/decompression boundaries, mip generation, color spaces, alpha rules, safe overflow handling, and row-parallel kernels for large images.
- [ ] Finish the cross-platform window system: multiple windows where supported, DPI, display enumeration, fullscreen modes, resize/minimize/focus, cursor modes, clipboard, drag/drop, icons, input routing, Vulkan/OpenGL surface lifetime, and C# APIs.

## Performance, quality, and platform support

- [ ] Add an allocation-count-only profiler that does not use the heavy `CW_TRACK_MEMORY` map/mutex. Establish zero-allocation steady-state targets for scene sync, render graph compile, draw preparation, editor selection/UI, text, and console.
- [ ] Use retained scratch, inline containers, frame-context-owned arenas, pools, and stable handles where lifetime proves safe. Never use one global frame arena for data crossing simulation/render threads.
- [ ] Bootstrap pinned Mono, OpenAL, Vulkan, and physics dependencies through repository scripts and caches. Support override environment variables, verify versions and hashes, avoid committing SDK binaries, and document offline/CI behavior.
- [ ] Keep Windows and Linux builds first-class. Add sanitizer options, leak checks, ThreadSanitizer-friendly CPU tests on Linux, Vulkan validation, OpenGL parity, and clear capability fallbacks.
- [ ] Improve Release startup with timing telemetry, async/coalesced scanning, prebuilt resource packs, lazy services, bounded shader work, and no unnecessary render-thread/device waits.
- [ ] Continue broad cleanup only in behavior-preserving, test-backed batches. Remove duplicated state, raw ownership, hidden global coupling, unsafe casts, repeated strings, and code that obscures lifetime or thread affinity.

## Delivery rules

- Build once per coherent integration batch, not after each line or agent handoff.
- Run focused Catch2 tests first, then one full ASan pass when shared/core code changed.
- Renderer changes also run Vulkan and OpenGL harnesses plus a normal Vulkan startup/shutdown VMA scan.
- Record exact changed files, commands, platforms, failures, and known limitations in each handoff.
- Keep parallel lanes in disjoint files or isolated worktrees. Pause writers before shared regeneration, builds, staging, commits, or editor launches.
- Commit reviewed milestones to `origin/the-new-age`; do not mix unrelated unfinished work.
