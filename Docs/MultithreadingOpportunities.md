# Multithreading opportunities in Crowny

## Executive recommendation

Crowny already has the right broad shape for more concurrency: a simulation thread, a Vulkan render thread, and a general `TaskSystem`. The largest gains now come from moving CPU-only work into bounded jobs while preserving single-thread ownership for EnTT mutation, Mono callbacks, OpenAL, render-world publication, and GPU submission.

The recommended boundary is:

1. Capture immutable inputs and a generation number on the owning thread.
2. Compute into task-local or disjoint output storage.
3. Join or poll at an explicit frame boundary.
4. Commit results in a stable order on the owning thread, discarding stale generations.

The first runtime targets should be animation/deformation and CPU visibility culling. The first low-risk editor targets should be managed compilation, file scanning, shader compilation, and bulk import. Do not start with arbitrary parallel script updates, asynchronous world mutation, or Vulkan secondary command recording.

## How the ranking works

Each score is from 1 to 5, where 5 is best.

- **Usefulness** measures how often the path matters and how broadly it affects engine or editor workloads.
- **Benefit** estimates the potential reduction in frame time, loading time, or editor stalls. It is a code-based estimate, not a measured speedup.
- **Ease** rewards isolated changes with existing handoff points and little redesign.
- **Safety** rates the final design described here, not a naive threaded rewrite. A high score means ownership and determinism remain easy to reason about.
- **Overall** is `35% usefulness + 30% benefit + 20% ease + 15% safety`, scaled to 100. Ties are ordered by runtime breadth and prerequisite value.

| Rank | Application | Usefulness | Benefit | Ease | Safety | Overall |
|---:|---|---:|---:|---:|---:|---:|
| 1 | Animation pose evaluation and CPU mesh deformation | 5 | 5 | 3 | 4 | 89 |
| 2 | CPU visibility culling and draw-candidate generation | 5 | 5 | 3 | 4 | 89 |
| 3 | Parallel editor asset imports | 5 | 5 | 3 | 4 | 89 |
| 4 | Non-blocking managed script compilation | 4 | 5 | 4 | 5 | 89 |
| 5 | Coalesced asynchronous asset file scanning | 5 | 4 | 4 | 4 | 87 |
| 6 | Shader variations, passes, stages, and files | 4 | 5 | 4 | 4 | 86 |
| 7 | Worker-decoded audio streaming | 5 | 5 | 2 | 4 | 85 |
| 8 | Mesh LOD and meshlet preprocessing | 4 | 5 | 3 | 4 | 82 |
| 9 | Asynchronous runtime asset loading | 5 | 5 | 2 | 3 | 82 |
| 10 | Recover editor/render-thread overlap | 5 | 5 | 2 | 3 | 82 |
| 11 | Asynchronous procedural node-graph evaluation | 4 | 5 | 3 | 3 | 79 |
| 12 | Dedicated audio owner thread | 5 | 4 | 2 | 4 | 79 |
| 13 | Explicit parallel world-transform propagation | 5 | 4 | 2 | 3 | 76 |
| 14 | Asynchronous scene load and save | 4 | 4 | 3 | 4 | 76 |
| 15 | Parallel render-snapshot extraction | 4 | 4 | 3 | 4 | 76 |
| 16 | Batched parallel physics queries | 4 | 4 | 3 | 4 | 76 |
| 17 | Parallel shadow-view construction and culling | 4 | 5 | 2 | 3 | 75 |
| 18 | Vulkan secondary command-buffer recording | 5 | 5 | 1 | 2 | 75 |
| 19 | Pixel conversion and mip-generation row jobs | 3 | 4 | 4 | 4 | 73 |
| 20 | Cached worker-side text layout | 3 | 4 | 3 | 4 | 69 |

## Existing concurrency and prerequisites

### What already exists

