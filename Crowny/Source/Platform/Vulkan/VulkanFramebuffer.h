#pragma once

#include "Platform/Vulkan/VulkanUtils.h"

#include "Crowny/Renderer/Framebuffer.h"
#include "Crowny/Renderer/Texture.h"

#include "Platform/Vulkan/VulkanRenderPass.h"

namespace Crowny
{

	struct VulkanAttachmentDesc
	{
		VkImage Image;
		VkImageView View;
		VkDeviceMemory Memory;
		VkImageSubresourceRange SubresourceRange;
		VkFormat Format;
		VkAttachmentDescription Description;
	};

	struct VulkanFramebufferDesc
	{
		uint32_t Width, Height;
		uint32_t LayerCount;
		VulkanAttachmentDesc Depth;
		VulkanAttachmentDesc Color[8];
	};

	struct VulkanFramebufferAttachment {
		VkImage Image = VK_NULL_HANDLE;
		VkImageView View;
		uint32_t Index = 0;
		VkFormat Format;
		VkImageLayout FinalLayout = VK_IMAGE_LAYOUT_UNDEFINED;		
	};
		
    class VulkanFramebuffer
    {
    public:
		VulkanFramebuffer(VulkanRenderPass* renderPass, const VulkanFramebufferDesc& desc);
		~VulkanFramebuffer();

		VulkanRenderPass* GetRenderPass() const { return m_RenderPass; }
		VkFramebuffer GetHandle() const { return m_Framebuffer; }
		uint32_t GetWidth() const { return m_Width; }
		uint32_t GetHeight() const { return m_Height; }
		
		const VulkanFramebufferAttachment& GetColorAttachment(uint32_t colorIdx) const { return m_ColorAttachments[colorIdx]; }
		const VulkanFramebufferAttachment& GetDepthStencilAttachment() const { return m_DepthStencilAttachment; }
		
	private:
		uint32_t m_Width;
		uint32_t m_Height;
		VulkanFramebufferAttachment m_ColorAttachments[8]{};
		VulkanFramebufferAttachment m_DepthStencilAttachment;
		VkDevice m_Device;
		VulkanRenderPass* m_RenderPass;
		VkFramebuffer m_Framebuffer;
    };
/*
	class VulkanRenderTexture : public Framebuffer
	{
	public:
		~VulkanRenderTexture() = default;

		VulkanRenderTexture(const FramebufferProperties& desc);
	private:
		FramebufferProperties m_Properties;
		VulkanFramebuffer* m_Framebuffer;
	};*/

}