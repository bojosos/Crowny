#pragma once

namespace Crowny
{

    class Texture;

    enum GpuViewUsage
    {
        GVU_DEFAULT = 1 << 0,
        GVU_RENDERTARGET = 1 << 1,
        GVU_DEPTHSTENCIL = 1 << 2,
        GVI_RANDOMWRITE = 1 << 3
    };

    struct TextureViewDesc
    {
        uint32_t MostDetailedMip;
        uint32_t NumMips;
        uint32_t FirstFace;
        uint32_t NumFaces;
        GpuViewUsage Usage;
    };

    class TextureView
    {
    public:
        virtual ~TextureView() = default;
        uint32_t GetMostDetailedMip() const { return m_Desc.MostDetailedMip; }
        uint32_t GetNumMips() const { return m_Desc.NumMips; }
        uint32_t GetFirstFace() const { return m_Desc.FirstFace; }
        uint32_t GetNumFaces() const { return m_Desc.NumFaces; }
        GpuViewUsage GetUsage() const { return m_Desc.Usage; }

        struct HashFunction
        {
            size_t operator()(const TextureViewDesc& desc) const;
        };

        struct EqualFunction
        {
            bool operator()(const TextureViewDesc& lhs, const TextureViewDesc& rhs) const;
        };

        TextureView(const TextureViewDesc& desc);

    private:
        TextureViewDesc m_Desc;
    };

} // namespace Crowny