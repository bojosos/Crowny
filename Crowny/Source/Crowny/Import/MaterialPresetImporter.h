#pragma once

#include "Crowny/Import/SpecificImporter.h"

namespace Crowny
{
    /** Imports `.cwpreset` YAML files as MaterialPreset assets. */
    class MaterialPresetImporter : public SpecificImporter
    {
    public:
        bool IsExtensionSupported(const String& ext) const override;
        bool IsMagicNumSupported(uint8_t* num, uint32_t numSize) const override;
        Ref<Asset> Import(const Path& path, Ref<const ImportOptions> importOptions) override;
        ImporterThreadingPolicy GetThreadingPolicy() const override { return ImporterThreadingPolicy::ParallelWorker; }
    };
} // namespace Crowny
