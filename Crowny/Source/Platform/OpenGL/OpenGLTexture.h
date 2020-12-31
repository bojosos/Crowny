#pragma once

#include "Crowny/Renderer/Texture.h"

namespace Crowny
{

	class OpenGLTexture2D : public Texture2D
	{
	public:
		OpenGLTexture2D(uint32_t width, uint32_t height, const TextureParameters& parameters);
		OpenGLTexture2D(const std::string& filepath, const TextureParameters& parameters, const std::string& name);
		~OpenGLTexture2D();

		virtual uint32_t GetWidth() const override { return m_Width; }
		virtual uint32_t GetHeight() const override { return m_Height; }

		virtual const std::string& GetName() const override { return m_Name; }
		virtual const std::string& GetFilepath() const override { return m_FilePath; }

		virtual uint32_t GetRendererID() const override { return m_RendererID; };

		virtual void Bind(uint32_t slot) const override;
		virtual void Unbind(uint32_t slot) const override;
		virtual void Clear() override;
		virtual void SetData(void* data, uint32_t size) override;
		virtual void SetData(void* data, TextureChannel channel = TextureChannel::CHANNEL_RGBA) override;

		virtual bool operator==(const Texture& other) const override
		{
			return (other.GetRendererID() == m_RendererID);
		}

	private:
		TextureParameters m_Parameters;
		uint32_t m_RendererID;
		std::string m_FilePath;
		uint32_t m_Width, m_Height;
		std::string m_Name;

	public:
		static uint32_t TextureChannelToOpenGLChannel(TextureChannel channel);
		static uint32_t TextureFormatToOpenGLFormat(TextureFormat format);
		static uint32_t TextureFormatToOpenGLInternalFormat(TextureFormat format);
		static uint32_t TextureFilterToOpenGLFilter(TextureFilter filter);
		static uint32_t TextureFormatToOpenGLType(TextureFormat format);
		static uint32_t TextureWrapToOpenGLWrap(TextureWrap wrap);
		static uint32_t TextureSwizzleToOpenGLSwizzle(SwizzleType swizzle);
		static int32_t  TextureSwizzleColorToOpenGLSwizzleColor(SwizzleChannel color);
	};


}