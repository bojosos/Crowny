# Model import

FBX, glTF/GLB, OBJ, and the other Assimp formats share `MeshImporter`. Existing imports remain mesh assets. The primary mesh UUID and material subasset reconciliation continue through `ProjectLibrary`.

## Prefab output

Enable **Generate Prefab** in the model import inspector and reimport. The dependent prefab references the primary combined mesh and its material slots. **Import Lights** and **Import Cameras** control scene components in that prefab. These options do not change the primary asset type.

The prefab retains source node transforms, names, directional/point/spot lights, and perspective/orthographic cameras. Node IDs derive from source-relative hierarchy paths, so reimporting unchanged nodes preserves their IDs. Duplicate sibling names use an occurrence number. Renaming/reparenting a node changes its ID.

Geometry is still baked into the combined mesh on the prefab root. The retained source nodes do not provide independently editable mesh pieces. Animated/skinned mesh restrictions remain those of the existing mesh importer. Unsupported light types produce a warning. glTF light intensities are converted from candela to Crowny's lumens for local lights; directional intensities remain lux. Legacy formats have less consistent intensity conventions. Missing local-light ranges use a finite 1,000-unit fallback.

Prefabs cook through the asset manager using the existing scene component serializer. Imported mesh/material handles receive their reconciled UUIDs before saving. Text `.cwprefab` sources remain readable and now produce binary cache assets instead of copying YAML into the binary cache.

## Materials and textures

Imported materials retain opacity, glTF OPAQUE/MASK/BLEND modes, alpha cutoff, emissive color/intensity/maps, and the existing PBR channels. BLEND uses weighted transparency. This is not refractive glass.

For legacy materials without an explicit alpha mode, varying albedo alpha crossing 0.5 becomes a cutout. Constant-zero alpha is treated as unused. Explicit glTF modes take precedence. Normal maps use linear decoding. Missing author paths and DDS references can resolve to supported image files beside the model or in its `Textures` directory.

Degenerate tangent bases are repaired per vertex rather than discarding otherwise valid mesh groups. Invalid geometry and singular transforms still report errors.

Model texture preparation uses up to four concurrent CPU lanes for decoding,
mip generation, alpha analysis, and compression. Repeated source/profile pairs
are prepared once per model. Preferred material channels are prepared first;
failed channels retain their existing fallback behavior. GPU initialization,
material binding, and dependent output ordering remain on the caller thread.
Custom importers that require serialized or main-thread execution keep the batch
on the caller thread. Mesh import itself still uses the main-thread policy.
Identical external raster sources also share compression work within the current
batch, including when disk caches are disabled. Distinct filenames retain separate
texture objects and names, so this does not merge their asset IDs. This temporary
table is discarded after preparation; sources larger than 32 MiB and custom
texture importers use their normal import path.

Basis compression leases reusable encoder queues from a bounded pool. Concurrent
encodes share the hardware thread budget instead of each creating a full-size
thread pool. This applies to cold imports as well as cache misses during reimport.
**Fast Texture Compression** defaults on for model imports. It cooks LDR material
textures as UASTC at search effort 1. Disabling it restores ETC1S color textures
and UASTC normal/data textures at effort 2. Fast mode produces larger color assets
and can increase GPU memory use, but avoids the slower compression searches.
Standalone texture imports retain effort 2 by default and expose a separate
compression-effort setting. Both settings round trip through existing import
metadata; changing effort invalidates the corresponding texture cook.

Mip filtering processes RGBA together using the existing Basis filter coefficients
and axis order. Gamma conversion, normal normalization, and alpha coverage remain
part of the same mip pipeline. Large intermediates use the streaming resampler to
bound temporary memory. Both texture cook keys use algorithm revision 2, so cooks
from the previous mip implementation are rebuilt once.

## Reimport performance

