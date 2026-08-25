#pragma once

#include "Crowny/Import/SpecificImporter.h"

namespace Crowny
{

    class ScriptImporter : public SpecificImporter
    {
    public:
        virtual ~ScriptImporter() = default;

        virtual bool IsExtensionSupported(const String& ext) const override;
        virtual bool IsMagicNumSupported(uint8_t* num, uint32_t numSize) const override;

        virtual Ref<Asset> Import(const Path& path, Ref<const ImportOptions> importOptions) override;

        virtual Ref<ImportOptions> CreateImportOptions() const override;

        // Script import only reads source text into ScriptCode. Managed assembly and
        // Mono publication happen elsewhere on the main thread.
        virtual ImporterThreadingPolicy GetThreadingPolicy() const override { return ImporterThreadingPolicy::ParallelWorker; }
    };

} // namespace Crowny
