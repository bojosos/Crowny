#include "cwpch.h"

#include "Platform/Vulkan/VulkanCommandBuffer.h"
#include "Platform/Vulkan/VulkanQueue.h"
#include "Platform/Vulkan/VulkanSwapChain.h"

#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanUtils.h"

namespace Crowny
{

    VulkanQueue::VulkanQueue(VulkanDevice& device, VkQueue queue, GpuQueueType type, uint32_t index)
      : m_Device(device), m_Queue(queue), m_Type(type), m_Index(index)
    {
        for (uint32_t i = 0; i < MAX_UNIQUE_QUEUES; i++)
            m_SubmitDstWaitMask[i] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        // m_SubmitDstWaitMask[i] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }

    void VulkanQueue::Submit(VulkanCmdBuffer* cmdBuffer, VulkanSemaphore** waitSemaphores, uint32_t semaphoreCount)
    {
        Lock lock(m_SubmitMutex);

        VkSemaphore signalSemaphores[MAX_VULKAN_CB_DEPENDENCIES + 1];
        cmdBuffer->AllocateSemaphores(signalSemaphores);
        VkCommandBuffer vkCmdBuffer = cmdBuffer->GetHandle();

        m_SemaphoresTemp.resize(semaphoreCount + 1);
        PrepareSemaphores(waitSemaphores, m_SemaphoresTemp.data(), semaphoreCount);

        VkSubmitInfo submitInfo;
        GetSubmitInfo(&vkCmdBuffer, signalSemaphores, MAX_VULKAN_CB_DEPENDENCIES + 1, m_SemaphoresTemp.data(), semaphoreCount, submitInfo);

        VkResult result = vkQueueSubmit(m_Queue, 1, &submitInfo, cmdBuffer->GetFence());
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
        cmdBuffer->SetIsSubmitted();
        m_ActiveSubmissions.push_back(SubmitInfo(cmdBuffer, m_NextSubmitIdx++, semaphoreCount, 1));
        m_ActiveBuffers.push(cmdBuffer);
        m_LastCommandBuffer = cmdBuffer;
        m_LastCBSemaphoreUsed = false;
    }

    bool VulkanQueue::IsExecuting() const
    {
        Lock lock(m_SubmitMutex);
        if (m_LastCommandBuffer == nullptr)
            return false;
        return m_LastCommandBuffer->IsSubmitted();
    }

    VulkanCmdBuffer* VulkanQueue::GetLastCommandBuffer() const
    {
        Lock lock(m_SubmitMutex);
        return m_LastCommandBuffer;
    }

    bool VulkanQueue::RequestLastCommandBufferSemaphore(VulkanSemaphore*& semaphore) const
    {
        Lock lock(m_SubmitMutex);
        semaphore = nullptr;
        if (m_LastCommandBuffer == nullptr || !m_LastCommandBuffer->IsSubmitted())
            return true;

        semaphore = m_LastCommandBuffer->RequestInterQueueSemaphore();
        return semaphore != nullptr;
    }

    void VulkanQueue::GetSubmitInfo(VkCommandBuffer* cmdBuffer, VkSemaphore* signalSemaphores, uint32_t numSignalSemaphores,
                                    VkSemaphore* waitSemaphores, uint32_t numWaitSemaphores, VkSubmitInfo& submitInfo)
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
        if (!swapChain->PrepareForPresent(backIdx))
            return VK_SUCCESS;

        Lock lock(m_SubmitMutex);

        m_SemaphoresTemp.resize(numSemaphores + 1);
        PrepareSemaphores(waitSemaphores, m_SemaphoresTemp.data(), numSemaphores);

        VkSwapchainKHR vkSwapChain = swapChain->GetHandle();
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
        VkResult result;
        {
            Lock lock(m_SubmitMutex);
            result = vkQueueWaitIdle(m_Queue);
        }
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
    }

    void VulkanQueue::PrepareSemaphores(VulkanSemaphore** inSemaphores, VkSemaphore* outSemaphores, uint32_t& semaphoreCount)
    {
        uint32_t semaphoreIdx = 0;
        for (uint32_t i = 0; i < semaphoreCount; i++)
        {
            VulkanSemaphore* semaphore = inSemaphores[i];
            semaphore->NotifyBound();
            semaphore->NotifyUsed(0, 0, VulkanAccessFlagBits::Read | VulkanAccessFlagBits::Write);
            outSemaphores[semaphoreIdx++] = semaphore->GetHandle();
            m_ActiveSemaphores.push(semaphore);
        }

        if (m_LastCommandBuffer != nullptr && m_LastCommandBuffer->IsSubmitted() && !m_LastCBSemaphoreUsed)
        {
            VulkanSemaphore* prevSemaphore = m_LastCommandBuffer->GetIntraQueueSemaphore();
            prevSemaphore->NotifyBound();
            prevSemaphore->NotifyUsed(0, 0, VulkanAccessFlagBits::Read | VulkanAccessFlagBits::Write);
            outSemaphores[semaphoreIdx++] = prevSemaphore->GetHandle();
            m_ActiveSemaphores.push(prevSemaphore);
            m_LastCBSemaphoreUsed = true;
        }
        semaphoreCount = semaphoreIdx;
    }

