# OSL C++ backend experiment and Crowny feasibility

Experiment completed on Windows x64 on 2026-09-19. The lab is at `C:/dev/osl-lab`, outside Crowny's source and dependency directories. No Crowny engine implementation was changed.

## Result

Built upstream OSL and executed shaders through both its LLVM CPU JIT and its generated C++ DLL backend. All 11 comparison scenarios passed: 15 image comparisons at an absolute threshold of 0.00002 with zero failing pixels allowed, plus two exact comparisons of printed closure structures. A detailed check measured a maximum pixel error of zero in every image pair. Four focused upstream unit tests passed. This demonstrates CPU execution and code generation; it does not demonstrate Vulkan, PTX, or GPU execution.

The experiment strengthens the case for evaluating LLVM-to-Vulkan before implementing a separate GLSL backend. The C++ backend exposes how much behavior depends on OSL's runtime, and reproducing that behavior in GLSL would be a substantial part of the work. An LLVM implementation might reuse more of it, but Vulkan-compatible resource access and memory representations remain unproven.

## Reproduction

- [Lab instructions](C:/dev/osl-lab/README.md)
- [Build script](C:/dev/osl-lab/build.ps1)
- [Environment setup](C:/dev/osl-lab/enter-env.ps1)
- [Experiment runner](C:/dev/osl-lab/run.ps1)
- [Exact dependency packages](C:/dev/osl-lab/dependencies-explicit.txt)
- [Machine-readable results](C:/dev/osl-lab/results/summary.json)
- [Detailed image differences](C:/dev/osl-lab/results/comparison-details.json)
- [Local source patch](C:/dev/osl-lab/windows-build.patch)

```powershell
& C:/dev/osl-lab/build.ps1
& C:/dev/osl-lab/run.ps1
```

For a small interactive command-line experiment:

```powershell
. C:/dev/osl-lab/enter-env.ps1
Set-Location C:/dev/osl-lab/experiments
oslc pattern.osl
testshade -g 2 2 -t 1 --groupname demo --options debug_output_cpp=3,cpp_output_dir=. pattern -o Cout null --print
```

The environment script configures the C++ compiler, DLL runtime, and import libraries for this lab. No global PATH or system SDK changes are required.

