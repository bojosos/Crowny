#include "cwpch.h"

#include "Crowny/Renderer/VertexArray.h"

#include "Crowny/Renderer/Renderer.h"
#include "Platform/OpenGL/OpenGLVertexArray.h"

namespace Crowny
{
	Ref<VertexArray> VertexArray::Create()
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::OpenGL: return CreateRef<OpenGLVertexArray>();
		}

		return nullptr;
	}
}