#pragma once

#include "Platform/Vulkan/VulkanUtils.h"

#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanSwapChain.h"

namespace Crowny
{
    class VulkanCmdBuffer;

    class VulkanQueue
    {
    public:
        VulkanQueue(VulkanDevice& device, VkQueue queue, GpuQueueType type, uint32_t index);

        VkQueue GetHandle() const { return m_Queue; }
        VulkanDevice& GetDevice() const { return m_Device; }
        GpuQueueType GetType() const { return m_Type; }
        bool IsExecuting() const;

        VulkanCmdBuffer* GetLastCommandBuffer() const { return m_LastCommandBuffer; }

        void Submit(VulkanCmdBuffer* buffer, VulkanSemaphore** waitSemaphores, uint32_t semaphoreLength);
        void SubmitQueued();
        void QueueSubmit(VulkanCmdBuffer* cmdBuffer, VulkanSemaphore** waitSemaphores, uint32_t semaphoreCount);

        VkResult Present(VulkanSwapChain* swapChain, VulkanSemaphore** waitSemaphores, uint32_t semaphoreLength);
        void WaitIdle() const;

        void GetSubmitInfo(VkCommandBuffer* cmdBuffer, VkSemaphore* signalSemaphores, uint32_t numSemaphores,
                           VkSemaphore* waitSemaphores, uint32_t numWaitSemaphores, VkSubmitInfo& submitInfo);
        void Refresh(bool wait, bool queueEmpty);

    private:
        void PrepareSemaphores(VulkanSemaphore** inSemaphores, VkSemaphore* outSemaphores, uint32_t& semaphoreCount);

        struct SubmitInfo
        {
            SubmitInfo(VulkanCmdBuffer* buffer, uint32_t idx, uint32_t semaphores, uint32_t cmdBuffers)
              : CmdBuffer(buffer), SubmitIdx(idx), NumSemaphores(semaphores), NumCommandBuffers(cmdBuffers)
            {
            }

            VulkanCmdBuffer* CmdBuffer;
            uint32_t SubmitIdx;
            uint32_t NumSemaphores;
            uint32_t NumCommandBuffers;
        };

    private:
        VkQueue m_Queue;
        VulkanDevice& m_Device;
        GpuQueueType m_Type;
        uint32_t m_Index;

        VkPipelineStageFlags m_SubmitDstWaitMask[MAX_UNIQUE_QUEUES];
        Vector<SubmitInfo> m_QueuedBuffers;
        Vector<VulkanSemaphore*> m_QueuedSemaphores;
        std::list<SubmitInfo> m_ActiveSubmissions;
        std::queue<VulkanCmdBuffer*> m_ActiveBuffers;
        std::queue<VulkanSemaphore*> m_ActiveSemaphores;
        VulkanCmdBuffer* m_LastCommandBuffer = nullptr;
        bool m_LastCBSemaphoreUsed = false;
        uint32_t m_NextSubmitIdx = 1;
        Vector<VkSemaphore> m_SemaphoresTemp;
    };
} // namespace Crowny