    void VulkanQueue::QueueSubmit(VulkanCmdBuffer* cmdBuffer, VulkanSemaphore** waitSemaphores, uint32_t semaphoreCount)
    {
        Lock lock(m_SubmitMutex);
        m_QueuedBuffers.push_back(SubmitInfo(cmdBuffer, 0, semaphoreCount, 1));
        for (uint32_t i = 0; i < semaphoreCount; i++)
            m_QueuedSemaphores.push_back(waitSemaphores[i]);
    }

    void VulkanQueue::SubmitQueued()
    {
        Lock lock(m_SubmitMutex);

        uint32_t numCbs = (uint32_t)m_QueuedBuffers.size();
        if (numCbs == 0)
            return;

        uint32_t totalNumWaitSemaphores = (uint32_t)m_QueuedSemaphores.size() + numCbs;
        uint32_t signalSemaphoresPerCB = MAX_VULKAN_CB_DEPENDENCIES + 1;

        m_SubmitInfosScratch.resize(numCbs);
        m_CmdBuffersScratch.resize(numCbs);
        m_SignalSemaphoresScratch.resize(signalSemaphoresPerCB * numCbs);
        m_WaitSemaphoresScratch.resize(totalNumWaitSemaphores);

        VkSubmitInfo* submitInfos = m_SubmitInfosScratch.data();
        VkCommandBuffer* commandBuffers = m_CmdBuffersScratch.data();
        VkSemaphore* signalSemaphores = m_SignalSemaphoresScratch.data();
        VkSemaphore* waitSemaphores = m_WaitSemaphoresScratch.data();

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
            GetSubmitInfo(&commandBuffers[i], &signalSemaphores[signalSemaphoreIdx], signalSemaphoresPerCB, &waitSemaphores[writeSemaphoreIdx],
                          semaphoresCount, submitInfos[i]);
            entry.CmdBuffer->SetIsSubmitted();
            m_LastCommandBuffer = entry.CmdBuffer;
            m_LastCBSemaphoreUsed = false;
            m_ActiveBuffers.push(entry.CmdBuffer);
            readSemaphoreIdx += entry.NumSemaphores;
            writeSemaphoreIdx += semaphoresCount;
            signalSemaphoreIdx += signalSemaphoresPerCB;
        }

        VulkanCmdBuffer* lastCB = m_QueuedBuffers[numCbs - 1].CmdBuffer;
        uint32_t totalNumSemaphores = writeSemaphoreIdx;
        m_ActiveSubmissions.push_back(SubmitInfo(lastCB, m_NextSubmitIdx++, totalNumSemaphores, numCbs));

        VkResult result = vkQueueSubmit(m_Queue, numCbs, submitInfos, m_LastCommandBuffer->GetFence());
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
        m_QueuedBuffers.clear();
        m_QueuedSemaphores.clear();
    }

    void VulkanQueue::Refresh(bool wait, bool empty)
    {
        // Collect completed work under the lock, then do the expensive
        // cleanup (Reset → vkFreeMemory) outside it. Holding the lock
        // during driver calls can deadlock on Intel drivers (igvk64.dll).
        Vector<VulkanSemaphore*> semaphoresToNotify;
        Vector<VulkanCmdBuffer*> buffersToReset;

        {
            Lock lock(m_SubmitMutex);
            uint32_t lastFinished = 0;
            auto iter = m_ActiveSubmissions.begin();
            while (iter != m_ActiveSubmissions.end())
            {
                VulkanCmdBuffer* cmdBuffer = iter->CmdBuffer;
                if (cmdBuffer == nullptr)
                {
                    ++iter;
                    continue;
                }
                if (!cmdBuffer->CheckFenceStatus(wait))
                {
                    CW_ENGINE_ASSERT(!wait);
                    break;
                }
                lastFinished = iter->SubmitIdx;
                ++iter;
            }

            if (empty)
                lastFinished = m_NextSubmitIdx - 1;

            while (!m_ActiveSubmissions.empty())
            {
                const SubmitInfo& front = m_ActiveSubmissions.front();
                if (front.SubmitIdx > lastFinished)
                    break;

                for (uint32_t i = 0; i < front.NumSemaphores; i++)
                {
                    semaphoresToNotify.push_back(m_ActiveSemaphores.front());
                    m_ActiveSemaphores.pop();
                }

                for (uint32_t i = 0; i < front.NumCommandBuffers; i++)
                {
                    buffersToReset.push_back(m_ActiveBuffers.front());
                    m_ActiveBuffers.pop();
                }
                m_ActiveSubmissions.pop_front();
            }
        }

        // Heavy cleanup outside the lock — driver calls (vkFreeMemory etc.)
        for (VulkanSemaphore* semaphore : semaphoresToNotify)
            semaphore->NotifyDone(0, VulkanAccessFlagBits::Read | VulkanAccessFlagBits::Write);

        for (VulkanCmdBuffer* cmdBuffer : buffersToReset)
            cmdBuffer->Reset();
    }

} // namespace Crowny
