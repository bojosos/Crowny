# Crowny Engine Backlog

This is the durable record of the requested engine work. A checked item is merged into `origin/the-new-age`; verification notes state what was actually exercised. Keep implementation batches small enough to review, but preserve backend-agnostic interfaces and cross-platform behavior.

## Current priorities

- [ ] Eliminate steady-state per-frame allocations after warm-up. Start with scene extraction generation stamps, render-graph scratch reuse, draw/shadow preparation, editor selection copies, material lookup copies, and animation upload closures. Measure 1, 1,000, and 10,000 entities.
- [x] Harden `TaskSystem`: continuation-based dependencies, exception propagation, cancellation, shutdown sealing/drain, scheduler generations, fair priority queues, and bounded `ParallelFor`. A shared CPU budget with Jolt remains part of the 3D physics integration.
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
- [x] Task scheduling now has continuation dependencies, exception propagation, cooperative cancellation, generation checks, strict timed waits, wait-cycle rejection, fair 8:4:1 priorities, bounded/fail-fast `ParallelFor`, and sealed drain/cancel shutdown (`8745408`). Import scheduling and asset previews handle scheduler rejection without stranding work.
- [x] Editor viewport/component inspection reuses retained selection and transform scratch storage and avoids temporary quick-tab label containers in steady-state UI paths (`537e2e6`).
- [x] Player-build orchestration validates, fingerprints, resolves, compiles, packs, stages, writes, and recoverably publishes a build with complete artifact readback and journal-based last-good restoration (`da625cb`). Editor UI, CLI, incremental cache, and build-and-run remain open.
- [x] Post-rebase Windows ASan verification passed 25,634 assertions in 386 cases; focused TaskSystem, build-pipeline, and import-scheduler suites passed 157/136/48 assertions respectively.
- [x] GPU draw-bin compaction registers stable command/count segments and feeds GPU-driven indirect submission without readback, while preserving baseline/OpenGL fallbacks (`2420a66`).
- [x] Engine shutdown drains owned task work before scene, scripting, importer, and asset consumers are destroyed; import rejection and exception paths are deterministic and do not poison the shared scheduler (`c2971ad`).
- [x] The editor build manager maps platform settings and exact content/managed/template inputs into the recoverable player pipeline with actionable preflight and cancellation reports (`16b7f06`). Final Build button input gathering and progress UI remain open.
- [x] Hierarchy search reuses match/path/action scratch storage and avoids per-entity lowercase/name copies (`b48d089`).
- [x] Two randomized Windows ASan full-suite passes completed with 25,685 assertions in 394 cases; focused TaskSystem, import-scheduler, and editor-build suites passed 157/58/42 assertions respectively.
- [x] Scene extraction uses allocation-free heterogeneous material-set lookup on stable entries and retains directional-cascade plus render-thread shadow scheduling/upload scratch across frames (`eb0ded9`).
- [x] Post-Linux-link integration passed 25,742 Windows ASan assertions in 399 cases; focused shadow, GPU-driven, and render-pipeline suites passed 101/69/63 assertions respectively.
- [x] Built-in shader freshness now hashes canonical transitive include content, invalidates the live stage cache on include edits, and preserves the last good compiled asset after include failures (`5452ab4`).
- [x] Console filtering parses fielded, quoted, and negative terms once per query; collapsed groups keep stable selection identity and deterministic sort ties.
- [x] Render-graph transient physical-ID lookup retains its hash scratch after warm-up and reports capacity growth instead of allocating a fresh map each frame.
- [x] Render-graph setup callbacks use an immediate, non-owning template path, avoiding standard-library-dependent heap allocation for large captures while deferred execution callbacks retain owning lifetime semantics.
- [x] Local-shadow scheduling retains candidate scratch and uses ordinal tie-breaking with allocation-free sorting; stable 1, 1,000, and 10,000-request schedules allocate nothing after warm-up.
- [x] CPU clustered-light reference builds retain cluster-count and projected-bounds scratch; stable 1, 1,000, and 10,000-light builds allocate nothing after warm-up. Runtime clustering remains GPU compute based.
- [x] Combined Windows ASan verification passed 25,832 assertions in 403 cases; focused console, render-graph, and shader suites passed 51/91/177 assertions respectively.
- [x] Re-cooked 32 shader assets for the transitive fingerprint migration and repacked 54 built-in resources; a repeat ASan cook-only launch found no stale assets and exited cleanly in 3.64 seconds. Vulkan, OpenGL, and cross-backend reference checks passed 4/4 each.
- [x] Thread-local allocation telemetry covers standard, aligned, sized, array, and nothrow allocation families without the heavy leak tracker's map/mutex. Render-graph construction and compilation retain graph/result/compiler scratch and perform zero calling-thread allocations across 120 identical frames after warm-up.
- [x] Render-graph transient bindings reuse frame-slot physical IDs directly, and renderer pass dispatch uses typed, small-buffer callbacks instead of copied strings and heap-backed functions. Forward+ and Deferred+ graph rebuilds plus transient binding resolve allocate nothing across 120 warm frames.
- [x] CPU draw-list and 2D ordering are measured allocation-free after warm-up at 10,000 items. Main-view draw lists and indirect run-count uploads now retain renderer-owned scratch instead of rebuilding temporary vectors every frame.
- [x] Per-record synchronization epochs replace the four node-based seen sets for instances, lights, meshes, and materials. The stable and post-removal 1,000-light extraction paths are specifically measured allocation-free after warm-up.
- [x] Common native component-inspector scopes retain typed pre-edit snapshots and bind undo factories without rebuilding captured vectors or heap-backed callables. Tag multi-edit snapshots are measured allocation-free for 120 warm frames; managed-script capture and components whose assignment rebuilds node containers remain separate optimizations.
- [x] Native component-inspector redo snapshots finalize after property setters, preserving immediate controls, final drag values, and distinct multi-edit undo state while coalescing a frame into one action.
- [x] Continuous editor edits share an explicit begin/update/commit transaction. Inspector controls build actions after their final setter, viewport gizmos use the same coalescing rules, and cancelled or unchanged gestures do not enter history.
- [x] Escape cancels an active multi-entity transform gizmo transaction, restores every captured world transform, and records no undo action; normal release still commits one grouped action.
- [x] Managed-script inspector undo captures detached persisted state only when an item changes, records redo after the managed setter, and resets its retained transaction on selection, scene, and Mono domain changes. Stable warm frames perform no undo capture or allocation.
- [x] Material inspection caches reflected parameter schemas by material instance and layout version. Warm resolves allocate nothing, value edits do not rebuild the schema, and the editor cache retains no material, texture, shader, or GPU-resource references.
- [x] Normal entity-hierarchy rows borrow their component-owned tag labels instead of allocating one string per visible entity per frame.
- [x] Dynamic animation and procedural-mesh uploads use typed, move-only render commands with retained result slots instead of heap-backed `std::function` targets; 1, 1,000, and 10,000-command warm paths allocate nothing.
- [x] Directional shadow cascade splits use fixed stack storage, removing the remaining per-camera split-vector allocation after warm-up.
- [x] Component, script, and reparent undo actions retain their target scene safely. Play and Simulate make edit history dormant instead of recording against runtime clones; edit-scene replacement clears stale history.
- [x] Add-component search retains sorted catalogs and query results, while the viewport HUD uses bounded fixed-buffer formatting. Their stable visible-frame paths are covered by zero-allocation tests.
- [x] Asset Browser selection uses full paths, survives filtering and sorting safely, and assigns distinct ImGui IDs to duplicate basenames. Empty-result keyboard actions and destructive operations are bounds-safe.
- [x] Reference-field popup IDs use owned fixed storage with no warm-frame allocation, and multiline properties share the standard balanced ImGui row cleanup path.
- [x] Physics and audio runtime components use explicit stable EnTT storage. `AddOrReplace` reapplies editable settings while preserving live handles, component identity, backend-observed velocity and sleep state, and initialized audio-source state across Box2D, Box3D, Jolt, and Bullet.
- [x] Entity hierarchy selection applies deterministic Shift ranges and Ctrl toggles against the current visible order, with filtered-view fallbacks and deleted-anchor recovery.
- [x] Native 2D and 3D collision callback payloads keep their fixed two-entity collider pair inline. Box2D snapshot/copy and scene-to-script dispatch paths perform zero native allocations for 10,000 warm contacts.
- [x] The render blackboard retains named resource entries across frame clears and uses generation-stamped heterogeneous CityHash lookup. Stable 1, 1,000, and 10,000-resource rebuilds allocate nothing across 120 warm frames.
- [x] Subtree duplication attaches each clone to its final parent once, preserves local and world transforms under transformed ancestors, and leaves source child storage untouched instead of allocating snapshots and performing transient reparent operations.
- [x] Scene lifecycle keeps a retained sorted loaded-scene index; native and C# enumeration no longer rebuild and sort a UUID vector for every count and element query.
- [x] Editor last/recent scene history persists stable asset UUIDs, migrates legacy paths without dropping unresolved entries, and rebinds the active scene identity after Save As.
- [x] Windows and Premake managed builds emit configuration-correct assemblies under the ignored `.deps/generated/managed` tree; editor and isolated tests resolve those outputs without rewriting committed fallback DLLs.
- [x] Input-map evaluation retains action state, context ordering, and consumed-control scratch; stable updates and long-name CityHash queries allocate nothing after warm-up, while callback-scoped edits keep authored IDs valid.
- [x] Legacy render snapshots flatten per-renderable material handles into one frame-context-owned buffer with checked offsets; 1, 1,000, and 10,000-object rebuilds allocate nothing after warm-up.
- [x] Asset Browser list and grid views clip offscreen entries, request previews only for submitted cards, and cache metadata presentation by revision without losing absolute selection or rename state.

