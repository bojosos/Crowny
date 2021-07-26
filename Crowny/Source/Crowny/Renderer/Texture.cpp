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

    Ref<Texture> Texture::Create(const TextureParameters& params)
    {
        switch (Renderer::GetAPI())
		{
			//TODO: Add support for binary OpenGL shaders
			//case RendererAPI::API::OpenGL: return CreateRef<OpenGLShader>(m_Filepath);
			case RendererAPI::API::Vulkan: return CreateRef<VulkanTexture>(params);
			default: 					   CW_ENGINE_ASSERT(false, "Renderer API not supporter"); return nullptr;
		}

		return nullptr;
    }

}
