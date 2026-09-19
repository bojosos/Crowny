# 2D expansion implementation ledger

The accepted September 5, 2026 plan covers the entire roadmap below. A completed
foundation or demonstration is not completion of this expansion. Keep this ledger
open until the authoring, runtime, migration, packaging, and performance gates pass.

## Current implementation status

Milestone 1 is in progress. The code adds generational retained sprite storage,
snapshot-owned changes, dirty GPU uploads, instanced sprites, adjacent ordered
batching, cached CPU radix ordering, and separate GPU draw-order caches for cameras.
GPU instance storage grows in retained pages bounded by the device storage-buffer
limit. Page changes preserve order, and unchanged packed records are not uploaded
again. The compatibility sprite path uses instances and flushes at primitive and
texture-table boundaries. Camera draw-order buffers now also grow in bounded
pages, splitting draws at page boundaries without changing transparency order.
Each page caches its own contents, so a local reorder uploads only affected pages.
Unused pages remain resident until their view is released. Vulkan/OpenGL checks
cover differing storage/order page sizes, empty views, page restoration, local
order edits, and transparent overlap with picking. Text-only views skip the
unused sprite-order buffer.

Pending changes now coalesce as slot indices. Publication copies each final value
directly into snapshot-owned storage, removing the intermediate arrays of full
records. The standard-vector adapter remains available. Tests cover cancellation,
motion settling, warm capacity, and earlier snapshots surviving subsequent drains
and slot reuse.

Texture bindings are now explicit snapshot deltas. Creates publish an owned
texture reference; updates publish one only when the binding changes. A flagged
null clears the binding, while an unflagged update retains the renderer's current
resource. Transform and tint changes no longer add a texture owner per snapshot
record. Native callers constructing complete changes retain the previous default
behavior. Extraction also keeps eight temporary resource owners shared across
sprites, with bounded replacement for fragmented textures. Tests cover more
textures than the cache holds, direct edits, reimport under the same asset identity,
slot reuse, clears, and old snapshots retaining their concrete resources.

Sprite synchronization now uses lazily allocated 1,024-slot tracking pages keyed
by ECS entity index. It keeps a dense list of active indices for retirement, so
sparse entity ranges do not require scanning unused pages. Component identities
distinguish replacement and entity reuse. Sprite moves preserve those identities
through ECS pool compaction; copies for duplication still receive fresh identities.
The regression originally produced four changes when removing two sprites because
compaction also changed an unrelated sprite's identity.

Transform composition scales the rotation matrix columns and writes translation
directly, avoiding two general matrix multiplications. Retained sprite updates
also avoid changing texture reference counts when the texture is unchanged.

Scene snapshots now carry four-byte sprite handles instead of another full copy
of each sprite's transform, color, and texture reference. The render-thread mirror
supplies culling transforms and can reconstruct full payloads for compatibility
drawing. Legacy native producers can still populate the existing `Sprites` array.
Scene-produced ordered sprite indices address `SpriteHandles`.

Capable Vulkan views now cull sprites and compact their ordered candidates on the
GPU, followed by indexed indirect instanced draws. Two dispatches compute local
prefixes and group counts, then scatter survivors to their stable positions. Each
submission is bounded at 65,536 entries and still respects texture, instance-page,
order-page and sprite/text boundaries. Per-view scratch buffers grow and remain
resident. Dispatch/draw transitions use the existing resource hazard tracking and
restore the target with attachment loads. OpenGL and an explicit native CPU override
retain CPU culling and direct instancing. Producers without a snapshot target also
use that compatibility path.

The GPU path caches candidate order independently of camera visibility. It reports
completed visibility samples with their frame number and validity, using at most
four pending readbacks per view. No visibility readback determines a draw or waits
inside rendering. Vulkan buffers retain up to four reusable readback requests.
Only requests with no external owner and no recorded or submitted GPU use can be
recycled. Additional requests remain independently owned, so cache pressure never
drops copies or overwrites retained results.

Vulkan command buffers now retain allocation storage for resource-tracking nodes.
Clearing a command buffer still releases the tracked resources. Dynamic buffers
retain idle backing versions for discard writes, preserving earlier recorded and
submitted uses without waiting for them. Texel buffers keep their existing view
ownership rules. Full active sprite pages use discard uploads; partial updates
preserve untouched records. These caches retain their high-water capacity until
their owning buffer or command buffer is destroyed.
Presentation reuses an exclusively owned main command-buffer wrapper and retains
the arrays used to retire completed queue work. Cleanup still runs outside the
submission lock; concurrent refreshes borrow separate arrays.

Sprite-only Vulkan views also retain their CPU draw lists. They compare ordered
generational handles and invalidate batch bindings when instances are created,
destroyed, hidden, shown, or assigned another texture. Transform and paint changes
still upload instance data and run GPU visibility, but reuse the candidate list.
CPU culling and mixed text prepare each view normally. Scene statistics and the
benchmark CSV expose draw-list cache hits and upload/preparation/submission CPU
times. Each view retains an extra array of four-byte candidate handles.

Text snapshots own their layout arrays and pin their concrete font objects.
Layout caching separates layout fields from paint fields. Scene text now uses
renderer-owned, immutable local-space glyph pages: 48 bytes per instance, with
unit quads generated in the vertex shader. Moving or repainting a label changes
draw constants without rebuilding or uploading placement. Text-only views do not
initialize the sprite material. Cameras share pages;
layouts pin font and atlas resources until extraction and queued snapshots release
them. Shadows, outlines, italics, decorations, and local clipping retain their
existing behavior. A new GPU regression checks compatibility pixels and IDs,
forced page splits, mixed sprite/text ordering, scene submission, cache reuse,
and retirement. Cached bounds include italic slant, shadows, decoration offsets,
local clipping, and transformed half-axes for conservative frustum culling.
The retained text path passes the focused Vulkan/OpenGL checks described below.

Text pages currently draw per label and consecutive font atlas run; cross-label
instance batching and retained text-object GPU records remain open. The immediate
compatibility text path still expands vertices and owns a global batch. World 2D
still uses legacy final composition. Views without meshes, a sky environment, or
custom render features now use two explicit graph passes to clear and draw the
output. They allocate no 3D graph textures or buffers and release obsolete 3D view
history. Shared scene tables are still synchronized before selecting this path;
2D buffers are still bound inside the draw callback rather than imported into the
graph. These paths, full scene-pass integration, and the remaining milestone 1
contracts are unfinished. Text still uses CPU visibility testing.