## Physics and ECS

- [ ] Refactor 2D physics behind a backend-neutral interface. Box2D is the default. Expose bodies, all common collider shapes, materials, filters, triggers, joints, queries, contacts, callbacks, sleeping, CCD, and deterministic lifecycle rules.
- [ ] Complete 3D physics with Jolt as default and Bullet as an interchangeable backend. Keep common features backend-neutral; expose backend extensions explicitly instead of weakening the common API.
- [ ] Vendor physics libraries consistently with existing submodules. Record whether upstream can be pinned directly or requires a Crowny fork for build fixes or stable patches.
- [ ] Complete C# bindings for 2D and 3D bodies, colliders, materials, layers, queries, contacts, callbacks, joints, assets, and serialization.
- [x] C# can create UUID-bearing, path-free 2D and 3D physics materials with backend-neutral properties and inline scene serialization. Managed-created material and mesh wrappers use collectible weak ownership and are drained safely at scripting shutdown.
- [ ] Serialize physics materials and collider overrides without raw paths. Define fallback/default material behavior and version migrations.
- [x] Draw Box2D box/circle and 3D box/sphere/capsule collider shapes in the viewport when physics overlays are enabled, using the same transform and scale rules as the active backends.
- [ ] Add Play, Simulate Physics, pause, and step behavior using an isolated temporary scene while preserving the edit scene.
- [ ] Continue ECS performance work: benchmark component access and iteration, remove accidental ownership churn, and preserve data-oriented storage.
- [ ] Replace recursive hierarchy invalidation with measured generation or topological transform propagation. Preserve physics/audio notification order and optimize parenting, reparenting, subtree deletion, and scene copying.

