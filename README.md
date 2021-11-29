# Crowny
![C/C++ CI](https://github.com/bojosos/Crowny/workflows/Crowny-Editor/badge.svg) ![Crowy-Sharp](https://github.com/bojosos/Crowny/workflows/Crowy-Sharp/badge.svg)

A C++ game engine

GOAL: Run the same code I wrote in a Minecraft clone I made in Unity with Crowny

### Features:
  * Editor
  * C# scripting using Mono
  * PBR (Currently being fixed after Vulkan implementation)
  * OpenGL (Currently being fixed after Vulkan implementation), Vulkan
  * Windows, Linux
  
### Roadmap:
  * Finish Vulkan (Mostly done, apart from some small texture format issues, event, timer and occlusion queries, structured buffers and load store textures which are currently not supported by the engine)
  * Audio using OpenAL (Mostly working, although not hooked up to C# and Vorbis decoding is buggy, also audio streaming foundation is there but no multithreading support)
  * Refactor OpenGL (Should be quite quick, requires a few changes to the old rendering system)
  * Multithreading? (Firstly only "queable" tasks (async resource loading, audio streaming, then whole engine)
  * Implememt 2D physics using Box2d (Should be fairly straightforward)
  * Implement 3D physics using PhysX
  * Proper material system and shadows
  * Documentation
  * Game builds (I have a "usable" engine here)
  * Big refactor/clean up