### Sprite benchmark

`crowny render-tests --backend Vulkan --benchmark-sprites full` runs 100,000
moving ECS sprites at 1920x1080 with VSync disabled, four texture pages, 8x8-pixel
quads, alpha 0.5, and a fixed seed. It records 1,800 frames after 300 warm-up
frames. `--benchmark-sprites smoke` keeps the same object count and resolution
but runs only 5 warm-up and 10 measured frames. Select OpenGL or All to exercise
the fallback backend. Image filters and reference updates cannot be combined
with a sprite benchmark.

Each backend writes `sprites.csv` and `sprites-summary.json` beneath the chosen
artifact root. Measurements separate ECS updates, snapshot extraction, rendering,
presentation, asynchronous GPU timing, current-thread C++ allocation counts,
upload bytes, visibility, and draw batches. Missing GPU samples are reported.
Every frame checks that the render graph succeeds and all 100,000 sprites remain
visible. A final offscreen capture and coverage check run after measurement.
This first diagnostic renders snapshots serially to the hidden render-test
window. GPU queries cover rendering commands; Vulkan transfer uploads occur
outside that query. It uses full-texture sprite regions on four pages;
authored SpriteAtlas assets are still pending. A timing result here does not
certify the player, atlas, memory, or full roadmap acceptance gates.

Full September 12 runs in the isolated 2D checkout recorded Vulkan p95 61.9024 ms
and OpenGL p95 62.6256 ms after direct snapshot publication. Both used all 300
warm-up and 1,800 measured frames, retained 100,000 visible sprites in three draws,
and returned every GPU timing sample. Extraction allocated nothing. Vulkan made
93 rendering allocations per measured frame; OpenGL made none in that scope.
The target remains unmet. The earlier Vulkan baseline was p95 101.341 ms under
different shared-machine load, so the entire difference is not attributed to the
code change. Its final capture and the new Vulkan capture are byte-identical.

The isolated Release All build passes. The focused native suite passes 23 cases
and 259 assertions; the full suite passes 946 cases and 113,362 assertions, with
one skipped case. Both persistent sprite/text GPU cases and their cross-backend
capture comparisons pass. The 60 Python tooling tests, generated interop check,
and parity for 542 managed host functions pass. The verified Vulkan dispatch
hazard fix from the decal task is included.

After integration, the main Release All build passes. The focused native suite
passes 23 cases and 259 assertions; the full native suite passes 967 cases and
113,660 assertions, with one skipped case. Persistent sprite/text rendering and
both cross-backend capture comparisons pass. Full benchmark runs recorded Vulkan
p95 52.9667 ms and OpenGL p95 53.6557 ms, each with 300 warm-up and 1,800 measured
frames. Both retained all sprites and returned every GPU timing sample. Extraction
again allocated nothing; Vulkan rendering still allocated 93 times per frame.
Median extraction time was 30.8943 ms on Vulkan and 30.2814 ms on OpenGL, exceeding
the entire frame budget. These results leave the primary target unfinished.

The final OpenGL capture was visually inspected. The final Vulkan/OpenGL images
differ above 3/255 at two of 2,073,600 pixels, with maximum channel error 35/255
and mean error 0.000006631/255. This diagnostic comparison does not replace the
focused rendering assertions. Linux validation remains open; this Windows
machine does not have WSL installed.

Run logs and captures are under `artifacts/2d-expansion/benchmark-*` in the isolated
checkout. The final workload records are in `benchmark-direct/`, with the earlier
baseline in `benchmark-baseline/`.
Main integration logs are `benchmark-direct-main-all.log`,
`benchmark-direct-main-native.log`, and `benchmark-direct-main-render.log` under
`artifacts/2d-expansion/`. Full main benchmark records and captures are under
`artifacts/2d-expansion/benchmark-direct-main/`.

### Paged sprite tracking validation on September 12, 2026

The instrumented 100,000-sprite diagnostic measured tracking lookup at roughly
10-12 ms per frame with the hash map and 1.4-1.6 ms with paged entity lookup.
Those values include per-object probe overhead. The probes were removed before
the final builds and measurements; use full-frame records for performance gates.

- Release All builds pass in the main and isolated checkouts, including editor,
  managed assemblies, render tests, and staged player templates.
- The focused native suite passes 27 cases and 443 assertions in both checkouts.
  The main full suite passes 973 cases and 113,902 assertions, with one skipped
  case. The isolated full suite passes 950 cases and 113,546 assertions, with one
  skipped case.
- The main renderer suite passes all 39 cases on Vulkan and OpenGL, and all 39
  cross-backend capture comparisons pass. The first OpenGL run failed a texture
  cache reimport check when Windows denied an atomic file replacement. A retry
  passed without source or reference changes. Persistent sprite/text captures
  were visually inspected; the isolated focused GPU checks also pass.
- All 65 Python tooling tests, generated interop checking, and parity for 542
  managed host functions pass. Native formatting and diff whitespace checks pass.
- Linux and Windows editor interaction checks remain open. These results do not
  complete milestone 1 or the expansion roadmap.

Main logs are `tracking-main-all.log`, `tracking-main-native-focused.log`,
`tracking-main-native-full.log`, `tracking-main-render.log`,
`tracking-main-render-opengl-retry.log`, `tracking-main-compare.log`, and
`tracking-tooling.log` under `artifacts/2d-expansion/`. Captures are under
`tracking-main-render/` in the same directory.

Before compact snapshots, the full main benchmark recorded Vulkan p95 26.5627 ms
and OpenGL p95 28.0406 ms. Median extraction was 13.3672 ms and 13.4018 ms,
respectively. Both runs retained 100,000 visible sprites, made no extraction
allocations, and returned every GPU sample. Vulkan still made 93 rendering
allocations per frame. The 16.67 ms gate remains unmet. The Vulkan final image is
byte-identical to the earlier main capture; OpenGL differs above 3/255 at one
pixel, with maximum channel error 16/255. Records are under
`artifacts/2d-expansion/tracking-main-benchmark/`.

### Compact snapshot validation on September 12, 2026

