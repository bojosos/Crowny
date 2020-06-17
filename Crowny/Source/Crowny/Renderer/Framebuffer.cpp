#include "cwpch.h"

#include "Crowny/Renderer/Framebuffer.h"
#include "Crowny/Renderer/Renderer.h"

#include "Platform/OpenGL/OpenGLFramebuffer.h"

namespace Crowny
{

	Ref<Framebuffer> Framebuffer::Create(const FramebufferProperties& props)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::OpenGL: return CreateRef<OpenGLFramebuffer>(props);
		}
	}

}