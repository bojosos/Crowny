#include "cwpch.h"

#include "Platform/OpenGL/OpenGLTextureCube.h"
#include "Platform/OpenGL/OpenGLTexture.h"

#include <glad/glad.h>
#include <stb_image.h>

namespace Crowny
{

	OpenGLTextureCube::OpenGLTextureCube(const std::string& filepath, const TextureParameters& parameters) : m_Parameters(parameters)
	{
		m_Files[0] = filepath;
		m_RendererID = LoadFromFile();
	}

	OpenGLTextureCube::OpenGLTextureCube(const std::array<std::string, 6>& files, const TextureParameters& parameters) : m_Parameters(parameters)
	{

	}

	OpenGLTextureCube::OpenGLTextureCube(const std::array<std::string, 6>& files, uint32_t mips, InputFormat format, const TextureParameters& parameters) : m_Parameters(parameters)
	{

	}

	uint32_t OpenGLTextureCube::LoadFromFile()
	{
		stbi_uc* data = nullptr;
		int32_t width, height, channels;
		stbi_set_flip_vertically_on_load(1);

		data = stbi_load(m_Files[0].c_str(), &width, &height, &channels, 0);
																		 
		//Divide the cross into 6 images, for now assumes it is a horizontal image
		uint32_t faceWidth = width / 4;
		uint32_t faceHeight = height / 3;
		std::array<stbi_uc*, 6> faces;

		for (uint32_t cy = 0; cy < 3; cy++)
		{
			for (uint32_t cx = 0; cx < 4; cx++)
			{
				if (cx == 0 || cx == 2 || cx == 3)
					if (cy != 1)
						continue;
				for (uint32_t y = 0; y < faceHeight; y++)
				{
					memcpy(faces[cx * 4 + cy], data + , faceWidth)
				}
			}
		}

		glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &m_RendererID);
		glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

		glBindTexture(GL_TEXTURE_CUBE_MAP, m_RendererID);
		
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, xp));
		glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, xn));
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, yp));
		glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, yn));
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, zp));
		glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, zn));

		glGenerateTextureMipmap(m_RendererID);

		stbi_image_free(data);
	}

	void OpenGLTextureCube::Bind(uint32_t slot) const
	{

	}

	void OpenGLTextureCube::Unbind(uint32_t slot) const
	{

	}
}