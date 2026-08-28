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

        enum class SourceReadStatus
        {
            Succeeded,
            Failed,
            TooLarge
        };

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
            return data != nullptr && size >= sizeof(KTX2_IDENTIFIER) && std::memcmp(data, KTX2_IDENTIFIER, sizeof(KTX2_IDENTIFIER)) == 0;
        }

        ImageFileFormat DetectFileFormat(const uint8_t* data, size_t size)
        {
            if (data == nullptr || size == 0)
                return ImageFileFormat::Unknown;
            if (IsKTX2(data, size))
                return ImageFileFormat::KTX2;
            if (size >= 8 && std::memcmp(data, "\x89PNG\r\n\x1A\n", 8) == 0)
                return ImageFileFormat::PNG;
            if (size >= 3 && data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF)
                return ImageFileFormat::JPEG;
            if (size >= 2 && data[0] == 'B' && data[1] == 'M')
                return ImageFileFormat::BMP;
            if (size >= 6 && (std::memcmp(data, "GIF87a", 6) == 0 || std::memcmp(data, "GIF89a", 6) == 0))
                return ImageFileFormat::GIF;
            if (size >= 4 && std::memcmp(data, "8BPS", 4) == 0)
                return ImageFileFormat::PSD;
            if (size >= 2 && data[0] == 'P' && data[1] >= '1' && data[1] <= '6')
                return ImageFileFormat::PNM;
            if (size >= 6 && (std::memcmp(data, "#?RGBE", 6) == 0 || std::memcmp(data, "#?RADI", 6) == 0))
                return ImageFileFormat::HDR;
            if (size >= 4 && data[0] == 0x53 && data[1] == 0x80 && data[2] == 0xF6 && data[3] == 0x34)
                return ImageFileFormat::PIC;
            if (size >= 18 && std::memcmp(data + size - 18, "TRUEVISION-XFILE.", 17) == 0)
                return ImageFileFormat::TGA;
            return ImageFileFormat::Unknown;
        }

        ImageFileFormat FormatFromExtension(StringView extension)
        {
            if (!extension.empty() && extension.front() == '.')
                extension.remove_prefix(1);
            String normalized(extension);
            StringUtils::ToLower(normalized);
            if (normalized == "png")
                return ImageFileFormat::PNG;
            if (normalized == "jpeg" || normalized == "jpg")
                return ImageFileFormat::JPEG;
            if (normalized == "psd")
                return ImageFileFormat::PSD;
            if (normalized == "gif")
                return ImageFileFormat::GIF;
            if (normalized == "tga")
                return ImageFileFormat::TGA;
            if (normalized == "bmp")
                return ImageFileFormat::BMP;
            if (normalized == "hdr")
                return ImageFileFormat::HDR;
            if (normalized == "pic")
                return ImageFileFormat::PIC;
            if (normalized == "ppm" || normalized == "pgm")
                return ImageFileFormat::PNM;
            if (normalized == "ktx2")
                return ImageFileFormat::KTX2;
            return ImageFileFormat::Unknown;
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

        Pnm16ParseStatus ParsePnm16(const uint8_t* data, size_t size, Pnm16Info& info, String* error = nullptr, bool requireCompleteRaster = false)
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
            case 1:
                return TextureFormat::R8;
            case 2:
                return TextureFormat::RG8;
            case 3:
                return TextureFormat::RGB8;
            case 4:
                return TextureFormat::RGBA8;
            default:
                return TextureFormat::NONE;
            }
        }

        TextureFormat GetFloatFormat(uint32_t channels)
        {
            switch (channels)
            {
            case 1:
                return TextureFormat::R32F;
            case 2:
                return TextureFormat::RG32F;
            case 3:
                return TextureFormat::RGB32F;
            case 4:
                return TextureFormat::RGBA32F;
            default:
                return TextureFormat::NONE;
            }
        }

        TextureFormat Get16BitFormat(uint32_t channels)
        {
            switch (channels)
            {
            case 1:
                return TextureFormat::R16;
            case 2:
                return TextureFormat::RG16;
            case 3:
                // Three-component 16-bit formats are not guaranteed to support
                // sampled images on Vulkan. Preserve the source channel layout
                // in ImageInfo, but use portable RGBA16 storage.
                return TextureFormat::RGBA16;
            case 4:
                return TextureFormat::RGBA16;
            default:
                return TextureFormat::NONE;
            }
        }

        ImageChannelLayout GetChannelLayout(uint32_t channels, bool grayAlpha = true)
        {
            switch (channels)
            {
            case 1:
                return ImageChannelLayout::Gray;
            case 2:
                return grayAlpha ? ImageChannelLayout::GrayAlpha : ImageChannelLayout::RG;
            case 3:
                return ImageChannelLayout::RGB;
            case 4:
                return ImageChannelLayout::RGBA;
            default:
                return ImageChannelLayout::Unknown;
            }
        }

        TextureFormat GetBasisFormat(const BasisTextureInfo& info)
        {
            const TextureFormat format = GetByteFormat(info.Components);
            return PixelUtils::IsValidFormat(format) ? format : (info.HasAlpha ? TextureFormat::RGBA8 : TextureFormat::RGB8);
        }

        ImageLoadResult SuccessResult()
        {
            ImageLoadResult result;
            result.Status = ImageLoadStatus::Succeeded;
            return result;
        }

        ImageLoadResult CanceledResult(ImageLoadStage stage)
        {
            ImageLoadResult result;
            result.Status = ImageLoadStatus::Canceled;
            result.Canceled = true;
            result.Diagnostics.push_back({ ImageDiagnosticCode::Canceled, stage, "Image load was canceled" });
            return result;
        }

        ImageLoadResult ErrorResult(ImageDiagnosticCode code, ImageLoadStage stage, String error)
        {
            ImageLoadResult result;
            result.Status = ImageLoadStatus::Failed;
            result.Error = std::move(error);
            result.Diagnostics.push_back({ code, stage, result.Error });
            return result;
        }

        bool CheckedMultiply(uint64_t value, uint64_t factor, uint64_t& output)
        {
            if (factor != 0 && value > std::numeric_limits<uint64_t>::max() / factor)
                return false;
            output = value * factor;
            return true;
        }

        bool CheckedAdd(uint64_t value, uint64_t addend, uint64_t& output)
        {
            if (value > std::numeric_limits<uint64_t>::max() - addend)
                return false;
            output = value + addend;
            return true;
        }

        bool CalculateDecodedBytes(const ImageInfo& info, uint64_t bytesPerPixel, uint64_t& output)
        {
            if (bytesPerPixel == 0 || info.Width == 0 || info.Height == 0 || info.Depth == 0 || info.Layers == 0 || info.Faces == 0)
                return false;

            uint64_t bytesPerSlice = 0;
            uint32_t width = info.Width;
            uint32_t height = info.Height;
            uint32_t depth = info.Depth;
            const uint32_t mipLevels = std::max(info.MipLevels, 1u);
            for (uint32_t mipLevel = 0; mipLevel < mipLevels; mipLevel++)
            {
                uint64_t mipBytes = bytesPerPixel;
                if (!CheckedMultiply(mipBytes, width, mipBytes) || !CheckedMultiply(mipBytes, height, mipBytes) ||
                    !CheckedMultiply(mipBytes, depth, mipBytes) || !CheckedAdd(bytesPerSlice, mipBytes, bytesPerSlice))
                    return false;

                width = std::max(width >> 1u, 1u);
                height = std::max(height >> 1u, 1u);
                depth = std::max(depth >> 1u, 1u);
            }

            output = bytesPerSlice;
            return CheckedMultiply(output, info.Layers, output) && CheckedMultiply(output, info.Faces, output);
        }

        bool ValidateImageLimits(const ImageInfo& info, const ImageLoadOptions& options, ImageDiagnosticCode& code, String& message)
        {
            if (options.MaximumDimension != 0 &&
                (info.Width > options.MaximumDimension || info.Height > options.MaximumDimension || info.Depth > options.MaximumDimension))
            {
                code = ImageDiagnosticCode::DimensionsTooLarge;
                message = "Image dimensions exceed the configured decode limit";
                return false;
            }

            const bool decodesPixels = !options.MetadataOnly && options.DecodePixels &&
                                       (info.Container != ImageContainerFormat::KTX2 || options.DecodeTextureContainers);
            if (!decodesPixels || options.MaximumDecodedBytes == 0)
                return true;

            uint64_t decodedBytes = 0;
            const uint64_t bytesPerPixel = info.Container == ImageContainerFormat::KTX2
                                             ? PixelUtils::GetNumBytes(TextureFormat::RGBA8)
                                             : static_cast<uint64_t>(PixelUtils::GetNumBytes(info.PixelFormat));
            if (!CalculateDecodedBytes(info, bytesPerPixel, decodedBytes) || decodedBytes > options.MaximumDecodedBytes)
            {
                code = ImageDiagnosticCode::DecodedImageTooLarge;
                message = "Decoded image exceeds the configured memory limit";
                return false;
            }
            return true;
        }

        ImageLoadResult ApplyImageLimits(ImageLoadResult result, const ImageLoadOptions& options)
        {
            ImageDiagnosticCode code = ImageDiagnosticCode::DecodedImageTooLarge;
            String message;
            if (!ValidateImageLimits(result.Info, options, code, message))
                return ErrorResult(code, ImageLoadStage::Probe, std::move(message));
            return result;
        }

        SourceReadStatus ReadFile(const Path& path, Vector<uint8_t>& data, uint64_t& sourceSize,
                                  size_t maximumBytes = std::numeric_limits<size_t>::max(), uint64_t maximumSourceBytes = 0)
        {
            const Ref<DataStream> stream = FileSystem::OpenFile(path);
            if (stream == nullptr || !stream->IsReadable() || stream->Size() == 0)
                return SourceReadStatus::Failed;

            sourceSize = stream->Size();
            if (maximumSourceBytes != 0 && sourceSize > maximumSourceBytes)
            {
                stream->Close();
                return SourceReadStatus::TooLarge;
            }

            const size_t size = static_cast<size_t>(std::min<uint64_t>(sourceSize, maximumBytes));
            data.resize(size);
            const bool read = stream->Read(data.data(), size) == size;
            stream->Close();
            return read ? SourceReadStatus::Succeeded : SourceReadStatus::Failed;
        }

        void FillBasisInfo(const BasisTextureInfo& basis, ImageInfo& info)
        {
            info.Container = ImageContainerFormat::KTX2;
            info.FileFormat = ImageFileFormat::KTX2;
            info.Width = basis.Width;
            info.Height = basis.Height;
            info.Depth = 1;
            info.Layers = basis.Layers;
            info.Faces = basis.Faces;
            info.MipLevels = basis.Levels;
            info.Channels = basis.Components;
            info.BitDepth = 8;
            info.ChannelLayout = GetChannelLayout(info.Channels, false);
            info.PixelFormat = GetBasisFormat(basis);
            info.DiskFormat = basis.DiskFormat;
            info.HasAlpha = basis.HasAlpha;
            info.IsCompressed = true;
            info.SRGB = basis.SRGB;
        }

        ImageLoadResult ProbeBytes(const uint8_t* data, size_t size, const ImageLoadOptions& options)
        {
            if (options.IsCancellationRequested())
                return CanceledResult(ImageLoadStage::Probe);
            if (data == nullptr || size == 0)
                return ErrorResult(ImageDiagnosticCode::EmptySource, ImageLoadStage::Source, "Image source is empty");
            if (options.MaximumSourceBytes != 0 && size > options.MaximumSourceBytes)
                return ErrorResult(ImageDiagnosticCode::SourceTooLarge, ImageLoadStage::Source,
                                   "Image source exceeds the configured size limit");
            if (size > static_cast<size_t>(std::numeric_limits<int>::max()))
                return ErrorResult(ImageDiagnosticCode::SourceTooLarge, ImageLoadStage::Source, "Image source is too large for the raster decoder");

            if (IsKTX2(data, size))
            {
                BasisTextureInfo basis;
                String error;
                if (!BasisTextureCodec::Inspect(data, size, basis, &error))
                    return ErrorResult(ImageDiagnosticCode::InvalidData, ImageLoadStage::Probe, "KTX2 probe failed: " + error);
                if (options.IsCancellationRequested())
                    return CanceledResult(ImageLoadStage::Probe);

                ImageLoadResult result = SuccessResult();
                FillBasisInfo(basis, result.Info);
                return ApplyImageLimits(std::move(result), options);
            }

            Pnm16Info pnm;
            String pnmError;
            const Pnm16ParseStatus pnmStatus = ParsePnm16(data, size, pnm, &pnmError);
            if (pnmStatus == Pnm16ParseStatus::Invalid)
                return ErrorResult(ImageDiagnosticCode::InvalidData, ImageLoadStage::Probe, "16-bit PNM probe failed: " + pnmError);
            if (pnmStatus == Pnm16ParseStatus::Valid)
            {
                ImageLoadResult result = SuccessResult();
                result.Info.Container = ImageContainerFormat::Raster;
                result.Info.FileFormat = ImageFileFormat::PNM;
                result.Info.Width = pnm.Width;
                result.Info.Height = pnm.Height;
                result.Info.Channels = pnm.Channels;
                result.Info.BitDepth = 16;
                result.Info.ChannelLayout = GetChannelLayout(pnm.Channels);
                result.Info.IsFloat = false;
                result.Info.PixelFormat = options.Preserve16Bit ? Get16BitFormat(pnm.Channels) : GetByteFormat(pnm.Channels);
                return ApplyImageLimits(std::move(result), options);
            }

            int width = 0;
            int height = 0;
            int channels = 0;
            if (stbi_info_from_memory(data, static_cast<int>(size), &width, &height, &channels) == 0 || width <= 0 || height <= 0)
            {
                const char* reason = stbi_failure_reason();
                return ErrorResult(ImageDiagnosticCode::UnsupportedFormat, ImageLoadStage::Probe,
                                   String("Raster image probe failed: ") + (reason != nullptr ? reason : "unsupported or corrupt data"));
            }
            if (options.IsCancellationRequested())
                return CanceledResult(ImageLoadStage::Probe);

            ImageLoadResult result = SuccessResult();
            result.Info.Container = ImageContainerFormat::Raster;
            result.Info.FileFormat = DetectFileFormat(data, size);
            if (result.Info.FileFormat == ImageFileFormat::Unknown)
                result.Info.FileFormat = ImageFileFormat::OtherRaster;
            result.Info.Width = static_cast<uint32_t>(width);
            result.Info.Height = static_cast<uint32_t>(height);
            result.Info.Channels = static_cast<uint32_t>(channels);
            result.Info.ChannelLayout = GetChannelLayout(result.Info.Channels);
            result.Info.HasAlpha = channels == 2 || channels == 4;
            result.Info.IsHDR = stbi_is_hdr_from_memory(data, static_cast<int>(size)) != 0;
            const bool is16Bit = !result.Info.IsHDR && stbi_is_16_bit_from_memory(data, static_cast<int>(size)) != 0;
            result.Info.BitDepth = result.Info.IsHDR ? 32u : (is16Bit ? 16u : 8u);
            result.Info.IsFloat = result.Info.IsHDR;
            result.Info.PixelFormat = result.Info.IsHDR
                                        ? GetFloatFormat(result.Info.Channels)
                                        : (is16Bit && options.Preserve16Bit ? Get16BitFormat(result.Info.Channels)
                                                                           : GetByteFormat(result.Info.Channels));
            if (!PixelUtils::IsValidFormat(result.Info.PixelFormat))
                return ErrorResult(ImageDiagnosticCode::UnsupportedFormat, ImageLoadStage::Probe, "Raster image has an unsupported channel layout");
            return ApplyImageLimits(std::move(result), options);
        }

        template <typename T>
        Ref<PixelData> CopyNativePixels(const T* source, const ImageInfo& info, bool flipVertically, const ImageLoadOptions& options)
        {
            if (source == nullptr)
                return nullptr;
            Ref<PixelData> pixels = PixelData::Create(info.Width, info.Height, 1, info.PixelFormat);
            if (!pixels || !pixels->IsValid())
                return nullptr;

            const uint32_t storageChannels = PixelUtils::GetComponentCount(info.PixelFormat);
            const uint64_t rowBytes64 = static_cast<uint64_t>(info.Width) * storageChannels * sizeof(T);
            if (rowBytes64 != pixels->GetRowPitch())
                return nullptr;
            const size_t rowBytes = static_cast<size_t>(rowBytes64);
            for (uint32_t y = 0; y < info.Height; y++)
            {
                if (options.IsCancellationRequested())
                    return nullptr;
                const uint32_t sourceY = flipVertically ? info.Height - y - 1u : y;
                const T* sourceRow = source + static_cast<size_t>(sourceY) * info.Width * storageChannels;
                uint8_t* destination = pixels->GetData() + static_cast<size_t>(y) * pixels->GetRowPitch();
                std::memcpy(destination, sourceRow, rowBytes);
            }
            return pixels;
        }

        Ref<PixelData> DecodePnm16(const uint8_t* data, const Pnm16Info& pnm, const ImageInfo& info, bool flipVertically,
                                   const ImageLoadOptions& options)
        {
            Ref<PixelData> pixels = PixelData::Create(info.Width, info.Height, 1, info.PixelFormat);
            if (!pixels || !pixels->IsValid())
                return nullptr;

            const float inverseMaxValue = 1.0f / static_cast<float>(pnm.MaxValue);
            const uint32_t pixelBytes = PixelUtils::GetNumBytes(info.PixelFormat);
            for (uint32_t y = 0; y < info.Height; y++)
            {
                if (options.IsCancellationRequested())
                    return nullptr;
                const uint32_t sourceY = flipVertically ? info.Height - y - 1u : y;
                uint8_t* destination = pixels->GetData() + static_cast<size_t>(y) * pixels->GetRowPitch();
                for (uint32_t x = 0; x < info.Width; x++)
                {
                    glm::vec4 color(0.0f, 0.0f, 0.0f, 1.0f);
                    const uint64_t firstSample = (static_cast<uint64_t>(sourceY) * info.Width + x) * pnm.Channels;
                    for (uint32_t channel = 0; channel < pnm.Channels; channel++)
                    {
                        const size_t offset = pnm.DataOffset + static_cast<size_t>(firstSample + channel) * sizeof(uint16_t);
                        const uint16_t sample =
                          static_cast<uint16_t>((static_cast<uint16_t>(data[offset]) << 8u) | static_cast<uint16_t>(data[offset + 1u]));
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
        if (DetectFileFormat(data, size) != ImageFileFormat::Unknown)
            return true;
        if (size > static_cast<size_t>(std::numeric_limits<int>::max()))
            return false;
        int width = 0;
        int height = 0;
        int channels = 0;
        return stbi_info_from_memory(data, static_cast<int>(size), &width, &height, &channels) != 0;
    }

    namespace
    {
        ImageLoadResult DecodeBytes(const uint8_t* data, size_t size, const ImageLoadOptions& options)
        {
            ImageLoadResult result = ProbeBytes(data, size, options);
            if (!result || options.MetadataOnly || !options.DecodePixels)
                return result;
            if (options.IsCancellationRequested())
                return CanceledResult(ImageLoadStage::Decode);

            if (result.Info.Container == ImageContainerFormat::KTX2)
            {
                if (!options.DecodeTextureContainers)
                    return result;

                BasisTextureTranscodeResult transcode;
                String error;
                const bool decoded = BasisTextureCodec::Transcode(data, size, result.Info.PixelFormat, TextureFormat::RGBA8, 0, transcode, &error);
                if (options.IsCancellationRequested())
                    return CanceledResult(ImageLoadStage::Decode);
                if (!decoded || transcode.Subresources.empty())
                    return ErrorResult(ImageDiagnosticCode::DecodeFailed, ImageLoadStage::Decode, "KTX2 decode failed: " + error);
                result.Info.PixelFormat = TextureFormat::RGBA8;
                result.Info.Width = transcode.Info.Width;
                result.Info.Height = transcode.Info.Height;
                result.Info.Depth = 1;
                result.Info.Layers = transcode.Info.Layers;
                result.Info.Faces = transcode.Info.Faces;
                result.Info.MipLevels = transcode.Info.Levels;
                result.Info.BitDepth = 8;
                result.Info.IsFloat = false;
                result.Info.IsCompressed = false;
                result.Info.Orientation = options.FlipVertically ? ImageOrientation::BottomLeft : ImageOrientation::TopLeft;
                result.Subresources.reserve(transcode.Subresources.size());
                for (BasisTextureSubresource& subresource : transcode.Subresources)
                {
                    Ref<PixelData> pixels = std::move(subresource.Pixels);
                    if (options.FlipVertically)
                    {
                        ImageInfo copyInfo = result.Info;
                        copyInfo.Width = pixels->GetWidth();
                        copyInfo.Height = pixels->GetHeight();
                        pixels = CopyNativePixels(pixels->GetData(), copyInfo, true, options);
                    }
                    if (!pixels)
                    {
                        if (options.IsCancellationRequested())
                            return CanceledResult(ImageLoadStage::Decode);
                        return ErrorResult(ImageDiagnosticCode::AllocationFailed, ImageLoadStage::Decode,
                                           "Decoded KTX2 image could not be stored in PixelData");
                    }
                    result.Subresources.push_back(
                      { subresource.MipLevel, subresource.Layer, subresource.Face, std::move(pixels) });
                }
                result.Pixels = result.Subresources.front().Pixels;
                return result;
            }

            Pnm16Info pnm;
            String pnmError;
            const Pnm16ParseStatus pnmStatus = ParsePnm16(data, size, pnm, &pnmError, true);
            if (pnmStatus == Pnm16ParseStatus::Invalid)
                return ErrorResult(ImageDiagnosticCode::InvalidData, ImageLoadStage::Decode, "16-bit PNM decode failed: " + pnmError);
            if (pnmStatus == Pnm16ParseStatus::Valid)
            {
                result.Pixels = DecodePnm16(data, pnm, result.Info, options.FlipVertically, options);
                if (!result.Pixels)
                {
                    if (options.IsCancellationRequested())
                        return CanceledResult(ImageLoadStage::Decode);
                    return ErrorResult(ImageDiagnosticCode::AllocationFailed, ImageLoadStage::Decode,
                                       "Decoded 16-bit PNM image could not be stored in PixelData");
                }
                result.Info.Orientation = options.FlipVertically ? ImageOrientation::BottomLeft : ImageOrientation::TopLeft;
                result.Subresources.push_back({ 0, 0, 0, result.Pixels });
                return result;
            }

            const int channels = static_cast<int>(PixelUtils::GetComponentCount(result.Info.PixelFormat));
            if (result.Info.IsHDR)
            {
                int width = 0;
                int height = 0;
                int sourceChannels = 0;
                float* raw = stbi_loadf_from_memory(data, static_cast<int>(size), &width, &height, &sourceChannels, channels);
                if (options.IsCancellationRequested())
                {
                    stbi_image_free(raw);
                    return CanceledResult(ImageLoadStage::Decode);
                }
                if (raw == nullptr)
                {
                    const char* reason = stbi_failure_reason();
                    return ErrorResult(ImageDiagnosticCode::DecodeFailed, ImageLoadStage::Decode,
                                       String("HDR decode failed: ") + (reason != nullptr ? reason : "unknown stb error"));
                }
                if (width == static_cast<int>(result.Info.Width) && height == static_cast<int>(result.Info.Height))
                    result.Pixels = CopyNativePixels(raw, result.Info, options.FlipVertically, options);
                stbi_image_free(raw);
            }
            else if (result.Info.BitDepth == 16 && options.Preserve16Bit)
            {
                int width = 0;
                int height = 0;
                int sourceChannels = 0;
                uint16_t* raw = stbi_load_16_from_memory(data, static_cast<int>(size), &width, &height, &sourceChannels, channels);
                if (options.IsCancellationRequested())
                {
                    stbi_image_free(raw);
                    return CanceledResult(ImageLoadStage::Decode);
                }
                if (raw == nullptr)
                {
                    const char* reason = stbi_failure_reason();
                    return ErrorResult(ImageDiagnosticCode::DecodeFailed, ImageLoadStage::Decode,
                                       String("16-bit raster decode failed: ") + (reason != nullptr ? reason : "unknown stb error"));
                }
                if (width == static_cast<int>(result.Info.Width) && height == static_cast<int>(result.Info.Height))
                    result.Pixels = CopyNativePixels(raw, result.Info, options.FlipVertically, options);
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
                if (options.IsCancellationRequested())
                {
                    stbi_image_free(raw);
                    return CanceledResult(ImageLoadStage::Decode);
                }
                if (raw == nullptr)
                {
                    const char* reason = stbi_failure_reason();
                    return ErrorResult(ImageDiagnosticCode::DecodeFailed, ImageLoadStage::Decode,
                                       String("Raster decode failed: ") + (reason != nullptr ? reason : "unknown stb error"));
                }
                if (width == static_cast<int>(result.Info.Width) && height == static_cast<int>(result.Info.Height))
                    result.Pixels = CopyNativePixels(raw, result.Info, options.FlipVertically, options);
                stbi_image_free(raw);
            }

            if (!result.Pixels)
            {
                if (options.IsCancellationRequested())
                    return CanceledResult(ImageLoadStage::Decode);
                return ErrorResult(ImageDiagnosticCode::AllocationFailed, ImageLoadStage::Decode, "Decoded image could not be stored in PixelData");
            }
            result.Info.Orientation = options.FlipVertically ? ImageOrientation::BottomLeft : ImageOrientation::TopLeft;
            result.Subresources.push_back({ 0, 0, 0, result.Pixels });
            return result;
        }
    } // namespace

    ImageLoadResult ImageLoader::Load(const ImageLoadRequest& request)
    {
        const ImageLoadOptions& options = request.Options;
        if (options.IsCancellationRequested())
            return CanceledResult(ImageLoadStage::Source);

        if (request.Source == ImageLoadSource::Memory)
        {
            if (request.SourceData == nullptr || request.SourceSize == 0)
                return ErrorResult(ImageDiagnosticCode::EmptySource, ImageLoadStage::Source, "Image source is empty");
            if (options.MaximumSourceBytes != 0 && request.SourceSize > options.MaximumSourceBytes)
                return ErrorResult(ImageDiagnosticCode::SourceTooLarge, ImageLoadStage::Source,
                                   "Image source exceeds the configured size limit");
            ImageLoadResult result = options.MetadataOnly || !options.DecodePixels ? ProbeBytes(request.SourceData, request.SourceSize, options)
                                                                                    : DecodeBytes(request.SourceData, request.SourceSize, options);
            if (result && !options.MetadataOnly && result.Info.Container == ImageContainerFormat::KTX2)
                result.SourceData.assign(request.SourceData, request.SourceData + request.SourceSize);
            return result;
        }

        if (request.Source != ImageLoadSource::File || request.SourcePath.empty())
            return ErrorResult(ImageDiagnosticCode::InvalidRequest, ImageLoadStage::Source,
                               "Image load request does not contain a file or memory source");

        Vector<uint8_t> data;
        uint64_t sourceSize = 0;
        const size_t maximumBytes = options.MetadataOnly ? MAX_RASTER_PROBE_BYTES : std::numeric_limits<size_t>::max();
        const SourceReadStatus readStatus = ReadFile(request.SourcePath, data, sourceSize, maximumBytes, options.MaximumSourceBytes);
        if (readStatus == SourceReadStatus::TooLarge)
            return ErrorResult(ImageDiagnosticCode::SourceTooLarge, ImageLoadStage::Source,
                               "Image source exceeds the configured size limit");
        if (readStatus != SourceReadStatus::Succeeded)
            return ErrorResult(ImageDiagnosticCode::ReadFailed, ImageLoadStage::Source,
                               "Could not read image source '" + request.SourcePath.string() + "'");
        if (options.IsCancellationRequested())
            return CanceledResult(ImageLoadStage::Source);

        if (options.MetadataOnly && IsKTX2(data.data(), data.size()))
        {
            if (sourceSize > data.size() &&
                ReadFile(request.SourcePath, data, sourceSize, std::numeric_limits<size_t>::max(), options.MaximumSourceBytes) !=
                  SourceReadStatus::Succeeded)
                return ErrorResult(ImageDiagnosticCode::ReadFailed, ImageLoadStage::Source,
                                   "Could not read KTX2 source '" + request.SourcePath.string() + "'");
            if (options.IsCancellationRequested())
                return CanceledResult(ImageLoadStage::Source);
        }

        ImageLoadResult result = options.MetadataOnly || !options.DecodePixels ? ProbeBytes(data.data(), data.size(), options)
                                                                               : DecodeBytes(data.data(), data.size(), options);
        if (result && result.Info.FileFormat == ImageFileFormat::OtherRaster &&
            FormatFromExtension(request.SourcePath.extension().string()) == ImageFileFormat::TGA)
            result.Info.FileFormat = ImageFileFormat::TGA;
        if (result && !options.MetadataOnly && result.Info.Container == ImageContainerFormat::KTX2)
            result.SourceData = std::move(data);
        return result;
    }

    ImageLoadResult ImageLoader::Probe(const Path& path, const ImageLoadOptions& options)
    {
        ImageLoadOptions probeOptions = options;
        probeOptions.MetadataOnly = true;
        probeOptions.DecodePixels = false;
        return Load(ImageLoadRequest::FromFile(path, probeOptions));
    }

    ImageLoadResult ImageLoader::Decode(const Path& path, const ImageLoadOptions& options) { return Load(ImageLoadRequest::FromFile(path, options)); }

    ImageLoadResult ImageLoader::ProbeMemory(const uint8_t* data, size_t size, const ImageLoadOptions& options)
    {
        ImageLoadOptions probeOptions = options;
        probeOptions.MetadataOnly = true;
        probeOptions.DecodePixels = false;
        return Load(ImageLoadRequest::FromMemory(data, size, probeOptions));
    }

    ImageLoadResult ImageLoader::DecodeMemory(const uint8_t* data, size_t size, const ImageLoadOptions& options)
    {
        return Load(ImageLoadRequest::FromMemory(data, size, options));
    }

} // namespace Crowny
