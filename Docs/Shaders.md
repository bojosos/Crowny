# Shader pipeline

Crowny compiles `.glsl` and `.cwsl` sources to SPIR-V through shaderc, then reflects descriptor and vertex-input metadata through SPIRV-Cross. The editor writes compiled shaders as versioned `.asset` files. `BuiltInShaderCompiler` recompiles built-in assets when the source, asset format, or timestamp changes.

## Source directives

Put engine directives on their own lines. The parser ignores matching text inside ordinary comments.

```glsl
#lang glsl
#pragma variation USE_FOG
#pragma variation_multi _ QUALITY_LOW QUALITY_HIGH
#pass forward
#type vertex
#version 450
// vertex source
#type fragment
#version 450
// fragment source
```

`#pragma variation NAME` compiles disabled and enabled forms. `#pragma variation_multi` selects exactly one listed identifier; `_` selects none. Crowny rejects duplicate identifiers and more than 256 total combinations. A variation stores every option as a boolean, so material lookups have a stable, exact key.

Render-state pragmas are `depth_read`, `depth_write`, `depth_compare`, `cull`, and `polygon_mode`. `depth_compare` accepts `never`, `always`, `less`, `less_equal`, `equal`, `not_equal`, `greater`, and `greater_equal`; reverse-Z passes normally use `greater_equal`. Graphics passes require vertex and fragment stages. Compute and ray-tracing stages must use separate passes. Tessellation control and evaluation stages must appear together.

## Cache and diagnostics

The in-process stage cache keys source text, stage, input language, output flags, and sorted defines. Editing source or defines creates a new entry. `ShaderCompiler::ClearCache()` explicitly invalidates the cache, which is capped at 1,024 entries. `CompileWithDiagnostics()` returns parser and compiler messages with file, line, and stage context. Importers do not save assets after any error.

Reflection covers uniform buffers, sampled and separate images, samplers, storage buffers, storage images, acceleration structures, annotations, defaults, and vertex inputs. Compiled assets serialize the same metadata.

## Current limits

Vulkan consumes the compiled SPIR-V directly. OpenGL cross-compiles it to core GLSL through SPIRV-Cross, remaps descriptor bindings, and binds reflected uniform blocks and samplers explicitly for the macOS 4.1 path. Compute shaders require OpenGL 4.3; ray-tracing stages remain Vulkan-only.

Source `#include` resolution, push-constant metadata, specialization constants, mesh shaders, and ray-tracing shader groups beyond one raygen, miss, and closest-hit stage are not exposed yet.
