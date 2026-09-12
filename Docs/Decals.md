# Decals

Decals are ordinary entities with a `DecalComponent` and a reusable `.cwmat` material in the decal domain. The component controls placement, projection, receiver filtering and instance appearance. Editing instance tint, opacity or UVs does not change the shared material.

The validation notes record automated rendering coverage, Windows authoring checks, hardware measurements and current limits.

## Authoring

Create a Box Decal or Cylinder Decal from the hierarchy, or add a Decal component to an existing entity. Assign a decal material in the component inspector. The texture picker can create a reusable decal material from an existing texture asset. Surface material slots reject loaded decal materials.

Use the entity transform to position, rotate and parent the projector. Use the component dimensions or viewport bounds handles to resize its projection volume. Parent scale affects the volume. Bounds resizing preserves the entity transform.

Box decals map local XY to UV and project along local negative Z. Width, height and depth are full dimensions. Make the depth small to avoid affecting nearby surfaces. Facing rejection uses the receiver's geometric normal.

Cylinders use local Y as their axis. Bottom and top radii define a tapered shell; equal radii produce a straight cylinder. Height is the full axial extent and shell thickness is the full radial band. Arc and seam rotation are in degrees. A 360-degree wrap has no angular edge feather. Nonuniform parent scale produces an elliptical cross section. This projection does not unwrap a mesh's UVs.

The selected cylinder draws a translucent label sleeve with top and bottom edges, faint inner and outer shell guides, and a gold seam. In Bounds mode, square handles resize the two radii, height edges and shell thickness. Gold diamond handles edit wrap angle and seam rotation. Those two handles sit at different heights so they remain separate on a full wrap. Hover a handle to see its name. Resize the component around the bottle with its entity scale at one; use different end radii when the bottle tapers.

Negative UV scale flips artwork. UV rotation is in degrees. Resizing stretches textures by default; Preserve Texel Density adjusts UV scale. Box face and cylinder cap drags keep the opposite face fixed. Shift resizes about the center. A drag commits one undo action containing dimensions, offset and UV settings; Escape restores all of them.

The component inspector places U/V flip checkboxes on one row. Shape-specific fields are hidden for mixed box/cylinder selections. Receiver targets, receiver layer settings, distance-fade start and expiry controls are disabled when their prerequisites are inactive; their saved values are preserved. Lifetime and fade durations are labeled in seconds, with zero lifetime meaning persistent.

Receiver layers are independent of camera visibility layers. Mesh and procedural receivers have Receive Decals and Decal Layers settings. Entity and Subtree targeting also require matching layers. Deleting an explicit target disables that decal's contribution until it is reassigned. Duplication and prefab instantiation remap references within the copied hierarchy.

## Materials

The decal material inspector exposes independent switches and strengths for color, normals, roughness, metallic, ambient occlusion, emission, corrections and coating. Surface materials expose a corresponding response mask. Disabled channels preserve the sampled receiver attribute.

Each channel has a collapsible section with its enable switch and dependent controls. Inactive controls are disabled. Blend modes use named dropdowns. Coverage opacity is separate from RGB tint, and emission intensity has its own control. Coverage textures remain editable regardless of the color-channel switch because their alpha affects other channels. The packed surface map is enabled when roughness, metallic or ambient occlusion is enabled. Surface shaders without a decal interface display disabled response controls.

Coverage combines base texture alpha, the mask texture, instance opacity, tint alpha, spatial fading and lifetime fading. Replacement still respects coverage. Color supports replacement/alpha blending, multiplication and addition; roughness supports replacement, addition and multiplication. Coating is separate from coverage and ordinary decals preserve receiver opacity.

Corrections run on the accumulated base color before the decal's color layer: tint and exposure, hue, saturation, then contrast around linear 0.18. Neutral settings preserve color exactly. The Color Correction preset needs no assigned texture. Other presets are Texture Sticker, Paper Label, Paint, Dirt, Wet Patch, Normal Detail and Emissive Mark. Applying a preset preserves assigned textures.

## Runtime

Native code can use `Material::CreateDecal()` and `entity.AddComponent<DecalComponent>()`. Managed scripts use `Crowny.DecalComponent` through both runtime backends:

```csharp
DecalComponent label = entity.AddComponent<DecalComponent>();
label.Projection = DecalProjection.Cylinder;
label.BottomRadius = 0.5f;
label.TopRadius = 0.4f;
label.Height = 0.8f;
label.ShellThickness = 0.03f;
label.Arc = 180;
label.Opacity = 1;
label.Material = labelMaterial;
label.TargetMode = DecalTargetMode.Entity;
label.Target = bottle.uuid;
label.Lifetime = 10;
label.FadeOut = 2;
label.RestartLifetime();
```

