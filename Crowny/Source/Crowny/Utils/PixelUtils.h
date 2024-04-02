#pragma once

#include <glm/glm.hpp>

namespace Crowny
{
    enum PixelComponentType
    {
        PCT_BYTE = 0,
        PCT_SHORT = 1,
        PCT_INT = 2,
        PCT_FLOAT16 = 3,
        PCT_FLOAT32 = 4,
        PCT_COUNT
    };

    enum PixelFormatFlags
    {
        PFF_HASALPHA = 0x1,
        PFF_COMPRESSED = 0x2,
        PFF_FLOAT = 0x4,
        PFF_DEPTH = 0x8,
        PFF_INTEGER = 0x10,
        PFF_SIGNED = 0x20,
        PFF_NORMALIZED = 0x40
    };

    // TODO: Copy constructor, copy-assignment operator
    class PixelData
    {
    public:
        PixelData() = default;
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
        uint32_t GetRowSkip() const;
        uint32_t GetSliceSkip() const;

        void AllocateInternalBuffer();
        void SetColorAt(uint32_t x, uint32_t y, const glm::vec4& color);
        void SetColorAt(uint32_t x, uint32_t y, uint32_t z, const glm::vec4& color);
        glm::vec4 GetColorAt(uint32_t x, uint32_t y, uint32_t z = 0) const;

    public:
        static Ref<PixelData> Create(uint32_t width, uint32_t height, TextureFormat format);
        static Ref<PixelData> Create(uint32_t width, uint32_t height, uint32_t depth, TextureFormat format);

    private:
        bool m_OwnsData;
        TextureFormat m_Format = TextureFormat::NONE;
        uint32_t m_Width = 0, m_Height = 0, m_Depth = 0;
        uint32_t m_RowPitch = 0, m_SlicePitch = 0;
        uint8_t* m_Buffer = nullptr;
    };

    class PixelUtils
    {
    public:
        // TODO: Texture compression.
        static uint32_t GetBlockSize(TextureFormat format);
        static glm::ivec2 GetBlockDimensions(TextureFormat format);
        static uint32_t GetNumBytes(TextureFormat format);
        static void GetPitch(uint32_t width, uint32_t height, uint32_t depth, TextureFormat format, uint32_t& rowPitch,
                             uint32_t& depthPitch);
        static void GetMipSizeForLevel(uint32_t width, uint32_t height, uint32_t depth, uint32_t mipLevel,
                                       uint32_t& mipWidth, uint32_t& mipHeight, uint32_t& mipDepth);

        static void ConvertPixels(const PixelData& src, PixelData& dst);
        static void PackPixel(float r, float g, float b, float a, TextureFormat format, uint8_t* dst);
        static void UnpackPixel(float* r, float* g, float* b, float* a, TextureFormat format, uint8_t* src);
        static void GetBitDepths(TextureFormat format, int (&rgba)[4]);
    };

} // namespace Crowny
