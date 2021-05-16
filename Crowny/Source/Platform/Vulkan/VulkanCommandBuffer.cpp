#include "cwpch.h"

#include "Platform/Vulkan/VulkanCommandBuffer.h"
#include "Platform/Vulkan/VulkanRendererAPI.h"
#include "Platform/Vulkan/VulkanRenderPass.h"
#include "Platform/Vulkan/VulkanVertexBuffer.h"
#include "Platform/Vulkan/VulkanIndexBuffer.h"

#include "Crowny/Common/Timer.h"

namespace Crowny
{

    VulkanCmdBuffer::VulkanCmdBuffer(GpuQueueType queueType)
    {
        const VulkanDevice& device = *gVulkanRendererAPI().GetPresentDevice().get();
        uint32_t numQueues = device.GetNumQueues(queueType);
        if (numQueues == 0)
        {
            queueType = GRAPHICS_QUEUE;
            numQueues = device.GetNumQueues(GRAPHICS_QUEUE);
        }

        //m_Buffer = new VulkanCommandBuffer(device.GetQueue(queueType));
        VulkanCommandBufferPool& pool = device.GetCmdBufferPool();
        uint32_t queueFamily = device.GetQueueFamily(queueType);
        m_Buffer = pool.GetBuffer(queueFamily, false);
        m_Buffer->m_Queue = device.GetQueue(queueType, 0);
    }

    VulkanCmdBuffer::~VulkanCmdBuffer()
    {
        //delete m_Buffer;
    }

    VulkanSemaphore::VulkanSemaphore()
    {
        VkDevice device = gVulkanRendererAPI().GetPresentDevice()->GetLogicalDevice();
        VkSemaphoreCreateInfo semaphoreCreateInfo;
        semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semaphoreCreateInfo.pNext = nullptr;
        semaphoreCreateInfo.flags = 0;
        VkResult result = vkCreateSemaphore(device, &semaphoreCreateInfo, gVulkanAllocator, &m_Semaphore);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
    }

    VulkanSemaphore::~VulkanSemaphore()
    {
        VkDevice device = gVulkanRendererAPI().GetPresentDevice()->GetLogicalDevice();
        vkDestroySemaphore(device, m_Semaphore, gVulkanAllocator);
    }
    
    VulkanTransferBuffer::VulkanTransferBuffer(VulkanDevice* device, GpuQueueType type, uint32_t queueIdx) : m_Device(device), m_Type(type), m_QueueIdx(queueIdx)
    {
        uint32_t numQueues = device->GetNumQueues(type);
        if (numQueues == 0)
        {
            m_Type = GRAPHICS_QUEUE;
            numQueues = device->GetNumQueues(m_Type);
        }

        uint32_t physicalQueueIdx = queueIdx % numQueues;
        m_Queue = device->GetQueue(m_Type, physicalQueueIdx);
        m_QueueMask = device->GetQueueMask(m_Type, queueIdx);
    }

    VulkanTransferBuffer::~VulkanTransferBuffer()
    {
        if (m_CommandBuffer != nullptr)
            m_CommandBuffer->End();
    }

    void VulkanTransferBuffer::MemoryBarrier(VkBuffer buffer, VkAccessFlags srcAccessFlags, VkAccessFlags dstAccessFlags, 
                                            VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage)
    {
        m_CommandBuffer->MemoryBarrier(buffer, srcAccessFlags, dstAccessFlags, srcStage, dstStage);
    }

    void VulkanTransferBuffer::Allocate()
    {
        if (m_CommandBuffer != nullptr)
            return;
        
        uint32_t queueFamily = m_Device->GetQueueFamily(m_Type);
        m_CommandBuffer = (new VulkanCmdBuffer(m_Type))->GetBuffer();
    }

    void VulkanTransferBuffer::Flush(bool wait)
    {
        if (m_CommandBuffer == nullptr)
            return;

        m_CommandBuffer->End();
        m_CommandBuffer->Submit(true);
        if (wait)
        {
            m_Queue->WaitIdle();
            m_Device->Refresh();
        }
        m_CommandBuffer = nullptr;
    }
    
