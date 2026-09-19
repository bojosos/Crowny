# Live procedural material inputs

Crowny's first procedural material input is opaque PBR base color. It evaluates a texture function in the fragment shader, multiplies the result by the existing albedo image and tint, then runs the existing PBR lighting. No procedural image is generated or uploaded during rendering.

The interface belongs to the material system. OSL and handwritten GLSL both implement it. Images remain `Texture` resources; a compiled procedure is shader code, not a new kind of image descriptor.

## Writing a GLSL source

Implement `vec4 cwEvaluateProceduralTexture(CwTextureContext context)` in a `.glslinc` file. The context supplies transformed UVs, explicit UV derivatives, world position and its derivatives, world normal, and time. The definitions live in `Crowny-Editor/Resources/Shaders/CrownyProceduralTexture.glslinc`.

The source can declare its own uniform blocks and sampled images at set 0, binding 14 and above. The reflected fields are ordinary `Material` parameters. The checker example in `Tools/procedural/fixtures/checker.glslinc` exposes two colors and a frequency. A GLSL noise implementation uses the same function and needs no OSL dependency or engine changes.

Cook the source with the Vulkan SDK tools:

```powershell
python Tools/procedural/compile.py Tools/procedural/fixtures/checker.glslinc artifacts/procedural/checker --spirv-bin C:/VulkanSDK/1.4.357.0/Bin
```

The cook compiles the existing `Pbribl.glsl` template with the procedure, optimizes both stages, and validates them for Vulkan 1.3. It publishes `material.vert.spv`, `material.frag.spv`, and `material.interface.spv` after validation. The interface file preserves the original fragment module's reflection metadata. It is never executed. It is an offline, Windows-tested tool; LLVM is only needed for OSL.

## Material instances

Load a cooked package directory, then use the same program for all instances of that compiled source. This also loads optional parameter defaults and inspector annotations:

```cpp
ProceduralMaterialProgram program;
if (!program.Initialize(packageDirectory, error))
    return;

auto material = program.CreateMaterial();
material->SetColor("proceduralUVTransform", glm::vec4(2, 2, 0, 0));
material->SetFloat("proceduralTime", elapsedSeconds);
material->SetFloat("proceduralAmount", 1.0f);
material->SetColor("checkerColorA", glm::vec4(0.05f, 0.2f, 0.8f, 1));
material->SetColor("checkerColorB", glm::vec4(0.9f, 0.5f, 0.1f, 1));
material->SetFloat("checkerFrequency", 8.0f);
material->SetFloat("roughness", 0.5f);
material->SetTexture("albedoMap", imageTexture);
```

Assign the resulting material to a mesh through the existing asset handle/material assignment path. Instances share a shader and graphics pipeline but own their parameter buffers. Time is explicit and must be updated by the caller. Updating time, UV transform, checker colors or frequency does not recompile the shader. Changing procedural source code does.

`proceduralAmount=0` selects the image contribution alone. At 1, the procedure multiplies the image. The default image is white, so the procedure supplies the base color. UV transform affects procedural coordinates; the existing image uses the mesh's original UVs. Derivatives are computed after transforming the UVs and before divergent material control flow.

The runtime accepts compiler-validated modules and checks the shared parameter layout. Failed initialization preserves the previously loaded program. `ShaderCompiler::LoadSpirv` checks stage/entry point and reflects resources; it is not a replacement for SPIR-V validation.

## OSL adapter

`Tools/osl/compile.py` also cooks these material stages. It generates `MaterialTexture.glslinc` in the package, mapping UVs, time, derivatives and typed uniforms to `crowny_osl_evaluate_material`. The offline cook replaces a controlled GLSL stub with a SPIR-V import, adapting GLSL pointer arguments to LLVM value arguments. SPIR-V Tools links and inlines the evaluator before Vulkan sees the module. There is no runtime shader function lookup or OSL interpreter.

OSL supports the subset documented in [OslGpuTextures.md](OslGpuTextures.md). Up to 32 inputs of type float, int, color, point, vector or normal become editable uniforms. Each occupies a 16-byte slot in `OslInputs` at set 0, binding 14. Reflection retains the actual scalar or triple type. Integers cross the evaluator boundary by bitcast, preserving negative values. Engine identifiers are `osl_<sourceName>` to avoid collisions with PBR fields; the inspector shows the OSL label or source name. Constant defaults and `label`, `min`, `max` metadata are retained. Arrays, structures, general strings, matrices, closures, space-qualified defaults and computed defaults are rejected with a diagnostic. String inputs used exclusively as texture asset slots are described below. The richer shared context still does not imply OSL position/normal global support.

