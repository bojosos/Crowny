#pragma once

#include "Crowny/Renderer/RenderTarget.h"

namespace Crowny
{
    
    class RenderTexture : public RenderTarget
    {
    public:
        virtual ~RenderTexture() = default;
        
        virtual const Ref<Texture>& GetColorTexture(uint32_t idx) const = 0;
        virtual const Ref<Texture>& GetDepthStencilTexture() const = 0;
        
    public:
        static Ref<RenderTexture> Create(const RenderTextureProperties& props);
    };
    
}