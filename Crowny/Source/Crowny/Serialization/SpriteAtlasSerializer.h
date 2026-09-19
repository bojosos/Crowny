#pragma once

#include "Crowny/Renderer/SpriteAtlas.h"

namespace Crowny
{
    class SpriteAtlasSerializer
    {
    public:
        explicit SpriteAtlasSerializer(const Ref<SpriteAtlas>& atlas) : m_Atlas(atlas) {}
        bool Serialize(const Path& path) const;
        bool Deserialize(const Path& path);
        String SerializeToString() const;
        bool DeserializeFromString(const String& text);

    private:
        Ref<SpriteAtlas> m_Atlas;
    };
} // namespace Crowny
