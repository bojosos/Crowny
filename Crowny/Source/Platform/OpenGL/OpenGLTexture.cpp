#include "cwpch.h"

#include "Platform/OpenGL/OpenGLTexture.h"

#include <glad/glad.h>

#include <stdexcept>

namespace Crowny
{
    namespace
    {
        GLenum GetBindingQuery(GLenum target)
        {
            switch (target)
            {
            case GL_TEXTURE_1D: return GL_TEXTURE_BINDING_1D;
            case GL_TEXTURE_2D: return GL_TEXTURE_BINDING_2D;
            case GL_TEXTURE_2D_ARRAY: return GL_TEXTURE_BINDING_2D_ARRAY;
            case GL_TEXTURE_2D_MULTISAMPLE: return GL_TEXTURE_BINDING_2D_MULTISAMPLE;
            case GL_TEXTURE_3D: return GL_TEXTURE_BINDING_3D;
            case GL_TEXTURE_CUBE_MAP: return GL_TEXTURE_BINDING_CUBE_MAP;
            case GL_TEXTURE_CUBE_MAP_ARRAY: return GL_TEXTURE_BINDING_CUBE_MAP_ARRAY;
            default: return GL_NONE;
            }
        }

        class TextureBinding
        {
        public:
            TextureBinding(GLenum target, GLuint texture) : m_Target(target)
            {
                GLint binding = 0;
                glGetIntegerv(GetBindingQuery(target), &binding);
                m_Previous = static_cast<GLuint>(binding);
                glBindTexture(target, texture);
            }
            ~TextureBinding() { glBindTexture(m_Target, m_Previous); }

        private:
            GLenum m_Target;
            GLuint m_Previous = 0;
        };

        class PixelStore
        {
        public:
            PixelStore()
            {
                glGetIntegerv(GL_PACK_ALIGNMENT, &m_PackAlignment);
                glGetIntegerv(GL_UNPACK_ALIGNMENT, &m_UnpackAlignment);
                glGetIntegerv(GL_PACK_ROW_LENGTH, &m_PackRowLength);
                glGetIntegerv(GL_UNPACK_ROW_LENGTH, &m_UnpackRowLength);
                glPixelStorei(GL_PACK_ALIGNMENT, 1);
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                glPixelStorei(GL_PACK_ROW_LENGTH, 0);
                glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            }
            ~PixelStore()
            {
                glPixelStorei(GL_PACK_ALIGNMENT, m_PackAlignment);
                glPixelStorei(GL_UNPACK_ALIGNMENT, m_UnpackAlignment);
                glPixelStorei(GL_PACK_ROW_LENGTH, m_PackRowLength);
                glPixelStorei(GL_UNPACK_ROW_LENGTH, m_UnpackRowLength);
            }

        private:
            GLint m_PackAlignment = 4;
            GLint m_UnpackAlignment = 4;
            GLint m_PackRowLength = 0;
            GLint m_UnpackRowLength = 0;
        };
    } // namespace

    OpenGLTexture::OpenGLTexture() : Texture() {}

    OpenGLTexture::OpenGLTexture(const TextureDesc& parameters) : Texture(parameters) { Init(); }

    OpenGLTexture::OpenGLTexture(const TextureDesc& parameters, bool deferred) : Texture(parameters, deferred) {}

    OpenGLTexture::~OpenGLTexture()
    {
        if (m_ReadFramebuffer != 0)
            glDeleteFramebuffers(1, &m_ReadFramebuffer);
        if (m_RendererID != 0)
            glDeleteTextures(1, &m_RendererID);
    }

    void OpenGLTexture::Init()
    {
        if (m_RendererID != 0)
            return;
        if (!PrepareForInit())
            throw std::runtime_error("Could not prepare texture data for OpenGL");
        if (!PixelUtils::IsValidFormat(m_Desc.Format))
            throw std::invalid_argument("Cannot create an OpenGL texture with an invalid format");
        if (m_Desc.Width == 0 || m_Desc.Height == 0 || m_Desc.Depth == 0 || m_Desc.Faces == 0)
            throw std::invalid_argument("Cannot create an OpenGL texture with a zero-sized dimension");
        if (m_Desc.Shape == TextureShape::TEXTURE_CUBE && m_Desc.Faces % 6u != 0)
            throw std::invalid_argument("OpenGL cube textures require a multiple of six faces");
        if (m_Desc.Samples > 1 &&
            (m_Desc.Shape != TextureShape::TEXTURE_2D || m_Desc.MipLevels != 0 || m_Desc.Faces != 1))
            throw std::invalid_argument("Multisampled OpenGL textures must be single-layer 2D textures without mip levels");

        m_Target = OpenGLUtils::TextureTargetToOpenGL(m_Desc.Shape, m_Desc.Samples);
        if (m_Desc.Samples <= 1 && m_Desc.Shape == TextureShape::TEXTURE_2D && m_Desc.Faces > 1)
            m_Target = GL_TEXTURE_2D_ARRAY;
        else if (m_Desc.Shape == TextureShape::TEXTURE_CUBE && m_Desc.Faces > 6)
            m_Target = GL_TEXTURE_CUBE_MAP_ARRAY;
        m_Format = OpenGLUtils::TextureFormatToOpenGL(m_Desc.Format, m_Desc.sRGB);
        glGenTextures(1, &m_RendererID);
        AllocateStorage();

        UploadPendingSubresources();
    }

