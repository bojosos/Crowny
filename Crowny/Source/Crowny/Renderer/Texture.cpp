#include "cwpch.h"

#include "Crowny/Renderer/Renderer.h"
#include "Crowny/Renderer/Texture.h"

#include "Platform/OpenGL/OpenGLTexture.h"

namespace Crowny
{

	Ref<Texture2D> Texture2D::Create(uint32_t width, uint32_t height, const TextureParameters& parameters)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::OpenGL: return CreateRef<OpenGLTexture2D>(width, height, parameters);
		}
	}

	Ref<Texture2D> Texture2D::Create(const std::string& filepath, const TextureParameters& parameters)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::OpenGL: return CreateRef<OpenGLTexture2D>(filepath, parameters);
		}
	}

}