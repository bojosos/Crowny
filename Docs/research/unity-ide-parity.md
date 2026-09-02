# Unity IDE parity notes

Reviewed 2026-09-01 against Unity's official `com.unity.ide.visualstudio` package at commit `66ffcb4`. This captures Crowny's baseline before the VS Code adapter work in this change. It is not a proposal to import Unity code or its Unity-specific debugger protocol.

## What Unity's package does

- It registers one external-editor integration that discovers Visual Studio and Visual Studio Code, generates the selected editor's project model, opens files, and synchronizes after relevant asset changes. Its incremental path reacts to source, DLL, and assembly-definition changes, then updates only the affected projects. [Registration and editor flow](https://github.com/Unity-Technologies/com.unity.ide.visualstudio/blob/66ffcb4f6931221c6125aa7eba354e5389998533/Packages/com.unity.ide.visualstudio/Editor/VisualStudioEditor.cs#L169-L232), [incremental generator](https://github.com/Unity-Technologies/com.unity.ide.visualstudio/blob/66ffcb4f6931221c6125aa7eba354e5389998533/Packages/com.unity.ide.visualstudio/Editor/ProjectGeneration/ProjectGeneration.cs#L93-L188)
- Its solution writer preserves user solution folders and external projects while replacing its own generated entries. That is compatible with Crowny keeping template-based `.sln` output. [Solution merge logic](https://github.com/Unity-Technologies/com.unity.ide.visualstudio/blob/66ffcb4f6931221c6125aa7eba354e5389998533/Packages/com.unity.ide.visualstudio/Editor/ProjectGeneration/ProjectGeneration.cs#L834-L898)
- It builds each generated project from the compiler's real graph: source files, DLL and response-file references, project references, defines, language version, analyzers, rulesets, `.editorconfig`, and additional files. It avoids rewriting unchanged files and provides generation callbacks. [Project graph](https://github.com/Unity-Technologies/com.unity.ide.visualstudio/blob/66ffcb4f6931221c6125aa7eba354e5389998533/Packages/com.unity.ide.visualstudio/Editor/ProjectGeneration/ProjectGeneration.cs#L477-L544), [analyzers and language version](https://github.com/Unity-Technologies/com.unity.ide.visualstudio/blob/66ffcb4f6931221c6125aa7eba354e5389998533/Packages/com.unity.ide.visualstudio/Editor/ProjectGeneration/ProjectGeneration.cs#L598-L687)
- It lets users choose which package origins generate projects: embedded, local, registry, Git, built-in, tarball, unknown, and player assemblies. [Package-generation options](https://github.com/Unity-Technologies/com.unity.ide.visualstudio/blob/66ffcb4f6931221c6125aa7eba354e5389998533/Packages/com.unity.ide.visualstudio/Documentation~/using-visual-studio-editor.md), [flags](https://github.com/Unity-Technologies/com.unity.ide.visualstudio/blob/66ffcb4f6931221c6125aa7eba354e5389998533/Packages/com.unity.ide.visualstudio/Editor/ProjectGeneration/ProjectGenerationFlag.cs)
- It discovers preview Visual Studio installations through `vswhere`. Version 18 uses SDK-style projects. [Windows discovery and generator choice](https://github.com/Unity-Technologies/com.unity.ide.visualstudio/blob/66ffcb4f6931221c6125aa7eba354e5389998533/Packages/com.unity.ide.visualstudio/Editor/VisualStudioForWindowsInstallation.cs#L124-L258)
- For VS Code it discovers Code and Code Insiders, prepares `.vscode/launch.json`, `settings.json`, and `extensions.json`, supports a user opt-out file, opens one `.code-workspace` when present, and otherwise opens the project directory with `-g file:line:column`. [Discovery](https://github.com/Unity-Technologies/com.unity.ide.visualstudio/blob/66ffcb4f6931221c6125aa7eba354e5389998533/Packages/com.unity.ide.visualstudio/Editor/VisualStudioCodeInstallation.cs#L67-L229), [workspace configuration](https://github.com/Unity-Technologies/com.unity.ide.visualstudio/blob/66ffcb4f6931221c6125aa7eba354e5389998533/Packages/com.unity.ide.visualstudio/Editor/VisualStudioCodeInstallation.cs#L231-L499), [file opening](https://github.com/Unity-Technologies/com.unity.ide.visualstudio/blob/66ffcb4f6931221c6125aa7eba354e5389998533/Packages/com.unity.ide.visualstudio/Editor/VisualStudioCodeInstallation.cs#L512-L538)
- Debugging is a separate editor-to-IDE contract. Unity seeds an attach configuration, exposes a per-editor debug port, and handles play, pause, stop, refresh, project-path, and test messages. [Attach configuration](https://github.com/Unity-Technologies/com.unity.ide.visualstudio/blob/66ffcb4f6931221c6125aa7eba354e5389998533/Packages/com.unity.ide.visualstudio/Editor/VisualStudioCodeInstallation.cs#L249-L303), [control channel](https://github.com/Unity-Technologies/com.unity.ide.visualstudio/blob/66ffcb4f6931221c6125aa7eba354e5389998533/Packages/com.unity.ide.visualstudio/Editor/VisualStudioIntegration.cs#L35-L175)

## Implemented from this review

This change adds a cross-platform VS Code adapter. It discovers Code and Code Insiders on Windows, macOS, and Linux; opens a single project-local workspace when present; otherwise opens the project folder; and supports argument-safe file-and-line navigation. It writes an additive `.vscode` setup (C# extension recommendations, default solution, and a CoreCLR attach profile) that preserves valid user JSON and can be disabled with `.vscode/.crowny-vscode-patch-disable`.

Project output is now content-stable: unchanged projects, solutions, and VS Code configuration files are left untouched. Solution regeneration retains the template-produced Crowny solution while preserving externally owned project blocks, solution folders, nesting, and project-configuration entries.

The code-editor manager now has a debounced `SyncIfNeeded` path. Asset changes to C# source, managed DLLs, project files, rulesets, response files, `.editorconfig`, and future assembly-definition files are batched; script creation and managed-dependency setting changes use the same path. The manager snapshots referenced binary/project timestamps too, so declared DLLs outside the asset folder still trigger a graph re-evaluation. It fingerprints the resolved graph before calling an adapter, while adapters report success separately from output changes so a failed write is not cached as current.

The remaining major work is enriching the graph from the compiler and defining a real debugger contract. The generated CoreCLR attach profile is intentionally a standard process picker, not a claim of Unity debugger-protocol compatibility; Mono receives no misleading debug profile.

## Crowny baseline

Crowny now has a version-aware Visual Studio generator, including VS 2026, and retains its template-produced `.sln`. It emits legacy Mono projects and SDK-style CoreCLR projects, then writes explicit source, file, assembly, and project references. [Generator model](../../Crowny-Editor/Source/Editor/Script/ScriptProjectGenerator.h), [project and solution writer](../../Crowny-Editor/Source/Editor/Script/ScriptProjectGenerator.cpp)

Crowny's dependency handling is stronger than Unity's IntelliSense-oriented DLL projection. Declared managed assemblies are stored in project settings, resolved as a closure, added to the generated project, staged for the program, and loaded by both Mono and CoreCLR paths. Keep that invariant. An IDE reference that compiles must also be valid at runtime. [Project synchronization](../../Crowny-Editor/Source/Editor/Script/CodeEditor.cpp), [dependency resolver](../../Crowny-Editor/Source/Editor/Script/ManagedProjectDependencies.cpp), [runtime artifacts](../../Crowny/Source/Crowny/Scripting/Managed/ManagedTypes.h)

The current editor manager creates Visual Studio and MonoDevelop adapters on Windows, and a VS Code adapter on Windows, macOS, and Linux. Visual Studio discovery uses `vswhere` plus legacy registry lookup, opening uses DTE/COM, and solution reload follows generation. [Manager registration](../../Crowny-Editor/Source/Editor/Script/CodeEditor.cpp), [Visual Studio adapter](../../Crowny-Editor/Source/Editor/Script/VisualStudioCodeEditor.cpp), [VS Code adapter](../../Crowny-Editor/Source/Editor/Script/VSCodeEditor.cpp)

Crowny synchronizes at project startup, from a manual settings action, and through a debounced asset/project-input path. The current graph has one game project, with runtime-valid binary dependency closure; it suppresses unchanged output and preserves user-owned projects or folders when re-emitting a solution. [Sync call sites](../../Crowny-Editor/Source/Editor/EditorLayerProject.cpp), [current sync implementation](../../Crowny-Editor/Source/Editor/Script/CodeEditor.cpp)

## Prioritized gaps

### P0: complete compiler-derived project graph inputs

The debounce and graph-change path is implemented. Next, make the build description expose analyzers, rulesets, additional files, project references, per-project language version, and package/assembly definition inputs directly. The IDE model must consume that description rather than scan substitute framework references.

Crowny should derive every `<Reference>`, `<ProjectReference>`, define, language version, analyzer, ruleset, and additional file from the same build description used for the selected runtime. Do not substitute the IDE's framework references for Crowny's runtime contract.

### P1: represent managed code as a project graph

`CodeSolutionData` currently produces one game project. Add explicit game, editor-only, test, and reusable managed-library nodes when the build system exposes them. Project dependencies should become `ProjectReference`s; external managed assemblies remain validated binary references. Give users a narrowly scoped inclusion policy for future package sources instead of copying Unity's package-origin flags before Crowny has an equivalent package manager.

### P2: build a debugger contract before launch profiles

Define a Crowny debug adapter or Visual Studio extension contract first: endpoint discovery, attach, breakpoints, step control, threads, stack frames, variables, and lifecycle commands. Then generate `launch.json` and Visual Studio profiles from that contract. Project files alone cannot create managed debugging.

### P2: add IDE setup diagnostics

Report missing .NET SDKs, the chosen solution format, unresolved analyzers, unsupported project language versions, and the active editor's executable/version. A Crowny `.vsconfig` can be offered later, but it must name Crowny's actual requirements rather than Unity's Managed Game workload.

## Recommended implementation order

1. Multi-project graph plus compiler-derived analyzers, rulesets, and additional files.
2. Explicit `.slnx` opt-in once the SDK floor and supported tooling are pinned.
3. Debug-adapter contract and then IDE launch profiles.
