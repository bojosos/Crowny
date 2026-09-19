#pragma once

#include "Crowny/Renderer/SpriteAnimationClip.h"

namespace Crowny
{
    class SpriteAnimationSerializer
    {
    public:
        explicit SpriteAnimationSerializer(const Ref<SpriteAnimationClip>& clip) : m_Clip(clip) {}
        bool Serialize(const Path& path) const;
        bool Deserialize(const Path& path);
        String SerializeToString() const;
        bool DeserializeFromString(const String& text);

    private:
        Ref<SpriteAnimationClip> m_Clip;
    };
} // namespace Crowny
