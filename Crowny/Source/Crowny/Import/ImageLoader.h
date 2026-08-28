#pragma once

#include "Crowny/Common/StdHeaders.h"
#include "Crowny/Utils/PixelUtils.h"

#include <atomic>

namespace Crowny
{

    enum class ImageContainerFormat
    {
        Unknown,
        Raster,
        KTX2
    };

    enum class ImageFileFormat
    {
        Unknown,
        OtherRaster,
        PNG,
        JPEG,
        PSD,
        GIF,
        TGA,
        BMP,
        HDR,
        PIC,
        PNM,
        KTX2
    };

    enum class ImageOrientation
    {
        TopLeft,
        BottomLeft
    };

    enum class ImageChannelLayout
    {
        Unknown,
        Gray,
        GrayAlpha,
        RG,
        RGB,
        RGBA
    };

    struct ImageInfo
    {
        ImageContainerFormat Container = ImageContainerFormat::Unknown;
        ImageFileFormat FileFormat = ImageFileFormat::Unknown;
        ImageOrientation Orientation = ImageOrientation::TopLeft;
        uint32_t Width = 0;
        uint32_t Height = 0;
        uint32_t Depth = 1;
        uint32_t Layers = 1;
        uint32_t Faces = 1;
        uint32_t MipLevels = 1;
        uint32_t Channels = 0;
        uint32_t BitDepth = 0;
        ImageChannelLayout ChannelLayout = ImageChannelLayout::Unknown;
        TextureFormat PixelFormat = TextureFormat::NONE;
        TextureDiskFormat DiskFormat = TextureDiskFormat::None;
        bool HasAlpha = false;
        bool IsHDR = false;
        bool IsFloat = false;
        bool IsCompressed = false;
        bool SRGB = false;

        TextureShape GetRuntimeShape() const { return Faces == 6 ? TextureShape::TEXTURE_CUBE : TextureShape::TEXTURE_2D; }
    };

    struct ImageLoadOptions
    {
        bool MetadataOnly = false;
        bool DecodePixels = true;
        // Texture containers can remain encoded for deferred GPU transcoding.
        bool DecodeTextureContainers = true;
        bool FlipVertically = false;
        bool Preserve16Bit = true;
        uint32_t MaximumDimension = 32768;
        uint64_t MaximumSourceBytes = 512ull * 1024ull * 1024ull;
        uint64_t MaximumDecodedBytes = 1024ull * 1024ull * 1024ull;
        const std::atomic<bool>* Cancellation = nullptr;

        bool IsCancellationRequested() const { return Cancellation != nullptr && Cancellation->load(std::memory_order_acquire); }
    };

    enum class ImageLoadSource
    {
        Invalid,
        File,
        Memory
    };

    struct ImageLoadRequest
    {
        static ImageLoadRequest FromFile(Path path, ImageLoadOptions options = {})
        {
            ImageLoadRequest request;
            request.Source = ImageLoadSource::File;
            request.SourcePath = std::move(path);
            request.Options = options;
            return request;
        }

        // Memory remains owned by the caller and must stay valid until Load returns.
        static ImageLoadRequest FromMemory(const uint8_t* data, size_t size, ImageLoadOptions options = {})
        {
            ImageLoadRequest request;
            request.Source = ImageLoadSource::Memory;
            request.SourceData = data;
            request.SourceSize = size;
            request.Options = options;
            return request;
        }

        ImageLoadSource Source = ImageLoadSource::Invalid;
        Path SourcePath;
        const uint8_t* SourceData = nullptr;
        size_t SourceSize = 0;
        ImageLoadOptions Options;
    };

    enum class ImageLoadStatus
    {
        Failed,
        Succeeded,
        Canceled
    };

    enum class ImageLoadStage
    {
        Source,
        Probe,
        Decode
    };

    enum class ImageDiagnosticCode
    {
        InvalidRequest,
        Canceled,
        ReadFailed,
        EmptySource,
        SourceTooLarge,
        DimensionsTooLarge,
        DecodedImageTooLarge,
        UnsupportedFormat,
        InvalidData,
        DecodeFailed,
        AllocationFailed
    };

    struct ImageLoadDiagnostic
    {
        ImageDiagnosticCode Code = ImageDiagnosticCode::InvalidData;
        ImageLoadStage Stage = ImageLoadStage::Probe;
        String Message;
    };

    struct ImageSubresource
    {
        uint32_t MipLevel = 0;
        uint32_t Layer = 0;
        uint32_t Face = 0;
        Ref<PixelData> Pixels;
    };

    struct ImageLoadResult
    {
        ImageInfo Info;
        Vector<ImageSubresource> Subresources;
        // Compatibility alias for the first decoded subresource.
        Ref<PixelData> Pixels;
        Vector<uint8_t> SourceData;
        ImageLoadStatus Status = ImageLoadStatus::Failed;
        Vector<ImageLoadDiagnostic> Diagnostics;

        // Kept for existing callers. New code should inspect Status and Diagnostics.
        String Error;
        bool Canceled = false;

        explicit operator bool() const
        {
            return Status == ImageLoadStatus::Succeeded && Error.empty() && !Canceled && Info.Width != 0 && Info.Height != 0;
        }
    };

    class ImageLoader
    {
    public:
        static bool SupportsExtension(StringView extension);
        static bool SupportsSignature(const uint8_t* data, size_t size);

        static ImageLoadResult Load(const ImageLoadRequest& request);

        // Compatibility wrappers. Prefer Load for new callers.
        static ImageLoadResult Probe(const Path& path, const ImageLoadOptions& options = {});
        static ImageLoadResult Decode(const Path& path, const ImageLoadOptions& options = {});
        static ImageLoadResult ProbeMemory(const uint8_t* data, size_t size, const ImageLoadOptions& options = {});
        static ImageLoadResult DecodeMemory(const uint8_t* data, size_t size, const ImageLoadOptions& options = {});
    };

} // namespace Crowny
