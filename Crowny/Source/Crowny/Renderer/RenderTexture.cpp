#include "cwpch.h"

#include "Crowny/Renderer/RenderTexture.h"

#include "Crowny/Renderer/Renderer.h"

#include "Platform/Vulkan/VulkanRenderTexture.h"
//#include "Platform/OpenGL/OpenGLRenderTexture.h"

namespace Crowny
{

	RenderTexture::RenderTexture(const RenderTextureProperties& props) : m_Props(props)
	{
		for (uint32_t i = 0; i < MAX_FRAMEBUFFER_COLOR_ATTACHMENTS; i++)
		{
			if (m_Props.ColorSurfaces[i].Texture != nullptr)
			{
				if ((m_Props.ColorSurfaces[i].Texture->GetProperties().Usage & TEXTURE_RENDERTARGET) == 0)
					CW_ENGINE_ASSERT(false, "Texture is not render target.");

				m_ColorSurfaces[i] = m_Props.ColorSurfaces[i].Texture->RequestView(m_Props.ColorSurfaces[i].MipLevel, 1, m_Props.ColorSurfaces[i].Face, m_Props.ColorSurfaces[i].NumFaces, GVU_RENDERTARGET);
			}
		}
		
		if (m_Props.DepthSurface.Texture != nullptr)
		{
			if ((m_Props.DepthSurface.Texture->GetProperties().Usage & TEXTURE_DEPTHSTENCIL) == 0)
					CW_ENGINE_ASSERT(false, "Texture is not depth stencil.");

			m_DepthStencilSurface = m_Props.DepthSurface.Texture->RequestView(m_Props.DepthSurface.MipLevel, 1, m_Props.DepthSurface.Face, m_Props.DepthSurface.NumFaces, GVU_RENDERTARGET);
		}
	}


    Ref<RenderTexture> RenderTexture::Create(const RenderTextureProperties& props)
	{
		switch (Renderer::GetAPI())
		{
			//case RendererAPI::API::OpenGL: return CreateRef<OpenGLRenderTexture>(props);
			case RendererAPI::API::Vulkan: return CreateRef<VulkanRenderTexture>(props);
			default: 					   CW_ENGINE_ASSERT(false, "Renderer API not supporter"); return nullptr;
		}

		return nullptr;
	}
    
}