#include "cwpch.h"

#include "Crowny/Renderer/Renderer.h"
#include "Crowny/Renderer/VertexBuffer.h"
#include "Platform/OpenGL/OpenGLVertexBuffer.h"

namespace Crowny
{

	Ref<VertexBuffer> VertexBuffer::Create(float* vertices, uint32_t size, const VertexBufferProperties& props)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::OpenGL: return CreateRef<OpenGLVertexBuffer>(vertices, size, props);
		}
	}
}