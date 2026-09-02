# Repository Guidelines

## Project structure and modules

`Crowny/Source/Crowny/` contains the C++20 engine. Systems are grouped by domain: `Audio/`, `Physics/`, `Renderer/`, `Scene/`, `Scripting/`, and `NodeGraph/`; platform implementations live under `Crowny/Source/Platform/`. `Crowny-Editor/Source/` contains editor panels and tooling, while `Crowny-Editor/Resources/` holds shaders, icons, scenes, and runtime assets. Managed bindings and example scripts live in `Crowny-Sharp/Source/` and `Crowny-Sandbox/Source/`. Add native tests to `Crowny-Tests/Source/`. Treat `Crowny/Dependencies/` and `3rdparty/` as vendored code unless a dependency update is the point of the change.

## Build and development commands

Initialize the exact dependency revisions first:

```powershell
git submodule sync --recursive
git submodule update --init --recursive
```

- `Scripts\setup-windows.ps1 -Build -Test` bootstraps local SDKs, generates VS2022 projects, builds Release, and runs Catch2.
- `Scripts\test-windows-build-common.ps1` validates worktree cache discovery and build-lock coordination without invoking a compiler.
- `Scripts\setup-windows.ps1 -Build -Test -Configuration Debug -Sanitizer Address` builds ASan-instrumented tests and enables the Windows CRT leak checker.
- `Scripts\build-windows.ps1 -Target Engine|Editor|Tests|RenderTests|All` is the Windows daily-build entrypoint; `Scripts\test-windows.ps1` builds and runs Catch2. Agents must use these commands instead of raw MSBuild. Auto scheduling gives the first build 8 of 12 compiler workers, leaves four for another output family, serializes overlapping output writes, and permits concurrent test readers. Pass `-Jobs` to request a fixed share; `-Jobs 12` waits for exclusive compiler capacity.
- `Scripts\genprojects.bat` regenerates `Crowny.sln` with node-editor support.
- `Scripts\run-render-tests.ps1` builds the render harness, checks Vulkan and OpenGL against shared references, and compares both outputs.
- `./Scripts/genprojects.sh && make Crowny-Editor Crowny-Tests -j2 config=release_linux64 CXX=clang++` generates and builds on Linux.
- `./bin/Release-linux-x86_64/Crowny-Tests/Crowny-Tests` runs the Linux test binary. The Windows executable uses the same path pattern with `Release-windows-x86_64`.

Linked worktrees reuse the main checkout's dependency cache while keeping generated projects and outputs local. Set `CROWNY_DEPS_ROOT` to override the shared SDK cache and `CROWNY_BUILD_COORDINATION_ROOT` to override the family-wide lock/lease directory. Component-specific overrides remain available through `VULKAN_SDK`, `CROWNY_MONO_ROOT`, `CROWNY_OPENAL_ROOT`, `CROWNY_PHYSICS_ROOT`, and `CROWNY_SPIRV_CROSS_ROOT`. Do not commit `bin/`, `bin-int/`, generated solutions, or downloaded SDKs.

## Coding style and naming

Use four spaces and no tabs. Follow `.clang-format` and `.clang-tidy`. Run `Scripts/format.sh` before submitting native changes; it normalizes line endings and formats engine and editor sources. Pair `PascalCase.h` with `PascalCase.cpp`. Types and methods use `PascalCase`; locals and parameters use `camelCase`; members follow the nearby `m_` convention. Keep public C# APIs documented and consistent with existing .NET naming.

## Tests

Tests use Catch2. Name files after the unit under test, such as `HierarchyTests.cpp`, and keep tests deterministic. Run the full `Crowny-Tests` executable before submitting. Renderer changes must also run `Scripts\run-render-tests.ps1`; inspect failure artifacts before deliberately updating references. For editor, audio, or scripting changes, record the manual workflow and platform tested. Run `python Scripts/check_headers.py` after adding headers.

## Commits and pull requests

History uses short subjects such as `Geometry nodes` and `fix: revert incorrect const`. Prefer a concise imperative subject and keep unrelated refactors separate. Pull requests must explain the behavior change, list build and test commands, link issues, and include screenshots or captures for visible editor or rendering changes. Do not merge with failing CI.