Lifetime zero is persistent. Otherwise fade-out begins at Lifetime and expiry occurs at Lifetime + FadeOut. The clock advances with scene simulation. RestartLifetime enables the decal and resets its age; StopLifetime freezes its current age. Expiry disables the component unless DestroyOwnerOnExpiry is explicitly enabled. Playback age is not serialized.

A projector parented to a moving bottle follows its transform. World projectors sample animated surfaces at their current positions. Attachment to deforming skin is not implemented.

## Rendering interface

Built-in Standard, Toon and Unlit surface shaders evaluate `CrownyDecals.glslinc` after texture sampling and alpha-cutout rejection, before lighting or G-buffer encoding. The evaluator takes world position, geometric normal, receiver ID and world-position derivatives calculated before the divergent decal loop. Projection gradients drive explicit texture sampling; cylinder derivatives avoid discontinuities at the angular seam.

Decals are ordered by signed SortOrder and then entity UUID. Higher sort orders apply later. Cluster lists hold up to 64 candidates; overflow uses the complete sorted visible list with exact volume rejection. Lists do not use opaque depth pruning. Vulkan uses the shared bindless texture table and a compute builder. Production Vulkan rendering does not build a CPU reference grid; validation requests build it explicitly. Completed GPU counters are read asynchronously per camera, with the originating frame number. OpenGL uses CPU lists and an atlas array with separate guarded regions for source mips.

Each scene renderer extracts a generation-checked DecalWorld journal, including offscreen edits and removals. Render-thread world tables retain stable decal slots and shared material slots across cameras. Per-view lists contain slot/generation references in blending order. Material reloads preserve slot identity; deleted and reused slots reject stale generations. Changes upload contiguous dirty ranges. Recorded Vulkan commands retain the previous buffer allocation and texture descriptors until their work retires.

Decal grid statistics and GPU timestamp durations arrive asynchronously with their camera and frame identity. The viewport statistics tooltip shows occupancy, maximum candidates, upload bytes and grid GPU time. These timestamps measure list construction; decal surface evaluation runs inside the receiver shader and has no separate pass duration.

Custom shaders need the explicit decal shader interface and bindings. The inspector reports when a surface shader lacks that interface. Arbitrary custom surface shaders are not automatically converted.

The coating path promotes fully coated transparent receiver samples to opaque depth and color rendering, then removes those samples from transparent rendering. Partially covered edges remain transparent. Standard, Toon and Unlit use shared surface lighting/tone-mapping operations where applicable. OpenGL blends tone-mapped transparent fragments in sorted order; Vulkan blends HDR fragments with weighted OIT. Uncovered glass therefore differs between backends, while opaque label cores have independent strict comparisons. This does not implement refraction, transmission, projector visibility tracing, mesh displacement or independent decal shadows.

## Content packaging

Scene and prefab sources contribute their decal material UUIDs to the build content database, including disabled decals that scripts may enable. Material sources contribute all assigned texture UUIDs, deduplicated across channels. Entity targets are not asset dependencies. Generated default textures need no content records. Missing or excluded dependencies fail content validation. The editor still includes other imported assets through its existing broad build-selection policy; this change does not introduce general scene dependency pruning.

## Validation status

The final full Release native executable on Windows passed all 970 tests and 113,884 assertions with the published ABI-20 CoreCLR package. The separate decal run passed 28 tests and 2,847 assertions; both Mono and CoreCLR execute. Logs are `artifacts/decals/final-native-with-coreclr.log` and `final-managed-decals.log`.

The full rendering lane passed 39 Vulkan cases, 39 OpenGL cases and all 39 cross-backend comparisons against reviewed references. Both Vulkan Forward+ and Deferred+ are exercised. Cases include Standard, Toon, Unlit, curved stickers, tapered partial/full wraps, coating with transparent geometry behind it, independent channel effects, camera-inside volumes and ordered overlap. The log is `artifacts/decals/final-acceptance.log`. `Scripts/run-render-tests.ps1` is absent in this checkout; `Scripts/crowny.bat render-tests` runs the supported equivalent.

Additional checks compare the first changed TAA frame against the current surface after movement, lifetime fading, material edits and removal. Both Vulkan paths passed. GPU preflights verify sparse table reuse, one-record dirty uploads, material generations, preserved recorded-buffer contents, cluster overflow, camera isolation and asynchronous timestamps. A two-bone receiver deforms through a fixed world projector and moves behind its thin projection band; both backends verify that the decal uses current skinned positions and does not leak behind the band. A 256-times repeated cylinder texture converges to its authored coarse mip color, including the front-facing angular seam and the OpenGL atlas. Content tests resolve scene and prefab roots, deduplicate channel textures, read their payload from the resulting pack, and reject excluded or missing dependencies.

