#include "cwpch.h"

#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Renderer/Renderer.h"
#include "Crowny/Renderer/BasisTextureCodec.h"

#include "Platform/OpenGL/OpenGLTexture.h"
#include "Platform/Vulkan/VulkanTexture.h"

namespace Crowny
{

    const TextureSurface TextureSurface::COMPLETE = TextureSurface(0, 0, 0, 0);

    Ref<TextureView> Texture::CreateView(const TextureViewDesc& desc) { return CreateRef<TextureView>(TextureViewDesc(desc)); }

    Ref<TextureView> Texture::RequestView(uint32_t mip, uint32_t numMips, uint32_t firstFace, uint32_t numFaces, GpuViewUsage usage)
    {
        const TextureDesc& props = GetDesc();
        mip = std::min(mip, props.MipLevels);
        firstFace = std::min(firstFace, std::max(props.Faces, 1u) - 1u);
        const uint32_t remainingMips = props.MipLevels + 1u - mip;
        const uint32_t remainingFaces = std::max(props.Faces, 1u) - firstFace;
        TextureViewDesc desc;
        desc.MostDetailedMip = mip;
        desc.NumMips = numMips == 0 ? remainingMips : std::min(numMips, remainingMips);
        desc.FirstFace = firstFace;
        desc.NumFaces = numFaces == 0 ? remainingFaces : std::min(numFaces, remainingFaces);
        desc.Usage = usage;

        auto iter = m_TextureViews.find(desc);
        if (iter == m_TextureViews.end())
        {
            m_TextureViews[desc] = CreateView(desc);
            iter = m_TextureViews.find(desc);
        }

        return iter->second;
    }

    Ref<Texture> Texture::WHITE;
    Ref<Texture> Texture::BLACK;
    Ref<Texture> Texture::NORMAL;
    Ref<Texture> Texture::MISSING;

    Texture::Texture(const TextureDesc& params) : m_Desc(params) {}
    Texture::Texture(const TextureDesc& params, bool deferred) : m_Desc(params) {}

    Ref<Texture> Texture::Create(const TextureDesc& params)
    {
        switch (RenderAPI::TryGet()->GetAPI())
        {
        case RenderAPI::API::OpenGL:
            return Ref<Texture>(new OpenGLTexture(params));
        case RenderAPI::API::Vulkan:
            return Ref<Texture>(new VulkanTexture(params));
        default:
            CW_ENGINE_ASSERT(false, "Renderer API not supported");
            return nullptr;
        }

        return nullptr;
    }

    Ref<Texture> Texture::CreateDeferred(const TextureDesc& params, const Ref<PixelData>& pixelData)
    {
        switch (RenderAPI::TryGet()->GetAPI())
        {
        case RenderAPI::API::OpenGL: {
            auto tex = Ref<Texture>(new OpenGLTexture(params, true));
            if (pixelData)
                tex->m_PendingSubresources.push_back({ 0, 0, pixelData });
            return tex;
        }
        case RenderAPI::API::Vulkan: {
            auto tex = Ref<Texture>(new VulkanTexture(params, true));
            if (pixelData)
                tex->m_PendingSubresources.push_back({ 0, 0, pixelData });
            return tex;
        }
        default:
            CW_ENGINE_ASSERT(false, "Renderer API not supported");
            return nullptr;
        }
    }

    Ref<PixelData> Texture::AllocatePixelData(uint32_t face, uint32_t mipLevel) const
    {
        CW_ENGINE_ASSERT(face < m_Desc.Faces, "Texture face is out of range");
        CW_ENGINE_ASSERT(mipLevel <= m_Desc.MipLevels, "Texture mip level is out of range");
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t depth = 0;
        PixelUtils::GetMipSizeForLevel(GetWidth(), GetHeight(), GetDepth(), mipLevel, width, height, depth);
        return PixelData::Create(width, height, depth, GetFormat());
    }

