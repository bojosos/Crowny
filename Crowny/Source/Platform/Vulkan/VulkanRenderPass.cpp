#include "cwpch.h"

#include "Platform/Vulkan/VulkanRenderPass.h"

#include "Platform/Vulkan/VulkanRendererAPI.h"

namespace Crowny
{
    
    uint32_t VulkanRenderPass::s_NextValidId = 1;
    
    VulkanRenderPass::VulkanRenderPass(const VulkanDevice& device, const VulkanRenderPassDesc& desc)
    {
        m_Id = s_NextValidId++;
        m_SampleFlags = VulkanUtils::GetSampleFlags(desc.Samples);
        m_Device = device.GetLogicalDevice();
        uint32_t idx = 0;
        VkAttachmentReference colorRefs[8];
        VkAttachmentReference depthRef;
        for (uint32_t i = 0; i < 8; i++)
        {
            if (!desc.Color[i].Enabled)
                continue;
            
            VkAttachmentDescription& colorAttachment = m_Attachments[idx];
            colorAttachment.flags = 0;
            colorAttachment.format = desc.Color[i].Format;
            colorAttachment.samples = m_SampleFlags;
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            
            if (desc.Offscreen) // assumes shader read
                colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            else
                colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            
            VkAttachmentReference& ref = colorRefs[idx];
            ref.attachment = i;
            ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            idx++;
        }
        
        m_HasDepth = desc.Depth.Enabled;
        m_NumColorAttachments = idx;
        
        if (m_HasDepth)
        {
            VkAttachmentDescription& colorAttachment = m_Attachments[idx];
            colorAttachment.flags = 0;
            colorAttachment.format = desc.Depth.Format;
            colorAttachment.samples = m_SampleFlags;
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            colorAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            
            VkAttachmentReference& ref = depthRef;
            ref.attachment = idx;
            ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            idx++;
        }
        
        m_NumAttachments = idx;

        VkSubpassDescription subpass{};
        subpass.flags = 0;
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = m_NumColorAttachments;
        subpass.inputAttachmentCount = 0;
        subpass.pInputAttachments = nullptr;
        subpass.preserveAttachmentCount = 0;
        subpass.pPreserveAttachments = nullptr;
        subpass.pResolveAttachments = nullptr;
        
        if (m_NumColorAttachments > 0)
            subpass.pColorAttachments = colorRefs;
        else
            subpass.pColorAttachments = nullptr;

        if (m_HasDepth)
            subpass.pDepthStencilAttachment = &depthRef;
        else
            subpass.pDepthStencilAttachment = nullptr;
        
        VkSubpassDependency deps[2];
        // read?
        /*
        deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        deps[0].dstSubpass = 0;
        deps[0].srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        deps[0].dstStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
        deps[0].srcAccessMask = 0; // wiki says not necessary
        deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT; // make these available for renderpass?
        deps[0].dependencyFlags = 0;
        
        // since I sync here Vulkan should add a dependency here implicitly with dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, dstAccessMask = 0
        deps[1].srcSubpass = 0;
        deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        deps[1].srcStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT; // for this stage flush the ones in srcAccessMask
        deps[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT; 
        deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | 
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT; // flush these from cache?
        deps[1].dstAccessMask = 0;
        deps[1].dependencyFlags = 0;*/
        // read?
        
        deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        deps[0].dstSubpass = 0;
        deps[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        deps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        deps[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        deps[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        deps[1].srcSubpass = 0;
        deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        deps[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        deps[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        deps[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
        
        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.pNext = nullptr;
        renderPassInfo.flags = 0;
        renderPassInfo.attachmentCount = m_NumColorAttachments;
        renderPassInfo.pAttachments = m_Attachments;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 2;
        renderPassInfo.pDependencies = deps;

        VkResult result = vkCreateRenderPass(m_Device, &renderPassInfo, nullptr, &m_RenderPass);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
    }

    VulkanRenderPass::~VulkanRenderPass()
    {
        vkDestroyRenderPass(m_Device, m_RenderPass, nullptr);
    }

    VulkanRenderPasses::~VulkanRenderPasses()
    {
        for (auto& entry : m_Passes)
            delete entry.second;
    }
    
    VulkanRenderPass* VulkanRenderPasses::GetRenderPass(const VulkanRenderPassDesc& desc)
    {
        PassVariant key(desc);
        VulkanRenderPass* pass;
        auto iter = m_Passes.find(key);
        if (iter != m_Passes.end())
            return iter->second;

        pass = new VulkanRenderPass(*gVulkanRendererAPI().GetPresentDevice().get(), desc);
        m_Passes[key] = pass;
        return pass;
    }
    
    VulkanRenderPasses::PassVariant::PassVariant(const VulkanRenderPassDesc& desc)
        : Desc(desc)
    {

    }

    size_t VulkanRenderPasses::PassVariant::HashFunction::operator()(const PassVariant& v) const
    {
        size_t hash = 0;
        HashCombine(hash, v.Desc.Offscreen, v.Desc.Samples, v.Desc.Depth.Enabled, v.Desc.Depth.Format);

        for (uint32_t i = 0; i < sizeof(v.Desc.Color); i++)
        {
            HashCombine(hash, v.Desc.Color[i].Enabled);
            HashCombine(hash, v.Desc.Color[i].Format);
        }
        
        return hash;
    }

    bool VulkanRenderPasses::PassVariant::EqualFunction::operator()(const PassVariant& lhs, const PassVariant& rhs) const
    {
        if (lhs.Desc.Offscreen != rhs.Desc.Offscreen
            || lhs.Desc.Samples != rhs.Desc.Samples
            || lhs.Desc.Depth.Enabled != rhs.Desc.Depth.Enabled
            || lhs.Desc.Depth.Format != rhs.Desc.Depth.Format)
                return false;

        for (uint32_t i = 0; i < sizeof(lhs.Desc.Color); i++)
        {
            if (lhs.Desc.Color[i].Enabled != rhs.Desc.Color[i].Enabled || lhs.Desc.Color[i].Format != rhs.Desc.Color[i].Format)
                return false;
        }

        return true;
    }

}