Built revision: [`ff51e4e4dd1d7841fa802cbfe10da2b1fc0bc666`](https://github.com/AcademySoftwareFoundation/OpenShadingLanguage/tree/ff51e4e4dd1d7841fa802cbfe10da2b1fc0bc666), reporting `1.16.0.0dev`. Dependencies include LLVM/Clang 20.1.8, OpenImageIO 3.1.17.0, Imath 3.2.2, and Python 3.12.14. The host toolchain uses Visual Studio 2022's MSVC headers/libraries and the Windows SDK, with clang-cl compiling OSL and clang++ compiling generated DLLs. OptiX and batched execution are disabled. CPU shadeops use the Windows build's default non-embedded-bitcode configuration.

## What was tested

| Workload | Execution | Result |
| --- | --- | --- |
| Procedural sine/cosine/noise with explicit Dx, Dy, and filterwidth outputs | 64x64, OSL optimization 0 and 2 | Color and derivatives agree |
| Two connected layers with a varying branch | 64x64, optimization 0 and 2 | Final color and source derivatives agree |
| Filtered checker texture with warped coordinates and periodic wrapping | 64x64, optimization 0 and 2 | Color and coordinate derivatives agree |
| Upstream closure test: diffuse, Phong, transparency, emission, debug, holdout, weights and string labels | 1x1, optimization 0 and 2 | Printed closure trees agree exactly |
| Upstream backend-cpp sample with connected parameters, loops, arrays, matrix components and printing | 2x2, optimization 0 and 2 | Output images agree |
| Diffuse sphere and constant background in testrender | 128x96, 4x4 samples, one thread | Rendered images agree |

The four existing unit tests were `unit_accum`, `unit_dual`, `unit_llvmutil`, and `unit_bsdl`. [Test log](C:/dev/osl-lab/unit-tests.log)

These are representative comparisons, not a full OSL conformance run. They do not cover every branch of the upstream sample or every renderer closure implementation.

The harness rejects error diagnostics as well as nonzero exit codes, and requires generated DLLs in C++ mode. This matters because the initial failed `testshade` invocation returned exit code zero despite reporting a compiler failure. The summary's `text_equal` field compares raw stdout; it is expected to be false for image cases because their output filenames differ. Closure cases assert exact text equality separately.

## What the C++ looks like

The generated files are small for these examples: 71 lines for the optimized procedural shader, 101 for the connected group, 74 for the textured shader, 210 for the closure test, and 43/44 for the render's two materials. The referenced runtime header and linked libraries account for much of their behavior; line counts are not implementation-cost estimates.

- [Procedural OSL source](C:/dev/osl-lab/experiments/pattern.osl)
- [Generated procedural C++](C:/dev/osl-lab/results/pattern/cpp-O2/group-cpp-pattern_1.cpp)
- [Generated connected group](C:/dev/osl-lab/results/connected/cpp-O2/group-cpp-connected_1.cpp)
- [Generated texture code](C:/dev/osl-lab/results/textured/cpp-O2/group-cpp-textured_1.cpp)
- [Generated closures](C:/dev/osl-lab/results/closures/cpp-O2/group-cpp-closures_1.cpp)
- [Printed closure structures](C:/dev/osl-lab/results/closures/cpp-O2.log)
- [LLVM IR for the procedural shader](C:/dev/osl-lab/results/pattern/llvm_inspect_O1.ll)

The generated procedural code includes this sequence:

```cpp
OSL::Dual2<float> u(sg->u, sg->dudx, sg->dudy);
OSL::Dual2<float> v(sg->v, sg->dvdx, sg->dvdy);
// ...
___tmp2 = u * 8.0f;
osl_sin_dfdf((void*)&___tmp1, (void*)&___tmp2);
___tmp4 = v * 8.0f;
osl_cos_dfdf((void*)&___tmp2, (void*)&___tmp4);
wave = ___tmp1 * ___tmp2;
```

The shader-group optimizer removes unnecessary derivative work when only the color output is requested. With derivative outputs retained, the generator uses `Dual2` values and derivative-aware runtime functions. Connections write values into group data. Layer run flags control dispatch. Outputs are copied into the renderer's specified output-buffer layout.

Texture generation initializes an `OIIO::TextureOpt`, sets wrapping, and calls `osl_texture` with value and gradient arguments. Closure generation allocates runtime records and fills fields using the renderer's registered closure layouts. It constructs descriptions of scattering; the renderer still evaluates and samples them.

The optimized LLVM dump calls the same `osl_sin_ff`, `osl_cos_ff`, and `osl_snoise_fv` runtime functions for the color-only example. It also contains host-ABI offsets, pointer arithmetic, and output placement. This CPU dump is not a ready-made Vulkan shader.

## Windows fixes and practical limitations

The successful lab uses two small local OSL source changes, totaling nine added lines and one removed line:

1. Put a color-table alignment attribute before the declaration so clang-cl accepts it.
2. Add an outer command-quoting pair for the Windows compiler subprocess and export generated DLL entry points with `__declspec(dllexport)`.

The runner additionally supplies Windows import libraries and chooses the DLL CRT. Clang's GNU-style driver added `libcmt` despite the requested DLL runtime, so the generated-DLL flags explicitly suppress that conflicting static CRT library. The original failure logs are [compiler launch](C:/dev/osl-lab/preflight.log) and [CRT linking](C:/dev/osl-lab/preflight-crt.log). A later [standalone command](C:/dev/osl-lab/standalone.log) successfully printed values from generated C++ execution.

The dependency environment needed fmt and libxml2 development files explicitly. LLVM 20's package metadata names `zstd.dll.lib`; the installed zstd package supplies `zstd.lib`, so the build script makes a local binary alias.

The C++ backend remains coupled to OSL internals. Its runtime header is private and uninstalled, and mode 3 still uses LLVM for group-data layout. Source inspection also found that closure prepare/setup callbacks are omitted by the C++ generator, with an assumption about testshade's closures. Passing these fixtures therefore does not establish compatibility with every renderer. [Detailed source audit](C:/dev/osl-lab/backend-audit.md)

Whole-process times for these small shader cases were approximately 0.36-4.3 seconds for LLVM and 7.2-39.8 seconds for C++; the rendered scene took about 2.0 and 58.8 seconds respectively. These single observations include startup, source compilation, linking, and rendering, under varying machine load. They are not steady-state shading benchmarks. The practical implication is to investigate caching and background compilation before using generated C++ for interactive material editing.

## Revised recommendation and effort

I would now run an **LLVM-to-Vulkan feasibility spike first**, while retaining the C++ backend as an inspection/reference tool. This changes the earlier recommendation to begin directly with GLSL generation.

OSL's existing OptiX path links device shadeop and renderer bitcode, applies NVIDIA-specific conventions, and generates PTX. That is the useful precedent for the V-Ray-style approach mentioned in the discussion. LLVM code generation can potentially preserve existing opcode lowering and derivative implementations. Porting its device runtime may still require substantial work. Current LLVM documents Vulkan SPIR-V targets, but neither those targets nor OSL's CUDA shadeops were exercised by this CPU experiment. [OSL GPU lowering](https://github.com/AcademySoftwareFoundation/OpenShadingLanguage/blob/ff51e4e4dd1d7841fa802cbfe10da2b1fc0bc666/src/liboslexec/llvm_instance.cpp), [LLVM SPIR-V guide](https://llvm.org/docs/SPIRVUsage.html)

The deciding spike should compile one material with arithmetic, derivatives, noise, and a texture lookup into Vulkan-valid SPIR-V, then execute it and compare against this CPU reference. It should test resource access, address spaces/pointers, entry-point conventions, and reuse of device math. Vulkan compute is a useful first target for this test; fitting the same evaluator into a ray-tracing stage remains a separate check. Switch to GLSL source generation if the necessary LLVM legalization becomes larger than implementing the supported subset directly.

Updated planning estimates for one experienced engineer, assuming Crowny already has a working Vulkan path tracer and material/texture infrastructure:

| Scope | Estimate after this experiment |
| --- | --- |
| Bounded LLVM-to-Vulkan decision spike | 1-2 weeks, producing evidence and a go/no-go decision rather than guaranteeing full support |
| Small Vulkan material prototype after a viable route is established | Another 2-4 weeks |
| Useful restricted OSL implementation, including resources, selected closures, caching, diagnostics and tests | Still roughly 3-6 engineer-months total |
| Broad compatibility and production hardening | Still roughly 9-18+ engineer-months |

The experiment reduces uncertainty around upstream setup, code generation and validation. It does not justify reducing the GPU runtime estimate. A separate CPU integration for material baking/previews would be much smaller, potentially a 1-2 week initial integration now that the build works, but that would not provide GPU OSL shading.
