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
graph. These paths, full scene-pass integration, GPU
visibility compaction, and the remaining milestone 1 contracts are unfinished.

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

- [ ] Persistent RenderWorld2D and immutable create/update/destroy changes.
- [ ] DrawList2D recording interface, ordered adjacent batching, shared unit quad
  instances, renderer-owned contexts, and legacy Renderer2D adapters.
- [ ] Retained radix ordering, sorting groups, custom-axis sorting, bounds culling.
- [ ] GPU stable visibility compaction/indirect instancing and CPU fallback.
- [ ] Explicit render-graph passes, mixed 3D transparency, depth/ID/velocity,
  reactive TAA, and no unused 3D resources for 2D-only views.
- [ ] Per-view batches/break reasons, instances, glyphs, bytes, cache, and timings.

## Milestone 2: sprites

- [ ] Sprite/SpriteAtlas assets: stable slice IDs, texture regions, original size,
  pivot, 100 pixels/unit, nine-slice borders, and reimport identity preservation.
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
