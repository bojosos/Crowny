#pragma once

#include "Crowny/Import/SpecificImporter.h"

namespace Crowny
{
    class PixelData;
    struct ImageLoadResult;

    class TextureImporter : public SpecificImporter
    {
    public:
        virtual ~TextureImporter() = default;

        virtual bool IsExtensionSupported(const String& ext) const override;
        static bool IsExtensionSupportedStatic(const String& ext);
        virtual bool IsMagicNumSupported(uint8_t* num, uint32_t numSize) const override;

        virtual Ref<Asset> Import(const Path& path, Ref<const ImportOptions> importOptions) override;

        static Ref<Texture> ImportFromMemory(const uint8_t* data, size_t size, StringView name,
                                             Ref<const TextureImportOptions> importOptions);
        static Ref<Texture> ImportFromPixels(const Ref<PixelData>& pixels, StringView name,
                                             Ref<const TextureImportOptions> importOptions, bool flipVertically = false);

        virtual Ref<ImportOptions> CreateImportOptions() const override;

        // Import creates a deferred texture from per-file CPU data. Decoding, mip
        // generation, and Basis encoding are independent across files. Asset::Init
        // performs GPU publication later.
        virtual ImporterThreadingPolicy GetThreadingPolicy() const override { return ImporterThreadingPolicy::ParallelWorker; }

    private:
        static Ref<Texture> ImportLoadedImage(ImageLoadResult image, StringView name,
                                              const Ref<const TextureImportOptions>& importOptions);
    };

} // namespace Crowny
