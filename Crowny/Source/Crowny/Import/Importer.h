#pragma once

#include "Crowny/Assets/Asset.h"
#include "Crowny/Common/HashedString.h"
#include "Crowny/Common/Module.h"
#include "Crowny/Common/Uuid.h"
#include "Crowny/Import/ImportOptions.h"
#include "Crowny/Import/SpecificImporter.h"

#include <initializer_list>

namespace Crowny
{

    class Importer : public Module<Importer>
    {
    public:
        ~Importer();
        Ref<Asset> Import(const Path& filepath, Ref<const ImportOptions> importOptions = nullptr);
        Vector<Ref<Asset>> ImportAll(const Path& filepath, Ref<const ImportOptions> importOptions = nullptr);
        Vector<Ref<Asset>> ImportAllDeferred(const Path& filepath, Ref<const ImportOptions> importOptions = nullptr);

        template <class T> Ref<T> Import(const Path& filepath, Ref<const ImportOptions> importOptions = nullptr)
        {
            return StaticRefCast<T>(Import(filepath, importOptions));
        }

        void RegisterImporter(SpecificImporter* importer);
        void RegisterImporter(SpecificImporter* importer, std::initializer_list<StringView> extensions);
        static void RegisterBuiltinImporters();

        Ref<ImportOptions> CreateImportOptions(const Path& path);

        template <class T> Ref<T> CreateImportOptions(const Path& path) { return StaticRefCast<T>(CreateImportOptions(path)); }

        bool SupportsFileType(const String& ext) const;
        bool SupportsFileType(uint8_t* num, uint32_t numSize) const;

        SpecificImporter* GetImporterForFile(const Path& path) const;

    private:
        SpecificImporter* PrepareForImport(const Path& path, Ref<const ImportOptions>& importOptions) const;
        SpecificImporter* FindImporterForExtension(const String& normalizedExtension) const;
        Vector<Ref<Asset>> ImportAllInternal(const Path& filepath, Ref<const ImportOptions> importOptions, bool initializeAssets);

    private:
        Vector<SpecificImporter*> m_Importers;
        Vector<SpecificImporter*> m_FallbackImporters;
        UnorderedMap<String, SpecificImporter*, StringHash, StringEqual> m_ExtensionImporters;
    };

} // namespace Crowny
