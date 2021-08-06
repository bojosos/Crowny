/*#pragma once

#include "Crowny/RenderAPI/Framebuffer.h"

namespace Crowny
{
    class OpenGLFramebuffer : public Framebuffer
    {
    public:
        OpenGLFramebuffer(const FramebufferProperties& props);
        ~OpenGLFramebuffer();

        void Invalidate();

        virtual void Bind() const override;
        virtual void Unbind() const override;

        virtual void Resize(uint32_t width, uint32_t height) override;

        virtual uint32_t GetColorAttachmentRendererID() const override { return m_Attachments[0]->GetRendererID(); }
        virtual Ref<Texture2D> GetColorAttachment(uint32_t slot = 0) const override { return m_Attachments[0]; };

        virtual const FramebufferProperties& GetProperties() const override { return m_Properties; }

    private:
        std::vector<Ref<Texture2D>> m_Attachments;
        uint32_t m_RendererID = 0;
        FramebufferProperties m_Properties;

    };

}
*/