The main Release All build passes after moving scene snapshots to sprite handles.
The focused native suite passes 28 cases and 457 assertions. The full native suite
passes 974 cases and 113,916 assertions, with one skipped case. All 39 rendering
cases pass on each backend, and all 39 Vulkan/OpenGL capture comparisons pass.
Persistent sprite/text captures were visually inspected. The mixed scene check
uses compact handles and verifies premultiplied color, depth clearing, and picking;
earlier direct draws in that check retain full legacy payloads.

Logs are `compact-main-all.log`, `compact-main-native-focused.log`,
`compact-main-native-full.log`, `compact-main-focused-render.log`, and
`compact-main-render.log` under `artifacts/2d-expansion/`. Captures are under
`compact-main-render/`.

Full compact-snapshot runs recorded Vulkan p95 48.5364 ms and OpenGL p95
46.7850 ms, with median extraction 23.2614 ms and 22.9295 ms. A Vulkan repeat
recorded p95 49.0293 ms. The preceding implementation, rerun as an isolated-checkout
control under the later conditions, recorded p95 51.3889 ms and median extraction
26.0424 ms. Its unchanged ECS update stage also slowed to 3.4496 ms median,
matching the compact run's 3.4000 ms and exceeding the earlier run's 1.7651 ms.
The cause of this broad CPU timing variation remains unresolved; do not attribute
the difference between the earlier 26.5627 ms and later runs solely to code.

Every run used all 100,000 moving sprites, 300 warm-up frames, and 1,800 measured
frames. Both backends retained three sprite draws, zero extraction allocations,
and every GPU timing sample. Vulkan still made 93 rendering allocations per frame.
The final compact captures are byte-identical to the preceding tracking captures
on each backend. The performance gate remains unmet. Full records and images are
under `compact-main-benchmark/` and `compact-main-repeat/` in the main artifact
directory; the control is under the isolated checkout's `tracking-control/`.

### GPU sprite compaction validation on September 12, 2026

Release All builds pass, including the new cooked compute shader, the built-in
resource pack and staged player template. The focused native suite passes 35
cases and 1,994 assertions; the full native suite passes 974 cases and 113,916
assertions, with one skipped case. All 39 renderer cases pass on Vulkan and
OpenGL, and all 39 cross-backend comparisons pass. The persistent sprite/text
captures were visually inspected. The Python tooling suite passes all 65 tests.

The new GPU/forced-CPU comparison checks 1,031 ordered sprites across partial
128-thread workgroups, different storage/order page sizes, texture boundaries,
negative scale and shear, camera changes, an entirely culled view, and delayed
visibility counts. Color and picking agree. GPU camera movement does not reupload
candidate order. Existing mixed text/sprite checks also use the GPU path.

The first Vulkan run caught missing draws because compute dispatch unbinds the
framebuffer. Restoring the snapshot target with attachment loads fixed both the
sprite comparison and text interleaving without changing image references.
No temporary instrumentation remains.

The full 100,000-sprite diagnostic records Vulkan p95 45.5409 ms and OpenGL p95
46.0857 ms. Both used 300 warm-up plus 1,800 measured frames, retained 100,000
visible sprites and three draws, and returned every GPU timing sample. Vulkan
visibility samples were consistently two frames behind; the CSV now records their
frame identity and validity. OpenGL visibility is current-frame CPU data.

Vulkan median extraction is 23.2763 ms, rendering 10.2409 ms, and GPU time
10.1604 ms. Uploads are 14,400,912 bytes per frame, including 672 extra bytes of
compaction constants. Vulkan now records 201 renderer C++ allocations per frame,
up from 93 on the preceding CPU path. OpenGL still records zero rendering
allocations, and both record zero extraction allocations. The visibility feature
is correct, but this allocation increase and the 16.67 ms gate remain unfinished.
Snapshot extraction alone exceeds the frame budget under these run conditions.
The final captures are byte-identical to each backend's preceding compact-snapshot
capture; the Vulkan GPU-rendered benchmark image was visually inspected. Records
and captures are in `artifacts/2d-expansion/gpu-compaction-benchmark/`.

Logs are under `artifacts/2d-expansion/`: `gpu-compaction-final-all.log`,
`gpu-compaction-native-focused.log`, `gpu-compaction-native-full.log`,
`gpu-compaction-render.log` and `gpu-compaction-tooling.log`. Full renderer
captures are in `gpu-compaction-render/`. Linux and Windows editor interaction
checks remain open. This completes a part of milestone 1, not the full milestone
or expansion.

### Texture extraction validation on September 19, 2026

Release All and the focused 2D/math suite pass, including 38 cases and 2,320
assertions. The full native suite passes 977 cases and 114,242 assertions, with
one skipped case. All 39 Vulkan cases, 39 OpenGL cases and 39 cross-backend image
comparisons pass without reference updates. Vulkan text and OpenGL sprite
captures were visually inspected. All 65 tooling tests, generated interop,
542-function managed parity, and header checks pass.

The full diagnostic after texture deltas and the extraction resource cache records
Vulkan p95 52.3721 ms and OpenGL p95 34.3967 ms. Both retain all 100,000 visible
sprites in three draws, report no missing GPU timer samples, and allocate nothing
during extraction. Vulkan still makes 201 renderer allocations per frame; OpenGL
makes none. Each backend used 300 warm-up and 1,800 measured frames. The target
remains unmet. These results precede the additional CPU draw-list cache.

Artifacts and logs are under `artifacts/2d-expansion/texture-extraction-*`.
Short diagnostic probes were removed before the full build and benchmark.
The native regression also covers 37 sprites using 12 textures, direct assignment,
clearing, and reimport while the previous snapshot remains alive.

### Draw-list cache validation on September 19, 2026

Release All passes with the editor, player template and render harness. The
focused native suite still passes 38 cases and 2,320 assertions; the full suite
passes 977 cases and 114,242 assertions, with one skipped case. All 39 Vulkan,
39 OpenGL and 39 cross-backend comparisons pass without reference changes.
The expanded CPU/GPU regression compares color and picking over 15 frames. It
checks expected cache hits during movement and paint changes, invalidation for
textures, visibility, reversed order, remapped handles and recycled slots, plus
camera switching and view retirement. Header checks and native formatting pass.

