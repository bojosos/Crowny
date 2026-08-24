#pragma once

#include "Crowny/RenderAPI/Texture.h"

#include "Platform/OpenGL/OpenGLUtils.h"

namespace Crowny
{
    class OpenGLTexture : public Texture
    {
    public:
        friend class Texture;
        OpenGLTexture();
        ~OpenGLTexture() override;

        PixelData Lock(GpuLockOptions options, uint32_t mipLevel = 0, uint32_t face = 0, uint32_t queueIdx = 0) override;
        void Unlock() override;
        void ReadData(PixelData& dest, uint32_t mipLevel = 0, uint32_t face = 0, uint32_t queueIdx = 0) override;
        bool ReadPixel(uint32_t x, uint32_t y, void* dest, size_t destSize, uint32_t mipLevel = 0, uint32_t face = 0,
                       uint32_t queueIdx = 0) override;
        void WriteData(const PixelData& src, uint32_t mipLevel = 0, uint32_t face = 0, uint32_t queueIdx = 0) override;

        uint32_t GetRendererID() const { return m_RendererID; }
        uint32_t GetTarget() const { return m_Target; }
        const OpenGLTextureFormat& GetOpenGLFormat() const { return m_Format; }
        void Bind(uint32_t slot) const;
        void Unbind(uint32_t slot) const;

    protected:
        explicit OpenGLTexture(const TextureDesc& parameters);
        OpenGLTexture(const TextureDesc& parameters, bool deferred);

    private:
        void Init() override;
        void AllocateStorage();
        GLenum GetTransferTarget(uint32_t face) const;
        void ValidateSurface(uint32_t mipLevel, uint32_t face) const;

        uint32_t m_RendererID = 0;
        uint32_t m_ReadFramebuffer = 0;
        GLenum m_Target = GL_NONE;
        OpenGLTextureFormat m_Format;
        uint8_t* m_MappedData = nullptr;
        uint32_t m_MappedMip = 0;
        uint32_t m_MappedFace = 0;
        GpuLockOptions m_MappedOptions = GpuLockOptions::READ_ONLY;
        bool m_IsMapped = false;
    };
} // namespace Crowny
