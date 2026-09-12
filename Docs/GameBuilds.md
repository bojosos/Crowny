# Standalone game builds

Build Game produces a Windows x64 application using the Mono runtime. The output contains `Game.exe`, native libraries, the managed runtime and assemblies, built-in rendering resources, and a verified content pack. The game opens the configured startup scene and runs physics, scripts, animation, and rendering without the editor.

Build the editor and its player template with:

```powershell
Scripts\crowny.bat build Editor --configuration Release
```

The template is staged at `bin/Release-windows-x86_64/PlayerTemplate`. `build Player` builds and stages the player separately. Debug editor builds can use the Release template; the Debug CRT is not distributed. Script debug symbols are controlled by the checkbox in Build Game.

In the editor:

1. Open **Build Game** and select a saved startup scene with a Camera component.
2. Use **Save and use current scene** to save and select the scene being edited. Cancelling Save As leaves the startup scene unchanged.
3. Choose an output folder outside the project and click **Build game**. This uses saved scenes on disk. It does not prompt to save an unrelated open scene.
4. Click **Run game** or launch `Game.exe` from the output folder. Distribute the whole folder.

The startup scene, output folder, and script debug setting are saved per project. All imported project assets, scenes, model subassets, and project input bindings are packaged. Builds run in the background and can be cancelled. Compiler failures show the compiler's detailed output in the build window. Failed builds preserve the previous output through the existing pipeline's staging and publication checks.

Scene choices show the complete filename relative to Assets, so scenes in different folders and files with repeated extensions remain distinct. Save As stays attached to the editor window; cancelling it leaves both the existing scene file and startup selection unchanged.

The player resolves files relative to its executable, so shortcuts and launching from another working directory work. Content payloads are verified and extracted to a temporary directory owned by that player process. The directory is removed on normal shutdown. The game does not import source assets at runtime.

## Verification

`python Scripts/test-player.py` builds the player and sample, copies the game to a directory with spaces, clears runtime environment overrides, and launches both Vulkan and OpenGL from an unrelated working directory. It checks rendering and managed lifecycle markers.

Use `python Scripts/test-player.py --no-build` after building the Release player and tests to repeat packaging and launch checks without compiling again.

For this same sample exported through the editor, use `python Scripts/test-player.py --game-directory <output-folder>` to run the relocation checks against that export. The sample's lifecycle-marker script is required by this check.

The explicit sample fixture builds a scene containing a camera and a square controlled by the arrow keys or WASD. Its script also writes `script-started.txt` and `script-updated.txt` into the game folder to verify lifecycle execution.

```powershell
$env:CROWNY_PLAYER_TEMPLATE = (Resolve-Path bin/Release-windows-x86_64/PlayerTemplate).Path
$env:CROWNY_PLAYER_SMOKE_ROOT = Join-Path (Get-Location) 'artifacts/standalone-sample'
Scripts\crowny.bat test --filter '[PlayerSmoke]'
& "$env:CROWNY_PLAYER_SMOKE_ROOT/Game/Game.exe" --frames 30 --hidden --report "$env:CROWNY_PLAYER_SMOKE_ROOT/vulkan.txt"
& "$env:CROWNY_PLAYER_SMOKE_ROOT/Game/Game.exe" --opengl --frames 30 --hidden --report "$env:CROWNY_PLAYER_SMOKE_ROOT/opengl.txt"
```

The bounded player run fails if it cannot load its startup scene or render a successful frame. Launch without arguments for the interactive sample. The sample test is excluded from the default test suite because it requires a staged native player.

Current limits: automatic runtime distribution supports Windows x64 and Mono. Packages contain all imported assets. The player uses a 1280 by 720 resizable window. Custom executable names, icons, installers, and CoreCLR player packaging are not exposed by this first editor workflow.
