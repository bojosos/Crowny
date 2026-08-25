#pragma once

#include "Crowny/Build/BuildTypes.h"

namespace Crowny
{
    constexpr uint32_t GAME_SETTINGS_SCHEMA = 1;
    constexpr uint32_t BUILD_PROFILE_SCHEMA = 1;

    enum class ContentRootKind
    {
        Asset,
        Folder
    };

    struct WindowSettings
    {
        uint32_t Width = 1280;
        uint32_t Height = 720;
        bool Resizable = true;
        bool Fullscreen = false;
    };

    struct GameSettings
    {
        uint32_t Schema = GAME_SETTINGS_SCHEMA;
        String ProductName = "Untitled";
        String ArtifactName = "Untitled";
        String ProductVersion = "0.1.0";
        String Company;
        UUID WindowsIcon = UUID::EMPTY;
        UUID LinuxIcon = UUID::EMPTY;
        WindowSettings Window;
    };

    struct ContentRoot
    {
        ContentRootKind Kind = ContentRootKind::Asset;
        Path PathValue;
        UUID AssetId;
    };

    struct BuildTarget
    {
        UUID Id;
        BuildPlatform Platform = BuildPlatform::WindowsX64;
        BuildConfiguration Configuration = BuildConfiguration::Development;
        Vector<String> Symbols;
        QualityTier DefaultQuality = QualityTier::High;
        RendererPolicy Renderers = RendererPolicy::VulkanThenOpenGL;
        CompatibilityPolicy Compatibility = CompatibilityPolicy::Exact;
        bool Archive = true;
        bool IncludeSymbols = true;
    };

    struct BuildProfile
    {
        uint32_t Schema = BUILD_PROFILE_SCHEMA;
        UUID Id;
        String Name = "Default";
        Vector<UUID> SceneOrder;
        UUID StartupScene;
        Vector<ContentRoot> ContentRoots;
        Vector<UUID> ExcludedAssets;
        Vector<String> Symbols;
        QualityTier DefaultQuality = QualityTier::High;
        Vector<QualityTier> AllowedQuality = { QualityTier::Low, QualityTier::Medium, QualityTier::High, QualityTier::Ultra };
        Vector<BuildTarget> Targets;
    };

    class BuildProfileStore
    {
    public:
        static String SaveGameSettings(const Path& path, const GameSettings& settings);
        static String LoadGameSettings(const Path& path, GameSettings& settings);
        static String SaveProfile(const Path& path, const BuildProfile& profile);
        static String LoadProfile(const Path& path, BuildProfile& profile);

        static BuildProfile CreateDefault(BuildPlatform hostPlatform);
    };

    BuildValidation ValidateBuildProfile(const GameSettings& game, const BuildProfile& profile);
} // namespace Crowny