    void Texture::SetEncodedSourceData(TextureDiskFormat diskFormat, TextureFormat sourceFormat, Vector<uint8_t> data)
    {
        CW_ENGINE_ASSERT(diskFormat == TextureDiskFormat::ETC1S || diskFormat == TextureDiskFormat::UASTC,
                         "Encoded textures require a Basis disk format");
        CW_ENGINE_ASSERT(PixelUtils::IsValidFormat(sourceFormat) && !PixelUtils::IsCompressedFormat(sourceFormat),
                         "Encoded textures require a valid uncompressed source format");
        CW_ENGINE_ASSERT(!data.empty(), "Encoded textures require a non-empty Basis payload");
        m_DiskFormat = diskFormat;
        m_SourceFormat = sourceFormat;
        m_EncodedSourceData = std::move(data);
    }

    void Texture::SetPendingSubresources(Vector<TextureSubresourceData> subresources)
    {
        m_PendingSubresources = std::move(subresources);
    }

    void Texture::ReleaseSourceData() { m_EncodedSourceData.clear(); }

    bool Texture::PrepareForInit()
    {
        if (m_EncodedSourceData.empty())
            return true;
        if (RenderAPI::TryGet() == nullptr)
        {
            CW_ENGINE_ERROR("Cannot transcode a Basis texture before the render API is initialized");
            return false;
        }

        BasisTextureInfo info;
        String error;
        if (!BasisTextureCodec::Inspect(m_EncodedSourceData.data(), m_EncodedSourceData.size(), info, &error))
        {
            CW_ENGINE_ERROR("Could not inspect Basis texture '{}': {}", GetName(), error);
            return false;
        }
        const TextureFormat target = BasisTextureCodec::SelectTarget(info, m_SourceFormat, RenderAPI::TryGet()->GetCapabilities());
        BasisTextureTranscodeResult result;
        const uint32_t requestedLevels = std::min(m_Desc.MipLevels, info.Levels - 1u) + 1u;
        if (!BasisTextureCodec::Transcode(m_EncodedSourceData.data(), m_EncodedSourceData.size(), m_SourceFormat, target,
                                          requestedLevels, result, &error))
        {
            CW_ENGINE_ERROR("Could not transcode Basis texture '{}': {}", GetName(), error);
            return false;
        }

        const uint64_t expectedSubresources = static_cast<uint64_t>(result.Info.Levels) * result.Info.Layers * result.Info.Faces;
        if (result.Info.GetSliceCount() == 0 || expectedSubresources != result.Subresources.size())
        {
            CW_ENGINE_ERROR("Basis texture '{}' produced an incomplete runtime subresource set", GetName());
            return false;
        }

        m_Desc.Width = result.Info.Width;
        m_Desc.Height = result.Info.Height;
        m_Desc.Depth = 1;
        m_Desc.Faces = result.Info.GetSliceCount();
        m_Desc.Shape = result.Info.GetRuntimeShape();
        m_Desc.MipLevels = result.Info.Levels - 1u;
        m_Desc.Format = result.Format;
        m_Desc.sRGB = result.Info.SRGB;
        m_Desc.GenerateMipmaps = false;

        m_PendingSubresources.clear();
        m_PendingSubresources.reserve(result.Subresources.size());
        for (BasisTextureSubresource& subresource : result.Subresources)
        {
            if (subresource.MipLevel >= result.Info.Levels || subresource.Layer >= result.Info.Layers ||
                subresource.Face >= result.Info.Faces || !subresource.Pixels)
            {
                CW_ENGINE_ERROR("Basis texture '{}' produced invalid runtime subresource metadata", GetName());
                m_PendingSubresources.clear();
                return false;
            }
            const uint32_t slice = result.Info.GetSliceIndex(subresource.Layer, subresource.Face);
            m_PendingSubresources.push_back({ subresource.MipLevel, slice, std::move(subresource.Pixels) });
        }
        return true;
    }

    void Texture::UploadPendingSubresources()
    {
        Vector<TextureSubresourceData> pending;
        pending.swap(m_PendingSubresources);
        for (const TextureSubresourceData& subresource : pending)
        {
            if (subresource.Pixels)
                WriteData(*subresource.Pixels, subresource.MipLevel, subresource.Face);
        }
    }

} // namespace Crowny
