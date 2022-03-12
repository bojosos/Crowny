#include "cwpch.h"

#include "Crowny/Import/Importer.h"

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
        for (auto iter = m_Importers.begin(); iter != m_Importers.end(); iter++)
        {
            if (*iter != nullptr && (*iter)->IsExtensionSupported(ext))
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

        SpecificImporter* importer = GetImporterForFile(path);
        if (importer == nullptr)
            return nullptr;
        return importer->CreateImportOptions();
    }

    void Importer::RegisterImporter(SpecificImporter* importer) { m_Importers.push_back(importer); }

    SpecificImporter* Importer::PrepareForImport(const Path& filepath, Ref<const ImportOptions>& importOptions) const
    {
        if (!fs::is_regular_file(filepath))
        {
            CW_ENGINE_WARN("Trying to import an asset that does not exist. {0}", filepath);
            return nullptr;
        }

        SpecificImporter* importer = GetImporterForFile(filepath);
        if (importer == nullptr)
            return nullptr;

        if (importOptions == nullptr)
            importOptions = importer->GetDefaultImportOptions();
        else
        {
            Ref<const ImportOptions> defaultImportOptions = importer->GetDefaultImportOptions();
            CW_ENGINE_ASSERT(importOptions->GetImportOptionsType() == defaultImportOptions->GetImportOptionsType(),
                             "Provided import options are of invalid type");
        }

        return importer;
    }

    Ref<Asset> Importer::Import(const Path& filepath, Ref<const ImportOptions> importOptions)
    {
        SpecificImporter* importer = PrepareForImport(filepath, importOptions);
        if (importer == nullptr)
            return nullptr;

        return importer->Import(filepath, importOptions);
    }

} // namespace Crowny