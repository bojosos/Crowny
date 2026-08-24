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

    enum class ImageOrientation
    {
        TopLeft,
        BottomLeft
    };

    struct ImageInfo
    {
        ImageContainerFormat Container = ImageContainerFormat::Unknown;
        ImageOrientation Orientation = ImageOrientation::TopLeft;
        uint32_t Width = 0;
        uint32_t Height = 0;
        uint32_t Depth = 1;
        uint32_t Faces = 1;
        uint32_t MipLevels = 1;
        uint32_t Channels = 0;
        uint32_t BitDepth = 0;
        TextureFormat PixelFormat = TextureFormat::NONE;
        TextureDiskFormat DiskFormat = TextureDiskFormat::None;
        bool HasAlpha = false;
        bool IsHDR = false;
        bool IsFloat = false;
        bool IsCompressed = false;
        bool SRGB = false;
    };

    struct ImageLoadOptions
    {
        bool MetadataOnly = false;
        bool DecodePixels = true;
        bool FlipVertically = false;
        bool Preserve16Bit = true;
        const std::atomic<bool>* Cancellation = nullptr;

        bool IsCancellationRequested() const
        {
            return Cancellation != nullptr && Cancellation->load(std::memory_order_acquire);
        }
    };

    struct ImageLoadResult
    {
        ImageInfo Info;
        Ref<PixelData> Pixels;
        Vector<uint8_t> SourceData;
        String Error;
        bool Canceled = false;

        explicit operator bool() const { return Error.empty() && !Canceled && Info.Width != 0 && Info.Height != 0; }
    };

    class ImageLoader
    {
    public:
        static bool SupportsExtension(StringView extension);
        static bool SupportsSignature(const uint8_t* data, size_t size);

        static ImageLoadResult Probe(const Path& path, const ImageLoadOptions& options = {});
        static ImageLoadResult Decode(const Path& path, const ImageLoadOptions& options = {});
        static ImageLoadResult ProbeMemory(const uint8_t* data, size_t size, const ImageLoadOptions& options = {});
        static ImageLoadResult DecodeMemory(const uint8_t* data, size_t size, const ImageLoadOptions& options = {});
    };

} // namespace Crowny
