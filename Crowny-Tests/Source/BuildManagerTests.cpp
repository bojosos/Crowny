#include <catch2/catch_test_macros.hpp>

#include "Build/BuildManager.h"

#include <chrono>
#include <fstream>

namespace Crowny
{
    namespace
    {
        struct TemporaryDirectory
        {
            TemporaryDirectory()
            {
                Root = fs::temp_directory_path() /
                       ("crowny-build-manager-tests-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
                fs::create_directories(Root);
            }

            ~TemporaryDirectory()
            {
                std::error_code error;
                fs::remove_all(Root, error);
            }

            Path Root;
        };

        void WriteText(const Path& path, StringView text)
        {
            fs::create_directories(path.parent_path());
            std::ofstream stream(path, std::ios::binary | std::ios::trunc);
            stream.write(text.data(), static_cast<std::streamsize>(text.size()));
        }

        EditorBuildInputs MakeInputs(const TemporaryDirectory& temporary, BuildPlatform platform, BuildConfiguration configuration)
        {
            const UUID scene("aaaaaaaa-1111-2222-3333-444444444444");
            EditorBuildInputs inputs;
            inputs.ProjectRoot = temporary.Root / "Project";
            fs::create_directories(inputs.ProjectRoot);
            inputs.Game.ProductName = "Build Manager Test";
            inputs.Game.ArtifactName = "BuildManagerTest";
            inputs.Game.ProductVersion = "1.0.0";
            inputs.Game.Company = "Crowny";
            inputs.HasGameSettings = true;
            inputs.Content.Assets = {
                { scene, "Assets/Scenes/Main.cwscene", "Cooked/DatabaseOwned/main.payload", {}, "Scene", "scene-source" },
            };
            inputs.HasContentDatabase = true;
            WriteText(inputs.ProjectRoot / "Cooked/DatabaseOwned/main.payload", "cooked-scene");
            inputs.TemplateRoot = temporary.Root / "Template";
            WriteText(inputs.TemplateRoot / "Player.bin", "player-template");
            PlayerTemplateManifest templateDescription;
            templateDescription.EngineVersion = "1.0.0";
            templateDescription.Platform = platform;
            templateDescription.Configuration = configuration;
            templateDescription.Renderers = { RendererBackend::Vulkan, RendererBackend::OpenGL };
            REQUIRE(PlayerTemplateStore::CreateManifest(inputs.TemplateRoot, templateDescription, {}, inputs.Template).empty());
            inputs.HasTemplate = true;
            inputs.EngineVersion = "1.0.0";
            inputs.MonoVersion = "6.12";
            return inputs;
        }

        void ConfigurePlatform(BuildManager& manager, PlatformType platform, const TemporaryDirectory& temporary, const UUID& scene, bool debug)
        {
            manager.SetActivePlatformInfo(platform);
            const Ref<PlatformInfo> info = manager.GetActivePlatformInfo();
            REQUIRE(info != nullptr);
            info->OutputDirectory = temporary.Root / "Build";
            info->MainScene = scene;
            info->Debug = debug;
        }
    } // namespace

    TEST_CASE("Build manager maps editor platforms into pipeline targets", "[Build][Editor]")
    {
        TemporaryDirectory temporary;
        const UUID scene("aaaaaaaa-1111-2222-3333-444444444444");
        BuildManager manager;
        ConfigurePlatform(manager, PlatformType::Windows, temporary, scene, true);
        manager.GetActivePlatformInfo()->Defines = "ZETA;ALPHA;ZETA";
        EditorBuildInputs windows = MakeInputs(temporary, BuildPlatform::WindowsX64, BuildConfiguration::Development);

        const EditorBuildRequest windowsRequest = manager.PrepareActiveBuild(windows);

        REQUIRE(windowsRequest.IsValid());
        CHECK(windowsRequest.Request.Target.Platform == BuildPlatform::WindowsX64);
        CHECK(windowsRequest.Request.Target.Configuration == BuildConfiguration::Development);
        CHECK(windowsRequest.Request.Target.IncludeSymbols);
        CHECK(windowsRequest.Request.Profile.StartupScene == scene);
        REQUIRE(windowsRequest.Request.Profile.Targets.size() == 1);
        CHECK(windowsRequest.Request.Profile.Targets.front().Id == windowsRequest.Request.Target.Id);
        CHECK(windowsRequest.Request.Profile.Targets.front().Platform == windowsRequest.Request.Target.Platform);
        CHECK(windowsRequest.Request.Profile.Targets.front().Configuration == windowsRequest.Request.Target.Configuration);
        const Vector<String> expectedSymbols{ "ALPHA", "ZETA" };
        CHECK(windowsRequest.Request.Target.Symbols == expectedSymbols);

        ConfigurePlatform(manager, PlatformType::Linux, temporary, scene, false);
        EditorBuildInputs linux = MakeInputs(temporary, BuildPlatform::LinuxX64, BuildConfiguration::Shipping);
        const EditorBuildRequest linuxRequest = manager.PrepareActiveBuild(linux);
        REQUIRE(linuxRequest.IsValid());
        CHECK(linuxRequest.Request.Target.Platform == BuildPlatform::LinuxX64);
        CHECK(linuxRequest.Request.Target.Configuration == BuildConfiguration::Shipping);
        CHECK_FALSE(linuxRequest.Request.Target.IncludeSymbols);
    }

