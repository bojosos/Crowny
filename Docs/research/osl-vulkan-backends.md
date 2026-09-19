# OSL backends for Vulkan

Reviewed 2026-09-19 against upstream OSL source, its C++ backend pull request, and LLVM documentation. The proposed Crowny design below is an engineering recommendation, not an existing upstream Vulkan integration.

The subsequent [C++ execution experiment](osl-cpp-experiment.md) supersedes the initial backend recommendation below: investigate LLVM-to-Vulkan reuse before committing to a separate GLSL generator. The source findings remain useful background.

## What upstream provides

OSL compiles source into `.oso` intermediate code. Its shading system specializes and optimizes connected shader groups before execution. This makes the optimized group the useful boundary for another backend, while preserving the existing parser, type checker, shader connections, and optimizer. [OSL overview](https://github.com/AcademySoftwareFoundation/OpenShadingLanguage), [LLVM group generation](https://github.com/AcademySoftwareFoundation/OpenShadingLanguage/blob/main/src/liboslexec/llvm_instance.cpp)

The C++ source backend merged on July 24, 2026. It emits C++ from the optimized group, primarily for inspection and execution comparisons with the LLVM JIT. Its author explicitly describes future Metal and GLSL output as a motivation, while expecting substantial refactoring for the next language. This is development-branch functionality associated with OSL 1.16, not evidence of an existing production GLSL backend. [BackendCpp PR](https://github.com/AcademySoftwareFoundation/OpenShadingLanguage/pull/2130), [OSL changes](https://github.com/AcademySoftwareFoundation/OpenShadingLanguage/blob/main/CHANGES.md)

The current class is declared `final`, despite having virtual `lang_*` methods. It uses OSL private internals. Generated code assumes a native ABI and invokes `osl_*` runtime functions. It also uses pointers and `goto` for some control flow. A GLSL backend therefore needs semantic lowering and runtime replacements, not just different type names. [BackendCpp declaration](https://github.com/AcademySoftwareFoundation/OpenShadingLanguage/blob/main/src/liboslexec/backendcpp.h), [BackendCpp implementation](https://github.com/AcademySoftwareFoundation/OpenShadingLanguage/blob/main/src/liboslexec/backendcpp.cpp)

OSL's existing NVIDIA GPU path generates LLVM IR and uses the NVPTX backend for OptiX. The implementation supplies GPU shade operations and renderer services. Vulkan ray tracing does not make these PTX modules usable as Vulkan shaders. [OSL LLVM backend](https://github.com/AcademySoftwareFoundation/OpenShadingLanguage/blob/main/src/liboslexec/llvm_instance.cpp), [OptiX integration notes](https://github.com/AcademySoftwareFoundation/OpenShadingLanguage/blob/main/docs/app_integration/OptiX-Inlining-Options.md)

Current LLVM documentation explicitly supports logical SPIR-V and Vulkan target environments. Saying LLVM only generates OpenCL SPIR-V would be incorrect. However, selecting a Vulkan target does not port OSL's shader globals, runtime calls, resources, pointer representations, or ray tracing integration. That distinction is an inference from the separate OSL ABI and LLVM resource representations. [LLVM SPIR-V target guide](https://llvm.org/docs/SPIRVUsage.html), [OSL shader globals lowering](https://github.com/AcademySoftwareFoundation/OpenShadingLanguage/blob/main/src/liboslexec/llvm_instance.cpp)

## Recommended Crowny implementation

Use `OSL source -> oslc -> optimized shader group -> Crowny GLSL generator and runtime -> shaderc -> Vulkan SPIR-V`. Keep CPU LLVM execution as a reference for conformance tests. Pin an OSL revision and isolate use of private optimized-group APIs behind a small adapter. Reuse or refactor BackendCpp's traversal and opcode knowledge where practical.

Compile a whole material group rather than every node separately. Define a GPU shading context carrying geometry, UVs, normal, transforms, material parameters, and texture handles. Emit a regular material evaluation function that the renderer can call from a closest-hit shader initially. Keep lighting, path sampling, and ray dispatch in the integrator.

The runtime is the substantial part of this proposal:

- Carry explicit value, dx, and dy fields for derivative-bearing values. Initialize them from ray differentials and propagate them through arithmetic and supported built-ins. Use explicit texture gradients. Fragment `dFdx` and `dFdy` are not a replacement for OSL derivatives in ray tracing.
- Lower supported closures to weighted GPU lobe records. Preserve closure addition and scaling; let the integrator evaluate and sample them. For an initial bounded representation, reject graphs whose maximum storage cannot be supported or report overflow explicitly. Never silently discard lobes.
- Map constant resource names to stable IDs and bindless texture indices. Specify which dynamic string operations are supported. Runtime string construction is separate work.
- Implement renderer services deliberately: texture filtering, coordinate transforms, attributes, color transforms, and later trace queries. Diagnose unsupported operations at material compilation.

Start with arithmetic, parameter connections, UVs, selected noise, textures, and a small documented closure set. Compare numerical results and derivatives against CPU OSL before judging final images. Exercise control flow, unused layers, closure mixtures, filtering, and error diagnostics. Expand the supported profile only with matching tests.

A dedicated LLVM-to-Vulkan lowering could become attractive if reuse of the C++ shade-operation library proves decisive. It is a second compiler integration to investigate with a prototype, not a shortcut that avoids the GPU ABI and runtime work. For Crowny's existing GLSL toolchain, direct source generation is the lower-risk first implementation.
