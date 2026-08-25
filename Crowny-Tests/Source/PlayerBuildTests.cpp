#include <catch2/catch_test_macros.hpp>

#include "Crowny/Build/BuildProfile.h"
#include "Crowny/Build/ContentGraph.h"
#include "Crowny/Build/ContentPack.h"
#include "Crowny/Build/PlayerTemplate.h"

#include <fstream>

namespace Crowny
{
    namespace
    {
        class TemporaryDirectory
        {
        public:
            TemporaryDirectory()
            {
                Root = fs::temp_directory_path() / ("crowny-build-tests-" + UuidGenerator::Generate().ToString());
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

        Vector<uint8_t> ReadBytes(const Path& path)
        {
            std::ifstream stream(path, std::ios::binary | std::ios::ate);
            const std::streamsize size = stream.tellg();
            Vector<uint8_t> bytes(static_cast<size_t>(size));
            stream.seekg(0, std::ios::beg);
            stream.read(reinterpret_cast<char*>(bytes.data()), size);
            return bytes;
        }
    } // namespace

    TEST_CASE("Build profiles round-trip editor-owned settings", "[Build]")
    {
        TemporaryDirectory temporary;
        GameSettings game;
        game.ProductName = "Crownfall";
        game.ArtifactName = "Crownfall";
        game.ProductVersion = "1.2.0";
        game.Company = "Crowny Games";
        game.WindowsIcon = UUID("12345678-2222-3333-4444-555555555555");

        BuildProfile profile;
        profile.Id = UUID("11111111-2222-3333-4444-555555555555");
        profile.Name = "Playtest";
        profile.SceneOrder = { UUID("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee") };
        profile.StartupScene = profile.SceneOrder.front();
        profile.ContentRoots.push_back({ ContentRootKind::Asset, {}, UUID("bbbbbbbb-cccc-dddd-eeee-ffffffffffff") });
        profile.Symbols = { "PLAYTEST", "ONLINE" };
        profile.DefaultQuality = QualityTier::High;
        profile.AllowedQuality = { QualityTier::Low, QualityTier::High };
        profile.Targets.push_back({ UUID("99999999-8888-7777-6666-555555555555"), BuildPlatform::WindowsX64, BuildConfiguration::Development });

        REQUIRE(BuildProfileStore::SaveGameSettings(temporary.Root / "Game.yaml", game).empty());
        REQUIRE(BuildProfileStore::SaveProfile(temporary.Root / "Playtest.yaml", profile).empty());

        GameSettings loadedGame;
        BuildProfile loadedProfile;
        REQUIRE(BuildProfileStore::LoadGameSettings(temporary.Root / "Game.yaml", loadedGame).empty());
        REQUIRE(BuildProfileStore::LoadProfile(temporary.Root / "Playtest.yaml", loadedProfile).empty());
        CHECK(loadedGame.ArtifactName == "Crownfall");
        CHECK(loadedGame.WindowsIcon == game.WindowsIcon);
        CHECK(loadedProfile.Id == profile.Id);
        CHECK(loadedProfile.StartupScene == profile.StartupScene);
        CHECK(loadedProfile.Targets.front().Platform == BuildPlatform::WindowsX64);
        CHECK(loadedProfile.Targets.front().Configuration == BuildConfiguration::Development);
        CHECK(loadedProfile.AllowedQuality == profile.AllowedQuality);
    }

    TEST_CASE("Profile validation catches unsafe and ambiguous output names", "[Build]")
    {
        GameSettings game;
        game.ProductName = "Bad name";
        game.ArtifactName = "../Game";

        BuildProfile profile;
        profile.Name = "Default";
        profile.SceneOrder = { UUID("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee") };
        profile.StartupScene = UUID("bbbbbbbb-cccc-dddd-eeee-ffffffffffff");
        profile.Targets.push_back({ UuidGenerator::Generate(), BuildPlatform::WindowsX64, BuildConfiguration::Development });

        const BuildValidation validation = ValidateBuildProfile(game, profile);
        CHECK_FALSE(validation.IsValid());
        CHECK(validation.ContainsCode("game.artifact_name.invalid"));
        CHECK(validation.ContainsCode("profile.startup_scene.not_in_scene_list"));
    }

    TEST_CASE("Content closure retains a root-to-dependency explanation", "[Build]")
    {
        const UUID scene("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee");
        const UUID material("11111111-2222-3333-4444-555555555555");
        const UUID texture("99999999-8888-7777-6666-555555555555");
        ContentDatabase database;
        database.Assets = {
            { scene, "Assets/Main.cwscene", "Internal/Assets/main.asset", { material }, "Scene", "scene-hash" },
            { material, "Assets/Stone.cwmat", "Internal/Assets/material.asset", { texture }, "Material", "material-hash" },
            { texture, "Assets/Stone.png", "Internal/Assets/texture.asset", {}, "Texture", "texture-hash" },
        };

        ContentResolveRequest request;
        request.SceneRoots = { scene };
        const ContentResolveResult result = ResolveContent(database, request);
        REQUIRE(result.Validation.IsValid());
        REQUIRE(result.Assets.size() == 3);
        REQUIRE(result.Assets[2].ReasonChain.size() == 3);
        CHECK(result.Assets[2].ReasonChain[0] == scene);
        CHECK(result.Assets[2].ReasonChain[1] == material);
        CHECK(result.Assets[2].ReasonChain[2] == texture);
    }

