# Windows edit-build-test loop

Run `Scripts\setup-windows.ps1` once to install and build prerequisites. Normal builds should use the focused commands below; they do not run Winget, update submodules, rebuild dependencies, or build unrelated executables.

```powershell
# Engine only, using the automatic compiler share
Scripts\build-windows.ps1 -Target Engine

# Engine and tests in one targeted MSBuild invocation, then the ordinary suite
Scripts\test-windows.ps1

# Routine sanitizer lane
Scripts\test-windows.ps1 -Sanitizer Address

# Hidden managed-process lane followed by the ordinary suite
Scripts\test-windows.ps1 -Sanitizer Address -ProcessIsolated

# Explicit full-editor sanitizer build
Scripts\build-windows.ps1 -Target Editor -Sanitizer Address
```

`-Jobs` controls MSVC translation-unit parallelism. The scripts deliberately keep MSBuild project parallelism at one, preventing the compiler count from multiplying across projects. Auto mode uses eight workers for the first build on this 12-thread machine and leaves four available for a concurrent build. Pass `-Jobs 12` when one build should wait for and use the complete compiler budget.

Project generation is exclusive, and writes to the same configuration output family are serialized with the owning PID and command reported while waiting. ASan writers also lock their mapped Debug or Release dependency outputs. Independent output families may compile concurrently within the shared 12-worker budget. After a build, tests take a shared read lock, so several agents can run the same stable configuration concurrently while a writer waits. Render comparisons remain exclusive because they share artifact paths.

The generated Visual Studio solution contains `Debug`, `Release`, `Dist`, `DebugASan`, and `ReleaseASan`. `--sanitizer=address` is still accepted by Premake for compatibility, but normal and ASan modes no longer require separate project generation.

Use the measurement command for repeatable clean, no-op, and one-file incremental probes:

```powershell
Scripts\measure-build-windows.ps1 -Target Tests -Scenario Clean,NoOp,TouchedSource
Scripts\measure-build-windows.ps1 -Target Engine -Configuration Release -Sanitizer Address -BuildInsights
```

Results, MSBuild binary logs, compiler-memory samples, and optional Build Insights traces are written below `artifacts\build-metrics`.

sccache remains opt-in. Install it, run `Scripts\probe-sccache-windows.ps1`, then pass `-CompilerCache Sccache` to the build command. Do not make it the default unless a representative second clean rebuild reaches at least 70% cache hits and improves wall time by at least 30%.
