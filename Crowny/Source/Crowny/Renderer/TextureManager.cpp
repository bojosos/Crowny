#include "cwpch.h"

#include "Crowny/Renderer/TextureManager.h"

namespace Crowny
{
    Vector<Ref<Texture>> TextureManager::m_Textures;

    Ref<Texture> TextureManager::Add(const Ref<Texture>& texture)
    {
        m_Textures.push_back(texture);
        return texture;
    }

    Ref<Texture> TextureManager::Get(const String& name)
    {
        for (auto& texture : m_Textures)
        {
            //			if (texture->GetName() == name)
            return texture;
        }

        return nullptr;
    }

    void TextureManager::Clear() { m_Textures.clear(); }

} // namespace Crowny