Both full sprite runs again use 300 warm-up plus 1,800 measured frames. Every
record reports 100,000 visible sprites, three draws and a valid visibility sample;
all GPU timing samples complete. Vulkan hits the CPU draw-list cache on every
measured frame. OpenGL continues to prepare CPU visibility each frame.

| Measurement | Vulkan | OpenGL |
| --- | ---: | ---: |
| p95 frame time | 29.0718 ms | 32.6772 ms |
| Median snapshot extraction | 12.3552 ms | 12.1063 ms |
| Median rendering CPU time | 5.6327 ms | 7.5294 ms |
| Median instance upload CPU time | 1.7790 ms | 2.5057 ms |
| Median draw preparation CPU time | 0.1795 ms | 1.8511 ms |
| Median draw submission CPU time | 1.6588 ms | 0.0361 ms |
| Median GPU time | 10.8987 ms | 14.3980 ms |
| Renderer C++ allocations per frame | 201 | 0 |
| Extraction C++ allocations per frame | 0 | 0 |

The 16.67 ms gate and zero-allocation Vulkan gate remain unmet. Timing variation
also affects extraction, which the draw-list cache does not change, so the entire
frame-time difference from the preceding run cannot be attributed to this cache.
Uploads remain 14,400,912 bytes per frame on Vulkan and 14,400,240 on OpenGL.
The final images are byte-identical to their preceding backend captures. The
Vulkan benchmark image and persistent text/sprite captures were visually inspected.

Logs and artifacts are in `artifacts/2d-expansion/draw-list-cache-*`. The missing
`Scripts/run-render-tests.ps1` entrypoint is covered by `crowny render-tests`.
Linux, interactive Windows editor workflows, standalone benchmark integration,
and the remaining roadmap contracts are still open.

### Vulkan buffer reuse validation on September 19, 2026

Command-buffer tracking now pools its CPU nodes. Dynamic buffer discard writes
reuse idle backing allocations, and full active sprite pages discard their old
contents. A bounded readback cache reuses requests only after external owners and
GPU uses have retired. Partial writes still preserve untouched bytes. Presentation
also retains queue cleanup arrays and reuses an exclusively owned command wrapper.

The Vulkan regression records different values before submission, checks both
copies after completion, and requires allocation-free writes and readbacks after
warm-up. It retains an earlier result across later frames, verifies that pending
copies are not ready, and checks different request lengths and six outstanding
requests against the four-request cache. The allocation assertion failed before
readback pooling and passes afterward. Existing ranged-write, mixed rendering,
capacity split, picking, and GPU/forced-CPU comparisons continue to pass.

Release All passes, including the editor and staged player template. The focused
native suite passes 38 cases and 2,320 assertions. The final full suite passes 979
cases and 114,265 assertions, with one skipped case and exit 0. Its existing SEH
filter warning still appears during shutdown. All 39 Vulkan cases, 39 OpenGL
cases, and 39 cross-backend capture comparisons pass without reference updates.
Sprite and text captures were visually inspected; final captures match those
inspected images byte for byte. All 65 tooling tests, generated interop, managed
parity for 542 host functions, header checks, and native formatting pass.

The final benchmark uses 300 warm-up and 1,800 measured frames on both backends.
Every measured frame retains all 100,000 visible sprites in three draws with valid
visibility data, and no GPU timing samples are missing. Final images match each
backend's preceding benchmark image byte for byte.

| Measurement | Vulkan | OpenGL |
| --- | ---: | ---: |
| p95 frame time | 16.2707 ms | 21.5810 ms |
| Median snapshot extraction | 8.9444 ms | 10.1251 ms |
| Median rendering CPU time | 3.0128 ms | 5.1897 ms |
| Median instance upload CPU time | 1.2262 ms | 1.5490 ms |
| Median draw preparation CPU time | 0.1393 ms | 1.4819 ms |
| Median draw submission CPU time | 0.0874 ms | 0.0292 ms |
| Median GPU time | 10.0395 ms | 13.5288 ms |
| Renderer C++ allocations, all measured frames | 84 | 0 |
| Extraction C++ allocations, all measured frames | 0 | 0 |
| Presentation C++ allocations, all measured frames | 5,402 | 0 |

Vulkan rendering allocates nothing on 1,799 measured frames, but measured frame
460 allocates 84 times. Its presentation count also rises from three to five.
The source of that isolated burst remains to be traced. The three recurring
presentation allocations are the binary synchronization semaphores recreated by
`VulkanCmdBuffer::AllocateSemaphores`. Their reuse must distinguish consumed waits
from unused, still-signaled semaphores; idle resource checks alone are insufficient.
The broader zero-allocation gate therefore remains unfinished.

This final Vulkan diagnostic meets the 16.67 ms timing threshold for this run.
The preceding full run recorded 18.9457 ms, with zero rendering allocations and
four presentation allocations per frame, before removing a temporary command
wrapper reference. CPU extraction also varies between runs, so the timing
difference is not attributed entirely to wrapper reuse. This does not certify
the standalone player, authored-atlas workload, or the complete performance gate.
Standalone integration, Linux validation, interactive editor checks, and the
remaining roadmap contracts are still open.

Final logs and captures are under `artifacts/2d-expansion/allocation-final-*`.
The focused native and tooling logs are `allocation-native-focused.log`,
`allocation-tooling.log`, `allocation-managed-parity.log`, and
`allocation-interop.log`. Earlier diagnostic logs use `allocation-*`; all tagged
allocation probes were removed from engine code before final validation.

### Retained text validation on September 12, 2026

- Release `build All --jobs 2` passes in the isolated checkout, including the
  editor, player template, native tests, and render harness. The build uses the
  x64 linker through `LinkToolPath` to avoid the local 32-bit linker's PDB failure.
- `crowny test --filter '[2D]'` passes 21 cases and 234 assertions. The full native
  suite passes 944 cases and 113,337 assertions, with one skipped case and exit 0.
- The 2D-only graph regression covers Forward+ and Deferred+ configurations,
  prerequisite execution, and zero transient 3D allocations. Scene submission
  passes depth-clear, premultiplied color, and picking checks on both Vulkan and
  OpenGL. The final shared text capture comparison also passes.
- `persistent-text` passes on Vulkan and OpenGL, including the shared capture
  comparison. It checks bounded glyph pages, paint/transform reuse, clipping,
  outlines/shadows/decorations, culling bounds, multiple cameras, mixed sprite/text
  order, picking, and layout retirement. Captures were visually inspected.
