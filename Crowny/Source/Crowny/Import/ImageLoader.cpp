#include "cwpch.h"

#include "Crowny/Import/ImageLoader.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/StringUtils.h"
#include "Crowny/Renderer/BasisTextureCodec.h"

#include <stb_image.h>

namespace Crowny
{
    namespace
    {
        constexpr uint8_t KTX2_IDENTIFIER[] = { 0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A };
        constexpr size_t MAX_RASTER_PROBE_BYTES = 1024u * 1024u;

        struct Pnm16Info
        {
            uint32_t Width = 0;
            uint32_t Height = 0;
            uint32_t Channels = 0;
            uint32_t MaxValue = 0;
            size_t DataOffset = 0;
        };

        enum class Pnm16ParseStatus
        {
            NotPnm,
            Not16Bit,
            Valid,
            Invalid,
        };

        bool IsKTX2(const uint8_t* data, size_t size)
        {
            return data != nullptr && size >= sizeof(KTX2_IDENTIFIER) &&
                   std::memcmp(data, KTX2_IDENTIFIER, sizeof(KTX2_IDENTIFIER)) == 0;
        }

        bool IsPnmWhitespace(uint8_t value)
        {
            return value == ' ' || value == '\t' || value == '\n' || value == '\v' || value == '\f' || value == '\r';
        }

        void SkipPnmSeparators(const uint8_t* data, size_t size, size_t& cursor)
        {
            for (;;)
            {
                while (cursor < size && IsPnmWhitespace(data[cursor]))
                    cursor++;
                if (cursor >= size || data[cursor] != '#')
                    return;
                while (cursor < size && data[cursor] != '\n' && data[cursor] != '\r')
                    cursor++;
            }
        }

        bool ReadPnmInteger(const uint8_t* data, size_t size, size_t& cursor, uint32_t& value)
        {
            SkipPnmSeparators(data, size, cursor);
            if (cursor >= size || data[cursor] < '0' || data[cursor] > '9')
                return false;

            uint64_t parsed = 0;
            while (cursor < size && data[cursor] >= '0' && data[cursor] <= '9')
            {
                parsed = parsed * 10u + static_cast<uint64_t>(data[cursor] - '0');
                if (parsed > std::numeric_limits<uint32_t>::max())
                    return false;
                cursor++;
            }
            value = static_cast<uint32_t>(parsed);
            return true;
        }

        Pnm16ParseStatus ParsePnm16(const uint8_t* data, size_t size, Pnm16Info& info, String* error = nullptr,
                                    bool requireCompleteRaster = false)
        {
            if (data == nullptr || size < 3 || data[0] != 'P' || (data[1] != '5' && data[1] != '6'))
                return Pnm16ParseStatus::NotPnm;

            size_t cursor = 2;
            uint32_t width = 0;
            uint32_t height = 0;
            uint32_t maxValue = 0;
            if (!ReadPnmInteger(data, size, cursor, width) || !ReadPnmInteger(data, size, cursor, height) ||
                !ReadPnmInteger(data, size, cursor, maxValue) || width == 0 || height == 0 || maxValue == 0 || maxValue > 65535)
            {
                if (error != nullptr)
                    *error = "PNM header has invalid dimensions or maximum value";
                return Pnm16ParseStatus::Invalid;
            }
            if (maxValue <= 255)
                return Pnm16ParseStatus::Not16Bit;
            if (cursor >= size || !IsPnmWhitespace(data[cursor]))
            {
                if (error != nullptr)
                    *error = "16-bit PNM header is not followed by raster data";
                return Pnm16ParseStatus::Invalid;
            }

            if (data[cursor] == '\r' && cursor + 1 < size && data[cursor + 1] == '\n')
                cursor += 2;
            else
                cursor++;

            const uint32_t channels = data[1] == '6' ? 3u : 1u;
            const uint64_t pixelCount = static_cast<uint64_t>(width) * height;
            if (pixelCount > std::numeric_limits<uint64_t>::max() / channels)
            {
                if (error != nullptr)
                    *error = "16-bit PNM dimensions exceed the supported range";
                return Pnm16ParseStatus::Invalid;
            }

            const uint64_t sampleCount = pixelCount * channels;
            if (requireCompleteRaster && sampleCount > (size - cursor) / sizeof(uint16_t))
            {
                if (error != nullptr)
                    *error = "16-bit PNM raster data is truncated";
                return Pnm16ParseStatus::Invalid;
            }

            info.Width = width;
            info.Height = height;
            info.Channels = channels;
            info.MaxValue = maxValue;
            info.DataOffset = cursor;
            return Pnm16ParseStatus::Valid;
        }

