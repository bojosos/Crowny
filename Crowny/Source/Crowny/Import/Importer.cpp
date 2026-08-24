#include "cwpch.h"

#include "Crowny/Import/Importer.h"

#include <tracy/Tracy.hpp>

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Import/AudioClipImporter.h"
#include "Crowny/Import/FontImporter.h"
#include "Crowny/Import/MaterialImporter.h"
#include "Crowny/Import/MeshImporter.h"
#include "Crowny/Import/NodeGraphImporter.h"
#include "Crowny/Import/PrefabImporter.h"
#include "Crowny/Import/SceneImporter.h"
#include "Crowny/Import/ScriptImporter.h"
#include "Crowny/Import/ShaderImporter.h"
#include "Crowny/Import/TextFileImporter.h"
#include "Crowny/Import/TextureImporter.h"

namespace Crowny
{

    Importer::~Importer()
    {
        m_ExtensionImporters.clear();
        m_FallbackImporters.clear();
        for (auto* importer : m_Importers)
            delete importer;

        m_Importers.clear();
    }

    bool Importer::SupportsFileType(const String& ext) const { return FindImporterForExtension(NormalizeImportExtension(ext)) != nullptr; }

    bool Importer::SupportsFileType(uint8_t* magic, uint32_t numSize) const
    {
        for (const auto* importer : m_Importers)
        {
            if (importer && importer->IsMagicNumSupported(magic, numSize))
                return true;
        }

        return false;
    }

    SpecificImporter* Importer::GetImporterForFile(const Path& path) const
    {
        const String ext = NormalizeImportExtension(path.extension().string());
        if (ext.empty())
            return nullptr;

        SpecificImporter* const importer = FindImporterForExtension(ext);
        if (importer == nullptr)
        {
            CW_ENGINE_WARN("There is no importer that supports {0} files. Aborting import for {1}.", ext, path);
            return nullptr;
        }

        return importer;
    }

    Ref<ImportOptions> Importer::CreateImportOptions(const Path& path)
    {
        if (!FileSystem::FileExists(path))
        {
            CW_ENGINE_WARN("Trying to import an asset that does not exist. {0}", path);
            return nullptr;
        }

        SpecificImporter* const importer = GetImporterForFile(path);
        if (importer == nullptr)
            return nullptr;
        return importer->CreateImportOptions();
    }

    void Importer::RegisterImporter(SpecificImporter* importer) { RegisterImporter(importer, {}); }

    void Importer::RegisterImporter(SpecificImporter* importer, std::initializer_list<StringView> extensions)
    {
        if (importer == nullptr)
            return;
        if (std::find(m_Importers.begin(), m_Importers.end(), importer) == m_Importers.end())
            m_Importers.push_back(importer);

        if (extensions.size() == 0)
        {
            if (std::find(m_FallbackImporters.begin(), m_FallbackImporters.end(), importer) == m_FallbackImporters.end())
                m_FallbackImporters.push_back(importer);
            return;
        }

        for (StringView extension : extensions)
        {
            const String normalized = NormalizeImportExtension(extension);
            if (normalized.empty())
                continue;

            const auto [iter, inserted] = m_ExtensionImporters.emplace(normalized, importer);
            if (!inserted && iter->second != importer)
                CW_ENGINE_WARN("Importer extension '{}' is already registered; keeping the first importer.", normalized);
        }
    }

    void Importer::RegisterBuiltinImporters()
    {
        Importer::Get().RegisterImporter(new AudioClipImporter(), { "ogg", "wav" });
        Importer::Get().RegisterImporter(new FontImporter(), { "ttf", "ttc", "otf", "otc", "fnt" });
        Importer::Get().RegisterImporter(new ScriptImporter(), { "cs" });
        Importer::Get().RegisterImporter(new ShaderImporter(), { "cwsl", "glsl" });
        Importer::Get().RegisterImporter(new TextFileImporter(), { "txt", "yaml", "json", "xml" });
        Importer::Get().RegisterImporter(new TextureImporter(),
                                         { "png", "jpeg", "psd", "gif", "tga", "bmp", "hdr", "pic", "ppm", "pgm", "jpg", "ktx2" });
        Importer::Get().RegisterImporter(new MaterialImporter(), { "cwmat", "mat" });
        Importer::Get().RegisterImporter(new MeshImporter());
        Importer::Get().RegisterImporter(new SceneImporter(), { "cwscene" });
        Importer::Get().RegisterImporter(new PrefabImporter(), { "cwprefab" });
        Importer::Get().RegisterImporter(new NodeGraphImporter(), { "cwng" });
    }

