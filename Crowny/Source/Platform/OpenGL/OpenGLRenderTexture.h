#pragma once

#include "Crowny/RenderAPI/RenderTexture.h"

namespace Crowny
{
    class OpenGLRenderTexture : public RenderTexture
    {
    public:
        friend class RenderTexture;
        ~OpenGLRenderTexture() override;

        void Resize(uint32_t width, uint32_t height) override;
        const RenderTextureDesc& GetProperties() const override { return m_Desc; }
        void SwapBuffers(uint32_t syncMask = 0xFFFFFFFF) override { (void)syncMask; }

        uint32_t GetFramebuffer() const { return m_Framebuffer; }

    protected:
        explicit OpenGLRenderTexture(const RenderTextureDesc& props);

    private:
        void AttachTexture(uint32_t attachment, const RenderTextureSurface& surface);

        uint32_t m_Framebuffer = 0;
    };
} // namespace Crowny