OSL uses preplaced userdata locations for uniform inputs and sets their derivatives to zero. The adapter converts OSL's integer pointer-offset expressions back to byte GEPs before scalar replacement, then rejects any surviving host addresses. The two-argument compute entry remains a default-value wrapper for differential tests.

## Asset editor workflow

### OSL image inputs

Declare a string input for each 2D image asset, then call `texture()` with that input:

```c
shader image_color(
    string image = "" [[ string label = "Image" ]],
    float scale = 1,
    output color Cout = 0)
{
    Cout = texture(image, u * scale, v * scale);
}
```

Import the shader, select it on a material, and assign an imported texture asset to **Image** in the material inspector. The input uses the existing texture picker, undo, material save/load and asset dependency handling. Each material instance owns its texture assignments. Shader reimport preserves assignments with matching names/types. Clearing an input restores a black texture.

Texture inputs use `osl_<name>` identifiers and reflected `sampler2D` bindings at set 0, bindings 15–22, with at most eight inputs. The compiler lowers each OSL lookup to a resource-specific value-only function. The material linker supplies that function using GLSL `textureGrad`. Coordinate derivatives from OSL determine the mip footprint, including inside varying control flow. OSL and LLVM are absent at runtime.

Supported calls return color or float, use implicit or explicit coordinate gradients, and can write the optional `alpha` output. Float results read red; their optional alpha reads the next channel, matching OSL's channel convention. GPU sampling uses Crowny's sampler, currently linear filtering and repeat addressing by default. Texture import settings control sRGB decoding. This does not reproduce OpenImageIO's filtering exactly.

Inputs represent asset slots, so their defaults must be empty and they may only occur as the filename argument of `texture()`. Literal filenames, nonempty filename defaults, dynamic filename expressions and string operations are rejected. Automatic path-to-asset resolution is not implemented. Nondefault texture options such as blur/wrap overrides, derivatives of the sampled result, `errormessage`, UDIMs, environment/3D lookups and texture queries remain unsupported. Unsupported lowered option/service calls fail compilation; OSL itself can elide options equal to its defaults.

The `image_texture.osl` fixture compares with a handwritten GLSL material using two image assets with distinct mip levels. It covers UV transforms, implicit/explicit gradients, a varying branch, RGB/red/alpha reads, independent instance assignments, shader reimport, shader serialization, material save/load and clearing an input.

### Import workflow

Build the offline tools with `python Tools/crowny deps osl`, then set `VULKAN_SDK`. See [OSL compiler setup](OslToolchain.md) for dependency acquisition and overrides. The editor finds `Tools/osl/editor-import.py` when launched from the repository or editor directory. Otherwise set `CROWNY_OSL_IMPORT_SCRIPT` to that script's absolute path before launching the editor. `CROWNY_PYTHON` can select the Python executable. Tool setup is explicit; asset import never downloads dependencies or builds OSL.

1. Create **OSL Texture** in the asset browser, or copy an `.osl` file into the project assets.
2. Select the source to choose its color output in the import inspector. The default is `Cout`. Apply recompiles source; failed import preserves the last successful asset.
3. Create or select a material, choose the imported shader, and edit its OSL inputs in the material inspector. Existing preview, reset, undo and save behavior applies. Assign the material to a mesh as usual.

The output selection is persisted in the existing shader import define map as the reserved `CROWNY_OSL_OUTPUT` option. OSL import currently uses no other shader defines. Import produces a normal Shader asset with executable SPIR-V, reflected defaults and annotations. The runtime needs neither the package directory nor the OSL/LLVM installation. Material YAML stores each instance's typed values. Shader reimport replaces material pipelines while preserving matching names and types; new parameters receive source defaults.

The OSL importer supports worker scheduling for asset scans. Its compiler process has a two-minute timeout and captures diagnostics in the editor log. Import settings Apply uses the library's existing synchronous reimport path. Include-file edits currently require manually reimporting the root `.osl` file; there is no OSL include dependency watcher yet.