    SpecificImporter* Importer::FindImporterForExtension(const String& normalizedExtension) const
    {
        if (normalizedExtension.empty())
            return nullptr;

        const auto indexed = m_ExtensionImporters.find(StringView(normalizedExtension));
        if (indexed != m_ExtensionImporters.end())
            return indexed->second;

        for (SpecificImporter* importer : m_FallbackImporters)
        {
            if (importer != nullptr && importer->IsExtensionSupported(normalizedExtension))
                return importer;
        }
        return nullptr;
    }

    SpecificImporter* Importer::PrepareForImport(const Path& filepath, Ref<const ImportOptions>& importOptions) const
    {
        if (!FileSystem::FileExists(filepath))
        {
            CW_ENGINE_WARN("Trying to import an asset that does not exist. {0}", filepath);
            return nullptr;
        }

        SpecificImporter* const importer = GetImporterForFile(filepath);
        if (importer == nullptr)
            return nullptr;

        if (importOptions == nullptr)
            importOptions = importer->GetDefaultImportOptions();
        else
        {
            const Ref<const ImportOptions> defaultImportOptions = importer->GetDefaultImportOptions();
            if (defaultImportOptions == nullptr || importOptions->GetImportOptionsType() != defaultImportOptions->GetImportOptionsType())
            {
                CW_ENGINE_ERROR("Import options do not match the importer selected for '{}'.", filepath);
                return nullptr;
            }
        }

        if (importOptions == nullptr)
        {
            CW_ENGINE_ERROR("Importer selected for '{}' did not provide import options.", filepath);
            return nullptr;
        }

        return importer;
    }

    Ref<Asset> Importer::Import(const Path& filepath, Ref<const ImportOptions> importOptions)
    {
        ZoneScopedN("Importer::Import");
        CW_ENGINE_INFO("Importing asset: {0}", filepath);
        SpecificImporter* const importer = PrepareForImport(filepath, importOptions);
        if (importer == nullptr)
            return nullptr;

        try
        {
            const Ref<Asset> asset = importer->Import(filepath, importOptions);
            if (asset)
                asset->Init();
            else
                CW_ENGINE_ERROR("Importer returned no asset for '{}'.", filepath);
            return asset;
        }
        catch (const std::exception& error)
        {
            CW_ENGINE_ERROR("Import failed for '{}': {}", filepath, error.what());
            return nullptr;
        }
    }

    Vector<Ref<Asset>> Importer::ImportAll(const Path& filepath, Ref<const ImportOptions> importOptions)
    {
        SpecificImporter* const importer = PrepareForImport(filepath, importOptions);
        if (importer == nullptr)
            return {};

        try
        {
            Vector<Ref<Asset>> assets = importer->ImportAll(filepath, importOptions);
            assets.erase(std::remove(assets.begin(), assets.end(), nullptr), assets.end());
            for (const auto& asset : assets)
                asset->Init();
            if (assets.empty())
                CW_ENGINE_ERROR("Importer returned no assets for '{}'.", filepath);
            return assets;
        }
        catch (const std::exception& error)
        {
            CW_ENGINE_ERROR("Import failed for '{}': {}", filepath, error.what());
            return {};
        }
    }

    Ref<Asset> Importer::ImportDeferred(const Path& filepath, Ref<const ImportOptions> importOptions)
    {
        SpecificImporter* const importer = PrepareForImport(filepath, importOptions);
        if (importer == nullptr)
            return nullptr;

        try
        {
            const Ref<Asset> asset = importer->Import(filepath, importOptions);
            if (!asset)
                CW_ENGINE_ERROR("Deferred importer returned no asset for '{}'.", filepath);
            return asset;
        }
        catch (const std::exception& error)
        {
            CW_ENGINE_ERROR("Deferred import failed for '{}': {}", filepath, error.what());
            return nullptr;
        }
    }

} // namespace Crowny