- `Crowny/Source/Crowny/Threading/TaskSystem.cpp:67-183` creates `hardware_concurrency - 2` workers. The only substantial production consumer found is `Crowny-Editor/Source/Editor/AssetPreviewService.cpp:473-475`.
- `Crowny/Source/Crowny/Application/Application.cpp:73-81` starts `RenderThread` for Vulkan. OpenGL stays on the main thread because its context is thread-affine.
- `Crowny/Source/Crowny/Renderer/RenderThread.cpp:42-175` already uses reusable frame contexts and a locked GPU-resource command queue.
- `Crowny/Source/Crowny/Physics/JoltPhysicsBackend.cpp:182-242` creates a Jolt worker pool and passes it to `PhysicsSystem::Update`.
- Basis encoding already enables its own threaded mode in `Crowny/Source/Crowny/Renderer/BasisTextureCodec.cpp:185`.
- Font atlas generation already uses its library's workload threading in `Crowny/Source/Crowny/Import/FontImporter.cpp:326-341`.

### Fix the task system before broad adoption

This is an enabling change rather than one of the 20 application sites.

- `Crowny/Source/Crowny/Threading/TaskSystem.cpp:110-113` and `:140-142` block a worker while waiting for another queued task. If enough high-priority dependants start before their lower-priority producers, all workers can block and the pool can deadlock. Use dependency ready-counts and continuations, or let a waiting worker execute ready work.
- `Crowny/Source/Crowny/Threading/TaskSystem.cpp:173-182` scans and erases from one shared vector for every task. Replace this with per-priority queues or work-stealing deques before submitting thousands of fine-grained jobs.
- `Crowny/Source/Crowny/Threading/TaskSystem.cpp:119-123` has no exception boundary. Capture `std::exception_ptr`, put the task into a terminal failed state, and notify waiters.
- Add chunked `ParallelFor`, owner-scoped fences, cancellation, and generation invalidation. Every scene, project, layer, or audio owner must drain its tasks before destruction.
- Coordinate one CPU budget. Crowny reserves nearly all cores for `TaskSystem`, while Jolt can independently reserve nearly all cores again. Cap nested libraries or adapt Jolt jobs to the engine scheduler.

## Ranked applications

### 1. Animation pose evaluation and CPU mesh deformation

**Where:** `Crowny/Source/Crowny/Scene/SceneRenderer.cpp:1577-1710`, `Crowny/Source/Crowny/Animation/Skeleton.cpp:277-351`, and `Crowny/Source/Crowny/Animation/MorphAnimation.cpp:55-124`.

`SceneRenderer::UpdateAnimations` serially updates each animated entity, evaluates its pose, deforms all vertices, calculates bounds, and only then queues the GPU upload.

**How:** split `AnimationPlayer::Update` into owner-thread `Advance` and worker-safe `Evaluate`. Keep events and root-motion hierarchy writes on the simulation thread. Run pose evaluation per entity; for large meshes, split deformation by vertex range and reduce task-local bounds. Join once before snapshot extraction, then commit only if entity, mesh, clip, and component generations still match.

**Safety condition:** workers receive immutable animation inputs and private output slots. They do not access EnTT, invoke callbacks, update transforms, or publish asset handles.

### 2. CPU visibility culling and draw-candidate generation

**Where:** `Crowny/Source/Crowny/Renderer/GpuScene.cpp`, in `GpuScene::BuildCpuDrawList`, which walks instances and selected LOD meshlets before sorting and uploading candidates.

**How:** partition immutable instance state into contiguous ranges. Each task builds a local candidate list and local counters. Merge by source range, then run the existing draw-list builder, sorting, and upload on the render thread.

**Safety condition:** remove worker mutation of shared draw candidates, builder state, and statistics. Keep resource lookup and GPU publication on the render thread.

### 3. Parallel editor asset imports

