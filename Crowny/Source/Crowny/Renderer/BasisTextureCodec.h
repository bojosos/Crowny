#pragma once

#include "Crowny/RenderAPI/RenderCapabilities.h"
#include "Crowny/Utils/PixelUtils.h"

namespace Crowny
{
    struct BasisTextureInfo
    {
        uint32_t Width = 0;
        uint32_t Height = 0;
        uint32_t Levels = 0;
        uint32_t Faces = 0;
        uint32_t Components = 0;
        bool HasAlpha = false;
        bool SRGB = false;
        TextureDiskFormat DiskFormat = TextureDiskFormat::None;
    };

    struct BasisTextureTranscodeResult
    {
        TextureFormat Format = TextureFormat::NONE;
        BasisTextureInfo Info;
        // Mip-major, then face-major. Index = mip * Info.Faces + face.
        Vector<Ref<PixelData>> Subresources;
    };

    class BasisTextureCodec
    {
    public:
        static bool Encode(const PixelData& source, TextureDiskFormat diskFormat, bool sRGB, bool generateMips,
                           Vector<uint8_t>& output, BasisTextureInfo* info = nullptr, String* error = nullptr);
        static bool Encode(const Vector<Ref<PixelData>>& mipChain, TextureDiskFormat diskFormat, bool sRGB,
                           Vector<uint8_t>& output, BasisTextureInfo* info = nullptr, String* error = nullptr);
        static bool Inspect(const void* data, size_t size, BasisTextureInfo& info, String* error = nullptr);
        static TextureFormat SelectTarget(const BasisTextureInfo& info, TextureFormat sourceFormat,
                                          const RenderCapabilities& capabilities);
        static bool Transcode(const void* data, size_t size, TextureFormat sourceFormat, TextureFormat targetFormat,
                              uint32_t maximumLevels, BasisTextureTranscodeResult& output, String* error = nullptr);
    };
} // namespace Crowny
