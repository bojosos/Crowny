#pragma once

#include <glm/glm.hpp>

namespace Crowny
{
    class PixelUtils
    {
    public:
        //TODO: Texture compression.
        static uint32_t GetBlockSize(TextureFormat format);
        static glm::ivec2 GetBlockDimensions(TextureFormat format);
        static uint32_t GetNumBytes(TextureFormat format);
        static void GetPitch(uint32_t width, uint32_t height, uint32_t depth, TextureFormat format, uint32_t& rowPitch, uint32_t& depthPitch);
        static void GetMipSizeForLevel(uint32_t width, uint32_t height, uint32_t depth, uint32_t mipLevel, uint32_t& mipWidth, uint32_t& mipHeight, uint32_t& mipDepth);
    };

    // TODO: Copy constructor, copy-assignment operator
    class PixelData
    {
    public:
        PixelData(uint32_t width, uint32_t height, uint32_t depth, TextureFormat textureFormat);
        ~PixelData();
        
        uint32_t GetRowPitch() const { return m_RowPitch; }
        uint32_t GetSlicePitch() const { return m_SlicePitch; }
        uint32_t GetWidth() const { return m_Width; }
        uint32_t GetHeight() const { return m_Height; }
        uint32_t GetDepth() const { return m_Depth; }
        TextureFormat GetFormat() const { return m_Format; }
        uint32_t GetSize() const { return m_SlicePitch * GetDepth(); }

        uint8_t* GetData() const { return m_Buffer; }
        void SetBuffer(uint8_t* data);
        void SetRowPitch(uint32_t rowPitch) { m_RowPitch = rowPitch; }
        void SetSlicePitch(uint32_t slicePitch) { m_SlicePitch = slicePitch; }
        
    private:
        TextureFormat m_Format = TextureFormat::NONE;
        uint32_t m_Width, m_Height, m_Depth;
        uint32_t m_RowPitch = 0, m_SlicePitch = 0;
        uint8_t* m_Buffer = nullptr;
    };
}