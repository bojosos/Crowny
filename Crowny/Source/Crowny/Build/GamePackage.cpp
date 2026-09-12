#include "cwpch.h"

#include "Crowny/Build/ContentPack.h"
#include "Crowny/Build/GamePackage.h"
#include "Crowny/Common/Version.h"

#include <fstream>

namespace Crowny
{
    GamePackage::~GamePackage()
    {
        if (!m_Cache.empty())
        {
            std::error_code error;
            fs::remove_all(m_Cache, error);
        }
    }

    String GamePackage::Open(const Path& root)
    {
        if (!m_Cache.empty())
            return "This game package is already open.";
        if (String error = BuildManifestStore::Load(root / "BuildManifest.yaml", m_Manifest); !error.empty())
            return error;
        const BuildValidation validation = ValidateBuildManifest(m_Manifest);
        if (!validation.IsValid())
            return validation.GetErrors().front();
        if (m_Manifest.EngineVersion != CROWNY_VERSION_STRING)
            return "The game was built with a different engine version.";
#ifdef CW_PLATFORM_WIN32
        if (m_Manifest.Platform != BuildPlatform::WindowsX64)
#else
        if (m_Manifest.Platform != BuildPlatform::LinuxX64)
#endif
            return "The game package targets another platform.";
        ContentPackReader reader;
        if (String error = reader.Open(root / m_Manifest.Paths.ContentPack); !error.empty())
            return error;
        const ContentPackDescriptor descriptor = reader.GetDescriptor();
        if (descriptor.EngineVersion != m_Manifest.EngineVersion || descriptor.PlayerAbi != m_Manifest.PlayerAbi ||
            descriptor.ContentSchema != m_Manifest.ContentSchema)
            return "The content pack is incompatible with this game manifest.";
        for (const BuildManifestScene& scene : m_Manifest.Scenes)
        {
            const auto entry = reader.Find(scene.Id);
            if (!entry || entry->LogicalPath != scene.LogicalPath)
                return "A configured scene is missing from the content pack.";
        }
        const Path cache = fs::temp_directory_path() / ("crowny-player-" + UuidGenerator::Generate().ToString());
        if (!fs::create_directory(cache))
            return "Could not create the game's content directory.";
        m_Cache = cache;
        Ref<AssetManifest> assets = CreateRef<AssetManifest>("Game");
        for (const ContentPackEntry& entry : reader.GetEntries())
        {
            Vector<uint8_t> bytes;
            if (String error = reader.Read(entry.Id, bytes); !error.empty())
                return error;
            // UUID filenames cannot escape the owned cache, regardless of a logical asset path.
            const Path file = m_Cache / (entry.Id.ToString() + ".asset");
            std::ofstream output(file, std::ios::binary | std::ios::trunc);
            output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            output.close();
            if (!output)
                return "Could not extract game asset: " + entry.LogicalPath.generic_string();
            assets->RegisterAsset(entry.Id, file);
            if (entry.LogicalPath == Path("Settings/Game.yaml"))
                m_SettingsPath = file;
        }
        m_Assets = std::move(assets);
        return {};
    }
} // namespace Crowny