        TextureFormat GetByteFormat(uint32_t channels)
        {
            switch (channels)
            {
            case 1: return TextureFormat::R8;
            case 2: return TextureFormat::RG8;
            case 3: return TextureFormat::RGB8;
            case 4: return TextureFormat::RGBA8;
            default: return TextureFormat::NONE;
            }
        }

        TextureFormat GetFloatFormat(uint32_t channels)
        {
            switch (channels)
            {
            case 1: return TextureFormat::R32F;
            case 2: return TextureFormat::RG32F;
            case 3: return TextureFormat::RGB32F;
            case 4: return TextureFormat::RGBA32F;
            default: return TextureFormat::NONE;
            }
        }

        TextureFormat GetBasisFormat(const BasisTextureInfo& info)
        {
            const TextureFormat format = GetByteFormat(info.Components);
            return PixelUtils::IsValidFormat(format) ? format : (info.HasAlpha ? TextureFormat::RGBA8 : TextureFormat::RGB8);
        }

        ImageLoadResult CanceledResult()
        {
            ImageLoadResult result;
            result.Canceled = true;
            return result;
        }

        ImageLoadResult ErrorResult(String error)
        {
            ImageLoadResult result;
            result.Error = std::move(error);
            return result;
        }

        bool ReadFile(const Path& path, Vector<uint8_t>& data, size_t maximumBytes = std::numeric_limits<size_t>::max())
        {
            const Ref<DataStream> stream = FileSystem::OpenFile(path);
            if (stream == nullptr || !stream->IsReadable() || stream->Size() == 0)
                return false;

            const size_t size = static_cast<size_t>(std::min<uint64_t>(stream->Size(), maximumBytes));
            data.resize(size);
            const bool read = stream->Read(data.data(), size) == size;
            stream->Close();
            return read;
        }

        void FillBasisInfo(const BasisTextureInfo& basis, ImageInfo& info)
        {
            info.Container = ImageContainerFormat::KTX2;
            info.Width = basis.Width;
            info.Height = basis.Height;
            info.Depth = 1;
            info.Faces = basis.Faces;
            info.MipLevels = basis.Levels;
            info.Channels = basis.Components;
            info.BitDepth = 8;
            info.PixelFormat = GetBasisFormat(basis);
            info.DiskFormat = basis.DiskFormat;
            info.HasAlpha = basis.HasAlpha;
            info.IsCompressed = true;
            info.SRGB = basis.SRGB;
        }

