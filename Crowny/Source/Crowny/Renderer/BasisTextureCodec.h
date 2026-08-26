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
        uint32_t Layers = 1;
        uint32_t Faces = 1;
        uint32_t Components = 0;
        bool HasAlpha = false;
        bool SRGB = false;
        TextureDiskFormat DiskFormat = TextureDiskFormat::None;

        uint32_t GetSliceCount() const { return Layers * Faces; }
        uint32_t GetSliceIndex(uint32_t layer, uint32_t face) const { return layer * Faces + face; }
        TextureShape GetRuntimeShape() const { return Faces == 6 ? TextureShape::TEXTURE_CUBE : TextureShape::TEXTURE_2D; }
    };

    struct BasisTextureSource
    {
        uint32_t Layers = 1;
        uint32_t Faces = 1;
        uint32_t Levels = 1;
        // Mip-major, then layer-major, then face-major. Cubemap faces use
        // +X, -X, +Y, -Y, +Z, -Z order within each layer.
        Vector<Ref<PixelData>> Subresources;
    };

    struct BasisTextureSubresource
    {
        uint32_t MipLevel = 0;
        uint32_t Layer = 0;
        uint32_t Face = 0;
        Ref<PixelData> Pixels;
    };

    struct BasisTextureTranscodeResult
    {
        TextureFormat Format = TextureFormat::NONE;
        BasisTextureInfo Info;
        // Mip-major, then layer-major, then face-major. Cubemap faces use
        // +X, -X, +Y, -Y, +Z, -Z order within each layer.
        Vector<BasisTextureSubresource> Subresources;
    };

    class BasisTextureCodec
    {
    public:
        static bool Encode(const PixelData& source, TextureDiskFormat diskFormat, bool sRGB, bool generateMips,
                           Vector<uint8_t>& output, BasisTextureInfo* info = nullptr, String* error = nullptr);
        static bool Encode(const Vector<Ref<PixelData>>& mipChain, TextureDiskFormat diskFormat, bool sRGB,
                           Vector<uint8_t>& output, BasisTextureInfo* info = nullptr, String* error = nullptr);
        static bool Encode(const BasisTextureSource& source, TextureDiskFormat diskFormat, bool sRGB,
                           Vector<uint8_t>& output, BasisTextureInfo* info = nullptr, String* error = nullptr);
        static bool Inspect(const void* data, size_t size, BasisTextureInfo& info, String* error = nullptr);
        static TextureFormat SelectTarget(const BasisTextureInfo& info, TextureFormat sourceFormat,
                                          const RenderCapabilities& capabilities);
        static bool Transcode(const void* data, size_t size, TextureFormat sourceFormat, TextureFormat targetFormat,
                              uint32_t maximumLevels, BasisTextureTranscodeResult& output, String* error = nullptr);
    };
} // namespace Crowny
