#include "cwpch.h"

#include "Crowny/Utils/PixelUtils.h"

#include "Crowny/Utils/Bitwise.h"

namespace Crowny
{

    struct PixelFormatDesc
    {
        const char* Name;
        uint8_t ElementBytes;
        uint32_t Flags;
        PixelComponentType ComponentType;
        uint8_t ComponentCount;
        uint8_t Rbits, Gbits, Bbits, Abits;
        uint32_t Rmask, Gmask, Bmask, Amask;
        uint8_t Rshift, Gshift, Bshift, Ashift;
    };

    PixelFormatDesc pixelFormats_[13] = {
        { "None", 0, 0, PCT_BYTE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { "R8", 1, PFF_INTEGER | PFF_NORMALIZED, PCT_BYTE, 1, 8, 0, 0, 0, 0x000000FF, 0, 0, 0, 0, 0, 0, 0 },
        { "RG8", 2, PFF_INTEGER | PFF_NORMALIZED, PCT_BYTE, 2, 8, 8, 0, 0, 0x000000FF, 0x0000FF00, 0, 0, 0, 8, 0, 0 },
        { "RGB8", 4, PFF_INTEGER | PFF_NORMALIZED, PCT_BYTE, 3, 8, 8, 8, 0, 0x000000FF, 0x0000FF00, 0x00FF0000, 0, 0, 8,
          16, 0 },
        { "RGBA8", 4, PFF_INTEGER | PFF_NORMALIZED | PFF_HASALPHA, PCT_BYTE, 4, 8, 8, 8, 8, 0x000000FF, 0x0000FF00,
          0x00FF0000, 0xFF000000, 0, 8, 16, 24 },
        { "RGBA16F", 8, PFF_FLOAT | PFF_HASALPHA, PCT_FLOAT16, 4, 16, 16, 16, 16, 0x0000FFFF, 0xFFFF0000, 0x0000FFFF,
          0xFFFF0000, 0, 16, 0, 16 },
        { "RGB32F", 12, PFF_FLOAT, PCT_FLOAT32, 3, 32, 32, 32, 32, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0, 0, 0, 0, 0 },
        { "RGBA32F", 16, PFF_FLOAT | PFF_HASALPHA, PCT_FLOAT32, 4, 32, 32, 32, 32, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
          0xFFFFFFFF, 0, 16, 0, 16 },
        { "RG16F", 4, PFF_FLOAT, PCT_FLOAT16, 2, 16, 16, 0, 0, 0x0000FFFF, 0xFFFF0000, 0, 0, 0, 16, 0, 0 },
        { "RG32F", 8, PFF_FLOAT, PCT_FLOAT32, 2, 32, 32, 0, 0, 0xFFFFFFFF, 0xFFFFFFFF, 0, 0, 0, 0, 0, 0 },
        { "R32I", 4, PFF_INTEGER, PCT_INT, 1, 32, 0, 0, 0, 0xFFFFFFFF, 0, 0, 0, 0, 0, 0, 0 },
        { "D32", 4, PFF_DEPTH | PFF_FLOAT, PCT_FLOAT32, 1, 32, 0, 0, 0, 0xFFFFFFFF, 0, 0, 0, 0, 0, 0, 0 },
        { "D24S8", 4, PFF_INTEGER | PFF_DEPTH | PFF_NORMALIZED, PCT_INT, 2, 24, 8, 0, 0, 0x00FFFFFF, 0x0FF0000, 0, 0, 0,
          24, 0, 0 }
    };

    static inline const PixelFormatDesc& GetFormatDesc(const TextureFormat fmt)
    {
        const int ord = (int)fmt;
        CW_ENGINE_ASSERT(ord >= 0 && ord < 13);
        return pixelFormats_[ord];
    }

    glm::ivec2 PixelUtils::GetBlockDimensions(TextureFormat format) { return glm::ivec2(1, 1); }

    uint32_t PixelUtils::GetBlockSize(TextureFormat format) { return GetNumBytes(format); }

    void PixelUtils::GetPitch(uint32_t width, uint32_t height, uint32_t depth, TextureFormat format, uint32_t& rowPitch,
                              uint32_t& depthPitch)
    {
        uint32_t blockSize = GetBlockSize(format);
        rowPitch = width * blockSize;
        depthPitch = width * height * blockSize;
    }

    void PixelUtils::GetMipSizeForLevel(uint32_t width, uint32_t height, uint32_t depth, uint32_t mipLevel,
                                        uint32_t& mipWidth, uint32_t& mipHeight, uint32_t& mipDepth)
    {
        mipWidth = width;
        mipHeight = height;
        mipDepth = depth;

        for (uint32_t i = 0; i < mipLevel; i++)
        {
            if (mipWidth != 1)
                mipWidth /= 2;
            if (mipHeight != 1)
                mipHeight /= 2;
            if (mipDepth != 1)
                mipDepth /= 2;
        }
    }

    void PixelUtils::ConvertPixels(const PixelData& src, PixelData& dst)
    {
        if (src.GetWidth() != dst.GetWidth() || src.GetHeight() != dst.GetHeight() || src.GetDepth() != dst.GetDepth())
        {
            CW_ENGINE_ERROR("Cannot convert pixel buffers with different sizes");
            return;
        }

        if (src.GetFormat() == dst.GetFormat())
        {
            std::memcpy(dst.GetData(), src.GetData(), src.GetSize());
            return;
        }

        // TODO: Check compressed formats

        uint32_t srcPixelSize = GetNumBytes(src.GetFormat());
        uint32_t dstPixelSize = GetNumBytes(dst.GetFormat());
        uint8_t* srcptr = static_cast<uint8_t*>(src.GetData());
        uint8_t* dstptr = static_cast<uint8_t*>(dst.GetData());

        uint32_t srcRowSkip = src.GetRowSkip();
        uint32_t srcSliceSkip = src.GetSliceSkip();
        uint32_t dstRowSkip = dst.GetRowSkip();
        uint32_t dstSliceSkip = dst.GetSliceSkip();

        float r, g, b, a;

        for (uint32_t z = 0; z < src.GetDepth(); z++)
        {
            for (uint32_t y = 0; y < src.GetHeight(); y++)
            {
                for (uint32_t x = 0; x < src.GetWidth(); x++)
                {
                    UnpackPixel(&r, &g, &b, &a, src.GetFormat(), srcptr);
                    PackPixel(r, g, b, a, dst.GetFormat(), dstptr);
                    srcptr += srcPixelSize;
                    dstptr += dstPixelSize;
                }
                srcptr += srcRowSkip;
                dstptr += dstRowSkip;
            }
            srcptr += srcSliceSkip;
            dstptr += dstSliceSkip;
        }
    }

    void PixelUtils::UnpackPixel(float* r, float* g, float* b, float* a, TextureFormat format, uint8_t* src)
    {
        float* outputs[] = { r, g, b, a };
        const PixelFormatDesc& desc = GetFormatDesc(format);
        uint8_t bits[] = { desc.Rbits, desc.Gbits, desc.Bbits, desc.Abits };
        uint32_t masks[] = { desc.Rmask, desc.Gmask, desc.Bmask, desc.Amask };
        uint8_t shifts[] = { desc.Rshift, desc.Gshift, desc.Bshift, desc.Ashift };

        uint32_t curBit = 0;
        for (uint32_t i = 0; i < desc.ComponentCount; i++)
        {
            uint32_t cur = curBit / 32;
            uint32_t numBytes = std::min((cur + 1) * 4, (uint32_t)desc.ElementBytes) - (cur * 4);
            uint32_t* curSrc = ((uint32_t*)src) + cur;
            uint32_t value = Bitwise::IntRead(curSrc, numBytes);
            if (desc.Flags & PFF_INTEGER)
            {
                if (desc.Flags & PFF_NORMALIZED)
                {
                    if (desc.Flags & PFF_SIGNED)
                        *outputs[i] = Bitwise::UintToSnorm((value & masks[i]) >> shifts[i], bits[i]);
                    else
                        *outputs[i] = Bitwise::UintToUnorm((value & masks[i]) >> shifts[i], bits[i]);
                }
                else
                    *outputs[i] = (float)((value & masks[i]) >> shifts[i]);
            }
            else if (desc.Flags & PFF_FLOAT)
            {
                if (desc.ComponentType == PCT_FLOAT16)
                    *outputs[i] = Bitwise::HalfToFloat((uint16_t)((value & masks[i]) >> shifts[i]));
                else
                    *outputs[i] = *(float*)(&value);
            }
            else
            {
                CW_ENGINE_ERROR("UnpackPixel format not supported.");
                return;
            }
            curBit += bits[i];
        }

        for (uint32_t i = desc.ComponentCount; i < 3; i++)
            *outputs[i] = 0.0f;

        if (desc.ComponentCount < 4)
            *outputs[3] = 1.0f;
    }

    void PixelUtils::PackPixel(float r, float g, float b, float a, TextureFormat format, uint8_t* dst)
    {
        const PixelFormatDesc desc = GetFormatDesc(format);
        float inputs[] = { r, g, b, a };
        uint8_t bits[] = { desc.Rbits, desc.Gbits, desc.Bbits, desc.Abits };
        uint32_t masks[] = { desc.Rmask, desc.Gmask, desc.Bmask, desc.Amask };
        uint8_t shifts[] = { desc.Rshift, desc.Gshift, desc.Bshift, desc.Ashift };

        std::memset(dst, 0, desc.ElementBytes);
        uint32_t curBit = 0;
        uint32_t prev = 0;
        uint32_t val = 0;

        for (uint32_t i = 0; i < desc.ComponentCount; i++)
        {
            uint32_t cur = curBit / 32;
            if (cur > prev)
            {
                uint32_t* curDst = ((uint32_t*)dst) + prev;
                Bitwise::IntWrite(curDst, 4, val);
                val = 0;
                prev = cur;
            }

            if (desc.Flags & PFF_INTEGER)
            {
                if (desc.Flags & PFF_NORMALIZED)
                {
                    if (desc.Flags & PFF_SIGNED)
                        val |= (Bitwise::SnormToUint(inputs[i], bits[i]) << shifts[i]) & masks[i];
                    else
                        val |= (Bitwise::UnormToUint(inputs[i], bits[i]) << shifts[i]) & masks[i];
                }
                else
                    val |= (((uint32_t)inputs[i]) << shifts[i]) & masks[i];
            }
            else if (desc.Flags & PFF_FLOAT)
            {
                if (desc.ComponentType == PCT_FLOAT16)
                    val |= (Bitwise::FloatToHalf(inputs[i]) << shifts[i]) & masks[i];
                else
                {
                    val |= *(uint32_t*)&inputs[i];
                }
            }
            else
            {
                CW_ENGINE_ERROR("PackPixel format not supported");
                return;
            }
            curBit += bits[i];
        }

        uint32_t numBytes = std::min((prev + 1) * 4, (uint32_t)desc.ElementBytes) - (prev * 4);
        uint32_t* curDst = ((uint32_t*)dst) + prev;
        Bitwise::IntWrite(curDst, numBytes, val);
    }

    uint32_t PixelUtils::GetNumBytes(TextureFormat format)
    {
        switch (format)
        {
        case TextureFormat::R8:
            return 1;
        case TextureFormat::RG8:
            return 2;
        case TextureFormat::RGB8:
            return 3;
        case TextureFormat::RGBA8:
            return 4;
        case TextureFormat::R32I:
            return 4;
        case TextureFormat::RG16F:
            return 4;
        case TextureFormat::RG32F:
            return 8;
        case TextureFormat::RGB32F:
            return 12;
        case TextureFormat::RGBA16F:
            return 8;
        case TextureFormat::RGBA32F:
            return 16;
        case TextureFormat::DEPTH32F:
            return 4;
        case TextureFormat::DEPTH24STENCIL8:
            return 4;
        default:
            return 0;
        }

        return 0;
    }

    void PixelUtils::GetBitDepths(TextureFormat format, int (&rgba)[4])
    {
        const PixelFormatDesc& desc = GetFormatDesc(format);
        rgba[0] = desc.Rbits;
        rgba[1] = desc.Gbits;
        rgba[2] = desc.Bbits;
        rgba[3] = desc.Abits;
    }

    PixelData::PixelData(uint32_t width, uint32_t height, uint32_t depth, TextureFormat textureFormat)
      : m_Format(textureFormat), m_Width(width), m_Height(height), m_Depth(depth), m_Buffer(nullptr), m_OwnsData(false)
    {
        PixelUtils::GetPitch(width, height, depth, textureFormat, m_RowPitch, m_SlicePitch);
    }

    PixelData::~PixelData()
    {
        if (m_OwnsData && m_Buffer)
            delete[] m_Buffer;
    }

    void PixelData::SetColorAt(uint32_t x, uint32_t y, const glm::vec4& color) { SetColorAt(x, y, 0, color); }

    void PixelData::SetColorAt(uint32_t x, uint32_t y, uint32_t z, const glm::vec4& color)
    {
        uint32_t size = PixelUtils::GetNumBytes(m_Format);
        uint32_t offset = z * m_SlicePitch + y * m_RowPitch + x * size;
        CW_ENGINE_INFO("Size: {0}, offset: {1}", size, offset);
        PixelUtils::PackPixel(color.r, color.g, color.b, color.a, m_Format, (uint8_t*)GetData() + offset);
    }

    void PixelData::AllocateInternalBuffer()
    {
        if (m_Buffer != nullptr && m_OwnsData)
            delete[] m_Buffer;
        m_Buffer = nullptr;
        m_Buffer = new uint8_t[GetSize()];
        m_OwnsData = true;
    }

    void PixelData::SetBuffer(uint8_t* data)
    {
        if (m_Buffer && m_OwnsData)
            delete[] m_Buffer;
        m_Buffer = data;
        m_OwnsData = false;
    }

    Ref<PixelData> PixelData::Create(uint32_t width, uint32_t height, TextureFormat format)
    {
        Ref<PixelData> pixelData = CreateRef<PixelData>(width, height, 1, format);
        pixelData->AllocateInternalBuffer();
        return pixelData;
    }

    Ref<PixelData> PixelData::Create(uint32_t width, uint32_t height, uint32_t depth, TextureFormat format)
    {
        Ref<PixelData> pixelData = CreateRef<PixelData>(width, height, depth, format);
        pixelData->AllocateInternalBuffer();
        return pixelData;
    }

} // namespace Crowny
