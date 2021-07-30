#include "cwpch.h"

#include "Platform/Vulkan/VulkanRenderPass.h"

#include "Platform/Vulkan/VulkanRendererAPI.h"

namespace Crowny
{
    
    VulkanRenderPass::VariantKey::VariantKey(RenderSurfaceMask loadMask, RenderSurfaceMask readMask, Crowny::ClearMask clearMask)
        : LoadMask(loadMask), ReadMask(readMask), ClearMask(clearMask)
    { }

    size_t VulkanRenderPass::VariantKey::HashFunction::operator()(const VariantKey& key) const
    {
        size_t hash = 0;
        HashCombine(hash, (uint32_t)key.ReadMask, (uint32_t)key.LoadMask, (uint32_t)key.ClearMask);
        return hash;
    }

    bool VulkanRenderPass::VariantKey::EqualFunction::operator()(const VariantKey& lhs, const VariantKey& rhs) const
    {
        return lhs.LoadMask == rhs.LoadMask && lhs.ReadMask == rhs.ReadMask && lhs.ClearMask == rhs.ClearMask;
    }

    uint32_t VulkanRenderPass::s_NextValidId = 1;
    
    VulkanRenderPass::VulkanRenderPass(const VkDevice& device, const VulkanRenderPassDesc& desc) : m_Device(device)
    {
        m_Id = s_NextValidId++;
        m_SampleFlags = VulkanUtils::GetSampleFlags(desc.Samples);
        
        uint32_t idx = 0;
        for (uint32_t i = 0; i < 8; i++)
        {
            if (!desc.Color[i].Enabled)
                continue;
            
            VkAttachmentDescription& colorAttachment = m_Attachments[idx];
            colorAttachment.flags = 0;
            colorAttachment.format = desc.Color[i].Format;
            colorAttachment.samples = m_SampleFlags;
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            
            if (desc.Offscreen) // assumes shader read
                colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            else
                colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            
            VkAttachmentReference& ref = m_ColorReferences[idx];
            ref.attachment = idx;
            ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            m_Indices[idx] = i;
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
            
            VkAttachmentReference& ref = m_DepthReference;
            ref.attachment = idx;
            ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            idx++;
        }
        
        m_NumAttachments = idx;

        m_SubpassDesc.flags = 0;
        m_SubpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        m_SubpassDesc.colorAttachmentCount = m_NumColorAttachments;
        m_SubpassDesc.inputAttachmentCount = 0;
        m_SubpassDesc.pInputAttachments = nullptr;
        m_SubpassDesc.preserveAttachmentCount = 0;
        m_SubpassDesc.pPreserveAttachments = nullptr;
        m_SubpassDesc.pResolveAttachments = nullptr;
        
        if (m_NumColorAttachments > 0)
            m_SubpassDesc.pColorAttachments = m_ColorReferences;
        else
            m_SubpassDesc.pColorAttachments = nullptr;

        if (m_HasDepth)
            m_SubpassDesc.pDepthStencilAttachment = &m_DepthReference;
        else
            m_SubpassDesc.pDepthStencilAttachment = nullptr;
                
        // read?
        m_Dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        m_Dependencies[0].dstSubpass = 0;
        m_Dependencies[0].srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        m_Dependencies[0].dstStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
        m_Dependencies[0].srcAccessMask = 0; // wiki says not necessary
        m_Dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT; // make these available for renderpass?
        m_Dependencies[0].dependencyFlags = 0;
        
        m_Dependencies[1].srcSubpass = 0;
        m_Dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        m_Dependencies[1].srcStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT; // for this stage flush the ones in srcAccessMask
        m_Dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT; 
        m_Dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | 
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT; // flush these from cache?
        m_Dependencies[1].dstAccessMask = 0;
        m_Dependencies[1].dependencyFlags = 0;
        
        m_RenderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        m_RenderPassCreateInfo.pNext = nullptr;
        m_RenderPassCreateInfo.flags = 0;
        m_RenderPassCreateInfo.attachmentCount = m_NumAttachments;
        m_RenderPassCreateInfo.pAttachments = m_Attachments;
        m_RenderPassCreateInfo.subpassCount = 1;
        m_RenderPassCreateInfo.pSubpasses = &m_SubpassDesc;
        m_RenderPassCreateInfo.dependencyCount = 2;
        m_RenderPassCreateInfo.pDependencies = m_Dependencies;

        m_DefaultRenderPass = CreateVariant(RT_NONE, RT_NONE, CLEAR_NONE);
    }