**Status:** implemented in `Crowny-Editor/Source/Editor/ImportScheduler.cpp` and the per-importer policy in `Crowny/Source/Crowny/Import/SpecificImporter.h`. The scheduler uses bounded `TaskSystem` lanes, publishes results in source order, and keeps unsafe importers on the main thread.

**Design:** importers opt into `ParallelWorker` or `SerializedWorker`; the default is `MainThreadOnly`. Worker-safe sources run in bounded jobs while publication and `Asset::Init` stay on the main thread. Source sequence controls publication order so UUID and manifest updates remain deterministic.

**Remaining work:** limit concurrent Basis and font work because those libraries already create their own workers. Extend deferred imports to represent every asset produced by `ImportAll`; multi-output importers still stay on the main thread.

### 4. Non-blocking managed script compilation

**Where:** `Crowny-Editor/Source/Editor/EditorLayerProject.cpp:429-507` synchronously invokes the compiler, then refreshes the managed domain. File-watch debounce reaches the same blocking path from `Crowny-Editor/Source/Editor/EditorLayer.cpp:624-631`.

**How:** execute the compiler process and capture output in a versioned background build job. Poll completion from the editor update. Validate and publish only the newest successful staging directory, then refresh assemblies on the main/Mono thread.

**Safety condition:** invoke the compiler as a native child process rather than calling the current managed compile method on an unattached worker. Domain unload, reflection, object backup, and restore stay on the Mono owner thread.

### 5. Coalesced asynchronous asset file scanning

**Where:** `Crowny-Editor/Source/Editor/EditorLayerProject.cpp:620-630`, `Crowny-Editor/Source/Editor/ProjectLibrary.cpp:115-132`, and `Crowny-Editor/Source/Editor/AssetLibraryServices.cpp`, in `AssetFileSystemScanner::Scan`.

`ExecuteProjectAssetRefresh` handles watcher events one at a time. Even `RefreshAsync` performs recursive scanning and diffing synchronously before it schedules imports.

**How:** deduplicate watcher paths into the smallest non-overlapping roots. Snapshot the relevant asset-index records, perform enumeration and diffing on a worker, and apply one ordered diff on the main thread. Attach a project generation so a result from a closed project is discarded.

**Safety condition:** workers compare against an immutable index snapshot. They never mutate `AssetIndex`, manifests, or file entries.

### 6. Shader variations, passes, stages, and files

**Where:** `Crowny/Source/Crowny/Utils/ShaderCompiler.cpp:632-918` and `Crowny/Source/Crowny/Utils/BuiltInShaderCompiler.cpp:138-202`.

Variations, render passes, stages, and changed built-in shader files are compiled serially. `CompileStage` creates local shaderc and SPIRV-Cross objects, and the shared shader cache already has a mutex.

**How:** preprocess a source once, schedule independent variation or stage jobs, store diagnostics per input index, and merge them in declaration order. Changed files can use a second bounded level only when the variation level is not already saturating the pool.

**Safety condition:** GPU pipeline creation and live shader replacement remain on the render thread. Cache insertion stays locked, and disk publication uses unique temporary files plus atomic rename.

### 7. Worker-decoded audio streaming

**Where:** `Crowny/Source/Crowny/Audio/AudioManager.cpp:221-235`, `Crowny/Source/Crowny/Audio/AudioSource.cpp:416-515`, and `Crowny/Source/Crowny/Audio/AudioClip.cpp:176-188`.

The main thread services every stream, decodes samples, converts PCM, and uploads OpenAL buffers. A streaming `AudioClip` also owns one mutable decoder cursor, so two sources can contend for the same state.

**How:** give each streaming source its own decoder and cursor. Decode ahead into fixed-size PCM blocks in an SPSC queue. Schedule one refill job per source using low and high watermarks. The audio owner consumes completed PCM and queues OpenAL buffers.

**Safety condition:** decoder ownership is exclusive, destruction cancels or invalidates in-flight jobs, and arbitrary workers never call OpenAL.

### 8. Mesh LOD and meshlet preprocessing

