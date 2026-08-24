# Renderer Regression Tests

`Crowny-RenderTests` renders fixed 64×64 offscreen cases through the normal Crowny render API. Vulkan and OpenGL run in separate processes, compare against the same canonical BMP references, and are then compared directly. This catches backend-specific output changes without relying on renderer reinitialization in one process.

Run the complete Release suite from the repository root:

```powershell
Scripts\run-render-tests.ps1
```

Use `-Backend Vulkan`, `-Backend OpenGL`, or `-Filter fullscreen` while developing. Add `-NoBuild` when the executable is already current. Results are written to `artifacts/render-tests/<backend>/`; a failure retains the actual image, expected image, amplified diff, and `summary.json`.

## Reference updates

References live in `Crowny-RenderTests/References/` and are shared by every backend. After deliberately changing renderer output, inspect the actual captures and approve them with:

```powershell
Scripts\run-render-tests.ps1 -UpdateReferences -NoBuild
```

The script uses Vulkan to produce canonical references, then verifies Vulkan, OpenGL, and their direct comparison. Never update references merely to make a failing change pass.

## Adding coverage

Add a named case to `BuildCases()` in `Source/RenderTestRunner.cpp`. Render only deterministic inputs into an offscreen `RGBA8` target, submit before readback, and return a top-left-origin `Image`. Prefer exact comparison for clears and integer/nearest-sampled output. For filtered or floating-point effects, set the smallest justified channel, mean-error, and failing-pixel thresholds. Keep test dimensions, time, animation state, random seeds, and asset inputs fixed.

Good next cases are depth/stencil, blending, texture sampling, material variations, skeletal skinning, text, and a small serialized scene. Keep one reference per logical case; backend-specific references should be a last resort because they hide cross-API drift.
