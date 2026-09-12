# Decals

Decals are ordinary entities with a `DecalComponent` and a reusable `.cwmat` material in the decal domain. The component controls placement, projection, receiver filtering and instance appearance. Editing instance tint, opacity or UVs does not change the shared material.

This implementation is still undergoing integration and rendering acceptance. The validation notes below distinguish checks already run from outstanding work.

## Authoring

Create a Box Decal or Cylinder Decal from the hierarchy, or add a Decal component to an existing entity. Assign a decal material in the component inspector. The texture picker can create a reusable decal material from an existing texture asset. Surface material slots reject loaded decal materials.

Use the entity transform to position, rotate and parent the projector. Use the component dimensions or viewport bounds handles to resize its projection volume. Parent scale affects the volume. Bounds resizing preserves the entity transform.

Box decals map local XY to UV and project along local negative Z. Width, height and depth are full dimensions. Make the depth small to avoid affecting nearby surfaces. Facing rejection uses the receiver's geometric normal.

Cylinders use local Y as their axis. Bottom and top radii define a tapered shell; equal radii produce a straight cylinder. Height is the full axial extent and shell thickness is the full radial band. Arc and seam rotation are in degrees. A 360-degree wrap has no angular edge feather. Nonuniform parent scale produces an elliptical cross section. This projection does not unwrap a mesh's UVs.

The selected cylinder draws a translucent label sleeve with top and bottom edges, faint inner and outer shell guides, and a gold seam. In Bounds mode, square handles resize the two radii, height edges and shell thickness. Gold diamond handles edit wrap angle and seam rotation. Those two handles sit at different heights so they remain separate on a full wrap. Hover a handle to see its name. Resize the component around the bottle with its entity scale at one; use different end radii when the bottle tapers.

Negative UV scale flips artwork. UV rotation is in degrees. Resizing stretches textures by default; Preserve Texel Density adjusts UV scale. Box face and cylinder cap drags keep the opposite face fixed. Shift resizes about the center. A drag commits one undo action containing dimensions, offset and UV settings; Escape restores all of them.

Receiver layers are independent of camera visibility layers. Mesh and procedural receivers have Receive Decals and Decal Layers settings. Entity and Subtree targeting also require matching layers. Deleting an explicit target disables that decal's contribution until it is reassigned. Duplication and prefab instantiation remap references within the copied hierarchy.

## Materials

The decal material inspector exposes independent switches and strengths for color, normals, roughness, metallic, ambient occlusion, emission, corrections and coating. Surface materials expose a corresponding response mask. Disabled channels preserve the sampled receiver attribute.

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

Decals are ordered by signed SortOrder and then entity UUID. Higher sort orders apply later. Cluster lists hold up to 64 candidates; overflow uses the complete sorted visible list with exact volume rejection. Lists do not use opaque depth pruning. Vulkan uses the shared bindless texture table and a compute builder. OpenGL uses CPU lists and an atlas array with separate guarded regions for source mips.

Custom shaders need the explicit decal shader interface and bindings. The inspector reports when a surface shader lacks that interface. Arbitrary custom surface shaders are not automatically converted.

The coating path promotes fully coated transparent receiver samples to opaque depth and color rendering, then removes those samples from transparent rendering. Partially covered edges remain transparent. This does not implement refraction, transmission, projector visibility tracing, mesh displacement or independent decal shadows.

## Validation status

On 2026-09-08, the resumed Debug `[decals]` run passed 24 tests and 2,761 assertions, with one CoreCLR package-dependent test skipped. This includes projection, blending, prefab remapping, domain assignment, snapshot extraction, cylinder handle placement, viewport release/cancel input, undo and the Mono runtime probe. The CoreCLR probe also passed separately with 21 assertions using the published ABI-20 package. Header checking and managed binding parity passed with 542 shared host functions.

Switching between editor windows reproduced an access violation in GLFW's Win32 event loop. The local dependency fix checks the active window against GLFW's own window list before reading its key state. Its process-isolated regression passed six assertions. The patch and fresh-checkout instructions are in `Scripts/patches/`. The rebuilt editor opened the separate `artifacts/decals/editor-project/Assets/BottleWrapGizmo.cwscene` fixture and survived subsequent window switching. The user's existing `DecalGizmos.cwscene` was preserved.

The Windows drag trace also reproduced a dropped final cursor sample: a top-radius drag began, received no held-frame movement, then released with a 0.4617-unit delta that was never applied. Bounds interaction now applies the release position before committing. An ImGui frame regression covers both box and cylinder release movement, one-step undo and Escape cancellation of dimensions, offset and density UVs. Its first build compiled successfully but failed in MSVC's 32-bit linker with heap exhaustion and a PDB RPC error; the 64-bit retry passed.

Manual Windows/Vulkan verification on 2026-09-12 confirmed the final interaction. A cylinder top-radius drag changed 0.510 to 0.780 without changing the bottom radius, height or entity scale; one undo restored 0.510. A box right-face drag changed width from 1.000 to 1.292 and local X offset from 0 to 0.146, keeping the left face fixed. One undo restored both values. Captures are `artifacts/decals/cylinder-resize-verified.jpg`, `artifacts/decals/cylinder-wrap-undo-verified.jpg` and `artifacts/decals/box-resize-verified.jpg`. This isolated authoring fixture has no decal material assigned. It demonstrates handles, not rendering acceptance. The fixture and grid setting were restored and the editor exited cleanly.