**Where:** `Crowny/Source/Crowny/Renderer/MeshProcessing.cpp:65-150`.

`BuildGpuGeometry` serially loops through LODs and submeshes while running meshoptimizer simplification, cache optimization, overdraw optimization, and meshlet construction.

**How:** compute each LOD/submesh into a local result block. Merge in stable LOD and material order while rebasing offsets. Use a size threshold so small submeshes stay inline.

**Safety condition:** input mesh data remains read-only, every meshoptimizer call receives local buffers, and only the merge step writes the final packed geometry.

### 9. Asynchronous runtime asset loading

**Where:** `Crowny/Source/Crowny/Assets/AssetManager.cpp:25-134` and GPU-facing texture and mesh deserialization in `Crowny/Source/Crowny/Assets/AssetCodecs.cpp`.

`AssetManager::Load` synchronously opens, deserializes, initializes, publishes, and notifies. Its handle and manifest maps are not ready for concurrent mutation.

**How:** coalesce each UUID into one in-flight request with explicit `Loading`, `Ready`, and `Failed` states. Workers perform file I/O and CPU decode into deferred data. The main thread publishes handles and listener events; Vulkan resource creation uses `RenderThread::EnqueueResourceCommand`.

**Safety condition:** split codecs into CPU decode and GPU finalize phases. Protect manager state, publish pointers with a clear happens-before edge, and drain owner-scoped load fences before asset-system shutdown.

### 10. Recover editor/render-thread overlap

**Where:** `Crowny-Editor/Source/Editor/EditorLayer.cpp:638-643` waits for the render thread every frame before overlays and entity picking, largely cancelling the existing frame-context pipeline.

**How:** place overlay commands in `RenderSnapshot`, render them on the Vulkan render thread, and make picking an asynchronous readback consumed one frame later. Wait only when a frame context or resource truly needs reuse.

**Safety condition:** remove hidden reliance on the global wait first. In particular, the procedural-mesh upload result in `SceneRenderer::UpdateProceduralMeshes` needs an atomic completion state and generation check.

### 11. Asynchronous procedural node-graph evaluation

**Where:** `Crowny/Source/Crowny/Scene/SceneRenderer.cpp:1480-1574`, `Crowny-Editor/Source/Panels/NodeEditor/NodeEditorPanel.cpp`, and `Crowny/Source/Crowny/NodeGraph/NodeGraphEvaluator.cpp:18-170`.

Dirty procedural components and editor previews evaluate synchronously. The evaluator uses mutable caches and raw node/pin pointers, while `NodeGraph` stores the last error as shared mutable state.

**How:** compile or clone an immutable evaluation graph, capture its revision and input values, and evaluate different components independently. Return mesh data and diagnostics together. Commit only if the graph and component generations still match. Coalesce editor requests so newer edits supersede old jobs.

**Safety condition:** editor mutation cannot overlap a worker reading live graph objects. Parallel branches inside one graph should come later, using a topological plan and dedicated result slots rather than locks around the recursive evaluator.

### 12. Dedicated audio owner thread

**Where:** `Crowny/Source/Crowny/Audio/AudioManager.cpp` and `Crowny/Source/Crowny/Audio/AudioSource.cpp`. `AudioManager::EnsureContextCurrent` exposes the existing OpenAL context-affinity concern.

**How:** create and bind the OpenAL context on one audio thread. Send source creation, playback, property changes, mixer changes, buffer queueing, and deletion through a command queue. Run stream maintenance at an audio cadence independent of frame rate and publish cached read-only state to the simulation thread.

**Safety condition:** getters stop making synchronous OpenAL calls from arbitrary threads. Shutdown stops new commands, drains deletes, releases sources, destroys the context, and joins in that order.

### 13. Explicit parallel world-transform propagation

**Where:** `Crowny/Source/Crowny/Ecs/Components.h:183-225` and `Crowny/Source/Crowny/Ecs/Entity.cpp:186-208`.

