#pragma once

#include "Platform/Vulkan/VulkanUtils.h"

#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanSwapChain.h"
#include "Platform/Vulkan/VulkanCommandBuffer.h"

namespace Crowny
{
    class VulkanQueue
    {
    public:
        VulkanQueue(VulkanDevice& device, VkQueue queue, GpuQueueType type, uint32_t index);

        VkQueue GetHandle() const { return m_Queue; }
        VulkanDevice& GetDevice() const { return m_Device; }
        GpuQueueType GetType() const { return m_Type; }
        bool IsExecuting() const;

        void Submit(VulkanCommandBuffer* buffer, VulkanSemaphore** waitSemaphores, uint32_t semaphoreLength);
        void SubmitQueued();
        void QueueSubmit(VulkanCommandBuffer* cmdBuffer, VulkanSemaphore* waitSemaphores, uint32_t semaphoreCount);

        VkResult Present(VulkanSwapChain* swapChain, VulkanSemaphore** waitSemaphores, uint32_t semaphoreLength);
        void PrepareSemaphores(VulkanSemaphore** inSemaphores, VkSemaphore* outSemaphores, uint32_t& semaphoreCount);
        void WaitIdle() const;
        void RefreshStates(bool forceWait, bool queueEmpty = false);

        void GetSubmitInfo(VkCommandBuffer* cmdBuffer, VkSemaphore* signalSemaphores, uint32_t numSemaphores, 
                           VkSemaphore* waitSemaphores, uint32_t numWaitSemaphores, VkSubmitInfo& submitInfo);
        
        struct SubmitInfo
        {
            VulkanCommandBuffer* CmdBuffer;
            uint32_t SubmitIdx;
            uint32_t NumSemaphores;
            uint32_t NumCommandBuffers;
        };
    private:
        VkQueue m_Queue;
        VulkanDevice& m_Device;
        GpuQueueType m_Type;
        uint32_t m_Index;

        std::vector<SubmitInfo> m_QueuedBuffers;
        std::vector<VulkanSemaphore*> m_QueuedSemaphores;
        std::list<SubmitInfo> m_ActiveSubmissions;
        std::queue<VulkanCommandBuffer*> m_ActiveBuffers;
        std::queue<VulkanSemaphore*> m_ActiveSemaphores;
        VulkanCommandBuffer* m_LastCommandBuffer = nullptr;
        bool m_LastCommandBufferUsed = false;
        uint32_t m_NextSubmitIdx = 1;
        std::vector<VkSemaphore> m_SemaphoresTemp;
    };
}