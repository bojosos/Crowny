#pragma once

#include "Platform/Vulkan/VulkanDevice.h"

namespace Crowny
{

    struct VulkanRenderPassDesc
    {
        uint32_t Samples = 1;
    };

    class VulkanRenderPass
    {
    public:
        VulkanRenderPass(const VulkanDevice& device, const VulkanRenderPassDesc& desc);
        ~VulkanRenderPass();
    private:
        VkRenderPass m_RenderPass;
        VkDevice m_Device;
    };

}