World-transform getters are logically const but lazily mutate cached values. Transform invalidation recursively visits descendants and immediately notifies audio and physics. Those semantics prevent safe parallel readers.

**How:** add an explicit transform phase. Rebuild hierarchy depth lists when structure changes, compute parents before children, and process all entities at one depth in parallel. Later systems read frozen world matrices for the frame.

**Safety condition:** defer audio and physics notifications to an ordered commit queue. Do not parallelize the current lazy getters.

### 14. Asynchronous scene load and save

**Where:** `Crowny/Source/Crowny/Serialization/SceneSerializer.cpp:79-432` and synchronous editor calls in `Crowny-Editor/Source/Editor/EditorLayerProject.cpp`.

YAML/binary parsing, entity traversal, encoding, and file I/O run on the caller. Loading also mutates the live registry while decoding.

**How:** parse into a detached scene DTO on a worker, then construct or swap the live registry at a safe main-thread boundary. For save, capture a stable component snapshot on the owner thread, encode it on a worker, write a temporary file, and atomically replace the destination.

**Safety condition:** managed-field capture, component callbacks, asset publication, hierarchy linking, and live EnTT mutation remain serial. A scene generation rejects late load results.

### 15. Parallel render-snapshot extraction

**Where:** `Crowny/Source/Crowny/Scene/SceneRenderer.cpp:1713-1868` and `:1993-2238`.

Mesh, procedural, sprite, text, and light views are traversed serially. World transforms, bounds, descriptors, and legacy draw objects are also prepared on the simulation thread.

**How:** after the explicit transform phase, run read-only view scans into task-local vectors. Merge in stable entity order and retain the existing final 2D sort. Split persistent `RenderWorld` resource-map changes into computed change records followed by a serial commit.

**Safety condition:** EnTT structural changes are forbidden during extraction. Workers do not write shared `FrameVector`, persistent render-world maps, camera history, or resource tables.

### 16. Batched parallel physics queries

**Where:** `Crowny/Source/Crowny/Physics/Physics3D.cpp`, in `Raycast`, `Sweep`, and `Overlap`, and the corresponding implementations in `Crowny/Source/Crowny/Physics/JoltPhysicsBackend.cpp`.

The public API issues one synchronous query at a time, even when gameplay, AI, or visibility code has many independent requests.

**How:** add indexed `RaycastBatch`, `SweepBatch`, and `OverlapBatch` request/result arrays. Jolt can execute read-only requests concurrently during a defined query phase. Provide a serialized fallback until each other backend proves concurrent-query support.

**Safety condition:** the query phase cannot overlap body creation, destruction, shape mutation, or simulation unless the backend explicitly supports it. Results are consumed after one batch fence, not through concurrent scene callbacks.

### 17. Parallel shadow-view construction and culling

**Where:** `Crowny/Source/Crowny/Scene/SceneRenderer.cpp`, in the shadow-render-view construction around lines 2490-2655.

Directional cascades, spot lights, and six point-light faces are prepared serially, and each view builds another CPU draw list.

**How:** reserve atlas/layer ownership serially, then compute view matrices, immutable shadow records, and culling results per scheduled light or face. Merge and upload on the render thread.

**Safety condition:** implement the task-local culling output from rank 2 first. Shared atlas allocation, render-world mutation, and uploads stay serial.

### 18. Vulkan secondary command-buffer recording

**Where:** `Crowny/Source/Crowny/Renderer/RenderGraph.cpp:549-610` executes every compiled pass callback serially. `Crowny/Source/Platform/Vulkan/VulkanCommandBuffer.cpp:282-363` already keeps command pools per thread.

**How:** form batches of render-graph passes whose dependencies are satisfied, record secondary command buffers on workers, and execute them in graph order from one primary command buffer. Centralize barriers, queue ownership, and submission.