Rendering acceptance, backend comparisons, editor captures and benchmark measurements are not yet complete. The render harness contains curved box, tapered partial/full wrap, glass coating and stress scenarios. No frame-time claims should be inferred from their presence.

The subsequent full Debug native run aborted after Mono initialization with MSVC's `unlock of unowned mutex` diagnostic. On 2026-09-12, the saved debugger stack and a single-worker regression traced this to TaskSystem releasing the final canceled-task reference while its state mutex was locked. Unlocking before releasing the task passed all 20 threading tests and 162 assertions. The subsequent full Debug run exited successfully: 950 tests passed, five were skipped, and all 113,502 assertions passed. Its log is `artifacts/decals/native-after-task-fix.log`; it predates the later cooked-shader location restoration.

The recorded-buffer-discard render preflight exposed a stale viewport-derived Vulkan scissor rectangle. Invalidating that rectangle when the viewport changes makes both draws pass. Declaring decal grid output buffers as GPU-writable fixed the later-frame list disagreement. All six Vulkan decal scenarios then passed their internal checks, including receiver filtering, list agreement and opaque coating. After fixing integer G-buffer sampling and restoring cooked vertex-input locations from SPIR-V, the 2026-09-12 Vulkan rerun completed without validation warnings. Its six actual images were inspected and added as references, including a front-facing full-wrap seam. The log is `artifacts/decals/final-vulkan.log`; its reported failures were the missing references at the time of that run. The Debug editor build also passed with these renderer and scheduler fixes. OpenGL comparison and the new masked-correction render cases remain pending.

Remaining integration work includes persistent generation-backed decal/material GPU records connected to DecalWorld, removing the production CPU reference-list build from Vulkan, complete compatibility coating parity, asset dependency closure, temporal acceptance scenes and the remaining showcase coverage. Current per-view decal/material buffers use dirty comparisons; they are not the persistent world tables specified in the implementation plan.

The subsequent full Release native run stopped with a player-template long-path validation failure and a crash in the decal snapshot fixture. An isolated debugger run traced the fixture crash to constructing a material before starting `AssetListenerManager`. The fixture now owns the listener manager when needed and destroys it after its assets; the rebuilt rerun is queued. The remaining Release decal tests passed independently: 24 tests and 2,756 assertions, with the package-dependent CoreCLR probe skipped. This includes the cooked-shader location regression. Logs are `artifacts/decals/native-release-final.log`, `artifacts/decals/release-snapshot-staged-stack.log` and `artifacts/decals/release-decals-excluding-fixture.log`.

Six additional render cases cover masked corrections, roughness-only wetness and normal-only detail in Forward+ and Deferred+. The correction cases also compare neutral settings against the original receiver image. These new cases are written but await the queued rebuild and rendering run. Current editor player builds include all imported assets as roots, so decal textures are packaged through that broad inclusion policy; selected-root dependency closure is still absent.

After the shared build window completed, the Release decal rerun passed 25 tests and 2,768 assertions, with the CoreCLR package-dependent test skipped. This verifies the snapshot fixture startup fix. The latest full native run from standalone validation still aborts in `PropertyLayoutTests.cpp`, in the asset-drop cursor test; it is not a passing full-suite result.

All 12 Release Vulkan decal scenarios passed their internal checks. The six channel images were inspected and added as references. OpenGL also passed the internal checks, including neutral masked corrections and coating occlusion, but the two opaque tapered-cylinder images failed comparison against Vulkan. The legacy OpenGL PBR shader applies a 0.001 direct-light scale and a different tone mapper; its images are much darker. The tapered images also show an orientation mismatch. The shared primitive tolerance permits some other decal images to pass despite these visible differences, so a passing comparison alone does not establish rendering parity. Logs and images are under `artifacts/decals/release-validation/`, with logs `release-vulkan.log` and `release-opengl.log` in `artifacts/decals/`.

Release measurements on Intel Iris Xe at 256 x 256, using three warmup frames and 12 measured frames per scenario:

| Scenario | Visible decals | Overflow clusters | Vulkan ms | OpenGL ms |
|---|---:|---:|---:|---:|
| No decals | 0 | 0 | 3.379 | 1.110 |
| Distributed decals | 1,000 | 52 | 9.877 | 20.399 |
| Deliberate overlap | 65 | 768 | 12.003 | 3.834 |

These are CPU submission plus GPU completion latencies on the shared desktop, not isolated decal GPU pass timings or a frame-time budget. The distributed scene still overflows 52 clusters, and the Vulkan implementation still builds a CPU reference grid. Saved measurements are `artifacts/decals/benchmark-release-acceptance-vulkan.json` and `artifacts/decals/benchmark-release-acceptance-opengl.json`.

The final full renderer invocation was `Scripts/crowny.bat render-tests --configuration Release --no-build --artifact-root artifacts/decals/full-render-validation`. Vulkan passed all 26 cases. OpenGL passed 24 and failed the two opaque tapered-cylinder comparisons above. `Scripts/run-render-tests.ps1` is absent in this checkout; the supported CLI ran the equivalent renderer lane. The complete log is `artifacts/decals/full-render-validation.log`.
