#include <catch2/catch_test_macros.hpp>

#include "Crowny/Build/BuildManifest.h"
#include "Crowny/Common/Version.h"

#include <fstream>

namespace Crowny
{
    namespace
    {
        class RuntimeTemporaryDirectory
        {
        public:
            RuntimeTemporaryDirectory()
            {
                Root = fs::temp_directory_path() / ("crowny-runtime-tests-" + UuidGenerator::Generate().ToString());
                fs::create_directories(Root);
            }

            ~RuntimeTemporaryDirectory()
            {
                std::error_code error;
                fs::remove_all(Root, error);
            }

            Path Root;
        };

        BuildManifest CreateValidBuildManifest()
        {
            const UUID sceneId("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee");
            BuildManifest manifest;
            manifest.ProductName = "Crownfall";
            manifest.ArtifactName = "Crownfall";
            manifest.ProductVersion = "1.2.0";
            manifest.Company = "Crowny Games";
            manifest.EngineVersion = CROWNY_VERSION_STRING;
            manifest.MonoVersion = "4.5";
            manifest.Platform = BuildPlatform::WindowsX64;
            manifest.Configuration = BuildConfiguration::Shipping;
            manifest.Renderers = RendererPolicy::VulkanThenOpenGL;
            manifest.DefaultQuality = QualityTier::High;
            manifest.AllowedQuality = { QualityTier::Medium, QualityTier::High };
            manifest.StartupScene = sceneId;
            manifest.Scenes = { { 0, sceneId, "Assets/Scenes/Main.cwscene" } };
            manifest.Paths.ContentPack = "Content/Crownfall.cwpack";
            return manifest;
        }
    } // namespace

    TEST_CASE("Build manifests round-trip the strict player contract", "[Build][Runtime]")
    {
        RuntimeTemporaryDirectory temporary;
        const BuildManifest manifest = CreateValidBuildManifest();
        REQUIRE(ValidateBuildManifest(manifest).IsValid());

        const Path manifestPath = temporary.Root / "BuildManifest.yaml";
        REQUIRE(BuildManifestStore::Save(manifestPath, manifest).empty());

        BuildManifest loaded;
        REQUIRE(BuildManifestStore::Load(manifestPath, loaded).empty());
        CHECK(loaded.Schema == BUILD_MANIFEST_SCHEMA);
        CHECK(loaded.EngineVersion == CROWNY_VERSION_STRING);
        CHECK(loaded.StartupScene == manifest.StartupScene);
        REQUIRE(loaded.Scenes.size() == 1);
        CHECK(loaded.Scenes.front().LogicalPath == Path("Assets/Scenes/Main.cwscene"));
        CHECK(loaded.Paths.ContentPack == Path("Content/Crownfall.cwpack"));
    }

    TEST_CASE("Build manifest validation rejects incompatible versions and unsafe paths", "[Build][Runtime]")
    {
        BuildManifest manifest = CreateValidBuildManifest();
        manifest.Schema = BUILD_MANIFEST_SCHEMA + 1;
        manifest.PlayerAbi = PLAYER_ABI_VERSION + 1;
        manifest.Paths.ContentPack = "../outside.cwpack";
        manifest.Scenes.front().LogicalPath = "../Main.cwscene";

        const BuildValidation validation = ValidateBuildManifest(manifest);
        CHECK_FALSE(validation.IsValid());
        CHECK(validation.ContainsCode("manifest.schema.unsupported"));
        CHECK(validation.ContainsCode("manifest.player_abi.incompatible"));
        CHECK(validation.ContainsCode("manifest.path.unsafe"));
        CHECK(validation.ContainsCode("manifest.scene.path_unsafe"));
    }

    TEST_CASE("Build manifest validation rejects ambiguous scene and quality policy", "[Build][Runtime]")
    {
        BuildManifest manifest = CreateValidBuildManifest();
        manifest.AllowedQuality = { QualityTier::Low, QualityTier::Low };
        manifest.Scenes.push_back({ 1, UUID("11111111-2222-3333-4444-555555555555"), "assets/scenes/main.cwscene" });

        const BuildValidation validation = ValidateBuildManifest(manifest);
        CHECK(validation.ContainsCode("manifest.quality.duplicate"));
        CHECK(validation.ContainsCode("manifest.quality.default_not_allowed"));
        CHECK(validation.ContainsCode("manifest.scene.path_duplicate"));
    }

    TEST_CASE("Build manifest loading is transactional and rejects invalid files", "[Build][Runtime]")
    {
        RuntimeTemporaryDirectory temporary;
        const Path manifestPath = temporary.Root / "BuildManifest.yaml";
        {
            std::ofstream stream(manifestPath, std::ios::binary | std::ios::trunc);
            stream << "Schema: 1\nPlatform: NotAPlatform\n";
        }

        BuildManifest destination = CreateValidBuildManifest();
        destination.ProductName = "Unchanged";
        CHECK_FALSE(BuildManifestStore::Load(manifestPath, destination).empty());
        CHECK(destination.ProductName == "Unchanged");
    }

    TEST_CASE("Build manifest validation rejects traversal before normalization", "[Build][Runtime]")
    {
        BuildManifest manifest = CreateValidBuildManifest();
        manifest.Paths.ContentPack = "Content/../Crownfall.cwpack";
        manifest.Scenes.front().LogicalPath = "Assets/Scenes/../Main.cwscene";

        const BuildValidation validation = ValidateBuildManifest(manifest);
        CHECK(validation.ContainsCode("manifest.path.unsafe"));
        CHECK(validation.ContainsCode("manifest.scene.path_unsafe"));
    }
} // namespace Crowny
