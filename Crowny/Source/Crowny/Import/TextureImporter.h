#pragma once

#include "Crowny/Import/SpecificImporter.h"

namespace Crowny
{

    class TextureImporter : public SpecificImporter
    {
    public:
        virtual ~TextureImporter() = default;

        virtual bool IsExtensionSupported(const String& ext) const override;
        static bool IsExtensionSupportedStatic(const String& ext);
        virtual bool IsMagicNumSupported(uint8_t* num, uint32_t numSize) const override;

        virtual Ref<Asset> Import(const Path& path, Ref<const ImportOptions> importOptions) override;

        virtual Ref<ImportOptions> CreateImportOptions() const override;

        // Import creates a deferred texture from per-file CPU data. ImageLoader
        // serializes stb_image's global diagnostic state; mip and Basis work remain
        // independent across files, and Asset::Init performs GPU publication later.
        virtual ImporterThreadingPolicy GetThreadingPolicy() const override { return ImporterThreadingPolicy::ParallelWorker; }
    };

} // namespace Crowny
