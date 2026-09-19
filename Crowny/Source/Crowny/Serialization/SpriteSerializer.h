#pragma once

#include "Crowny/Renderer/Sprite.h"

namespace Crowny
{
    class SpriteSerializer
    {
    public:
        explicit SpriteSerializer(const Ref<Sprite>& sprite) : m_Sprite(sprite) {}
        bool Serialize(const Path& path) const;
        bool Deserialize(const Path& path);
        String SerializeToString() const;
        bool DeserializeFromString(const String& text);

    private:
        Ref<Sprite> m_Sprite;
    };
} // namespace Crowny
