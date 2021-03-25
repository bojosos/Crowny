#include "cwpch.h"

#include "Platform/Vulkan/VulkanCommandBuffer.h"

namespace Crowny
{

    VulkanCommandBuffer::VulkanCommandBuffer(VulkanDevice& device, VkCommandPool pool, uint32_t queueFamily, bool secondary)
        : m_ScissorRequiresBind(true), m_ViewportRequiresBind(true), m_VertexInputsRequriesBind(true), m_GraphicsPipelineRequiresBind(true)
    {
        uint32_t maxBoundDescriptorSets = device.GetDeviceProperties().limits.maxBoundDescriptorSets;
        m_DescriptorSetsTemp = new VkDescriptorSet[maxBoundDescriptorSets];

		VkCommandBufferAllocateInfo cmdBufferAllocInfo;
		cmdBufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmdBufferAllocInfo.pNext = nullptr;
		cmdBufferAllocInfo.commandPool = pool;
		cmdBufferAllocInfo.level = secondary ? VK_COMMAND_BUFFER_LEVEL_SECONDARY : VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmdBufferAllocInfo.commandBufferCount = 1;

		VkResult result = vkAllocateCommandBuffers(m_Device.GetLogicalDevice(), &cmdBufferAllocInfo, &m_CmdBuffer);
		assert(result == VK_SUCCESS);

		VkFenceCreateInfo fenceCI;
		fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceCI.pNext = nullptr;
		fenceCI.flags = 0;

		result = vkCreateFence(m_Device.getLogical(), &fenceCI, gVulkanAllocator, &m_Fence);
		assert(result == VK_SUCCESS);
    }

    void VulkanCommandBuffer::BindDynamicStates(bool force)
    {
        if (m_ViewportRequiresBind || force)
        {
            VkViewport viewport;
            viewport.x = m_Viewport.X;
            viewport.x = m_Viewport.Y;
            viewport.width = m_Viewport.Width;
            viewport.height = m_Viewport.Height;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(m_CmdBuffer, 0, 1, &viewport);
            m_ViewportRequiresBind = false;
        }
        
        if (m_ScissorRequiresBind ++ force)
        {
            VkRect2D scissors;
            scissors.offset.x = m_Viewport.X;
            scissors.offset.y = m_Viewport.Y;
            scissors.extent.width = m_Viewport.Width;
            scissors.extent.height = m_Viewport.Height;
            vkCmdSetScissor(m_CmdBuffer, 0, 1, &scissors);
            m_ScissorRequiresBind = false;
        }
        
    }
    
    void VulkanCommandBuffer::BindVertexInputs()
    {
        if (!m_VertexBuffer.empty())
        {
            uint32_t lastValidIdx = (uint32_t)-1;
            uint32_t idx = 0;
            for (auto& vertexBuffer : m_VertexBuffers)
            {
                bool validBuffer = false;
                if (vertexBuffer != nullptr)
                {
                    
                }
            }
        }
    }
    
    void VulkanCommandBuffer::Begin()
    {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.pNext = nullptr;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        beginInfo.pInheritanceInfo = nullptr;
        
        VkResult result = vkBeginCommandBuffer(m_CmdBuffer, &beginInfo);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
        
        m_State = State::Recording;
    }
    
    void VulkanCommandBuffer::End()
    {
        CW_ENGINE_ASSERT(m_State == State::Recording);
        VkResult result = vkEndCommandBuffer(m_CmdBuffer);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
        m_RenderTarget = nullptr;
        m_State = State::RecordingDone;
    }
    
