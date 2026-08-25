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

Status: implemented in the current working tree, including Basis/KTX2 encode/transcode, compressed format selection, subresource validation, fallback behavior, and the mip-chain integration below. Keep compressed cubemap/array fixtures open under mip validation.

> and continue with the renderer, i saw it's not finished

> Finish whatever is left of the renderer plan

Status: ongoing under the master renderer acceptance outline above. Do not interpret a completed subtask as completion of the whole renderer.

> Make sure all of the skeletal and morph animation stuff is well integrated into the renderer

Status: integrated into GPU-scene deformation, culling bounds, shared depth/shadow/velocity/opaque paths, previous-frame data, and CPU/OpenGL fallback work in the current tree; retain parity and stress coverage in the master plan.

### Current implementation batch

> Integrate a mip generation library and the rproper rendering of mips

> Add more material models, I want a cel shading model too, toon like shading with outlines, scratching etc, there was something, look online for things like https://godotshaders.com/shader/ultimate-toon-shader/ https://godotshaders.com/shader/complete-toon-shader/ https://gameidea.org/2024/02/15/toon-cel-shader/ look other approaches too find the best one and best parameter contorls we should expose

> Do we have allocators for things like vertex buffers, index buffers, pipelines? Look at here I think it would make sense to have pools for those so we can easily get already pre-allocated buffers, tehy have for gpu buffers it seems samplers etc https://github.com/GameFoundry/B3DFramework/tree/master/Source/Engine/Core/GpuBackend

## 1. Mip generation and rendering

Status: core integration and isolated GPU mip-selection coverage delivered; compressed cubemap/array fixtures remain

- [x] Reuse the pinned Basis Universal resampler for import-time mip generation instead of adding a second image-resize dependency.
- [x] Filter color textures in linear light, premultiply alpha during filtering, renormalize normal maps, optionally preserve alpha coverage, and support box, triangle, Mitchell, Lanczos4, and Kaiser filters.
- [x] Generate one shared chain for uncompressed textures and Basis/KTX2 compression; accept validated authored chains in the Basis codec.
- [x] Preserve the full chain through texture import, serialization, backend upload, subresource views, and Vulkan/OpenGL sampling.
- [x] Use mip-capable sampler filters, explicit LOD ranges and bias, and feature-checked/clamped anisotropy. `MipFilter::NONE` samples mip zero only.
- [x] Unit-test odd dimensions, one-pixel termination, sRGB filtering, alpha edges, normal renormalization, and maximum-level limits.
- [x] Add an authored four-level mip-selection render reference. Vulkan and OpenGL both select the requested mip and match the shared reference on Intel Iris Xe.
- [ ] Add authored compressed cubemap/array fixtures.

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
- [ ] Move inverted-hull silhouettes onto GPU-driven indirect submission, then add managed convenience APIs/editor presets, asset migration, and dedicated toon golden images.

## 3. GPU resource allocation and caches

Status: reusable object pools, caches, a static geometry suballocator, and persistent immutable-mesh residency delivered

- [x] Inventory the existing VMA-backed Vulkan allocation, frame upload rings, render-graph reuse/aliasing, descriptor pools, render-target cache, pipeline variants, and persistent Vulkan pipeline cache.
- [x] Compare Crowny with B3DFramework's frame-delayed transient buffer recycling and suballocation design.
- [x] Add exact-descriptor whole-object pools for generic, vertex, and index buffers above the backend allocator, with retained-byte budgets, explicit trimming, hit/creation/rejection statistics, and frames-in-flight retirement.
- [x] Connect the buffer pool to render-graph physical resource acquisition/release so transient buffers are reused automatically.
- [x] Add a complete-descriptor immutable sampler cache and clear it during renderer shutdown.
- [x] Test frame-delayed reuse and object identity after the safe retirement window.
- [x] Add fixed-capacity vertex/index geometry heaps with aligned best-fit suballocation, generational handles, frames-in-flight deferred frees, range coalescing, uploads, and allocation/high-water/fragmentation telemetry.
- [x] Back immutable Vulkan meshes with persistent vertex/index heap pages grouped by structural vertex layout, index width, and topology. Use stable heap binding IDs, GPU-to-GPU buffer copies, meshlet-index uploads, delayed allocation reuse, per-mesh fallbacks for dynamic/skinned/morphed/OpenGL geometry, and capacity/live/high-water telemetry.
- [ ] Finish GPU-only draw-run compaction so the main shading path consumes compute-generated run/count buffers without CPU draw-list generation.
- [ ] Add pooled transient images where render-graph lifetime aliasing cannot reuse an allocation.

## Validation

- [x] Focused `[Mips],[Materials],[Resources],[Shader],[Animation]` tests: 28 cases and 277 assertions passed; all 14 GPU-driven and legacy toon shader assets compiled together.
- [x] `Scripts\run-render-tests.ps1 -Configuration Release -Backend All -NoBuild`: Vulkan and OpenGL each passed 4 captures on Intel Iris Xe, including explicit mip selection; all 4 backend comparisons passed.
- [x] `python Scripts/check_headers.py`: passed.
- [x] Full Release validation after the integration repair: 295 Catch2 cases and 25,025 assertions passed with clean process exit.
- [x] Vulkan and OpenGL renderer regression captures passed 4/4 each, with all 4 cross-backend comparisons matching.
- [x] Normal Vulkan editor shutdown completed with exit code 0, zero assertions, and zero VMA leak lines; the former 15 leaked storage allocations (~27.01 MB) are fixed.
- [ ] Linux CI has progressed past SPIRV-Cross header discovery; keep the current Actions run as the authoritative Linux compile result.
