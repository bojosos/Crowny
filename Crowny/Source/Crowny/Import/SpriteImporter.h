#pragma once
#include "Crowny/Import/SpecificImporter.h"

namespace Crowny
{
    class SpriteImporter : public SpecificImporter
    {
    public:
        bool IsExtensionSupported(const String& ext) const override { return ext == "cwsprite"; }
        bool IsMagicNumSupported(uint8_t*, uint32_t) const override { return false; }
        Ref<Asset> Import(const Path& path, Ref<const ImportOptions> options) override;
    };

    class SpriteAnimationImporter : public SpecificImporter
    {
    public:
        bool IsExtensionSupported(const String& ext) const override { return ext == "cwspriteanim"; }
        bool IsMagicNumSupported(uint8_t*, uint32_t) const override { return false; }
        Ref<Asset> Import(const Path& path, Ref<const ImportOptions> options) override;
    };
    class SpriteAtlasImporter : public SpecificImporter
    {
    public:
        bool IsExtensionSupported(const String& ext) const override { return ext == "cwatlas"; }
        bool IsMagicNumSupported(uint8_t*, uint32_t) const override { return false; }
        Ref<Asset> Import(const Path& path, Ref<const ImportOptions> options) override;
    };
} // namespace Crowny