        ImageLoadResult ProbeBytes(const uint8_t* data, size_t size, const ImageLoadOptions& options)
        {
            if (options.IsCancellationRequested())
                return CanceledResult();
            if (data == nullptr || size == 0)
                return ErrorResult("Image source is empty");
            if (size > static_cast<size_t>(std::numeric_limits<int>::max()))
                return ErrorResult("Image source is too large for the raster decoder");

            if (IsKTX2(data, size))
            {
                BasisTextureInfo basis;
                String error;
                if (!BasisTextureCodec::Inspect(data, size, basis, &error))
                    return ErrorResult("KTX2 probe failed: " + error);

                ImageLoadResult result;
                FillBasisInfo(basis, result.Info);
                return result;
            }

            Pnm16Info pnm;
            String pnmError;
            const Pnm16ParseStatus pnmStatus = ParsePnm16(data, size, pnm, &pnmError);
            if (pnmStatus == Pnm16ParseStatus::Invalid)
                return ErrorResult("16-bit PNM probe failed: " + pnmError);
            if (pnmStatus == Pnm16ParseStatus::Valid)
            {
                ImageLoadResult result;
                result.Info.Container = ImageContainerFormat::Raster;
                result.Info.Width = pnm.Width;
                result.Info.Height = pnm.Height;
                result.Info.Channels = pnm.Channels;
                result.Info.BitDepth = 16;
                result.Info.IsFloat = options.Preserve16Bit;
                result.Info.PixelFormat = result.Info.IsFloat ? GetFloatFormat(pnm.Channels) : GetByteFormat(pnm.Channels);
                return result;
            }

            int width = 0;
            int height = 0;
            int channels = 0;
            if (stbi_info_from_memory(data, static_cast<int>(size), &width, &height, &channels) == 0 || width <= 0 || height <= 0)
            {
                const char* reason = stbi_failure_reason();
                return ErrorResult(String("Raster image probe failed: ") + (reason != nullptr ? reason : "unsupported or corrupt data"));
            }

            ImageLoadResult result;
            result.Info.Container = ImageContainerFormat::Raster;
            result.Info.Width = static_cast<uint32_t>(width);
            result.Info.Height = static_cast<uint32_t>(height);
            result.Info.Channels = static_cast<uint32_t>(channels);
            result.Info.HasAlpha = channels == 4;
            result.Info.IsHDR = stbi_is_hdr_from_memory(data, static_cast<int>(size)) != 0;
            const bool is16Bit = !result.Info.IsHDR && stbi_is_16_bit_from_memory(data, static_cast<int>(size)) != 0;
            result.Info.BitDepth = result.Info.IsHDR ? 32u : (is16Bit ? 16u : 8u);
            result.Info.IsFloat = result.Info.IsHDR || is16Bit && options.Preserve16Bit;
            result.Info.PixelFormat = result.Info.IsFloat ? GetFloatFormat(result.Info.Channels) : GetByteFormat(result.Info.Channels);
            if (!PixelUtils::IsValidFormat(result.Info.PixelFormat))
                return ErrorResult("Raster image has an unsupported channel layout");
            return result;
        }

        template <typename T, typename Convert>
        Ref<PixelData> CopyPixels(const T* source, const ImageInfo& info, bool flipVertically, Convert&& convert)
        {
            Ref<PixelData> pixels = PixelData::Create(info.Width, info.Height, 1, info.PixelFormat);
            if (!pixels || !pixels->IsValid())
                return nullptr;

            const size_t componentCount = info.Channels;
            const uint32_t pixelBytes = PixelUtils::GetNumBytes(info.PixelFormat);
            for (uint32_t y = 0; y < info.Height; y++)
            {
                const uint32_t sourceY = flipVertically ? info.Height - y - 1u : y;
                const T* sourceRow = source + static_cast<size_t>(sourceY) * info.Width * componentCount;
                uint8_t* destination = pixels->GetData() + static_cast<size_t>(y) * pixels->GetRowPitch();
                for (uint32_t x = 0; x < info.Width; x++)
                {
                    glm::vec4 color(0.0f, 0.0f, 0.0f, 1.0f);
                    for (uint32_t channel = 0; channel < componentCount; channel++)
                        color[channel] = convert(sourceRow[static_cast<size_t>(x) * componentCount + channel]);
                    PixelUtils::PackPixel(color.r, color.g, color.b, color.a, info.PixelFormat, destination);
                    destination += pixelBytes;
                }
            }
            return pixels;
        }

        Ref<PixelData> DecodePnm16(const uint8_t* data, const Pnm16Info& pnm, const ImageInfo& info, bool flipVertically)
        {
            Ref<PixelData> pixels = PixelData::Create(info.Width, info.Height, 1, info.PixelFormat);
            if (!pixels || !pixels->IsValid())
                return nullptr;

            const float inverseMaxValue = 1.0f / static_cast<float>(pnm.MaxValue);
            const uint32_t pixelBytes = PixelUtils::GetNumBytes(info.PixelFormat);
            for (uint32_t y = 0; y < info.Height; y++)
            {
                const uint32_t sourceY = flipVertically ? info.Height - y - 1u : y;
                uint8_t* destination = pixels->GetData() + static_cast<size_t>(y) * pixels->GetRowPitch();
                for (uint32_t x = 0; x < info.Width; x++)
                {
                    glm::vec4 color(0.0f, 0.0f, 0.0f, 1.0f);
                    const uint64_t firstSample = (static_cast<uint64_t>(sourceY) * info.Width + x) * pnm.Channels;
                    for (uint32_t channel = 0; channel < pnm.Channels; channel++)
                    {
                        const size_t offset = pnm.DataOffset + static_cast<size_t>(firstSample + channel) * sizeof(uint16_t);
                        const uint16_t sample = static_cast<uint16_t>((static_cast<uint16_t>(data[offset]) << 8u) |
                                                                      static_cast<uint16_t>(data[offset + 1u]));
                        color[channel] = static_cast<float>(sample) * inverseMaxValue;
                    }
                    PixelUtils::PackPixel(color.r, color.g, color.b, color.a, info.PixelFormat, destination);
                    destination += pixelBytes;
                }
            }
            return pixels;
        }
    } // namespace