**Safety condition:** this needs explicit per-job recording contexts and thread-safe renderer caches. The current buffer-pool path must distinguish primary and secondary buffers; `GetBuffer(..., secondary)` presently reaches a creation path that hardcodes a primary buffer. OpenGL remains serial.

### 19. Pixel conversion and mip-generation row jobs

**Where:** `Crowny/Source/Crowny/Utils/PixelUtils.cpp:149-277` and `:480-564`.

Format conversion, source conversion, normal-map renormalization, and output packing use large independent pixel loops.

**How:** divide each image or slice into row ranges with disjoint destination spans. Reduce any statistics after the group. Keep mip levels sequential because level N depends on level N-1, but parallelize rows within each sufficiently large level.

**Safety condition:** tasks never share destination pixels. Keep small images inline and do not add another full-width outer layer around already-threaded Basis encoding.

### 20. Cached worker-side text layout

**Where:** `Crowny/Source/Crowny/Renderer/Renderer2D.cpp:474-1053`. `Renderer2D::DrawString` calls `TextLayout::Build`, which performs several token, line, and glyph passes using one global scratch buffer.

**How:** key a layout cache by text, font revision, size, wrapping, spacing, and alignment. When a text component changes, build layout with task-local scratch and publish owned glyph/line arrays into the render snapshot. The render thread only emits vertices in `Ordered2D` order.

**Safety condition:** `TextLayoutResult` currently points into scratch storage, so the cached result must own its arrays. Font metrics and glyph geometry must be immutable for the job's lifetime.

## Suggested implementation sequence

1. Harden `TaskSystem`, add owner fences and Tracy queue/wait instrumentation, and define the engine-wide CPU budget.
2. Deliver isolated editor wins: managed compilation, file scanning, shader compilation, and bounded imports.
3. Add pure-data worker kernels: pixel rows, mesh preprocessing, animation evaluation/deformation, and CPU culling.
4. Introduce explicit immutable frame data: transform propagation, snapshot extraction, procedural graph snapshots, and text layout results.
5. Add staged I/O: runtime assets and scene load/save.
6. Move audio behind one owner thread, then add worker decode queues.
7. Add batched physics queries. Attempt asynchronous physics stepping only after the immediate physics API has a command-buffer boundary.
8. Remove the editor render barrier and consider Vulkan secondary recording last, after profiling confirms command recording is a bottleneck.

## Ideas to avoid for now

- Do not parallelize `ScriptRuntime::OnUpdate`. `Crowny/Source/Crowny/Scene/ScriptRuntime.cpp:88-108` invokes arbitrary managed callbacks that can mutate ECS, physics, audio, assets, and scene lifecycle. A future script-job API should expose snapshots and command buffers instead of raw entities.
- Do not wrap `Physics3D::Step` in another full-pool task and assume it becomes faster. Jolt is already internally parallel and currently competes with Crowny's worker count. An asynchronous coordinator is an ownership and frame-pipelining project, not a one-line task submission.
- Do not mutate live EnTT registries, `FrameVector` instances, `AssetManager` maps, OpenGL state, OpenAL state, or GPU resources from arbitrary workers.
- Do not count particles as an immediate target. The repository has a particle sprite primitive but no particle simulation to retrofit.

## Validation plan

- Record Tracy baselines for editor startup/import, scene load, animation, culling, render-thread waits, audio refill, and task queue contention.
- Add deterministic tests that compare single-worker and multi-worker outputs byte for byte where practical.
- Run ThreadSanitizer on Linux for task-owned CPU data and the existing Windows ASan/CRT configuration for lifetime errors.
- Add forced cancellation and shutdown tests for scenes, projects, asset loads, audio sources, and graph evaluations.
- Keep serial fallbacks and size thresholds. A job should be large enough to repay queueing and merge costs.
- Renderer changes must also run `Scripts/crowny.bat render-tests`; new headers require `python Scripts/check_headers.py`.
