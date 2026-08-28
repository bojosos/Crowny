# Renderer backlog

This file records the renderer requests that were queued on August 24, 2026, together with their implementation state. Keep it until every remaining item below is implemented and tested.

## Source prompts

### Renderer rebuild and follow-ups

> Make a plan for a full renderer for Crowny. I want exceptional performance, nice API, support for millions of objects, proper instancing and batching, consider also making it GPU driven, make sure it is extensible, in the future we will probably want ray-tracing (but that's very far off). I want materials, meshes, text, sprites, circles, IBL, PBR, sorting, alpha, shadows, compositing/post processing, and particles. Consider deferred, tiled, forward, tiled forward/tiled deferred, modern techniques, Unity/Unreal approaches, and keep it runnable on this current laptop.

> Btw the Real-Time Rendering edition 4 had some nice papers about GPU driven pipelines which I quite liked.

> PLEASE IMPLEMENT THIS PLAN: Crowny renderer rebuild.

The supplied implementation plan is retained here as its durable acceptance outline:

- real hardware-tier discovery and fallbacks for OpenGL compatibility, Vulkan baseline, GPU-driven Vulkan, and future mesh-shader/ray-tracing tiers;
- frame contexts, a compiled render graph, explicit resource states, dynamic rendering/synchronization2 fallbacks, timestamps, debug regions, and restrained queue use;
- a persistent generational-handle GPU scene with dirty-range uploads and no stable-scene per-object submission or allocation;
- meshoptimizer import, LODs, meshlets, geometry heaps, two-phase Hi-Z occlusion, GPU binning, indirect commands, and multi-draw submission;
- bindless/material property APIs, render features, native and managed surfaces, shader variants, and serialized migrations;
- clustered Forward+ and Deferred+ with shared PBR/IBL/shadow code, Forward+ transparency, reverse-Z, and laptop-sensitive `Auto` selection;
- physical lights, clustered assignment, directional cascades, cached spot/point atlases, shadow budgets, and GPU shadow culling;
- alpha modes, stable transparent/2D sorting, instanced sprites/circles/text, cached text layout, object-ID picking, and asynchronous readback;
- post-processing/compositing insertion points, GTAO/TAA/FXAA/bloom/exposure/tonemapping/color grading/dynamic resolution, and per-camera history;
- compute skinning and morphs, GPU particles, reflection probes, streaming, and raster data structures that can later coexist with ray tracing;
- automated correctness, migration, shader-parity, Vulkan validation, OpenGL smoke, renderer capture, and laptop performance gates from the original plan.

Status: ongoing. Much of this architecture exists in the current working tree, but this durable list stays open until the implementation and performance gates are audited end-to-end.

> do not rebuild so often btw

Build policy: batch edits and validations; prefer selected translation-unit compilation, focused tests, and `-NoBuild` renderer runs once binaries are current. Do not repeatedly rebuild the solution.

> Oh yeah and lights.... you will need to add proper lights

Status: implemented in the renderer work (directional, point, and spot lights, clustered extraction, physical intensity conversion, and scheduled shadows); retain correctness/performance coverage in the master plan.

> Finish the integration of basis universal and compressed textures

Status: implemented, including Basis/KTX2 encode/transcode for 2D and cubemap arrays, checked layer/face/mip metadata, compressed format selection, flattened Vulkan/OpenGL runtime uploads, fallback behavior, and mip-chain integration.

> and continue with the renderer, i saw it's not finished

> Finish whatever is left of the renderer plan

Status: ongoing under the master renderer acceptance outline above. Do not interpret a completed subtask as completion of the whole renderer.

> Make sure all of the skeletal and morph animation stuff is well integrated into the renderer

Status: CPU deformation is integrated across culling bounds, depth, shadows, velocity, opaque shading, and OpenGL fallback. Unchanged poses now settle previous-position motion data once and then skip repeated deformation and full-mesh uploads. Compute skinning and morph caching remain open.

### Current implementation batch

> Integrate a mip generation library and the rproper rendering of mips

> Add more material models, I want a cel shading model too, toon like shading with outlines, scratching etc, there was something, look online for things like https://godotshaders.com/shader/ultimate-toon-shader/ https://godotshaders.com/shader/complete-toon-shader/ https://gameidea.org/2024/02/15/toon-cel-shader/ look other approaches too find the best one and best parameter contorls we should expose

> Do we have allocators for things like vertex buffers, index buffers, pipelines? Look at here I think it would make sense to have pools for those so we can easily get already pre-allocated buffers, tehy have for gpu buffers it seems samplers etc https://github.com/GameFoundry/B3DFramework/tree/master/Source/Engine/Core/GpuBackend

## 1. Mip generation and rendering

Status: core integration, isolated GPU mip-selection coverage, and deterministic compressed-array codec fixtures delivered

- [x] Reuse the pinned Basis Universal resampler for import-time mip generation instead of adding a second image-resize dependency.
- [x] Filter color textures in linear light, premultiply alpha during filtering, renormalize normal maps, optionally preserve alpha coverage, and support box, triangle, Mitchell, Lanczos4, and Kaiser filters.
- [x] Generate one shared chain for uncompressed textures and Basis/KTX2 compression; accept validated authored chains in the Basis codec.
- [x] Preserve the full chain through texture import, serialization, backend upload, subresource views, and Vulkan/OpenGL sampling.
- [x] Use mip-capable sampler filters, explicit LOD ranges and bias, and feature-checked/clamped anisotropy. `MipFilter::NONE` samples mip zero only.
- [x] Unit-test odd dimensions, one-pixel termination, sRGB filtering, alpha edges, normal renormalization, and maximum-level limits.
- [x] Add an authored four-level mip-selection render reference. Vulkan and OpenGL both select the requested mip and match the shared reference on Intel Iris Xe.
- [x] Add deterministic authored compressed 2D-array and cubemap-array fixtures with layer/face/mip identity and malformed-range coverage.

## 2. Material models and toon rendering

Status: core model, screen-space outlines, ramps, and matcaps delivered; GPU-driven silhouette and public-tooling work remains

- [x] Research the supplied Godot shaders and configurable production-style toon approaches.
- [x] Add explicit Standard, Unlit, and Toon GPU material models with one std430 record shared by Forward+ and Deferred+.
- [x] Add stepped diffuse bands, edge smoothing, shadow tint, banded specular, shadow-aware rim light, emissive/IBL contribution, and artist-controlled hatching/scratch textures.
- [x] Support UV, triplanar, and screen-space pattern mapping with scale, strength, smoothness, and distance fade.
- [x] Add a material-aware screen-space depth/normal outline pass after opaque lighting and before transparency, with color, width, thresholds, and distance fade.
- [x] Preserve shared skinning/morph deformation, depth, velocity, object-ID, shadow, alpha, and material-index paths around the new shading model.
- [x] Add bindless diffuse-ramp and view-space matcap texture modes with strength, offset, and rotation controls shared by Forward+ and Deferred+.
- [x] Preserve the existing opt-in inverted-hull pass in the legacy toon shader and expose its outline controls.
- [x] Cook and pack the legacy `Toon.asset` and `Unlit.asset` with the material-model-aware compiler. Their sources are now language-tagged so the built-in cooker no longer skips them.
- [x] Keep successfully cooked or content-hash-valid built-in shader, icon, and font asset timestamps synchronized with their sources, including future-dated inputs, and allow explicit Dist cooker runs so strict packaging works after a fresh checkout.
- [ ] Move inverted-hull silhouettes onto GPU-driven indirect submission, then add managed convenience APIs/editor presets, asset migration, and dedicated toon golden images.

## 3. GPU resource allocation and caches

Status: reusable buffer and texture pools, caches, a static geometry suballocator, and persistent immutable-mesh residency delivered

- [x] Inventory the existing VMA-backed Vulkan allocation, frame upload rings, render-graph reuse/aliasing, descriptor pools, render-target cache, pipeline variants, and persistent Vulkan pipeline cache.
- [x] Compare Crowny with B3DFramework's frame-delayed transient buffer recycling and suballocation design.
- [x] Add exact-descriptor whole-object pools for generic, vertex, and index buffers above the backend allocator, with retained-byte budgets, explicit trimming, hit/creation/rejection statistics, and frames-in-flight retirement.
- [x] Connect the buffer pool to render-graph physical resource acquisition/release so transient buffers are reused automatically.
- [x] Add a complete-descriptor immutable sampler cache and clear it during renderer shutdown.
- [x] Test frame-delayed reuse and object identity after the safe retirement window.
- [x] Add fixed-capacity vertex/index geometry heaps with aligned best-fit suballocation, generational handles, frames-in-flight deferred frees, range coalescing, uploads, and allocation/high-water/fragmentation telemetry.
- [x] Back immutable Vulkan meshes with persistent vertex/index heap pages grouped by structural vertex layout, index width, and topology. Use stable heap binding IDs, GPU-to-GPU buffer copies, meshlet-index uploads, delayed allocation reuse, per-mesh fallbacks for dynamic/skinned/morphed/OpenGL geometry, and capacity/live/high-water telemetry.
- [x] Add GPU-only draw-run compaction for heap-resident opaque and masked main shading. Persistent CPU-known bins own deterministic command segments and count offsets; compute compacts visible meshlets into those segments and Vulkan submits them with indirect counts without visibility readback or per-object CPU draws.
- [x] Make GPU draw-bin admission non-lossy under command and device limits. A bin is accepted only when its complete known demand fits its fixed segment; rejected bins stay on the CPU fallback path instead of losing overflowed geometry.
- [x] Share resolved clustered-light dimensions and limits between graph allocation, compute dispatch, and lighting. Non-default tile, depth-slice, per-cluster, and directional-light settings now use matching bounds, including the shader's 128-light local limit.
- [x] Invalidate cached spot and point shadows when a shadow caster is created, destroyed, transformed, reconfigured, animated, or has relevant mesh/material residency changes.
- [x] Reject zero-sized or otherwise invalid transient texture descriptors from the reuse pool so they cannot alias valid one-valued descriptors.
- [ ] Extend GPU draw generation beyond this bounded path. Vulkan baseline, OpenGL, early depth, shadow views, dynamic/skinned/morphed/per-mesh geometry, rejected bins, and strict transparency intentionally retain CPU submission until their ordering and fallback contracts are proven. Transparent GPU radix sorting and toon inverted-hull submission remain separate work.
- [x] Add pooled transient images where render-graph lifetime aliasing cannot reuse an allocation.
- [x] Route explicitly classified custom opaque materials through the post-lighting forward-only pass; keep standard GPU bins separate and reject unsupported records during GPU compaction.
- [x] Require explicit `#pragma material_model custom` metadata before using the reverse-Z forward-only contract; unmarked shaders remain unsupported instead of inheriting an incompatible depth state.
- [x] Settle persistent-instance previous transforms one frame after motion stops without allocating or resubmitting stable objects forever.
- [x] Retire inactive and scene-replaced camera history namespaces, and preserve frame-context numbers through snapshot extraction.
- [x] Key runtime camera history by stable generational entity identity so ECS camera-storage relocation cannot reset TAA or Hi-Z history, age retention by distinct frames rather than camera count, and retire it when a scene remains camera-less.
- [x] Consume queued camera-history releases even after the view loses its render target.
- [x] Isolate temporal history between renderer/view owners that render the same scene camera.
- [x] Give view owners monotonic identities and queue namespace-only teardown releases directly to the render thread behind a submission watermark, so destruction does not depend on another snapshot.
- [x] Ping-pong temporal resources per camera/history entry rather than global frame parity, preserving history when cameras skip or interleave frames.
- [x] Commit temporal ping-pong writes only after a successful graph execution; failed frames preserve the last valid history slot.
- [x] Invalidate per-camera TAA and Hi-Z history when its resolved Forward+/Deferred+ path changes or it falls back through compatibility rendering.
- [x] Keep process-wide renderer helper data alive until the last `SceneRenderer` consumer is destroyed; preview rendering also disables the editor grid it does not use.
- [x] Use the simulation frame for standalone snapshot extraction so public value-returning overloads do not age histories by camera count.
- [x] Transform culling spheres with a shear-safe singular-value bound instead of the longest basis vector.
- [x] Apply visibility, layer, and frustum culling to custom forward-only and compatibility draws; pin mixed custom-material instances to LOD zero until their forward pass consumes geometry-heap LOD ranges.
- [x] Reject custom forward-only depth pragmas that violate the reverse-Z depth-prepass contract before expanding shader variations.
- [ ] Add material-aware masked depth/shadow passes and transparent ordering before routing custom masked or transparent materials through the new renderer.
- [x] Alpha-test standard GPU-record masked materials in the static and animated main depth variants, including motion-vector and object-ID output layouts; retain the material-aware shadow alpha test.
- [x] Cook and pack the independent object-ID-only depth variants for static and animated geometry.
- [x] Verify the complete depth-prepass output matrix and route it from per-view flags: depth-only, motion-vector-only, object-ID-only, and combined motion-vector/object-ID. Runtime views skip the optional ID target by default; editor submissions request it for picking.

## Validation

- [x] Focused `[Mips],[Materials],[Resources],[Shader],[Animation]` tests: 28 cases and 277 assertions passed; all 14 GPU-driven and legacy toon shader assets compiled together.
- [x] `Scripts\run-render-tests.ps1 -Configuration Release -Backend All -NoBuild`: Vulkan and OpenGL each passed 4 captures on Intel Iris Xe, including explicit mip selection; all 4 backend comparisons passed.
- [x] `python Scripts/check_headers.py`: passed.
- [x] Full Release validation after the integration repair: 295 Catch2 cases and 25,025 assertions passed with clean process exit.
- [x] Vulkan and OpenGL renderer regression captures passed 4/4 each, with all 4 cross-backend comparisons matching.
- [x] Normal Vulkan editor shutdown completed with exit code 0, zero assertions, and zero VMA leak lines; the former 15 leaked storage allocations (~27.01 MB) are fixed.
- [x] Isolated GPU draw-bin validation: focused layout/graph/shader coverage passed 136 assertions in 11 cases; focused GPU-scene coverage passed 94 assertions in 13 cases; full Release Catch2 passed 25,405 assertions in 351 cases.
- [x] Updated 54-resource built-in pack loaded successfully; Release render harness passed Vulkan 4/4 and OpenGL 4/4 on Intel Iris Xe, with all 4 cross-backend captures matching.
- [x] Isolated transient-texture validation: focused ASan `[Renderer][Resources]` coverage passed 94 assertions in 8 cases, including descriptor separation, frame-delayed reuse, budget rejection, trimming, and render-graph retirement.
- [x] Run the focused renderer/shader regression batch for the temporal-history, custom-material, and motion-settling repairs. The final isolated ASan suite passed 29,142 assertions in 558 cases, and the Vulkan/OpenGL render harness passed 4/4 captures per backend with all 4 cross-backend comparisons matching.
- [x] Depth-output matrix validation: focused Release `[Renderer][Pipeline]` passed 180 assertions in 9 cases; full no-build Release Catch2 passed 29,351 assertions in 570 cases with one optional CoreCLR case skipped; Vulkan and OpenGL each passed 5/5 captures on Intel Iris Xe, and all 5 cross-backend captures matched.
- [x] Standard masked-depth validation: the focused Release renderer/shader batch passed 256 assertions in 12 cases; full no-build Release Catch2 passed 29,389 assertions in 572 cases with one optional CoreCLR case skipped; the 59-resource pack loaded on Vulkan and OpenGL, which each passed 5/5 captures with all 5 cross-backend comparisons matching.
- [ ] Linux CI has progressed past SPIRV-Cross header discovery; keep the current Actions run as the authoritative Linux compile result.