    bool ImageLoader::SupportsExtension(StringView extension)
    {
        if (!extension.empty() && extension.front() == '.')
            extension.remove_prefix(1);
        String normalized(extension);
        StringUtils::ToLower(normalized);
        return normalized == "png" || normalized == "jpeg" || normalized == "jpg" || normalized == "psd" || normalized == "gif" ||
               normalized == "tga" || normalized == "bmp" || normalized == "hdr" || normalized == "pic" || normalized == "ppm" ||
               normalized == "pgm" || normalized == "ktx2";
    }

    bool ImageLoader::SupportsSignature(const uint8_t* data, size_t size)
    {
        if (data == nullptr || size == 0)
            return false;
        if (IsKTX2(data, size))
            return true;
        if (size >= 8 && std::memcmp(data, "\x89PNG\r\n\x1A\n", 8) == 0)
            return true;
        if (size >= 3 && data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF)
            return true;
        if (size >= 2 && data[0] == 'B' && data[1] == 'M')
            return true;
        if (size >= 6 && (std::memcmp(data, "GIF87a", 6) == 0 || std::memcmp(data, "GIF89a", 6) == 0))
            return true;
        if (size >= 4 && std::memcmp(data, "8BPS", 4) == 0)
            return true;
        if (size >= 2 && data[0] == 'P' && data[1] >= '1' && data[1] <= '6')
            return true;
        if (size >= 6 && (std::memcmp(data, "#?RGBE", 6) == 0 || std::memcmp(data, "#?RADI", 6) == 0))
            return true;
        if (size >= 4 && data[0] == 0x53 && data[1] == 0x80 && data[2] == 0xF6 && data[3] == 0x34)
            return true;
        if (size > static_cast<size_t>(std::numeric_limits<int>::max()))
            return false;
        int width = 0;
        int height = 0;
        int channels = 0;
        return stbi_info_from_memory(data, static_cast<int>(size), &width, &height, &channels) != 0;
    }

    ImageLoadResult ImageLoader::Probe(const Path& path, const ImageLoadOptions& options)
    {
        if (options.IsCancellationRequested())
            return CanceledResult();

        Vector<uint8_t> data;
        if (!ReadFile(path, data, MAX_RASTER_PROBE_BYTES))
            return ErrorResult("Could not read image source '" + path.string() + "'");

        if (IsKTX2(data.data(), data.size()))
        {
            const uint64_t fileSize = FileSystem::GetFileSize(path);
            if (fileSize > data.size() && !ReadFile(path, data))
                return ErrorResult("Could not read KTX2 source '" + path.string() + "'");
        }
        return ProbeBytes(data.data(), data.size(), options);
    }

    ImageLoadResult ImageLoader::Decode(const Path& path, const ImageLoadOptions& options)
    {
        if (options.MetadataOnly)
            return Probe(path, options);
        if (options.IsCancellationRequested())
            return CanceledResult();

        Vector<uint8_t> data;
        if (!ReadFile(path, data))
            return ErrorResult("Could not read image source '" + path.string() + "'");
        if (options.IsCancellationRequested())
            return CanceledResult();

        ImageLoadResult result = options.DecodePixels ? DecodeMemory(data.data(), data.size(), options)
                                                      : ProbeMemory(data.data(), data.size(), options);
        if (result && result.Info.Container == ImageContainerFormat::KTX2)
            result.SourceData = std::move(data);
        return result;
    }

    ImageLoadResult ImageLoader::ProbeMemory(const uint8_t* data, size_t size, const ImageLoadOptions& options)
    {
        return ProbeBytes(data, size, options);
    }

