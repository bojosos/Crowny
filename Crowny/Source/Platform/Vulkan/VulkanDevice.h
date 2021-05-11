#pragma once

#include "Platform/Vulkan/VulkanUtils.h"

#include "Platform/Vulkan/VulkanCommandBuffer.h"
#include "Platform/Vulkan/VulkanQueue.h"

namespace Crowny
{
    struct SurfaceFormat
    {
        VkFormat ColorFormat;
        VkFormat DepthFormat;
        VkColorSpaceKHR ColorSpace;
    };
    
    class VulkanDevice
    {
    public:
        VulkanDevice(VkPhysicalDevice device, uint32_t deviceIdx);
        ~VulkanDevice();
        
        SurfaceFormat GetSurfaceFormat(const VkSurfaceKHR& surface) const;
        VulkanCommandBufferPool& GetCmdBufferPool() const { return *m_CommandBufferPool; }
        uint32_t GetMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties, VkBool32* found = nullptr);
        VkDevice GetLogicalDevice() const { return m_LogicalDevice; }
        VkPhysicalDeviceProperties GetDeviceProperties() const { return m_DeviceProperties; }
        VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
        VkPhysicalDeviceFeatures GetDeviceFeatures() const { return m_DeviceFeatures; }
        void SetPrimary();
        void SetIndex(uint32_t idx);
        VulkanQueue* GetQueue(GpuQueueType type, uint32_t idx) const { return m_QueueInfos[(int)type].Queues[idx]; }
        uint32_t GetQueueFamily(GpuQueueType type) const { return m_QueueInfos[(int)type].FamilyIdx; }
        void Refresh(bool wait = false);
        uint32_t FindMemoryType(uint32_t requirement, VkMemoryPropertyFlags flags);
        VmaAllocation AllocateMemory(VkBuffer buffer, VkMemoryPropertyFlags flags);
        VmaAllocation AllocateMemory(VkImage image, VkMemoryPropertyFlags flags);
        void FreeMemory(VmaAllocation allocation);
        
        void WaitIdle();
        uint32_t GetNumQueues(GpuQueueType type) const { return (uint32_t)m_QueueInfos[(int)type].Queues.size(); }
    private:
    private:
        VulkanCommandBufferPool* m_CommandBufferPool;
        
        struct QueueInfo
        {
            uint32_t FamilyIdx = -1;
            std::vector<VulkanQueue*> Queues;
        };
        QueueInfo m_QueueInfos[QUEUE_COUNT];
        
        VkPhysicalDevice m_PhysicalDevice;
        VkDevice m_LogicalDevice = nullptr;
        VmaAllocator m_Allocator;

        VkPhysicalDeviceProperties m_DeviceProperties;
        VkPhysicalDeviceFeatures m_DeviceFeatures;
        VkPhysicalDeviceFeatures m_EnabledFeatures;
        VkPhysicalDeviceMemoryProperties m_MemoryProperties;
        std::vector<std::string> m_SupportedExtensions;
    };
}