#include "cwpch.h"

#include "Platform/OpenGL/OpenGLSamplerState.h"
#include "Platform/OpenGL/OpenGLUtils.h"

#include <glad/glad.h>

namespace Crowny
{
    namespace
    {
        constexpr GLenum TEXTURE_MAX_ANISOTROPY = 0x84FE;
        constexpr GLenum MAX_TEXTURE_MAX_ANISOTROPY = 0x84FF;

        bool HasExtension(const char* requested)
        {
            GLint count = 0;
            glGetIntegerv(GL_NUM_EXTENSIONS, &count);
            for (GLint index = 0; index < count; ++index)
            {
                const char* extension = reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, static_cast<GLuint>(index)));
                if (extension != nullptr && std::strcmp(extension, requested) == 0)
                    return true;
            }
            return false;
        }

        GLenum AddressMode(TextureWrap wrap)
        {
            switch (wrap)
            {
            case TextureWrap::REPEAT: return GL_REPEAT;
            case TextureWrap::MIRRORED_REPEAT: return GL_MIRRORED_REPEAT;
            case TextureWrap::CLAMP_TO_BORDER: return GL_CLAMP_TO_BORDER;
            case TextureWrap::NONE:
            case TextureWrap::CLAMP_TO_EDGE: return GL_CLAMP_TO_EDGE;
            }
            return GL_CLAMP_TO_EDGE;
        }

        GLenum MagFilter(TextureFilter filter) { return filter == TextureFilter::NEAREST ? GL_NEAREST : GL_LINEAR; }

        GLenum MinFilter(TextureFilter minFilter, TextureFilter mipFilter)
        {
            if (mipFilter == TextureFilter::NONE)
                return MagFilter(minFilter);
            if (minFilter == TextureFilter::NEAREST)
                return mipFilter == TextureFilter::NEAREST ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST_MIPMAP_LINEAR;
            return mipFilter == TextureFilter::NEAREST ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_LINEAR;
        }
    } // namespace

    OpenGLSamplerState::OpenGLSamplerState(const SamplerStateDesc& desc) : SamplerState(desc)
    {
        glGenSamplers(1, &m_RendererID);
        glSamplerParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, MinFilter(desc.MinFilter, desc.MipFilter));
        glSamplerParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, MagFilter(desc.MagFilter));
        glSamplerParameteri(m_RendererID, GL_TEXTURE_WRAP_S, AddressMode(desc.AddressMode.U));
        glSamplerParameteri(m_RendererID, GL_TEXTURE_WRAP_T, AddressMode(desc.AddressMode.V));
        glSamplerParameteri(m_RendererID, GL_TEXTURE_WRAP_R, AddressMode(desc.AddressMode.W));
        glSamplerParameterf(m_RendererID, GL_TEXTURE_LOD_BIAS, desc.MipmapBias);
        glSamplerParameterf(m_RendererID, GL_TEXTURE_MIN_LOD, desc.MipMin);
        glSamplerParameterf(m_RendererID, GL_TEXTURE_MAX_LOD, desc.MipMax);
        if (desc.CompareFunc == CompareFunction::ALWAYS_PASS)
            glSamplerParameteri(m_RendererID, GL_TEXTURE_COMPARE_MODE, GL_NONE);
        else
        {
            glSamplerParameteri(m_RendererID, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
            glSamplerParameteri(m_RendererID, GL_TEXTURE_COMPARE_FUNC, OpenGLUtils::CompareFunctionToOpenGL(desc.CompareFunc));
        }
        if (desc.MaxAnsio > 1)
        {
            if (HasExtension("GL_EXT_texture_filter_anisotropic") || HasExtension("GL_ARB_texture_filter_anisotropic"))
            {
                GLfloat maximum = 1.0f;
                glGetFloatv(MAX_TEXTURE_MAX_ANISOTROPY, &maximum);
                glSamplerParameterf(m_RendererID, TEXTURE_MAX_ANISOTROPY,
                                    std::clamp(static_cast<float>(desc.MaxAnsio), 1.0f, maximum));
            }
            else
                CW_ENGINE_WARN("OpenGL anisotropic filtering was requested but the driver does not support it");
        }
    }

    OpenGLSamplerState::~OpenGLSamplerState()
    {
        if (m_RendererID != 0)
            glDeleteSamplers(1, &m_RendererID);
    }
} // namespace Crowny
