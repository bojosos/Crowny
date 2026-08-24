#include "cwpch.h"

#include "Platform/OpenGL/OpenGLRenderTexture.h"
#include "Platform/OpenGL/OpenGLTexture.h"

#include <glad/glad.h>

#include <stdexcept>

namespace Crowny
{
    OpenGLRenderTexture::OpenGLRenderTexture(const RenderTextureDesc& props) : RenderTexture(props)
    {
        GLint previousFramebuffer = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousFramebuffer);
        glGenFramebuffers(1, &m_Framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, m_Framebuffer);

        Vector<GLenum> drawBuffers;
        for (uint32_t index = 0; index < MAX_FRAMEBUFFER_COLOR_ATTACHMENTS; ++index)
        {
            if (!m_Desc.ColorSurfaces[index].Texture)
                continue;
            const GLenum attachment = GL_COLOR_ATTACHMENT0 + index;
            AttachTexture(attachment, m_Desc.ColorSurfaces[index]);
            if (drawBuffers.size() <= index)
                drawBuffers.resize(index + 1, GL_NONE);
            drawBuffers[index] = attachment;
        }
        if (!drawBuffers.empty())
            glDrawBuffers(static_cast<GLsizei>(drawBuffers.size()), drawBuffers.data());
        else
        {
            glDrawBuffer(GL_NONE);
            glReadBuffer(GL_NONE);
        }

        if (m_Desc.DepthSurface.Texture)
        {
            const TextureFormat format = m_Desc.DepthSurface.Texture->GetFormat();
            const GLenum attachment = format == TextureFormat::DEPTH24STENCIL8 ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT;
            AttachTexture(attachment, m_Desc.DepthSurface);
        }

        const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previousFramebuffer));
        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            glDeleteFramebuffers(1, &m_Framebuffer);
            m_Framebuffer = 0;
            throw std::runtime_error("OpenGL render texture framebuffer is incomplete, status " + std::to_string(status));
        }
    }

    OpenGLRenderTexture::~OpenGLRenderTexture()
    {
        if (m_Framebuffer != 0)
            glDeleteFramebuffers(1, &m_Framebuffer);
    }

    void OpenGLRenderTexture::AttachTexture(uint32_t attachment, const RenderTextureSurface& surface)
    {
        OpenGLTexture* texture = static_cast<OpenGLTexture*>(surface.Texture.get());
        const GLenum target = texture->GetTarget();
        if (target == GL_TEXTURE_3D)
            glFramebufferTextureLayer(GL_FRAMEBUFFER, attachment, texture->GetRendererID(), surface.MipLevel, surface.Face);
        else if (target == GL_TEXTURE_CUBE_MAP)
            glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_CUBE_MAP_POSITIVE_X + surface.Face, texture->GetRendererID(),
                                   surface.MipLevel);
        else
            glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, target, texture->GetRendererID(), surface.MipLevel);
    }

    void OpenGLRenderTexture::Resize(uint32_t width, uint32_t height)
    {
        if (width != m_Desc.Width || height != m_Desc.Height)
            CW_ENGINE_WARN("OpenGL render textures cannot resize attached textures in place; recreate the textures and render target");
    }
} // namespace Crowny
