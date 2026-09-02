#include "cwpch.h"

#include "Crowny/Utils/PixelUtils.h"

#include "Crowny/Utils/Bitwise.h"

#include "basis_universal/encoder/basisu_enc.h"

#include <bit>
#include <cstdint>
#include <limits>

namespace Crowny
{
    namespace
    {
        const char* GetMipFilterName(TextureMipFilter filter)
        {
            switch (filter)
            {
            case TextureMipFilter::Box:
                return "box";
            case TextureMipFilter::Triangle:
                return "tent";
            case TextureMipFilter::Mitchell:
                return "mitchell";
            case TextureMipFilter::Lanczos4:
                return "lanczos4";
            case TextureMipFilter::Kaiser:
                return "kaiser";
            case TextureMipFilter::Count:
                break;
            }
            return nullptr;
        }

        void SetMipError(String* error, const char* message)
        {
            if (error != nullptr)
                *error = message;
        }

        float AlphaCoverage(const basisu::imagef& image, float cutoff, float scale = 1.0f)
        {
            const uint64_t pixelCount = static_cast<uint64_t>(image.get_width()) * image.get_height();
            if (pixelCount == 0)
                return 0.0f;

            uint64_t covered = 0;
            for (uint32_t y = 0; y < image.get_height(); ++y)
            {
                for (uint32_t x = 0; x < image.get_width(); ++x)
                    covered += glm::clamp(image(x, y)[3] * scale, 0.0f, 1.0f) >= cutoff ? 1u : 0u;
            }
            return static_cast<float>(covered) / static_cast<float>(pixelCount);
        }

        void PreserveAlphaCoverage(basisu::imagef& image, float targetCoverage, float cutoff, bool premultiplied)
        {
            float low = 0.0f;
            float high = 8.0f;
            for (uint32_t iteration = 0; iteration < 16; ++iteration)
            {
                const float scale = (low + high) * 0.5f;
                if (AlphaCoverage(image, cutoff, scale) < targetCoverage)
                    low = scale;
                else
                    high = scale;
            }

            for (uint32_t y = 0; y < image.get_height(); ++y)
            {
                for (uint32_t x = 0; x < image.get_width(); ++x)
                {
                    const float oldAlpha = image(x, y)[3];
                    const float newAlpha = glm::clamp(oldAlpha * high, 0.0f, 1.0f);
                    if (premultiplied)
                    {
                        if (oldAlpha > 1e-8f)
                        {
                            const float colorScale = newAlpha / oldAlpha;
                            image(x, y)[0] *= colorScale;
                            image(x, y)[1] *= colorScale;
                            image(x, y)[2] *= colorScale;
                        }
                        else
                            image(x, y)[0] = image(x, y)[1] = image(x, y)[2] = 0.0f;
                    }
                    image(x, y)[3] = newAlpha;
                }
            }
        }
    } // namespace

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

