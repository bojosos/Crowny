#include "cwpch.h"

#include "Crowny/Import/Importer.h"

#include <tracy/Tracy.hpp>

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
        for (auto* importer : m_Importers)
            delete importer;

        m_Importers.clear();
    }

    bool Importer::SupportsFileType(const String& ext) const
    {
        for (const auto* importer : m_Importers)
        {
            if (importer && importer->IsExtensionSupported(ext))
                return true;
        }

        return false;
    }

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
        String ext = path.extension().string();
        if (ext.empty())
            return nullptr;
        ext = ext.substr(1, ext.size() - 1); // remove .

        if (!SupportsFileType(ext))
        {
            CW_ENGINE_WARN("There is no importer that supports {0} files. Aborting import for {1}.", ext, path);
            return nullptr;
        }

        for (auto iter = m_Importers.begin(); iter != m_Importers.end(); iter++)
        {
            if (*iter != nullptr && (*iter)->IsExtensionSupported(ext))
                return *iter;
        }

        return nullptr;
    }

    Ref<ImportOptions> Importer::CreateImportOptions(const Path& path)
    {
        if (!fs::is_regular_file(path))
        {
            CW_ENGINE_WARN("Trying to import an asset that does not exist. {0}", path);
            return nullptr;
        }

        SpecificImporter* const importer = GetImporterForFile(path);
        if (importer == nullptr)
            return nullptr;
        return importer->CreateImportOptions();
    }

    void Importer::RegisterImporter(SpecificImporter* importer) { m_Importers.push_back(importer); }

    void Importer::RegisterBuiltinImporters()
    {
        Importer::Get().RegisterImporter(new AudioClipImporter());
        Importer::Get().RegisterImporter(new FontImporter());
        Importer::Get().RegisterImporter(new ScriptImporter());
        Importer::Get().RegisterImporter(new ShaderImporter());
        Importer::Get().RegisterImporter(new TextFileImporter());
        Importer::Get().RegisterImporter(new TextureImporter());
        Importer::Get().RegisterImporter(new MaterialImporter());
        Importer::Get().RegisterImporter(new MeshImporter());
        Importer::Get().RegisterImporter(new SceneImporter());
        Importer::Get().RegisterImporter(new PrefabImporter());
        Importer::Get().RegisterImporter(new NodeGraphImporter());
    }

    SpecificImporter* Importer::PrepareForImport(const Path& filepath, Ref<const ImportOptions>& importOptions) const
    {
        if (!fs::is_regular_file(filepath))
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
            CW_ENGINE_ASSERT(importOptions->GetImportOptionsType() == defaultImportOptions->GetImportOptionsType(),
                             "Provided import options are of invalid type");
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

        const Ref<Asset> asset = importer->Import(filepath, importOptions);
        if (asset)
            asset->Init(); // Initialize GPU resources immediately for synchronous callers
        return asset;
    }

    Vector<Ref<Asset>> Importer::ImportAll(const Path& filepath, Ref<const ImportOptions> importOptions)
    {
        SpecificImporter* const importer = PrepareForImport(filepath, importOptions);
        if (importer == nullptr)
            return {};

        Vector<Ref<Asset>> assets = importer->ImportAll(filepath, importOptions);
        for (const auto& asset : assets)
        {
            if (asset)
                asset->Init();
        }
        return assets;
    }

    Ref<Asset> Importer::ImportDeferred(const Path& filepath, Ref<const ImportOptions> importOptions)
    {
        SpecificImporter* const importer = PrepareForImport(filepath, importOptions);
        if (importer == nullptr)
            return nullptr;

        return importer->Import(filepath, importOptions); // No Init() — caller handles GPU init later
    }

} // namespace Crowny