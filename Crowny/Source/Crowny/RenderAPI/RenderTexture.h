#pragma once

#include "Crowny/RenderAPI/RenderTarget.h"

namespace Crowny
{

    enum class RenderSurfaceFormat
    {
        None = 0,
        RGB8 = 1,
        RGBA16F = 2,
        RGBA32F = 3,
        RG32F = 4,
        R32I = 5,
        DEPTH32F = 6,
        DEPTH24STENCIL8 = 7,
        RGBA8 = 8,
        Depth = DEPTH24STENCIL8
    };

    struct RenderTextureSurface
    {
        RenderTextureSurface() = default;

        Ref<Crowny::Texture> Texture;
        uint32_t Face = 0;
        uint32_t NumFaces = 0;
        uint32_t MipLevel = 0;
    };

    struct RenderTextureDesc : public RenderTargetProperties
    {
        RenderTextureSurface ColorSurfaces[MAX_FRAMEBUFFER_COLOR_ATTACHMENTS];
        RenderTextureSurface DepthSurface;
    };

    class RenderTexture : public RenderTarget
    {
    public:
        virtual ~RenderTexture() = default;

        Ref<Texture> GetColorTexture(uint32_t idx) const { return m_Desc.ColorSurfaces[idx].Texture; }
        Ref<Texture> GetDepthStencilTexture() const { return m_Desc.DepthSurface.Texture; }

    protected:
        RenderTexture(const RenderTextureDesc& props);

        Ref<TextureView> m_ColorSurfaces[MAX_FRAMEBUFFER_COLOR_ATTACHMENTS];
        Ref<TextureView> m_DepthStencilSurface;

        RenderTextureDesc m_Desc;

    public:
        static Ref<RenderTexture> Create(const RenderTextureDesc& props);
    };

} // namespace Crowny