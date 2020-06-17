#include "cwpch.h"

#include "Crowny/Renderer/RendererAPI.h"
#include "Platform/OpenGL/OpenGLRendererAPI.h"

namespace Crowny
{
	RendererAPI::API RendererAPI::s_API = RendererAPI::API::OpenGL;

	Scope<RendererAPI> RendererAPI::Create()
	{
		switch (s_API)
		{
		case RendererAPI::API::OpenGL: return CreateScope<OpenGLRendererAPI>();
		}

		CW_ENGINE_ASSERT(false, "Unsupported Rendeerer API!");
		return nullptr;
	}
}