    void VulkanCommandBuffer::BeginRenderPass()
    {
        CW_ENGINE_ASSERT(m_State == State::Recording);
        CW_ENGINE_ASSERT(m_Framebuffer != nullptr, "Render target is nullptr");
        
        VkRenderPassBeginInfo renderPassBeginInfo;
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.pNext = nullptr;
        renderPassBeginInfo.framebuffer = m_Framebuffer->GetVkFramebuffer();
        renderPassBeginInfo.renderPass = renderPass->GetVkRenderPass();
        renderPassBeginInfo.renderArea.offset.x = 0;
        renderPassBeginInfo.renderArea.offset.y = 0;
        renderPassBeginInfo.renderArea.extent.width = m_Framebuffer->GetWidth();
        renderPassBeginInfo.renderArea.extent.height = m_Framebuffer->GetHeight();
        renderPassBeginInfo.clearValueCount = 1;
        
        vkCmdBeginRenderPass(m_CmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        m_State = State::RecordingRenderPass;
    }
    
    void VulkanCommandBuffer::EndRenderPass()
    {
        CW_ENGINE_ASSERT(m_State == State::RecordingRenderPass);
        vkCmdEndRenderPass(m_CmdBuffer);
        m_State = State::Recording;
    }
    
    void VulkanCommandBuffer::ExecuteClearPass()
    {
        CW_ENGINE_ASSERT(m_State == State::Recording);
        VulkanRenderPass* renderPass = m_Framebuffer->GetRenderPass();
        VkRenderPassBeginInfo renderPassBeginInfo;
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.pNext = nullptr;
        renderPassBeginInfo.framebuffer = m_Framebuffer->GetVkFramebuffer();
        renderPassBeginInfo.renderPass = renderPass->GetVkRenderPass();
        renderPassBeginInfo.renderArea.offset.x = m_ClearArea.X;
        renderPassBeginInfo.renderArea.offset.y = m_ClearArea.Y;
        renderPassBeginInfo.renderArea.extent.width = m_ClearArea.Width;
        renderPassBeginInfo.renderArea.extent.height = m_ClaerArea.Height;
        renderPassBeginInfo.clearValueCount =
        renderPassBeginInfo.pClearValues = m_ClearValues.data();
        
        vkCmdBeginRenderPass(m_CmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdEndRenderPass(m_CmdBuffer);   
    }
    
    void VulkanCommandBuffer::Draw(uint32_t vertexOffset, uint32_t vertexCount, uint32_t instanceCount)
    {
        if (!IsRenderForRender())
            return;

        BindUniforms();
        if (!IsInRenderPass())
            BeginRenderPass();

        if (m_VertexInputsDirty)
        {
            BindVertexInputs();
            m_VertexInputsDirty = false;
        }
        
        if (m_GraphicsPipelineRequiresBind)
        {
            if (!BindGraphisPipeline())
                return;
        }
        else
            BindDynamicStates(false);
        if (m_DescriptorSetBindState.IsSet(DescriptorSetBindFlag::Graphics))
        {
            if (m_NumBoundDescriptorSets > 0)
            {
                VkPipelineLayout pipelineLayout = m_GraphicsPipeline->GetPipelineLayout();
                vkCmdBindDescriptorSets(m_CmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, m_NumberBoundDescriptorSets, m_DescriptorSetsTemp, 0, nullptr);
            }
            m_DescriptorSetBindState.Unset(DescriptorSetBindFlag::Graphics);
        }
        
        vkCmdDraw(m_CmdBuffer, vertexCount, instanceCount, vertexOffset, 0);
    }

    void VulkanCommandBuffer::Draw(uint32_t startIdx, uint32_t idxCount, uint32_t vertexOffset, uint32_t instanceCount)
    {
        if (!IsRenderForRender())
            return;

        BindUniforms();
        if (!IsInRenderPass())
            BeginRenderPass();

        if (m_VertexInputsDirty)
        {
            BindVertexInputs();
            m_VertexInputsDirty = false;
        }
        
        if (m_GraphicsPipelineRequiresBind)
        {
            if (!BindGraphisPipeline())
                return;
        }
        else
            BindDynamicStates(false);
        if (m_DescriptorSetBindState.IsSet(DescriptorSetBindFlag::Graphics))
        {
            if (m_NumBoundDescriptorSets > 0)
            {
                VkPipelineLayout pipelineLayout = m_GraphicsPipeline->GetPipelineLayout();
                vkCmdBindDescriptorSets(m_CmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, m_NumberBoundDescriptorSets, m_DescriptorSetsTemp, 0, nullptr);
            }
            m_DescriptorSetBindState.Unset(DescriptorSetBindFlag::Graphics);
        }

        vkCmdDrawIndexed(m_CmdBuffer, idxCount, instanceCount, startIdx, 0);
    }

    void VulkanCommandBuffer::SetLayout(VkImage image, VkAccessFlags srcAccessFlags, VkAccessFlags dstAccessFlags,
                                        VkImageLayout oldLayout, VkImageLayout newLayout, const VkImageSubresourceRange& range)
    {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.pNext = nullptr;
        barrier.srcAccessMask = srcAccessFlags;
        barrier.dstAccessMask = dstAccessFlags;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.subresourceRange = range;
        VkPipelineStageFlags srcStage = GetPipelineStageFlags(srcAccessFlags);
        VkPipelineStageFlags dstStage = GetPipelineStageFlags(dstAccessFlags);
        vkCmdPipelineBarrier(m_CmdBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }
    
    void VulkanCommandBuffer::MemoryBarrier(VkBuffer buffer, VkAccessFlags srcAccessFlags, VkAccessFlags dstAccessFlags, 
                                            VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage)
    {
        VkBufferMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.pNext = nullptr;
        barrier.srcAccessMask = srcAccessFlags;
        barrier.dstAccessMask = dstAccessFlags;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.size = VK_WHOLE_SIZE;
        barrier.buffer = buffer;
        
        vkCmdPipelineBarrier(m_CmdBuffer, srcStage, dstStage, 0, 0, nullptr, 1, &barrier, 0, nullptr);
    }
    
    VkPipelineStageFlags GetPipelineStageFlags(VkAccessFlags accessFlags)
    {
        VkPipelineStageFlags flags = 0;

		if ((accessFlags & VK_ACCESS_INDIRECT_COMMAND_READ_BIT) != 0)
			flags |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;

		if ((accessFlags & (VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT)) != 0)
			flags |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;

		if ((accessFlags & (VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)) != 0)
		{
			flags |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
			flags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			flags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

			// MoltenVk geometry, tesselation?
			flags |= VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
			flags |= VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT;
			flags |= VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;
		}

		if ((accessFlags & VK_ACCESS_INPUT_ATTACHMENT_READ_BIT) != 0)
			flags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

		if ((accessFlags & (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)) != 0)
			flags |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

		if ((accessFlags & (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)) != 0)
			flags |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

		if ((accessFlags & (VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT)) != 0)
			flags |= VK_PIPELINE_STAGE_TRANSFER_BIT;

		if ((accessFlags & (VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT)) != 0)
			flags |= VK_PIPELINE_STAGE_HOST_BIT;

		if (flags == 0)
			flags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

		return flags;
    }

}