    constexpr std::array<PixelFormatDesc, static_cast<size_t>(TextureFormat::FormatCount)> PIXEL_FORMATS = { {
      { "None", 0, 0, PCT_BYTE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { "R8", 1, PFF_INTEGER | PFF_NORMALIZED, PCT_BYTE, 1, 8, 0, 0, 0, 0x000000FF, 0, 0, 0, 0, 0, 0, 0 },
      { "RG8", 2, PFF_INTEGER | PFF_NORMALIZED, PCT_BYTE, 2, 8, 8, 0, 0, 0x000000FF, 0x0000FF00, 0, 0, 0, 8, 0, 0 },
      { "RGB8", 3, PFF_INTEGER | PFF_NORMALIZED, PCT_BYTE, 3, 8, 8, 8, 0, 0x000000FF, 0x0000FF00, 0x00FF0000, 0, 0, 8, 16, 0 },
      { "RGBA8", 4, PFF_INTEGER | PFF_NORMALIZED | PFF_HASALPHA, PCT_BYTE, 4, 8, 8, 8, 8, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000, 0, 8, 16,
        24 },
      { "RGBA16F", 8, PFF_FLOAT | PFF_HASALPHA, PCT_FLOAT16, 4, 16, 16, 16, 16, 0x0000FFFF, 0xFFFF0000, 0x0000FFFF, 0xFFFF0000, 0, 16, 0, 16 },
      { "RGB32F", 12, PFF_FLOAT, PCT_FLOAT32, 3, 32, 32, 32, 0, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0, 0, 0, 0, 0 },
      { "RGBA32F", 16, PFF_FLOAT | PFF_HASALPHA, PCT_FLOAT32, 4, 32, 32, 32, 32, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0, 0, 0, 0 },
      { "RG16F", 4, PFF_FLOAT, PCT_FLOAT16, 2, 16, 16, 0, 0, 0x0000FFFF, 0xFFFF0000, 0, 0, 0, 16, 0, 0 },
      { "RG32F", 8, PFF_FLOAT, PCT_FLOAT32, 2, 32, 32, 0, 0, 0xFFFFFFFF, 0xFFFFFFFF, 0, 0, 0, 0, 0, 0 },
      { "R32I", 4, PFF_INTEGER | PFF_SIGNED, PCT_INT, 1, 32, 0, 0, 0, 0xFFFFFFFF, 0, 0, 0, 0, 0, 0, 0 },
      { "D32", 4, PFF_DEPTH | PFF_FLOAT, PCT_FLOAT32, 1, 32, 0, 0, 0, 0xFFFFFFFF, 0, 0, 0, 0, 0, 0, 0 },
      { "D24S8", 4, PFF_INTEGER | PFF_DEPTH | PFF_NORMALIZED, PCT_INT, 2, 24, 8, 0, 0, 0x00FFFFFF, 0xFF000000, 0, 0, 0, 24, 0, 0 },
      { "BC1", 0, PFF_COMPRESSED, PCT_BYTE, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { "BC1a", 0, PFF_COMPRESSED | PFF_HASALPHA, PCT_BYTE, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { "BC2", 0, PFF_COMPRESSED | PFF_HASALPHA, PCT_BYTE, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { "BC3", 0, PFF_COMPRESSED | PFF_HASALPHA, PCT_BYTE, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { "BC4", 0, PFF_COMPRESSED, PCT_BYTE, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { "BC5", 0, PFF_COMPRESSED, PCT_BYTE, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { "BC6H", 0, PFF_COMPRESSED | PFF_FLOAT, PCT_FLOAT16, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { "BC7", 0, PFF_COMPRESSED | PFF_HASALPHA, PCT_BYTE, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { "BGRA8", 4, PFF_INTEGER | PFF_HASALPHA | PFF_NORMALIZED, PCT_BYTE, 4, 8, 8, 8, 8, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000, 16, 8, 0,
        24 },
      { "ETC2_RGB", 0, PFF_COMPRESSED, PCT_BYTE, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { "ETC2_RGBA", 0, PFF_COMPRESSED | PFF_HASALPHA, PCT_BYTE, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { "ETC2_R11", 0, PFF_COMPRESSED, PCT_BYTE, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { "ETC2_RG11", 0, PFF_COMPRESSED, PCT_BYTE, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { "ASTC4x4", 0, PFF_COMPRESSED | PFF_HASALPHA, PCT_BYTE, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { "R32F", 4, PFF_FLOAT, PCT_FLOAT32, 1, 32, 0, 0, 0, 0xFFFFFFFF, 0, 0, 0, 0, 0, 0, 0 },
      { "R16", 2, PFF_INTEGER | PFF_NORMALIZED, PCT_SHORT, 1, 16, 0, 0, 0, 0x0000FFFF, 0, 0, 0, 0, 0, 0, 0 },
      { "RG16", 4, PFF_INTEGER | PFF_NORMALIZED, PCT_SHORT, 2, 16, 16, 0, 0, 0x0000FFFF, 0xFFFF0000, 0, 0, 0, 16, 0, 0 },
      { "RGB16", 6, PFF_INTEGER | PFF_NORMALIZED, PCT_SHORT, 3, 16, 16, 16, 0, 0x0000FFFF, 0xFFFF0000, 0x0000FFFF, 0, 0, 16, 0, 0 },
      { "RGBA16", 8, PFF_INTEGER | PFF_NORMALIZED | PFF_HASALPHA, PCT_SHORT, 4, 16, 16, 16, 16, 0x0000FFFF, 0xFFFF0000, 0x0000FFFF, 0xFFFF0000, 0, 16,
        0, 16 },
    } };
    static_assert(PIXEL_FORMATS.size() == static_cast<size_t>(TextureFormat::FormatCount));

    constexpr PixelFormatDesc UNKNOWN_PIXEL_FORMAT = { "Unknown", 0, 0, PCT_BYTE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    static bool RangesOverlap(const void* first, size_t firstSize, const void* second, size_t secondSize)
    {
        if (first == nullptr || second == nullptr || firstSize == 0 || secondSize == 0)
            return false;

        const uintptr_t firstBegin = reinterpret_cast<uintptr_t>(first);
        const uintptr_t secondBegin = reinterpret_cast<uintptr_t>(second);
        const uintptr_t maximum = std::numeric_limits<uintptr_t>::max();
        if (firstSize > maximum - firstBegin || secondSize > maximum - secondBegin)
            return true;

        return firstBegin < secondBegin + secondSize && secondBegin < firstBegin + firstSize;
    }

    static inline const PixelFormatDesc& GetFormatDesc(const TextureFormat fmt)
    {
        const size_t index = static_cast<size_t>(fmt);
        if (index >= PIXEL_FORMATS.size())
            return UNKNOWN_PIXEL_FORMAT;
        return PIXEL_FORMATS[index];
    }

    bool PixelUtils::IsValidFormat(TextureFormat format)
    {
        const size_t index = static_cast<size_t>(format);
        return index > static_cast<size_t>(TextureFormat::NONE) && index < PIXEL_FORMATS.size();
    }

    bool PixelUtils::GenerateMipChain(const PixelData& source, const TextureMipGenerationOptions& options, Vector<Ref<PixelData>>& output,
                                      String* error)
    {
        output.clear();
        if (!source.IsValid() || source.GetDepth() != 1 || IsCompressedFormat(source.GetFormat()) || IsDepthFormat(source.GetFormat()) ||
            (IsIntegerFormat(source.GetFormat()) && !IsNormalizedFormat(source.GetFormat())))
        {
            SetMipError(error, "Mip generation requires one valid, uncompressed 2D color image");
            return false;
        }

        if (options.Mode >= TextureMipMode::Count)
        {
            SetMipError(error, "Mip generation mode is invalid");
            return false;
        }

        const char* filter = GetMipFilterName(options.Filter);
        if (filter == nullptr)
        {
            SetMipError(error, "Mip generation filter is invalid");
            return false;
        }

        uint32_t levelCount = GetMaxMipCount(source.GetWidth(), source.GetHeight());
        if (options.MaxLevels != 0)
            levelCount = std::min(levelCount, options.MaxLevels);
        levelCount = std::max(levelCount, 1u);

        output.reserve(levelCount);
        output.push_back(CreateRef<PixelData>(source));
        if (levelCount == 1)
            return true;

        const bool colorMip = options.Mode == TextureMipMode::Color;
        const bool normalMip = options.Mode == TextureMipMode::NormalMap;
        const bool hasAlpha = HasAlpha(source.GetFormat());
        const bool premultiplyAlpha = colorMip && hasAlpha && options.PremultiplyAlpha;
        const bool decodeSRGB = colorMip && options.SRGB;
        const float alphaCutoff = glm::clamp(options.AlphaCutoff, 0.0f, 1.0f);

        basisu::imagef baseImage(source.GetWidth(), source.GetHeight());
        for (uint32_t y = 0; y < source.GetHeight(); ++y)
        {
            for (uint32_t x = 0; x < source.GetWidth(); ++x)
            {
                glm::vec4 color;
                if (!source.TryGetColorAt(x, y, 0, color))
                {
                    SetMipError(error, "Mip source pixels cannot be read");
                    output.clear();
                    return false;
                }
                if (decodeSRGB)
                {
                    color.r = SRGBToLinear(color.r);
                    color.g = SRGBToLinear(color.g);
                    color.b = SRGBToLinear(color.b);
                }
                if (premultiplyAlpha)
                    color *= glm::vec4(color.a, color.a, color.a, 1.0f);
                baseImage(x, y).set(color.r, color.g, color.b, color.a);
            }
        }

        const float sourceCoverage = options.PreserveAlphaCoverage && hasAlpha ? AlphaCoverage(baseImage, alphaCutoff) : 0.0f;
        for (uint32_t mip = 1; mip < levelCount; ++mip)
        {
            const uint32_t width = std::max(source.GetWidth() >> mip, 1u);
            const uint32_t height = std::max(source.GetHeight() >> mip, 1u);
            basisu::imagef mipImage(width, height);
            if (!basisu::image_resample(baseImage, mipImage, filter, 1.0f, options.Wrap, 0, 4))
            {
                SetMipError(error, "Basis Universal failed to resample a mip level");
                output.clear();
                return false;
            }

            if (normalMip)
            {
                for (uint32_t y = 0; y < height; ++y)
                {
                    for (uint32_t x = 0; x < width; ++x)
                    {
                        glm::vec3 normal(mipImage(x, y)[0] * 2.0f - 1.0f, mipImage(x, y)[1] * 2.0f - 1.0f, mipImage(x, y)[2] * 2.0f - 1.0f);
                        const float lengthSquared = glm::dot(normal, normal);
                        normal = lengthSquared > 1e-10f ? normal * glm::inversesqrt(lengthSquared) : glm::vec3(0.0f, 0.0f, 1.0f);
                        mipImage(x, y)[0] = normal.x * 0.5f + 0.5f;
                        mipImage(x, y)[1] = normal.y * 0.5f + 0.5f;
                        mipImage(x, y)[2] = normal.z * 0.5f + 0.5f;
                    }
                }
            }
            if (options.PreserveAlphaCoverage && hasAlpha)
                PreserveAlphaCoverage(mipImage, sourceCoverage, alphaCutoff, premultiplyAlpha);

            Ref<PixelData> pixels = PixelData::Create(width, height, 1, source.GetFormat());
            for (uint32_t y = 0; y < height; ++y)
            {
                for (uint32_t x = 0; x < width; ++x)
                {
                    glm::vec4 color(mipImage(x, y)[0], mipImage(x, y)[1], mipImage(x, y)[2], mipImage(x, y)[3]);
                    if (premultiplyAlpha && color.a > 1e-8f)
                    {
                        color.r /= color.a;
                        color.g /= color.a;
                        color.b /= color.a;
                    }
                    else if (premultiplyAlpha)
                        color.r = color.g = color.b = 0.0f;
                    if (decodeSRGB)
                    {
                        color.r = LinearToSRGB(color.r);
                        color.g = LinearToSRGB(color.g);
                        color.b = LinearToSRGB(color.b);
                    }
                    if (!pixels->TrySetColorAt(x, y, 0, color))
                    {
                        SetMipError(error, "Generated mip pixels cannot be stored in the source format");
                        output.clear();
                        return false;
                    }
                }
            }
            output.push_back(std::move(pixels));
        }
        return true;
    }

    const char* PixelUtils::GetFormatName(TextureFormat format) { return GetFormatDesc(format).Name; }

    uint32_t PixelUtils::GetComponentCount(TextureFormat format) { return GetFormatDesc(format).ComponentCount; }

    uint32_t PixelUtils::GetFormatFlags(TextureFormat format)
    {
        const PixelFormatDesc& desc = GetFormatDesc(format);
        return desc.Flags;
    }

    glm::ivec2 PixelUtils::GetBlockDimensions(TextureFormat format)
    {
        switch (format)
        {
        case TextureFormat::BC1:
        case TextureFormat::BC1a:
        case TextureFormat::BC2:
        case TextureFormat::BC3:
        case TextureFormat::BC4:
        case TextureFormat::BC5:
        case TextureFormat::BC6H:
        case TextureFormat::BC7:
        case TextureFormat::ETC2_RGB:
        case TextureFormat::ETC2_RGBA:
        case TextureFormat::ETC2_R11:
        case TextureFormat::ETC2_RG11:
        case TextureFormat::ASTC4x4:
            return glm::ivec2(4, 4);
        default:
            return glm::ivec2(1, 1);
        }
    }

    uint32_t PixelUtils::GetBlockSize(TextureFormat format)
    {
        switch (format)
        {
        case TextureFormat::BC1:
        case TextureFormat::BC1a:
        case TextureFormat::BC4:
        case TextureFormat::ETC2_RGB:
        case TextureFormat::ETC2_R11:
            return 8;
        case TextureFormat::BC2:
        case TextureFormat::BC3:
        case TextureFormat::BC5:
        case TextureFormat::BC6H:
        case TextureFormat::BC7:
        case TextureFormat::ETC2_RGBA:
        case TextureFormat::ETC2_RG11:
        case TextureFormat::ASTC4x4:
            return 16;
        default:
            return GetNumBytes(format);
        }
    }

    void PixelUtils::GetPitch(uint32_t width, uint32_t height, uint32_t depth, TextureFormat format, uint32_t& rowPitch, uint32_t& depthPitch)
    {
        rowPitch = 0;
        depthPitch = 0;
        if (!IsValidFormat(format) || width == 0 || height == 0 || depth == 0)
            return;

        const glm::ivec2 blockDimensions = GetBlockDimensions(format);
        const size_t blockWidth = static_cast<size_t>(blockDimensions.x);
        const size_t blockHeight = static_cast<size_t>(blockDimensions.y);
        const size_t blocksWide = static_cast<size_t>(width) / blockWidth + (width % blockWidth != 0);
        const size_t blocksHigh = static_cast<size_t>(height) / blockHeight + (height % blockHeight != 0);
        const size_t blockSize = GetBlockSize(format);
        if (blockSize == 0 || blocksWide > std::numeric_limits<size_t>::max() / blockSize)
        {
            CW_ENGINE_ERROR("Pixel row pitch overflow");
            return;
        }

        const size_t rowSize = blocksWide * blockSize;
        if (rowSize > std::numeric_limits<size_t>::max() / blocksHigh)
        {
            CW_ENGINE_ERROR("Pixel slice pitch overflow");
            return;
        }

        const size_t sliceSize = rowSize * blocksHigh;
        if (rowSize > std::numeric_limits<uint32_t>::max() || sliceSize > std::numeric_limits<uint32_t>::max())
        {
            CW_ENGINE_ERROR("Pixel pitch exceeds the 32-bit PixelData layout limit");
            return;
        }
        rowPitch = static_cast<uint32_t>(rowSize);
        depthPitch = static_cast<uint32_t>(sliceSize);
    }

    void PixelUtils::GetMipSizeForLevel(uint32_t width, uint32_t height, uint32_t depth, uint32_t mipLevel, uint32_t& mipWidth, uint32_t& mipHeight,
                                        uint32_t& mipDepth)
    {
        const uint32_t shift = std::min(mipLevel, 31U);
        mipWidth = width == 0 ? 0 : std::max(1U, width >> shift);
        mipHeight = height == 0 ? 0 : std::max(1U, height >> shift);
        mipDepth = depth == 0 ? 0 : std::max(1U, depth >> shift);
    }

    uint32_t PixelUtils::GetMaxMipCount(uint32_t width, uint32_t height, uint32_t depth)
    {
        const uint32_t largestDimension = std::max({ width, height, depth });
        return largestDimension == 0 ? 0 : std::bit_width(largestDimension);
    }

    size_t PixelUtils::GetMemorySize(uint32_t width, uint32_t height, uint32_t depth, TextureFormat format)
    {
        uint32_t rowPitch = 0;
        uint32_t slicePitch = 0;
        GetPitch(width, height, depth, format, rowPitch, slicePitch);
        if (slicePitch != 0 && depth > std::numeric_limits<size_t>::max() / slicePitch)
        {
            CW_ENGINE_ERROR("Pixel buffer size overflow");
            return 0;
        }
        return static_cast<size_t>(slicePitch) * depth;
    }

    size_t PixelUtils::GetMipChainSize(uint32_t width, uint32_t height, uint32_t depth, TextureFormat format, uint32_t mipCount, uint32_t faceCount)
    {
        const uint32_t maximumMipCount = GetMaxMipCount(width, height, depth);
        if (!IsValidFormat(format) || maximumMipCount == 0 || faceCount == 0)
            return 0;

        mipCount = mipCount == 0 ? maximumMipCount : std::min(mipCount, maximumMipCount);
        size_t perFaceSize = 0;
        for (uint32_t mip = 0; mip < mipCount; mip++)
        {
            uint32_t mipWidth = 0;
            uint32_t mipHeight = 0;
            uint32_t mipDepth = 0;
            GetMipSizeForLevel(width, height, depth, mip, mipWidth, mipHeight, mipDepth);
            const size_t mipSize = GetMemorySize(mipWidth, mipHeight, mipDepth, format);
            if (mipSize == 0)
                return 0;
            if (mipSize > std::numeric_limits<size_t>::max() - perFaceSize)
            {
                CW_ENGINE_ERROR("Pixel mip chain size overflow");
                return 0;
            }
            perFaceSize += mipSize;
        }

        if (perFaceSize != 0 && faceCount > std::numeric_limits<size_t>::max() / perFaceSize)
        {
            CW_ENGINE_ERROR("Pixel mip chain face count overflow");
            return 0;
        }
        return perFaceSize * faceCount;
    }

    size_t PixelUtils::GetMipOffset(uint32_t width, uint32_t height, uint32_t depth, TextureFormat format, uint32_t mipLevel, uint32_t face,
                                    uint32_t mipCount)
    {
        const uint32_t maximumMipCount = GetMaxMipCount(width, height, depth);
        mipCount = mipCount == 0 ? maximumMipCount : std::min(mipCount, maximumMipCount);
        if (!IsValidFormat(format) || mipLevel >= mipCount)
            return 0;

        const size_t faceSize = GetMipChainSize(width, height, depth, format, mipCount);
        if (faceSize != 0 && face > std::numeric_limits<size_t>::max() / faceSize)
        {
            CW_ENGINE_ERROR("Pixel mip offset overflow");
            return 0;
        }

        size_t offset = face * faceSize;
        for (uint32_t mip = 0; mip < mipLevel; mip++)
        {
            uint32_t mipWidth = 0;
            uint32_t mipHeight = 0;
            uint32_t mipDepth = 0;
            GetMipSizeForLevel(width, height, depth, mip, mipWidth, mipHeight, mipDepth);
            const size_t mipSize = GetMemorySize(mipWidth, mipHeight, mipDepth, format);
            if (mipSize == 0)
                return 0;
            if (mipSize > std::numeric_limits<size_t>::max() - offset)
            {
                CW_ENGINE_ERROR("Pixel mip offset overflow");
                return 0;
            }
            offset += mipSize;
        }
        return offset;
    }

    uint32_t PixelUtils::GetMemSize(uint32_t width, uint32_t height, uint32_t depth, TextureFormat format)
    {
        const size_t size = GetMemorySize(width, height, depth, format);
        if (size > std::numeric_limits<uint32_t>::max())
        {
            CW_ENGINE_ERROR("Pixel buffer size exceeds the legacy 32-bit size limit");
            return 0;
        }
        return static_cast<uint32_t>(size);
    }

    bool PixelUtils::ConvertPixels(const PixelData& src, PixelData& dst)
    {
        if (src.GetWidth() != dst.GetWidth() || src.GetHeight() != dst.GetHeight() || src.GetDepth() != dst.GetDepth())
        {
            CW_ENGINE_ERROR("Cannot convert pixel buffers with different sizes");
            return false;
        }

        if (!src.IsValid() || !dst.IsValid())
        {
            CW_ENGINE_ERROR("Cannot convert invalid pixel buffers");
            return false;
        }

        if (src.GetData() == dst.GetData() && src.GetFormat() == dst.GetFormat() && src.GetRowPitch() == dst.GetRowPitch() &&
            src.GetSlicePitch() == dst.GetSlicePitch())
            return true;

        if (RangesOverlap(src.GetData(), src.GetSize(), dst.GetData(), dst.GetSize()))
        {
            PixelData sourceCopy(src);
            return ConvertPixels(sourceCopy, dst);
        }

        if (src.GetFormat() == dst.GetFormat())
        {
            if (src.IsNice() && dst.IsNice())
            {
                std::memmove(dst.GetData(), src.GetData(), src.GetConsecutiveSize());
                return true;
            }

            uint32_t tightRowPitch = 0;
            CW_MAYBE_UNUSED uint32_t tightSlicePitch = 0;
            GetPitch(src.GetWidth(), src.GetHeight(), src.GetDepth(), src.GetFormat(), tightRowPitch, tightSlicePitch);
            const uint32_t rowCount = src.GetPhysicalRowCount();
            for (uint32_t z = 0; z < src.GetDepth(); z++)
            {
                const uint8_t* srcSlice = src.GetData() + static_cast<size_t>(z) * src.GetSlicePitch();
                uint8_t* dstSlice = dst.GetData() + static_cast<size_t>(z) * dst.GetSlicePitch();
                for (uint32_t row = 0; row < rowCount; row++)
                {
                    std::memcpy(dstSlice + static_cast<size_t>(row) * dst.GetRowPitch(), srcSlice + static_cast<size_t>(row) * src.GetRowPitch(),
                                tightRowPitch);
                }
            }
            return true;
        }

        if (IsCompressedFormat(src.GetFormat()))
        {
            CW_ENGINE_ERROR("Cannot convert from compressed format {}", GetFormatName(src.GetFormat()));
            return false;
        }

        if (IsCompressedFormat(dst.GetFormat()))
        {
            CW_ENGINE_ERROR("Cannot convert to compressed format {} without an encoder", GetFormatName(dst.GetFormat()));
            return false;
        }

        const uint32_t srcPixelSize = GetNumBytes(src.GetFormat());
        const uint32_t dstPixelSize = GetNumBytes(dst.GetFormat());
        if (srcPixelSize == 0 || dstPixelSize == 0)
            return false;

        float r, g, b, a;
        for (uint32_t z = 0; z < src.GetDepth(); z++)
        {
            for (uint32_t y = 0; y < src.GetHeight(); y++)
            {
                const uint8_t* srcPtr = src.GetData() + static_cast<size_t>(z) * src.GetSlicePitch() + static_cast<size_t>(y) * src.GetRowPitch();
                uint8_t* dstPtr = dst.GetData() + static_cast<size_t>(z) * dst.GetSlicePitch() + static_cast<size_t>(y) * dst.GetRowPitch();
                for (uint32_t x = 0; x < src.GetWidth(); x++)
                {
                    UnpackPixel(&r, &g, &b, &a, src.GetFormat(), srcPtr);
                    PackPixel(r, g, b, a, dst.GetFormat(), dstPtr);
                    srcPtr += srcPixelSize;
                    dstPtr += dstPixelSize;
                }
            }
        }
        return true;
    }

    void PixelUtils::UnpackPixel(float* r, float* g, float* b, float* a, TextureFormat format, const uint8_t* src)
    {
        float* outputs[] = { r, g, b, a };
        const PixelFormatDesc& desc = GetFormatDesc(format);
        const uint8_t bits[] = { desc.Rbits, desc.Gbits, desc.Bbits, desc.Abits };
        const uint32_t masks[] = { desc.Rmask, desc.Gmask, desc.Bmask, desc.Amask };
        const uint8_t shifts[] = { desc.Rshift, desc.Gshift, desc.Bshift, desc.Ashift };

        if (r != nullptr)
            *r = 0.0f;
        if (g != nullptr)
            *g = 0.0f;
        if (b != nullptr)
            *b = 0.0f;
        if (a != nullptr)
            *a = 1.0f;
        if (src == nullptr || !IsValidFormat(format) || IsCompressedFormat(format) || desc.ElementBytes == 0)
        {
            CW_ENGINE_ERROR("Cannot unpack pixel format {}", desc.Name);
            return;
        }

        uint32_t curBit = 0;
        for (uint32_t i = 0; i < desc.ComponentCount; i++)
        {
            const uint32_t wordIndex = curBit / 32;
            const uint32_t byteOffset = wordIndex * 4;
            const uint32_t numBytes = std::min(byteOffset + 4, static_cast<uint32_t>(desc.ElementBytes)) - byteOffset;
            uint32_t value = 0;
            std::memcpy(&value, src + byteOffset, numBytes);
            float output = 0.0f;
            if (desc.Flags & PFF_INTEGER)
            {
                const uint32_t rawValue = (value & masks[i]) >> shifts[i];
                if (desc.Flags & PFF_NORMALIZED)
                {
                    if (desc.Flags & PFF_SIGNED)
                        output = Bitwise::UintToSnorm(rawValue, bits[i]);
                    else
                        output = Bitwise::UintToUnorm(rawValue, bits[i]);
                }
                else if (desc.Flags & PFF_SIGNED)
                {
                    uint32_t signedBits = rawValue;
                    if (bits[i] < 32 && (signedBits & (1U << (bits[i] - 1))) != 0)
                        signedBits |= ~((1U << bits[i]) - 1U);
                    output = static_cast<float>(static_cast<int32_t>(signedBits));
                }
                else
                    output = static_cast<float>(rawValue);
            }
            else if (desc.Flags & PFF_FLOAT)
            {
                if (desc.ComponentType == PCT_FLOAT16)
                    output = Bitwise::HalfToFloat((uint16_t)((value & masks[i]) >> shifts[i]));
                else
                    output = std::bit_cast<float>(value);
            }
            else
            {
                CW_ENGINE_ERROR("UnpackPixel format not supported.");
                return;
            }
            if (outputs[i] != nullptr)
                *outputs[i] = output;
            curBit += bits[i];
        }
    }

    void PixelUtils::PackPixel(float r, float g, float b, float a, TextureFormat format, uint8_t* dst)
    {

        const PixelFormatDesc& desc = GetFormatDesc(format);
        const float inputs[] = { r, g, b, a };
        const uint8_t bits[] = { desc.Rbits, desc.Gbits, desc.Bbits, desc.Abits };
        const uint32_t masks[] = { desc.Rmask, desc.Gmask, desc.Bmask, desc.Amask };
        const uint8_t shifts[] = { desc.Rshift, desc.Gshift, desc.Bshift, desc.Ashift };

        if (dst == nullptr || !IsValidFormat(format) || IsCompressedFormat(format) || desc.ElementBytes == 0)
        {
            CW_ENGINE_ERROR("Cannot pack pixel format {}", desc.Name);
            return;
        }

        std::memset(dst, 0, desc.ElementBytes);
        uint32_t curBit = 0;

        for (uint32_t i = 0; i < desc.ComponentCount; i++)
        {
            const uint32_t wordIndex = curBit / 32;
            const uint32_t byteOffset = wordIndex * 4;
            const uint32_t numBytes = std::min(byteOffset + 4, static_cast<uint32_t>(desc.ElementBytes)) - byteOffset;
            uint32_t word = 0;
            std::memcpy(&word, dst + byteOffset, numBytes);
            uint32_t componentValue = 0;

            if (desc.Flags & PFF_INTEGER)
            {
                const float input = std::isnan(inputs[i]) ? 0.0f : inputs[i];
                if (desc.Flags & PFF_NORMALIZED)
                {
                    if (desc.Flags & PFF_SIGNED)
                        componentValue = Bitwise::SnormToUint(input, bits[i]);
                    else
                        componentValue = Bitwise::UnormToUint(input, bits[i]);
                }
                else if (desc.Flags & PFF_SIGNED)
                {
                    const double minimum =
                      bits[i] == 32 ? static_cast<double>(std::numeric_limits<int32_t>::min()) : -static_cast<double>(1LL << (bits[i] - 1));
                    const double maximum =
                      bits[i] == 32 ? static_cast<double>(std::numeric_limits<int32_t>::max()) : static_cast<double>((1LL << (bits[i] - 1)) - 1);
                    componentValue = static_cast<uint32_t>(static_cast<int32_t>(std::clamp(static_cast<double>(input), minimum, maximum)));
                }
                else
                {
                    const double maximum =
                      bits[i] == 32 ? static_cast<double>(std::numeric_limits<uint32_t>::max()) : static_cast<double>((1ULL << bits[i]) - 1ULL);
                    componentValue = static_cast<uint32_t>(std::clamp(static_cast<double>(input), 0.0, maximum));
                }
            }
            else if (desc.Flags & PFF_FLOAT)
            {
                if (desc.ComponentType == PCT_FLOAT16)
                    componentValue = Bitwise::FloatToHalf(inputs[i]);
                else
                    componentValue = std::bit_cast<uint32_t>(inputs[i]);
            }
            else
            {
                CW_ENGINE_ERROR("PackPixel format not supported");
                return;
            }
            word = (word & ~masks[i]) | ((componentValue << shifts[i]) & masks[i]);
            std::memcpy(dst + byteOffset, &word, numBytes);
            curBit += bits[i];
        }
    }

    uint32_t PixelUtils::GetNumBytes(TextureFormat format) { return GetFormatDesc(format).ElementBytes; }

    void PixelUtils::GetBitDepths(TextureFormat format, int (&rgba)[4])
    {
        const PixelFormatDesc& desc = GetFormatDesc(format);
        rgba[0] = desc.Rbits;
        rgba[1] = desc.Gbits;
        rgba[2] = desc.Bbits;
        rgba[3] = desc.Abits;
    }

    PixelData::PixelData(uint32_t width, uint32_t height, uint32_t depth, TextureFormat textureFormat)
      : m_OwnsData(false), m_Format(textureFormat), m_Width(width), m_Height(height), m_Depth(depth), m_Buffer(nullptr)
    {
        PixelUtils::GetPitch(width, height, depth, textureFormat, m_RowPitch, m_SlicePitch);
    }

    PixelData::PixelData(const PixelData& other)
      : RefCounted(other), m_OwnsData(false), m_Format(other.m_Format), m_Width(other.m_Width), m_Height(other.m_Height), m_Depth(other.m_Depth),
        m_RowPitch(other.m_RowPitch), m_SlicePitch(other.m_SlicePitch)
    {
        if (other.IsValid())
        {
            m_Buffer = new uint8_t[other.GetSize()];
            std::memcpy(m_Buffer, other.m_Buffer, other.GetSize());
            m_OwnsData = true;
        }
    }

    PixelData::PixelData(PixelData&& other) noexcept
      : RefCounted(std::move(other)), m_OwnsData(other.m_OwnsData), m_Format(other.m_Format), m_Width(other.m_Width), m_Height(other.m_Height),
        m_Depth(other.m_Depth), m_RowPitch(other.m_RowPitch), m_SlicePitch(other.m_SlicePitch), m_Buffer(other.m_Buffer)
    {
        other.m_OwnsData = false;
        other.m_Format = TextureFormat::NONE;
        other.m_Width = 0;
        other.m_Height = 0;
        other.m_Depth = 0;
        other.m_RowPitch = 0;
        other.m_SlicePitch = 0;
        other.m_Buffer = nullptr;
    }

    PixelData& PixelData::operator=(const PixelData& other)
    {
        if (this != &other)
        {
            PixelData copy(other);
            Swap(copy);
        }
        return *this;
    }

    PixelData& PixelData::operator=(PixelData&& other) noexcept
    {
        if (this != &other)
        {
            Clear();
            RefCounted::operator=(std::move(other));
            m_OwnsData = other.m_OwnsData;
            m_Format = other.m_Format;
            m_Width = other.m_Width;
            m_Height = other.m_Height;
            m_Depth = other.m_Depth;
            m_RowPitch = other.m_RowPitch;
            m_SlicePitch = other.m_SlicePitch;
            m_Buffer = other.m_Buffer;

            other.m_OwnsData = false;
            other.m_Format = TextureFormat::NONE;
            other.m_Width = 0;
            other.m_Height = 0;
            other.m_Depth = 0;
            other.m_RowPitch = 0;
            other.m_SlicePitch = 0;
            other.m_Buffer = nullptr;
        }
        return *this;
    }

    PixelData::~PixelData() { Clear(); }

    void PixelData::Swap(PixelData& other) noexcept
    {
        std::swap(m_OwnsData, other.m_OwnsData);
        std::swap(m_Format, other.m_Format);
        std::swap(m_Width, other.m_Width);
        std::swap(m_Height, other.m_Height);
        std::swap(m_Depth, other.m_Depth);
        std::swap(m_RowPitch, other.m_RowPitch);
        std::swap(m_SlicePitch, other.m_SlicePitch);
        std::swap(m_Buffer, other.m_Buffer);
    }

    uint32_t PixelData::GetPhysicalRowCount() const
    {
        const glm::ivec2 blockDimensions = PixelUtils::GetBlockDimensions(m_Format);
        if (blockDimensions.y <= 0)
            return 0;

        const uint32_t blockHeight = static_cast<uint32_t>(blockDimensions.y);
        return m_Height / blockHeight + (m_Height % blockHeight != 0);
    }

    size_t PixelData::GetSize() const
    {
        if (m_SlicePitch == 0 || m_Depth == 0 || m_Depth > std::numeric_limits<size_t>::max() / m_SlicePitch)
            return 0;
        return static_cast<size_t>(m_SlicePitch) * m_Depth;
    }

    bool PixelData::HasValidPitches() const
    {
        if (!PixelUtils::IsValidFormat(m_Format) || m_Width == 0 || m_Height == 0 || m_Depth == 0)
            return false;

        uint32_t tightRowPitch = 0;
        uint32_t ignoredSlicePitch = 0;
        PixelUtils::GetPitch(m_Width, m_Height, m_Depth, m_Format, tightRowPitch, ignoredSlicePitch);
        const uint32_t physicalRowCount = GetPhysicalRowCount();
        if (physicalRowCount == 0 || m_RowPitch > std::numeric_limits<size_t>::max() / physicalRowCount)
            return false;

        const size_t minimumSlicePitch = static_cast<size_t>(m_RowPitch) * physicalRowCount;
        return tightRowPitch > 0 && m_RowPitch >= tightRowPitch && m_SlicePitch >= minimumSlicePitch && GetSize() != 0;
    }

    bool PixelData::IsValid() const { return m_Buffer != nullptr && HasValidPitches(); }

    bool PixelData::IsNice() const
    {
        uint32_t tightRowPitch = 0;
        uint32_t tightSlicePitch = 0;
        PixelUtils::GetPitch(m_Width, m_Height, m_Depth, m_Format, tightRowPitch, tightSlicePitch);
        return HasValidPitches() && m_RowPitch == tightRowPitch && m_SlicePitch == tightSlicePitch;
    }

    bool PixelData::TryGetColorAt(uint32_t x, uint32_t y, uint32_t z, glm::vec4& color) const
    {
        color = glm::vec4(0.0f);
        const uint32_t pixelSize = PixelUtils::GetNumBytes(m_Format);
        if (!IsValid() || PixelUtils::IsCompressedFormat(m_Format) || pixelSize == 0 || x >= m_Width || y >= m_Height || z >= m_Depth)
            return false;

        const size_t offset = static_cast<size_t>(z) * m_SlicePitch + static_cast<size_t>(y) * m_RowPitch + static_cast<size_t>(x) * pixelSize;
        const size_t size = GetSize();
        if (offset > size || pixelSize > size - offset)
            return false;
        PixelUtils::UnpackPixel(&color.x, &color.y, &color.z, &color.w, m_Format, GetData() + offset);
        return true;
    }

    glm::vec4 PixelData::GetColorAt(uint32_t x, uint32_t y, uint32_t z) const
    {
        glm::vec4 color(0.0f);
        TryGetColorAt(x, y, z, color);
        return color;
    }

    bool PixelData::TrySetColorAt(uint32_t x, uint32_t y, uint32_t z, const glm::vec4& color)
    {
        const uint32_t pixelSize = PixelUtils::GetNumBytes(m_Format);
        if (!IsValid() || PixelUtils::IsCompressedFormat(m_Format) || pixelSize == 0 || x >= m_Width || y >= m_Height || z >= m_Depth)
            return false;

        const size_t offset = static_cast<size_t>(z) * m_SlicePitch + static_cast<size_t>(y) * m_RowPitch + static_cast<size_t>(x) * pixelSize;
        const size_t size = GetSize();
        if (offset > size || pixelSize > size - offset)
            return false;
        PixelUtils::PackPixel(color.r, color.g, color.b, color.a, m_Format, GetData() + offset);
        return true;
    }

    void PixelData::SetColorAt(uint32_t x, uint32_t y, const glm::vec4& color) { SetColorAt(x, y, 0, color); }

    void PixelData::SetColorAt(uint32_t x, uint32_t y, uint32_t z, const glm::vec4& color) { TrySetColorAt(x, y, z, color); }

    void PixelData::AllocateInternalBuffer()
    {
        Clear();
        if (!HasValidPitches() || GetSize() == 0)
        {
            CW_ENGINE_ERROR("Cannot allocate invalid pixel buffer layout");
            return;
        }
        m_Buffer = new uint8_t[GetSize()];
        std::memset(m_Buffer, 0, GetSize());
        m_OwnsData = true;
    }

    bool PixelData::SetBuffer(uint8_t* data)
    {
        if (data != nullptr && !HasValidPitches())
        {
            CW_ENGINE_ERROR("Cannot bind storage to an invalid pixel layout");
            return false;
        }
        if (m_Buffer == data)
            return true;

        Clear();
        m_Buffer = data;
        m_OwnsData = false;
        return true;
    }

    bool PixelData::SetOwnedBuffer(uint8_t* data)
    {
        if (data != nullptr && !HasValidPitches())
        {
            CW_ENGINE_ERROR("Cannot bind owned storage to an invalid pixel layout");
            return false;
        }
        if (m_Buffer == data)
        {
            m_OwnsData = data != nullptr;
            return true;
        }

        Clear();
        m_Buffer = data;
        m_OwnsData = data != nullptr;
        return true;
    }

    bool PixelData::SetRowPitch(uint32_t rowPitch)
    {
        if (rowPitch == m_RowPitch)
            return true;
        if (m_Buffer != nullptr)
        {
            CW_ENGINE_ERROR("Cannot change pixel row pitch while storage is bound");
            return false;
        }

        m_RowPitch = rowPitch;
        return true;
    }

    bool PixelData::SetSlicePitch(uint32_t slicePitch)
    {
        if (slicePitch == m_SlicePitch)
            return true;
        if (m_Buffer != nullptr)
        {
            CW_ENGINE_ERROR("Cannot change pixel slice pitch while storage is bound");
            return false;
        }

        m_SlicePitch = slicePitch;
        return true;
    }

    uint8_t* PixelData::ReleaseBuffer()
    {
        uint8_t* buffer = m_Buffer;
        m_Buffer = nullptr;
        m_OwnsData = false;
        return buffer;
    }

    void PixelData::Clear()
    {
        if (m_OwnsData)
            delete[] m_Buffer;
        m_Buffer = nullptr;
        m_OwnsData = false;
    }

    uint32_t PixelData::GetSliceSkip() const
    {
        const uint32_t physicalRowCount = GetPhysicalRowCount();
        if (physicalRowCount == 0 || m_RowPitch > std::numeric_limits<size_t>::max() / physicalRowCount)
            return 0;

        const size_t usedSliceBytes = static_cast<size_t>(m_RowPitch) * physicalRowCount;
        return m_SlicePitch >= usedSliceBytes ? static_cast<uint32_t>(m_SlicePitch - usedSliceBytes) : 0;
    }

    uint32_t PixelData::GetRowSkip() const
    {
        uint32_t optimalRowPitch, optimalSlicePitch;
        PixelUtils::GetPitch(GetWidth(), GetHeight(), GetDepth(), m_Format, optimalRowPitch, optimalSlicePitch);
        return m_RowPitch >= optimalRowPitch ? m_RowPitch - optimalRowPitch : 0;
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

    Ref<PixelData> PixelData::CreateView(uint32_t width, uint32_t height, uint32_t depth, TextureFormat format, uint8_t* data, uint32_t rowPitch,
                                         uint32_t slicePitch)
    {
        Ref<PixelData> pixelData = CreateRef<PixelData>(width, height, depth, format);
        if (rowPitch != 0)
            pixelData->SetRowPitch(rowPitch);
        if (slicePitch != 0)
            pixelData->SetSlicePitch(slicePitch);
        else if (rowPitch != 0)
        {
            const uint32_t physicalRowCount = pixelData->GetPhysicalRowCount();
            const bool canDerivePitch = physicalRowCount != 0 && rowPitch <= std::numeric_limits<size_t>::max() / physicalRowCount;
            const size_t derivedSlicePitch = canDerivePitch ? static_cast<size_t>(rowPitch) * physicalRowCount : 0;
            if (canDerivePitch && derivedSlicePitch <= std::numeric_limits<uint32_t>::max())
                pixelData->SetSlicePitch(static_cast<uint32_t>(derivedSlicePitch));
            else
                pixelData->SetSlicePitch(0);
        }
        pixelData->SetBuffer(data);
        if (!pixelData->HasValidPitches())
            CW_ENGINE_ERROR("Created PixelData view with an invalid pitch layout");
        return pixelData;
    }

} // namespace Crowny