- `integer-clear-draw` passes on both backends, with exact capture agreement. It
  checks repeated nonzero-to-zero integer clears followed by partial sprite draws,
  including odd dimensions, one-pixel rows/columns, a single-pixel target, and
  fully transparent sprites that must preserve the cleared picking value.
- OpenGL integer attachments use typed clears that preserve the supplied bits,
  matching Vulkan. Iris Xe driver 32.0.101.7085 reproduced stale IDs after a full
  integer clear followed by a partial draw, including with a single plain sprite.
  A memory barrier and `glFlush` did not fix it. Two disjoint scissored clears did.
  Intel OpenGL uses that bounded workaround, preserving the original scissor and
  write state without readbacks or GPU waits. Diagnostic probes were removed.
- The text fixture avoids exact pixel-center ties on glyph rectangle edges.
  Remaining cross-backend MSDF edge differences measure maximum 13/255 and mean
  0.0191/255. The text-only capture tolerance is max 16/255, mean 0.025/255, and
  at most 0.5% of pixels above 3/255. Same-backend retained/legacy color and ID
  comparisons and cache assertions remain separate. No existing 3D tolerance or
  reference was changed for this work.
- The full OpenGL run passes its first 12 cases, including all five new 2D/backend
  cases, then stops at the existing `toon-silhouette` framebuffer error 36055.
  Vulkan passes 13 of 20 cases; the isolated sphere material-preview and six decal
  failures remain.
- Header checking from `Scripts/` reports six existing missing-pragma headers;
  the added headers have guards. Native formatting was run on the changed files.
  Windows editor workflows, Linux validation, full international text, and the
  primary 100,000-sprite benchmark remain open.

Logs: `order-pages-all.log`, `order-pages-native-focused.log`, `order-pages-native-full.log`,
`graph2d-final.log`, `order-pages.log`, `integer-alpha.log`, and
`graph2d-full-*.log` under `artifacts/2d-expansion/`. Captures are in `graph2d-final/`,
`order-pages/`, `graph2d-full/`, `text-verified/`,
`integer-verified/`, and `text-full/`. The final isolated build and native results
above include draw-order paging.

### Main-checkout validation on September 12, 2026

- Release `build All` passes and stages the standalone player template. The log
  is `artifacts/2d-expansion/text-main-all.log`.
- The full native suite passes 965 cases and 113,635 assertions, with one skipped
  case and exit 0. The combined 2D/drag-drop checks pass 22 cases and 250 assertions.
  Logs are `integrated-native-full.log` and `integrated-native-focused.log` under
  `artifacts/2d-expansion/`.
- The suite initially crashed in the new nested asset-drop cursor fixture. It
  fabricated another target after `EndDragDropTarget` had cleared a delivered
  payload. The fixture now offers the parent target only while a payload remains,
  matching real target discovery. No production drag/drop behavior changed.
- `persistent-sprites` and `persistent-text` pass on both backends, including
  capture comparisons, under `artifacts/2d-expansion/integrated-pages/`.
- All five 2D/backend cases pass in the full renderer runs. Vulkan passes 20 of
  26 cases, with six missing references for new decal cases. OpenGL passes 18 of
  26, with the same missing references and two tapered-decal image mismatches.
  No decal references were changed. Logs are `artifacts/player-render-september12.log`
  and `artifacts/2d-expansion/integrated-full-opengl.log`.
- The standalone task reports successful clean and replacement exports, with
  relocated Vulkan/OpenGL players each rendering one sprite and running managed
  Start/Update. Its evidence is in `artifacts/player-validation-september12-final.log`.
  This is packaging coverage, not the 100,000-sprite performance gate.

### Foundation validation on September 8, 2026

- Release `build All` passes in the isolated checkout, including the latest compact
  OpenGL bindings. It produces the standalone player template.
- Focused native checks pass 22 cases and 236 assertions, covering 2D plus the
  corrected package/highlight fixtures. The full native suite passes 943 cases
  and 113,307 assertions, with one skipped case and no failures.
- `persistent-sprites`, `mixed-2d-order`, and `storage-buffer-bindings` pass on
  Vulkan and OpenGL on the Iris Xe. Their captures were visually inspected and
  their backend comparisons pass. Only these three new references were added.
  Checks cover page growth, partial pages, texture switches, IDs, unchanged
  uploads, alternating cameras, premultiplied blending, and shared buffers across
  shader stages and descriptor sets.
- The full Vulkan renderer run passes 11 of 18 cases. The sphere material-preview
  test and six decal cases fail. The sphere test also fails when run alone, so
  that failure does not depend on running the new 2D cases first.
- The full OpenGL run passes ten cases, then stops in `toon-silhouette` with
  incomplete framebuffer status 36055. Full renderer parity remains unverified.
  The failing 3D references were not changed.
- All 57 Python tooling tests, the managed generator check, and parity checks for
  542 existing host functions passed. No new managed 2D interfaces are delivered
  by those checks. Native formatting passes; the corrected header-check invocation
  and its existing warnings are recorded above.
- Windows editor workflows, standalone project packaging, Linux validation, and
  all performance gates remain open. No 100,000-sprite frame-rate result has been
  measured or claimed.

Logs and captures are under `artifacts/2d-expansion/`. Latest build and native
results are `compact-build-all.log`, `final-focused.log`, and `final-native.log`.
GPU results are in `compact-sprites/`, `compact-mixed/`, `verified-bindings/`,
and `compact-full/`; the corresponding full-suite logs retain failure details.

### Backend and validation fixes

OpenGL storage buffers now use predictable, stage-specific block names and dense
pipeline binding indices. This handles source block/instance name differences,
sparse descriptor sets, and shared byte layouts whose member names differ between
stages. Pipeline creation checks the binding-count limit, and OpenGL reports its
actual maximum storage-buffer range. Blending is explicitly disabled for
non-normalized integer attachments, preserving picking IDs while normalized color
targets retain premultiplied blending. The circle vertex layout now names its
entity-ID input consistently with its shader.

The built-in cooker reproduced a Vulkan shutdown crash with
`VUID-vkFreeDescriptorSets-descriptorPool-parameter`. Command buffers retained
uniform parameters after the descriptor manager had been destroyed. VulkanDevice
now releases command buffers before descriptor/query pools and keeps the resource
manager alive throughout. The original cooker command and the validation-enabled
rerun exit 0; the latter reports no validation errors.

