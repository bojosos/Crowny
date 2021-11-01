#pragma once

#include "Crowny/Assets/Asset.h"
#include "Crowny/Common/Module.h"
#include "Crowny/Common/Uuid.h"
#include "Crowny/Import/ImportOptions.h"
#include "Crowny/Import/SpecificImporter.h"

namespace Crowny
{

    class Importer : public Module<Importer>
    {
    public:
        ~Importer();
        Ref<Asset> Import(const Path& filepath, Ref<const ImportOptions> importOptions = nullptr);

        template <class T> Ref<T> Import(const Path& filepath, Ref<const ImportOptions> importOptions = nullptr)
        {
            return std::static_pointer_cast<T>(Import(filepath, importOptions));
        }

        void RegisterImporter(SpecificImporter* importer);

        Ref<ImportOptions> CreateImportOptions(const Path& path);

        template <class T> Ref<T> CreateImportOptions(const Path& path)
        {
            return std::static_pointer_cast<T>(CreateImportOptions(path));
        }

        bool SupportsFileType(const String& ext) const;
        bool SupportsFileType(uint8_t* num, uint32_t numSize) const;

    private:
        SpecificImporter* PrepareForImport(const Path& path, Ref<const ImportOptions>& impotyOptions) const;
        SpecificImporter* GetImporterForFile(const Path& path) const;

    private:
        Vector<SpecificImporter*> m_Importers;
    };

} // namespace Crowny