#pragma once

#include "Crowny/Common/RefCounted.h"
#include "Crowny/Common/Types.h"
#include <cmath>
#include <cstddef>
#include <glm/glm.hpp>

namespace Crowny
{

    enum class TextureMipFilter
    {
        Box,
        Triangle,
        Mitchell,
        Lanczos4,
        Kaiser,
        Count
    };

    enum class TextureMipMode
    {
        Color,
        NormalMap,
        Data,
        Count
    };

    struct TextureMipGenerationOptions
    {
        TextureMipFilter Filter = TextureMipFilter::Kaiser;
        TextureMipMode Mode = TextureMipMode::Color;
        uint32_t MaxLevels = 0;
        bool SRGB = false;
        bool Wrap = false;
        bool PremultiplyAlpha = true;
        bool PreserveAlphaCoverage = false;
        float AlphaCutoff = 0.5f;
    };

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

    inline float LinearToSRGB(float x)
    {
        if (x <= 0.0f)
            return 0.0f;
        else if (x >= 1.0f)
            return 1.0f;
        else if (x < 0.0031308f)
            return x * 12.92f;
        else
            return std::pow(x, 1.0f / 2.4f) * 1.055f - 0.055f;
    }

    inline float SRGBToLinear(float x)
    {
        if (x <= 0.0f)
            return 0.0f;
        else if (x >= 1.0f)
            return 1.0f;
        else if (x < 0.04045f)
            return x / 12.92f;
        else
            return std::pow((x + 0.055f) / 1.055f, 2.4f);
    }

    class PixelData;

    class PixelUtils
    {
    public:
        static bool IsValidFormat(TextureFormat format);
        static const char* GetFormatName(TextureFormat format);
        static uint32_t GetComponentCount(TextureFormat format);
        static uint32_t GetBlockSize(TextureFormat format);
        static glm::ivec2 GetBlockDimensions(TextureFormat format);
        static uint32_t GetNumBytes(TextureFormat format);
        static void GetPitch(uint32_t width, uint32_t height, uint32_t depth, TextureFormat format, uint32_t& rowPitch, uint32_t& depthPitch);
        static void GetMipSizeForLevel(uint32_t width, uint32_t height, uint32_t depth, uint32_t mipLevel, uint32_t& mipWidth, uint32_t& mipHeight,
                                       uint32_t& mipDepth);
        static uint32_t GetMaxMipCount(uint32_t width, uint32_t height, uint32_t depth = 1);
        static size_t GetMemorySize(uint32_t width, uint32_t height, uint32_t depth, TextureFormat format);
        static size_t GetMipChainSize(uint32_t width, uint32_t height, uint32_t depth, TextureFormat format, uint32_t mipCount = 0,
                                      uint32_t faceCount = 1);
        static size_t GetMipOffset(uint32_t width, uint32_t height, uint32_t depth, TextureFormat format, uint32_t mipLevel, uint32_t face = 0,
                                   uint32_t mipCount = 0);

        // Generates a complete mip chain through Basis Universal's production resampler. The first entry is a copy of source.
        static bool GenerateMipChain(const PixelData& source, const TextureMipGenerationOptions& options,
                                     Vector<Ref<PixelData>>& output, String* error = nullptr);

