# Shippable build systems research

Research date: 2026-08-25

Scope: Unity 6, current stable Godot 4.x, and Unreal Engine 5.8. Only first-party documentation is cited. The target Crowny design assumes C# game code, Mono at runtime, Windows x64 and Linux x64, and versioned native player templates built by Crowny maintainers.

## Conclusion

Crowny does not need to compile C++ when a user builds a game. It can compile the game's C# to portable managed assemblies, cook content, and combine both with an already-built player template for the target OS. Windows can therefore package Linux and Linux can package Windows without a native cross-compiler. A native compiler returns only if Crowny later accepts user C++ modules, native plugins, or a build option that relinks the engine.

The best design is a deliberate mix of the three engines:

- Take versioned, target-specific player templates and simple export presets from Godot.
- Take version-controlled build profiles, per-profile scene lists, scripting defines, and runtime quality levels from Unity.
- Take Unreal's explicit `Build`, `Cook`, `Stage`, `Package`, `Deploy`, and `Run` boundaries, plus its distinction between referenced assets and explicit cook roots.

The first release should create one deterministic pack named from the game, such as `Crownfall.cwpack`. Included scenes and explicit content roots seed a dependency walk. The build report should record why every asset entered the pack. The file format should support multiple named packs later without exposing that complexity in the first UI.

## Comparison

| Concern | Unity 6 | Godot 4.x | Unreal Engine 5.8 | Crowny implication |
| --- | --- | --- | --- | --- |
| Saved build setup | Multiple version-controlled Build Profile assets per platform | Named export presets in a source-controlled config, with secrets stored separately | Project Launcher profiles plus project packaging settings | Saved build profiles, edited only through the editor, with local secrets and output paths kept outside tracked profile data |
| Scenes | Ordered global list or a profile override | Selected scenes with dependencies, or all project resources | Maps selected by profile, command line, or packaging settings | Ordered scene list per profile, with an explicit startup scene |
| Automatic content | Direct reference graph from included scenes and project settings | Dependencies of selected scenes or resources | Referenced assets discovered during cook, with primary assets managing secondary assets | Scene roots plus serialized runtime dependency closure |
| Dynamic content | `Resources` and Addressables provide explicit runtime-loadable roots | Selected resources, include filters, or all resources | Always-cook assets and directories, Primary Asset Labels | Explicit asset and folder content roots for C# path or UUID loads |
| Content archives | Player data plus platform-specific AssetBundles or Addressables | Main PCK or ZIP, plus patch and DLC packs | Pak or IoStore containers split into chunks | One game-named `.cwpack` in version one, with format support for named packs and patches later |
| Quality | Named runtime quality levels, filtered and defaulted per platform | No equivalent tier matrix; project-setting overrides and custom feature tags are the nearest build-time mechanism | Scalability buckets selected or overridden by device profiles | Named runtime tiers, with each profile choosing the allowed set and default |
| Managed code | Mono or IL2CPP; profile symbols affect compilation | Modern .NET; build tools require an SDK and exported games carry runtime support | C# runs build automation, not first-party gameplay | Compile C# once per profile because symbols can differ; ship Mono in the target template |
| Windows and Linux cross-host packaging | Desktop target modules support standalone targets; IL2CPP Linux needs a cross-toolchain because it emits native code | Target export templates are prebuilt binaries installed by platform and architecture | Official native cross-compilation is Windows to Linux only | With C# IL and prebuilt templates, Crowny needs no native cross-toolchain in either direction |

## Unity 6

### Profiles, scenes, and automation

