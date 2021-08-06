#include "cwpch.h"

#include "Platform/Vulkan/VulkanFramebuffer.h"
#include "Platform/Vulkan/VulkanRenderAPI.h"
#include "Platform/Vulkan/VulkanRenderPass.h"

namespace Crowny
{
    /*
        static TextureFormat FramebufferToTextureFormat(FramebufferTextureFormat textureFormat)
        {
            switch(textureFormat)
            {
                case FramebufferTextureFormat::R32I:			    return TextureFormat::R32I;
                case FramebufferTextureFormat::RGB8:    			return TextureFormat::RGB8;
                case FramebufferTextureFormat::RG32F:   			return TextureFormat::RG32F;
                case FramebufferTextureFormat::RGBA16F: 			return TextureFormat::RGBA16F;
                case FramebufferTextureFormat::RGBA32F:		  		return TextureFormat::RGBA32F;
                case FramebufferTextureFormat::DEPTH24STENCIL8: 	return TextureFormat::DEPTH24STENCIL8;
                case FramebufferTextureFormat::DEPTH32F:        	return TextureFormat::DEPTH32F;
                case FramebufferTextureFormat::RGBA8: 				return TextureFormat::RGBA8;
                default: 											return TextureFormat::NONE;
            }

            return TextureFormat::NONE;
        }
        */
    VulkanFramebuffer::VulkanFramebuffer(VulkanResourceManager* owner, VulkanRenderPass* renderPass,
                                         const VulkanFramebufferDesc& desc)
      : VulkanResource(owner, false), m_RenderPass(renderPass), m_NumLayers(desc.LayerCount)
    {
        m_Width = desc.Width;
        m_Height = desc.Height;
        VkImageView attachmentViews[MAX_FRAMEBUFFER_COLOR_ATTACHMENTS + 1];
        VkFramebufferCreateInfo framebufferCI;
        uint32_t idx = 0;
        for (uint32_t i = 0; i < MAX_FRAMEBUFFER_COLOR_ATTACHMENTS; i++)
        {
            if (desc.Color[i].Image == VK_NULL_HANDLE)
                continue;
            m_ColorAttachments[idx].Image = desc.Color[i].Image;
            m_ColorAttachments[idx].FinalLayout = renderPass->GetColorDesc(idx).finalLayout;
            m_ColorAttachments[idx].Index = i;
            m_ColorAttachments[idx].Surface = desc.Color[i].Surface;
            m_ColorAttachments[idx].BaseLayer = desc.Color[i].BaseLayer;

            if (desc.Color[i].Surface.NumMipLevels == 0)
                attachmentViews[idx] = desc.Color[i].Image->GetView(true);
            else
                attachmentViews[idx] = desc.Color[i].Image->GetView(desc.Color[i].Surface, true);
            idx++;
        }

        if (renderPass->HasDepthAttachment())
        {
            m_DepthStencilAttachment.BaseLayer = desc.Depth.BaseLayer;
            m_DepthStencilAttachment.Surface = desc.Depth.Surface;
            m_DepthStencilAttachment.Image = desc.Depth.Image;
            m_DepthStencilAttachment.FinalLayout = renderPass->GetDepthDesc().finalLayout;
            m_DepthStencilAttachment.Index = 0;

            if (desc.Depth.Surface.NumMipLevels == 0)
                attachmentViews[idx] = desc.Depth.Image->GetView(true);
            else
                attachmentViews[idx] = desc.Depth.Image->GetView(desc.Depth.Surface, true);
            idx++;
        }

        framebufferCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferCI.pNext = nullptr;
        framebufferCI.flags = 0;
        framebufferCI.attachmentCount = renderPass->GetNumAttachments();
        framebufferCI.pAttachments = attachmentViews;
        framebufferCI.width = desc.Width;
        framebufferCI.height = desc.Height;
        framebufferCI.layers = desc.LayerCount;
        framebufferCI.renderPass = m_RenderPass->GetVkRenderPass(RT_NONE, RT_NONE, CLEAR_NONE);

        VulkanDevice& device = m_Owner->GetDevice();
        VkResult result =
          vkCreateFramebuffer(device.GetLogicalDevice(), &framebufferCI, gVulkanAllocator, &m_Framebuffer);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
    }

    VulkanFramebuffer::~VulkanFramebuffer()
    {
        VulkanDevice& device = m_Owner->GetDevice();
        vkDestroyFramebuffer(device.GetLogicalDevice(), m_Framebuffer, gVulkanAllocator);
    }
    /*
        VulkanRenderTexture::VulkanRenderTexture(const FramebufferProperties& desc) : m_Properties(desc)
        {
            if (desc.SwapChainTarget)
            {
            }
        }*/

} // namespace Crowny