Header checking and managed parity passed; the shared ABI has 542 host functions. All 65 Python tooling tests passed. Fresh-checkout setup now applies the GLFW ownership patch idempotently and reports conflicts without overwriting local edits.

## Windows authoring verification

Switching between editor windows reproduced an access violation in GLFW's Win32 event loop. The local dependency fix checks the active window against GLFW's own window list before reading its key state. Its process-isolated regression passed six assertions. The patch and fresh-checkout instructions are in `Scripts/patches/`. The rebuilt editor opened the separate `artifacts/decals/editor-project/Assets/BottleWrapGizmo.cwscene` fixture and survived subsequent window switching. The user's existing `DecalGizmos.cwscene` was preserved.

The Windows drag trace also reproduced a dropped final cursor sample: a top-radius drag began, received no held-frame movement, then released with a 0.4617-unit delta that was never applied. Bounds interaction now applies the release position before committing. An ImGui frame regression covers both box and cylinder release movement, one-step undo and Escape cancellation of dimensions, offset and density UVs. Its first build compiled successfully but failed in MSVC's 32-bit linker with heap exhaustion and a PDB RPC error; the 64-bit retry passed.

Manual Windows/Vulkan verification on 2026-09-12 confirmed the final interaction. A cylinder top-radius drag changed 0.510 to 0.780 without changing the bottom radius, height or entity scale; one undo restored 0.510. A box right-face drag changed width from 1.000 to 1.292 and local X offset from 0 to 0.146, keeping the left face fixed. One undo restored both values. Captures are `artifacts/decals/cylinder-resize-verified.jpg`, `artifacts/decals/cylinder-wrap-undo-verified.jpg` and `artifacts/decals/box-resize-verified.jpg`. This isolated authoring fixture has no decal material assigned. It demonstrates handles, not rendering acceptance. The fixture and grid setting were restored and the editor exited cleanly.

The subsequent full Debug native run aborted after Mono initialization with MSVC's `unlock of unowned mutex` diagnostic. On 2026-09-12, the saved debugger stack and a single-worker regression traced this to TaskSystem releasing the final canceled-task reference while its state mutex was locked. Unlocking before releasing the task passed all 20 threading tests and 162 assertions. The subsequent full Debug run exited successfully: 950 tests passed, five were skipped, and all 113,502 assertions passed. Its log is `artifacts/decals/native-after-task-fix.log`; it predates the later cooked-shader location restoration.

## Reusable showcase

`artifacts/decals/showcase-verified` contains 25 exported scenes with reusable `.cwmat` files, source BMP textures, tapered OBJ receivers, stable matching metadata and a camera. Open it as an editor project and choose a `decal-*.cwscene`. The full-wrap example passed the actual editor import/capture path; `artifacts/decals/showcase-wrap-verified.bmp` shows the imported texture wrapping around its tapered receiver. A fresh capture with editor handles is pending because Windows became locked; the earlier box/cylinder resize captures remain available.

To regenerate into an empty directory after building RenderTests:

```powershell
bin/Release-windows-x86_64/Crowny-RenderTests/Crowny-RenderTests.exe --backend vulkan --filter decal- --export-decal-scenes artifacts/decals/new-showcase
```

The exporter refuses to overwrite an existing project. It uses normal source imports, computes OBJ tangents, and preserves assigned texture UUIDs. The exported scenes show each case's initial appearance; the render harness additionally exercises its runtime changes and asserts their results.

## Measurements

Release, Core i7-1355U / Intel Iris Xe, 256 x 256, TAA/Bloom/GTAO disabled, three warmup frames and twelve measured frames:

| Scenario | Visible | Overflow clusters | Vulkan frame ms | Vulkan grid GPU ms | OpenGL frame ms |
|---|---:|---:|---:|---:|---:|
| No decals | 0 | 0 | 3.965 | skipped | 1.024 |
| Distributed decals | 1,000 | 52 | 14.200 | 0.082 | 36.177 |
| Deliberate overlap | 65 | 768 | 4.428 | 0.371 | 5.987 |

Frame measurements include CPU submission and GPU completion/readback latency. Grid timestamps isolate Vulkan list construction. OpenGL builds its lists on the CPU, so no grid GPU duration is reported. These are shared-desktop measurements, including editor/import activity during some runs; they are not an isolated frame-time budget or a promise for other hardware. JSON results are `artifacts/decals/benchmark-vulkan.json` and `benchmark-opengl.json`. Earlier measurements before the Vulkan synchronization fixes are obsolete.

OpenGL sorted transparency and Vulkan weighted OIT have documented differences outside opaque coating cores; strict core tests establish label occlusion without claiming identical uncovered glass.
