# Crowny Builder

`Crowny-Builder` is the headless adapter for one player-build target. It loads an editor-generated request, selects the saved target, and passes one `BuildPipelineRequest` to `BuildPipeline`. It does not compile Crowny or reimplement any build stage.

```text
Crowny-Builder build --request <file> [--report <file>] [--format text|json]
```

Relative paths in the request use `ProjectRoot` as their base, except `ProjectRoot` itself, which uses the request file's directory. Installed player-template and managed-toolchain paths may be absolute. The editor and CI should write request files. Project users should not need to edit them.

`EngineVersion` must match the builder binary. Managed compiler timeouts are limited to 30 minutes. `Ctrl+C` cancels an active managed compiler process and returns exit code `5` after cleaning its staging output.

```yaml
Schema: 1
ProjectRoot: ../MyProject
OutputDirectory: ../Builds/Windows
GameSettings: ProjectSettings/Game.yaml
BuildProfile: ProjectSettings/BuildProfiles/default.yaml
BuildTarget: 11111111-2222-3333-4444-555555555555
ContentDatabase: Internal/Build/ContentDatabase.yaml
Managed:
  ToolchainRoot: C:/Crowny/Toolchains/Mono
  Sources:
    - Assets/Scripts/Game.cs
  References:
    - Managed/CrownySharp.dll
  Symbols:
    - CROWNY_GAME
  LanguageVersion: "9.0"
  TimeoutMilliseconds: 120000
  MaxCapturedOutputBytes: 1048576
TemplateRoot: C:/Crowny/Templates/WindowsX64/Development
TemplateManifest: template.yaml
EngineVersion: 1.0.0
MonoVersion: 6.12.0
```

`--format json` writes one JSON object to standard output. `--report` atomically writes the same structured result for text and JSON invocations. Exit codes are stable: `0` success, `2` invalid command line, `3` invalid request input, `4` build failure, `5` cancellation, and `70` an internal builder or report-writing failure.
