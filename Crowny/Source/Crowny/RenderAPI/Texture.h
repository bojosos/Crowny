#pragma once

#include "Crowny/Assets/Asset.h"
#include "Crowny/RenderAPI/TextureView.h"
#include "Crowny/Utils/PixelUtils.h"

namespace Crowny
{

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
        uint32_t Width = 1, Height = 1, Depth = 1;
        TextureUsage Usage = TextureUsage::TEXTURE_STATIC;
        TextureFormat Format = TextureFormat::RGBA8;
    };

    class Texture : public Asset
    {
    public:
        virtual ~Texture() = default;

        virtual AssetType GetAssetType() const override { return AssetType::Texture; }
		static AssetType GetStaticType() { return AssetType::Texture; }

        uint32_t GetWidth() const { return m_Params.Width; }
        uint32_t GetHeight() const { return m_Params.Height; }
        uint32_t GetDepth() const { return m_Params.Depth; }
        TextureFormat GetFormat() const { return m_Params.Format; }

        virtual PixelData Lock(GpuLockOptions options, uint32_t mipLevel = 0, uint32_t face = 0,
                               uint32_t queueIdx = 0) = 0;
        virtual void Unlock() = 0;
        // virtual void Copy(const Ref<Texture>& target, )
        virtual void ReadData(PixelData& dest, uint32_t mipLevel = 0, uint32_t face = 0, uint32_t queueIdx = 0) = 0;
        virtual void WriteData(const PixelData& src, uint32_t mipLevel = 0, uint32_t face = 0,
                               uint32_t queueIdx = 0) = 0;

        const TextureParameters& GetProperties() const { return m_Params; }
        Ref<TextureView> CreateView(const TextureViewDesc& desc);
        Ref<TextureView> RequestView(uint32_t mip, uint32_t numMips, uint32_t firstFace, uint32_t numFaces,
                                     GpuViewUsage usage);

        Ref<PixelData> AllocatePixelData(uint32_t face, uint32_t mipLevel) const;

        CW_SERIALIZABLE(Texture);

    public:
        static Ref<Texture> Create(const TextureParameters& params);

    public:
        static Ref<Texture> WHITE;
        static Ref<Texture> BLACK;

    protected:
        UnorderedMap<TextureViewDesc, Ref<TextureView>, TextureView::HashFunction, TextureView::EqualFunction>
          m_TextureViews;
        Texture(const TextureParameters& params);
        TextureParameters m_Params;
    };

} // namespace Crowny