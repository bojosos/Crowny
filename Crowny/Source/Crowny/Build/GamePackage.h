#pragma once

#include "Crowny/Assets/AssetManifest.h"
#include "Crowny/Build/BuildManifest.h"

namespace Crowny
{
    // Materializes verified pack payloads for the existing file-based asset codecs.
    // Owns a unique temporary directory to avoid modifying packaged content.
    class GamePackage
    {
    public:
        GamePackage() = default;
        GamePackage(const GamePackage&) = delete;
        GamePackage& operator=(const GamePackage&) = delete;
        ~GamePackage();
        String Open(const Path& root);
        const BuildManifest& GetManifest() const { return m_Manifest; }
        const Ref<AssetManifest>& GetAssets() const { return m_Assets; }
        const Path& GetSettingsPath() const { return m_SettingsPath; }

    private:
        BuildManifest m_Manifest;
        Ref<AssetManifest> m_Assets;
        Path m_Cache;
        Path m_SettingsPath;
    };
} // namespace Crowny
