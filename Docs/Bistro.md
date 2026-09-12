# Bistro scene

The local Bistro project is `C:\dev\Projects\New Project123`. Its exterior scene is
`Assets\Bistro\Bistro.cwscene`. The source is Amazon Lumberyard Bistro v5.2, downloaded
in the Claude session `32e1a7d6-0823-4bd1-a89e-433ef9b26878`.

## Assets and import settings

- The original archive and extracted sources are in `C:\dev\Projects\BistroSource`.
- The project uses 512-pixel PNG conversions of the source DDS textures.
- The 203 BC5 normal maps require reconstructing positive Z from their red and
  green channels. A direct Pillow conversion leaves blue at zero and reverses
  the normal direction. `Scripts/convert-bistro-textures.py` handles this step.
- `BistroExterior.fbx` has stable asset ID `83da01d3-c054-4f60-9023-042f48a48d98`.
- Import materials and generate meshlets. Calculate tangents. Keep scale factor 1.
- Disable collision, animation, bones, morphs, and LOD generation for this scene.
- Keep the scene entity scale at `[1, 1, 1]`. The FBX node transforms already convert
  its coordinates to meters. Applying another 0.01 scale shrinks the scene twice.
- Enable Generate Prefab to retain the imported sun, camera, and source transforms.
  The local scene uses the generated mesh/material references and source hierarchy.

The interior FBXs are held outside the project. They have not been imported here.
The project and generated cache are local assets and are not committed to this repository.

The exterior cache contains 3,206,551 vertices, 8,496,360 indices, 1,591 submeshes,
and 56,750 meshlets. Repairing degenerate tangent bases recovered 48 mesh groups
that the old importer discarded. Material slots must follow the emitted FBX node instances,
not the source mesh array. The source array has 1,591 entries and a different order.
After reimport, the scene's material list must match the subasset list in the FBX metadata.

Imported material caches now retain texture asset UUIDs and restore default white
and normal textures before applying saved overrides. Reimport older material caches
to record their texture references. Buffer-layout cache IDs are assigned at runtime
when loading assets; IDs stored by separate imports can collide.

To repair the existing local normal maps, with Pillow installed:

```powershell
python Scripts/convert-bistro-textures.py `
  'C:\dev\Projects\BistroSource\Bistro_v5_2\Textures' `
  'C:\dev\Projects\New Project123\Assets\Bistro\Textures' `
  --existing-only --normals-only `
  --backup-dir 'C:\dev\Projects\BistroSource\original-converted-normals'
```

Reimport the exterior FBX afterward to refresh its embedded texture subassets.

## Import performance

The original session stalled for over an hour. Two allocation patterns made this
scene expensive: copying scene-wide vertex arrays for each submesh, and growing
serialization buffers by exactly the size of every small field. Mesh processing
now compacts each submesh and lets output vectors grow geometrically.
`MemoryDataStream` also grows its allocation geometrically while preserving the
logical stream size.

The exterior vertex buffer is about 205 MB, so it uses the renderer's per-mesh
allocation path instead of a shared 64 MB geometry page.

A controlled Windows/Vulkan comparison on September 5, 2026 measured the same
4,877 output assets and 717,477,620 logical bytes with the same executable:

| Path | Total |
| --- | ---: |
| Uncached textures, original dependency loop, sequential writes | 163.107 s |
| Reimport 1 | 15.884 s |
| Reimport 2 | 12.478 s |
| Reimport 3 | 10.486 s |

Median reimport improved **13.07×**. These runs used scale 1, calculated tangents,
meshlets, materials and prefab output, with animation, morph and collision import
disabled. The exact settings are retained in
`artifacts/bistro/before-final-reimport.meta`. Reusable texture cooks and existing
asset outputs were present for the optimized runs. A first import must still cook
new textures; this is not a claim of 13× faster cold import.

The timings include import, GPU asset initialization, dependency resolution and
asset publication. Editor metadata/index commit, viewport refresh, executable
startup and output-size verification are excluded. Reports and the comparison
check are in `artifacts/bistro/import-comparison-final.json` and the corresponding
`import-complete-*.json` files. Cached scene loading was separately measured at
16.54 seconds before these reimport changes.

## Lighting and presentation

The source exterior FBX contains one directional light and one camera. Street lamps
and string lights are geometry with emissive materials. The supplied `.pyscene`
references `san_giuseppe_bridge_4k.hdr`; FBX alone does not carry that environment.
The local scene uses a cooked environment subasset from `Assets/Bistro/Lighting`.
The imported sun's color and direction are retained. Its scene intensity is reduced
to 3 for Crowny's current preview exposure; the source prefab retains 110.1.
The user's latest editor camera is preserved. GPU scene projections now use
reverse depth, while compatibility rendering retains ordinary depth tests.
Vulkan face winding matches its positive-height viewport. The environment's base
and prefiltered cubemaps use RGBA32F, avoiding the reflection artifacts observed
with the previous half-float path on the tested Intel driver.

