#pragma once

#include "Crowny/Import/SpecificImporter.h"

namespace Crowny
{
    class NodeGraphImporter : public SpecificImporter
    {
    public:
        bool IsExtensionSupported(const String& ext) const override;
        bool IsMagicNumSupported(uint8_t* num, uint32_t numSize) const override;
        Ref<Asset> Import(const Path& path, Ref<const ImportOptions> importOptions) override;
    };
} // namespace Crowny