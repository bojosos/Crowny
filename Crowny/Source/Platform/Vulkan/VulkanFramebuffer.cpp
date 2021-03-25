#include "cwpch.h"

#include "Platform/Vulkan/VulkanFramebuffer.h"

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
	
	VulkanFramebuffer::VulkanFramebuffer(const FramebufferProperties& props) : m_Properties(props)
	{
		Invalidate();
	}

	void VulkanFramebuffer::Invalidate()
	{
		VkImage image;
		VkImageCreateInfo imageCreateInfo{};
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.pNext = nullptr;
		imageCreateInfo.flags = 0;
		imageCreateInfo.format = format;
		imageCreateInfo.extent.width = m_Properties.Width;
		imageCreateInfo.extent.height = m_Properties.Height;
		imageCreateInfo.extent.depth = 1;
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VkResult result = vkCreateImage(m_Device, &imageCreateInfo, nullptr, &image);
		CW_ENGINE_ASSERT(result == VK_SUCCESS);
		
		VkMemoryRequirements memReqs;
		vkGetImageMemoryRequirements(device, image, &memReqs);
		VkMemoryAllocateInfo memAllocInfo{};
		memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		memAllocInfo.allocationSize = memReqs.size;
		memAllocInfo.memoryTypeIndex = ;
		result = vkAllocateMemory(device, &memAllocInfo, nullptr, &memory);
		CW_ENGINE_ASSERT(result == VK_SUCCESS);
		result = vkBindImageMemory(device, image, memory, 0);
		CW_ENGINE_ASSERT(result == VK_SUCCESS);
		
		VkImageView view;
		
		VkImageViewCreateInfo viewCreateInfo{};
		viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewCreateInfo.pNext = nullptr;
		viewCreateInfo.flags = 0;
		viewCreateInfo.format = format;
		viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewCreateInfo.subresourceRange = {};
		viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewCreateInfo.subresourceRange.baseMipLevel = 0;
		viewCreateInfo.subresourceRange.levelCount = 1;
		viewCreateInfo.subresourceRange.baseArrayLayer = 0;
		viewCreateInfo.subresourceRange.layerCount = 1;
		viewCreateInfo.image = image;
		result = vkCreateImageView(device, &viewCreateInfo, nullptr, &view);
		CW_ENGINE_ASSERT(result == VK_SUCCESS);
		
		VkFramebufferCreateInfo framebufferCreateInfo{};
		framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferCreateInfo.pNext = nullptr;
		framebufferCreateInfo.flags = 0;
		framebufferCreateInfo.renderPass;
		framebufferCreateInfo.attachmentCount = m_Attachments.size();
		framebufferCreateInfo.pAttachments = m_Attachments.data();
		framebufferCreateInfo.width = m_Properties.Width;
		framebufferCreateInfo.height = m_Properties.Height;
		framebufferCreateInfo.layers = 1;
		
		
		
		VkResult result = vkCreateFramebuffer(device, &framebufferCreateInfo, nullptr, m_Framebuffer);
	}

	VulkanFramebuffer::~VulkanFramebuffer()
	{
		glDeleteFramebuffers(1, &m_RendererID);
	}

	void VulkanFramebuffer::Bind() const
	{
		glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);
		glViewport(0, 0, m_Properties.Width, m_Properties.Height);
	}

	void VulkanFramebuffer::Unbind() const
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void VulkanFramebuffer::Resize(uint32_t width, uint32_t height)
	{
		m_Properties.Width = width;
		m_Properties.Height = height;

		Invalidate();
	}

}