## Rendering, shaders, and GPU lifetime

- [ ] Finish Vulkan correctness, shutdown ordering, Intel-driver hang diagnosis, synchronization, descriptor/resource lifetimes, and performance. Prefer bug fixes and optimization over new Vulkan features.
- [ ] Finish the OpenGL backend to behavioral parity for supported features, including clean startup/shutdown and renderer harness coverage.
- [ ] Complete shader parsing, GLSL preprocessing, includes, stages, passes, variations, cache keys, reflection, diagnostics, hot reload, and backend parity. Parallelize independent variants only after TaskSystem hardening.
- [ ] Persist canonical transitive shader-include dependencies and a combined content fingerprint. Built-in shader freshness currently checks the root source only, so shared PBR, clustered-lighting, or shadow include edits can remain stale after restart.
- [ ] Package built-in shaders, textures, fonts, icons, and fallback assets into a versioned built-in resource pack. Keep source development and hot reload convenient.
- [ ] Expand rendering statistics: FPS, CPU/GPU frame time, vertices, triangles, draw/dispatch calls, culled instances, shadow work, upload bytes, descriptor use, geometry-heap occupancy, and frame-allocation counts.
- [ ] Unify entity picking without adding an entity ID output to every material shader. Make readback asynchronous, bounds-safe, generation-checked, and fast for large scenes.
  - [x] Click and material-drop picking share one finite, half-open display-to-texture coordinate resolver, use the actual object-ID extent, reject invalid targets before synchronous readback, and propagate the new viewport size only after target replacement succeeds.
- [ ] Finish cameras: projection validation, physical/orthographic controls, viewport/aspect changes, clear modes, culling masks, camera selection, and C# parity.
  - [x] EditorCamera normalizes invalid projection inputs, updates pose setters coherently, and lazily caches stable view/view-projection matrices with zero steady-state allocations.
- [ ] Add and retain reference-image regression tests across Vulkan and OpenGL, with tolerances, diff artifacts, headless CI policy, and future-backend comparison.
- [ ] Keep `RENDERER_BACKLOG.md` as the detailed renderer design and verification log.

## Geometry, meshes, and animation