    TEST_CASE("Build manager reports missing project packaging inputs", "[Build][Editor]")
    {
        TemporaryDirectory temporary;
        const UUID scene("aaaaaaaa-1111-2222-3333-444444444444");
        BuildManager manager;
        ConfigurePlatform(manager, PlatformType::Windows, temporary, scene, true);
        EditorBuildInputs inputs;
        inputs.ProjectRoot = temporary.Root / "Project";
        fs::create_directories(inputs.ProjectRoot / "Scripts");
        inputs.Managed.Sources = { inputs.ProjectRoot / "Scripts/Game.cs" };
        inputs.EngineVersion = "1.0.0";
        inputs.MonoVersion = "6.12";

        const EditorBuildRequest request = manager.PrepareActiveBuild(inputs);

        CHECK_FALSE(request.IsValid());
        CHECK(request.Diagnostics.ContainsCode("editor.build.game_settings.missing"));
        CHECK(request.Diagnostics.ContainsCode("editor.build.content_database.missing"));
        CHECK(request.Diagnostics.ContainsCode("editor.build.template.missing"));
        CHECK(request.Diagnostics.ContainsCode("editor.build.toolchain.missing"));
    }

    TEST_CASE("Build manager propagates cancellation and pipeline stage reports", "[Build][Editor]")
    {
        TemporaryDirectory temporary;
        const UUID scene("aaaaaaaa-1111-2222-3333-444444444444");
        BuildManager manager;
        ConfigurePlatform(manager, PlatformType::Windows, temporary, scene, true);
        const EditorBuildInputs inputs = MakeInputs(temporary, BuildPlatform::WindowsX64, BuildConfiguration::Development);

        const EditorBuildReport report = manager.ExecuteActiveBuild(inputs, []() { return true; });

        CHECK(report.PipelineStarted);
        CHECK_FALSE(report.Succeeded());
        CHECK(report.Pipeline.Cancelled);
        REQUIRE(report.Pipeline.Find(BuildPipelineStage::Validate) != nullptr);
        CHECK(report.Pipeline.Find(BuildPipelineStage::Validate)->Status == BuildPipelineStageStatus::Cancelled);
        CHECK(report.Diagnostics.ContainsCode("pipeline.cancelled"));
    }

    TEST_CASE("Build manager preserves asset database paths without guessing packaging inputs", "[Build][Editor]")
    {
        TemporaryDirectory temporary;
        const UUID scene("aaaaaaaa-1111-2222-3333-444444444444");
        BuildManager manager;
        ConfigurePlatform(manager, PlatformType::Windows, temporary, scene, true);
        EditorBuildInputs inputs = MakeInputs(temporary, BuildPlatform::WindowsX64, BuildConfiguration::Development);
        inputs.Managed.Sources = { inputs.ProjectRoot / "AssetApi/Scripts/Exact.cs" };
        inputs.Managed.References = { inputs.ProjectRoot / "AssetApi/Assemblies/Exact.dll" };
        inputs.Toolchain.CompilerAssembly = temporary.Root / "Toolchain/csc.exe";
        inputs.Toolchain.ReferenceDirectory = temporary.Root / "Toolchain/Framework";

        const EditorBuildRequest request = manager.PrepareActiveBuild(inputs);

        CHECK(request.Request.Content.Assets.front().CookedPath == Path("Cooked/DatabaseOwned/main.payload"));
        CHECK(request.Request.Content.Assets.front().LogicalPath == Path("Assets/Scenes/Main.cwscene"));
        CHECK(request.Request.Managed.Sources == inputs.Managed.Sources);
        CHECK(request.Request.Managed.References == inputs.Managed.References);
    }

    TEST_CASE("Build manager rejects unsupported editor platforms before execution", "[Build][Editor]")
    {
        TemporaryDirectory temporary;
        const UUID scene("aaaaaaaa-1111-2222-3333-444444444444");
        BuildManager manager;
        ConfigurePlatform(manager, PlatformType::Mac, temporary, scene, true);
        const EditorBuildInputs inputs = MakeInputs(temporary, BuildPlatform::WindowsX64, BuildConfiguration::Development);

        const EditorBuildReport report = manager.ExecuteActiveBuild(inputs);

        CHECK_FALSE(report.PipelineStarted);
        CHECK(report.Diagnostics.ContainsCode("editor.build.platform.unsupported"));
    }
} // namespace Crowny
