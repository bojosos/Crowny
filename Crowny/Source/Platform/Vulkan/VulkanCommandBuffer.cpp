#include "cwpch.h"

#include "Platform/Vulkan/VulkanCommandBuffer.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanFramebuffer.h"
#include "Platform/Vulkan/VulkanIndexBuffer.h"
#include "Platform/Vulkan/VulkanPipeline.h"
#include "Platform/Vulkan/VulkanQuery.h"
#include "Platform/Vulkan/VulkanQueue.h"
#include "Platform/Vulkan/VulkanRenderAPI.h"
#include "Platform/Vulkan/VulkanRenderPass.h"
#include "Platform/Vulkan/VulkanRenderTexture.h"
#include "Platform/Vulkan/VulkanRenderWindow.h"
#include "Platform/Vulkan/VulkanSwapChain.h"
#include "Platform/Vulkan/VulkanUniformParams.h"
#include "Platform/Vulkan/VulkanVertexBuffer.h"

#include "Crowny/Common/Timer.h"

namespace Crowny
{

    VulkanCommandBuffer::VulkanCommandBuffer(VulkanDevice& device, GpuQueueType queueType, uint32_t queueIdx,
                                             bool secondary)
      : CommandBuffer(queueType, queueIdx, secondary), m_Queue(nullptr), m_IdMask(0), m_Device(device)
    {
        uint32_t numQueues = device.GetNumQueues(queueType);
        if (numQueues == 0)
        {
            queueType = GRAPHICS_QUEUE;
            numQueues = device.GetNumQueues(GRAPHICS_QUEUE);
        }

        m_Queue = device.GetQueue(queueType, m_QueueIdx % numQueues);
        m_IdMask = device.GetQueueMask(queueType, m_QueueIdx);
        AcquireNewBuffer();
    }

    void VulkanCommandBuffer::AcquireNewBuffer()
    {
        VulkanCommandBufferPool& pool = m_Device.GetCmdBufferPool();
        if (m_Buffer != nullptr)
            CW_ENGINE_ASSERT(m_Buffer->IsSubmitted());

        uint32_t queueFamily = m_Device.GetQueueFamily(m_Type);
        m_Buffer = pool.GetBuffer(queueFamily, m_IsSecondary);
    }

    CommandBufferState VulkanCommandBuffer::GetState() const
    {
        if (m_Buffer->IsSubmitted())
            return m_Buffer->CheckFenceStatus(false) ? CommandBufferState::Done : CommandBufferState::Executing;
        bool recording = m_Buffer->IsRecording() || m_Buffer->IsReadyForSubmit() || m_Buffer->IsInRenderPass();
        return recording ? CommandBufferState::Recording : CommandBufferState::Empty;
    }

    void VulkanCommandBuffer::Reset() { AcquireNewBuffer(); }

    void VulkanCommandBuffer::Submit(uint32_t syncMask)
    {
        if (GetState() == CommandBufferState::Executing)
        {
            CW_ENGINE_ERROR("Cannot submit command buffer that's still executing");
            return;
        }

        syncMask &= ~m_IdMask;
        if (m_Buffer->IsInRenderPass())
            m_Buffer->EndRenderPass();
        m_Buffer->ExecuteLayoutTransitions();

        Vector<VulkanTimerQuery*> timerQueries;
        Vector<VulkanPipelineQuery*> pipelineQueries;
        Vector<VulkanOcclusionQuery*> occlusionQueries;

        m_Buffer->GetInProgressQueries(timerQueries, pipelineQueries, occlusionQueries);
        if (!timerQueries.empty() || !occlusionQueries.empty())
        {
            CW_ENGINE_WARN("Submitting a command buffer with {0} timer queries, {1} pipeline queries and {2} occlusion "
                           "queries in progress. They will be closed automatically.",
                           timerQueries.size(), pipelineQueries.size(), occlusionQueries.size());
            for (auto entry : timerQueries)
                entry->Interrupt(m_Buffer);
            for (auto entry : pipelineQueries)
                entry->Interrupt(m_Buffer);
            for (auto entry : occlusionQueries)
                entry->Interrupt(m_Buffer);
        }

        if (m_Buffer->IsRecording())
            m_Buffer->End();
        if (m_Buffer->IsReadyForSubmit())
        {
            m_Buffer->Submit(m_Queue, m_QueueIdx, syncMask);
            m_Device.Refresh(false);
            // m_Device.Refresh(true);
        }
    }

    VulkanSemaphore::VulkanSemaphore(VulkanResourceManager* owner) : VulkanResource(owner, false)
    {
        VkSemaphoreCreateInfo semaphoreCreateInfo;
        semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semaphoreCreateInfo.pNext = nullptr;
        semaphoreCreateInfo.flags = 0;
        VkResult result = vkCreateSemaphore(m_Owner->GetDevice().GetLogicalDevice(), &semaphoreCreateInfo,
                                            gVulkanAllocator, &m_Semaphore);

        CW_ENGINE_ASSERT(result == VK_SUCCESS);
    }

    VulkanSemaphore::~VulkanSemaphore()
    {
        vkDestroySemaphore(m_Owner->GetDevice().GetLogicalDevice(), m_Semaphore, gVulkanAllocator);
    }

    VulkanTransferBuffer::VulkanTransferBuffer(VulkanDevice* device, GpuQueueType type, uint32_t queueIdx)
      : m_Device(device), m_Type(type), m_QueueIdx(queueIdx)
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

    void VulkanTransferBuffer::MemoryBarrier(VkBuffer buffer, VkAccessFlags srcAccessFlags,
                                             VkAccessFlags dstAccessFlags, VkPipelineStageFlags srcStage,
                                             VkPipelineStageFlags dstStage)
    {
        m_CommandBuffer->MemoryBarrier(buffer, srcAccessFlags, dstAccessFlags, srcStage, dstStage);
    }

    void VulkanTransferBuffer::SetLayout(VkImage image, VkAccessFlags srcAccessFlags, VkAccessFlags dstAccessFlags,
                                         VkImageLayout oldLayout, VkImageLayout newLayout,
                                         const VkImageSubresourceRange& range)
    {
        m_CommandBuffer->SetLayout(image, srcAccessFlags, dstAccessFlags, oldLayout, newLayout, range);
    }

    void VulkanTransferBuffer::SetLayout(VulkanImage* image, const VkImageSubresourceRange& range,
                                         VkAccessFlags newAccessMask, VkImageLayout newLayout)
    {
        image->GetBarriers(range, m_BarriersTemp);
        if (m_BarriersTemp.size() == 0)
            return;

        int32_t count = (int32_t)m_BarriersTemp.size();
        for (int32_t i = 0; i < count; i++)
        {
            VkImageMemoryBarrier& barrier = m_BarriersTemp[i];
            if (barrier.oldLayout == newLayout)
            {
                if (i < (count - 1))
                    std::swap(m_BarriersTemp[i], m_BarriersTemp[count - 1]);
                m_BarriersTemp.erase(m_BarriersTemp.begin() + count - 1);
                count--;
                i--;
            }
        }

        for (auto& entry : m_BarriersTemp)
        {
            entry.dstAccessMask = newAccessMask;
            entry.newLayout = newLayout;
        }

        vkCmdPipelineBarrier(m_CommandBuffer->GetHandle(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr,
                             (uint32_t)m_BarriersTemp.size(), m_BarriersTemp.data());

        m_BarriersTemp.clear();
    }

    void VulkanTransferBuffer::Allocate()
    {
        if (m_CommandBuffer != nullptr)
            return;

        uint32_t queueFamily = m_Device->GetQueueFamily(m_Type);
        m_CommandBuffer = m_Device->GetCmdBufferPool().GetBuffer(queueFamily, false);
    }

    void VulkanTransferBuffer::Flush(bool wait)
    {
        if (m_CommandBuffer == nullptr)
            return;
        uint32_t syncMask = m_SyncMask & ~m_QueueMask;

        m_CommandBuffer->End();
        m_CommandBuffer->Submit(m_Queue, m_QueueIdx, syncMask);
        if (wait)
        {
            m_Queue->WaitIdle();
            m_Device->Refresh(true);
            CW_ENGINE_ASSERT(!m_CommandBuffer->IsSubmitted());
        }
        m_CommandBuffer = nullptr;
    }

    VulkanTransferManager::VulkanTransferManager()
    {
        VulkanDevice* device = gVulkanRenderAPI().GetPresentDevice().get();
        for (uint32_t i = 0; i < QUEUE_COUNT; i++)
        {
            GpuQueueType queueType = (GpuQueueType)i;
            for (uint32_t j = 0; j < MAX_QUEUES_PER_TYPE; j++)
                m_TransferBuffers[i][j] = VulkanTransferBuffer(device, queueType, j);
        }
    }

    void VulkanTransferManager::GetSyncSemaphores(uint32_t syncMask, VulkanSemaphore** semaphores, uint32_t& count)
    {
        bool failed = false;
        Ref<VulkanDevice> device = gVulkanRenderAPI().GetPresentDevice();
        uint32_t semaphoreIdx = 0;
        for (uint32_t i = 0; i < QUEUE_COUNT; i++)
        {
            GpuQueueType queueType = (GpuQueueType)i;
            uint32_t numQueues = device->GetNumQueues(queueType);
            for (uint32_t j = 0; j < numQueues; j++)
            {
                VulkanQueue* queue = device->GetQueue(queueType, j);
                VulkanCmdBuffer* lastCb = queue->GetLastCommandBuffer();
                if (lastCb == nullptr || !lastCb->IsSubmitted())
                    continue;
                uint32_t queueMask = device->GetQueueMask(queueType, j);
                if ((syncMask & queueMask) == 0)
                    continue;
                VulkanSemaphore* semaphore = lastCb->RequestInterQueueSemaphore();
                if (semaphore == nullptr)
                {
                    failed = true;
                    continue;
                }

                semaphores[semaphoreIdx++] = semaphore;
            }
        }

        count = semaphoreIdx;
        if (failed)
            CW_ENGINE_ERROR("Failed to allocate semaphores for command buffer synchronisation.");
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
            PoolInfo& poolInfo = m_Pools[familyIdx];
            poolInfo.QueueFamily = familyIdx;
            std::memset(poolInfo.Buffers, 0, sizeof(poolInfo.Buffers));
            vkCreateCommandPool(device.GetLogicalDevice(), &poolCreateInfo, gVulkanAllocator, &poolInfo.Pool);
        }
    }

    VulkanCommandBufferPool::~VulkanCommandBufferPool()
    {
        for (auto& entry : m_Pools)
        {
            PoolInfo& info = entry.second;
            for (uint32_t i = 0; i < MAX_VULKAN_CB_PER_QUEUE_FAMILY; i++)
            {
                VulkanCmdBuffer* buffer = info.Buffers[i];
                if (buffer == nullptr)
                    break;
                delete buffer;
            }
            vkDestroyCommandPool(m_Device.GetLogicalDevice(), info.Pool, gVulkanAllocator);
        }
    }

