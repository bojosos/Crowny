#pragma once
#include "Crowny/RenderAPI/GenericGpuBuffer.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Renderer/ComputeMaterial.h"

namespace Crowny
{
    // Every mip is packed separately with its own wrap gutter; the atlas itself has no mip chain.
    class DecalAtlas
    {
    public:
        bool Update(const Vector<Ref<Texture>>& textures);
        const Ref<Texture>& GetTexture() const { return m_Atlas; }
        const Ref<GenericGpuBuffer>& GetRects() const { return m_Rects; }

    private:
        Vector<Ref<Texture>> m_Textures;
        Ref<Texture> m_Atlas;
        Ref<GenericGpuBuffer> m_Rects;
        GraphicsMaterial m_Copy;
    };
} // namespace Crowny
