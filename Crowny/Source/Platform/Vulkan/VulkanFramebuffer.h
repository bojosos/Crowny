#pragma once

#include "Platform/Vulkan/VulkanRenderPass.h"
#include "Platform/Vulkan/VulkanResource.h"
#include "Platform/Vulkan/VulkanTexture.h"
#include "Platform/Vulkan/VulkanUtils.h"

namespace Crowny
{

    class VulkanImage;

    struct VulkanAttachmentDesc
    {
        VulkanImage* Image = nullptr;
        TextureSurface Surface;
        uint32_t BaseLayer;
    };

    struct VulkanFramebufferDesc
    {
        uint32_t Width, Height;
        uint32_t LayerCount;
        VulkanAttachmentDesc Depth;
        VulkanAttachmentDesc Color[MAX_FRAMEBUFFER_COLOR_ATTACHMENTS];
    };

    struct VulkanFramebufferAttachment
    {
        uint32_t Index = 0;
        uint32_t BaseLayer = 0;
        TextureSurface Surface;
        VulkanImage* Image = nullptr;
        VkImageLayout FinalLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    };

    class VulkanFramebuffer : public VulkanResource
    {
    public:
        VulkanFramebuffer(VulkanResourceManager* owner, VulkanRenderPass* renderPass,
                          const VulkanFramebufferDesc& desc);
        ~VulkanFramebuffer();

        VulkanRenderPass* GetRenderPass() const { return m_RenderPass; }
        VkFramebuffer GetHandle() const { return m_Framebuffer; }
        uint32_t GetWidth() const { return m_Width; }
        uint32_t GetHeight() const { return m_Height; }

        const VulkanFramebufferAttachment& GetColorAttachment(uint32_t colorIdx) const
        {
            return m_ColorAttachments[colorIdx];
        }
        const VulkanFramebufferAttachment& GetDepthStencilAttachment() const { return m_DepthStencilAttachment; }
        uint32_t GetNumLayers() const { return m_NumLayers; }

    private:
        uint32_t m_Width;
        uint32_t m_Height;
        uint32_t m_NumLayers;
        VulkanFramebufferAttachment m_ColorAttachments[MAX_FRAMEBUFFER_COLOR_ATTACHMENTS]{};
        VulkanFramebufferAttachment m_DepthStencilAttachment;
        VulkanRenderPass* m_RenderPass;
        VkFramebuffer m_Framebuffer;
    };

} // namespace Crowny
