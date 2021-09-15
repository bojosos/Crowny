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
        VulkanRenderPassAttachmentDesc Color[MAX_FRAMEBUFFER_COLOR_ATTACHMENTS];
        VulkanRenderPassAttachmentDesc Depth;
    };

    class VulkanRenderPass
    {
    public:
        VulkanRenderPass(const VkDevice& device, const VulkanRenderPassDesc& desc);
        ~VulkanRenderPass();

        VkRenderPass GetHandle() const { return m_DefaultRenderPass; }
        VkSampleCountFlagBits GetSampleFlags() const { return m_SampleFlags; }
        uint32_t GetNumAttachments() const { return m_NumAttachments; }
        uint32_t GetNumColorAttachments() const { return m_NumColorAttachments; }
        bool HasDepthAttachment() const { return m_HasDepth; }
        const VkAttachmentDescription& GetColorDesc(uint32_t idx) const { return m_Attachments[idx]; }
        const VkAttachmentDescription& GetDepthDesc() const
        {
            CW_ENGINE_ASSERT(m_HasDepth);
            return m_Attachments[m_NumColorAttachments];
        }
        uint32_t GetId() const { return m_Id; }
        VkRenderPass GetVkRenderPass(RenderSurfaceMask loadMask, RenderSurfaceMask readMask, ClearMask clearMask) const;
        uint32_t GetNumClearEntries(ClearMask clearMask) const;

    private:
        VkRenderPass CreateVariant(RenderSurfaceMask loadMask, RenderSurfaceMask readMask, ClearMask clearMask) const;
        struct VariantKey
        {
            VariantKey(RenderSurfaceMask loadMask, RenderSurfaceMask readMask, ClearMask clearMask);
            struct HashFunction
            {
                size_t operator()(const VariantKey& key) const;
            };

            struct EqualFunction
            {
                bool operator()(const VariantKey& lhs, const VariantKey& rhs) const;
            };

            RenderSurfaceMask LoadMask;
            RenderSurfaceMask ReadMask;
            Crowny::ClearMask ClearMask;
        };

        bool m_HasDepth;
        uint32_t m_NumAttachments;
        uint32_t m_NumColorAttachments;
        VkSampleCountFlagBits m_SampleFlags = VK_SAMPLE_COUNT_1_BIT;
        VkDevice m_Device;
        uint32_t m_Id;
        uint32_t m_Indices[MAX_FRAMEBUFFER_COLOR_ATTACHMENTS]{ 0 };

        mutable VkAttachmentDescription m_Attachments[MAX_FRAMEBUFFER_COLOR_ATTACHMENTS + 1];
        mutable VkAttachmentReference m_ColorReferences[MAX_FRAMEBUFFER_COLOR_ATTACHMENTS];
        mutable VkAttachmentReference m_DepthReference;
        mutable VkSubpassDependency m_Dependencies[2];
        mutable VkSubpassDescription m_SubpassDesc{};
        mutable VkRenderPassCreateInfo m_RenderPassCreateInfo;

        VkRenderPass m_DefaultRenderPass;
        mutable std::unordered_map<VariantKey, VkRenderPass, VariantKey::HashFunction, VariantKey::EqualFunction>
          m_Variants;
        static uint32_t s_NextValidId;
    };

    class VulkanRenderPasses : public Module<VulkanRenderPasses>
    {
    public:
        ~VulkanRenderPasses();
        VulkanRenderPass* GetRenderPass(const VulkanRenderPassDesc& desc) const;

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
        mutable std::unordered_map<PassVariant, VulkanRenderPass*, PassVariant::HashFunction,
                                   PassVariant::EqualFunction>
          m_Passes;
    };

} // namespace Crowny