SPIR-V linking merges structurally identical block types. Distinct GLSL blocks can consequently share a type name and member names in the executable module. Crowny retains reflection from the original interface module instead, preserving the source names and descriptor bindings. A production package should serialize this metadata compactly rather than retaining an extra module.

## Rendering scope

These programs use the Vulkan GPU scene renderer's existing custom opaque forward pass. They write linear HDR color and use reverse depth, so scene tone mapping runs once and opaque depth ordering is preserved. Direct lighting currently inherits the compatibility PBR shader's four selected lights. This is not yet a generated variant of the GPU-driven Forward+ or Deferred+ material shaders.

The first input is RGB base color. Keep materials opaque. Procedural opacity, normal/displacement outputs, shadow/depth material evaluation, picking and velocity outputs, graph connections, include tracking and compiler caches need further integration. The asset workflow selects a procedural material shader; it does not add a procedural picker to existing image slots.

For more inputs, extend the material evaluation result with roughness, metallic, emission and normal outputs while retaining the context and source adapters. Closures need a separate result representation and renderer evaluation/sampling. A texture color must not become an implicit closure format.

## Validation

### Chaos Mandelbrot example

`Tools/osl/fixtures/mandelbrot.osl` preserves the shader from [Chaos OSL Support](https://documentation.chaos.com/space/VMAX/113575764/OSL+Support), with attribution added. Its output is named `result`, so select that output when importing it. The background color, fractal color and iteration count appear as editable material inputs. The source compiles unchanged, including its `while` loop and parameter-dependent output initializer.

To cook and capture it on a face-on plane:

```powershell
python Tools/crowny osl compile Tools/osl/fixtures/mandelbrot.osl artifacts/osl/mandelbrot --output result
Scripts/crowny.bat build RenderTests
bin/Release-windows-x86_64/Crowny-RenderTests/Crowny-RenderTests.exe --backend vulkan --procedural-preview artifacts/osl/mandelbrot --artifacts artifacts/osl/mandelbrot/capture
```

The capture is `artifacts/osl/mandelbrot/capture/vulkan/procedural-preview/plane.bmp`. It renders the procedural PBR material directly on two triangles at 1400x800, with UVs spanning [0, 1] and source defaults of black/white and 40 iterations. The aspect ratio preserves the shader's 3.5 by 2 complex-coordinate domain. There is no texture bake. This capture command also accepts other procedural packages using their default parameters.

On Intel Iris Xe, the Mandelbrot compute test matched all 7,215 CPU samples exactly. The live PBR test also matched all 7,215 reference pixels exactly, including edited colors/iteration counts, serialization and shader reload. The fixture is included in `python Tools/crowny osl test`.

The plane capture was visually checked on Windows. Vulkan validation reported no errors, with the existing unused object-ID output warning. The RenderTests build, header checks and formatting checks passed. The native executable passed 997 cases with two skipped, and the renderer suite passed 39 Vulkan, 39 OpenGL and 39 cross-backend comparisons. Logs are under `artifacts/osl/mandelbrot`. A PNG copy of the capture is beside the BMP for inline viewing.

### Regression commands

Run the GLSL-only end-to-end test without an OSL installation:

```powershell
python Tools/crowny osl test --procedural-only
```

`python Tools/crowny osl test` exercises OSL compute evaluation, live PBR materials for color and derivative outputs, the GLSL checker, and unsupported OSL operations. Add `--no-build` after building the renderer test executable.

Normal `compile.py` and editor imports emit shader code and metadata only. They never evaluate a sample grid or produce reference images. `Tools/osl/Reference.cpp` builds the separate `crowny-osl-reference` executable used only by tests. Passing `--test-reference-generator` with its path explicitly requests CPU comparison data and, for shaders without image inputs, the compute test module. The OSL runner passes this flag. Image-input tests compare live GPU sampling with GLSL because the material's image assets and sampler settings belong to Crowny.

The material test compares 65x37 pixels at three times with CPU OSL or checker values passed through the same PBR shader. OSL input values change between frames. It covers repeated/negative UVs, raster derivatives, image multiplication, tint, roughness, procedure blending, independent instance edits, shared pipelines and failed reloads. It also checks shader asset serialization, edited material save/reload and shader replacement while retaining values. Two spheres render through `SceneRenderer`, and both must be visible. Reports and previews appear under each package's `surface/vulkan/procedural` directory in the OSL runner, or `artifacts/procedural/checker/render/vulkan/procedural` for the GLSL-only runner.

### Image-input milestone, 2026-09-19

The complete OSL runner passed 36,075 compute comparisons and 50,505 live material pixel comparisons, including Mandelbrot and the new image-input fixture. All scene checks and unsupported-operation checks passed. The image fixture compares OSL against handwritten GLSL using two distinct mipmapped RGBA assets, swapped between material instances, with repeated coordinates, implicit and explicit gradients, alpha, scalar reads and sampling inside a varying branch. Its maximum absolute color error was 0.000227243 against a 0.0005 tolerance; the other material fixtures retain their 0.0002 tolerance. The two-sphere image capture was visually inspected on Windows with Intel Iris Xe.

The RenderTests build passed. Five focused OSL cases passed 63 assertions. The full native suite passed 114,831 assertions across 1,009 passing cases, with one case skipped. Renderer regressions passed 39 Vulkan cases, 39 OpenGL cases and all 39 cross-backend comparisons. No Vulkan validation errors appeared in the OSL log; the existing unused object-ID output warnings remain. Formatting, header and Python syntax checks passed. The material inspector workflow was not manually clicked; automated checks cover texture reflection, labels, black defaults, assignment, serialization, reload and rendering.

A normal Mandelbrot compilation was also checked separately: it produced neither reference `.bin` files nor `evaluate.spv`. CPU reference evaluation now lives exclusively in the explicit test generator. Logs are under `artifacts/osl`: `texture-validation.log`, `texture-unit-tests-final.log`, `texture-render-regression.log`, `texture-render-build-final.log` and `reference-split-check.log`. Image comparison reports and captures are under `artifacts/osl/image-texture/surface/vulkan/procedural`.

### Editable-parameter milestone, 2026-09-19

Windows, Intel Iris Xe Graphics, with `VK_LAYER_KHRONOS_validation` requested. The final complete OSL runner returned zero with no validation-error diagnostics. Unconsumed object-ID output warnings remain when a test target has only a color attachment.

| Live PBR source | Compared pixels | Maximum absolute color error | Failing components |
| --- | --- | --- | --- |
| OSL color, Perlin noise and branch | 7,215 | 0.0000128746 | 0 |
| OSL trigonometric derivatives/filterwidth | 7,215 | 0.0000168681 | 0 |
| OSL Perlin derivatives/filterwidth | 7,215 | 0.000000685453 | 0 |
| OSL editable float/int/color/point/vector/normal | 7,215 | 0.0000123382 | 0 |
| GLSL checker with instance uniforms | 7,215 | 0.000000119209 | 0 |

All five cases also passed their two-sphere scene checks. All 28,860 OSL compute samples passed. These results cover the fixtures on this device and are not a performance measurement or a general OSL compatibility guarantee.

The final All build passed. Focused OSL tests passed 39 assertions in four cases, including the actual source importer with spaces in its path, reflected defaults/labels/ranges, output selection persistence, and an invalid selected output. Renderer regression tests passed all 39 cases on Vulkan and 39 on OpenGL, plus all 39 cross-backend comparisons.

The full native executable ran 999 cases: 997 passed, one skipped, and one unrelated existing sprite test failed 12 assertions. `Sprite edits and source imports reject invalid data transactionally` accepts malformed `UvRect: [1, 2]` and clears the sprite name. This OSL change does not modify that serializer. Logs are under `artifacts/osl`: `parameter-final-build.log`, `parameter-focused-tests.log`, `parameter-unit-tests.log`, `parameter-validation-final.log`, and `parameter-regression.log`.

Validation platform was Windows with Intel Iris Xe. The editor UI compiled successfully; native inspector clicks were not manually exercised. The asset/editor workflow above is the manual follow-up, while automated tests cover its import, reflection, persistence and rendering paths.

The integration exposed two engine issues. `Material::ReloadParams` rebuilt a pipeline per instance; it now reuses an existing pass pipeline while allocating independent parameter buffers. Vulkan's fallback cubemap initialized only its first face; it now uploads every face so all subresources exposed by its view have a readable layout. The linked OSL module additionally exposed the uniform-name merging described above, which the retained interface metadata resolves.
