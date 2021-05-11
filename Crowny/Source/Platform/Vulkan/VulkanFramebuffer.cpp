#include "cwpch.h"

#include "Platform/Vulkan/VulkanFramebuffer.h"
#include "Platform/Vulkan/VulkanRendererAPI.h"
#include "Platform/Vulkan/VulkanRenderPass.h"

namespace Crowny
{

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
	
	VulkanFramebuffer::VulkanFramebuffer(VulkanRenderPass* renderPass, const VulkanFramebufferDesc& desc) : m_RenderPass(renderPass)
	{
		m_Width = desc.Width;
		m_Height = desc.Height;
		m_Device = gVulkanRendererAPI().GetPresentDevice()->GetLogicalDevice();
		VkImageView attachmentViews[8 + 1];
		VkFramebufferCreateInfo framebufferCI;
		uint32_t idx = 0;
		for (uint32_t i = 0; i < 8; i++)
		{
			if (desc.Color[i].Image == VK_NULL_HANDLE)
				continue;
			m_ColorAttachments[idx].Image = desc.Color[i].Image;
			m_ColorAttachments[idx].FinalLayout = renderPass->GetColorDesc(idx).finalLayout;
			m_ColorAttachments[idx].Index = i;
			
			//TODO: image view
			attachmentViews[idx] = desc.Color[i].View;
			idx++;
		}

		if (renderPass->HasDepthAttachment())
		{
			m_DepthStencilAttachment.Image = desc.Depth.Image;
			m_DepthStencilAttachment.FinalLayout = renderPass->GetDepthDesc().finalLayout;
			m_DepthStencilAttachment.Index = 0;
			
			//TODO: image view
			attachmentViews[idx] = desc.Depth.View;
			idx++;
		}
		
		framebufferCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferCI.pNext = nullptr;
		framebufferCI.flags = 0;
		framebufferCI.attachmentCount = renderPass->GetNumAttachments();
		framebufferCI.pAttachments = attachmentViews;
		framebufferCI.width = desc.Width;
		framebufferCI.height = desc.Height;
		framebufferCI.layers = 1;
		
		framebufferCI.renderPass = m_RenderPass->GetHandle();
		VkResult result = vkCreateFramebuffer(m_Device, &framebufferCI, gVulkanAllocator, &m_Framebuffer);
		CW_ENGINE_ASSERT(result == VK_SUCCESS);
	}

	VulkanFramebuffer::~VulkanFramebuffer()
	{
		vkDestroyFramebuffer(m_Device, m_Framebuffer, gVulkanAllocator);
	}
/*
	VulkanRenderTexture::VulkanRenderTexture(const FramebufferProperties& desc) : m_Properties(*desc)
	{
		VulkanRenderPassDesc rpDesc;
		rpDesc.Samples = desc.Samples > 1 ? desc.Samples : 1;
		rpDesc.Offscreen = true; // ???
		VulkanFramebufferDesc fbDesc;
		fbDesc.Width = desc.Width;
		fbDesc.Height = desc.Height;

		for (uint32_t i = 0; i < 8; i++)
		{
			
		}
		VulkanRenderPass* renderPass = VulkanRenderPasses::Get().Get(m_Device, rpDesc);
		m_Framebuffer = new VulkanFramebuffer(renderPass, fbDesc);
	}*/

}