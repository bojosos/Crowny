#include <catch2/catch_test_macros.hpp>

#include "Crowny/Build/ContentPack.h"
#include "Crowny/Build/GamePackage.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/Version.h"
#include "Crowny/Common/Yaml.h"

#include <fstream>

using namespace Crowny;

TEST_CASE("Player content opens outside the project and preserves asset identities", "[Build][Player]")
{
    const Path root = fs::temp_directory_path() / ("crowny-package-test-" + UuidGenerator::Generate().ToString());
    fs::create_directories(root);
    const UUID sceneId(1, 2, 3, 4);
    const UUID assetId(5, 6, 7, 8);
    const Path source = root / "Source.txt";
    std::ofstream(source) << "Scene: Standalone\n";
    BuildManifest manifest;
    manifest.ProductName = manifest.ArtifactName = "Test";
    manifest.ProductVersion = "1.0.0";
    manifest.EngineVersion = CROWNY_VERSION_STRING;
    manifest.MonoVersion = "6.12";
#ifndef CW_PLATFORM_WIN32
    manifest.Platform = BuildPlatform::LinuxX64;
#endif
    manifest.StartupScene = sceneId;
    manifest.Scenes = { { 0, sceneId, "Assets/Start.cwscene" } };
    manifest.Paths.ContentPack = "Content/main.cwpack";
    ContentPackDescriptor descriptor;
    descriptor.EngineVersion = CROWNY_VERSION_STRING;
    REQUIRE(ContentPackWriter::Write(root / manifest.Paths.ContentPack, descriptor,
                                     { { sceneId, "Assets/Start.cwscene", source }, { assetId, "Assets/Model/Subasset", source } })
              .empty());
    REQUIRE(BuildManifestStore::Save(root / "BuildManifest.yaml", manifest).empty());
    fs::remove(source);
    Path extracted;
    {
        GamePackage package;
        REQUIRE(package.Open(root).empty());
        REQUIRE(package.GetAssets()->UuidToFilepath(sceneId, extracted));
        CHECK(FileSystem::ReadTextFile(extracted) == "Scene: Standalone\n");
        CHECK(package.GetAssets()->UuidExists(assetId));
        CHECK_FALSE(extracted.string().starts_with(root.string()));
    }
    CHECK_FALSE(fs::exists(extracted));
    SECTION("Missing startup scene fails before a scene can be launched")
    {
        manifest.StartupScene = assetId;
        manifest.Scenes = { { 0, assetId, "Assets/Missing.cwscene" } };
        REQUIRE(BuildManifestStore::Save(root / "BuildManifest.yaml", manifest).empty());
        GamePackage package;
        CHECK_FALSE(package.Open(root).empty());
        CHECK(package.GetAssets() == nullptr);
    }
    SECTION("Incompatible content fails before extraction")
    {
        manifest.EngineVersion = "999.0.0";
        REQUIRE_FALSE(BuildManifestStore::Save(root / "BuildManifest.yaml", manifest).empty());
        // The writer rejects incompatible versions. Simulate a package produced
        // by a different engine by editing the valid manifest on disk.
        YAML::Node incompatible = YAML::LoadFile((root / "BuildManifest.yaml").string());
        incompatible["EngineVersion"] = manifest.EngineVersion;
        {
            std::ofstream stream(root / "BuildManifest.yaml");
            stream << incompatible;
            stream.flush();
            REQUIRE(stream.good());
        }
        GamePackage package;
        CHECK(package.Open(root).find("manifest.engine_version.incompatible") != String::npos);
        CHECK(package.GetAssets() == nullptr);
    }
    fs::remove_all(root);
}
