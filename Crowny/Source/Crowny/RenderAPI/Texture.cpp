#include "cwpch.h"

#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Renderer/Renderer.h"

#include "Platform/OpenGL/OpenGLTexture.h"
#include "Platform/Vulkan/VulkanTexture.h"

namespace Crowny
{

    const TextureSurface TextureSurface::COMPLETE = TextureSurface(0, 0, 0, 0);

    Ref<TextureView> Texture::CreateView(const TextureViewDesc& desc) { return CreateRef<TextureView>(TextureViewDesc(desc)); }

    Ref<TextureView> Texture::RequestView(uint32_t mip, uint32_t numMips, uint32_t firstFace, uint32_t numFaces, GpuViewUsage usage)
    {
        const TextureDesc& props = GetDesc();
        TextureViewDesc desc;
        desc.MostDetailedMip = mip;
        desc.NumMips = numMips == 0 ? (props.MipLevels + 1) : numMips;
        desc.FirstFace = firstFace;
        desc.NumFaces = numFaces == 0 ? props.Faces : numFaces;
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

    Texture::Texture(const TextureDesc& params) : m_Desc(params) {}
    Texture::Texture(const TextureDesc& params, bool deferred) : m_Desc(params) {}

    Ref<Texture> Texture::Create(const TextureDesc& params)
    {
        switch (gRenderAPI->GetAPI())
        {
        // case RenderAPI::API::OpenGL: return Ref<Texture>(new VulkanTexture(m_Filepath));
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
        switch (gRenderAPI->GetAPI())
        {
        case RenderAPI::API::Vulkan: {
            auto tex = Ref<Texture>(new VulkanTexture(params, true));
            tex->m_PendingPixelData = pixelData;
            return tex;
        }
        default:
            CW_ENGINE_ASSERT(false, "Renderer API not supported");
            return nullptr;
        }
    }

    Ref<PixelData> Texture::AllocatePixelData(uint32_t face, uint32_t mipLevel) const
    {
        uint32_t width = GetWidth();
        uint32_t height = GetHeight();
        uint32_t depth = GetDepth();

        for (uint32_t j = 0; j < mipLevel; j++)
        {
            if (width != 1)
                width /= 2;
            if (height != 1)
                height /= 2;
            if (depth != 1)
                depth /= 2;
        }

        Ref<PixelData> dst = CreateRef<PixelData>(width, height, depth, GetFormat());
        dst->AllocateInternalBuffer();
        return dst;
    }

} // namespace Crowny
