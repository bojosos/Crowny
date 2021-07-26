/*#include "cwpch.h"

#include "Platform/OpenGL/OpenGLTexture.h"
#include "Crowny/Common/VirtualFileSystem.h"

#include <glad/glad.h>
#include <stb_image.h>

namespace Crowny
{
	GLenum OpenGLTexture2D::TextureChannelToOpenGLChannel(TextureChannel channel)
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
			default: 												 CW_ENGINE_ASSERT(false, "Unknown TextureChannel!"); return GL_NONE;
#endif
		}

		return GL_NONE;
	}

	GLenum OpenGLTexture2D::TextureFormatToOpenGLFormat(TextureFormat format)
	{
		switch (format)
		{
			case Crowny::TextureFormat::R8:					return GL_RED;
			case Crowny::TextureFormat::R32I:     			return GL_RED_INTEGER;
			case Crowny::TextureFormat::RG32F:				return GL_RG;
			case Crowny::TextureFormat::RGB8:     			return GL_RGB;
			case Crowny::TextureFormat::RGBA8:				return GL_RGBA;
			case Crowny::TextureFormat::RGBA16F:  			return GL_RGBA;
			case Crowny::TextureFormat::RGBA32F:  			return GL_RGBA;
			case Crowny::TextureFormat::DEPTH32F:     		return GL_DEPTH_COMPONENT;
			case Crowny::TextureFormat::DEPTH24STENCIL8:    return GL_DEPTH_STENCIL;
			default:										CW_ENGINE_ASSERT(false, "Unknown TextureFormat!"); return GL_NONE;
		}

		return GL_NONE;
	}

	GLenum OpenGLTexture2D::TextureFormatToOpenGLInternalFormat(TextureFormat format)
	{
		switch (format)
		{
			case Crowny::TextureFormat::R8:					return GL_R8;
			case Crowny::TextureFormat::R32I:     			return GL_R32I;
			case Crowny::TextureFormat::RG32F:				return GL_RG32F;
			case Crowny::TextureFormat::RGB8:     			return GL_RGB8;
			case Crowny::TextureFormat::RGBA8:				return GL_RGBA8;
			case Crowny::TextureFormat::RGBA16F:  			return GL_RGBA16F;
			case Crowny::TextureFormat::RGBA32F:  			return GL_RGBA32F;
			case Crowny::TextureFormat::DEPTH32F:     		return GL_DEPTH_COMPONENT32F;
			case Crowny::TextureFormat::DEPTH24STENCIL8:    return GL_DEPTH24_STENCIL8;
			default:		    							CW_ENGINE_ASSERT(false, "Unknown TextureFormat!"); return GL_NONE;
		}

		return GL_NONE;
	}

	GLenum OpenGLTexture2D::TextureFormatToOpenGLType(TextureFormat format)
	{
		switch (format)
		{
			case Crowny::TextureFormat::R8:					return GL_FLOAT;
			case Crowny::TextureFormat::R32I:     			return GL_RED_INTEGER;
			case Crowny::TextureFormat::RG32F:				return GL_FLOAT;
			case Crowny::TextureFormat::RGB8:     			return GL_FLOAT;
			case Crowny::TextureFormat::RGBA8:				return GL_FLOAT;
			case Crowny::TextureFormat::RGBA16F:  			return GL_FLOAT;
			case Crowny::TextureFormat::RGBA32F:  			return GL_FLOAT;
			case Crowny::TextureFormat::DEPTH32F:     		return GL_FLOAT;
			case Crowny::TextureFormat::DEPTH24STENCIL8:    return GL_FLOAT;
			default:		    							CW_ENGINE_ASSERT(false, "Unknown TextureFormat!"); return GL_NONE;
		}

		return GL_NONE;
	}

	GLenum OpenGLTexture2D::TextureFilterToOpenGLFilter(TextureFilter filter)
	{
		switch (filter)
		{
			case Crowny::TextureFilter::LINEAR:  return GL_LINEAR;
			case Crowny::TextureFilter::NEAREST: return GL_NEAREST;
			default: 							 CW_ENGINE_ASSERT(false, "Unknown TextureFilter!"); return GL_NONE;
		}
		
		return GL_NONE;
	}

	GLenum OpenGLTexture2D::TextureWrapToOpenGLWrap(TextureWrap wrap)
	{
		switch (wrap)
		{
			case Crowny::TextureWrap::REPEAT:             return GL_REPEAT;
			case Crowny::TextureWrap::MIRRORED_REPEAT:    return GL_MIRRORED_REPEAT;
			case Crowny::TextureWrap::CLAMP_TO_EDGE:      return GL_CLAMP_TO_EDGE;
#ifndef CW_WEB
			case Crowny::TextureWrap::CLAMP_TO_BORDER:    return GL_CLAMP_TO_BORDER;
#endif
			default: 									  CW_ENGINE_ASSERT(false, "Unknown TextureWrap!"); return GL_NONE;
		}

		return GL_NONE;
	}

#ifndef CW_WEB
	GLenum OpenGLTexture2D::TextureSwizzleToOpenGLSwizzle(SwizzleType swizzle)
	{
		switch (swizzle)
		{
			case Crowny::SwizzleType::SWIZZLE_RGBA:  return GL_TEXTURE_SWIZZLE_RGBA;
			case Crowny::SwizzleType::SWIZZLE_R:     return GL_TEXTURE_SWIZZLE_R;
			case Crowny::SwizzleType::SWIZZLE_G:     return GL_TEXTURE_SWIZZLE_G;
			case Crowny::SwizzleType::SWIZZLE_B:     return GL_TEXTURE_SWIZZLE_B;
			case Crowny::SwizzleType::SWIZZLE_A:     return GL_TEXTURE_SWIZZLE_A;
			default: 									  CW_ENGINE_ASSERT(false, "Unknown TextureSwizzle!"); return GL_NONE;
		}

		return GL_NONE;
	}
#endif

	GLint OpenGLTexture2D::TextureSwizzleColorToOpenGLSwizzleColor(SwizzleChannel color)
	{
		switch (color)
		{
			case Crowny::SwizzleChannel::RED:     return GL_RED;
			case Crowny::SwizzleChannel::GREEN:   return GL_GREEN;
			case Crowny::SwizzleChannel::BLUE:    return GL_BLUE;
			case Crowny::SwizzleChannel::ALPHA:   return GL_ALPHA;
			case Crowny::SwizzleChannel::ONE:     return GL_ONE;
			case Crowny::SwizzleChannel::ZERO:    return GL_ZERO;
			default: 							  CW_ENGINE_ASSERT(false, "Unknown TextureSwizzleColor!"); return GL_NONE;
		}

		return GL_NONE;
	}

	OpenGLTexture2D::OpenGLTexture2D(uint32_t width, uint32_t height, const TextureParameters& parameters) : m_Width(width), m_Height(height), m_Parameters(parameters)
	{
#ifdef MC_WEB
		glGenTextures(1, &m_RendererID);
		glBindTexture(GL_TEXTURE_2D, m_RendererID);
		glTexStorage2D(GL_TEXTURE_2D, 1, TextureFormatToOpenGLInternalFormat(m_Parameters.Format), m_Width, m_Height);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, TextureFilterToOpenGLFilter(m_Parameters.Filter));
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, TextureFilterToOpenGLFilter(m_Parameters.Filter));

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, TextureWrapToOpenGLWrap(m_Parameters.Wrap));
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, TextureWrapToOpenGLWrap(m_Parameters.Wrap));

		if (m_Parameters.GenerateMipmaps)
			glGenerateMipmap(GL_TEXTURE_2D);
#else
		glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
		glTextureStorage2D(m_RendererID, 1, TextureFormatToOpenGLInternalFormat(m_Parameters.Format), m_Width, m_Height);
		glBindTexture(GL_TEXTURE_2D, m_RendererID);
		//glTexImage2D(GL_TEXTURE_2D, 0, TextureFormatToOpenGLInternalFormat(m_Parameters.Format), m_Width, m_Height, 0, TextureFormatToOpenGLFormat(m_Parameters.Format), GL_UNSIGNED_BYTE, nullptr);
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

		if (m_Parameters.GenerateMipmaps)
			glGenerateTextureMipmap(m_RendererID);
#endif
	}

	OpenGLTexture2D::OpenGLTexture2D(const std::string& filepath, const TextureParameters& parameters, const std::string& name) 
									: m_FilePath(filepath), m_Parameters(parameters), m_Name(name)
	{
		int width, height, channels;
		stbi_set_flip_vertically_on_load(1);

		auto [loaded, size] = VirtualFileSystem::Get()->ReadFile(filepath);
		auto* data = stbi_load_from_memory(loaded, size, &width, &height, &channels, 0);

		CW_ENGINE_ASSERT(data, "Failed to load texture!");
		m_Width = width;
		m_Height = height;

		if (channels == 4)
		{
			m_Parameters.Format = TextureFormat::RGBA8;
		}
		else if (channels == 3)
		{
			m_Parameters.Format = TextureFormat::RGB8;
		}
		else if (channels == 1)
		{
			m_Parameters.Format = TextureFormat::R8;
		}

#ifdef MC_WEB
		glGenTextures(1, &m_RendererID);
		glBindTexture(GL_TEXTURE_2D, m_RendererID);
		glTexStorage2D(GL_TEXTURE_2D, 1, TextureFormatToOpenGLInternalFormat(m_Parameters.Format), m_Width, m_Height);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, TextureFilterToOpenGLFilter(m_Parameters.Filter));
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, TextureFilterToOpenGLFilter(m_Parameters.Filter));

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, TextureWrapToOpenGLWrap(m_Parameters.Wrap));
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, TextureWrapToOpenGLWrap(m_Parameters.Wrap));
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_Width, m_Height, TextureFormatToOpenGLFormat(m_Parameters.Format), GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);
#else
		glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
		glTextureStorage2D(m_RendererID, 1, TextureFormatToOpenGLInternalFormat(m_Parameters.Format), m_Width, m_Height);
		
		glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, TextureFilterToOpenGLFilter(m_Parameters.Filter));
		glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, TextureFilterToOpenGLFilter(m_Parameters.Filter));

		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, TextureWrapToOpenGLWrap(m_Parameters.Wrap));
		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, TextureWrapToOpenGLWrap(m_Parameters.Wrap));
		glTextureSubImage2D(m_RendererID, 0, 0, 0, m_Width, m_Height, TextureFormatToOpenGLFormat(m_Parameters.Format), GL_UNSIGNED_BYTE, data);

		if (m_Parameters.GenerateMipmaps)
			glGenerateTextureMipmap(m_RendererID);
#endif

		CW_ENGINE_INFO("Loaded texture {0}, {1}x{2}x{3}.", m_FilePath, m_Width, m_Height, channels);
		stbi_image_free(data);
		delete loaded;
	}

	OpenGLTexture2D::~OpenGLTexture2D()
	{
		glDeleteTextures(1, &m_RendererID);
	}

	void OpenGLTexture2D::Clear(int32_t clearColor)
	{
		glClearTexImage(m_RendererID, 0, TextureFormatToOpenGLType(m_Parameters.Format), GL_INT, &clearColor);
	}
	
	void OpenGLTexture2D::SetData(void* data, uint32_t size)
	{
		uint32_t bpp = m_Parameters.Format == TextureFormat::RGBA8 ? 4 : 3; // TODO: Fix this!
		//CW_ENGINE_ASSERT(size == m_Width * m_Height * bpp, "Data must be an entire texture!");
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

	void OpenGLTexture2D::Unbind(uint32_t slot) const
	{
		glBindTextureUnit(slot, 0);
	}

}*/