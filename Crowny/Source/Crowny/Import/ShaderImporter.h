#pragma once

#include "Crowny/Import/SpecificImporter.h"

namespace Crowny
{

    class ShaderImporter : public SpecificImporter
    {
    public:
        virtual ~ShaderImporter() = default;

        virtual bool IsExtensionSupported(const String& ext) const override;
        virtual bool IsMagicNumSupported(uint8_t* num, uint32_t numSize) const override;

        virtual Ref<Asset> Import(const Path& path, Ref<const ImportOptions> importOptions) override;

        virtual Ref<ImportOptions> CreateImportOptions() const override;
    };

} // namespace Crowny