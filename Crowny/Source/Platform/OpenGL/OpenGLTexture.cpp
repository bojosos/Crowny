#include "cwpch.h"

#ifdef MC_WEB
#include <GLFW/glfw3.h>
#else
#include <glad/glad.h>
#endif

#include "Platform/OpenGL/OpenGLTexture.h"
#include "stb_image.h"

namespace Crowny
{
	static GLenum TextureChannelToOpenGLChannel(TextureChannel channel)
	{
		switch (channel)
		{
		case Crowny::TextureChannel::CHANNEL_RED:                return GL_RED;
		case Crowny::TextureChannel::CHANNEL_RG:                 return GL_RG;
		case Crowny::TextureChannel::CHANNEL_RGB:                return GL_RGB;
		case Crowny::TextureChannel::CHANNEL_RGBA:               return GL_RGBA;
		case Crowny::TextureChannel::CHANNEL_DEPTH_COMPONENT:    return GL_DEPTH_COMPONENT;
#ifndef CW_WEB
		case Crowny::TextureChannel::CHANNEL_BGR:                return GL_BGR;
		case Crowny::TextureChannel::CHANNEL_BGRA:               return GL_BGRA;
		case Crowny::TextureChannel::CHANNEL_STENCIL_INDEX:      return GL_STENCIL_INDEX;
#endif
		}
		CW_ENGINE_ASSERT(false, "Unknown TextureChannel!");
		return GL_NONE;
	}

	static GLenum TextureFormatToOpenGLFormat(TextureFormat format)
	{
		switch (format)
		{
		case Crowny::TextureFormat::RGB:     return GL_RGB;
		case Crowny::TextureFormat::RGBA:    return GL_RGBA;
		}
		CW_ENGINE_ASSERT(false, "Unknown TextureFormat!");
		return GL_NONE;
	}

	static GLenum TextureFormatToOpenGLInternalFormat(TextureFormat format)
	{
		switch (format)
		{
		case Crowny::TextureFormat::RGB:     return GL_RGB8;
		case Crowny::TextureFormat::RGBA:    return GL_RGBA8;
		}
		CW_ENGINE_ASSERT(false, "Unknown TextureFormat!");
		return GL_NONE;
	}

	static GLenum TextureFilterToOpenGLFilter(TextureFilter filter)
	{
		switch (filter)
		{
		case Crowny::TextureFilter::LINEAR:  return GL_LINEAR;
		case Crowny::TextureFilter::NEAREST: return GL_NEAREST;
		}
		CW_ENGINE_ASSERT(false, "Unknown TextureFilter!");
		return GL_NONE;
	}

	static GLenum TextureWrapToOpenGLWrap(TextureWrap wrap)
	{
		switch (wrap)
		{
		case Crowny::TextureWrap::REPEAT:             return GL_REPEAT;
		case Crowny::TextureWrap::MIRRORED_REPEAT:    return GL_MIRRORED_REPEAT;
		case Crowny::TextureWrap::CLAMP_TO_EDGE:      return GL_CLAMP_TO_EDGE;
#ifndef CW_WEB
		case Crowny::TextureWrap::CLAMP_TO_BORDER:    return GL_CLAMP_TO_BORDER;
#endif
		}
		CW_ENGINE_ASSERT(false, "Unknown TextureWrap!");
		return GL_NONE;
	}

#ifndef CW_WEB
	static GLenum TextureSwizzleToOpenGLSwizzle(SwizzleType swizzle)
	{
		switch (swizzle)
		{
		case Crowny::SwizzleType::SWIZZLE_RGBA:  return GL_TEXTURE_SWIZZLE_RGBA;
		case Crowny::SwizzleType::SWIZZLE_R:     return GL_TEXTURE_SWIZZLE_R;
		case Crowny::SwizzleType::SWIZZLE_G:     return GL_TEXTURE_SWIZZLE_G;
		case Crowny::SwizzleType::SWIZZLE_B:     return GL_TEXTURE_SWIZZLE_B;
		case Crowny::SwizzleType::SWIZZLE_A:     return GL_TEXTURE_SWIZZLE_A;
		}
		CW_ENGINE_ASSERT(false, "Unknown TextureSwizzle!");
		return GL_NONE;
	}
#endif

	static GLint TextureSwizzleColorToOpenGLSwizzleColor(SwizzleChannel color)
	{
		switch (color)
		{
		case Crowny::SwizzleChannel::RED:     return GL_RED;
		case Crowny::SwizzleChannel::GREEN:   return GL_GREEN;
		case Crowny::SwizzleChannel::BLUE:    return GL_BLUE;
		case Crowny::SwizzleChannel::ALPHA:   return GL_ALPHA;
		case Crowny::SwizzleChannel::ONE:     return GL_ONE;
		case Crowny::SwizzleChannel::ZERO:    return GL_ZERO;
		}
		CW_ENGINE_ASSERT(false, "Unknown TextureSwizzleColor!");
		return GL_NONE;
	}