    TEST_CASE("Build profiles map stable asset roots into content resolution", "[Build]")
    {
        const UUID scene("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee");
        const UUID explicitAsset("11111111-2222-3333-4444-555555555555");
        const UUID excluded("99999999-8888-7777-6666-555555555555");
        BuildProfile profile;
        profile.SceneOrder = { scene };
        profile.ContentRoots = {
            { ContentRootKind::Asset, {}, explicitAsset },
            { ContentRootKind::Folder, "Assets/UI", UUID::EMPTY },
        };
        profile.ExcludedAssets = { excluded };

        const ContentResolveRequest request = CreateContentResolveRequest(profile);
        CHECK(request.SceneRoots == Vector<UUID>{ scene });
        CHECK(request.AssetRoots == Vector<UUID>{ explicitAsset });
        CHECK(request.FolderRoots == Vector<Path>{ "Assets/UI" });
        CHECK(request.ExcludedAssets == Vector<UUID>{ excluded });
    }

    TEST_CASE("Required content cannot be excluded", "[Build]")
    {
        const UUID scene("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee");
        const UUID texture("99999999-8888-7777-6666-555555555555");
        ContentDatabase database;
        database.Assets = {
            { scene, "Assets/Main.cwscene", "Internal/Assets/main.asset", { texture }, "Scene", "scene-hash" },
            { texture, "Assets/Stone.png", "Internal/Assets/texture.asset", {}, "Texture", "texture-hash" },
        };
        ContentResolveRequest request;
        request.SceneRoots = { scene };
        request.ExcludedAssets = { texture };

        const ContentResolveResult result = ResolveContent(database, request);
        CHECK_FALSE(result.Validation.IsValid());
        CHECK(result.Validation.ContainsCode("content.required_asset_excluded"));
    }

    TEST_CASE("Content databases reject unreferenced corruption", "[Build]")
    {
        const UUID asset("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee");
        const UUID missing("11111111-2222-3333-4444-555555555555");
        ContentDatabase database;
        database.Assets = {
            { asset, "Assets/Folder/../Broken.asset", "Internal/Assets/broken.asset", { missing, missing }, "Mesh", "hash" },
        };

        const BuildValidation validation = ValidateContentDatabase(database);
        CHECK(validation.ContainsCode("content.asset.path.unsafe"));
        CHECK(validation.ContainsCode("content.dependency.duplicate"));
        CHECK(validation.ContainsCode("content.dependency.missing"));
    }

    TEST_CASE("Content packs are byte-identical and support random access", "[Build]")
    {
        TemporaryDirectory temporary;
        WriteText(temporary.Root / "a.asset", "alpha");
        WriteText(temporary.Root / "b.asset", "bravo");

        Vector<ContentPackInput> inputs = {
            { UUID("99999999-8888-7777-6666-555555555555"), "Assets/B.asset", temporary.Root / "b.asset" },
            { UUID("11111111-2222-3333-4444-555555555555"), "Assets/A.asset", temporary.Root / "a.asset" },
        };
        ContentPackDescriptor descriptor;
        descriptor.PackId = "main";
        descriptor.EngineVersion = "0.1.0";
        descriptor.PlayerAbi = 1;
        descriptor.ContentSchema = 1;

        const Path first = temporary.Root / "first.cwpack";
        const Path second = temporary.Root / "second.cwpack";
        REQUIRE(ContentPackWriter::Write(first, descriptor, inputs).empty());
        std::reverse(inputs.begin(), inputs.end());
        REQUIRE(ContentPackWriter::Write(second, descriptor, inputs).empty());
        CHECK(ReadBytes(first) == ReadBytes(second));

        ContentPackReader reader;
        REQUIRE(reader.Open(first).empty());
        CHECK(reader.GetDescriptor().PackId == "main");
        Vector<uint8_t> payload;
        REQUIRE(reader.Read("Assets/B.asset", payload).empty());
        CHECK(String(payload.begin(), payload.end()) == "bravo");
    }

    TEST_CASE("Template validation checks declared hashes and target identity", "[Build]")
    {
        TemporaryDirectory temporary;
        WriteText(temporary.Root / "Crowny-Player.exe", "player");
        WriteText(temporary.Root / "LICENSES" / "Crowny.txt", "license");

        PlayerTemplateManifest manifest;
        manifest.EngineVersion = "0.1.0";
        manifest.PlayerAbi = 1;
        manifest.ContentSchemaMin = 1;
        manifest.ContentSchemaMax = 1;
        manifest.Platform = BuildPlatform::WindowsX64;
        manifest.Configuration = BuildConfiguration::Development;
        manifest.Renderers = { RendererBackend::Vulkan, RendererBackend::OpenGL };
        manifest.Files = {
            { "Crowny-Player.exe", ComputeFileSha256(temporary.Root / "Crowny-Player.exe"), true },
            { "LICENSES/Crowny.txt", ComputeFileSha256(temporary.Root / "LICENSES" / "Crowny.txt"), false },
        };

        PlayerTemplateRequest request;
        request.EngineVersion = "0.1.0";
        request.PlayerAbi = 1;
        request.ContentSchema = 1;
        request.Platform = BuildPlatform::WindowsX64;
        request.Configuration = BuildConfiguration::Development;

        CHECK(ValidatePlayerTemplate(temporary.Root, manifest, request).IsValid());
        WriteText(temporary.Root / "Crowny-Player.exe", "changed");
        const BuildValidation validation = ValidatePlayerTemplate(temporary.Root, manifest, request);
        CHECK_FALSE(validation.IsValid());
        CHECK(validation.ContainsCode("template.file.hash_mismatch"));
    }
} // namespace Crowny
