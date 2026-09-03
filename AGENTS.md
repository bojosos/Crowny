# Repository Guidelines

## Project structure and modules

`Crowny/Source/Crowny/` contains the C++20 engine. Systems are grouped by domain: `Audio/`, `Physics/`, `Renderer/`, `Scene/`, `Scripting/`, and `NodeGraph/`; platform implementations live under `Crowny/Source/Platform/`. `Crowny-Editor/Source/` contains editor panels and tooling, while `Crowny-Editor/Resources/` holds shaders, icons, scenes, and runtime assets. Managed bindings and example scripts live in `Crowny-Sharp/Source/` and `Crowny-Sandbox/Source/`. Add native tests to `Crowny-Tests/Source/`. Treat `Crowny/Dependencies/` and `3rdparty/` as vendored code unless a dependency update is the point of the change.

## Build and development commands

All build orchestration lives in the Python tool `Tools/crowny` (stdlib-only, Python 3.9+). Invoke it with `Scripts\crowny.bat <command>` on Windows, `./Scripts/crowny <command>` elsewhere, or `python Tools/crowny <command>` directly. The legacy `Scripts\*.ps1` entrypoints are thin compatibility shims that translate to the same tool.

Initialize the exact dependency revisions first:

```powershell
git submodule sync --recursive
git submodule update --init --recursive
```

- `Scripts\crowny.bat setup --build --test` bootstraps local SDKs, generates VS2022 projects, builds Release, and runs Catch2. Add `--configuration Debug --sanitizer Address` for ASan-instrumented tests with the Windows CRT leak checker.
- `Scripts\crowny.bat doctor` reports discovered tools, MSBuild, and dependency roots.
- `Scripts\crowny.bat deps vulkan|openal|physics|spirv-cross|dotnet` bootstraps a single dependency.
- `Scripts\crowny.bat build Engine|Editor|Tests|RenderTests|All` is the Windows daily-build entrypoint; `Scripts\crowny.bat test` builds and runs Catch2. Agents must use these commands instead of raw MSBuild. Auto scheduling gives the first build 8 of 12 compiler workers, leaves four for another output family, serializes overlapping output writes, and permits concurrent test readers. Pass `--jobs` to request a fixed share; `--jobs 12` waits for exclusive compiler capacity. `--inner-loop` skips building project references for fast single-file iteration after a full build. `--profile` records binlogs and build metrics under `artifacts/build-metrics/`.
- `Scripts\crowny.bat managed` builds the managed C# assemblies; `Scripts\crowny.bat gen --force` regenerates `Crowny.sln` with node-editor support.
- `Scripts\crowny.bat render-tests` builds the render harness, checks Vulkan and OpenGL against shared references, and compares both outputs.
- `./Scripts/crowny gen --force && make Crowny-Editor Crowny-Tests -j2 config=release_linux64 CXX=clang++` generates and builds on Linux.
- `./bin/Release-linux-x86_64/Crowny-Tests/Crowny-Tests` runs the Linux test binary. The Windows executable uses the same path pattern with `Release-windows-x86_64`.
- Python tooling tests: `python -m unittest discover -s Tools/crowny/tests`.

`Scripts\windows-build-common.ps1`, `Scripts\measure-build-windows.ps1`, and `Scripts\probe-sccache-windows.ps1` remain PowerShell for build benchmarking.

Linked worktrees reuse the main checkout's dependency cache while keeping generated projects and outputs local. Set `CROWNY_DEPS_ROOT` to override the shared SDK cache and `CROWNY_BUILD_COORDINATION_ROOT` to override the family-wide lock/lease directory. Component-specific overrides remain available through `VULKAN_SDK`, `CROWNY_MONO_ROOT`, `CROWNY_OPENAL_ROOT`, `CROWNY_PHYSICS_ROOT`, and `CROWNY_SPIRV_CROSS_ROOT`. Do not commit `bin/`, `bin-int/`, generated solutions, or downloaded SDKs.

## Coding style and naming

Use four spaces and no tabs. Follow `.clang-format` and `.clang-tidy`. Run `Scripts/format.sh` before submitting native changes; it normalizes line endings and formats engine and editor sources. Pair `PascalCase.h` with `PascalCase.cpp`. Types and methods use `PascalCase`; locals and parameters use `camelCase`; members follow the nearby `m_` convention. Keep public C# APIs documented and consistent with existing .NET naming.

## Tests

Tests use Catch2. Name files after the unit under test, such as `HierarchyTests.cpp`, and keep tests deterministic. Run the full `Crowny-Tests` executable before submitting. Renderer changes must also run `Scripts\run-render-tests.ps1`; inspect failure artifacts before deliberately updating references. For editor, audio, or scripting changes, record the manual workflow and platform tested. Run `python Scripts/check_headers.py` after adding headers.

## Commits and pull requests

History uses short subjects such as `Geometry nodes` and `fix: revert incorrect const`. Prefer a concise imperative subject and keep unrelated refactors separate. Pull requests must explain the behavior change, list build and test commands, link issues, and include screenshots or captures for visible editor or rendering changes. Do not merge with failing CI.
