# OSL GPU textures: first implementation

Crowny evaluates procedural OSL on Vulkan compute and inside opaque PBR fragment shaders. Compilation happens offline; GPU evaluation receives UVs, time and their derivatives. It does not bake an image on the CPU. The shared material interface also supports handwritten GLSL procedures. OSL asset import and editable material parameters are described in [ProceduralTextures.md](ProceduralTextures.md). Material graph connections and ray-tracing stages remain subsequent work.

## Compiler path

1. Upstream `oslc` compiles `.osl` into `.oso`.
2. An OSL shading system optimizes the selected color output and generates its normal LLVM group code. A two-line upstream patch exports complete bitcode, including types and metadata.
3. `Tools/osl/Compiler.cpp` wraps the group with value arguments for shading context and editable numeric inputs, seeds shader globals and preplaced userdata, and links a small shadeop bitcode library. Perlin noise comes from upstream OSL's scalar implementation. LLVM inlining and scalar replacement remove the CPU group-data, userdata and shader-global pointer layouts.
4. The adapter rejects remaining host pointers, runtime globals and unresolved shadeops. It concretizes undefined constant lanes to avoid an LLVM 20 Vulkan code-generation issue.
5. LLVM emits a logical SPIR-V evaluator library. SPIR-V Tools links it into the GLSL PBR material, including texture sampling functions for image inputs. The final module must pass `spirv-val --target-env vulkan1.3`. Tests can also request a compute wrapper for shaders without image inputs.

This uses the LLVM 20 logical SPIR-V target already installed in the experiment. It does not require LLVM's newer Vulkan target spelling, translate generated C++ text to GLSL, or ship LLVM and OSL DLLs with the engine.

The adapter deliberately targets one shader layer and one color output. Its use of CPU bitcode plus scalar replacement is a feasibility implementation, not yet the final device ABI. Unsupported operations produce compilation errors. A material that needs an unavailable renderer service must not silently use a default value.

## Runtime boundary

`OslTextureProgram` consumes a validated compute module and two structured GPU buffers. Sample ABI v1 is 32 bytes:

| Field | Values |
| --- | --- |
| First float4 | u, v, time, reserved |
| Second float4 | dudx, dudy, dvdx, dvdy |
| Output float4 | selected color output, alpha = 1 |

The compute wrapper uses set 0, bindings 0 and 1, and 64x1x1 workgroups. It bounds-checks the last workgroup. The runtime checks bindings, strides, workgroup size and buffer sizes. `ShaderCompiler::LoadComputeSpirv` reflects already validated compiler output; it is not a general SPIR-V validator.

The evaluator itself has no compute built-ins or descriptor access. The material cook now links the same evaluator into the PBR fragment stage. A future ray-tracing caller would use another wrapper.

