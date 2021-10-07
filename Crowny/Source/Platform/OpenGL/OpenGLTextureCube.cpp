/*#include "cwpch.h"

#include "Platform/OpenGL/OpenGLTextureCube.h"
#include "Platform/OpenGL/OpenGLTexture.h"

#include <glad/glad.h>
#include <stb_image.h>

namespace Crowny
{

  OpenGLTextureCube::OpenGLTextureCube(const Path& filepath, const TextureParameters& parameters) :
m_Parameters(parameters)
  {
    m_Files[0] = filepath;
    LoadFromFile();
  }

  OpenGLTextureCube::OpenGLTextureCube(const std::array<String, 6>& files, const TextureParameters& parameters) :
m_Parameters(parameters)
  {
    CW_ENGINE_ASSERT(false, "Not implemented");
  }

  OpenGLTextureCube::OpenGLTextureCube(const std::array<String, 6>& files, uint32_t mips, InputFormat format, const
TextureParameters& parameters) : m_Parameters(parameters)
  {
    CW_ENGINE_ASSERT(false, "Not implemented");
  }

  OpenGLTextureCube::~OpenGLTextureCube()
  {
    glDeleteTextures(1, &m_RendererID);
  }

  void OpenGLTextureCube::LoadFromFile()
  {
    stbi_uc* data = nullptr;
    int32_t width, height, channels;
    stbi_set_flip_vertically_on_load(1);

    data = stbi_load(m_Files[0].c_str(), &width, &height, &channels, 0);

    //Divide the cross into 6 images, for now assumes it is a horizontal image
    uint32_t faceWidth = width / 4;
    uint32_t faceHeight = height / 3;
    std::array<stbi_uc*, 6> faces;

    for (uint32_t cy = 0; cy < 4; cy++)
    {
      for (uint32_t cx = 0; cx < 3; cx++)
      {
        if (cy == 0 || cy == 2 || cy == 3) // horizontal, vertical
                    if (cx != 1)
                        continue;
                for (uint32_t y = 0; y < faceHeight; y++)
                {
                    memcpy(faces[cy * 3 + cx] + y * faceWidth, data + cy * 3 * faceHeight + cx * faceWidth + y *
faceWidth, faceWidth);
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

        uint32_t format = GL_RGBA;
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, format, faceWidth, faceHeight, 0, format, GL_UNSIGNED_BYTE,
faces[3]); glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, format, faceWidth, faceHeight, 0, format, GL_UNSIGNED_BYTE,
faces[1]); glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, format, faceWidth, faceHeight, 0, format, GL_UNSIGNED_BYTE,
faces[0]); glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, format, faceWidth, faceHeight, 0, format, GL_UNSIGNED_BYTE,
faces[5]); glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, format, faceWidth, faceHeight, 0, format, GL_UNSIGNED_BYTE,
faces[2]); glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, format, faceWidth, faceHeight, 0, format, GL_UNSIGNED_BYTE,
faces[4]);

        glGenerateTextureMipmap(m_RendererID);

        stbi_image_free(data);
    }

    void OpenGLTextureCube::Bind(uint32_t slot) const
    {
        glBindTextureUnit(slot, m_RendererID);
    }

    void OpenGLTextureCube::Unbind(uint32_t slot) const
    {
        glBindTextureUnit(slot, 0);
    }
}
*/