# Crowny
![C/C++ CI](https://github.com/bojosos/Crowny/workflows/Crowny-Editor/badge.svg) ![Crowy-Sharp](https://github.com/bojosos/Crowny/workflows/Crowy-Sharp/badge.svg)

A C++ game engine

GOAL: Run the same code I wrote in a Minecraft clone I made in Unity with Crowny

### Features:
  * Editor
  * C# scripting through one shared managed API, with Mono and opt-in desktop CoreCLR backends
  * PBR
  * OpenGL, Vulkan
  * Windows, Linux

Vulkan is the default renderer. Pass `--render-api=opengl` or `--opengl` to the editor to use OpenGL. The OpenGL backend targets 4.5 on Windows and Linux and 4.1 on macOS.

Native x86-64 desktop builds target AVX2 by default, including source-built dependencies. Use the `sse4.1` fallback consistently when generating Crowny and bootstrapping physics dependencies. For example, run `Scripts\crowny.bat deps physics --simd sse4.1` with Premake's `--simd=sse4.1` option. Prebuilt SDK binaries such as Vulkan and Mono retain their vendor instruction-set settings.

### Roadmap:
  * Multithreading? (Firstly only "queable" tasks (async resource loading, audio streaming, then whole engine))
  * Extend scene tooling for advanced 3D physics shapes and constraints
  * Proper material system and shadows
  * Documentation
  * Game builds (I have a "usable" engine here)
  * Big refactor/clean up
