# Crowny game builds

This context names the concepts used to turn a Crowny project into a standalone game that runs without the editor or separately installed prerequisites.

## Language

**Player build**:
A standalone game produced for one build target in a build profile. It contains the staged runtime, managed code, content, and configuration for a supported target system.
_Avoid_: Export, game export

**Build profile**:
A saved, user-named specification for a related set of player builds. It owns shared scenes, content rules, symbols, and quality policy plus one or more build targets.
_Avoid_: Platform info, build settings

**Build target**:
A platform and architecture entry inside a build profile that selects its build configuration and any platform-specific overrides. Resolving one build target produces one player build.
_Avoid_: Platform info, build profile

**Player template**:
A versioned, ready-to-package native Crowny runtime for one target platform and architecture. Project code and content are added to it to create a player build.
_Avoid_: Prebuilt project, runtime shell

**Build configuration**:
The development or shipping policy that controls optimization, diagnostics, debugging support, and symbols in a player build.
_Avoid_: Quality level, tier

**Quality tier**:
A named set of runtime performance and fidelity settings. A build profile chooses the default and allowed tiers, but the tier is not a build configuration.
_Avoid_: Build configuration, graphics preset

**Content root**:
A scene, asset, or folder explicitly included in a player build. Every runtime asset it depends on is included automatically.
_Avoid_: Included asset

**Content pack**:
A deployable archive containing cooked runtime assets and the catalog needed to locate them.
_Avoid_: Asset folder, bundle

**Build manifest**:
The player build's startup contract. It identifies the required engine, player ABI, content schema, target, startup scene, quality policy, and staged paths.
_Avoid_: Build profile, project settings

**Player ABI**:
The version of the contract between a player template and the engine runtime. A build cannot stage a template with an incompatible player ABI.
_Avoid_: Engine version, content schema

**Content schema**:
The version of the serialized runtime content contract understood by a player template.
_Avoid_: Player ABI, asset version

**Cook**:
To produce validated, target-ready runtime content from project assets and their dependency graph.
_Avoid_: Import, compile assets

**Streaming file**:
A project file copied unchanged into a player build for direct runtime access by game code, rather than imported and loaded through the asset system.
_Avoid_: Raw asset, included file

**Managed dependency**:
A third-party .NET assembly and its assembly references that contain no native runtime payload or platform invocation and can therefore run on Crowny's supported Mono targets.
_Avoid_: Native plugin, package