The native package fixture now edits a valid on-disk manifest to simulate another
engine version, because the writer correctly rejects incompatible versions.
Viewport highlight assertions allow 0.001 output pixels of floating-point error.
These are test corrections, not runtime behavior changes. The editor guide adapter
uses the common clip-depth conversion and inverse play-camera world transform;
its visual editor check remains pending.

### Checkout and integration

Validation uses `C:/dev/Crowny-2d-expansion`, branch `codex/2d-expansion`, with
baseline workspace snapshot `e45604099a59d395b6c8d59f165e0f3ad111c07c`. Submodules
are pinned and initialized, and `crowny doctor` passes. That checkpoint contains
other tasks' changes as build context. Do not merge it wholesale.

Reviewed 2D changes and references have been integrated into the main checkout
while preserving concurrent work. The main checkout independently gained dense
OpenGL binding changes during this validation; integration retains that mapping
and adds the tested stage-specific block names. Full-suite results above describe
the isolated checkout, not the continuously changing main workspace.

After integration, the main checkout's Release RenderTests build and all three
new cases pass on Vulkan and OpenGL, including backend capture comparisons.
Those logs and captures are under the main checkout's
`artifacts/2d-expansion/integrated-*` paths.

Fresh-checkout build context also required a capabilities include in DecalAtlas,
selection-property includes and a live entity array in the inspector, and initial
normal Editor/Tests builds for editor/test-only dependency libraries skipped by
the first `build All`. These context fixes must not overwrite newer main-checkout
changes.

### Sprite component geometry on September 19, 2026

Texture-backed sprites now expose local `Size`, normalized `Pivot`, texture
endpoint `UvRect`, `FlipX`, `FlipY`, and `Visible` through native components,
the inspector, and the generated managed contract. The inspector previews the
selected region with its flips and aspect ratio. Flips mirror artwork around
the pivot without changing the entity transform or its colliders. Empty,
reversed, out-of-range or non-finite geometry suppresses drawing and picking.

Extraction folds the geometry into retained quad transforms and UV records.
Bounds, previous transforms, GPU compaction, the CPU baseline, and the legacy
rendering adapter consume those values. Direct component edits retain handles;
unchanged geometry does not generate updates after motion history settles.
Prefab syncing includes these properties and both sorting keys, respecting
individual overrides. Scene format 15 stores the added fields; versions 12–14
keep centered, full-texture unit quads with no flips and visibility enabled.
Prefab persistence, undo/redo, duplication and play copies use the same fields.

Validation: Release All passes, including managed assemblies, editor, render
harness and the staged player template. The focused 2D suite passes 35 cases
and 868 assertions; the full native suite passes 984 cases and 114,369 assertions
with one skipped case. All 39 Vulkan tests, 39 OpenGL tests and 39 cross-backend
capture comparisons pass without reference updates. Added pixel/ID assertions
exercise cropping, pivot reflection, hiding, invalid geometry and restoration
through GPU compaction, forced CPU draws and the compatibility adapter. Sprite
and text captures were visually inspected. The 65 tooling tests, generated
interop, parity for 554 host functions, header checks and native formatting pass.

Logs use `artifacts/2d-expansion/sprite-geometry-*`. The first All attempt met a
missing include in the concurrently added procedural-material test; that work
corrected it before the successful final build. An initial broad focused run
exited in a managed constructor before the interrupted All build had refreshed
the managed assemblies. Final focused and full runs pass after rebuilding them.
The pre-existing `complex_scene.yaml` content was restored byte for byte after
test execution. Interactive editor checks and Linux validation remain open.
The performance workload has not been rerun after adding geometry controls.

Sprite/atlas assets, slicing, packing, animation, nine-slice/tiled modes,
materials, visibility layers and scene compositing remain open, along with
international text, canvases, masks and tilemaps. Vulkan allocation work is
deferred at the user's request; the outstanding measurements above still apply.

### Authored sprite assets on September 19, 2026

The first sprite asset increment adds `.cwsprite` sources and a versioned cooked
`Sprite` asset containing a texture UUID, normalized region, pivot, original
pixel dimensions, pixels-per-unit and border metadata. Zero original dimensions
derive from the texture region. Defaults are a centered pivot and 100 pixels
per unit. Invalid edits leave metadata unchanged; missing references retain
their UUIDs. Borders are stored but nine-slice rendering is not implemented yet.

`SpriteRendererComponent.Sprite` selects an authored asset. `UseSpriteSize` and
`UseSpritePivot` default to true and allow per-instance overrides when disabled.
Asset edits and replacement under the same UUID update retained instances
without rewriting entities or changing their renderer handles. Missing or
mistyped sprite/texture references suppress authored-sprite drawing. Legacy
texture-only geometry remains unchanged. Scene format 16 appends the asset
reference and override flags; versions 12–15 remain readable. Prefabs, scene
copies and component undo retain these fields.

The asset browser can create sprites; the asset inspector edits their metadata
with retained undo, pending saves and a cropped preview. Sprite assets and image
files can be dropped into the viewport through its existing placement/undo
workflow. The component inspector offers asset selection and geometry overrides.
Source saves update the compiled cache immediately. Build dependency discovery
includes sprite assets, their textures and retained legacy texture references.
The shared managed ABI is version 21 with 567 functions, including a `Sprite`
wrapper, metadata access and component asset/override properties.

Validation: Release All passes, including managed assemblies, editor, render
harness and the staged player template. The focused suite passes 63 cases and
1,215 assertions. The full native suite passes 997 cases and 114,546 assertions;
the optional installed-OSL-compiler and CoreCLR-package cases are skipped.
All 39 Vulkan tests, 39 OpenGL tests and 39 capture comparisons pass without
reference changes. Sprite captures were visually inspected. The render
regression loads a sprite and texture from a cooked game package after deleting
its loose inputs, then checks pixels and picking through the CPU baseline,
GPU compaction and compatibility paths. Generated interop, parity for 567 host
functions, header checks, native formatting and all 65 tooling tests pass.

