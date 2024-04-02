#pragma once

#include "Crowny/Assets/Asset.h"
#include "Crowny/Import/ImportOptions.h"

namespace Crowny
{

    class SpecificImporter
    {
    public:
        SpecificImporter() = default;
        virtual ~SpecificImporter() = default;

        virtual bool IsExtensionSupported(const String& ext) const = 0;

        virtual bool IsMagicNumSupported(uint8_t* num, uint32_t numSize) const = 0;

        virtual Ref<Asset> Import(const Path& path, Ref<const ImportOptions> importOptions) = 0;
        virtual Vector<Ref<Asset>> ImportAll(const Path& path, Ref<const ImportOptions> importOptions);
        virtual Ref<ImportOptions> CreateImportOptions() const;

        Ref<const ImportOptions> GetDefaultImportOptions() const;

    private:
        mutable Ref<const ImportOptions> m_DefaultImportOptions;
    };

} // namespace Crowny