    VulkanCmdBuffer* VulkanCommandBufferPool::GetBuffer(uint32_t queueFamily, bool secondary)
    {
        auto iter = m_Pools.find(queueFamily);
        if (iter == m_Pools.end())
            return nullptr;
        VulkanCmdBuffer** buffers = iter->second.Buffers;

        uint32_t i = 0;
        for (; i < MAX_VULKAN_CB_PER_QUEUE_FAMILY; i++)
        {
            if (buffers[i] == nullptr)
                break;
            if (buffers[i]->m_State == VulkanCmdBuffer::State::Ready)
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

    VulkanCmdBuffer* VulkanCommandBufferPool::CreateBuffer(uint32_t queueFamily, bool secondary)
    {
        auto iter = m_Pools.find(queueFamily);
        if (iter == m_Pools.end())
            return nullptr;
        const PoolInfo& poolInfo = iter->second;
        return new VulkanCmdBuffer(m_Device, m_NextId++, poolInfo.Pool, poolInfo.QueueFamily, secondary);
    }

    VulkanCmdBuffer::VulkanCmdBuffer(VulkanDevice& device, uint32_t id, VkCommandPool pool, uint32_t queueFamily,
                                     bool secondary)
      : m_ScissorRequiresBind(true), m_ViewportRequiresBind(true), m_VertexInputsRequriesBind(true),
        m_GraphicsPipelineRequiresBind(true), m_Id(id), m_QueueFamily(queueFamily), m_Device(device), m_Pool(pool),
        m_ComputePipelineRequiresBind(true), m_NeedsRawMemoryBarrier(false), m_NeedsWarMemoryBarrier(false)
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
        // vkResetFences(m_Device.GetLogicalDevice(), 1, &m_Fence);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
    }

    VulkanCmdBuffer::~VulkanCmdBuffer()
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
        else if (m_State != State::Ready)
        {
            for (auto& entry : m_Resources)
            {
                ResourceUseHandle& useHandle = entry.second;
                CW_ENGINE_ASSERT(!useHandle.Used);
                entry.first->NotifyUnbound();
            }

            for (auto& entry : m_Images)
            {
                uint32_t imageInfoIdx = entry.second;
                ImageInfo& imageInfo = m_ImageInfos[imageInfoIdx];
                ResourceUseHandle& useHandle = imageInfo.UseHandle;
                CW_ENGINE_ASSERT(!useHandle.Used);
                entry.first->NotifyUnbound();
            }

            for (auto& entry : m_Buffers)
            {
                ResourceUseHandle& useHandle = entry.second.UseHandle;
                CW_ENGINE_ASSERT(!useHandle.Used);
                entry.first->NotifyUnbound();
            }

            for (auto& entry : m_SwapChains)
            {
                ResourceUseHandle& useHandle = entry.second;
                CW_ENGINE_ASSERT(!useHandle.Used);
                entry.first->NotifyUnbound();
            }
        }

        if (m_IntraQueueSemaphore != nullptr)
            m_IntraQueueSemaphore->Destroy();

        for (uint32_t i = 0; i < MAX_VULKAN_CB_DEPENDENCIES; i++)
        {
            if (m_InterQueueSemaphores[i] != nullptr)
                m_InterQueueSemaphores[i]->Destroy();
        }

        vkDestroyFence(device, m_Fence, gVulkanAllocator);
        vkFreeCommandBuffers(device, m_Pool, 1, &m_CmdBuffer);
        delete[] m_DescriptorSetsTemp;
    }

    bool VulkanCmdBuffer::BindGraphicsPipeline()
    {
        VulkanRenderPass* renderPass = m_Framebuffer->GetRenderPass();
        VulkanPipeline* pipeline = m_GraphicsPipeline->GetPipeline(renderPass, m_DrawMode);
        // get pipeline using the vertex layout and renderpass here, make sure that the flags are correct... not just
        // this
        m_GraphicsPipeline->RegisterPipelineResources(this);
        RegisterResource(pipeline, VulkanAccessFlagBits::Read);
        vkCmdBindPipeline(m_CmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetHandle());
        BindDynamicStates(true);
        m_GraphicsPipelineRequiresBind = false;
        return true;
    }

    void VulkanCmdBuffer::SetViewport(const Rect2F& rect)
    {
        if (m_Viewport == rect)
            return;
        m_Viewport = rect;
        m_ViewportRequiresBind = true;
    }

    void VulkanCmdBuffer::BindDynamicStates(bool force)
    {
        if (m_ViewportRequiresBind || force)
        {
            VkViewport viewport;
            viewport.x = m_Viewport.X * m_Framebuffer->GetWidth();
            viewport.y = m_Viewport.Y * m_Framebuffer->GetHeight();
            viewport.width = m_Viewport.Width * m_Framebuffer->GetWidth();
            viewport.height = m_Viewport.Height * m_Framebuffer->GetHeight();
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(m_CmdBuffer, 0, 1, &viewport);
            m_ViewportRequiresBind = false;
        }

        if (m_ScissorRequiresBind || force)
        {
            VkRect2D scissors;
            scissors.offset.x = (int32_t)m_Viewport.X;
            scissors.offset.y = (int32_t)m_Viewport.Y;
            scissors.extent.width = m_Framebuffer->GetWidth();
            scissors.extent.height = m_Framebuffer->GetHeight();
            vkCmdSetScissor(m_CmdBuffer, 0, 1, &scissors);
            m_ScissorRequiresBind = false;
        }
    }

    void VulkanCmdBuffer::SetPipeline(const Ref<GraphicsPipeline>& pipeline)
    {
        if (m_GraphicsPipeline == pipeline)
            return;
        m_GraphicsPipeline = std::static_pointer_cast<VulkanGraphicsPipeline>(pipeline);
        m_GraphicsPipelineRequiresBind = true;
    }

    void VulkanCmdBuffer::SetPipeline(const Ref<ComputePipeline>& pipeline)
    {
        if (m_ComputePipeline == pipeline)
            return;

        m_ComputePipeline = std::static_pointer_cast<VulkanComputePipeline>(pipeline);
        m_ComputePipelineRequiresBind = true;
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

            flags |= VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
            flags |= VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT;
            flags |= VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;
        }

        if ((accessFlags & VK_ACCESS_INPUT_ATTACHMENT_READ_BIT) != 0)
            flags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

        if ((accessFlags & (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)) != 0)
            flags |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        if ((accessFlags &
             (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)) != 0)
            flags |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

        if ((accessFlags & (VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT)) != 0)
            flags |= VK_PIPELINE_STAGE_TRANSFER_BIT;

        if ((accessFlags & (VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT)) != 0)
            flags |= VK_PIPELINE_STAGE_HOST_BIT;

        if (flags == 0)
            flags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

