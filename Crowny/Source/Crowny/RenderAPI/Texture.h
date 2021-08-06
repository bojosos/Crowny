#pragma once

#include "Crowny/RenderAPI/TextureView.h"
#include "Crowny/Utils/PixelUtils.h"

namespace Crowny
{

    struct TextureSwizzle
    {
        SwizzleType Type;
        SwizzleChannel Swizzle[4];

        TextureSwizzle()
        {
            Type = SwizzleType::NONE;
            Swizzle[0] = SwizzleChannel::NONE;
        }

        TextureSwizzle(SwizzleType type, SwizzleChannel swizzle) : Type(type)
        {
            CW_ENGINE_ASSERT(type != SwizzleType::SWIZZLE_RGBA, "A SWIZZLE_RGBA requires 4 parameters!");
            Swizzle[0] = swizzle;
        }

        TextureSwizzle(SwizzleType type, SwizzleChannel ar[4]) : Type(type)
        {
            CW_ENGINE_ASSERT(type == SwizzleType::SWIZZLE_RGBA, "A four parameter TEXTURE_SWIZZLE has to be RGBA!");
            Swizzle[0] = ar[0];
            Swizzle[1] = ar[1];
            Swizzle[2] = ar[2];
            Swizzle[3] = ar[3];
        }
    };

    struct TextureParameters
    {
        TextureType Type = TextureType::TEXTURE_DEFAULT;
        TextureShape Shape = TextureShape::TEXTURE_2D;

        bool sRGB = true;
        bool ReadWrite = false;
        bool GenerateMipmaps = false;
        uint32_t MipLevels = 0;
        uint32_t Samples = 1;
        uint32_t Faces = 1;
        uint32_t NumArraySlices = 1;
        uint32_t Width = 1, Height = 1, Depth = 1;
        TextureUsage Usage = TextureUsage::TEXTURE_STATIC;
        TextureFormat Format = TextureFormat::RGBA8;
    };

    class Texture
    {
    public:
        virtual ~Texture() = default;

        virtual uint32_t GetWidth() const { return m_Params.Width; }
        virtual uint32_t GetHeight() const { return m_Params.Height; }

        virtual PixelData Lock(GpuLockOptions options, uint32_t mipLevel = 0, uint32_t face = 0,
                               uint32_t queueIdx = 0) = 0;
        virtual void Unlock() = 0;
        // virtual void Copy(const Ref<Texture>& target, )
        virtual void ReadData(PixelData& dest, uint32_t mipLevel = 0, uint32_t face = 0, uint32_t queueIdx = 0) = 0;
        virtual void WriteData(const PixelData& src, uint32_t mipLevel = 0, uint32_t face = 0,
                               uint32_t queueIdx = 0) = 0;

        const TextureParameters& GetProperties() const { return m_Params; }
        Ref<TextureView> CreateView(const TextureViewDesc& desc);
        Ref<TextureView> RequestView(uint32_t mip, uint32_t numMips, uint32_t firstArraySlice, uint32_t numArraySlices,
                                     GpuViewUsage usage);

    public:
        static Ref<Texture> Create(const TextureParameters& params);

    protected:
        std::unordered_map<TextureViewDesc, Ref<TextureView>, TextureView::HashFunction, TextureView::EqualFunction>
          m_TextureViews;
        Texture(const TextureParameters& params);
        TextureParameters m_Params;
    };

} // namespace Crowny