	OpenGLTexture2D::OpenGLTexture2D(uint32_t width, uint32_t height, const TextureParameters& parameters) : m_Width(width), m_Height(height), m_Parameters(parameters)
	{
#ifdef MC_WEB
		glGenTextures(1, &m_RendererID);
#else
		glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
#endif

#ifdef MC_WEB
		glBindTexture(GL_TEXTURE_2D, m_RendererID);
		glTexStorage2D(GL_TEXTURE_2D, 1, TextureFormatToOpenGLInternalFormat(m_Parameters.Format), m_Width, m_Height);
#else
		glTextureStorage2D(m_RendererID, 1, TextureFormatToOpenGLInternalFormat(m_Parameters.Format), m_Width, m_Height);
#endif

#ifdef MC_WEB
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, TextureFilterToOpenGLFilter(m_Parameters.Filter));
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, TextureFilterToOpenGLFilter(m_Parameters.Filter));
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, TextureWrapToOpenGLWrap(m_Parameters.Wrap));
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, TextureWrapToOpenGLWrap(m_Parameters.Wrap));
#else
		glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, TextureFilterToOpenGLFilter(m_Parameters.Filter));
		glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, TextureFilterToOpenGLFilter(m_Parameters.Filter));
		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, TextureWrapToOpenGLWrap(m_Parameters.Wrap));
		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, TextureWrapToOpenGLWrap(m_Parameters.Wrap));
		if (m_Parameters.Swizzle.Type != SwizzleType::NONE)
		{
			if (m_Parameters.Swizzle.Type == SwizzleType::SWIZZLE_RGBA)
			{

				GLint tmp[4];
				for (int i = 0; i < 4; i++)
				{
					tmp[i] = TextureSwizzleColorToOpenGLSwizzleColor(m_Parameters.Swizzle.Swizzle[i]);
				}

				glTextureParameteriv(m_RendererID, TextureSwizzleToOpenGLSwizzle(m_Parameters.Swizzle.Type), tmp);
			}
			else
			{
				glTextureParameteri(m_RendererID, TextureSwizzleToOpenGLSwizzle(m_Parameters.Swizzle.Type), TextureSwizzleColorToOpenGLSwizzleColor(m_Parameters.Swizzle.Swizzle[0]));
			}
		}
#endif

	}

	OpenGLTexture2D::OpenGLTexture2D(const std::string& path, const TextureParameters& parameters) : m_FilePath(path), m_Parameters(parameters)
	{
		std::string filepath = DIRECTORY_PREFIX + path;
		int width, height, channels;
		stbi_set_flip_vertically_on_load(1);
		stbi_uc* data = nullptr;
		data = stbi_load(filepath.c_str(), &width, &height, &channels, 0);
		CW_ENGINE_ASSERT(data, "Failed to load texture!");
		m_Width = width;
		m_Height = height;

		if (channels == 4)
		{
			m_Parameters.Format = TextureFormat::RGBA;
		}
		else if (channels == 3)
		{
			m_Parameters.Format = TextureFormat::RGB;
		}
#ifdef MC_WEB
		glGenTextures(1, &m_RendererID);
#else
		glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
#endif

#ifdef MC_WEB
		glBindTexture(GL_TEXTURE_2D, m_RendererID);
		glTexImage2D(GL_TEXTURE_2D, 0, TextureFormatToOpenGLFormat(m_Parameters.Format), width, height, 0, TextureFormatToOpenGLFormat(m_Parameters.Format), GL_UNSIGNED_BYTE, data);
#else
		glTextureStorage2D(m_RendererID, 1, TextureFormatToOpenGLInternalFormat(m_Parameters.Format), m_Width, m_Height);
#endif
#ifdef MC_WEB
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, TextureFilterToOpenGLFilter(m_Parameters.Filter));
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, TextureFilterToOpenGLFilter(m_Parameters.Filter));

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, TextureWrapToOpenGLWrap(m_Parameters.Wrap));
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, TextureWrapToOpenGLWrap(m_Parameters.Wrap));
#else
		glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, TextureFilterToOpenGLFilter(m_Parameters.Filter));
		glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, TextureFilterToOpenGLFilter(m_Parameters.Filter));

		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, TextureWrapToOpenGLWrap(m_Parameters.Wrap));
		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, TextureWrapToOpenGLWrap(m_Parameters.Wrap));
#endif
#ifndef MC_WEB
		glTextureSubImage2D(m_RendererID, 0, 0, 0, m_Width, m_Height, TextureFormatToOpenGLFormat(m_Parameters.Format), GL_UNSIGNED_BYTE, data);
#endif

		CW_ENGINE_INFO("Loaded texture {0}, {1}x{2}x{3}.", m_FilePath, m_Width, m_Height, channels);
		stbi_image_free(data);
	}

	OpenGLTexture2D::~OpenGLTexture2D()
	{
		glDeleteTextures(1, &m_RendererID);
	}

	void OpenGLTexture2D::SetData(void* data, uint32_t size)
	{
		uint32_t bpp = m_Parameters.Format == TextureFormat::RGBA ? 4 : 3;
		CW_ENGINE_ASSERT(size == m_Width * m_Height * bpp, "Data must be an entire texture!");
#ifdef MC_WEB
		glBindTexture(GL_TEXTURE_2D, m_RendererID);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_Width, m_Height, TextureFormatToOpenGLFormat(m_Parameters.Format), GL_UNSIGNED_BYTE, data);
#else
		glTextureSubImage2D(m_RendererID, 0, 0, 0, m_Width, m_Height, TextureFormatToOpenGLFormat(m_Parameters.Format), GL_UNSIGNED_BYTE, data);
#endif
	}

	void OpenGLTexture2D::SetData(void* data, TextureChannel channel)
	{
#ifdef MC_WEB
		glBindTexture(GL_TEXTURE_2D, m_RendererID);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_Width, m_Height, TextureChannelToOpenGLChannel(channel), GL_UNSIGNED_BYTE, data);
#else
		glTextureSubImage2D(m_RendererID, 0, 0, 0, m_Width, m_Height, TextureChannelToOpenGLChannel(channel), GL_UNSIGNED_BYTE, data);
#endif
	}

	void OpenGLTexture2D::Bind(uint32_t slot) const
	{
#ifdef MC_WEB
		glActiveTexture(GL_TEXTURE0 + slot);
		glBindTexture(GL_TEXTURE_2D, m_RendererID);
#else
		glBindTextureUnit(slot, m_RendererID);
#endif
	}

}