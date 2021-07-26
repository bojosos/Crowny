#include "cwpch.h"

#include "Crowny/Renderer/RenderTexture.h"

#include "Crowny/Renderer/Renderer.h"

//#include "Platform/Vulkan/VulkanRenderTexture.h"
//#include "Platform/OpenGL/OpenGLRenderTexture.h"

namespace Crowny
{

    Ref<RenderTexture> RenderTexture::Create(const RenderTextureProperties& props)
	{
		switch (Renderer::GetAPI())
		{
			//case RendererAPI::API::OpenGL: return CreateRef<OpenGLRenderTexture>(props);
//			case RendererAPI::API::Vulkan: return CreateRef<VulkanRenderTexture>(props);
			default: 					   CW_ENGINE_ASSERT(false, "Renderer API not supporter"); return nullptr;
		}

		return nullptr;
	}
    
}