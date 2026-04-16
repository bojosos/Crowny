#include "cwpch.h"

#include "Crowny/RenderAPI/RenderTexture.h"

#include "Crowny/Renderer/Renderer.h"

#include "Platform/Vulkan/VulkanRenderTexture.h"
// #include "Platform/OpenGL/OpenGLRenderTexture.h"

namespace Crowny
{

    RenderTexture::RenderTexture(const RenderTextureDesc& props) : m_Desc(props)
    {
        for (uint32_t i = 0; i < MAX_FRAMEBUFFER_COLOR_ATTACHMENTS; i++)
        {
            if (m_Desc.ColorSurfaces[i].Texture != nullptr)
            {
                if ((m_Desc.ColorSurfaces[i].Texture->GetDesc().Usage & TEXTURE_RENDERTARGET) == 0)
                    CW_ENGINE_ASSERT(false, "Texture is not render target.");

                m_ColorSurfaces[i] = m_Desc.ColorSurfaces[i].Texture->RequestView(
                  m_Desc.ColorSurfaces[i].MipLevel, 1, m_Desc.ColorSurfaces[i].Face, m_Desc.ColorSurfaces[i].NumFaces, GVU_RENDERTARGET);
            }
        }

        if (m_Desc.DepthSurface.Texture != nullptr)
        {
            if ((m_Desc.DepthSurface.Texture->GetDesc().Usage & TEXTURE_DEPTHSTENCIL) == 0)
                CW_ENGINE_ASSERT(false, "Texture is not depth stencil.");

            m_DepthStencilSurface = m_Desc.DepthSurface.Texture->RequestView(m_Desc.DepthSurface.MipLevel, 1, m_Desc.DepthSurface.Face,
                                                                              m_Desc.DepthSurface.NumFaces, GVU_RENDERTARGET);
        }
    }

    Ref<RenderTexture> RenderTexture::Create(const RenderTextureDesc& props)
    {
        switch (gRenderAPI->GetAPI())
        {
        // case RenderAPI::API::OpenGL: return CreateRef<OpenGLRenderTexture>(props);
        case RenderAPI::API::Vulkan:
            return Ref<RenderTexture>(new VulkanRenderTexture(props));
        default:
            CW_ENGINE_ASSERT(false, "Renderer API not supported");
            return nullptr;
        }

        return nullptr;
    }

} // namespace Crowny