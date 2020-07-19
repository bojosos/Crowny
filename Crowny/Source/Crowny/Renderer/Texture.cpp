#include "cwpch.h"

#include "Crowny/Renderer/Renderer.h"
#include "Crowny/Renderer/Texture.h"

#include "Platform/OpenGL/OpenGLTextureCube.h"
#include "Platform/OpenGL/OpenGLTexture.h"

namespace Crowny
{

	Ref<Texture> Texture::Create()
	{

		if (m_Data.SelectedTextureShape == 0)
		{
			return Texture2D::Create(m_Data.Filepath, m_Data);
		}

	}

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

	Ref<TextureCube> TextureCube::Create(const std::string& filepath, const TextureParameters& parameters)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::OpenGL: return CreateRef<OpenGLTextureCube>(filepath, parameters);
		}
	}

	Ref<TextureCube> TextureCube::Create(const std::vector<std::string>& files, const TextureParameters& parameters)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::OpenGL: return CreateRef<OpenGLTextureCube>(files, parameters);
		}
	}

	Ref<TextureCube> TextureCube::Create(const std::vector<std::string>& files, uint32_t mips, const TextureParameters& parameters)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::OpenGL: return CreateRef<OpenGLTextureCube>(files, mips, InputFormat::VERTICAL_CROSS, parameters);
		}
	}
}