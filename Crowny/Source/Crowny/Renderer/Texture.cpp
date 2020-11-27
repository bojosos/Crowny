#include "cwpch.h"

#include "Crowny/Renderer/Renderer.h"
#include "Crowny/Renderer/Texture.h"

#include "Platform/OpenGL/OpenGLTextureCube.h"
#include "Platform/OpenGL/OpenGLTexture.h"

namespace Crowny
{

	Ref<Texture2D> Texture2D::Create(uint32_t width, uint32_t height, const TextureParameters& parameters)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::OpenGL: return CreateRef<OpenGLTexture2D>(width, height, parameters);
			default: 					   CW_ENGINE_ASSERT(false, "Renderer API not supporter"); return nullptr;
		}
		
		return nullptr;
	}

	Ref<Texture2D> Texture2D::Create(const std::string& filepath, const TextureParameters& parameters)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::OpenGL: return CreateRef<OpenGLTexture2D>(filepath, parameters);
			default: 					   CW_ENGINE_ASSERT(false, "Renderer API not supporter"); return nullptr;
		}

		return nullptr;
	}

	Ref<TextureCube> TextureCube::Create(const std::string& filepath, const TextureParameters& parameters)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::OpenGL: return CreateRef<OpenGLTextureCube>(filepath, parameters);
			default: 					   CW_ENGINE_ASSERT(false, "Renderer API not supporter"); return nullptr;
		}

		return nullptr;
	}

	Ref<TextureCube> TextureCube::Create(const std::array<std::string, 6>& files, const TextureParameters& parameters)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::OpenGL: return CreateRef<OpenGLTextureCube>(files, parameters);
			default: 					   CW_ENGINE_ASSERT(false, "Renderer API not supporter"); return nullptr;
		}

		return nullptr;
	}

	Ref<TextureCube> TextureCube::Create(const std::array<std::string, 6>& files, uint32_t mips, const TextureParameters& parameters)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::OpenGL: return CreateRef<OpenGLTextureCube>(files, mips, InputFormat::VERTICAL_CROSS, parameters);
			default: 					   CW_ENGINE_ASSERT(false, "Renderer API not supporter"); return nullptr;
		}

		return nullptr;
	}
}