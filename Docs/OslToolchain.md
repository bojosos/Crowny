# OSL compiler setup

OSL source compilation is an optional editor/build tool. Crowny's player uses the cooked shader assets and does not load LLVM, OSL or OpenImageIO. GLSL procedural textures need only the Vulkan SDK tools.

All orchestration runs through the Python 3.9+ Crowny tool. Use `python Tools/crowny` on any host, `Scripts/crowny.bat` on Windows, or `./Scripts/crowny` on Unix. The OSL and procedural tool directories no longer contain PowerShell scripts.

## Install the compiler

Install Git, CMake 3.24+, the Vulkan SDK and [micromamba](https://mamba.readthedocs.io/en/stable/installation/micromamba-installation.html). Windows also needs the Visual Studio C++ build tools and Windows SDK. Linux/macOS need the native development tools and system SDK. Then run:

```text
python Tools/crowny deps osl
```

This creates an isolated dependency environment, fetches OSL at the exact commit in `Tools/osl/toolchain.json`, applies the repository's two patches, builds and installs OSL, and builds the Crowny adapter, reference generator and device bitcode. It checks LLVM/Clang 20.1.8 and the LLVM SPIR-V target before building. Dependencies live in the shared `.deps/osl` directory, including across linked worktrees. `CROWNY_DEPS_ROOT` changes the shared dependency cache; `CROWNY_OSL_ROOT` overrides this toolchain's directory. Builds use Crowny's shared compiler-worker lease and default to two workers. `--jobs 4` requests four.

The checkout remains detached at its pinned commit. Setup reapplies missing patches but never resets local changes or switches an existing checkout to another revision. A changed pin requires a fresh toolchain directory. The bootstrap never modifies the original experiment checkout.

Windows x64 has an explicit package lock at `Tools/osl/locks/windows-x86_64.txt`, with exact conda-forge package URLs and hashes from the validated environment. [Micromamba explicit specifications](https://mamba.readthedocs.io/en/stable/user_guide/micromamba.html) are platform-specific. Linux and macOS currently resolve the versions in `toolchain.json`; their transitive dependencies are not yet locked or validated on native hosts. The script reports this when resolving them. Do not describe those builds as reproducible release packages until each host has its own tested lock.

To use an existing dependency installation instead of micromamba:

```text
python Tools/crowny deps osl --deps-prefix /path/to/llvm-oiio-prefix
```

The prefix must contain `bin`, `include` and `lib`, including matching LLVM/Clang 20.1.8, OpenImageIO 3, Imath and OSL's build dependencies. For a Windows conda environment this is its `Library` directory. This override is recorded in the machine-local `.deps/osl/toolchain.json`. It deliberately makes that installation dependent on the supplied prefix. `--micromamba /path/to/micromamba` selects a manager outside PATH.

## Compile and import

Set `VULKAN_SDK`, or supply `--spirv-bin` pointing to the directory containing glslang and SPIR-V Tools. Windows SDKs normally use `Bin`; Unix SDKs use `bin`. Compile an asset with:

```text
python Tools/crowny osl compile Tools/osl/fixtures/image_texture.osl artifacts/osl/example
python Tools/crowny osl compile Tools/osl/fixtures/mandelbrot.osl artifacts/osl/mandelbrot --output result
```

Normal compilation produces shader code and metadata, with no CPU sample grid. Imports never download dependencies or build the compiler. Missing tools produce a setup diagnostic.

The editor launches `Tools/osl/editor-import.py` directly with Python, without a command shell. Windows uses a job object and POSIX uses a process group so the two-minute import timeout can stop compiler children. Set `CROWNY_PYTHON` to a Python executable if `python.exe` on Windows or `python3` on Unix is unavailable. Set `CROWNY_OSL_IMPORT_SCRIPT` to the script's absolute path when launching outside the repository/editor directory. Compiler children receive the OSL/LLVM library paths; the editor and renderer retain their own environment.

## Validation

```text
python Tools/crowny osl test
python Tools/crowny osl test --no-build
python Tools/crowny osl test --procedural-only --no-build
python -m unittest discover -s Tools/crowny/tests
```

The test runner explicitly opts into CPU reference generation, cooks every supported fixture, runs the GPU comparisons, and checks unsupported-feature diagnostics. `--runner` accepts a render-test executable in a nonstandard build location. `--no-build` also allows testing on a host whose engine build is managed separately. The Python code selects host executable suffixes and library paths. Native Linux/macOS execution, including the POSIX editor process path, still needs CI coverage; Windows validation is recorded separately.

### Windows validation, 2026-09-19

A fresh checkout of the pinned OSL revision built successfully. Micromamba then recreated all 123 locked development packages in `.deps/osl/environment`, and the source plus adapter rebuilt against that installation. The final compiler manifest and both CMake caches contain no paths to the original experiment. Switching dependency prefixes initially exposed stale CMake package paths; setup now configures with `--fresh` to discard those caches.

The completed installation passed the full OSL GPU runner: 36,075 compute samples, 50,505 material pixels, all scene checks and unsupported-operation checks. No Vulkan validation errors were reported on Intel Iris Xe. All five focused importer cases passed 63 assertions using the direct Python launcher. A normal image-input compilation into a directory with spaces produced no `.bin` reference files or `evaluate.spv`. The GLSL-only runner passed independently.

All 73 Python tooling tests passed, including real local Git acquisition, repeated patch application, refusal to change a pinned checkout, preservation of edits, host tool naming and compiler-environment isolation. The native Tests build passed; the full native executable passed 114,831 assertions across 1,009 passing cases with one skipped. Renderer regressions passed 39 Vulkan cases, 39 OpenGL cases and 39 cross-backend comparisons. Formatting and header checks passed. These are automated import/render checks; native editor clicks were not manually exercised.

Logs are in `artifacts/osl`: `portable-locked-bootstrap-final.log`, `portable-locked-gpu-tests.log`, `portable-locked-import-tests.log`, `portable-normal-compile.log`, `portable-glsl-tests.log`, `portable-python-tests.log`, `portable-native-tests.log` and `portable-render-regression.log`.

## Distribution

The current deliverable is a source bootstrap with a locked Windows development environment, not a relocatable binary SDK. Do not zip `.deps/osl` and assume it will relocate: CMake files, the dependency environment and the local manifest can contain absolute paths.

A distributable editor should eventually download a versioned compiler SDK for its OS and architecture. That SDK needs the adapter, oslc, llc, device bitcode, OSL headers used by oslc, required shared libraries, and the shader cooking scripts/templates, with a dependency manifest and notices. Build and test those archives in CI, then add checksum-verified acquisition to `deps osl`. Keep this compiler package separate from player exports. The source bootstrap remains the way to rebuild it and develop the backend.