See [Model import](ModelImport.md) for the shared importer and reimport behavior.

## September 5 validation record

Logs and local investigation scripts are under `artifacts/bistro/`.
The final native run passed all 89,632 assertions: 873 cases passed and one was
skipped. The process-isolated scripting cases also passed. The render harness
passed nine Vulkan cases, nine OpenGL cases and nine backend comparisons.

Manual Windows/Vulkan validation reopened Bistro with coherent surface occlusion
and the saved camera. An editor reimport retained all 4,877 primary/dependent UUIDs
in order. The scene saved during that session was retained. During that run the
editor settings changed to scale 0.8 with morph import enabled; those settings
were retained.
That interactive run is not part of the controlled timing comparison. See
`artifacts/bistro/editor-reimport-verification.json` and `final-editor.log`.

## September 8 cold-import follow-up

Cold imports now prepare up to four textures concurrently, reuse compression
workers, coalesce identical texture source bytes within an import, and publish
assets through a bounded writer pool. Mip filtering processes RGBA together while
retaining the existing filters, gamma handling, normal normalization and alpha
coverage behavior. Geometry and submesh processing still run serially, and the
whole FBX import still requires the main thread for GPU initialization.

Models default to Fast Texture Compression, which uses UASTC effort 1 for LDR
material textures. Disabling it restores ETC1S colors and UASTC effort 2 for normal
and data textures. Standalone texture imports retain effort 2 by default. Both
settings round-trip through import metadata and participate in texture cook keys.
Existing asset formats and UUID assignment remain in use.

Six alternating Windows/Vulkan runs used the same executable, disabled texture
cook caches and fresh output files each time. Every run imported 4,877 assets.

| Cold import | Legacy compression | Fast compression |
| --- | ---: | ---: |
| Run 1 | 117.927 s | 80.698 s |
| Run 2 | 131.144 s | 68.132 s |
| Run 3 | 100.956 s | 69.782 s |
| Median | 117.927 s | 69.782 s |

Fast compression improved the median **1.69×**. Both policies already include
parallel texture preparation, source coalescing, the mip changes and bounded
writers, so this comparison isolates the compression policy. The **10× cold-import
target has not been reached**. An earlier fast run took 59.653 seconds, but is not
part of this paired comparison. The user's scale 0.8 and enabled morph import were
preserved. The benchmark reserved ten compiler workers through Crowny's scheduler,
leaving two available for other work. Interactive previews were left running.
The OS file cache was not flushed. Timing exclusions match the earlier benchmark.

The faster policy increased logical asset output from 717,967,287 to 967,302,599
bytes. Estimated runtime texture payload increased from 60,150,000 to 77,304,416
bytes; these estimates exclude allocation overhead and other GPU resources.
Three sampled normal maps had less than 0.08 degrees additional mean angular
error at effort 1 versus effort 2. Sampled foliage color had lower RGB error and
fewer alpha-cutoff mismatches than ETC1S, but larger maximum alpha error. These
fixture measurements do not replace a visual check of the full scene.

The earlier full native suite passed 950 cases and 113,374 assertions, with one
case skipped. Final editor and player builds passed. Subsequent importer fixtures passed on
both Vulkan and OpenGL, including texture cook invalidation, alpha metadata,
parallel texture identity and model material import. OpenGL storage buffers now
use sequential physical binding points so material shaders do not exceed the
device limit merely by using descriptor set 2.

Full renderer validation is not passing in this shared working tree. A rebuild
resolved a crash in the sprite tests, but the subsequent Vulkan run reported a
decal CPU/GPU-list mismatch and crashed inside the Intel Vulkan driver. That fault
location does not establish its underlying cause. Earlier runs also had 2D and
material-preview failures. References were not changed by this work. The latest
editor controls and full Bistro appearance have not been manually rechecked.

The final native rebuild first hit a compiler PDB-service failure, then a newly
added build-pipeline test accessed diagnostics on the wrong report type. The test
now reads the managed-compilation stage's diagnostics and retains its assertions.
Its retry and the final full OpenGL run remained queued behind other builds; those
idle waiters were stopped. The latest complete native pass remains the earlier
950-case run, and the final full-suite checks remain pending. No other task's
editor or build was stopped.

Local evidence is under `artifacts/bistro/cold-validation-7/`, with the earlier
legacy-policy run under `cold-validation-5/`. The subsequent backend importer
results are in `artifacts/bistro/gl-dense-validation/results.json`.
The six-run comparison, executable hash and metadata snapshot are under
`artifacts/bistro/cold-policy-comparison-3/`.
Final build and importer logs are under `artifacts/bistro/cold-validation-9/` and
`cold-validation-9-driver.log`. The later render failure is recorded in
`artifacts/bistro/render-validation-final-driver.log`; native rebuild failures are
in that validation driver log and `artifacts/bistro/native-validation-final.log`.