    ImageLoadResult ImageLoader::DecodeMemory(const uint8_t* data, size_t size, const ImageLoadOptions& options)
    {
        ImageLoadResult result = ProbeBytes(data, size, options);
        if (!result || options.MetadataOnly || !options.DecodePixels)
            return result;
        if (options.IsCancellationRequested())
            return CanceledResult();

        if (result.Info.Container == ImageContainerFormat::KTX2)
        {
            BasisTextureTranscodeResult transcode;
            String error;
            if (!BasisTextureCodec::Transcode(data, size, result.Info.PixelFormat, TextureFormat::RGBA8, 1, transcode, &error) ||
                transcode.Subresources.empty())
                return ErrorResult("KTX2 decode failed: " + error);
            result.Pixels = transcode.Subresources.front();
            result.Info.PixelFormat = TextureFormat::RGBA8;
            result.Info.Channels = 4;
            result.Info.BitDepth = 8;
            result.Info.IsFloat = false;
            result.Info.Orientation = options.FlipVertically ? ImageOrientation::BottomLeft : ImageOrientation::TopLeft;
            if (options.FlipVertically)
            {
                ImageInfo copyInfo = result.Info;
                Ref<PixelData> flipped = CopyPixels(result.Pixels->GetData(), copyInfo, true, [](uint8_t value) {
                    return static_cast<float>(value) / 255.0f;
                });
                result.Pixels = std::move(flipped);
            }
            return result;
        }

        Pnm16Info pnm;
        String pnmError;
        const Pnm16ParseStatus pnmStatus = ParsePnm16(data, size, pnm, &pnmError, true);
        if (pnmStatus == Pnm16ParseStatus::Invalid)
            return ErrorResult("16-bit PNM decode failed: " + pnmError);
        if (pnmStatus == Pnm16ParseStatus::Valid)
        {
            result.Pixels = DecodePnm16(data, pnm, result.Info, options.FlipVertically);
            if (!result.Pixels)
                return ErrorResult("Decoded 16-bit PNM image could not be stored in PixelData");
            result.Info.Orientation = options.FlipVertically ? ImageOrientation::BottomLeft : ImageOrientation::TopLeft;
            return result;
        }

        const int channels = static_cast<int>(result.Info.Channels);
        if (result.Info.IsHDR)
        {
            int width = 0;
            int height = 0;
            int sourceChannels = 0;
            float* raw = stbi_loadf_from_memory(data, static_cast<int>(size), &width, &height, &sourceChannels, channels);
            if (raw == nullptr)
            {
                const char* reason = stbi_failure_reason();
                return ErrorResult(String("HDR decode failed: ") + (reason != nullptr ? reason : "unknown stb error"));
            }
            result.Pixels = CopyPixels(raw, result.Info, options.FlipVertically, [](float value) { return value; });
            stbi_image_free(raw);
        }
        else if (result.Info.BitDepth == 16 && options.Preserve16Bit)
        {
            int width = 0;
            int height = 0;
            int sourceChannels = 0;
            uint16_t* raw = stbi_load_16_from_memory(data, static_cast<int>(size), &width, &height, &sourceChannels, channels);
            if (raw == nullptr)
            {
                const char* reason = stbi_failure_reason();
                return ErrorResult(String("16-bit raster decode failed: ") + (reason != nullptr ? reason : "unknown stb error"));
            }
            result.Pixels = CopyPixels(raw, result.Info, options.FlipVertically, [](uint16_t value) {
                return static_cast<float>(value) / 65535.0f;
            });
            stbi_image_free(raw);
        }
        else
        {
            result.Info.IsFloat = false;
            result.Info.PixelFormat = GetByteFormat(result.Info.Channels);
            int width = 0;
            int height = 0;
            int sourceChannels = 0;
            uint8_t* raw = stbi_load_from_memory(data, static_cast<int>(size), &width, &height, &sourceChannels, channels);
            if (raw == nullptr)
            {
                const char* reason = stbi_failure_reason();
                return ErrorResult(String("Raster decode failed: ") + (reason != nullptr ? reason : "unknown stb error"));
            }
            result.Pixels = CopyPixels(raw, result.Info, options.FlipVertically, [](uint8_t value) {
                return static_cast<float>(value) / 255.0f;
            });
            stbi_image_free(raw);
        }

        if (!result.Pixels)
            return ErrorResult("Decoded image could not be stored in PixelData");
        result.Info.Orientation = options.FlipVertically ? ImageOrientation::BottomLeft : ImageOrientation::TopLeft;
        return result;
    }

} // namespace Crowny
