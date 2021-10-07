#include "cwpch.h"

#include "Platform/Vulkan/VulkanRenderTexture.h"

#include "Platform/Vulkan/VulkanRenderAPI.h"
#include "Platform/Vulkan/VulkanTexture.h"

namespace Crowny
{

    VulkanRenderTexture::VulkanRenderTexture(const RenderTextureProperties& props) : RenderTexture(props)
    {
        VulkanRenderPassDesc rpDesc;
        rpDesc.Samples = m_Props.Samples;
        rpDesc.Offscreen = true;

        VulkanFramebufferDesc fbDesc;
        fbDesc.Width = m_Props.Width;
        fbDesc.Height = m_Props.Height;
        fbDesc.LayerCount = m_Props.NumSlices;

        for (uint32_t i = 0; i < MAX_FRAMEBUFFER_COLOR_ATTACHMENTS; i++)
        {
            if (m_ColorSurfaces[i] == nullptr)
                continue;

            const Ref<TextureView>& view = m_ColorSurfaces[i];
            VulkanTexture* texture = static_cast<VulkanTexture*>(m_Props.ColorSurfaces[i].Texture.get());
            VulkanImage* image = texture->GetImage();
            if (image == nullptr)
                continue;

            TextureSurface surface;
            surface.MipLevel = view->GetMostDetailedMip();
            surface.NumMipLevels = view->GetNumMips();

            if (texture->GetProperties().Shape == TextureShape::TEXTURE_3D)
            {
                if (view->GetFirstFace() > 0)
                    CW_ENGINE_ASSERT(false, "Non zero slice in a 3D texture.");
                if (view->GetNumFaces() > 1)
                    CW_ENGINE_ASSERT(false, "3D texture with more than one slice.");
                surface.Face = 0;
                surface.NumFaces = m_Props.NumSlices;
                fbDesc.Color[i].BaseLayer = 0;
            }
            else
            {
                surface.Face = view->GetFirstFace();
                surface.NumFaces = view->GetNumFaces();
                fbDesc.Color[i].BaseLayer = view->GetFirstFace();
                fbDesc.LayerCount = view->GetNumFaces();
            }

            fbDesc.Color[i].Image = image;
            fbDesc.Color[i].Surface = surface;
            rpDesc.Color[i].Enabled = true;
            rpDesc.Color[i].Format = VulkanUtils::GetTextureFormat(texture->GetProperties().Format, false);
        }

        if (m_DepthStencilSurface != nullptr)
        {
            const Ref<TextureView>& view = m_DepthStencilSurface;
            VulkanTexture* texture = static_cast<VulkanTexture*>(m_Props.DepthSurface.Texture.get());
            VulkanImage* image = texture->GetImage();
            if (image != nullptr)
            {
                TextureSurface surface;
                surface.MipLevel = view->GetMostDetailedMip();
                surface.NumMipLevels = view->GetNumMips();
                if (texture->GetProperties().Shape == TextureShape::TEXTURE_3D)
                {
                    if (view->GetFirstFace() > 0)
                        CW_ENGINE_ASSERT(false, "Non zero slice in a 3D texture.");
                    if (view->GetNumFaces() > 1)
                        CW_ENGINE_ASSERT(false, "3D texture with more than one slice.");
                    surface.Face = 0;
                    surface.NumFaces = m_Props.NumSlices;
                }
                else
                {
                    surface.Face = view->GetFirstFace();
                    surface.NumFaces = view->GetNumFaces();
                    fbDesc.LayerCount = view->GetNumFaces();
                }

                fbDesc.Depth.Image = image;
                fbDesc.Depth.Surface = surface;
                fbDesc.Depth.BaseLayer = view->GetFirstFace();
                rpDesc.Depth.Enabled = true;
                rpDesc.Depth.Format = VulkanUtils::GetTextureFormat(texture->GetProperties().Format, false);
            }
        }

        VulkanDevice& device = *gVulkanRenderAPI().GetPresentDevice().get();
        VulkanRenderPass* renderPass = VulkanRenderPasses::Get().GetRenderPass(rpDesc);
        m_Framebuffer = device.GetResourceManager().Create<VulkanFramebuffer>(renderPass, fbDesc);
    }

    VulkanRenderTexture::~VulkanRenderTexture() { m_Framebuffer->Destroy(); }

} // namespace Crowny