        return flags;
    }

    template <class T>
    void GetPipelineStageFlags(const Vector<T>& barriers, VkPipelineStageFlags& src, VkPipelineStageFlags& dst)
    {
        for (auto& entry : barriers)
        {
            src |= GetPipelineStageFlags(entry.srcAccessMask);
            dst |= GetPipelineStageFlags(entry.dstAccessMask);
        }
        if (src == 0)
            src = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        if (dst == 0)
            dst = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    }

    void VulkanCmdBuffer::RegisterBuffer(VulkanBuffer* buffer, BufferUseFlagBits useFlags,
                                         VulkanAccessFlags accessFlags, VkPipelineStageFlags stages)
    {
        auto res = m_Buffers.insert(std::make_pair(buffer, BufferInfo()));
        if (res.second)
        {
            BufferInfo& bufferInfo = res.first->second;
            bufferInfo.UseFlags = useFlags;
            bufferInfo.UseHandle.Used = false;
            bufferInfo.UseHandle.Flags = accessFlags;

            if (useFlags != BufferUseFlagBits::Transfer)
            {
                bufferInfo.WriteHazardUse.AccessFlags = accessFlags;
                bufferInfo.WriteHazardUse.Stages = stages;
            }
            buffer->NotifyBound();
        }
        else
        {
            BufferInfo& bufferInfo = res.first->second;
            CW_ENGINE_ASSERT(!bufferInfo.UseHandle.Used);

            bool resetRenderPass = false;
            if (useFlags != BufferUseFlagBits::Transfer)
            {
                if (accessFlags.IsSetAny(VulkanAccessFlagBits::Read | VulkanAccessFlagBits::Write))
                {
                    // read after write
                    if (bufferInfo.WriteHazardUse.AccessFlags.IsSet(VulkanAccessFlagBits::Write))
                    {
                        m_NeedsRawMemoryBarrier = true;
                        m_MemoryBarrierSrcStages |= bufferInfo.WriteHazardUse.Stages;
                        m_MemoryBarrierDstStages |= stages;
                        m_MemoryBarrierSrcAccess |= VK_ACCESS_SHADER_WRITE_BIT;
                        switch (useFlags)
                        {
                        case (BufferUseFlagBits::Generic):
                            if (accessFlags == VulkanAccessFlagBits::Read)
                                m_MemoryBarrierDstAccess |= VK_ACCESS_SHADER_READ_BIT;
                            if (accessFlags == VulkanAccessFlagBits::Write)
                                m_MemoryBarrierDstAccess |= VK_ACCESS_SHADER_WRITE_BIT;
                            break;

                        case (BufferUseFlagBits::Index):
                            m_MemoryBarrierDstAccess |= VK_ACCESS_INDEX_READ_BIT;
                            break;
                        case (BufferUseFlagBits::Vertex):
                            m_MemoryBarrierDstAccess |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
                            break;
                        case (BufferUseFlagBits::Uniform):
                            m_MemoryBarrierDstAccess |= VK_ACCESS_UNIFORM_READ_BIT;
                            break;
                        case (BufferUseFlagBits::Transfer):
                            if (accessFlags == VulkanAccessFlagBits::Read)
                                m_MemoryBarrierDstAccess |= VK_ACCESS_TRANSFER_READ_BIT;
                            if (accessFlags == VulkanAccessFlagBits::Write)
                                m_MemoryBarrierDstAccess |= VK_ACCESS_TRANSFER_WRITE_BIT;
                            break;
                        }
                        resetRenderPass = true;
                    }
                }

                if (accessFlags.IsSet(VulkanAccessFlagBits::Write))
                {
                    // write after read
                    if (bufferInfo.WriteHazardUse.AccessFlags.IsSet(VulkanAccessFlagBits::Read))
                    {
                        m_NeedsWarMemoryBarrier = true;
                        m_MemoryBarrierSrcStages |= bufferInfo.WriteHazardUse.Stages;
                        m_MemoryBarrierDstStages |= stages;
                        resetRenderPass = true;
                    }
                }

                bufferInfo.WriteHazardUse.AccessFlags |= accessFlags;
                bufferInfo.WriteHazardUse.Stages |= stages;
            }

            bufferInfo.UseHandle.Flags |= accessFlags;
            bufferInfo.UseFlags |= useFlags;

            if (resetRenderPass && IsInRenderPass())
                EndRenderPass();
        }
    }

    void VulkanCmdBuffer::RegisterResource(VulkanSwapChain* swapChain)
    {
        auto insertResult = m_Resources.insert(std::make_pair(swapChain, ResourceUseHandle()));
        if (insertResult.second)
        {
            ResourceUseHandle& useHandle = insertResult.first->second;
            useHandle.Used = false;
            useHandle.Flags = VulkanAccessFlagBits::Write;

            swapChain->NotifyBound();
        }
        else
        {
            ResourceUseHandle& useHandle = insertResult.first->second;
            CW_ENGINE_ASSERT(!useHandle.Used);
            useHandle.Flags |= VulkanAccessFlagBits::Write;
        }
    }

    void VulkanCmdBuffer::RegisterResource(VulkanResource* resource, VulkanAccessFlags flags)
    {
        auto insertResult = m_Resources.insert(std::make_pair(resource, ResourceUseHandle()));
        if (insertResult.second)
        {
            ResourceUseHandle& useHandle = insertResult.first->second;
            useHandle.Used = false;
            useHandle.Flags = flags;
            resource->NotifyBound();
        }
        else
        {
            ResourceUseHandle& useHandle = insertResult.first->second;
            CW_ENGINE_ASSERT(!useHandle.Used);
            useHandle.Flags |= flags;
        }
    }

    void VulkanCmdBuffer::RegisterResource(VulkanFramebuffer* framebuffer, RenderSurfaceMask loadMask,
                                           uint32_t readMask)
    {
        auto insertResult = m_Resources.insert(std::make_pair(framebuffer, ResourceUseHandle()));
        if (insertResult.second)
        {
            ResourceUseHandle& handle = insertResult.first->second;
            handle.Used = false;
            handle.Flags = VulkanAccessFlagBits::Write;
            framebuffer->NotifyBound();
        }
        else
        {
            ResourceUseHandle& handle = insertResult.first->second;
            CW_ENGINE_ASSERT(!handle.Used);
            handle.Flags |= VulkanAccessFlagBits::Write;
        }

        VulkanRenderPass* renderPass = framebuffer->GetRenderPass();
        uint32_t numColors = renderPass->GetNumColorAttachments();
        for (uint32_t i = 0; i < numColors; i++)
        {
            const VulkanFramebufferAttachment& attachment = framebuffer->GetColorAttachment(i);
            VkImageLayout layout;
            if (loadMask.IsSet((RenderSurfaceMaskBits)(1 << i)))
                layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            else
                layout = VK_IMAGE_LAYOUT_UNDEFINED;

            VulkanAccessFlagBits access =
              ((readMask & FBT_COLOR) != 0) ? VulkanAccessFlagBits::Read : VulkanAccessFlagBits::Write;
            VkImageSubresourceRange range = attachment.Image->GetRange(attachment.Surface);
            RegisterImageFramebuffer(attachment.Image, range, layout, attachment.FinalLayout, access,
                                     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        }

        if (renderPass->HasDepthAttachment())
        {
            const VulkanFramebufferAttachment& attachment = framebuffer->GetDepthStencilAttachment();
            VkImageLayout layout;
            if (loadMask.IsSet(RT_DEPTH) || loadMask.IsSet(RT_STENCIL))
                layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            else
                layout = VK_IMAGE_LAYOUT_UNDEFINED;

            VulkanAccessFlagBits access = (((readMask & FBT_DEPTH) != 0) && ((readMask & FBT_STENCIL) != 0))
                                            ? VulkanAccessFlagBits::Read
                                            : VulkanAccessFlagBits::Write;
            VkImageSubresourceRange range = attachment.Image->GetRange(attachment.Surface);
            RegisterImageFramebuffer(attachment.Image, range, layout, attachment.FinalLayout, access,
                                     VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                       VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
        }
    }

    void VulkanCmdBuffer::RegisterImageShader(VulkanImage* image, const VkImageSubresourceRange& range,
                                              VkImageLayout layout, VulkanAccessFlags accessFlags,
                                              VkPipelineStageFlags stages)
    {
        CW_ENGINE_ASSERT(layout != VK_IMAGE_LAYOUT_UNDEFINED);
        RegisterResource(image, range, ImageUseFlagBits::Shader, layout, layout, accessFlags, stages);
    }

    void VulkanCmdBuffer::RegisterImageFramebuffer(VulkanImage* image, const VkImageSubresourceRange& range,
                                                   VkImageLayout layout, VkImageLayout finalLayout,
                                                   VulkanAccessFlags accessFlags, VkPipelineStageFlags stages)
    {
        RegisterResource(image, range, ImageUseFlagBits::Framebuffer, layout, finalLayout, accessFlags, stages);
    }

    void VulkanCmdBuffer::RegisterImageTransfer(VulkanImage* image, const VkImageSubresourceRange& range,
                                                VkImageLayout layout, VulkanAccessFlags accessFlags)
    {
        RegisterResource(image, range, ImageUseFlagBits::Transfer, layout, layout, accessFlags,
                         VK_PIPELINE_STAGE_TRANSFER_BIT);
    }

    void VulkanCmdBuffer::RegisterResource(VulkanImage* image, const VkImageSubresourceRange& range,
                                           ImageUseFlagBits use, VkImageLayout layout, VkImageLayout finalLayout,
                                           VulkanAccessFlags accessFlags, VkPipelineStageFlags stages)
    {
        uint32_t nextImageInfoIdx = (uint32_t)m_ImageInfos.size();
        auto registerSubresourceInfo = [&](const VkImageSubresourceRange& subresourceRange) {
            m_SubresourceInfoStorage.push_back(ImageSubresourceInfo());
            ImageSubresourceInfo& subresourceInfo = m_SubresourceInfoStorage.back();
            subresourceInfo.CurrentLayout = layout;
            subresourceInfo.InitialLayout = layout;
            subresourceInfo.InitialReadOnly = !accessFlags.IsSet(VulkanAccessFlagBits::Write);
            subresourceInfo.RequiredLayout = layout;
            subresourceInfo.RenderPassLayout = finalLayout;
            subresourceInfo.Range = subresourceRange;
            switch (use)
            {
            default:
            case ImageUseFlagBits::Shader:
                subresourceInfo.ShaderUse.AccessFlags = accessFlags;
                subresourceInfo.ShaderUse.Stages = stages;
                subresourceInfo.WriteHazardUse.AccessFlags = accessFlags;
                subresourceInfo.WriteHazardUse.Stages = stages;
                break;
            case ImageUseFlagBits::Framebuffer:
                subresourceInfo.FbUse.AccessFlags = accessFlags;
                subresourceInfo.FbUse.Stages = stages;
                break;
            case ImageUseFlagBits::Transfer:
                subresourceInfo.TransferUse.AccessFlags = accessFlags;
                subresourceInfo.TransferUse.Stages = stages;
                break;
            }

            subresourceInfo.UseFlags = use;
            if (use == ImageUseFlagBits::Shader)
                m_ShaderBoundSubresourceInfos.insert((int32_t)m_SubresourceInfoStorage.size() - 1);
        };

        auto insertResult = m_Images.insert(std::make_pair(image, nextImageInfoIdx));
        if (insertResult.second)
        {
            uint32_t imageInfoIdx = insertResult.first->second;
            m_ImageInfos.push_back(ImageInfo());
            ImageInfo& imageInfo = m_ImageInfos[imageInfoIdx];
            imageInfo.SubresourceInfoIdx = (uint32_t)m_SubresourceInfoStorage.size();
            imageInfo.NumSubresourceInfos = 1;
            imageInfo.UseHandle.Used = false;
            imageInfo.UseHandle.Flags = accessFlags;
            registerSubresourceInfo(range);
            image->NotifyBound();
        }
        else
        {
            uint32_t imageInfoIdx = insertResult.first->second;
            ImageInfo& imageInfo = m_ImageInfos[imageInfoIdx];
            CW_ENGINE_ASSERT(!imageInfo.UseHandle.Used);
            imageInfo.UseHandle.Flags |= accessFlags;
            ImageSubresourceInfo* subresources = &m_SubresourceInfoStorage[imageInfo.SubresourceInfoIdx];
            bool foundRange = false;
            for (uint32_t i = 0; i < imageInfo.NumSubresourceInfos; i++)
            {
                if (VulkanUtils::RangeOverlaps(subresources[i].Range, range))
                {
                    if (subresources[i].Range.layerCount == range.layerCount &&
                        subresources[i].Range.levelCount == range.levelCount &&
                        subresources[i].Range.baseArrayLayer == range.baseArrayLayer &&
                        subresources[i].Range.baseMipLevel == range.baseMipLevel)
                    {
                        switch (use)
                        {
                        default:
                        case ImageUseFlagBits::Shader:
                            UpdateShaderSubresource(image, imageInfoIdx, subresources[i], layout, accessFlags, stages);
                            break;
                        case ImageUseFlagBits::Framebuffer:
                            UpdateFramebufferSubresource(image, imageInfoIdx, subresources[i], layout, finalLayout,
                                                         accessFlags, stages);
                            break;
                        case ImageUseFlagBits::Transfer:
                            UpdateTransferSubresource(image, imageInfoIdx, subresources[i], layout, accessFlags,
                                                      stages);
                            break;
                        }

                        if (use == ImageUseFlagBits::Shader)
                            m_ShaderBoundSubresourceInfos.insert(imageInfo.SubresourceInfoIdx + i);
                        foundRange = true;
                        break;
                    }
                    break;
                }
            }

            if (!foundRange)
            {
                std::array<VkImageSubresourceRange, 5> tempRanges;
                uint32_t newSubresourceIdx = (uint32_t)m_SubresourceInfoStorage.size();
                Vector<uint32_t> overlappingRanges;
                for (uint32_t i = 0; i < imageInfo.NumSubresourceInfos; i++)
                {
                    uint32_t subresourceIdx = imageInfo.SubresourceInfoIdx + i;
                    ImageSubresourceInfo& subresource = m_SubresourceInfoStorage[subresourceIdx];

                    if (!VulkanUtils::RangeOverlaps(subresource.Range, range))
                    {
                        m_SubresourceInfoStorage.push_back(subresource);
                        if (use == ImageUseFlagBits::Shader)
                            m_ShaderBoundSubresourceInfos.insert((uint32_t)m_SubresourceInfoStorage.size() - 1);
                    }
                    else // rejesh rejesh
                    {
                        uint32_t numCutRanges;
                        VulkanUtils::CutRange(subresource.Range, range, tempRanges, numCutRanges);
                        for (uint32_t j = 0; j < numCutRanges; j++)
                        {
                            ImageSubresourceInfo newInfo = m_SubresourceInfoStorage[subresourceIdx];
                            newInfo.Range = tempRanges[j];
                            if (VulkanUtils::RangeOverlaps(tempRanges[j], range))
                            {
                                switch (use)
                                {
                                default:
                                case ImageUseFlagBits::Shader:
                                    UpdateShaderSubresource(image, imageInfoIdx, newInfo, layout, accessFlags, stages);
                                    break;
                                case ImageUseFlagBits::Framebuffer:
                                    UpdateFramebufferSubresource(image, imageInfoIdx, newInfo, layout, finalLayout,
                                                                 accessFlags, stages);
                                    break;
                                case ImageUseFlagBits::Transfer:
                                    UpdateTransferSubresource(image, imageInfoIdx, newInfo, layout, accessFlags,
                                                              stages);
                                    break;
                                }
                                overlappingRanges.push_back((uint32_t)m_SubresourceInfoStorage.size());
                            }

                            m_SubresourceInfoStorage.push_back(newInfo);
                            if (use == ImageUseFlagBits::Shader)
                                m_ShaderBoundSubresourceInfos.insert((uint32_t)m_SubresourceInfoStorage.size() - 1);
                        }
                    }
                }
                if (overlappingRanges.empty())
                {
                    registerSubresourceInfo(range);
                }
                else
                {
                    std::queue<VkImageSubresourceRange> ranges;
                    ranges.push(range);
                    for (auto& entry : overlappingRanges)
                    {
                        VkImageSubresourceRange& overlappingRange = m_SubresourceInfoStorage[entry].Range;
                        uint32_t numRanges = (uint32_t)ranges.size();
                        for (uint32_t j = 0; j < numRanges; j++)
                        {
                            VkImageSubresourceRange srcRange = ranges.front();
                            ranges.pop();
                            uint32_t numRanges;
                            VulkanUtils::CutRange(srcRange, overlappingRange, tempRanges, numRanges);
                            for (uint32_t j = 0; j < numRanges; j++)
                            {
                                if (!VulkanUtils::RangeOverlaps(tempRanges[j], overlappingRange))
                                    ranges.push(tempRanges[j]);
                            }
                        }
                    }

                    while (!ranges.empty())
                    {
                        registerSubresourceInfo(ranges.front());
                        ranges.pop();
                    }
                }

                imageInfo.SubresourceInfoIdx = newSubresourceIdx;
                imageInfo.NumSubresourceInfos = (uint32_t)m_SubresourceInfoStorage.size() - newSubresourceIdx;
            }
        }

        for (uint32_t i = 0; i < range.layerCount; i++)
        {
            for (uint32_t j = 0; j < range.levelCount; j++)
            {
                uint32_t layer = range.baseArrayLayer + i;
                uint32_t mipLevel = range.baseMipLevel + j;
                RegisterResource(image->GetSubresource(layer, mipLevel), accessFlags);
            }
        }
    }

    void VulkanCmdBuffer::UpdateShaderSubresource(VulkanImage* image, uint32_t imageInfoIdx,
                                                  ImageSubresourceInfo& subresourceInfo, VkImageLayout layout,
                                                  VulkanAccessFlags access, VkPipelineStageFlags stages)
    {
        if (layout != VK_IMAGE_LAYOUT_UNDEFINED)
        {
            if (subresourceInfo.UseFlags.IsSet(ImageUseFlagBits::Framebuffer))
            {
                CW_ENGINE_ASSERT(!access.IsSet(VulkanAccessFlagBits::Write));
            }
            else
            {
                bool firstUseInRenderPass =
                  !subresourceInfo.UseFlags.IsSetAny(ImageUseFlagBits::Shader | ImageUseFlagBits::Framebuffer);
                if (firstUseInRenderPass || subresourceInfo.RequiredLayout == VK_IMAGE_LAYOUT_UNDEFINED)
                    subresourceInfo.RequiredLayout = layout;
                else if (subresourceInfo.RequiredLayout != layout)
                    subresourceInfo.RequiredLayout = VK_IMAGE_LAYOUT_GENERAL;
            }
        }

        if (subresourceInfo.CurrentLayout != subresourceInfo.RequiredLayout)
            m_QueuedLayoutTransitions[image] = imageInfoIdx;

        bool resetRenderPass = false;
        if (!subresourceInfo.UseFlags.IsSet(ImageUseFlagBits::Shader))
        {
            if (subresourceInfo.UseFlags.IsSet(ImageUseFlagBits::Framebuffer))
            {
                if (subresourceInfo.RequiredLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                {
                    resetRenderPass = ((m_RenderTargetReadOnlyFlags & FBT_DEPTH) == 0 &&
                                       (m_RenderTargetReadOnlyFlags & FBT_STENCIL) == 0);
                }
                else
                    resetRenderPass = true;
            }
        }

        if (access.IsSetAny(VulkanAccessFlagBits::Read | VulkanAccessFlagBits::Write))
        {
            if (subresourceInfo.WriteHazardUse.AccessFlags.IsSet(VulkanAccessFlagBits::Write))
            {
                m_NeedsRawMemoryBarrier = true;
                m_MemoryBarrierSrcStages |= subresourceInfo.WriteHazardUse.Stages;
                m_MemoryBarrierDstStages |= stages;
                m_MemoryBarrierSrcAccess |= VK_ACCESS_SHADER_WRITE_BIT;

                if (access.IsSet(VulkanAccessFlagBits::Read))
                    m_MemoryBarrierDstAccess |= VK_ACCESS_SHADER_READ_BIT;
                if (access.IsSet(VulkanAccessFlagBits::Write))
                    m_MemoryBarrierDstAccess |= VK_ACCESS_SHADER_WRITE_BIT;

                resetRenderPass = true;
            }
        }

        if (access.IsSet(VulkanAccessFlagBits::Write))
        {
            if (subresourceInfo.WriteHazardUse.AccessFlags.IsSet(VulkanAccessFlagBits::Read))
            {
                m_NeedsWarMemoryBarrier = true;
                m_MemoryBarrierSrcStages |= subresourceInfo.WriteHazardUse.Stages;
                m_MemoryBarrierDstStages |= stages;
                resetRenderPass = true;
            }
        }

        subresourceInfo.ShaderUse.AccessFlags |= access;
        subresourceInfo.ShaderUse.Stages |= stages;

        subresourceInfo.WriteHazardUse.AccessFlags |= access;
        subresourceInfo.WriteHazardUse.Stages |= stages;

        subresourceInfo.UseFlags |= ImageUseFlagBits::Shader;

        if (resetRenderPass && IsInRenderPass())
            EndRenderPass();
    }

    void VulkanCmdBuffer::UpdateFramebufferSubresource(VulkanImage* image, uint32_t imageInfoIdx,
                                                       ImageSubresourceInfo& subresourceInfo, VkImageLayout layout,
                                                       VkImageLayout finalLayout, VulkanAccessFlags access,
                                                       VkPipelineStageFlags stages)
    {
        subresourceInfo.RequiredLayout = layout;
        subresourceInfo.RenderPassLayout = finalLayout;

        if (subresourceInfo.CurrentLayout != subresourceInfo.RequiredLayout)
            m_QueuedLayoutTransitions[image] = imageInfoIdx;

        bool resetRenderPass = false;
        if (!subresourceInfo.UseFlags.IsSet(ImageUseFlagBits::Framebuffer))
            resetRenderPass = subresourceInfo.UseFlags.IsSet(ImageUseFlagBits::Shader);

        if (access.IsSetAny(VulkanAccessFlagBits::Read | VulkanAccessFlagBits::Write))
        {
            if (subresourceInfo.WriteHazardUse.AccessFlags.IsSet(VulkanAccessFlagBits::Write))
            {
                m_NeedsRawMemoryBarrier = true;
                m_MemoryBarrierSrcStages |= subresourceInfo.WriteHazardUse.Stages;
                m_MemoryBarrierDstStages |= stages;
                m_MemoryBarrierSrcAccess |= VK_ACCESS_SHADER_WRITE_BIT;

                if (access.IsSet(VulkanAccessFlagBits::Read))
                {
                    if ((stages & VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT) != 0)
                        m_MemoryBarrierDstAccess |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
                    if ((stages & VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT) != 0 ||
                        (stages & VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT) != 0)
                        m_MemoryBarrierDstAccess |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
                }

                if (access.IsSet(VulkanAccessFlagBits::Write))
                {
                    if ((stages & VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT) != 0)
                        m_MemoryBarrierDstAccess |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                    if ((stages & VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT) != 0 ||
                        (stages & VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT) != 0)
                        m_MemoryBarrierDstAccess |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                }
                resetRenderPass = true;
            }
        }

        subresourceInfo.FbUse.AccessFlags |= access;
        subresourceInfo.FbUse.Stages |= stages;
        subresourceInfo.UseFlags |= ImageUseFlagBits::Framebuffer;

        if (resetRenderPass && IsInRenderPass())
            EndRenderPass();
    }

    void VulkanCmdBuffer::UpdateTransferSubresource(VulkanImage* image, uint32_t imageInfoIdx,
                                                    ImageSubresourceInfo& subresourceInfo, VkImageLayout layout,
                                                    VulkanAccessFlags access, VkPipelineStageFlags stages)
    {
        subresourceInfo.CurrentLayout = layout;
        subresourceInfo.RequiredLayout = layout;
        subresourceInfo.TransferUse.AccessFlags |= access;
        subresourceInfo.TransferUse.Stages |= stages;
        subresourceInfo.UseFlags |= ImageUseFlagBits::Transfer;
    }

    void VulkanCmdBuffer::ExecuteWriteHazardBarrier()
    {
        if (!m_NeedsRawMemoryBarrier && !m_NeedsWarMemoryBarrier)
            return;

        if (m_NeedsRawMemoryBarrier)
        {
            VkMemoryBarrier barrier;
            barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            barrier.pNext = nullptr;
            barrier.srcAccessMask = m_MemoryBarrierSrcAccess;
            barrier.dstAccessMask = m_MemoryBarrierDstAccess;

            vkCmdPipelineBarrier(m_CmdBuffer, m_MemoryBarrierSrcStages, m_MemoryBarrierDstStages, 0, 1, &barrier, 0,
                                 nullptr, 0, nullptr);
        }
        else
        {
            vkCmdPipelineBarrier(m_CmdBuffer, m_MemoryBarrierSrcStages, m_MemoryBarrierDstStages, 0, 0, nullptr, 0,
                                 nullptr, 0, nullptr);
        }

        m_NeedsRawMemoryBarrier = false;
        m_NeedsWarMemoryBarrier = false;
        m_MemoryBarrierSrcStages = 0;
        m_MemoryBarrierDstStages = 0;
        m_MemoryBarrierSrcAccess = 0;
        m_MemoryBarrierDstAccess = 0;

        for (auto& buffer : m_Buffers)
        {
            BufferInfo& bufferInfo = buffer.second;
            bufferInfo.WriteHazardUse.AccessFlags = VulkanAccessFlagBits::None;
            bufferInfo.WriteHazardUse.Stages = 0;
        }
    }

    void VulkanCmdBuffer::ExecuteLayoutTransitions()
    {
        auto createLayoutTransitionBarrier = [&](VulkanImage* image, ImageInfo& imageInfo) {
            ImageSubresourceInfo* infos = &m_SubresourceInfoStorage[imageInfo.SubresourceInfoIdx];
            for (uint32_t i = 0; i < imageInfo.NumSubresourceInfos; i++)
            {
                ImageSubresourceInfo& info = infos[i];
                if (info.RequiredLayout == VK_IMAGE_LAYOUT_UNDEFINED || info.CurrentLayout == info.RequiredLayout)
                    continue;
                const bool isReadOnly = !info.FbUse.AccessFlags.IsSet(VulkanAccessFlagBits::Write) &&
                                        !info.ShaderUse.AccessFlags.IsSet(VulkanAccessFlagBits::Write);

                m_LayoutTransitionBarriersTemp.push_back(VkImageMemoryBarrier());
                VkImageMemoryBarrier& barrier = m_LayoutTransitionBarriersTemp.back();
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.pNext = nullptr;
                barrier.srcAccessMask = 0;
                barrier.dstAccessMask = image->GetAccessFlags(info.RequiredLayout, isReadOnly);
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.oldLayout = info.CurrentLayout;
                barrier.newLayout = info.RequiredLayout;
                barrier.image = image->GetHandle();
                barrier.subresourceRange = info.Range;

                info.CurrentLayout = info.RequiredLayout;
            }
        };

        for (auto& entry : m_QueuedLayoutTransitions)
        {
            uint32_t imageInfoIdx = entry.second;
            ImageInfo& imageInfo = m_ImageInfos[imageInfoIdx];
            createLayoutTransitionBarrier(entry.first, imageInfo);
        }

        VkPipelineStageFlags srcStage = 0;
        VkPipelineStageFlags dstStage = 0;
        GetPipelineStageFlags(m_LayoutTransitionBarriersTemp, srcStage, dstStage);
        if (!m_LayoutTransitionBarriersTemp.empty())
        {
            vkCmdPipelineBarrier(m_CmdBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr,
                                 (uint32_t)m_LayoutTransitionBarriersTemp.size(),
                                 m_LayoutTransitionBarriersTemp.data());
        }

        m_QueuedLayoutTransitions.clear();
        m_LayoutTransitionBarriersTemp.clear();
    }

    void VulkanCmdBuffer::UpdateFinalLayouts()
    {
        if (m_Framebuffer == nullptr)
            return;

        VulkanRenderPass* renderPass = m_Framebuffer->GetRenderPass();
        uint32_t numColorAttachments = renderPass->GetNumColorAttachments();
        for (uint32_t i = 0; i < numColorAttachments; i++)
        {
            const VulkanFramebufferAttachment& fbAttachment = m_Framebuffer->GetColorAttachment(i);
            ImageSubresourceInfo& subresourceInfo =
              FindSubresourceInfo(fbAttachment.Image, fbAttachment.Surface.Face, fbAttachment.Surface.MipLevel);
            subresourceInfo.CurrentLayout = subresourceInfo.RenderPassLayout;
            subresourceInfo.RequiredLayout = subresourceInfo.RenderPassLayout;
        }

        if (renderPass->HasDepthAttachment())
        {
            const VulkanFramebufferAttachment& fbAttachment = m_Framebuffer->GetDepthStencilAttachment();
            ImageSubresourceInfo& subresourceInfo =
              FindSubresourceInfo(fbAttachment.Image, fbAttachment.Surface.Face, fbAttachment.Surface.MipLevel);
            subresourceInfo.CurrentLayout = subresourceInfo.RenderPassLayout;
            subresourceInfo.RequiredLayout = subresourceInfo.RenderPassLayout;
        }
    }

    void VulkanCmdBuffer::BindVertexInputs()
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
                    m_VertexBuffersTemp[idx] = vertexBuffer->GetHandle();
                    RegisterBuffer(vertexBuffer->GetBuffer(), BufferUseFlagBits::Vertex, VulkanAccessFlagBits::Read);
                    if (lastValidIdx == (uint32_t)-1)
                        lastValidIdx = idx;
                    validBuffer = true;
                }

                if (!validBuffer && lastValidIdx != (uint32_t)-1)
                {
                    uint32_t count = idx - lastValidIdx;
                    if (count > 0)
                    {
                        vkCmdBindVertexBuffers(m_CmdBuffer, lastValidIdx, count, m_VertexBuffersTemp,
                                               m_VertexBufferOffsets);
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
                    vkCmdBindVertexBuffers(m_CmdBuffer, lastValidIdx, count, m_VertexBuffersTemp,
                                           m_VertexBufferOffsets);
                }
            }
        }

        if (m_IndexBuffer != nullptr)
        {
            VkBuffer vkBuffer = m_IndexBuffer->GetHandle();
            RegisterBuffer(m_IndexBuffer->GetBuffer(), BufferUseFlagBits::Index, VulkanAccessFlagBits::Read);
            vkCmdBindIndexBuffer(m_CmdBuffer, vkBuffer, 0, VulkanUtils::GetIndexType(m_IndexBuffer->GetIndexType()));
        }
    }

    void VulkanCmdBuffer::Begin()
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

    void VulkanCmdBuffer::End()
    {
        CW_ENGINE_ASSERT(m_State == State::Recording);
        if (m_ClearMask)
            ExecuteClearPass();
        VkResult result = vkEndCommandBuffer(m_CmdBuffer);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
        m_RenderTarget = nullptr;
        m_State = State::RecordingDone;
    }

    void VulkanCmdBuffer::BeginRenderPass()
    {
        CW_ENGINE_ASSERT(m_State == State::Recording);
        CW_ENGINE_ASSERT(m_RenderTarget != nullptr, "Render target is nullptr");

        if (m_ClearMask != CLEAR_NONE)
        {
            Rect2I clrArea(0, 0, m_Framebuffer->GetWidth(), m_Framebuffer->GetHeight());
            if (m_ClearArea.X != clrArea.X || m_ClearArea.Y != clrArea.Y || m_ClearArea.Width != clrArea.Width ||
                m_ClearArea.Height != clrArea.Height)
                ExecuteClearPass();
        }

        ExecuteWriteHazardBarrier();
        ExecuteLayoutTransitions();

        RenderSurfaceMask readMask = GetFBReadMask();
        VulkanRenderPass* renderPass = m_Framebuffer->GetRenderPass();

        VkRenderPassBeginInfo renderPassBeginInfo;
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.pNext = nullptr;
        renderPassBeginInfo.framebuffer = m_Framebuffer->GetHandle();
        renderPassBeginInfo.renderPass = renderPass->GetVkRenderPass(m_RenderTargetLoadMask, readMask, m_ClearMask);
        renderPassBeginInfo.renderArea.offset.x = 0;
        renderPassBeginInfo.renderArea.offset.y = 0;
        renderPassBeginInfo.renderArea.extent.width = m_Framebuffer->GetWidth();
        renderPassBeginInfo.renderArea.extent.height = m_Framebuffer->GetHeight();
        renderPassBeginInfo.clearValueCount = renderPass->GetNumClearEntries(m_ClearMask);
        renderPassBeginInfo.pClearValues = m_ClearValues.data();

        vkCmdBeginRenderPass(m_CmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        m_ClearMask = CLEAR_NONE;
        m_State = State::RecordingRenderPass;
    }

    void VulkanCmdBuffer::EndRenderPass()
    {
        CW_ENGINE_ASSERT(m_State == State::RecordingRenderPass);
        vkCmdEndRenderPass(m_CmdBuffer);

        for (auto& entry : m_ShaderBoundSubresourceInfos)
        {
            ImageSubresourceInfo& subresourceInfo = m_SubresourceInfoStorage[entry];
            subresourceInfo.UseFlags.Unset(ImageUseFlagBits::Shader);
            subresourceInfo.ShaderUse.AccessFlags = VulkanAccessFlagBits::None;
            subresourceInfo.ShaderUse.Stages = 0;
        }

        m_ShaderBoundSubresourceInfos.clear();
        UpdateFinalLayouts();

        m_State = State::Recording;
        m_BoundUniformsDirty = true;
    }

    void VulkanCmdBuffer::ClearViewport(const Rect2I& area, uint32_t buffers, const glm::vec4& color, float depth,
                                        uint16_t stencil, uint8_t targetMask)
    {
        if (buffers == 0 || m_Framebuffer == nullptr)
            return;

        VulkanRenderPass* renderPass = m_Framebuffer->GetRenderPass();

        if (IsInRenderPass())
        {
            VkClearAttachment attachments[MAX_FRAMEBUFFER_COLOR_ATTACHMENTS + 1];
            uint32_t baseLayer = 0;
            uint32_t attachmentIdx = 0;
            if ((buffers & FBT_COLOR) != 0)
            {
                uint32_t numColorAttachments = renderPass->GetNumColorAttachments();
                for (uint32_t i = 0; i < numColorAttachments; i++)
                {
                    const VulkanFramebufferAttachment& attachment = m_Framebuffer->GetColorAttachment(i);
                    if (((1 << attachment.Index) & targetMask) == 0)
                        continue;

                    attachments[attachmentIdx].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    attachments[attachmentIdx].colorAttachment = i;

                    VkClearColorValue& colorValue = attachments[attachmentIdx].clearValue.color;
                    colorValue.float32[0] = color.r;
                    colorValue.float32[1] = color.g;
                    colorValue.float32[2] = color.b;
                    colorValue.float32[3] = color.a;

                    uint32_t curBaseLayer = attachment.BaseLayer;
                    if (attachmentIdx == 0)
                        baseLayer = curBaseLayer;
                    else
                    {
                        if (baseLayer != curBaseLayer)
                        {
                            CW_ENGINE_ASSERT(false);
                        }
                    }
                    attachmentIdx++;
                }
            }

            if ((buffers & FBT_DEPTH) != 0 || (buffers & FBT_STENCIL) != 0)
            {
                if (renderPass->HasDepthAttachment())
                {
                    attachments[attachmentIdx].aspectMask = 0;
                    if ((buffers & FBT_DEPTH) != 0)
                    {
                        attachments[attachmentIdx].aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
                        attachments[attachmentIdx].clearValue.depthStencil.depth = depth;
                    }

                    if ((buffers & FBT_STENCIL) != 0)
                    {
                        attachments[attachmentIdx].aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
                        attachments[attachmentIdx].clearValue.depthStencil.stencil = stencil;
                    }

                    attachments[attachmentIdx].colorAttachment = 0;

                    uint32_t curBaseLayer = m_Framebuffer->GetDepthStencilAttachment().BaseLayer;
                    if (attachmentIdx == 0)
                        baseLayer = curBaseLayer;
                    else
                    {
                        if (baseLayer != curBaseLayer)
                        {
                            CW_ENGINE_ASSERT(false);
                        }
                    }

                    attachmentIdx++;
                }
            }

            uint32_t numAttachments = attachmentIdx;
            if (numAttachments == 0)
                return;
            VkClearRect clearRect;
            clearRect.baseArrayLayer = baseLayer;
            clearRect.layerCount = m_Framebuffer->GetNumLayers();
            clearRect.rect.offset.x = area.X;
            clearRect.rect.offset.y = area.Y;
            clearRect.rect.extent.width = area.Width;
            clearRect.rect.extent.height = area.Height;
            vkCmdClearAttachments(m_CmdBuffer, numAttachments, attachments, 1, &clearRect);
        }
        else
        {
            ClearMask clearMask;
            std::array<VkClearValue, MAX_FRAMEBUFFER_COLOR_ATTACHMENTS + 1> clearValues = m_ClearValues;
            uint32_t numColorAttachments = renderPass->GetNumColorAttachments();
            if ((buffers & FBT_COLOR) != 0)
            {
                for (uint32_t i = 0; i < numColorAttachments; i++)
                {
                    const VulkanFramebufferAttachment& attachment = m_Framebuffer->GetColorAttachment(i);
                    if (((1 << attachment.Index) & targetMask) == 0)
                        continue;

                    clearMask |= (ClearMaskBits)(1 << attachment.Index);
                    VkClearColorValue& colorValue = clearValues[i].color;
                    colorValue.float32[0] = color.r;
                    colorValue.float32[1] = color.g;
                    colorValue.float32[2] = color.b;
                    colorValue.float32[3] = color.a;
                }
            }

            if ((buffers & FBT_DEPTH) != 0 || (buffers & FBT_STENCIL) != 0)
            {
                if (renderPass->HasDepthAttachment())
                {
                    uint32_t depthAttachmentIdx = numColorAttachments;
                    if ((buffers & FBT_DEPTH) != 0)
                    {
                        clearValues[depthAttachmentIdx].depthStencil.depth = depth;
                        clearMask |= CLEAR_DEPTH;
                    }

                    if ((buffers & FBT_STENCIL) != 0)
                    {
                        clearValues[depthAttachmentIdx].depthStencil.stencil = stencil;
                        clearMask |= CLEAR_STENCIL;
                    }
                }
            }

            if (!clearMask)
                return;

            bool prevClear = (m_ClearMask & clearMask) != CLEAR_NONE;
            if (prevClear)
                ExecuteClearPass();

            m_ClearMask |= clearMask;
            m_ClearValues = clearValues;
            m_ClearArea = area;
        }
    }

    void VulkanCmdBuffer::ClearRenderTarget(uint32_t buffers, const glm::vec4& color, float depth, uint16_t stencil,
                                            uint8_t targetMask)
    {
        Rect2I area(0, 0, m_Framebuffer->GetWidth(), m_Framebuffer->GetHeight());
        ClearViewport(area, buffers, color, depth, stencil, targetMask);
    }

    void VulkanCmdBuffer::ClearViewport(uint32_t buffers, const glm::vec4& color, float depth, uint16_t stencil,
                                        uint8_t targetMask)
    {
        Rect2I area;
        area.X = (uint32_t)(m_Viewport.X);
        area.Y = (uint32_t)(m_Viewport.Y);
        area.Width = (uint32_t)(m_Viewport.Width);
        area.Height = (uint32_t)(m_Viewport.Height);

        ClearViewport(area, buffers, color, depth, stencil, targetMask);
    }

    void VulkanCmdBuffer::ExecuteClearPass()
    {
        CW_ENGINE_ASSERT(m_State == State::Recording);
        VulkanRenderPass* renderPass = m_Framebuffer->GetRenderPass();
        VkRenderPassBeginInfo renderPassBeginInfo;
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.pNext = nullptr;
        renderPassBeginInfo.framebuffer = m_Framebuffer->GetHandle();
        renderPassBeginInfo.renderPass = renderPass->GetHandle();
        renderPassBeginInfo.renderArea.offset.x = m_ClearArea.X;
        renderPassBeginInfo.renderArea.offset.y = m_ClearArea.Y;
        renderPassBeginInfo.renderArea.extent.width = m_ClearArea.Width;
        renderPassBeginInfo.renderArea.extent.height = m_ClearArea.Height;
        renderPassBeginInfo.clearValueCount = renderPass->GetNumClearEntries(m_ClearMask);
        renderPassBeginInfo.pClearValues = m_ClearValues.data();

        vkCmdBeginRenderPass(m_CmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdEndRenderPass(m_CmdBuffer);
    }

    void VulkanCmdBuffer::Reset()
    {
        bool submitted = m_State == State::Submitted;

        m_State = State::Ready;
        vkResetCommandBuffer(m_CmdBuffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
        if (submitted)
        {
            for (auto& entry : m_Resources)
            {
                ResourceUseHandle& useHandle = entry.second;
                CW_ENGINE_ASSERT(useHandle.Used);
                entry.first->NotifyDone(m_GlobalQueueIdx, useHandle.Flags);
            }

            for (auto& entry : m_Images)
            {
                uint32_t imageInfoIdx = entry.second;
                ImageInfo& imageInfo = m_ImageInfos[imageInfoIdx];

                ResourceUseHandle& useHandle = imageInfo.UseHandle;
                CW_ENGINE_ASSERT(useHandle.Used);
                entry.first->NotifyDone(m_GlobalQueueIdx, useHandle.Flags);
            }

            for (auto& entry : m_Buffers)
            {
                ResourceUseHandle& useHandle = entry.second.UseHandle;
                CW_ENGINE_ASSERT(useHandle.Used);
                entry.first->NotifyDone(m_GlobalQueueIdx, useHandle.Flags);
            }

            for (auto& entry : m_SwapChains)
            {
                ResourceUseHandle& useHandle = entry.second;
                CW_ENGINE_ASSERT(useHandle.Used);
                entry.first->NotifyDone(m_GlobalQueueIdx, useHandle.Flags);
            }
        }
        else
        {
            for (auto& entry : m_Resources)
                entry.first->NotifyUnbound();
            for (auto& entry : m_Images)
                entry.first->NotifyUnbound();
            for (auto& entry : m_Buffers)
                entry.first->NotifyUnbound();
            for (auto& entry : m_Images)
                entry.first->NotifyUnbound();
        }

        m_Resources.clear();
        m_Images.clear();
        m_Buffers.clear();
        m_SwapChains.clear();
        m_ImageInfos.clear();
        m_SubresourceInfoStorage.clear();
        m_ShaderBoundSubresourceInfos.clear();
        m_NeedsRawMemoryBarrier = false;
        m_NeedsWarMemoryBarrier = false;
        m_MemoryBarrierSrcAccess = 0;
        m_MemoryBarrierDstAccess = 0;
        m_MemoryBarrierSrcStages = 0;
        m_MemoryBarrierDstStages = 0;
    }

    void VulkanCmdBuffer::Submit(VulkanQueue* queue, uint32_t queueIdx, uint32_t syncMask)
    {
        CW_ENGINE_ASSERT(IsReadyForSubmit());

        VkResult result = vkResetFences(m_Device.GetLogicalDevice(), 1, &m_Fence);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);

        VulkanDevice& device = queue->GetDevice();

        if (!m_QueuedQueryResets.empty())
        {
            VulkanCmdBuffer* cmdBuffer = device.GetCmdBufferPool().GetBuffer(m_QueueFamily, false);
            VkCommandBuffer vkCmdBuffer = cmdBuffer->GetHandle();
            for (auto entry : m_QueuedQueryResets)
                entry->Reset(vkCmdBuffer);
            cmdBuffer->End();
            queue->QueueSubmit(cmdBuffer, nullptr, 0);
            m_QueuedQueryResets.clear();
        }

        for (auto& entry : m_Buffers)
        {
            VulkanBuffer* buffer = static_cast<VulkanBuffer*>(entry.first);
            if (!buffer->IsExclusive())
                continue;
            uint32_t currentQueueFamily = buffer->GetQueueFamily();
            if (currentQueueFamily != (uint32_t)-1 && currentQueueFamily != m_QueueFamily)
            {
                Vector<VkBufferMemoryBarrier>& barriers = m_TransitionInfoTemp[currentQueueFamily].BufferBarriers;

                barriers.push_back(VkBufferMemoryBarrier());
                VkBufferMemoryBarrier& barrier = barriers.back();
                barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
                barrier.pNext = nullptr;
                barrier.srcAccessMask = 0;
                barrier.dstAccessMask = 0;
                barrier.srcQueueFamilyIndex = currentQueueFamily;
                barrier.dstQueueFamilyIndex = m_QueueFamily;
                barrier.buffer = buffer->GetHandle();
                barrier.offset = 0;
                barrier.size = VK_WHOLE_SIZE;
            }
        }

        Vector<VkImageMemoryBarrier>& localBarriers = m_TransitionInfoTemp[m_QueueFamily].ImageBarriers;
        for (auto& entry : m_Images)
        {
            VulkanImage* image = static_cast<VulkanImage*>(entry.first);
            ImageInfo& imageInfo = m_ImageInfos[entry.second];
            uint32_t currentQueueFamily = image->GetQueueFamily();
            bool queueMismatch =
              image->IsExclusive() && currentQueueFamily != (uint32_t)-1 && currentQueueFamily != m_QueueFamily;
            ImageSubresourceInfo* subresourceInfos = &m_SubresourceInfoStorage[imageInfo.SubresourceInfoIdx];
            if (queueMismatch)
            {
                Vector<VkImageMemoryBarrier>& barriers = m_TransitionInfoTemp[currentQueueFamily].ImageBarriers;
                for (uint32_t i = 0; i < imageInfo.NumSubresourceInfos; i++)
                {
                    ImageSubresourceInfo& subresourceInfo = subresourceInfos[i];
                    uint32_t startIdx = (uint32_t)barriers.size();
                    image->GetBarriers(subresourceInfo.Range, barriers);
                    for (uint32_t j = startIdx; j < (uint32_t)barriers.size(); j++)
                    {
                        VkImageMemoryBarrier& barrier = barriers[j];
                        barrier.dstAccessMask = 0;
                        barrier.newLayout = barrier.oldLayout;
                        barrier.srcQueueFamilyIndex = currentQueueFamily;
                        barrier.dstQueueFamilyIndex = m_QueueFamily;
                    }
                }
            }

            for (uint32_t i = 0; i < imageInfo.NumSubresourceInfos; i++)
            {
                ImageSubresourceInfo& subresourceInfo = subresourceInfos[i];
                const VkImageSubresourceRange& range = subresourceInfo.Range;
                uint32_t mipEnd = range.baseMipLevel + range.levelCount;
                uint32_t faceEnd = range.baseArrayLayer + range.layerCount;

                VkImageLayout initialLayout = subresourceInfo.InitialLayout;
                if (initialLayout != VK_IMAGE_LAYOUT_UNDEFINED)
                {
                    bool layoutMismatch = false;
                    for (uint32_t mip = range.baseMipLevel; mip < mipEnd; mip++)
                    {
                        for (uint32_t face = range.baseArrayLayer; face < faceEnd; face++)
                        {
                            VulkanImageSubresource* subresource = image->GetSubresource(face, mip);
                            if (subresource->GetLayout() != initialLayout)
                            {
                                layoutMismatch = true;
                                break;
                            }
                        }

                        if (layoutMismatch)
                            break;
                    }

                    if (layoutMismatch)
                    {
                        uint32_t startIdx = (uint32_t)localBarriers.size();
                        image->GetBarriers(subresourceInfo.Range, localBarriers);
                        for (uint32_t j = startIdx; j < (uint32_t)localBarriers.size(); j++)
                        {
                            VkImageMemoryBarrier& barrier = localBarriers[j];
                            barrier.dstAccessMask =
                              image->GetAccessFlags(initialLayout, subresourceInfo.InitialReadOnly);
                            barrier.newLayout = initialLayout;
                        }
                    }
                }

                for (uint32_t mip = range.baseMipLevel; mip < mipEnd; mip++)
                {
                    for (uint32_t face = range.baseArrayLayer; face < faceEnd; face++)
                    {
                        VulkanImageSubresource* subresource = image->GetSubresource(face, mip);
                        subresource->SetLayout(subresourceInfo.CurrentLayout);
                    }
                }
            }
        }

        for (auto& entry : m_TransitionInfoTemp)
        {
            bool empty = entry.second.ImageBarriers.empty() && entry.second.BufferBarriers.empty();
            if (empty)
                continue;

            uint32_t entryQueueFamily = entry.first;
            if (entryQueueFamily != (uint32_t)-1 && entryQueueFamily == m_QueueFamily)
                continue;

            VulkanCmdBuffer* cmdBuffer = device.GetCmdBufferPool().GetBuffer(entryQueueFamily, false);
            VkCommandBuffer vkCmdBuffer = cmdBuffer->GetHandle();
            TransitionInfo& barriers = entry.second;
            uint32_t numImgBarriers = (uint32_t)barriers.ImageBarriers.size();
            uint32_t numBufferBarriers = (uint32_t)barriers.BufferBarriers.size();

            VkPipelineStageFlags srcStage = 0;
            VkPipelineStageFlags dstStage = 0;
            GetPipelineStageFlags(barriers.ImageBarriers, srcStage, dstStage);

            vkCmdPipelineBarrier(vkCmdBuffer, srcStage, dstStage, 0, 0, nullptr, numBufferBarriers,
                                 barriers.BufferBarriers.data(), numImgBarriers, barriers.ImageBarriers.data());

            uint32_t otherQueueIdx = 0;
            VulkanQueue* otherQueue = nullptr;
            GpuQueueType otherQueueType = GRAPHICS_QUEUE;
            for (uint32_t i = 0; i < QUEUE_COUNT; i++)
            {
                otherQueueType = (GpuQueueType)i;
                if (device.GetQueueFamily(otherQueueType) != entryQueueFamily)
                    continue;
                uint32_t numQueues = device.GetNumQueues(otherQueueType);
                for (uint32_t j = 0; j < numQueues; j++)
                {
                    VulkanQueue* queue = device.GetQueue(otherQueueType, j);
                    if (!queue->IsExecuting())
                    {
                        otherQueue = queue;
                        otherQueueIdx = j;
                    }
                }

                if (otherQueue == nullptr)
                {
                    otherQueue = device.GetQueue(otherQueueType, 0);
                    otherQueueIdx = 0;
                }
                break;
            }

            syncMask |= CommandSyncMask::GetGlobalQueueMask(otherQueueType, otherQueueIdx);
            cmdBuffer->End();
            otherQueue->Submit(cmdBuffer, nullptr, 0);
        }

        uint32_t deviceIdx = device.GetIndex();
        VulkanTransferManager& cbm = VulkanTransferManager::Get();

        uint32_t numSemaphores;
        cbm.GetSyncSemaphores(syncMask, m_SemaphoresTemp.data(), numSemaphores);
        for (auto& entry : m_ActiveSwapChains)
        {
            const SwapChainSurface& surface = entry->GetBackBuffer();
            if (surface.NeedsWait)
            {
                VulkanSemaphore* semaphore = entry->GetBackBuffer().Sync;
                if (numSemaphores >= (uint32_t)m_SemaphoresTemp.size())
                    m_SemaphoresTemp.push_back(semaphore);
                else
                    m_SemaphoresTemp[numSemaphores] = semaphore;
                numSemaphores++;

                entry->BackBufferWaitIssued();
            }
        }

        for (auto& entry : m_TransitionInfoTemp)
        {
            bool empty = entry.second.ImageBarriers.size() == 0 && entry.second.BufferBarriers.size() == 0;
            if (empty)
                continue;

            VulkanCmdBuffer* cmdBuffer = device.GetCmdBufferPool().GetBuffer(m_QueueFamily, false);
            VkCommandBuffer vkCmdBuffer = cmdBuffer->GetHandle();

            TransitionInfo& barriers = entry.second;
            uint32_t numImageBarriers = (uint32_t)barriers.ImageBarriers.size();
            uint32_t numBufferBarriers = (uint32_t)barriers.BufferBarriers.size();

            VkPipelineStageFlags srcStage = 0;
            VkPipelineStageFlags dstStage = 0;
            GetPipelineStageFlags(barriers.ImageBarriers, srcStage, dstStage);

            vkCmdPipelineBarrier(vkCmdBuffer, srcStage, dstStage, 0, 0, nullptr, numBufferBarriers,
                                 barriers.BufferBarriers.data(), numImageBarriers, barriers.ImageBarriers.data());

            cmdBuffer->End();
            queue->QueueSubmit(cmdBuffer, m_SemaphoresTemp.data(), numSemaphores);
            numSemaphores = 0;
        }

        queue->QueueSubmit(this, m_SemaphoresTemp.data(), numSemaphores);
        queue->SubmitQueued();

        m_GlobalQueueIdx = CommandSyncMask::GetGlobalQueueIdx(queue->GetType(), queueIdx);
        for (auto& entry : m_Resources)
        {
            ResourceUseHandle& useHandle = entry.second;
            CW_ENGINE_ASSERT(!useHandle.Used);
            useHandle.Used = true;
            entry.first->NotifyUsed(m_GlobalQueueIdx, m_QueueFamily, useHandle.Flags);
        }

        for (auto& entry : m_Images)
        {
            uint32_t imageInfoIdx = entry.second;
            ImageInfo& imageInfo = m_ImageInfos[imageInfoIdx];
            ResourceUseHandle& useHandle = imageInfo.UseHandle;
            CW_ENGINE_ASSERT(!useHandle.Used);
            useHandle.Used = true;
            entry.first->NotifyUsed(m_GlobalQueueIdx, m_QueueFamily, useHandle.Flags);
        }

        for (auto& entry : m_Buffers)
        {
            ResourceUseHandle& useHandle = entry.second.UseHandle;
            CW_ENGINE_ASSERT(!useHandle.Used);
            useHandle.Used = true;
            entry.first->NotifyUsed(m_GlobalQueueIdx, m_QueueFamily, useHandle.Flags);
        }

        for (auto& entry : m_SwapChains)
        {
            ResourceUseHandle& useHandle = entry.second;
            CW_ENGINE_ASSERT(!useHandle.Used);
            useHandle.Used = true;
            entry.first->NotifyUsed(m_GlobalQueueIdx, m_QueueFamily, useHandle.Flags);
        }

        for (auto& entry : m_TransitionInfoTemp)
        {
            entry.second.ImageBarriers.clear();
            entry.second.BufferBarriers.clear();
        }

        m_GraphicsPipeline = nullptr;
        m_ComputePipeline = nullptr;
        m_GraphicsPipelineRequiresBind = true;
        m_ComputePipelineRequiresBind = true;
        m_Framebuffer = nullptr;
        m_DescriptorSetsBindState = DescriptorSetBindFlagBits::Graphics | DescriptorSetBindFlagBits::Compute;
        m_QueuedLayoutTransitions.clear();
        m_BoundUniforms = nullptr;
        m_IndexBuffer = nullptr;
        m_VertexBuffers.clear();
        m_VertexInputsRequriesBind = true;
        m_ActiveSwapChains.clear();
    }

    void VulkanCmdBuffer::SetRenderTarget(const Ref<RenderTarget>& renderTarget, uint32_t readOnlyFlags,
                                          RenderSurfaceMask loadMask)
    {
        CW_ENGINE_ASSERT(m_State != State::Submitted);
        VulkanFramebuffer* newBuffer;
        VulkanSwapChain* swapChain = nullptr;
        if (renderTarget)
        {
            if (renderTarget->GetProperties().SwapChainTarget)
            {
                VulkanRenderWindow* window = static_cast<VulkanRenderWindow*>(renderTarget.get());
                window->AcquireBackBuffer();
                swapChain = window->GetSwapChain();
                m_ActiveSwapChains.insert(swapChain);
                newBuffer = window->GetFramebuffer();
            }
            else
                newBuffer = static_cast<VulkanRenderTexture*>(renderTarget.get())->GetFramebuffer();
        }
        else
        {
            newBuffer = nullptr;
        }
        m_RenderTarget = renderTarget;
        m_RenderTargetModified = false;

        if (loadMask.IsSet(RT_DEPTH) && !loadMask.IsSet(RT_STENCIL))
        {
            CW_ENGINE_WARN("SetRenderTarget() with invalid load mask. Depth enabled but stencil disabled.");
            loadMask.Set(RT_STENCIL);
        }

        if (!loadMask.IsSet(RT_DEPTH) && loadMask.IsSet(RT_STENCIL))
        {
            CW_ENGINE_WARN("SetRenderTarget() with invalid load mask. Stencil enabled but depth disabled.");
            loadMask.Set(RT_STENCIL);
        }

        if (m_Framebuffer == newBuffer && m_RenderTargetReadOnlyFlags == readOnlyFlags &&
            m_RenderTargetLoadMask == loadMask)
            return;

        if (IsInRenderPass())
            EndRenderPass();
        else if (m_ClearMask)
            ExecuteClearPass();
        if (m_Framebuffer != nullptr)
        {
            VulkanRenderPass* renderPass = m_Framebuffer->GetRenderPass();
            uint32_t numColors = renderPass->GetNumColorAttachments();
            for (uint32_t i = 0; i < numColors; i++)
            {
                const VulkanFramebufferAttachment& fbAttachment = m_Framebuffer->GetColorAttachment(i);
                uint32_t imageInfoIdx = m_Images[fbAttachment.Image];
                ImageInfo& imageInfo = m_ImageInfos[imageInfoIdx];

                ImageSubresourceInfo* subresourceInfos = &m_SubresourceInfoStorage[imageInfo.SubresourceInfoIdx];
                for (uint32_t j = 0; j < imageInfo.NumSubresourceInfos; j++)
                {
                    ImageSubresourceInfo& entry = subresourceInfos[j];
                    entry.UseFlags.Unset(ImageUseFlagBits::Framebuffer);
                    entry.FbUse.AccessFlags = VulkanAccessFlagBits::None;
                    entry.FbUse.Stages = 0;
                }
            }

            if (renderPass->HasDepthAttachment())
            {
                const VulkanFramebufferAttachment& fbAttachment = m_Framebuffer->GetDepthStencilAttachment();
                uint32_t imageInfoIdx = m_Images[fbAttachment.Image];
                ImageInfo& imageInfo = m_ImageInfos[imageInfoIdx];
                ImageSubresourceInfo* subresourceInfos = &m_SubresourceInfoStorage[imageInfo.SubresourceInfoIdx];
                for (uint32_t j = 0; j < imageInfo.NumSubresourceInfos; j++)
                {
                    ImageSubresourceInfo& entry = subresourceInfos[j];
                    entry.UseFlags.Unset(ImageUseFlagBits::Framebuffer);
                    entry.FbUse.AccessFlags = VulkanAccessFlagBits::None;
                    entry.FbUse.Stages = 0;
                }
            }
        }

        if (newBuffer == nullptr)
        {
            m_Framebuffer = nullptr;
            m_RenderTargetReadOnlyFlags = 0;
            m_RenderTargetLoadMask = RT_NONE;
        }
        else
        {
            m_Framebuffer = newBuffer;
            m_RenderTargetReadOnlyFlags = readOnlyFlags;
            m_RenderTargetLoadMask = loadMask;
        }

        SetUniforms(m_BoundUniforms);
        if (m_Framebuffer)
        {
            RegisterResource(m_Framebuffer, loadMask, readOnlyFlags);
            if (swapChain)
                RegisterResource(swapChain);
        }
        m_GraphicsPipelineRequiresBind = true;
    }

    bool VulkanCmdBuffer::CheckFenceStatus(bool blocking) const
    {
        VkResult result = vkWaitForFences(m_Device.GetLogicalDevice(), 1, &m_Fence, true, blocking ? 1000000000 : 0);
        CW_ENGINE_ASSERT(result == VK_SUCCESS || result == VK_TIMEOUT);

        return result == VK_SUCCESS;
    }

    bool VulkanCmdBuffer::IsReadyForRender() const
    {
        if (m_GraphicsPipeline == nullptr)
            return false;

        return m_RenderTarget != nullptr;
    }

    void VulkanCmdBuffer::SetDrawMode(DrawMode drawMode)
    {
        if (m_DrawMode == drawMode)
            return;
        m_DrawMode = drawMode;
        m_GraphicsPipelineRequiresBind = true;
    }

    void VulkanCmdBuffer::SetVertexBuffers(uint32_t idx, Ref<VertexBuffer>* buffers, uint32_t bufferCount)
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

    void VulkanCmdBuffer::SetIndexBuffer(const Ref<IndexBuffer>& indexBuffer)
    {
        m_IndexBuffer = std::static_pointer_cast<VulkanIndexBuffer>(indexBuffer);
        m_VertexInputsRequriesBind = true;
    }

    void VulkanCmdBuffer::SetUniforms(const Ref<UniformParams>& uniforms)
    {
        m_BoundUniforms = std::static_pointer_cast<VulkanUniformParams>(uniforms);
        if (m_BoundUniforms != nullptr)
            m_BoundUniformsDirty = true;
        else
        {
            m_NumBoundDescriptorSets = 0;
            m_BoundUniformsDirty = false;
        }

        m_DescriptorSetsBindState = DescriptorSetBindFlagBits::Graphics | DescriptorSetBindFlagBits::Compute;
    }

    void VulkanCmdBuffer::BindUniforms()
    {
        if (m_BoundUniformsDirty)
        {
            if (m_BoundUniforms != nullptr)
            {
                m_NumBoundDescriptorSets = m_BoundUniforms->GetNumSets();
                m_BoundUniforms->Prepare(*this, m_DescriptorSetsTemp);
            }
            else
                m_NumBoundDescriptorSets = 0;
            m_BoundUniformsDirty = false;
        }
        else
            m_NumBoundDescriptorSets = 0;
    }

    void VulkanCmdBuffer::Draw(uint32_t vertexOffset, uint32_t vertexCount, uint32_t instanceCount)
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

        if (m_DescriptorSetsBindState.IsSet(DescriptorSetBindFlagBits::Graphics))
        {
            if (m_NumBoundDescriptorSets > 0)
            {
                VkPipelineLayout pipelineLayout = m_GraphicsPipeline->GetLayout();
                vkCmdBindDescriptorSets(m_CmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0,
                                        m_NumBoundDescriptorSets, m_DescriptorSetsTemp, 0, nullptr);
            }
            m_DescriptorSetsBindState.Unset(DescriptorSetBindFlagBits::Graphics);
        }
        if (instanceCount <= 0)
            instanceCount = 1;
        vkCmdDraw(m_CmdBuffer, vertexCount, instanceCount, vertexOffset, 0);
    }

    void VulkanCmdBuffer::DrawIndexed(uint32_t startIdx, uint32_t idxCount, uint32_t vertexOffset,
                                      uint32_t instanceCount)
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

        if (m_DescriptorSetsBindState.IsSet(DescriptorSetBindFlagBits::Graphics))
        {
            if (m_NumBoundDescriptorSets > 0)
            {
                VkPipelineLayout pipelineLayout = m_GraphicsPipeline->GetLayout();
                vkCmdBindDescriptorSets(m_CmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0,
                                        m_NumBoundDescriptorSets, m_DescriptorSetsTemp, 0, nullptr);
            }
            m_DescriptorSetsBindState.Unset(DescriptorSetBindFlagBits::Graphics);
        }

        if (instanceCount <= 0)
            instanceCount = 1;
        vkCmdDrawIndexed(m_CmdBuffer, idxCount, instanceCount, startIdx, vertexOffset, 0);
    }

    void VulkanCmdBuffer::AllocateSemaphores(VkSemaphore* semaphores)
    {
        if (m_IntraQueueSemaphore != nullptr)
            m_IntraQueueSemaphore->Destroy();

        m_IntraQueueSemaphore = m_Device.GetResourceManager().Create<VulkanSemaphore>();
        semaphores[0] = m_IntraQueueSemaphore->GetHandle();

        for (uint32_t i = 0; i < MAX_VULKAN_CB_DEPENDENCIES; i++)
        {
            if (m_InterQueueSemaphores[i] != nullptr)
                m_InterQueueSemaphores[i]->Destroy();
            m_InterQueueSemaphores[i] = m_Device.GetResourceManager().Create<VulkanSemaphore>();
            semaphores[i + 1] = m_InterQueueSemaphores[i]->GetHandle();
        }

        m_NumUsedInterQueueSemaphores = 0;
    }

    VulkanSemaphore* VulkanCmdBuffer::RequestInterQueueSemaphore() const
    {
        if (m_NumUsedInterQueueSemaphores >= MAX_VULKAN_CB_DEPENDENCIES)
            return nullptr;
        return m_InterQueueSemaphores[m_NumUsedInterQueueSemaphores++];
    }

    void VulkanCmdBuffer::SetLayout(VkImage image, VkAccessFlags srcAccessFlags, VkAccessFlags dstAccessFlags,
                                    VkImageLayout oldLayout, VkImageLayout newLayout,
                                    const VkImageSubresourceRange& range)
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
        barrier.image = image;
        barrier.subresourceRange = range;
        VkPipelineStageFlags srcStage = GetPipelineStageFlags(srcAccessFlags);
        VkPipelineStageFlags dstStage = GetPipelineStageFlags(dstAccessFlags);
        vkCmdPipelineBarrier(m_CmdBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    void VulkanCmdBuffer::MemoryBarrier(VkBuffer buffer, VkAccessFlags srcAccessFlags, VkAccessFlags dstAccessFlags,
                                        VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage)
    {
        VkBufferMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barrier.pNext = nullptr;
        barrier.srcAccessMask = srcAccessFlags;
        barrier.dstAccessMask = dstAccessFlags;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.size = VK_WHOLE_SIZE;
        barrier.buffer = buffer;

        vkCmdPipelineBarrier(m_CmdBuffer, srcStage, dstStage, 0, 0, nullptr, 1, &barrier, 0, nullptr);
    }

    VulkanCmdBuffer::ImageSubresourceInfo& VulkanCmdBuffer::FindSubresourceInfo(VulkanImage* image, uint32_t face,
                                                                                uint32_t mip)
    {
        uint32_t imageInfoIdx = m_Images[image];
        ImageInfo& imageInfo = m_ImageInfos[imageInfoIdx];

        ImageSubresourceInfo* subresourceInfos = &m_SubresourceInfoStorage[imageInfo.SubresourceInfoIdx];
        for (uint32_t i = 0; i < imageInfo.NumSubresourceInfos; i++)
        {
            ImageSubresourceInfo& entry = subresourceInfos[i];
            if (face >= entry.Range.baseArrayLayer && face < (entry.Range.baseArrayLayer + entry.Range.layerCount) &&
                mip >= entry.Range.baseMipLevel && mip < (entry.Range.baseMipLevel + entry.Range.levelCount))
                return entry;
        }

        CW_ENGINE_ASSERT(false);
        return subresourceInfos[0];
    }

    void VulkanCmdBuffer::ResetQuery(VulkanQuery* query)
    {
        if (IsInRenderPass())
            m_QueuedQueryResets.push_back(query);
        else
            query->Reset(m_CmdBuffer);
    }

    void VulkanCmdBuffer::GetInProgressQueries(Vector<VulkanTimerQuery*>& timers,
                                               Vector<VulkanPipelineQuery*>& pipelines,
                                               Vector<VulkanOcclusionQuery*>& occlusions) const
    {
        for (auto entry : m_TimerQueries)
            if (entry->IsInProgress())
                timers.push_back(entry);
        for (auto entry : m_PipelineQueries)
            if (entry->IsInProgress())
                pipelines.push_back(entry);
        for (auto entry : m_OcclusionQueries)
            if (entry->IsInProgress())
                occlusions.push_back(entry);
    }

    RenderSurfaceMask VulkanCmdBuffer::GetFBReadMask()
    {
        VulkanRenderPass* renderPass = m_Framebuffer->GetRenderPass();
        RenderSurfaceMask readMask = RT_NONE;
        uint32_t numColorAttachments = renderPass->GetNumColorAttachments();
        for (uint32_t i = 0; i < numColorAttachments; i++)
        {
            const VulkanFramebufferAttachment& attachment = m_Framebuffer->GetColorAttachment(i);
            ImageSubresourceInfo& subresourceInfo =
              FindSubresourceInfo(attachment.Image, attachment.Surface.Face, attachment.Surface.MipLevel);
            bool readOnly = subresourceInfo.UseFlags.IsSet(ImageUseFlagBits::Shader);
            if (readOnly)
                readMask.Set((RenderSurfaceMaskBits)(1 << i));
        }

        if (renderPass->HasDepthAttachment())
        {
            const VulkanFramebufferAttachment& attachment = m_Framebuffer->GetDepthStencilAttachment();
            ImageSubresourceInfo& subresourceInfo =
              FindSubresourceInfo(attachment.Image, attachment.Surface.Face, attachment.Surface.MipLevel);

            bool readOnly = subresourceInfo.UseFlags.IsSet(ImageUseFlagBits::Shader);
            if (readOnly)
                readMask.Set(RT_DEPTH);

            if ((m_RenderTargetReadOnlyFlags & FBT_DEPTH) != 0)
                readMask.Set(RT_DEPTH);

            if ((m_RenderTargetReadOnlyFlags & FBT_STENCIL) != 0)
                readMask.Set(RT_STENCIL);
        }

        return readMask;
    }

    VkImageLayout VulkanCmdBuffer::GetCurrentLayout(VulkanImage* image, const VkImageSubresourceRange& range,
                                                    bool inRenderPass)
    {
        uint32_t face = range.baseArrayLayer;
        uint32_t mip = range.baseMipLevel;

        VulkanImageSubresource* subresource = image->GetSubresource(face, mip);
        auto iter = m_Images.find(image);
        if (iter == m_Images.end())
            return subresource->GetLayout();

        uint32_t imageInfoIdx = iter->second;
        ImageInfo& imageInfo = m_ImageInfos[imageInfoIdx];
        VulkanRenderPass* renderPass = nullptr;
        if (m_Framebuffer)
            renderPass = m_Framebuffer->GetRenderPass();

        ImageSubresourceInfo* subresourceInfos = &m_SubresourceInfoStorage[imageInfo.SubresourceInfoIdx];
        for (uint32_t i = 0; i < imageInfo.NumSubresourceInfos; i++)
        {
            ImageSubresourceInfo& entry = subresourceInfos[i];
            if (face >= entry.Range.baseArrayLayer && face < (entry.Range.baseArrayLayer + entry.Range.layerCount) &&
                mip >= entry.Range.baseMipLevel && mip < (entry.Range.baseMipLevel + entry.Range.levelCount))
            {
                if (entry.UseFlags.IsSet(ImageUseFlagBits::Framebuffer) && inRenderPass && m_Framebuffer)
                {
                    RenderSurfaceMask readMask = GetFBReadMask();
                    if (renderPass->HasDepthAttachment() && m_Framebuffer->GetDepthStencilAttachment().Image == image)
                    {
                        if (readMask.IsSet(RT_DEPTH))
                        {
                            if (readMask.IsSet(RT_STENCIL))
                                return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                            else
                                return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL_KHR;
                        }
                        else
                        {
                            if (readMask.IsSet(RT_STENCIL))
                                return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL_KHR;
                            else
                                return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                        }
                    }
                    else
                    {
                        uint32_t numColorAttachments = renderPass->GetNumColorAttachments();
                        for (uint32_t j = 0; j < numColorAttachments; j++)
                        {
                            const VulkanFramebufferAttachment& attachment = m_Framebuffer->GetColorAttachment(j);
                            if (attachment.Image == image)
                            {
                                if (readMask.IsSet((RenderSurfaceMaskBits)(1 << attachment.Index)))
                                    return VK_IMAGE_LAYOUT_GENERAL;
                                else
                                    return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                            }
                        }
                    }
                }
                return entry.RequiredLayout;
            }
        }

        return subresource->GetLayout();
    }

} // namespace Crowny
