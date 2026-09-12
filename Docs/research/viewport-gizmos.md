# Viewport icon and gizmo research

Reviewed 2026-09-05 using Unity 6 documentation, Epic's viewport documentation, and Godot 4.4 source. The recommendations below are Crowny design choices, not claims that every engine uses identical behavior.

## What other editors do

Unity separates small component icons from wireframe gizmos. Its camera and light direction examples show the wireframes only for the selected object; its audio source has an adjustable spherical range. Icon visibility and gizmo visibility have separate controls per component type. [Unity gizmos introduction](https://docs.unity3d.com/6000.0/Documentation/Manual/gizmos-introduction.html)

Unity exposes both occlusion policies: enabling **3D Icons** scales icons with distance and allows scene geometry to obscure them; disabling it gives fixed-size icons drawn over scene objects. An icon size slider and small-gizmo fading address clutter. This makes occlusion a user choice rather than a universal rule. [Unity Gizmos menu](https://docs.unity3d.com/6000.0/Documentation/Manual/GizmosMenu.html)

Unreal uses editor billboard sprites to mark actor positions. Its Show menu separates sprite categories, including Lighting and Sounds, from Camera Frustums, Light Radius, and Audio Radius. Sprites can be hidden individually by category or all at once. This documentation does not establish one depth-test policy for every editor primitive, so it should not be cited as evidence that Unreal always overlays or always occludes icons. [Epic viewport show flags](https://dev.epicgames.com/documentation/en-us/unreal-engine/viewport-show-flags-in-unreal-engine)

Godot assigns distinct directional, omni, and spot light icons. Their billboards remain visible without selection, while selection adds a directional arrow, omni range circles, or a spot cone and range/angle handles. It brightens the light's hue for legibility. [Godot light gizmo source](https://github.com/godotengine/godot/blob/4.4/editor/plugins/gizmos/light_3d_gizmo_plugin.cpp)

Godot's camera gizmo includes a billboard, a wire representation appropriate to perspective, orthographic, or frustum projection, and collision segments for picking. It includes a small triangle that marks camera up. The perspective wire is a short orientation/FOV representation, rather than a drawing extending to the camera's full far plane. [Godot camera gizmo source](https://github.com/godotengine/godot/blob/4.4/editor/plugins/gizmos/camera_3d_gizmo_plugin.cpp)

Godot's 3D audio player always has an icon. Selection adds an attenuation circle and an emission cone when enabled. The source deliberately uses a billboard circle to distinguish audio from omni-light spheres; it also distinguishes estimated audibility distance from a configured hard cutoff. [Godot audio gizmo source](https://github.com/godotengine/godot/blob/4.4/editor/plugins/gizmos/audio_stream_player_3d_gizmo_plugin.cpp)

Godot's icon materials are unshaded, fixed-size billboards with alpha scissoring and dimmer unselected colors. Ordinary materials retain depth testing. The plugin's **ON_TOP** state explicitly disables depth testing for selected gizmos, and hidden gizmos are not selectable by default. Fixed screen size and depth testing are therefore independent choices. [Godot gizmo materials and visibility](https://github.com/godotengine/godot/blob/4.4/editor/plugins/node_3d_editor_gizmos.cpp)

## Recommended Crowny behavior

Use small, recognizable icons for components that have no visible scene geometry. Distinguish type by silhouette as well as color; a sun/direction symbol, bulb, spotlight, camera, speaker, and listener/headphones are enough for the currently relevant families. Use the existing editor icon system where possible.

Default icons to fixed screen size and overlay visibility: their main job is finding and selecting otherwise invisible entities, including ones inside walls. Expose a depth-tested option for dense scenes. Keep icon size separate from occlusion so changing occlusion does not change click-target size. Selected icons should stand out. When icons overlap, draw and pick consistently with the nearest candidate winning.

Show spatial guides only for selection:

| Component | Selected guide |
| --- | --- |
| Directional light | Direction arrow; no invented finite range |
| Point light | Three orthogonal circles at its actual range |
| Spot light | Outer cone/range and an inner cone when the engine exposes one |
| Camera | Frustum using the actual projection, aspect ratio, and clipping planes; mark forward/up |
| Spatial audio source | Reference/minimum distance and maximum-distance guides according to backend semantics |
| Audio listener | Forward/up direction, with no fabricated listening radius |

Prefer depth-tested world-space guide lines to preserve their relationship to geometry. A deliberate overlay option can expose obscured guides when needed. Never label an audio attenuation clamp as a silence boundary unless the backend actually stops sound there.

Provide one Gizmos visibility control, category toggles for lights/cameras/audio, and an icon size control. Keep rendering and picking tied to the same visibility decisions. Clip to the scene viewport, reject icons behind the view camera, and give existing transform handles priority over icon selection. These are editor aids and should not appear in the game's rendered output.

## Implemented in Crowny

The viewport's Gizmos menu controls all gizmos, lights, cameras, audio, selected-object guides, and icon size from 16 to 40 pixels. These controls last for the current editor session. Icons use fixed-size overlays; selected guides use the existing depth-tested line renderer. The initial implementation has no occlusion toggle.

Point lights use a bulb, directional lights a sun and arrow, spotlights a cone, cameras a video camera, audio sources a speaker, and listeners headphones. Disabled lights and muted sources appear gray and crossed out. Multiple component icons sit side by side. Overlapping entities draw farthest first and pick the nearest visible icon. Category switches affect drawing and picking together. Ctrl/Shift selection uses the existing hierarchy selection behavior. Play and paused Play hide these aids; simulation retains them.

Selected guides show point-light range and source radius, inner/outer spotlight angles and range, directional-light arrows, camera near/far frustums for both projections, audio minimum/maximum distance and directional cones, and listener direction. World positions and rotations follow parents; physical distances do not inherit entity scale. Audio maximum distance is an attenuation parameter, not a promised silence boundary. Wide audio cones use spherical geometry to avoid the tangent singularity at 180 degrees.

### Validation workflow

Automated coverage includes projection and clipping, both camera projections, wide audio cones, icon overlap ordering, category visibility, parent transforms, and multiple components on one entity. Build with `Scripts\crowny.bat build Editor`, run `Scripts\crowny.bat test`, and run `Scripts\crowny.bat render-tests`. The legacy `Scripts/run-render-tests.ps1` referenced by the repository instructions is absent in this checkout.

On September 5, 2026, targeted `Scripts/format.sh` formatting and `python Scripts/check_headers.py` passed. Build and runtime validation was initially delayed by shared build locks.

Validation resumed on September 8. The successful Release All build is recorded in `artifacts/material-completion-build.log`. The full native run in `artifacts/bistro/cold-validation-7/native.log` passed 950 test cases with one skipped and 113,374 passing assertions. The gizmo sources predate that run; the test project's `SceneGizmos.obj` and `SceneGizmoGeometryTests.obj` were built at 19:59 and 20:01, before the 20:02 native run. All five gizmo test cases use normal, non-hidden tags, so the full suite includes them. These completed results were reused instead of queuing another full rebuild.

The September 8 render results also pass `persistent-sprites`, `mixed-2d-order`, and `storage-buffer-bindings` on Vulkan and OpenGL under `artifacts/2d-expansion/integrated-*`. The OpenGL suite in `artifacts/material-final-suite-opengl/opengl/summary.json` passes its 12 non-decal cases, including `depth-output-matrix`. Its six decal cases lack references and belong to the separate decal task; this is not a fully passing render suite. These checks support the shared renderer but do not substitute for the viewport overlay check below.

Manual Windows/Vulkan validation completed on September 12 using the isolated project under `artifacts/gizmo-validation-20260908/Project`. Point, spot, directional, camera, source, and listener icons have distinct silhouettes; disabled lights and muted sources have crossed gray icons. Icon picking selected a point light behind an opaque cube. The Gizmos category toggle and size slider worked. Selected perspective and orthographic camera bounds, spotlight inner/outer cones, audio distance spheres, and listener direction rendered. Play and paused Play hid component icons and guides; simulation retained them. Overlap ordering and parent transforms have automated coverage rather than an additional manual check.

The first interactive run exposed missing Vulkan guide lines. Rebinding the viewport target did not fix them, and that probe was removed. `ToneMap.glsl` requested depth writes while disabling depth testing, which also disables Vulkan depth writes. Enabling its existing always-pass depth test preserves the scene depth for later editor drawing. The same camera screenshot check changed from zero to 687 visible guide pixels. The point-light range then disappeared behind the cube while its overlay icon remained visible and clickable.

Captures are `artifacts/gizmo-validation-20260908/camera-guide-fixed.png` and `artifacts/gizmo-validation-20260908/light-depth-occlusion.png`. The Release Editor build and shader cook passed in `depth-copy-build.log`. Targeted formatting and header checks passed. `primitive-lit-sphere` now reads the final viewport depth at the background and sphere center, checking the depth convention selected by the backend; this guards the actual scene-to-viewport depth copy without changing image references.

The final focused run passed on Vulkan and OpenGL, including their image comparison: `Scripts/crowny.bat render-tests --backend All --filter primitive-lit-sphere --jobs 2 --artifact-root artifacts/gizmo-validation-20260908/depth-regression-final`. Results are in `depth-regression-final.log` and each backend's `summary.json`. The first assertion assumed reverse depth on both backends; it was corrected to account for OpenGL's compatibility tier before this passing run.
