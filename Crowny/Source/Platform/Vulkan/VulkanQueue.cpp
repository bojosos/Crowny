#include "cwpch.h"

#include "Platform/Vulkan/VulkanQueue.h"

namespace Crowny
{
    
    VulkanQueue::VulkanQueue(VulkanDevice& device, VkQueue* queue)
        : m_Device(device), m_Queue(queue) { }

    void VulkanQueue::Submit(VulkanCmdBuffer* cmdBuffer, VulkanSemaphore** waitSemaphores, uint32_t semaphoreLength)
    {
        VkSemaphore signals[];
        cmdBuffer->AllocateSemaphores(signals);
        VkCommandBuffer vkCmdBuffer = cmdBuffer->GetHandle();

        m_SemaphoresTemp.resize(semaphoreLength);
        PrepareSemaphores(waitSemaphores, m_SemaphoresTemp.data(), semaphoreLength);
        
        VkSubmitInfo submitInfo;
        GetSubmitInfo(&vkCmdBuffer, signalSemaphores, MAX_VULKAN_CB_DEPENDENCIES + 1, m_SemaphoresTemp.data(), semaphoreLength, submitInfo);
        
        VkResult result = vkQueueSubmit(m_Queue, 1, &submitInfo, cmdBuffer->GetFence());
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
        cmdBuffer->SetSubmitted();
        m_LastCommandBuffer = cmdBuffer;
        m_LastCBSemaphoreUsed = false;
        m_ActuveSubmissions.push_back(SubmitInfo(cmdBuffer, m_NextSubmitIdx++, semaphoreLength, 1);
        m_ActiveBuffers.push(cmdBuffer);
    }

    bool VulkanQueue::IsExecuting() const
    {
        if (m_LastCommandBuffer == nullptr)
            return false;
        return m_LastCommandBuffer->IsSubmitted();
    }

    void VulkanQueue::GetSubmitInfo(VkCommandBuffer* cmdBuffer, VkSemaphore* signalSemaphore,
                                    uint32_t numSingalSemaphores, VkSemaphore* waitSemaphores, uint32_t numWaitSemaphores, VkSubmitInfo& submitInfo)
    {
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pNext = nullptr;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = cmdBuffer;
        submitInfo.signalSemaphoreCount = numSignalSemaphores;
        submitInfo.pSignalSemaphores = signalSemaphores;
        submitInfo.waitSemaphoreCount = numWaitSemaphores;
        
        if (numWaitSemaphores > 0)
        {
            submitInfo.pWaitSemaphores = waitSemaphores;
            submitInfo.pWaitDstStageMask = m_SubmitDstWaitMask;
        }
        else
        {
            submitInfo.pWaitSemaphores = nullptr;
            submitInfo.pWaitDstStageMask = nullptr;
        }
    }
    
    VkResult VulkanQueue::Present(VulkanSwapChain* swapChain, VulkanSemaphore** waitSemaphores, uint32_t numSemaphores)
    {
        uint32_t backIdx;
        if (!swapChain->PrepareFromPresent(backIdx))
            return VK_SUCCESS;
        
        m_SemaphoresTemp.resize(numSemaphores + 1);
        PrepareSemaphores(waitSemaphores, m_SemaphoresTemp.data(), numSemaphores);
        
        VkSwapChainKHR vkWwapChain = swapChain->GetHandle();
        VkPresentInfoKHR presentInfo;
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.pNext = nullptr;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &vkSwapChain;
        presentInfo.pImageIndices = &backIdx;
        presentInfo.pResults = nullptr;
        
        if (numSemaphores > 0)
        {
            presentInfo.pWaitSemaphores = m_SemaphoresTemp.data();
            presentInfo.waitSemaphoreCount = numSemaphores;
        }
        else
        {
            presentInfo.pWaitSemaphores = nullptr;
            presentInfo.waitSemaphoreCount = 0;
        }

        VkResult result = vkQueuePresentKHR(m_Queue, &presentInfo);
        CW_ENGINE_ASSERT(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR);

        m_ActiveSubmissions.push_back(SubmitInfo(nullptr, m_NextSubmitIdx++, numSemaphores, 0));
        return result;
    }

    void VulkanQueue::WaitIdle() const
    {
        VkResult result = vkQueueWaitIdle(m_Queue);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
    }
    
    void VulkanQueue::PrepareSemaphores(VulkanSemaphore** inSemaphores, VkSemaphore* outSemaphores, uint32_t& semaphoreCount)
    {
        uint32_t semaphoreIdx = 0;
        for (uint32_t i = 0; i < semaphoreCount; i++)
        {
            VulkanSemaphore* semaphore = inSemaphores[i];
            semaphore->NotifyBound();
            semaphore->NotifyUsed(0, 0, VulkanAccessFlag::Read | VulkanAccessFlag::Write);
            
            outSemaphores[semaphoreIdx++] = semaphore->GetHandle();
            m_ActiveSemaphores.push(semaphore);
        }
        
        if (m_LastCommandBuffer != nullptr && m_LastCommandBuffer->IsSubmitted() && !m_LastCBSemaphoreUsed)
        {
            VulkanSemaphore* prev = m_LastCommandBuffer->GetIntraQueueSemaphore();
            prev->NotifyBound();
            semaphore->NotifyUsed(0, 0, VulkanAccessFlag::Read | VulkanAccessFlag::Write);
            outSemaphores[semaphoreIdx++] = prev->GetHandle();
            m_ActiveSemaphores.push(prev);
            
            m_LastCBSemaphoreUsed = true;
        }
        
        semaphoresCount = semaphoreIdx;
    }

    void VulkanQueue::Submit(VulkanCommandBuffer* cmdBuffer, VulkanSemaphore** waitSemaphores, uint32_t semaphoresCount)
    {
        VkSemaphore signalSemaphores[MAX_VULKAN_CB_DEPENDENCIES + 1];
        cmdBuffer->AllocateSemaphores(signalSemaphores);
        m_SemaphoresTemp.resize(semaphoreCount + 1);
        PrepareSemaphores(waitSemaphores, m_SemaphoresTemp.data(), semaphoresCount);
        VkSubmitInfo submitInfo;
        VkCommandBuffer vkCmdBuffer = cmdBuffer->GetHandle();
        GetSubmitInfo(&vkCmdBuffer, signalSemaphores, MAX_VULKAN_CB_DEPENDENCIES + 1, m_SemaphoresTemp.data(), semaphoresCount, submitInfo);
        VkResult result = vkQueueSubmit(m_Queue, 1, &submitInfo, cmdBuffer->GetFence());
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
        cmdBuffer->SetIsSubmitted();
        m_LastCommandBuffer = cmdBuffer;
        m_LastCBSemaphoreUsed = false;

        m_ActiveSubmissions.push_back(SubmitInfo(cmdBuffer, m_NextSubmitIdx++, semaphoresCount + 1));
        m_ActiveBuffers.push(cmdBuffer);
    }

    void VulkanQueue::QueueSubmit(VulkanCmdBuffer* cmdBuffer, VulkanSemaphore* waitSemaphores, uint32_t semaphoreCount)
    {
        m_QueuedBuffers.push_back(SubmitInfo(cmdBuffer, 0, semaphoresCount, 1));
        for (uint32_t i = 0; i < semaphoreCount; i++)
            m_QueuedSemaphores.push_back(waitSemaphores[i]);
    }

    void VulkanQueue::SubmitQueued()
    {
        uint32_t numCbs = (uint32_t)m_QueuedBuffers.size();
        if (numCbs == 0)
            return;
        
        uint32_t totalNumWaitSemaphores = (uint32_t)m_QueuedSemaphores.size() + numCbs;
        uint32_t signalSemaphoresPerCB = MAX_VULKAN_CB_DEPENDENCIES + 1;
        
        byte* data = new byte[(sizeof(VkSubmitInfo) + sizeof(VkCommandBuffer)) * numCbs + sizeof(VkSemaphore) * signalSemaphoresPerCB * numCbs + sizeof(VkSemaphore) * totalNumWaitSemaphores];
        byte* dataPtr = data;
        
        VkSubmitInfo* submitInfos = (VkSubmitInfo*)dataPtr;
        dataPtr += sizeof(VkSubmitInfo) * numCbs;

        VkCommandBuffer* commandBuffers = (VkCommandBuffer*)dataPtr;
        dataPtr += sizeof(VkCommandBuffer) * numCbs;
        
        VkSemaphore* signalSemaphores = (VkSemaphore*)dataPtr;
        dataPtr += sizeof(VkSemaphore) * signalSemaphoresPerCB * numCbs;

        VkSemaphore* waitSemaphores = (VkSemaphore*)dataPtr;
        dataPtr += sizeof(VkSemaphore) * totalNumWaitSemaphores;

        uint32_t readSemaphoreIdx = 0;
        uint32_t writeSemaphoreIdx = 0;
        uint32_t signalSemaphoreIdx = 0;

        for (uint32_t i = 0; i < numCbs; i++)
        {
            const SubmitInfo& entry = m_QueuedBuffers[i];
            commandBuffers[i] = entry.CmdBuffer->GetHandle();
            
            entry.CmdBuffer->AllocateSemaphores(&signalSemaphores[signalSemaphoreIdx]);
            uint32_t semaphoresCount = entry.NumSemaphores;
            PrepareSemaphores(m_QueuedSemaphores.data() + readSemaphoreIdx, &waitSemaphores[writeSemaphoreIdx], semaphoresCount);
            GetSubmitInfo(&commandBuffers[i], &signalSemaphores[signalSemaphoreIdx], signalSemaphoresPerCB, &waitSemaphores[writeSemaphoreIdx], semaphoresCount, submitInfos[i]);
            entry.CmdBuffer->SetIsSubmitted();
            m_LastCommandBuffer = entry.CmdBuffer;
            m_LastCBSemaphoreUsed = false;
            m_ActiveBuffers.push(entry.CmdBuffer);
            readSemaphoreIdx += entry.NumSemaphores;
            writeSemaphoreIdx += semaphoresCount;
            signalSemaphoreIdx += signalSemaphoresPerCB;
        }

        VulkanCommandBuffer* lastCB = m_QueuedBuffers[numCbs - 1].CmdBuffer;
        uint32_t totalNumSemaphores = writeSemaphoreIdx;
        m_ActiveSubmissions.push_back(SubmitInfo(lastCB, m_NextSubmitIdx++, totalNumSemaphores, numCbs));

        VkResult result = vkQueueSubmit(m_Queue, numCbs, submitInfos, m_LastCommandBuffer->GetFence());
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
        m_QueuedBuffers.clear();
        m_QueuedSemaphores.clear();
        delete[] data;
    }

    void VulkanQueue::RefreshStates(bool forceWait, bool queueEmpty)
    {
        
    }
    
}