- [ ] Expand mesh import and parsing: attributes, index widths, submeshes, topology, tangents, skin data, morph targets, bounds, LODs, meshlets, validation, corrupt-input diagnostics, and round-trip asset serialization.
- [x] Validate mesh-processing submesh ranges, triangle alignment, vertex references, and finite positions before passing imported or deserialized geometry to meshoptimizer.
- [x] Expand static Assimp scene mesh instances through their accumulated node transforms, including non-uniform normal transforms, mirrored winding and tangent handedness, bounds, morph deltas, and material-slot duplication. Reject transformed skinned instances until per-instance bind transforms exist.
- [ ] Add asset-backed and runtime mesh primitives: cube, plane, sphere, cylinder, cone, capsule, quad, and configurable tessellation with safe upper bounds.
- [ ] Finish morph animation, then implement skeletal animation: skeleton assets, bones, bind/inverse-bind poses, clips, channels, interpolation, blending, layers, masks, root motion, events, retarget-ready IDs, GPU/CPU skinning, bounds, serialization, import, editor inspection, and C# APIs.
- [x] Compose transient skeletal override and additive layers in deterministic stack order with independent speed/wrap settings, reusable poses, layer weights, and per-bone masks. Layer events, morphs, root motion, serialization, and C# control remain separate work.
- [x] Dispatch ping-pong animation events in traversal order across both reflected clip boundaries without double-firing endpoint events.
- [x] Expose animation clips and backend-safe component playback controls to C#, preserving requested state and time when renderer runtime objects are recreated.
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
  - [x] Streaming sources retain decoded/conversion scratch, cache per-device PCM capabilities, validate OpenAL upload sizes, and allocate nothing after reaching their warm high-water mark.
- [ ] Expand audio assets, import, previews, waveform/duration metadata, serialization, editor controls, and C# parity.

## Editor experience

- [x] Defer Asset Browser rename and drag/drop moves until list, grid, and tree traversal finishes, then reconcile exact-path selection after a successful move.
- [ ] Polish the Asset Browser search and properties layout after ImGui updates. Cover empty states, focus, keyboard navigation, filters, breadcrumbs, thumbnails, context actions, multi-selection, and no-stack-imbalance crashes.
- [ ] Complete hierarchy multi-entity/component editing, mixed-value display, common-component presentation, gizmo rules, and safe removal/addition across a selection.
- [ ] Finish undo/redo for entity/component creation and deletion, reparenting, transforms/gizmos, multi-edit, text edits, sliders, and drag transactions. One gesture must produce one understandable command.
- [x] Box Collider 2D bounds gizmo drags produce one undo action, restore both offset and size, and discard no-op or cancelled gestures.
- [x] Polish the console: correct chronological order, cached local-time conversion, search/filter, collapsed duplicate counts, selection, copying, clearing, severity toggles, and bounded storage.
- [x] Cache console severity labels and selected callstack source labels between changes, and order collapsed sort ties by latest activity.
- [x] Bound console retention to 10,000 rows, prune oldest rows in batches, rebuild retained collapsed counts, and keep stable snapshot/search frames allocation-free.
- [ ] Finish previews for materials, fonts, animations, scenes, and scripts. Mesh, texture, and audio previews now cancel stale submitted tasks, retire canceled queued jobs without running their worker body, and drain active jobs before project/editor teardown. Add GPU material previews only after their renderer lifetime follows the same rule.
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
- [x] Text layout retains UTF-8 source offsets, emits allocation-reusing caret stops after wrapping, justification, and ellipsis, and resolves a layout-space point to the closest visible caret.
- [x] Shared editor property dropdowns borrow selected labels, use fixed ImGui-generated IDs, and iterate literal options directly. String, literal, and selector-backed dropdowns allocate nothing across 120 warm frames.
- [x] Audio source and mixer dropdowns borrow bus, parent, effect, and waveform labels instead of rebuilding option strings each visible frame. The borrowed-label path preserves mixed-value previews and allocates nothing across 120 warm frames.
- [ ] Finish `PixelUtils` and `PixelData`: validated pitches/sizes, all declared conversions, compression/decompression boundaries, mip generation, color spaces, alpha rules, safe overflow handling, and row-parallel kernels for large images.
  - [x] Alpha-coverage mip scaling preserves premultiplied RGB through the existing unpremultiply/store path, including sparse opaque colors and fully transparent inputs.
  - [x] PixelData freezes pitches while storage is bound, rejects invalid layouts before adoption, and never copies from invalid or unbound storage.
- [ ] Finish the cross-platform window system: multiple windows where supported, DPI, display enumeration, fullscreen modes, resize/minimize/focus, cursor modes, clipboard, drag/drop, icons, input routing, Vulkan/OpenGL surface lifetime, and C# APIs.
  - [x] GLFW logical-size, framebuffer-size, DPI, position, focus, and minimize callback storms coalesce into one deterministic, allocation-free state batch per poll on Windows and Linux.

## Performance, quality, and platform support

- [ ] Extend the allocation-count-only profiler's zero-allocation steady-state targets beyond the completed render-graph compile path to scene sync, draw preparation, editor selection/UI, text, and console.
  - [x] Font fallback traversal uses fixed stack storage and allocates nothing for 1, 1,000, or 10,000 warm missing-glyph lookups.
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
