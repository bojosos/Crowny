#include "cwpch.h"

#include "Crowny/Renderer/Framebuffer.h"
#include "Crowny/Renderer/Renderer.h"

#include "Platform/OpenGL/OpenGLFramebuffer.h"
#include "Platform/Vulkan/VulkanFramebuffer.h"

namespace Crowny
{

	Ref<Framebuffer> Framebuffer::Create(const FramebufferProperties& props)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::OpenGL: return CreateRef<OpenGLFramebuffer>(props);
			//case RendererAPI::API::Vulkan: return CreateRef<VulkanRenderTexture>(props);
			default: 					   CW_ENGINE_ASSERT(false, "Renderer API not supporter"); return nullptr;
		}

		return nullptr;
	}

}