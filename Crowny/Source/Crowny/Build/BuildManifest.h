#pragma once

#include "Crowny/Build/BuildTypes.h"

namespace Crowny
{
    inline constexpr uint32_t BUILD_MANIFEST_SCHEMA = 1;
    inline constexpr uint32_t PLAYER_ABI_VERSION = 1;
    inline constexpr uint32_t CONTENT_SCHEMA_VERSION = 1;

    struct BuildManifestScene
    {
        uint32_t Order = 0;
        UUID Id = UUID::EMPTY;
        Path LogicalPath;
    };

    struct BuildManifestPaths
    {
        Path ContentPack;
        Path ManagedAssembly;
        Path MonoRoot;
    };

    struct BuildManifest
    {
        uint32_t Schema = BUILD_MANIFEST_SCHEMA;
        uint32_t PlayerAbi = PLAYER_ABI_VERSION;
        uint32_t ContentSchema = CONTENT_SCHEMA_VERSION;
        String ProductName;
        String ArtifactName;
        String ProductVersion;
        String Company;
        String EngineVersion;
        String MonoVersion;
        BuildPlatform Platform = BuildPlatform::WindowsX64;
        BuildConfiguration Configuration = BuildConfiguration::Development;
        RendererPolicy Renderers = RendererPolicy::VulkanThenOpenGL;
        QualityTier DefaultQuality = QualityTier::High;
        Vector<QualityTier> AllowedQuality = { QualityTier::High };
        UUID StartupScene = UUID::EMPTY;
        Vector<BuildManifestScene> Scenes;
        BuildManifestPaths Paths;
    };

    class BuildManifestStore
    {
    public:
        static String Save(const Path& path, const BuildManifest& manifest);
        static String Load(const Path& path, BuildManifest& manifest);
    };

    BuildValidation ValidateBuildManifest(const BuildManifest& manifest);
} // namespace Crowny