    VulkanTransferManager::VulkanTransferManager()
    {
        VulkanDevice* device = gVulkanRendererAPI().GetPresentDevice().get();
        for (uint32_t i = 0; i < QUEUE_COUNT; i++)
        {
            GpuQueueType queueType = (GpuQueueType)i;
            for (uint32_t j = 0; j < MAX_QUEUES_PER_TYPE; j++)
                m_TransferBuffers[i][j] = VulkanTransferBuffer(device, queueType, j);
        }
    }

    VulkanTransferBuffer* VulkanTransferManager::GetTransferBuffer(GpuQueueType queueType, uint32_t queueIdx)
    {
        VulkanTransferBuffer* buffer = &m_TransferBuffers[queueType][queueIdx];
        buffer->Allocate();
        return buffer;
    }
    
    void VulkanTransferManager::FlushTransferBuffers()
    {
        for (uint32_t i = 0; i < QUEUE_COUNT; i++)
        {
            for (uint32_t j = 0; j < MAX_QUEUES_PER_TYPE; j++)
            m_TransferBuffers[i][j].Flush(false);
        }
    }

    VulkanCommandBufferPool::VulkanCommandBufferPool(VulkanDevice& device) : m_Device(device)
    {
        for (uint32_t i = 0; i < QUEUE_COUNT; i++)
        {
            uint32_t familyIdx = device.GetQueueFamily((GpuQueueType)i);
            if (familyIdx == (uint32_t)-1)
                continue;
            VkCommandPoolCreateInfo poolCreateInfo;
            poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            poolCreateInfo.pNext = nullptr;
            poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            poolCreateInfo.queueFamilyIndex = familyIdx;
            m_Pools[familyIdx].QueueFamily = familyIdx;
            memset(m_Pools[familyIdx].Buffers, 0, sizeof(m_Pools[familyIdx].Buffers));
            vkCreateCommandPool(device.GetLogicalDevice(), &poolCreateInfo, gVulkanAllocator, &m_Pools[familyIdx].Pool);
        }
    }

    VulkanCommandBufferPool::~VulkanCommandBufferPool()
    {
        for (auto& entry : m_Pools)
        {
            PoolInfo& info = entry.second;
            for (uint32_t i = 0; i < MAX_VULKAN_CB_PER_QUEUE_FAMILY; i++)
            {
                VulkanCommandBuffer* buffer = info.Buffers[i];
                if (buffer == nullptr)
                    break;
                delete buffer;
            }
            vkDestroyCommandPool(m_Device.GetLogicalDevice(), info.Pool, gVulkanAllocator);
        }
    }
    
    VulkanCommandBuffer* VulkanCommandBufferPool::GetBuffer(uint32_t queueFamily, bool secondary)
    {
        auto iter = m_Pools.find(queueFamily);
        if (iter == m_Pools.end())
            return nullptr;
        VulkanCommandBuffer** buffers = iter->second.Buffers;
        
        uint32_t i = 0;
        for (; i < MAX_VULKAN_CB_PER_QUEUE_FAMILY; i++)
        {
            if (buffers[i] == nullptr)
                break;
            if (buffers[i]->m_State == VulkanCommandBuffer::State::Ready)
            {
                buffers[i]->Begin();
                return buffers[i];
            }
        }
        
        CW_ENGINE_ASSERT(i < MAX_VULKAN_CB_PER_QUEUE_FAMILY, "Too many command buffers allocated.");
        
        buffers[i] = CreateBuffer(queueFamily, secondary);
        buffers[i]->Begin();
        
        return buffers[i];
    }
    
    VulkanCommandBuffer* VulkanCommandBufferPool::CreateBuffer(uint32_t queueFamily, bool secondary)
    {
        auto iter = m_Pools.find(queueFamily);
        if (iter == m_Pools.end())
            return nullptr;
        const PoolInfo& poolInfo = iter->second;
        return new VulkanCommandBuffer(m_Device, m_NextId++, poolInfo.Pool, poolInfo.QueueFamily, secondary);
    }
    
