#pragma once

#include "Crowny/Import/Importer.h"

namespace msdfgen
{
    class FreetypeHandle;
}

namespace Crowny
{
    class FontImporter : public SpecificImporter
    {
    public:
        virtual ~FontImporter() = default;

        virtual bool IsExtensionSupported(const String& extension) const override;
        virtual bool IsMagicNumSupported(uint8_t* num, uint32_t numSize) const override;

        virtual Ref<Asset> Import(const Path& path, Ref<const ImportOptions> importOptions) override;
        virtual Ref<ImportOptions> CreateImportOptions() const override;
    };
} // namespace Crowny