#include "cwpch.h"

#include "Crowny/Renderer/IndexBuffer.h"
#include "Crowny/Renderer/Renderer.h"

#include "Platform/OpenGL/OpenGLIndexBuffer.h"

namespace Crowny
{
	Ref<IndexBuffer> IndexBuffer::Create(uint32_t count)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::OpenGL: return CreateRef<OpenGLIndexBuffer>(count);
			default: 					   CW_ENGINE_ASSERT(false, "Renderer API not supporter"); return nullptr;
		}

		return nullptr;
	}

	Ref<IndexBuffer> IndexBuffer::Create(uint32_t* indices, uint32_t count)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::OpenGL: return CreateRef<OpenGLIndexBuffer>(indices, count);
			default: 					   CW_ENGINE_ASSERT(false, "Renderer API not supporter"); return nullptr;
		}

		return nullptr;
	}
}