    VulkanCommandBuffer::VulkanCommandBuffer(VulkanDevice& device, uint32_t id, VkCommandPool pool, uint32_t queueFamily, bool secondary)
        : m_ScissorRequiresBind(true), m_ViewportRequiresBind(true), m_VertexInputsRequriesBind(true), m_GraphicsPipelineRequiresBind(true),
          m_Id(id), m_QueueFamily(queueFamily), m_Device(device), m_Pool(pool), m_ComputePipelineRequiresBind(true)
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

		result = vkCreateFence(m_Device.GetLogicalDevice(), &fenceCI, gVulkanAllocator, &m_Fence);
        vkResetFences(m_Device.GetLogicalDevice(), 1, &m_Fence);
		CW_ENGINE_ASSERT(result == VK_SUCCESS);
        m_SwapChain = gVulkanRendererAPI().GetSwapChain();
        m_Semaphore = new VulkanSemaphore();
    }

    VulkanCommandBuffer::~VulkanCommandBuffer()
    {
        VkDevice device = m_Device.GetLogicalDevice();
        
        if (m_State == State::Submitted)
        {
            uint64_t wait = 1000 * 1000 * 1000;
            VkResult result = vkWaitForFences(device, 1, &m_Fence, true, wait);
            CW_ENGINE_ASSERT(result == VK_SUCCESS || result == VK_TIMEOUT);

            if (result == VK_TIMEOUT)
                CW_ENGINE_WARN("Command buffer freed, fence wait expired!");
            Reset();
        }
        
        vkDestroyFence(device, m_Fence, gVulkanAllocator);
        vkFreeCommandBuffers(device, m_Pool, 1, &m_CmdBuffer);
        delete m_Semaphore;
      //  delete m_DesciptorSetsTemp;
    }

    bool VulkanCommandBuffer::BindGraphicsPipeline()
    {
        VulkanRenderPass* renderPass = m_RenderTarget->GetRenderPass();        
        VulkanPipeline* pipeline = m_GraphicsPipeline->GetPipeline(renderPass, m_DrawMode);
        // get pipeline using the vertex layout and renderpass here, make sure that the flags are correct... not just this
        vkCmdBindPipeline(m_CmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetHandle());
        BindDynamicStates(true);
        m_GraphicsPipelineRequiresBind = false;
        return true;
    }

    void VulkanCommandBuffer::SetViewport(const Rect2F& rect)
    {
        if (m_Viewport == rect)
            return;
        m_Viewport = rect;
        m_ViewportRequiresBind = true;
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
        
        if (m_ScissorRequiresBind || force)
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

    void VulkanCommandBuffer::SetPipeline(const Ref<GraphicsPipeline>& pipeline)
    {
        if (m_GraphicsPipeline == pipeline)
            return;
        m_GraphicsPipeline = std::static_pointer_cast<VulkanGraphicsPipeline>(pipeline);
        m_GraphicsPipelineRequiresBind = true;
    }
    
    void VulkanCommandBuffer::SetPipeline(const Ref<ComputePipeline>& pipeline)
    {
        if (m_ComputePipeline == pipeline)
            return;
        
        m_ComputePipeline = std::static_pointer_cast<VulkanComputePipeline>(pipeline);
        m_ComputePipelineRequiresBind = true;
    }

    CommandBufferState VulkanCommandBuffer::GetState() const
    {
        if (IsSubmitted())
            return CheckFenceStatus(false) ? CommandBufferState::Done : CommandBufferState::Executing;

        bool recording = IsRecording() || IsReadyForSubmit() || IsInRenderPass();
        return recording ? CommandBufferState::Recording : CommandBufferState::Empty;
    }
    
    void VulkanCommandBuffer::BindVertexInputs()
    {
        if (!m_VertexBuffers.empty())
        {
            uint32_t lastValidIdx = (uint32_t)-1;
            uint32_t idx = 0;
            for (auto& vertexBuffer : m_VertexBuffers)
            {
                bool validBuffer = false;
                if (vertexBuffer != nullptr)
                {
                    m_VertexBuffersTemp[idx] = vertexBuffer->GetHandle();// gethandle
                    // registerbuffer
                    if (lastValidIdx == (uint32_t)-1)
                        lastValidIdx = idx;
                    validBuffer = true;
                }

                if (!validBuffer && lastValidIdx != (uint32_t)-1)
                {
                    uint32_t count = idx - lastValidIdx;
                    if (count > 0)
                    {
                        vkCmdBindVertexBuffers(m_CmdBuffer, lastValidIdx, count, m_VertexBuffersTemp, m_VertexBufferOffsets);
                        lastValidIdx = (uint32_t)-1;
                    }
                }
                idx++;
            }

            if (lastValidIdx != (uint32_t)-1)
            {
                uint32_t count = idx - lastValidIdx;
                if (count > 0)
                {
                    vkCmdBindVertexBuffers(m_CmdBuffer, lastValidIdx, count, m_VertexBuffersTemp, m_VertexBufferOffsets);
                }
            }
        }

        if (m_IndexBuffer != nullptr)
        {
            VkBuffer vkBuffer = m_IndexBuffer->GetHandle();
            vkCmdBindIndexBuffer(m_CmdBuffer, vkBuffer, 0, VulkanUtils::GetIndexType(m_IndexBuffer->GetIndexType()));
        }
    }
    
    void VulkanCommandBuffer::Begin()
    {
        CW_ENGINE_ASSERT(m_State == State::Ready);
        
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
        CW_ENGINE_ASSERT(m_RenderTarget != nullptr, "Render target is nullptr");
        
        // TODO: layout transitions, barriers
        VkClearValue clearValue;
        clearValue.color = { 0.0f, 0.0f, 0.0f, 1.0f };

        VkRenderPassBeginInfo renderPassBeginInfo;
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.pNext = nullptr;
        renderPassBeginInfo.framebuffer = m_RenderTarget->GetHandle();
        renderPassBeginInfo.renderPass = m_RenderTarget->GetRenderPass()->GetHandle();
        renderPassBeginInfo.renderArea.offset.x = 0;
        renderPassBeginInfo.renderArea.offset.y = 0;
        renderPassBeginInfo.renderArea.extent.width = m_RenderTarget->GetWidth();
        renderPassBeginInfo.renderArea.extent.height = m_RenderTarget->GetHeight();
        renderPassBeginInfo.clearValueCount = 1;
        renderPassBeginInfo.pClearValues = &clearValue;
        
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
        VulkanRenderPass* renderPass = m_RenderTarget->GetRenderPass();
        VkRenderPassBeginInfo renderPassBeginInfo;
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.pNext = nullptr;
        renderPassBeginInfo.framebuffer = m_RenderTarget->GetHandle();
        renderPassBeginInfo.renderPass = renderPass->GetHandle();
        renderPassBeginInfo.renderArea.offset.x = m_ClearArea.X;
        renderPassBeginInfo.renderArea.offset.y = m_ClearArea.Y;
        renderPassBeginInfo.renderArea.extent.width = m_ClearArea.Width;
        renderPassBeginInfo.renderArea.extent.height = m_ClearArea.Height;
        renderPassBeginInfo.clearValueCount = 5;
        renderPassBeginInfo.pClearValues = m_ClearValues.data();
        
        vkCmdBeginRenderPass(m_CmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdEndRenderPass(m_CmdBuffer);   
    }
    
    void VulkanCommandBuffer::Reset()
    {
        bool submitted = m_State == State::Submitted;

        m_State = State::Ready;
        // maybe dont reset the cb itself only the state
        vkResetCommandBuffer(m_CmdBuffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
    }
    
    void VulkanCommandBuffer::Submit(bool transfer)
    {
        if (IsInRenderPass())
            EndRenderPass();
        
        if (IsRecording())
            End();
        
        if (IsReadyForSubmit())
        {
            VkResult result = vkResetFences(m_Device.GetLogicalDevice(), 1, &m_Fence);
            CW_ENGINE_ASSERT(result == VK_SUCCESS);

            if (!transfer)
            {
                const SwapChainSurface& surface = m_SwapChain->GetBackBuffer();
                VulkanSemaphore* semaphore = { surface.Sync };
                m_Queue->Submit(this, &semaphore, 1);
                m_SwapChain->BackBufferWaitIssued();
            }
            else
            {
            CW_ENGINE_INFO("TRANSFER");
                m_Queue->Submit(this, nullptr, 0);
            }
            m_GraphicsPipeline = nullptr;
            m_ComputePipeline = nullptr;
            m_GraphicsPipelineRequiresBind = true;
            m_ComputePipelineRequiresBind = true;
            m_RenderTarget = nullptr;
            //m_DescriptorSetsBindState = DescriptorSetBindFlag::Graphics | DescriptorSetBindFlag::Compute;
            //m_IndexBuffer = nullptr;
            m_VertexBuffers.clear();
            //m_VertexInputsRequiresBind = true;
            //m_ActiveSwapChains.clear();
        }
    }
    
    void VulkanCommandBuffer::SetRenderTarget(const Ref<Framebuffer>& framebuffer)
    {
        CW_ENGINE_ASSERT(m_State != State::Submitted);/*
        
        if (framebuffer != nullptr)
        {
           // m_RenderTarget = std::static_pointer_cast<VulkanFramebuffer>(framebuffer);
            m_RenderTargetModified = true;
            m_GraphicsPipelineRequiresBind = true;
        }*/
        if (IsInRenderPass())
            EndRenderPass();
        m_SwapChain->AcquireBackBuffer();
        m_RenderTarget = m_SwapChain->GetBackBuffer().Framebuffer;
    }
    
    bool VulkanCommandBuffer::CheckFenceStatus(bool blocking) const
    {
        VkResult result = vkWaitForFences(m_Device.GetLogicalDevice(), 1, &m_Fence, true, blocking ? 1000000000 : 0);
        CW_ENGINE_ASSERT(result == VK_SUCCESS || result == VK_TIMEOUT);
        
        return result == VK_SUCCESS;
    }
    
    bool VulkanCommandBuffer::IsReadyForRender() const
    {
        if (m_GraphicsPipeline == nullptr)
            return false;

        return m_RenderTarget != nullptr;// && mVertexDecl != nullptr;
    }

    void VulkanCommandBuffer::BindUniforms()
    {

    }

    void VulkanCommandBuffer::SetDrawMode(DrawMode drawMode)
    {
        if (m_DrawMode == drawMode)
            return;
        m_DrawMode = drawMode;
        m_GraphicsPipelineRequiresBind = true;
    }

    void VulkanCommandBuffer::SetVertexBuffers(uint32_t idx, Ref<VertexBuffer>* buffers, uint32_t bufferCount)
    {
        if (bufferCount == 0)
            return;
        uint32_t endIdx = idx + bufferCount;
        if (m_VertexBuffers.size() < endIdx)
            m_VertexBuffers.resize(endIdx);
        for (uint32_t i = idx; i < endIdx; i++)
        {
            m_VertexBuffers[i] = std::static_pointer_cast<VulkanVertexBuffer>(buffers[i]);
        }
        m_VertexInputsRequriesBind = true;
    }

    void VulkanCommandBuffer::SetIndexBuffer(const Ref<IndexBuffer>& indexBuffer)
    {
        m_IndexBuffer = std::static_pointer_cast<VulkanIndexBuffer>(indexBuffer);
        m_VertexInputsRequriesBind = true;
    }

    void VulkanCommandBuffer::Draw(uint32_t vertexOffset, uint32_t vertexCount, uint32_t instanceCount)
    {
        if (!IsReadyForRender())
            return;

        BindUniforms();
        if (!IsInRenderPass())
            BeginRenderPass();

        if (m_VertexInputsRequriesBind)
        {
            BindVertexInputs();
            m_VertexInputsRequriesBind = false;
        }
        
        if (m_GraphicsPipelineRequiresBind)
        {
            if (!BindGraphicsPipeline())
                return;
        }
        else
            BindDynamicStates(false);
            /*
        if (m_DescriptorSetBindState.IsSet(DescriptorSetBindFlag::Graphics))
        {
            if (m_NumBoundDescriptorSets > 0)
            {
                VkPipelineLayout pipelineLayout = m_GraphicsPipeline->GetPipelineLayout();
                vkCmdBindDescriptorSets(m_CmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, m_NumberBoundDescriptorSets, m_DescriptorSetsTemp, 0, nullptr);
            }
            m_DescriptorSetBindState.Unset(DescriptorSetBindFlag::Graphics);
        }*/
        if (instanceCount <= 0)
            instanceCount = 1;
        vkCmdDraw(m_CmdBuffer, vertexCount, instanceCount, vertexOffset, 0);
    }

    void VulkanCommandBuffer::DrawIndexed(uint32_t startIdx, uint32_t idxCount, uint32_t vertexOffset, uint32_t instanceCount)
    {
        if (!IsReadyForRender())
            return;

        BindUniforms();
        if (!IsInRenderPass())
            BeginRenderPass();

        if (m_VertexInputsRequriesBind)
        {
            BindVertexInputs();
            m_VertexInputsRequriesBind = false;
        }
        
        if (m_GraphicsPipelineRequiresBind)
        {
            if (!BindGraphicsPipeline())
                return;
        }
        else
            BindDynamicStates(false);
      /*  if (m_DescriptorSetBindState.IsSet(DescriptorSetBindFlag::Graphics))
        {
            if (m_NumBoundDescriptorSets > 0)
            {
                VkPipelineLayout pipelineLayout = m_GraphicsPipeline->GetPipelineLayout();
                vkCmdBindDescriptorSets(m_CmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, m_NumberBoundDescriptorSets, m_DescriptorSetsTemp, 0, nullptr);
            }
            m_DescriptorSetBindState.Unset(DescriptorSetBindFlag::Graphics);
        }*/
        if (instanceCount <= 0)
            instanceCount = 1;
        vkCmdDrawIndexed(m_CmdBuffer, idxCount, instanceCount, startIdx, vertexOffset, 0);
    }

    VkSemaphore VulkanCommandBuffer::AllocateSemaphores(VkSemaphore* semaphores)
    {
        //semaphores = &(m_Semaphore->m_Semaphore);
        return m_Semaphore->GetHandle();
        /*
        if (m_IntraQueueSemaphore != nullptr)
            delete m_IntraQueueSemaphore;

        m_IntraQueueSemaphore = new VulkanSemaphore();
        semaphores[0] = m_IntraQueueSemaphore->GetHandle();

        for (uint32_t i = 0; i < MAX_VULKAN_CB_DEPENDENCIES; i++)
        {
            if (m_InterQueueSemaphores[i] != nullptr)
                delete m_InterQueueSemaphores[i];
            m_InterQueueSemaphores[i] = new VulkanSemaphore();
            semaphores[i + 1] = m_InterQueueSemaphores[i]->GetHandle();
        }

        m_NumUsedInterQueueSemaphores = 0;*/
    }
    /*
    VulkanSemaphore* VulkanCommandBuffer::RequestInterQueueSemaphore() const
    {
       // if (m_NumUsedInterQueueSemaphores >= MAX_VULKAN_CB_DEPENDENCIES)
       //     return nullptr;
        //return m_InterQueueSemaphores[m_NumUsedInterQueueSemaphores++];
    }*/

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

}
