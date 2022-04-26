#pragma once

#include "Platform/Vulkan/VulkanUtils.h"

#include "Platform/Vulkan/VulkanCommandBuffer.h"
#include "Platform/Vulkan/VulkanGpuBufferManager.h"
#include "Platform/Vulkan/VulkanQueue.h"
#include "Platform/Vulkan/VulkanResource.h"

namespace Crowny
{

    class VulkanDescriptorManager;
    class VulkanQueryPool;

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
        VulkanQueryPool& GetQueryPool() const { return *m_QueryPool; }

        uint32_t GetMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties, VkBool32* found = nullptr);
        VkDevice GetLogicalDevice() const { return m_LogicalDevice; }
        VkPhysicalDeviceProperties GetDeviceProperties() const { return m_DeviceProperties; }
        VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
        VkPhysicalDeviceFeatures GetDeviceFeatures() const { return m_DeviceFeatures; }
        void SetPrimary();
        void SetIndex(uint32_t idx);
        uint32_t GetIndex() const { return 0; }
        uint32_t GetNumQueues(GpuQueueType type) const { return (uint32_t)m_QueueInfos[(int)type].Queues.size(); }
        VulkanQueue* GetQueue(GpuQueueType type, uint32_t idx) const { return m_QueueInfos[(int)type].Queues[idx]; }
        uint32_t GetQueueFamily(GpuQueueType type) const { return m_QueueInfos[(int)type].FamilyIdx; }
        uint32_t GetQueueMask(GpuQueueType type, uint32_t queueIdx) const;
        void Refresh(bool wait = false);

        uint32_t FindMemoryType(uint32_t requirement, VkMemoryPropertyFlags flags);
        VmaAllocation AllocateMemory(VkBuffer buffer, VkMemoryPropertyFlags flags);
        VmaAllocation AllocateMemory(VkImage image, VkMemoryPropertyFlags flags);
        void GetAllocationInfo(VmaAllocation allocation, VkDeviceMemory& memory, VkDeviceSize& offset);
        void FreeMemory(VmaAllocation allocation);

        VulkanDescriptorManager& GetDescriptorManager() const { return *m_DescriptorManager; }
        VulkanResourceManager& GetResourceManager() const { return *m_ResourceManager; }

        void WaitIdle();

    private:
        VulkanCommandBufferPool* m_CommandBufferPool;
        VulkanQueryPool* m_QueryPool;

        struct QueueInfo
        {
            uint32_t FamilyIdx = -1;
            Vector<VulkanQueue*> Queues;
        };
        QueueInfo m_QueueInfos[QUEUE_COUNT];

        VkPhysicalDevice m_PhysicalDevice;
        VkDevice m_LogicalDevice = nullptr;
        VmaAllocator m_Allocator;

        VulkanDescriptorManager* m_DescriptorManager;
        VulkanResourceManager* m_ResourceManager;

        VkPhysicalDeviceProperties m_DeviceProperties;
        VkPhysicalDeviceFeatures m_DeviceFeatures;
        VkPhysicalDeviceFeatures m_EnabledFeatures;
        VkPhysicalDeviceMemoryProperties m_MemoryProperties;
        Vector<String> m_SupportedExtensions;
    };
} // namespace Crowny
