#include "cwpch.h"

#include "Crowny/Renderer/Renderer.h"
#include "Crowny/Renderer/Texture.h"

#include "Platform/OpenGL/OpenGLTexture.h"
#include "Platform/Vulkan/VulkanTexture.h"

namespace Crowny
{

    Texture::Texture(const TextureParameters& params) : m_Params(params)
    { }

	const TextureSurface TextureSurface::COMPLETE = TextureSurface(0, 0, 0, 0);

	Ref<TextureView> Texture::CreateView(const TextureViewDesc& desc)
	{
		return CreateRef<TextureView>(TextureViewDesc(desc));
	}

	Ref<TextureView> Texture::RequestView(uint32_t mip, uint32_t numMips, uint32_t firstArraySlice, uint32_t numArraySlices, GpuViewUsage usage)
	{
		const TextureParameters& props = GetProperties();
		TextureViewDesc desc;
		desc.MostDetailedMip = mip;
		desc.NumMips = numMips == 0 ? (props.MipLevels + 1) : numMips;
		desc.FirstArraySlice = firstArraySlice;
		desc.NumArraySlices = numArraySlices == 0 ? props.Faces : numArraySlices;
		desc.Usage = usage;

		auto iter = m_TextureViews.find(desc);
		if (iter == m_TextureViews.end())
		{
			m_TextureViews[desc] = CreateView(desc);
			iter = m_TextureViews.find(desc);
		}

		return iter->second;
	}

    Ref<Texture> Texture::Create(const TextureParameters& params)
    {
        switch (Renderer::GetAPI())
		{
			//case RendererAPI::API::OpenGL: return CreateRef<VulkanTexture>(m_Filepath);
			case RendererAPI::API::Vulkan: return CreateRef<VulkanTexture>(params);
			default: 					   CW_ENGINE_ASSERT(false, "Renderer API not supporter"); return nullptr;
		}

		return nullptr;
    }

}