Initial supported examples cover scalar/color arithmetic, a varying branch, sine/cosine, explicit derivatives, filterwidth and scalar three-dimensional Perlin noise, including noise derivatives. Material inputs support float, int, color, point, vector and normal with constant defaults. Declared string inputs with empty defaults become 2D image asset slots for `texture()`; see [image input semantics and limits](ProceduralTextures.md#osl-image-inputs). Other OSL operations may work if they reduce to supported LLVM instructions, but they are not a compatibility promise. Other noise families, renderer queries, general strings, multiple layers and closures remain unsupported. The compute comparison uses default parameters; the fragment material receives editable per-instance uniforms and textures.

## Reproduce

Prerequisite: the optional compiler installed by `python Tools/crowny deps osl` and the Vulkan SDK. See [OSL compiler setup](OslToolchain.md) for pinned source acquisition, dependency locks and platform status. The original [C++ experiment](research/osl-cpp-experiment.md) is historical evidence and is no longer required for setup.

```powershell
python Tools/crowny osl test
```

Use `--no-build` only when Crowny-RenderTests has already been rebuilt. `python Tools/crowny osl compile --help` describes compiling a different OSL source/color output. The Python runner selects host executable names; native execution is currently validated on Windows.

Normal compilation produces OSL bitcode, the lowered evaluator, linked and optimized material SPIR-V, and reflected input metadata. It performs no CPU image evaluation. Test-only sample inputs and CPU colors come from the separate `Tools/osl/Reference.cpp` executable, requested explicitly with `--test-reference-generator`. The renderer writes comparison images, raw GPU colors and `comparison.txt` under `<package>/gpu/vulkan/osl`. None of this test data is loaded by runtime materials.

The comparisons use 65x37 samples at three times, negative and repeated UVs, and skewed derivatives. They poison the output before every dispatch and allow a maximum absolute component error of 0.00002. Color, trigonometric derivatives and Perlin derivatives are separate cases. Negative cases check unsupported globals and shadeops. CPU unit tests cover SPIR-V loading and reflection.

## Measured result

The subsequent image-input milestone passed the complete OSL runner, including live image sampling against a handwritten GLSL reference, material texture persistence and shader reload. It also passed the full native suite (1,009 cases passed, one skipped) and all renderer regressions. Normal compilation was verified to emit no CPU sample files or compute test module. See the [image-input validation record](ProceduralTextures.md#image-input-milestone-2026-09-19) for tolerances, counts and logs. The measurements below record the earlier editable-parameter milestone.

Validated on Windows on 2026-09-19 with Intel Iris Xe Graphics, driver 101.7085. The run requested `VK_LAYER_KHRONOS_validation`; no validation-error diagnostics appeared in its captured output. All final modules also passed SPIR-V Tools' Vulkan 1.3 validator.

| GPU comparison | Samples | Maximum absolute component error | Failures |
| --- | --- | --- | --- |
| Procedural color, Perlin noise and varying branch | 7,215 | 0.000017792 | 0 |
| Sine/cosine derivatives and filterwidth | 7,215 | 0.000017643 | 0 |
| Perlin derivatives and filterwidth | 7,215 | 0.000000581145 | 0 |
| Editable-input fixture, compute defaults | 7,215 | 0.0000107288 | 0 |

The GPU math path is not bit-identical to the CPU runtime. These results cover the fixture inputs on this device, not arbitrary coordinate magnitudes or other GPU vendors. No performance claim is made.

The editable-parameter milestone passed four focused OSL tests and all GPU comparisons. The full native run passed 997 cases, skipped one and failed one unrelated sprite serialization case; see [the detailed validation record](ProceduralTextures.md#validation). The renderer suite passed all 39 cases on Vulkan and all 39 on OpenGL; all 39 cross-backend comparisons passed. Formatting and header checks passed. The historical `Scripts/run-render-tests.ps1` named in AGENTS.md is absent; validation used the current `Scripts/crowny.bat render-tests --no-build` entry point.

Current logs and numeric reports are under `artifacts/osl`: `parameter-validation-final.log`, `parameter-unit-tests.log`, `parameter-focused-tests.log`, `parameter-regression.log`, and each package's `gpu/vulkan/osl/comparison.txt`. The full reproduction script completed with exit code zero.

## Next steps and material roadmap

1. Expand the differential test corpus and parameter binding before promising a supported OSL subset. Add proper package metadata, dependency hashes and compiler caching.
2. Extend the implemented 2D texture service with filename-to-asset resolution, more sampling options and result derivatives. Current inputs bind engine texture assets and use GPU explicit-gradient sampling. GPU filtering does not match OpenImageIO bit-for-bit.
3. Add shader-space position and its derivatives, transforms, normals and renderer attributes to a versioned shading context. These are required for useful 3D procedural textures and later materials.
4. Extend the implemented asset import and opaque base-color material integration to graph connections, other material inputs, and GPU-driven material variants.
5. Support optimized multi-layer OSL groups and connections. Keep group compilation separate from whichever shader stage calls the evaluator.
6. Introduce a separate closure output ABI with closure IDs, parameter records and bounded per-sample storage. Register the renderer's supported closures and implement evaluation/sampling in the integrator. Do not treat the current float4 color output as a closure representation.

LLVM reuse remains promising if the growing device runtime can stay free of host services. Texture resource access, group memory that cannot be scalarized, and closure allocation are the next architectural tests. This milestone does not prove those parts or justify a broad OSL compatibility estimate.
