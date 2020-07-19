#pragma once

#include "Crowny/Renderer/Texture.h"

namespace Crowny
{
	class OpenGLTextureCube : public TextureCube
	{
	private:
		uint32_t m_RendererID;
		uint32_t m_Width;
		uint32_t m_Height;
		uint32_t m_Bits;
		TextureParameters m_Parameters;
		std::array<std::string, 6> m_Files;
	public:
		OpenGLTextureCube(const std::string& filepath, const TextureParameters& parameters);
		OpenGLTextureCube(const std::array<std::string, 6>& files, const TextureParameters& parameters);
		OpenGLTextureCube(const std::array<std::string, 6>& filepath, uint32_t mips, InputFormat format, const TextureParameters& parameters);
		~OpenGLTextureCube();

		void Bind(uint32_t slot = 0) const override;
		void Unbind(uint32_t slot = 0) const override;

	private:
		uint32_t LoadFromFile();
		uint32_t LoadFromFiles();
		uint32_t LoadFromVCross(uint32_t mips);
	};
}