Unity Build Profiles save multiple independent configurations for the same platform. Unity stores them as project assets suitable for version control. A profile can override the global ordered scene list and add scripting defines. Development builds, script debugging, clean builds, and incremental builds are exposed in the same window. [Build Profiles](https://docs.unity3d.com/6000.0/Documentation/Manual/build-profiles.html), [Build Profiles window reference](https://docs.unity3d.com/6000.0/Documentation/Manual/build-profiles-reference.html), [manage scenes in a build](https://docs.unity3d.com/6000.0/Documentation/Manual/build-profile-scene-list.html)

Unity exposes the same model to batch builds. A command can select a saved profile or a target, but Unity recommends one target per editor process because switching targets reloads editor assemblies. Platform-specific conditional compilation and assembly selection can change the output. Crowny should copy the reproducibility and CLI parity, but it does not need Unity's expensive target switching. A Crowny build process can load one immutable profile snapshot per job. [Build a player from the command line](https://docs.unity3d.com/6000.0/Documentation/Manual/build-command-line.html)

### Content selection and packs

Unity starts from scenes in the build list and follows direct object references. Assets inside any `Resources` directory are always included even when no scene references them, which gives scripts an explicit convention for dynamic lookup. Unity records scene data and shared referenced assets so that a referenced object is built once instead of copied for every scene. [Content output of a build](https://docs.unity3d.com/6000.0/Documentation/Manual/build-content-output.html)

Addressables provide the more scalable dynamic-content model. An addressable asset can be requested by address, and the system locates the asset and its dependencies in local or remote bundles. Traditional AssetBundles are target-specific. They contain data, not new C# assemblies, and Unity does not support loading a bundle produced by a newer editor into an older player. This is a good warning for Crowny: the pack header needs an engine ABI and content-schema version, and compatibility must be checked before deserialization. [Addressables](https://docs.unity3d.com/6000.0/Documentation/Manual/com.unity.addressables.html), [AssetBundle introduction and compatibility](https://docs.unity3d.com/2023.2/Documentation/Manual/AssetBundlesIntro.html), [BuildPipeline.BuildAssetBundles](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/BuildPipeline.BuildAssetBundles.html)

Unity's useful default is automatic dependency inclusion with an explicit escape hatch for dynamic loads. Crowny should not adopt `Resources` as a magic folder. Assets and folders selected in a Build Profiles UI are clearer, can be validated, and can carry an inclusion reason in the build report.

### Quality and C#

Unity has named quality levels. A project chooses which levels apply to each platform, chooses a platform default, and can change the active level at runtime. Quality settings include texture, shadow, level-of-detail, mesh, and renderer settings. This is a runtime policy, not a Development or Shipping code configuration. [Quality settings](https://docs.unity3d.com/6000.0/Documentation/Manual/class-QualitySettings.html), [QualitySettings API](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/QualitySettings.html)

Unity 6 supports Mono on Windows x64 and Linux x64. Mono compiles managed assemblies to native instructions at runtime. IL2CPP instead converts IL to C++ and invokes a native compiler. Unity's Linux IL2CPP support ships host-specific toolchains and a fixed sysroot, including a Windows-to-Linux toolchain. That machinery exists because IL2CPP generates native code. It is not needed for Crowny's Mono plan. [Scripting back ends](https://docs.unity3d.com/6000.0/Documentation/Manual/scripting-backends-intro.html), [Linux IL2CPP cross-compiler](https://docs.unity3d.com/6000.0/Documentation/Manual/linux-il2cpp-crosscompiler.html)

### Lesson for Crowny

Build profiles should own the target, build configuration, ordered scenes, extra C# symbols, content roots, default quality tier, allowed quality tiers, and packaging options. They should not own machine-local output history, credentials, or signing keys. The editor should be the normal way to create and edit profiles even if YAML is the storage format.

## Godot 4.x

### Presets and player templates

Godot is the closest existing example of the proposed Crowny player-template model. Export presets can produce one selected build or all configured builds. Export requires installed templates for the requested platform and architecture. The official template package contains separate debug and release executables for Windows and Linux, along with a `version.txt` identifier used to install templates into a version-specific directory. [Exporting projects](https://docs.godotengine.org/en/stable/tutorials/export/exporting_projects.html), [build system and export templates](https://docs.godotengine.org/en/stable/engine_details/development/compiling/introduction_to_the_buildsystem.html)

Godot keeps most preset settings in `export_presets.cfg`, which can be committed. It puts passwords and encryption keys in `.godot/export_credentials.cfg`, which should stay untracked. Command-line exports still name a preset, so GUI and CI use the same saved configuration. Crowny should make the same separation, but it should hide the YAML implementation behind the editor UI. [Exporting projects](https://docs.godotengine.org/en/stable/tutorials/export/exporting_projects.html)

### Content selection and packs

A Godot preset can export all resources, selected scenes and dependencies, selected resources and dependencies, all resources except selected exclusions, or a dedicated-server subset. It also has include and exclude filters for non-resource files. Windows and Linux exports combine a smaller non-editor template binary with a `data.pck`. The export tool can emit only a PCK or ZIP, and its documentation allows a main pack to be used with multiple Godot executables. [Exporting projects](https://docs.godotengine.org/en/stable/tutorials/export/exporting_projects.html), [exporting for Windows](https://docs.godotengine.org/en/stable/tutorials/export/exporting_for_windows.html), [exporting for Linux](https://docs.godotengine.org/en/stable/tutorials/export/exporting_for_linux.html)

Godot can load additional PCK or ZIP files for patches, DLC, and mods. Patch export compares against one or more base packs and emits changed resources. Its docs also warn that delta patches depend on the exact base files and load order, and that Godot exports can be nondeterministic. Crowny should design pack identity, hashing, mount priority, and deterministic output now, while leaving patch creation out of the first release. [Exporting packs, patches, and mods](https://docs.godotengine.org/en/stable/tutorials/export/exporting_pcks.html)

### Quality and C#

Godot lacks Unity's dedicated quality-level matrix. Export presets can add custom feature tags, code can query them at runtime, and project settings can have platform, debug, release, or custom-feature overrides. These tools are useful for build variants, but a game still needs its own runtime quality menu and policy. Godot's Compatibility renderer is its explicit low-end option. [Feature tags](https://docs.godotengine.org/en/stable/tutorials/export/feature_tags.html), [overview of renderers](https://docs.godotengine.org/en/stable/tutorials/rendering/renderers.html)

Godot 4 C# uses modern .NET and supports Windows, Linux, and macOS desktop exports. The editor needs a .NET SDK because Godot does not bundle MSBuild or the C# compiler. Godot does bundle the runtime pieces needed to run already-compiled games. This cleanly separates build-machine tools from end-user runtime dependencies. Crowny should do the same with its chosen compiler and bundled Mono runtime. [C# basics](https://docs.godotengine.org/en/stable/tutorials/scripting/c_sharp/c_sharp_basics.html), [C# platform support](https://docs.godotengine.org/en/stable/tutorials/scripting/c_sharp/index.html)

### Cross-host lesson for Crowny

Godot's export package contains target-native binaries for several platforms and architectures, and the editor selects the required template rather than compiling that binary during a game export. This is the model Crowny needs. Code signing remains a separate concern. For example, Godot can sign a Windows export on Windows with SignTool or on another OS with `osslsigncode`. [Exporting for Windows](https://docs.godotengine.org/en/stable/tutorials/export/exporting_for_windows.html)

## Unreal Engine 5.8

### Pipeline and profiles

Unreal names each build operation: `Build` compiles executables, `Cook` converts content, `Stage` assembles a standalone directory, `Package` creates the platform distribution, `Deploy` transfers it, and `Run` launches it. AutomationTool's `BuildCookRun` command and Project Launcher profiles drive the same operations. This makes failures, caching, logs, and CI easier to reason about than a single opaque "Build Game" action. [Build operations](https://dev.epicgames.com/documentation/en-us/unreal-engine/build-operations-cooking-packaging-deploying-and-running-projects-in-unreal-engine), [Packaging a project](https://dev.epicgames.com/documentation/en-us/unreal-engine/packaging-your-project)

Crowny should expose one Build button while retaining these internal stages. A build result should say exactly which stage failed. The standalone builder should also support validation-only, cook-only, pack-only, full build, and build-and-run commands without forcing those choices into the first editor screen.

### Content ownership and chunks

Unreal cooking converts assets to target-ready formats and excludes unreferenced content. Maps can be selected explicitly. Packaging settings can also name asset directories that must always cook, even when nothing references them. This is the same problem as a C# script loading an asset by path at runtime. [Packaging settings reference](https://dev.epicgames.com/documentation/en-us/unreal-engine/project-section-of-the-unreal-engine-project-settings), [Content cooking](https://dev.epicgames.com/documentation/en-us/unreal-engine/cooking-content-in-unreal-engine)

Unreal's Asset Manager distinguishes Primary Assets from automatically loaded Secondary Assets. Primary Asset Rules and Primary Asset Labels assign ownership and cook rules. A chunk contains a primary asset and the secondary assets it manages, and each non-default chunk can become a separate Pak file for independent delivery. [Asset management](https://dev.epicgames.com/documentation/en-us/unreal-engine/asset-management-in-unreal-engine), [Cooking and chunking](https://dev.epicgames.com/documentation/en-us/unreal-engine/cooking-content-and-creating-chunks-in-unreal-engine)

Crowny does not need Unreal's full ownership system in version one. It does need the underlying distinction:

- A scene or explicit asset is a content root.
- Serialized runtime references add dependencies automatically.
- A script-only lookup must name an explicit asset or folder root because static dependency discovery cannot prove an arbitrary string.
- Each asset in the report records its root and dependency path.

The first main pack should have a deterministic, sanitized game-derived filename. The pack format and catalog should include a pack ID so a later profile can assign roots to named packs without changing asset identity or lookup APIs.

### Quality and code

Unreal separates scalability settings from device profiles. Scalability buckets define values such as Low, Medium, High, and Epic for groups of rendering settings. Device profiles select or override settings for hardware families and PC or Mac buckets. This supports the same Crowny conclusion as Unity: quality is runtime policy, while Development and Shipping are code and diagnostics policies. [Device profiles](https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-device-profiles-in-unreal-engine), [device profiles and scalability](https://dev.epicgames.com/documentation/en-us/unreal-engine/customizing-device-profiles-and-scalability-in-unreal-engine-projects-for-android)

Unreal's first-party gameplay languages are C++ and Blueprint. Its AutomationTool is written and extended in C#, but that C# controls builds rather than running as game code. Unreal is therefore useful as a pipeline and content-management comparison, not as a managed-player template. [Blueprint versus C++](https://dev.epicgames.com/documentation/en-us/unreal-engine/coding-in-unreal-engine-blueprint-vs-cplusplus), [AutomationTool](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-automation-tool-overview-for-unreal-engine)

### Cross-host lesson for Crowny

Epic officially supports Windows-to-Linux native cross-compilation with a pinned Clang toolchain and fixed sysroot. The current Linux floor is glibc 2.28. Epic documents that cross-compilation support in this workflow is Windows-hosted; it does not document a corresponding Linux-to-Windows path. [Linux development requirements](https://dev.epicgames.com/documentation/en-us/unreal-engine/linux-development-requirements-for-unreal-engine)

Crowny can avoid this asymmetry for game builds. Crowny maintainers build and test each native player template on the proper toolchain. A user's Linux host packages the Windows template, and a Windows host packages the Linux template. The only compiled project artifact is C# IL. Target-specific content cooking can still differ because shaders, texture compression, audio codecs, and path case rules may differ.

## Recommended Crowny contract

### Player templates

Ship four version-matched templates at first: Windows x64 Development, Windows x64 Shipping, Linux x64 Development, and Linux x64 Shipping. The editor includes its host templates and installs the other target as a version-matched module. Each module needs a manifest with:

- Crowny engine version and player ABI.
- Content schema versions the player can read.
- Target OS, architecture, build configuration, and renderer support.
- File hashes and executable permission metadata.
- Bundled runtime libraries, Mono files, notices, and licenses.

Exact engine and content-schema compatibility should be the shipping default. A profile option may allow only a declared compatible schema range. The builder must never silently combine an arbitrary player and pack or offer a generic version-check bypass.

### Build profiles and UI

Store profiles as tracked data, but make manual editing unnecessary. The editor owns profile creation, duplication, validation, and migration. A useful initial split is:

- Project game settings hold product name, version, company, icons, and window defaults.
- Build profiles hold target, Development or Shipping, scene order, startup scene, C# symbols, content roots, quality default and allowed tiers, packaging, compatibility policy, and output naming.
- User-local settings hold recent output directories, installed template paths, signing identities, and credentials.

The CLI consumes the same profile data as the editor. It should never depend on the currently open editor state.

### Build stages

Use these stable stages and cache boundaries:

1. `Validate`: resolve the profile, template, scenes, assets, compiler, and output policy.
2. `CompileScripts`: compile C# once for that profile's symbols and references.
3. `ResolveContent`: walk scene and explicit-root dependencies and explain inclusion.
4. `Cook`: produce target and tier-aware runtime asset variants.
5. `Pack`: write a deterministic `<SanitizedGameName>.cwpack` and catalog.
6. `Stage`: copy the player template, Mono runtime, native libraries, assemblies, pack, licenses, and configuration into a clean directory.
7. `Verify`: inspect hashes and dependencies, launch on the host target when possible, and emit a machine-readable report.
8. `Archive`: optionally produce a ZIP or platform distribution artifact.

Cross-host builds can complete through `Stage` and static verification. Running the result requires the target OS or a configured VM or remote test machine.

### C# and native-code boundary

Compile per profile even though CIL is portable. Target, Development or Shipping, and custom symbols can change conditional code and serialized types. Cache by compiler version, engine API assemblies, source hashes, references, and the complete symbol set.

The no-cross-compiler guarantee holds only while game code is managed. P/Invoke libraries, native NuGet assets, and future native plugins must either be forbidden or supplied as prebuilt binaries for every selected target. They must also declare their runtime dependencies so staging and clean-machine verification can catch omissions.

### Content and quality

Version one should expose two inclusion mechanisms in the editor: ordered scenes and explicit asset or folder roots. Both feed one dependency graph. Missing dependencies, excluded referenced assets, cycles, duplicate logical paths, case-only path collisions, and assets with no target cooker should fail or warn before the pack is written.

Use Low, Medium, High, and Ultra as editable runtime tiers. The build profile chooses the default and allowed tiers. A Shipping build can still contain multiple tiers, letting the game adapt on first launch or expose an options menu. Cook-time variants should be introduced only where runtime settings cannot solve the cost, such as texture payloads or shader sets.

## Caveats that should become acceptance tests

- "Runs on any machine" needs a published support floor. Test Windows and Linux clean VMs with 8 GB RAM and hardware near the chosen low-end target. Record CPU instruction, GPU API, driver, Windows version, Linux distribution, kernel, and glibc requirements.
- Bundling Mono is not enough. Every native dependency loaded by the player or a managed assembly must come from the template or the operating system support floor.
- A portable C# assembly can still contain platform-specific code, P/Invoke, case-sensitive paths, or conditional serialized fields. Compile and validate each profile independently.
- A Linux executable staged on Windows needs executable permission metadata preserved in the archive. A raw copy to a Windows filesystem cannot prove that the bit will survive distribution.
- Cross-host packaging is different from cross-host testing. CI needs at least one real or virtual runner for each target OS.
- Content compatibility is directional. Reject newer schemas in older players. Treat any broader compatibility mode as an explicit, reported choice.
- Signing and store submission are separate stages. Packaging should work without them, while release profiles can require them later.