Compressed texture imports keep a disposable cook cache under the project's
`Internal/ImportCache/Textures-v1` directory. The key includes decoded pixels,
dimensions, pixel format, and serialized import settings. Equivalent images can
share a cook even when the source filenames differ. Mesh and material edits still
rebuild the model; unchanged images reuse their mip chain and Basis payload.
The `Sources-v1` subdirectory additionally keys cooked textures by their source
bytes and settings, allowing unchanged files and embedded images to skip decoding.
Legacy albedo alpha classification has a separate source-content cache under
`Internal/ImportCache/Alpha-v1`, with checksum validation and the same recook fallback.

Texture edits and settings changes produce new keys. Cache payloads have SHA-256
checksums; missing, truncated, or damaged entries are rebuilt. The cache is optional,
and a failed cache write does not fail the import. It does not change texture quality,
material slots, asset UUID reconciliation, or exported asset formats. Increment the
cook version when changing mip generation or encoder policy.

ProjectLibrary resolves each unique imported object's final dependency ID once.
Repeated texture slots keep their dependent metadata and the same final reference
IDs as before. Assets serialize on the calling thread, with up to sixteen disk
operations queued and 64 MiB of queued payloads, except for a single oversized mesh.
A fixed writer pool services the batch instead of creating a thread for each file.
Repeated texture objects share a serialized buffer, bounded separately at 32 MiB.

Reimport compares mesh, material, and texture payloads against their existing files.
If every byte except the compile timestamp matches, the existing file is retained.
Changed or damaged payloads use the existing flushed, atomic replacement path.
Asset notifications run on the calling thread after all writes have joined. Metadata
is committed only after successful publication. The batch is not an all-or-nothing
filesystem transaction, matching the previous per-asset publication behavior.

For a local timing comparison after building RenderTests, run its executable with
`--benchmark-import <model-path> --artifacts <output-directory>`. The adjacent
`.meta` file supplies the import options. Output assets and timing JSON go under
the artifact directory; the source project is not changed. Add
`--benchmark-no-cache --benchmark-serial-writes` for the control path. Compare a
cache-populating run separately from subsequent reimports, using the same executable
and otherwise idle machine. Artifact directories with the same parent share the
benchmark's disposable texture cache. Repeat the same output directory to measure
reimport, including unchanged asset-file retention. Temporary asset and built-in
shader IDs are stable across benchmark processes. Timing includes source parsing,
texture preparation, dependency assignment, and asset publication. Output byte-count
verification and application startup are outside the timed interval. The benchmark
does not include the editor's metadata/index commit or subsequent viewport refresh.
For cold-import measurements, use `--benchmark-no-cache` and a fresh output
directory on every run. This disables texture and alpha cook caches and prevents
unchanged asset-file retention from hiding initial publication costs.
The benchmark does not flush the operating system's file cache.
Add `--benchmark-legacy-textures` to override only the model's fast compression
setting in memory. This allows both policies to be measured with the same binary
and source metadata, without editing the project.
Run the render harness with `--validate-importers --backend vulkan` and again with
`--backend opengl` to check cold texture preparation, cache recovery, material
semantics, and stable dependent ordering without running the visual cases.

To compare texture compression on an individual source image, run
`--benchmark-texture <image-path> --artifacts <output-directory>`. Add
`--benchmark-texture-normal` for normal-map angular error. The report contains
encode time, payload bytes, RGB PSNR, alpha error, and cutout classification
differences, plus decoded images for UASTC efforts 0, 1, and 2 and the current
ETC1S quality 192. This diagnostic encodes
only the base level; it does not measure full texture or model import time and
does not change project import settings.

## Environment lighting

For an HDR texture, enable **Generate Environment Map** and reimport. The texture remains primary; its environment subasset contains the cubemap, irradiance, prefiltered reflections, and diffuse SH. The viewport settings popup accepts that environment asset.

Scene format 13 saves the environment UUID in YAML and binary scenes and preserves it in scene copies. Version 12 scenes remain readable with no environment assigned. Environment cooking submits GPU rendering before cache readback.