    VulkanRenderPass::~VulkanRenderPass()
    {
        for (auto& entry : m_Variants)
            vkDestroyRenderPass(m_Device, entry.second, gVulkanAllocator);
    }
    
    VkRenderPass VulkanRenderPass::CreateVariant(RenderSurfaceMask loadMask, RenderSurfaceMask readMask, ClearMask clearMask) const
    {
        for (uint32_t i = 0; i < m_NumColorAttachments; i++)
        {
            VkAttachmentDescription& attachmentDesc = m_Attachments[i];
            VkAttachmentReference& attachmentRef = m_ColorReferences[i];
            uint32_t idx = m_Indices[i];
            
            if (loadMask.IsSet((RenderSurfaceMaskBits)(1 << idx)))
            {
                attachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
                attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            }
            else if (clearMask.IsSet((ClearMaskBits)(1 << idx)))
            {
                attachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            }
            else
            {
                attachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            }
            
            if (readMask.IsSet((RenderSurfaceMaskBits)(1 << idx)))
                attachmentRef.layout = VK_IMAGE_LAYOUT_GENERAL;
            else
                attachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }
        
        if (m_HasDepth)
        {
            VkAttachmentDescription& attachmentDesc = m_Attachments[m_NumColorAttachments];
            VkAttachmentReference& attachmentRef = m_DepthReference;
            
            if (loadMask.IsSet(RT_DEPTH) || loadMask.IsSet(RT_STENCIL))
            {
                attachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
                attachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
                attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            }
            else
            {
                if (clearMask.IsSet(CLEAR_DEPTH))
                    attachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                else
                    attachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                
                if (clearMask.IsSet(CLEAR_STENCIL))
                    attachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                else
                    attachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                
                attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            }
            
            if (readMask.IsSet(RT_DEPTH))
            {
                if (readMask.IsSet(RT_STENCIL))
                    attachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL_KHR;
                else
                    attachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            }
        }
        
        VkRenderPass output;
        VkResult result = vkCreateRenderPass(m_Device, &m_RenderPassCreateInfo, gVulkanAllocator, &output);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
        return output;
    }

    VkRenderPass VulkanRenderPass::GetVkRenderPass(RenderSurfaceMask loadMask, RenderSurfaceMask readMask, ClearMask clearMask) const
    {
        if (loadMask == RT_NONE && readMask == RT_NONE && clearMask == CLEAR_NONE)
            return m_DefaultRenderPass;

        VariantKey key(loadMask, readMask, clearMask);
        auto iter = m_Variants.find(key);
        if (iter != m_Variants.end())
            return iter->second;
        VkRenderPass newVariant = CreateVariant(loadMask, readMask, clearMask);
        m_Variants[key] = newVariant;
        return newVariant;
    }
    
    uint32_t VulkanRenderPass::GetNumClearEntries(ClearMask clearMask) const
    {
        if (clearMask == CLEAR_NONE)
            return 0;
        else if (clearMask == CLEAR_ALL)
            return GetNumAttachments();
        else if (((uint32_t)clearMask & (uint32_t)(CLEAR_DEPTH | CLEAR_STENCIL)) != 0 && HasDepthAttachment())
            return GetNumAttachments();
        
        uint32_t numAttachments = 0;
        for (int32_t i = MAX_FRAMEBUFFER_COLOR_ATTACHMENTS - 1; i >= 0; i--)
        {
            if (((1 << i) & (uint32_t)clearMask) != 0)
            {
                numAttachments = i + 1;
                break;
            }
        }
        return std::min(numAttachments, GetNumColorAttachments());
    }

    VulkanRenderPasses::~VulkanRenderPasses()
    {
        for (auto& entry : m_Passes)
            delete entry.second;
    }
    
    VulkanRenderPass* VulkanRenderPasses::GetRenderPass(const VulkanRenderPassDesc& desc) const
    {
        PassVariant key(desc);
        VulkanRenderPass* pass;
        auto iter = m_Passes.find(key);
        if (iter != m_Passes.end())
            return iter->second;

        pass = new VulkanRenderPass(gVulkanRendererAPI().GetPresentDevice()->GetLogicalDevice(), desc);
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

        for (uint32_t i = 0; i < MAX_FRAMEBUFFER_COLOR_ATTACHMENTS; i++)
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

        for (uint32_t i = 0; i < MAX_FRAMEBUFFER_COLOR_ATTACHMENTS; i++)
        {
            if (lhs.Desc.Color[i].Enabled != rhs.Desc.Color[i].Enabled || lhs.Desc.Color[i].Format != rhs.Desc.Color[i].Format)
                return false;
        }
        
        return true;
    }

}