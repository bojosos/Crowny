#include "cwpch.h"

#include "Crowny/Renderer/UniformBuffer.h"
#include "Crowny/Renderer/Renderer.h"

//#include "Platform/OpenGL/OpenGLUniformBuffer.h"
#include "Platform/Vulkan/VulkanUniformBuffer.h"

namespace Crowny
{

    Ref<UniformBuffer> UniformBuffer::Create(uint32_t size, BufferUsage usage)
    {
        switch (Renderer::GetAPI())
		{
//			case RendererAPI::API::OpenGL: return CreateRef<OpenGLUniformBuffer>(size, usage);
			case RendererAPI::API::Vulkan: return CreateRef<VulkanUniformBuffer>(size, usage);
			default: 					   CW_ENGINE_ASSERT(false, "Renderer API not supporter"); return nullptr;
		}
		return nullptr;
    }

}