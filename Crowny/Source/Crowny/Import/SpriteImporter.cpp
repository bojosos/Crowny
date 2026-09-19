#include "cwpch.h"

#include "Crowny/Import/SpriteImporter.h"
#include "Crowny/Serialization/SpriteAnimationSerializer.h"
#include "Crowny/Serialization/SpriteAtlasSerializer.h"
#include "Crowny/Serialization/SpriteSerializer.h"

namespace Crowny
{
    Ref<Asset> SpriteAtlasImporter::Import(const Path& path, Ref<const ImportOptions>)
    {
        const auto atlas = CreateRef<SpriteAtlas>();
        if (SpriteAtlasSerializer(atlas).Deserialize(path))
            return atlas;
        CW_ENGINE_ERROR("Cannot import atlas '{}': {}", path, atlas->GetLastError());
        return nullptr;
    }

    Ref<Asset> SpriteAnimationImporter::Import(const Path& path, Ref<const ImportOptions>)
    {
        const auto clip = CreateRef<SpriteAnimationClip>();
        return SpriteAnimationSerializer(clip).Deserialize(path) ? Ref<Asset>(clip) : nullptr;
    }

    Ref<Asset> SpriteImporter::Import(const Path& path, Ref<const ImportOptions>)
    {
        const auto sprite = CreateRef<Sprite>();
        return SpriteSerializer(sprite).Deserialize(path) ? Ref<Asset>(sprite) : nullptr;
    }
} // namespace Crowny