Logs use `artifacts/2d-expansion/sprite-assets-*`; final runs are
`build-verified`, `focused-verified`, `native`, and `render-verified`. Focused
tests caught YAML's fallback conversion accepting malformed optional fields;
the parser now distinguishes absent fields from invalid values and rejects
invalid edits transactionally. Cold texture references also reject known
non-texture asset headers before loading dependencies, preventing sprite cycles.
Earlier build attempts encountered an include-order error (fixed), a concurrent
OSL importer absent from generated projects, and DLL copy contention with
another native test process. The final All build and tests pass. The pre-existing
`complex_scene.yaml` was restored byte for byte. Windows placement/undo and
package loading have automated coverage; interactive editor and Linux validation
remain open. The primary performance workload has not been rerun for this change.

This does not complete milestone 2: stable sliced subassets, atlas packing,
slicing tools, sprite animation, nine-slice/tiled drawing, sampler controls,
pixel snapping and sprite materials remain open. Scene compositing and the
remaining renderer contracts, international text, canvases/masks and tilemaps
also remain open. Vulkan allocation tuning stays deferred.

### Atlas and sprite animation increment on September 19, 2026

`.cwatlas` sources now select existing Sprite UUIDs, a power-of-two page size,
and a mip count. Packing is deterministic, uses multiple pages, separates sRGB
and linear images, and retains pivots, original dimensions and border metadata.
It does not trim or rotate entries. Every sprite rectangle is extruded and
filtered separately at each configured mip level. Invalid input, missing
sources, oversized rectangles and page/memory limits fail explicitly without
replacing the previous packed result. Source regions must align to image pixels.
The authoring path reads RGBA pixels or decodes retained/cooked Basis payloads.

`SpriteAtlas` embeds its page textures in the cooked asset. The optional
`SpriteRendererComponent.Atlas` resolves the original Sprite identity to an
entry; missing atlases or entries suppress drawing. Repacking retains scene
and animation identities. Scene loading and atlas animation avoid loading the
original sprite textures merely to select an atlas entry. Scene format 17 adds
the atlas reference; formats 12–16 remain readable.

`.cwspriteanim` sources contain Sprite UUIDs, positive frame durations and
loop/once/ping-pong behavior. `SpriteAnimatorComponent` advances in scene
simulation between script Update and LateUpdate, independently of visibility
or camera extraction. It supports speed (including reverse), play/pause/stop,
seek, frame queries and consumable completion counts. Large time steps skip
cycles arithmetically. Frame sampling uses cached cumulative durations; looping
non-atlas playback retains resolved frame assets. Copies reset playback, while
ECS moves preserve it. Simulation start/stop resets runtime state.

The asset browser creates both asset types. Inspectors provide atlas source
selection, page previews and repacking, plus animation frame editing, ordering,
duration controls and playback preview. Edits use retained undo and existing
pending-save handling. Viewport drops create atlas sprites or animated sprite
entities with placement and undo. Native/managed component and asset access,
scene/prefab persistence, duplication, play isolation, source import, cooked
serialization and build dependency discovery are connected. Managed ABI 22
currently exposes 588 shared functions. Completion notifications are polled
through `ConsumeCompletions()` rather than dispatched as callbacks.

Validation: the Release All build passes. The focused suite passes 73 cases
and 1,433 assertions; the full native suite passes 1,007 cases and 114,797
assertions. Three optional cases are skipped: the two installed-OSL-compiler
fixtures and the published CoreCLR program package. All 39 Vulkan tests, 39
OpenGL tests and 39 backend capture comparisons pass without reference updates.
Sprite captures were visually inspected. The renderer regression builds an
atlas, imports its source description, saves/loads its embedded cooked pages,
then checks color and picking through CPU baseline, GPU compaction and the
compatibility renderer. Missing atlas entries suppress color and picking.

Managed generation/parity (588 functions), added-header checks, native formatting
and all 65 tooling tests pass. Logs use
`artifacts/2d-expansion/atlas-animation-*`. The pre-existing `complex_scene.yaml`
was restored byte for byte. The primary performance workload has not been rerun.
Windows editor placement/undo and cooked loading have automated coverage;
interactive editor and Linux validation remain open. The final All build
(`atlas-animation-build-staged.log`) passed and refreshed the editor/player
binaries and staged player template after the playback fix.

This increment does not complete the 2D roadmap. Automatic/grid slicing and
stable sliced subassets, trimming, nine-slice/tiled drawing, sampler controls,
pixel snapping, sprite materials, scene compositing, international text,
canvases/masks and tilemaps still require work. Source changes can be incorporated
with Repack Sources; automatic dependent-atlas reimport is not implemented here.
Vulkan allocation tuning remains deferred at the user's request.

## Contracts

- Renderer-owned persistent generational 2D instances, immutable snapshot changes,
  retained per-view scratch, dirty-range GPU uploads, and frame-safe retirement.
- One ordered stream for sprites, text, circles, lines, and tiles. Batch adjacent
  compatible items only. Merge world 2D and strict 3D transparency using layer,
  order, back-to-front coordinate, and stable identity. Preserve the existing
  separate weighted-OIT group; do not expose OIT for sprites/UI.
- World opaque/cutout content participates in depth, IDs, and motion vectors.
  World transparency precedes post-processing with reactive TAA coverage.
  Screen canvases and editor icons follow post-processing. World content may
  explicitly bypass post-processing while retaining scene-depth testing.
- Linear-light premultiplied compositing, capability-checked bindless access,
  bounded texture-table fallback, instancing on Vulkan and OpenGL, no dropped
  items at batch/capacity boundaries, no visibility readback for GPU submission.
- Scene synchronization detects direct native component writes. Moving text does
  not reshape it. Unchanged objects retain GPU records. Cameras do not advance
  animation or invalidate another view's retained data.

## Milestone 1: renderer and pipeline

- [x] Persistent RenderWorld2D and immutable create/update/destroy changes.
- [ ] DrawList2D recording interface, ordered adjacent batching, shared unit quad
  instances, renderer-owned contexts, and legacy Renderer2D adapters.
- [ ] Retained radix ordering, sorting groups, custom-axis sorting, bounds culling.
- [ ] GPU stable visibility compaction/indirect instancing and CPU fallback.
  Sprite paths are implemented; shared-stream integration remains.
- [ ] Explicit render-graph passes, mixed 3D transparency, depth/ID/velocity,
  reactive TAA, and no unused 3D resources for 2D-only views.
