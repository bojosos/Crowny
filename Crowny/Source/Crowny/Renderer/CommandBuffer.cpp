#include "cwpch.h"

#include "Crowny/Renderer/CommandBuffer.h"
#include "Crowny/Renderer/Renderer.h"

#include "Platform/Vulkan/VulkanCommandBuffer.h"

namespace Crowny
{

    Ref<CommandBuffer> CommandBuffer::Create(GpuQueueType type)
    {
        switch (Renderer::GetAPI())
		{
			//case RendererAPI::API::OpenGL: return CreateRef<OpenGLFramebuffer>(props);
			case RendererAPI::API::Vulkan: return CreateRef<VulkanCmdBuffer>(type);
			default: 					   CW_ENGINE_ASSERT(false, "Renderer API not supporter"); return nullptr;
		}

		return nullptr;
    }

}