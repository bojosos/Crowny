#include "cwpch.h"

#include "Crowny/Renderer/Renderer.h"
#include "Crowny/Renderer/VertexBuffer.h"
#include "Platform/OpenGL/OpenGLVertexBuffer.h"
#include "Platform/Vulkan/VulkanVertexBuffer.h"

namespace Crowny
{

	Ref<VertexBuffer> VertexBuffer::Create(void* vertices, uint32_t size, const VertexBufferProperties& props)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::OpenGL: return CreateRef<OpenGLVertexBuffer>(vertices, size, props);
			//case RendererAPI::API::Vulkan: return CreateRef<VulkanVertexBuffer>(vertices, size, props);
			default: 					   CW_ENGINE_ASSERT(false, "Renderer API not supporter"); return nullptr;
		}
		return nullptr;
	}

}