        static bool ConvertPixels(const PixelData& src, PixelData& dst);
        static void PackPixel(float r, float g, float b, float a, TextureFormat format, uint8_t* dst);
        static void UnpackPixel(float* r, float* g, float* b, float* a, TextureFormat format, const uint8_t* src);
        static void GetBitDepths(TextureFormat format, int (&rgba)[4]);
        static uint32_t GetMemSize(uint32_t width, uint32_t height, uint32_t depth, TextureFormat format);
        static uint32_t GetFormatFlags(TextureFormat format);
        static bool IsCompressedFormat(TextureFormat format) { return (PixelUtils::GetFormatFlags(format) & PixelFormatFlags::PFF_COMPRESSED) != 0; }
        static bool HasAlpha(TextureFormat format) { return (GetFormatFlags(format) & PixelFormatFlags::PFF_HASALPHA) != 0; }
        static bool IsFloatFormat(TextureFormat format) { return (GetFormatFlags(format) & PixelFormatFlags::PFF_FLOAT) != 0; }
        static bool IsDepthFormat(TextureFormat format) { return (GetFormatFlags(format) & PixelFormatFlags::PFF_DEPTH) != 0; }
        static bool IsIntegerFormat(TextureFormat format) { return (GetFormatFlags(format) & PixelFormatFlags::PFF_INTEGER) != 0; }
        static bool IsNormalizedFormat(TextureFormat format) { return (GetFormatFlags(format) & PixelFormatFlags::PFF_NORMALIZED) != 0; }
    };

    class PixelData : public RefCounted
    {
    public:
        PixelData() = default;
        PixelData(uint32_t width, uint32_t height, uint32_t depth, TextureFormat textureFormat);
        PixelData(const PixelData& other);
        PixelData(PixelData&& other) noexcept;
        PixelData& operator=(const PixelData& other);
        PixelData& operator=(PixelData&& other) noexcept;
        ~PixelData();

        uint32_t GetRowPitch() const { return m_RowPitch; }
        uint32_t GetSlicePitch() const { return m_SlicePitch; }
        uint32_t GetWidth() const { return m_Width; }
        uint32_t GetHeight() const { return m_Height; }
        uint32_t GetDepth() const { return m_Depth; }
        TextureFormat GetFormat() const { return m_Format; }
        size_t GetSize() const;

        uint8_t* GetData() { return m_Buffer; }
        const uint8_t* GetData() const { return m_Buffer; }
        bool OwnsBuffer() const { return m_OwnsData; }
        void SetBuffer(uint8_t* data);
        void SetOwnedBuffer(uint8_t* data);
        uint8_t* ReleaseBuffer();
        void Clear();
        void SetRowPitch(uint32_t rowPitch) { m_RowPitch = rowPitch; }
        void SetSlicePitch(uint32_t slicePitch) { m_SlicePitch = slicePitch; }
        uint32_t GetRowSkip() const;
        uint32_t GetSliceSkip() const;
        uint32_t GetPhysicalRowCount() const;
        bool HasValidPitches() const;
        bool IsValid() const;

        void AllocateInternalBuffer();
        bool TrySetColorAt(uint32_t x, uint32_t y, uint32_t z, const glm::vec4& color);
        bool TryGetColorAt(uint32_t x, uint32_t y, uint32_t z, glm::vec4& color) const;
        void SetColorAt(uint32_t x, uint32_t y, const glm::vec4& color);
        void SetColorAt(uint32_t x, uint32_t y, uint32_t z, const glm::vec4& color);
        glm::vec4 GetColorAt(uint32_t x, uint32_t y, uint32_t z = 0) const;
        size_t GetConsecutiveSize() const { return PixelUtils::GetMemorySize(m_Width, m_Height, m_Depth, m_Format); }
        size_t GetConsequtiveSize() const { return GetConsecutiveSize(); }
        bool IsNice() const;

    public:
        static Ref<PixelData> Create(uint32_t width, uint32_t height, TextureFormat format);
        static Ref<PixelData> Create(uint32_t width, uint32_t height, uint32_t depth, TextureFormat format);
        static Ref<PixelData> CreateView(uint32_t width, uint32_t height, uint32_t depth, TextureFormat format, uint8_t* data, uint32_t rowPitch = 0,
                                         uint32_t slicePitch = 0);

    private:
        void Swap(PixelData& other) noexcept;

        bool m_OwnsData = false;
        TextureFormat m_Format = TextureFormat::NONE;
        uint32_t m_Width = 0, m_Height = 0, m_Depth = 0;
        uint32_t m_RowPitch = 0, m_SlicePitch = 0;
        uint8_t* m_Buffer = nullptr;
    };

} // namespace Crowny
