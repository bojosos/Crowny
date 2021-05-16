#include "cwpch.h"

#include "Crowny/Renderer/Renderer.h"
#include "Crowny/Renderer/VertexBuffer.h"
#include "Platform/OpenGL/OpenGLVertexBuffer.h"
#include "Platform/Vulkan/VulkanVertexBuffer.h"

namespace Crowny
{

	Ref<VertexBuffer> VertexBuffer::Create(uint32_t size, BufferUsage usage)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::OpenGL: return CreateRef<OpenGLVertexBuffer>(size, usage);
			case RendererAPI::API::Vulkan: return CreateRef<VulkanVertexBuffer>(size, usage);
			default: 					   CW_ENGINE_ASSERT(false, "Renderer API not supporter"); return nullptr;
		}
		return nullptr;
	}

	Ref<VertexBuffer> VertexBuffer::Create(void* vertices, uint32_t size, BufferUsage usage)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::OpenGL: return CreateRef<OpenGLVertexBuffer>(vertices, size, usage);
			case RendererAPI::API::Vulkan: return CreateRef<VulkanVertexBuffer>(vertices, size, usage);
			default: 					   CW_ENGINE_ASSERT(false, "Renderer API not supporter"); return nullptr;
		}
		return nullptr;
	}

}