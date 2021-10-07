#pragma once

#include "Crowny/RenderAPI/Texture.h"

namespace Crowny
{
    class TextureManager
    {
    public:
        TextureManager() = delete;

    private:
        static Vector<Ref<Texture>> m_Textures;

    public:
        static Ref<Texture> Add(const Ref<Texture>& texture);
        static Ref<Texture> Get(const String& name);
        static void Clear();
    };

} // namespace Crowny