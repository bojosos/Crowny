#pragma once

#include "Platform/Vulkan/VulkanDevice.h"

#include "Crowny/Common/Module.h"

namespace Crowny
{

    struct VulkanRenderPassAttachmentDesc
    {
        bool Enabled = false;
        VkFormat Format = VK_FORMAT_UNDEFINED;
    };
    
    struct VulkanRenderPassDesc
    {
        uint32_t Samples = 1;
        bool Offscreen = false;
        VulkanRenderPassAttachmentDesc Color[8]; // TODO: max render targets
        VulkanRenderPassAttachmentDesc Depth;
    };

    class VulkanRenderPass
    {
    public:
        VulkanRenderPass(const VulkanDevice& device, const VulkanRenderPassDesc& desc);
        ~VulkanRenderPass();
        
        VkRenderPass GetHandle() const { return m_RenderPass; }
        VkSampleCountFlagBits GetSampleFlags() const { return m_SampleFlags; }
        uint32_t GetNumAttachments() const { return m_NumColorAttachments; }
        uint32_t GetNumColorAttachments() const { return m_NumColorAttachments; }
        bool HasDepthAttachment() const { return m_HasDepth; }
        const VkAttachmentDescription& GetColorDesc(uint32_t idx) const { return m_Attachments[idx]; }
        const VkAttachmentDescription& GetDepthDesc() const { CW_ENGINE_ASSERT(m_HasDepth); return m_Attachments[m_NumColorAttachments]; }
        uint32_t GetId() const { return m_Id; }
    private:
        VkAttachmentDescription m_Attachments[8 + 1];
        bool m_HasDepth;
        uint32_t m_NumAttachments;
        uint32_t m_NumColorAttachments;
        VkSampleCountFlagBits m_SampleFlags = VK_SAMPLE_COUNT_1_BIT;
        VkRenderPass m_RenderPass;
        VkDevice m_Device;
        uint32_t m_Id;
        static uint32_t s_NextValidId;
    };
    
    class VulkanRenderPasses : public Module<VulkanRenderPasses>
    {
    public:
        ~VulkanRenderPasses();
        VulkanRenderPass* GetRenderPass(const VulkanRenderPassDesc& desc);
    private:
        struct PassVariant
        {   
            PassVariant(const VulkanRenderPassDesc& desc);
            
            class HashFunction
            {
            public:
                size_t operator()(const PassVariant& key) const;
            };

            class EqualFunction
            {
            public:
                bool operator()(const PassVariant& lhs, const PassVariant& rhs) const;
            };

            VulkanRenderPassDesc Desc;
        };
        
    private:
        mutable std::unordered_map<PassVariant, VulkanRenderPass*, PassVariant::HashFunction, PassVariant::EqualFunction> m_Passes;
    };

}