- [ ] Per-view batches/break reasons, instances, glyphs, bytes, cache, and timings.

## Milestone 2: sprites

- [x] Individual Sprite assets: texture regions, original size, pivot,
  100 pixels/unit and border metadata, with editor, persistence and managed access.
- [ ] SpriteAtlas assets, stable sliced subasset identities and slice reimport.
- [ ] Full-image, manual/grid/alpha slicing; deterministic multi-page packing,
  compatible sampling groups, trimming, extruded borders and isolated mipmaps.
  No rotated packing; do not trim nine-sliced images.
- [ ] Simple/nine-sliced/tiled drawing, nearest/linear sampling, opt-in camera
  pixel snapping and integer scaling.
- [ ] Existing material/shader workflow with versioned 2D position/coverage
  contract across color, depth, motion, clipping and picking. Unlit now, extensible
  UV/material/world metadata for later normal maps and 2D lighting.
- [ ] SpriteAnimationClip and SpriteAnimatorComponent: frame durations, loop,
  once, ping-pong, speed, pause, seek and completion notification in simulation.

## Milestone 3: international text

- [ ] Pinned HarfBuzz shaping and ICU bidi, script/grapheme/line analysis, with
  Windows/Linux dependency tooling and packaged ICU data.
- [ ] Cluster-safe fallback and contextual reshaping at line boundaries; UTF-8
  source mapping and explicit C# UTF-16 conversion; caret affinity and selections.
- [ ] Typed style spans and owned measurement/hit-test results. Preserve existing
  wrapping, alignment, sizing, outlines, shadows, and decorations.
- [ ] Separate shaping/layout/paint/atlas caches; immutable font revision keys.
- [ ] Versioned font-face data and glyph-ID MSDF pages; worker-local generation,
  render-thread upload, stable handles, bounded caches and deferred retirement.
- [ ] Scene/localization prewarming and readiness; cold glyph replacement images
  retain shaped advances without blocking rendering.

## Milestone 4: canvas and masks

- [ ] Screen-overlay/world Canvas2DComponent, explicit output/camera, top-left
  logical coordinates, pixel/reference scaling, world pixels/unit.
- [ ] Retained images, text, nine-slice panels, circles, rounded rectangles, lines.
- [ ] Clip2DComponent: rectangular scissor intersections, transformed/rounded
  rectangles, alpha textures, and line/quadratic/cubic paths with holes and
  even-odd/nonzero fill rules.
- [ ] Immutable hierarchical clips; cached antialiased bounds-sized coverage
  targets from existing pools. Empty masks suppress draws; failures never render
  unclipped content. Shared color and picking coverage.
- [ ] Geometry hit tests and optional asynchronous rendered-ID queries.

## Milestone 5: tilemaps

- [ ] TileSet/TileMap assets and TileMapComponent: signed sparse coordinates,
  32x32 chunks, orthogonal/diamond-isometric layouts, layers, cell tint/transforms.
- [ ] Dirty chunk rebuilds, oversized bounds, shared instances, individual-tile
  Y ordering where needed, shared animation tables without rebuilding chunks.
- [ ] Palette painting, erase/rectangle/flood fill, selection/copy/paste,
  eyedropper, undoable strokes and deterministic edge/corner terrain rules.
- [ ] Optional chunk-owned static polygon collision via the backend-neutral
  physics interface, concave decomposition and safe-point rebuilds. Callbacks
  retain tilemap entity ownership; rendering never requires physics.

## Milestone 6: workflows and compatibility

- [ ] Integrate the existing SceneGizmos implementation: logical-pixel clickable
  category icons, selected depth-tested frustums/ranges/audio guides, x-ray option,
  consistent projection/picking, DPI, hidden entities and gizmo input priority.
- [ ] 2D viewport, slice/atlas/animation previews, tile palette, fallback diagnostics,
  asset-browser/file placement, scene/prefab persistence and undo/redo.
- [ ] Generated native/managed interfaces for every asset/component, including
  bulk sprite/tile updates; play isolation, dependencies, cooking/player builds.
- [ ] Legacy texture sprites retain full-texture unit quads, sorting and
  post-processing bypass. New sprites use scene compositing, centered pivots,
  premultiplied transparency and linear sampling.
- [ ] Reimport legacy fonts when source exists; retain atlas-only compatibility
  with a diagnostic otherwise. Remove the global batch after callers migrate.

## Acceptance

- [ ] Unit/regression coverage for ordering, overflow, texture switching,
  lifetime, multiple views, direct writes, migration, managed parity and packaging.
- [ ] Vulkan/OpenGL visual coverage for mixed 3D, alpha, IDs, reverse-Z, motion,
  TAA, clipping/path holes, text effects, pixel art, and DPI/resolution changes.
- [ ] Text cases: Latin/Cyrillic/Arabic/Hebrew/Indic/Thai/CJK, combining marks,
  ligatures, malformed UTF-8, fallback, bidi selections, wrapping and ellipsis.
- [ ] Tile coordinate/projection/chunk/order/terrain/animation/collision/undo tests.
- [ ] Primary benchmark: Release standalone, Iris Xe, 1920x1080, VSync off,
  100,000 visible ECS sprites updated every frame, 8x8 output pixels each,
  four atlas pages, premultiplied alpha, fixed seed, 300 warm-up + 1,800 measured
  frames, p95 <= 16.67 ms. A miss remains unfinished work.
- [ ] Measure 20k cached glyphs, changing text, custom-axis sorting, fragmented
  materials, heavy overdraw, and a million-cell sparse map; report CPU/GPU times,
  allocations, upload bytes, batches, occupancy and cold-resource stalls.
- [ ] Zero warm renderer allocations, unchanged text reshapes and unchanged
  instance-content uploads. OpenGL/baseline correctness and bounded memory.
- [ ] Full native tests, Vulkan/OpenGL captures with visual inspection, managed
  and tooling tests, header checks, native formatting, Windows editor/player and
  Linux validation. Use Tools/crowny entrypoints; batch build/check work.

Deferred: 2D lighting/shadows, skeletal sprites, SVG documents, hex grids,
navigation baking, color emoji, variable-font controls, vertical text, and UI
widgets/layout/input/focus/IME/accessibility. Typed text spans do not imply markup.
