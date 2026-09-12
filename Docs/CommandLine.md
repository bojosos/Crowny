# Engine command-line arguments

`CommandLineArgs::Get()` retains the raw argument vector, including the executable.
`CommandLineArgs::GetParsed()` returns a parsed snapshot. For tests or tools, construct
`CommandLineArguments` directly from a vector with the executable at index zero.
Its optional second argument lists known flags that must not consume a following value.

The parser accepts `--name value`, `--name=value`, `-n value`, and bare flags.
Names are case-sensitive and include their dash prefix. Short flags are not bundled.
Values preserve spaces and additional equals signs. The shell handles quoting before
the engine receives the arguments.

An option consumes the next token as its value unless that token starts another
option. Negative numbers and a single `-` are values. Use `--name=--value` for a
value that starts with an option prefix. Put `--` before positional arguments
following flags to prevent them from being consumed as values. Everything after
`--` is positional, including tokens beginning with a dash.

- `HasOption(name)` checks presence, including bare flags and empty values.
- `GetValue(name)` returns the last occurrence's value, or no value for a missing
  option or a bare flag. An explicit `--name=` returns an empty string.
- `GetValues(name)` returns all supplied values in command-line order.
- `GetInteger(name)` reads a signed 64-bit decimal integer. Invalid input, overflow,
  whitespace, leading plus signs, missing values, and trailing characters return no value.
- `GetOptions()` preserves option order, including flags and repeated options.
- `GetPositionals()` returns unconsumed arguments, excluding the executable.

The parser does not validate application-specific option names or required values.

## Editor options

```text
Crowny-Editor.exe --render-api vulkan
Crowny-Editor.exe --render-api=gl
Crowny-Editor.exe --opengl --cook-builtins
```

`--render-api` accepts `vulkan`, `vk`, `opengl`, or `gl`, ignoring value case.
`--vulkan` and `--opengl` are shortcuts. The first renderer option wins.
An invalid renderer value emits a diagnostic and uses Vulkan. Missing values are launch errors.
`--cook-builtins` compiles built-in resources and exits after renderer initialization.
The editor rejects unknown options and positional arguments, including arguments after `--`.

## Launch projects, scenes, and captures

```powershell
Crowny-Editor.exe --project "C:\Projects\My Game"
Crowny-Editor.exe --project "C:\Projects\My Game" --scene Assets/Scenes/Level.cwscene --play
Crowny-Editor.exe --project "C:\Projects\My Game" --scene Assets/Scenes/Level.cwscene --render level.bmp --width 1920 --height 1080 --frames 5 --quit
Crowny-Editor.exe --project "C:\Projects\My Game" --scene Assets/Scenes/Level.cwscene --render camera.bmp --scene-camera --quit --opengl
```

`--project` overrides the saved last project and must name a directory containing
`Assets`. Relative project and capture paths resolve against the shell's working
directory. Relative scene paths resolve against the project root. Scenes must be
inside that project's `Assets` folder and load through the usual import workflow.
Without `--scene`, the editor restores the project's last scene as usual.

`--play` enters Play after imports and scene loading finish. `--render` waits for
imports, renders a fixed-size viewport, and writes a 32-bit BMP without UI, grid,
or selection overlays. It uses the saved editor camera by default. `--scene-camera`
uses the scene's primary camera without entering Play; `--play --render` captures
the running scene through its primary camera. Both require a primary camera.

Capture defaults are 1280 by 720 pixels and three rendered frames. `--width` and
`--height` accept 1 through 8192; `--frames` accepts 1 through 1000000. Frames are
counted after imports and scene loading finish. Play uses normal runtime timing,
so a frame count does not guarantee deterministic simulation time.

The editor stays open after capture unless `--quit` is supplied. Captures use a
normal editor window and require a working graphics device. Existing output files
are overwritten and missing output directories are created.

`--scene`, `--play`, and `--render` require an explicit `--project`. Capture modifiers
require `--render`. Launch, scene load, and capture errors exit with status 1;
successful captures with `--quit` exit with status 0. `--help` prints available
options without starting the renderer.
