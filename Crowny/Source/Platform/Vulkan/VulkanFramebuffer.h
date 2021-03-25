#pragma once

#include "Platform/Vulkan/VulkanUtils.h"

#include "Crowny/Renderer/Framebuffer.h"

namespace Crowny
{

		struct FramebufferAttachment
	{
		VkImage Image;
		VkDeviceMemory Memory;
		VkImageView View;
		VkFormat Format;
		VkImageSubresourceRange SubresourceRange;
		VkAttachmentDescription Description;
	};

	struct VulkanFramebufferDesc
	{
		uint32_t Width;
		uint32_t Height;
		uint32_t Layers;
		VulkanTexture Depth;
		VulkanTexture Color[MAX_FRAMEBUFFER_COLOR_ATTACHMENTS];
	};
	
    class VulkanFramebuffer : public Framebuffer
    {
    public:
		VulkanFramebuffer(VulkanRenderPass* renderPass, const FramebufferProperties& props);
		VulkanFramebuffer(VulkanRenderPass* renderPass, const VulkanFramebufferDesc& desc);
		~VulkanFramebuffer();

		void Invalidate();

		virtual void Bind() const override;
		virtual void Unbind() const override;

		virtual void Resize(uint32_t width, uint32_t height) override;

		virtual uint32_t GetColorAttachmentRendererID() const override { return m_Attachments[0]->GetRendererID(); }
		virtual Ref<Texture2D> GetColorAttachment(uint32_t slot = 0) const override { return m_Attachments[0]; };

		virtual const FramebufferProperties& GetProperties() const override { return m_Properties; }

	private:
		VkDevice m_Device;
		VulkanRenderPass* m_RenderPass;
		std::vector<Ref<Texture2D>> m_Attachments;
		uint32_t m_RendererID = 0;
		FramebufferProperties m_Properties;
    }

}