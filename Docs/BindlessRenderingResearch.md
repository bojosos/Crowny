# Bindless rendering research

Reviewed against Khronos documentation on 2026-09-05. This section covers API capabilities; it does not establish which capabilities Crowny currently enables.

## Textures and material data

Vulkan descriptor indexing lets shaders select resources from descriptor arrays. A renderer can keep a large texture table bound and pass indices instead of rebinding each material's textures. Khronos calls this bindless. It reduces resource binding work, but does not by itself combine draw calls or remove pipeline changes. [Khronos descriptor indexing sample](https://docs.vulkan.org/samples/latest/samples/extensions/descriptor_indexing/README.html)

Material tables are a separate, compatible design choice. Store material records in a storage buffer, with scalar parameters and indices into the texture table. Objects then carry a material index. This is an application design inferred from ordinary indexed storage-buffer access, not a distinct Vulkan material feature. Khronos demonstrates a scene-wide buffer indexed by instance ID and bound once; the same mechanism can hold material records. Buffer-array access does not require an array of buffer descriptors. The sample also shows that shader data access strategies have hardware-dependent costs, so reduced CPU binding work does not establish a GPU speedup. [Khronos constant data sample](https://docs.vulkan.org/samples/latest/samples/performance/constant_data/README.html)

## Vulkan requirements

Descriptor indexing entered core Vulkan 1.2, but the implementation must support and the application must enable the individual features its design uses. Non-uniform texture selection needs `shaderSampledImageArrayNonUniformIndexing` and the shader's `nonuniformEXT` annotation. Runtime descriptor arrays, partially populated bindings, variable allocation sizes, and updates after binding are separate capabilities. A fixed-size, fully populated table updated at safe points does not need every option. [Vulkan descriptor indexing guide](https://docs.vulkan.org/guide/latest/extensions/VK_EXT_descriptor_indexing.html), [descriptor indexing features](https://docs.vulkan.org/refpages/latest/refpages/source/VkPhysicalDeviceDescriptorIndexingFeatures.html)

Descriptor arrays can also hold storage buffers and storage images, subject to the corresponding feature bits. For more general buffer access, Vulkan buffer device address provides shader-visible buffer pointers. It requires its own enabled feature, buffer usage flag, and allocation flag. This is a later option for geometry and other buffer data, not a prerequisite for a single material storage buffer. [Descriptor indexing features](https://docs.vulkan.org/refpages/latest/refpages/source/VkPhysicalDeviceDescriptorIndexingFeatures.html), [buffer device address guide](https://docs.vulkan.org/guide/latest/buffer_device_address.html)

Table sizing must respect the device's descriptor limits. Update-after-bind layouts and pools use their own limits and require matching binding, layout, and pool flags. [Descriptor indexing guide](https://docs.vulkan.org/guide/latest/extensions/VK_EXT_descriptor_indexing.html), [descriptor indexing properties](https://registry.khronos.org/vulkan/specs/latest/man/html/VkPhysicalDeviceDescriptorIndexingPropertiesEXT.html)

Update-after-bind is not permission to overwrite descriptors that pending work uses. Partially bound means unused entries may be invalid; entries actually accessed must be valid. The update-unused-while-pending flag permits changes only to entries pending commands do not use. [Descriptor binding flags](https://docs.vulkan.org/refpages/latest/refpages/source/VkDescriptorBindingFlagBits.html)

Referenced images must survive until all submitted uses complete. A practical design should defer resource destruction and slot reuse, or maintain tables per frame and update them after that frame's fence signals. These are implementation choices derived from resource lifetime rules and Khronos's frame-safe update example. [Image destruction rules](https://docs.vulkan.org/refpages/latest/refpages/source/vkDestroyImage.html), [stable descriptor updates](https://docs.vulkan.org/tutorial/latest/Building_a_Simple_Engine/Advanced_Topics/Descriptor_Indexing_UpdateAfterBind.html)

## OpenGL constraints

`GL_ARB_bindless_texture` exposes 64-bit texture or texture/sampler handles instead of texture units. Handles must be resident before shaders use them. Extracting a handle prevents subsequent texture/sampler parameter changes and texture storage redefinition, though texel contents can still change. The extension alone requires dynamically uniform sampler selection; arbitrary per-invocation selection needs additional support such as `NV_gpu_shader5`. Draw ID can support uniform selection within each constituent draw of a multidraw. Check extension support explicitly and preserve a texture-unit fallback if OpenGL portability matters. [ARB_bindless_texture specification](https://registry.khronos.org/OpenGL/extensions/ARB/ARB_bindless_texture.txt)

This means a shared material-index abstraction can map to Vulkan descriptor indices and OpenGL texture handles, but identical unrestricted shader indexing behavior cannot be assumed across the two APIs. That is an architectural inference from the differing API requirements above.

## Initial source assessment

Inspected the working tree on 2026-09-05, including existing uncommitted changes. This was a source inspection, not a runtime capture or performance measurement.

Crowny already implements bindless textures and indexed material records in the newer scene renderer. The fragment shader reads `materials[inputData.materialIndex]`, then samples `cwTextures[nonuniformEXT(material.textureIndices0.x)]`. The texture array is a descriptor array of independent images, not layers of one texture. [ForwardPlusStandard.glsl](../Crowny-Editor/Resources/Shaders/ForwardPlusStandard.glsl#L179)

`GpuMaterialData` stores material parameters and texture indices in a shared 256-byte record. Standard, Unlit, and Toon use this representation; the classifier also recognizes forward-only and unsupported material routes. [GpuMaterial.h](../Crowny/Source/Crowny/Renderer/GpuMaterial.h#L14)

`SceneRenderer::BindMaterialTable` binds the material buffer at set 1, binding 0, and the texture array at set 1, binding 1. It updates the array when the scene texture version changes. Shadow, masked depth, Forward+, Deferred+, toon, and standard transparency passes call this helper. [SceneRenderer.cpp](../Crowny/Source/Crowny/Scene/SceneRenderer.cpp#L1055)

Vulkan enables supported descriptor-indexing feature bits and creates descriptor-array layouts and update-after-bind pools. Its runtime texture-array allocation currently caps capacity at the smaller of the reported device limit and 4096. [VulkanDevice.cpp](../Crowny/Source/Platform/Vulkan/VulkanDevice.cpp#L279), [VulkanUniformParamInfo.cpp](../Crowny/Source/Platform/Vulkan/VulkanUniformParamInfo.cpp#L59), [VulkanDescriptorPool.cpp](../Crowny/Source/Platform/Vulkan/VulkanDescriptorPool.cpp#L250)

Draw bin keys include pipeline, geometry heap, material template, phase, and alpha mode, while individual material indices travel with visible instances. This allows compatible material instances to share indirect submission. Geometry still uses vertex/index buffer bindings per bin. Instance, light, mesh, and material data have shared GPU buffers; this is not a universal array of arbitrary buffer descriptors. [GpuDrivenDraw.h](../Crowny/Source/Crowny/Renderer/GpuDrivenDraw.h#L20), [GpuScene.h](../Crowny/Source/Crowny/Renderer/GpuScene.h#L80), [SceneRenderer.cpp](../Crowny/Source/Crowny/Scene/SceneRenderer.cpp#L1093)

OpenGL binds textures to texture units. No engine calls to ARB bindless texture-handle or residency functions were found. The compatibility renderer and forward-only material path still set per-material pipeline/uniform state; 2D rendering also binds its batch textures explicitly. [OpenGLUniformParams.cpp](../Crowny/Source/Platform/OpenGL/OpenGLUniformParams.cpp#L34), [ForwardRenderer.cpp](../Crowny/Source/Crowny/Renderer/ForwardRenderer.cpp#L331), [Renderer2D.cpp](../Crowny/Source/Crowny/Renderer/Renderer2D.cpp#L552)

## Proposed changes from the initial assessment

Bindless is a good fit for Crowny's existing material tables and indirect draw bins. The practical next step is improving the existing path, with performance claims deferred until profiling.

1. Keep texture slots persistent and update changed material records and descriptors incrementally. Material resource changes currently trigger `RebuildMaterialTable`, which reconstructs the texture registry and uploads the material table. The standalone `BindlessResourceTable` already supplies generation checks and deferred slot reuse, but rebuilding the scene registry does not make full use of that design. [GpuScene.cpp](../Crowny/Source/Crowny/Renderer/GpuScene.cpp#L279), [GpuScene.cpp](../Crowny/Source/Crowny/Renderer/GpuScene.cpp#L971), [BindlessResourceTable.h](../Crowny/Source/Crowny/Renderer/BindlessResourceTable.h#L53)
2. Unify scene allocation with actual descriptor capacity and define overflow behavior. The scene sizes its table from material count, independently of the Vulkan runtime-array cap. Vulkan clamps descriptor population to its binding capacity, so this boundary needs validation before treating the table as suitable for arbitrarily large scenes. [GpuScene.cpp](../Crowny/Source/Crowny/Renderer/GpuScene.cpp#L981), [VulkanUniformParams.cpp](../Crowny/Source/Platform/Vulkan/VulkanUniformParams.cpp#L432)
3. Profile resource updates and descriptor preparation alongside CPU submission and GPU pass time. Extend bindless to geometry or other buffers only if remaining binding work is material. Shared material storage already avoids a separate buffer binding per material. Environment maps, shadow resources, and other pass inputs currently use explicit bindings; making these bindless is an optional design change. [SceneRenderer.cpp](../Crowny/Source/Crowny/Scene/SceneRenderer.cpp#L1030)

Retain the OpenGL compatibility route. A Vulkan implementation does not establish equivalent OpenGL support, and material shader or render-state differences still require compatible pipeline groups.

## Implementation follow-up

The update and capacity changes above are now implemented in the working tree:

- Material changes repack only affected records, merge adjacent dirty ranges, and upload those ranges. Growing the GPU buffer still initializes the complete table. An unchanged packed record produces no upload.
- Texture slots persist across material changes. Reference counts cover shared textures and all eight material texture fields, including Toon resources. A batch retains still-used textures before releasing unused slots, so replacing a texture can succeed even at capacity.
- Slot zero remains the missing-texture fallback. Capacity exhaustion produces a warning and an overflow-material count, never an out-of-range shader index. Materials that could not allocate textures are retried on later material resource changes.
- Scene allocation and Vulkan runtime-array layouts use `RenderCapabilities::GetBindlessTextureCapacity`. Vulkan derives its budget from sampled-image, sampler, and per-stage limits, using update-after-bind limits when applicable. It reserves 256 slots for the decal array plus 32 descriptors/resources for other pass inputs and retains the 4096-slot engine ceiling. Both descriptor pool types can accommodate a full table. Direct oversized Vulkan array assignments throw before mutating stored resources.
- Each cached Vulkan descriptor set records its own image descriptors. Writes cover only changed contiguous array ranges; a newly allocated set receives its full initial contents. Reusing an older set compares against that set's own contents. Sets in use remain untouched. CPU texture slot reuse relies on this descriptor-set isolation and the existing buffer/resource synchronization, rather than assuming that a fixed number of frames has completed.

Regression coverage includes stable/shared texture indices, changed material records, capacity exhaustion and recovery, replacement at capacity, reset/fallback behavior, device limits, and descriptor updates for stale cached sets. The render harness also reads back a partial GPU material upload and checks neighboring records and the unchanged-frame upload count.

OpenGL keeps the CPU material table at the engine ceiling because its compatibility renderer binds material textures conventionally; Vulkan's descriptor budget does not apply to that table.

### Initial validation, 2026-09-06

Scoped native formatting and `git diff --check` passed. The focused unit-test build failed in concurrently edited scene-renderer code (`DecalRenderer` and `RenderableObject::ObjectID` declarations). A full `Scripts\crowny.bat test` retry is queued behind other builds in the shared checkout; the new unit tests have not yet run.

The available render-test executable passed 8 of 9 cases on both Vulkan and OpenGL (Intel Iris Xe). Both failed `primitive-lit-sphere` at the viewport material-drop object-ID picking check. The new Vulkan partial-material-upload readback check, which runs earlier in that case, passed. These runs include the incremental-update implementation but precede the final decal-budget reservation and OpenGL bookkeeping correction. No reference images were updated. Results are under `artifacts/bindless-updates/{vulkan,opengl}/summary.json`.

### Completed bindless validation, 2026-09-08

Rechecked the newer shared build and test artifacts against the source and compiled test objects. They cover the final 288-resource reservation, the OpenGL CPU-table correction, and the partial descriptor/material updates. `GpuSceneTests.cpp`, `VulkanDescriptorTests.cpp`, and `RenderCapabilitiesTests.cpp` were compiled on September 8 before the full native test run; their sources include the final capacity expectations. No additional engine changes or duplicate builds were needed for this validation.

- The supported Release All build completed successfully: `artifacts/material-completion-build.log`.
- The full native suite passed 950 tests, with one skipped and all 113374 assertions passing: `artifacts/bistro/cold-validation-7/native.log`. This includes the bindless capacity, overflow/recovery, stable-slot, and descriptor-range regressions.
- `primitive-lit-sphere` now passes on Vulkan and OpenGL, including `CheckGpuMaterialUpdates` and the previously failing material/object-picking checks: `artifacts/material-final-focused-vulkan.log` and `artifacts/material-final-suite-opengl.log`. Vulkan's focused `toon-silhouette` run also passes: `artifacts/material-final-toon-vulkan.log`.
- The broader renderer suite is not wholly green. The OpenGL run passed its 12 non-decal cases but lacked reference images for six new decal cases. The earlier Vulkan suite reported decal-list mismatches, while its non-decal checks have passing coverage across the suite and subsequent focused runs. The active decal task owns those remaining failures; they are not being treated as passing bindless validation or having their references updated here.

The previously unfinished bindless build/test validation is complete. This does not establish a measured performance improvement or validate the separate decal work.