    void OpenGLTexture::AllocateStorage()
    {
        TextureBinding binding(m_Target, m_RendererID);
        if (m_Target == GL_TEXTURE_2D_MULTISAMPLE)
        {
            glTexImage2DMultisample(m_Target, static_cast<GLsizei>(m_Desc.Samples), m_Format.InternalFormat,
                                    static_cast<GLsizei>(m_Desc.Width), static_cast<GLsizei>(m_Desc.Height), GL_TRUE);
            return;
        }

        glTexParameteri(m_Target, GL_TEXTURE_MIN_FILTER, m_Desc.MipLevels > 0 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
        glTexParameteri(m_Target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(m_Target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        if (m_Target != GL_TEXTURE_1D)
            glTexParameteri(m_Target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        if (m_Target == GL_TEXTURE_3D || m_Target == GL_TEXTURE_2D_ARRAY || m_Target == GL_TEXTURE_CUBE_MAP ||
            m_Target == GL_TEXTURE_CUBE_MAP_ARRAY)
            glTexParameteri(m_Target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexParameteri(m_Target, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(m_Target, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(m_Desc.MipLevels));

        for (uint32_t mip = 0; mip <= m_Desc.MipLevels; ++mip)
        {
            uint32_t width = 0, height = 0, depth = 0;
            PixelUtils::GetMipSizeForLevel(m_Desc.Width, m_Desc.Height, m_Desc.Depth, mip, width, height, depth);
            const GLsizei dataSize = static_cast<GLsizei>(PixelUtils::GetMemorySize(width, height, depth, m_Desc.Format));
            if (m_Target == GL_TEXTURE_1D)
            {
                if (m_Format.Compressed)
                    glCompressedTexImage1D(m_Target, mip, m_Format.InternalFormat, width, 0, dataSize, nullptr);
                else
                    glTexImage1D(m_Target, mip, m_Format.InternalFormat, width, 0, m_Format.TransferFormat, m_Format.TransferType, nullptr);
            }
            else if (m_Target == GL_TEXTURE_3D || m_Target == GL_TEXTURE_2D_ARRAY || m_Target == GL_TEXTURE_CUBE_MAP_ARRAY)
            {
                const uint32_t storageDepth = m_Target == GL_TEXTURE_3D ? depth : m_Desc.Faces;
                const GLsizei storageSize = static_cast<GLsizei>(
                  PixelUtils::GetMemorySize(width, height, storageDepth, m_Desc.Format));
                if (m_Format.Compressed)
                    glCompressedTexImage3D(m_Target, mip, m_Format.InternalFormat, width, height, storageDepth, 0, storageSize, nullptr);
                else
                    glTexImage3D(m_Target, mip, m_Format.InternalFormat, width, height, storageDepth, 0,
                                 m_Format.TransferFormat, m_Format.TransferType, nullptr);
            }
            else
            {
                const uint32_t faceCount = m_Target == GL_TEXTURE_CUBE_MAP ? 6 : 1;
                const GLsizei faceSize = static_cast<GLsizei>(PixelUtils::GetMemorySize(width, height, 1, m_Desc.Format));
                for (uint32_t face = 0; face < faceCount; ++face)
                {
                    const GLenum target = GetTransferTarget(face);
                    if (m_Format.Compressed)
                        glCompressedTexImage2D(target, mip, m_Format.InternalFormat, width, height, 0, faceSize, nullptr);
                    else
                        glTexImage2D(target, mip, m_Format.InternalFormat, width, height, 0, m_Format.TransferFormat, m_Format.TransferType, nullptr);
                }
            }
        }
    }

    GLenum OpenGLTexture::GetTransferTarget(uint32_t face) const
    {
        return m_Target == GL_TEXTURE_CUBE_MAP ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + face : m_Target;
    }

    void OpenGLTexture::ValidateSurface(uint32_t mipLevel, uint32_t face) const
    {
        CW_ENGINE_ASSERT(mipLevel <= m_Desc.MipLevels, "OpenGL texture mip level is out of range");
        CW_ENGINE_ASSERT(face < m_Desc.Faces, "OpenGL texture face is out of range");
        CW_ENGINE_ASSERT(m_Target != GL_TEXTURE_2D_MULTISAMPLE, "Multisampled OpenGL textures cannot be read or written directly");
    }

    PixelData OpenGLTexture::Lock(GpuLockOptions options, uint32_t mipLevel, uint32_t face, CW_MAYBE_UNUSED uint32_t queueIdx)
    {
        CW_ENGINE_ASSERT(!m_IsMapped, "OpenGL texture is already locked");
        ValidateSurface(mipLevel, face);
        uint32_t width = 0, height = 0, depth = 0;
        PixelUtils::GetMipSizeForLevel(m_Desc.Width, m_Desc.Height, m_Desc.Depth, mipLevel, width, height, depth);
        PixelData data(width, height, depth, m_Desc.Format);
        data.AllocateInternalBuffer();
        if (options == GpuLockOptions::READ_ONLY || options == GpuLockOptions::READ_WRITE || options == GpuLockOptions::WRITE_ONLY_NO_OVERWRITE)
            ReadData(data, mipLevel, face);
        m_MappedData = data.GetData();
        m_MappedMip = mipLevel;
        m_MappedFace = face;
        m_MappedOptions = options;
        m_IsMapped = true;
        return data;
    }

    void OpenGLTexture::Unlock()
    {
        CW_ENGINE_ASSERT(m_IsMapped, "OpenGL texture is not locked");
        if (m_MappedOptions != GpuLockOptions::READ_ONLY)
        {
            const Ref<PixelData> view = AllocatePixelData(m_MappedFace, m_MappedMip);
            view->Clear();
            view->SetBuffer(m_MappedData);
            WriteData(*view, m_MappedMip, m_MappedFace);
        }
        m_MappedData = nullptr;
        m_IsMapped = false;
    }

    void OpenGLTexture::ReadData(PixelData& dest, uint32_t mipLevel, uint32_t face, CW_MAYBE_UNUSED uint32_t queueIdx)
    {
        ValidateSurface(mipLevel, face);
        CW_ENGINE_ASSERT(dest.GetFormat() == m_Desc.Format, "OpenGL texture read format does not match");
        CW_ENGINE_ASSERT(dest.IsValid(), "OpenGL texture read destination is invalid");
        TextureBinding binding(m_Target, m_RendererID);
        PixelStore pixelStore;
        if (m_Target == GL_TEXTURE_2D_ARRAY || m_Target == GL_TEXTURE_CUBE_MAP_ARRAY)
        {
            const size_t surfaceSize = dest.GetSize();
            if (surfaceSize == 0 || surfaceSize > std::numeric_limits<size_t>::max() / m_Desc.Faces)
                return;
            Vector<uint8_t> layers(surfaceSize * m_Desc.Faces);
            if (m_Format.Compressed)
                glGetCompressedTexImage(m_Target, mipLevel, layers.data());
            else
                glGetTexImage(m_Target, mipLevel, m_Format.TransferFormat, m_Format.TransferType, layers.data());
            std::memcpy(dest.GetData(), layers.data() + surfaceSize * face, surfaceSize);
            return;
        }
        if (m_Format.Compressed)
            glGetCompressedTexImage(GetTransferTarget(face), mipLevel, dest.GetData());
        else
            glGetTexImage(GetTransferTarget(face), mipLevel, m_Format.TransferFormat, m_Format.TransferType, dest.GetData());
    }

    bool OpenGLTexture::ReadPixel(uint32_t x, uint32_t y, void* dest, size_t destSize, uint32_t mipLevel, uint32_t face,
                                  CW_MAYBE_UNUSED uint32_t queueIdx)
    {
        if (dest == nullptr || m_Format.Compressed || m_Target == GL_TEXTURE_2D_MULTISAMPLE || mipLevel > m_Desc.MipLevels ||
            face >= m_Desc.Faces)
            return false;

        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t depth = 0;
        PixelUtils::GetMipSizeForLevel(m_Desc.Width, m_Desc.Height, m_Desc.Depth, mipLevel, width, height, depth);
        if (x >= width || y >= height)
            return false;

        const size_t pixelSize = PixelUtils::GetMemorySize(1, 1, 1, m_Desc.Format);
        if (pixelSize == 0 || destSize < pixelSize)
            return false;

        PixelStore pixelStore;
        if (glad_glGetTextureSubImage != nullptr)
        {
            const GLint zOffset = m_Desc.Faces > 1 ? static_cast<GLint>(face) : 0;
            glGetTextureSubImage(m_RendererID, static_cast<GLint>(mipLevel), static_cast<GLint>(x), static_cast<GLint>(y), zOffset, 1, 1, 1,
                                 m_Format.TransferFormat, m_Format.TransferType, static_cast<GLsizei>(destSize), dest);
            return true;
        }

        if (m_Target == GL_TEXTURE_2D || m_Target == GL_TEXTURE_CUBE_MAP || m_Target == GL_TEXTURE_2D_ARRAY ||
            m_Target == GL_TEXTURE_CUBE_MAP_ARRAY)
        {
            GLint previousFramebuffer = 0;
            glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousFramebuffer);
            if (m_ReadFramebuffer == 0)
                glGenFramebuffers(1, &m_ReadFramebuffer);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, m_ReadFramebuffer);
            if (m_Target == GL_TEXTURE_2D_ARRAY || m_Target == GL_TEXTURE_CUBE_MAP_ARRAY)
                glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, m_RendererID,
                                          static_cast<GLint>(mipLevel), static_cast<GLint>(face));
            else
                glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GetTransferTarget(face), m_RendererID,
                                       static_cast<GLint>(mipLevel));
            glReadBuffer(GL_COLOR_ATTACHMENT0);
            const bool complete = glCheckFramebufferStatus(GL_READ_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
            if (complete)
                glReadPixels(static_cast<GLint>(x), static_cast<GLint>(y), 1, 1, m_Format.TransferFormat, m_Format.TransferType, dest);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(previousFramebuffer));
            return complete;
        }

        Ref<PixelData> surface = AllocatePixelData(face, mipLevel);
        ReadData(*surface, mipLevel, face);
        const size_t offset = static_cast<size_t>(y) * surface->GetRowPitch() + static_cast<size_t>(x) * pixelSize;
        if (offset + pixelSize > surface->GetSize())
            return false;
        std::memcpy(dest, surface->GetData() + offset, pixelSize);
        return true;
    }

    void OpenGLTexture::WriteData(const PixelData& src, uint32_t mipLevel, uint32_t face, CW_MAYBE_UNUSED uint32_t queueIdx)
    {
        ValidateSurface(mipLevel, face);
        CW_ENGINE_ASSERT(src.GetFormat() == m_Desc.Format, "OpenGL texture write format does not match");
        CW_ENGINE_ASSERT(src.IsValid(), "OpenGL texture write source is invalid");
        uint32_t width = 0, height = 0, depth = 0;
        PixelUtils::GetMipSizeForLevel(m_Desc.Width, m_Desc.Height, m_Desc.Depth, mipLevel, width, height, depth);
        CW_ENGINE_ASSERT(src.GetWidth() == width && src.GetHeight() == height && src.GetDepth() == depth,
                         "OpenGL texture write dimensions do not match the target mip");

        TextureBinding binding(m_Target, m_RendererID);
        PixelStore pixelStore;
        const GLenum target = GetTransferTarget(face);
        if (m_Target == GL_TEXTURE_1D)
        {
            if (m_Format.Compressed)
                glCompressedTexSubImage1D(target, mipLevel, 0, width, m_Format.InternalFormat, static_cast<GLsizei>(src.GetSize()), src.GetData());
            else
                glTexSubImage1D(target, mipLevel, 0, width, m_Format.TransferFormat, m_Format.TransferType, src.GetData());
        }
        else if (m_Target == GL_TEXTURE_3D)
        {
            if (m_Format.Compressed)
                glCompressedTexSubImage3D(target, mipLevel, 0, 0, 0, width, height, depth, m_Format.InternalFormat,
                                          static_cast<GLsizei>(src.GetSize()), src.GetData());
            else
                glTexSubImage3D(target, mipLevel, 0, 0, 0, width, height, depth, m_Format.TransferFormat, m_Format.TransferType, src.GetData());
        }
        else if (m_Target == GL_TEXTURE_2D_ARRAY || m_Target == GL_TEXTURE_CUBE_MAP_ARRAY)
        {
            if (m_Format.Compressed)
                glCompressedTexSubImage3D(target, mipLevel, 0, 0, face, width, height, 1, m_Format.InternalFormat,
                                          static_cast<GLsizei>(src.GetSize()), src.GetData());
            else
                glTexSubImage3D(target, mipLevel, 0, 0, face, width, height, 1, m_Format.TransferFormat,
                                m_Format.TransferType, src.GetData());
        }
        else if (m_Format.Compressed)
            glCompressedTexSubImage2D(target, mipLevel, 0, 0, width, height, m_Format.InternalFormat, static_cast<GLsizei>(src.GetSize()),
                                      src.GetData());
        else
            glTexSubImage2D(target, mipLevel, 0, 0, width, height, m_Format.TransferFormat, m_Format.TransferType, src.GetData());

        if (m_Desc.GenerateMipmaps && mipLevel == 0 && m_Desc.MipLevels > 0)
            glGenerateMipmap(m_Target);
    }

    void OpenGLTexture::Bind(uint32_t slot) const
    {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(m_Target, m_RendererID);
    }

    void OpenGLTexture::Unbind(uint32_t slot) const
    {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(m_Target, 0);
    }
} // namespace Crowny
