#include "cwpch.h"

#include "Crowny/Renderer/GraphicsContext.h"
#include "Crowny/Renderer/Renderer.h"

#include "Platform/OpenGL/OpenGLContext.h"

namespace Crowny
{

	Scope<GraphicsContext> GraphicsContext::Create(void* window)
	{
		switch (Renderer::GetAPI())
		{
			//TODO: Do not tie OpenGL and GLFW
		case RendererAPI::API::OpenGL: return CreateScope<OpenGLContext>(window);	
		}
	}

}