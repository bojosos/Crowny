#pragma once

#include "Crowny/Common/RefCounted.h"
#include "Platform/Vulkan/VulkanUtils.h"

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

    class VulkanDescriptorManager;
    class VulkanResourceManager;

    class VulkanDevice : public RefCounted
    {
    public:
        VulkanDevice(VkPhysicalDevice device, uint32_t deviceIdx);
        ~VulkanDevice();

        SurfaceFormat GetSurfaceFormat(const VkSurfaceKHR& surface) const;

        VulkanCommandBufferPool& GetCmdBufferPool() const { return *m_CommandBufferPool; }
        VulkanQueryPool& GetQueryPool() const { return *m_QueryPool; }
        VkDevice GetLogicalDevice() const { return m_LogicalDevice; }
        const VkPhysicalDeviceProperties& GetDeviceProperties() const { return m_DeviceProperties; }
        VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
        const VkPhysicalDeviceFeatures& GetDeviceFeatures() const { return m_DeviceFeatures.features; }
        VkPipelineCache GetPipelineCache() const { return m_PipelineCache; }
        void SetPrimary();
        void SetIndex(uint32_t idx);
        uint32_t GetIndex() const { return 0; }
        uint32_t GetNumQueues(GpuQueueType type) const { return (uint32_t)m_QueueInfos[(int)type].Queues.size(); }
        VulkanQueue* GetQueue(GpuQueueType type, uint32_t idx) const { return m_QueueInfos[(int)type].Queues[idx]; }
        uint32_t GetQueueFamily(GpuQueueType type) const { return m_QueueInfos[(int)type].FamilyIdx; }
        uint32_t GetQueueMask(GpuQueueType type, uint32_t queueIdx) const;
        void Refresh(bool wait = false);

        uint32_t FindMemoryType(uint32_t requirement, VkMemoryPropertyFlags flags);
        VmaAllocation AllocateMemory(VkBuffer buffer, VkMemoryPropertyFlags flags, const char* tag = nullptr);
        VmaAllocation AllocateMemory(VkImage image, VkMemoryPropertyFlags flags, const char* tag = nullptr);
        void GetAllocationInfo(VmaAllocation allocation, VkDeviceMemory& memory, VkDeviceSize& offset);
        void FreeMemory(VmaAllocation allocation);
        void SetAllocationName(VmaAllocation allocation, const char* name);

        const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& GetRayTracingDeviceProperties() const { return m_RayTracingPipelineProperties; }

        VulkanDescriptorManager& GetDescriptorManager() const { return *m_DescriptorManager; }
        VulkanResourceManager& GetResourceManager() const { return *m_ResourceManager; }

        void WaitIdle();

    private:
        VulkanCommandBufferPool* m_CommandBufferPool = nullptr;
        VulkanQueryPool* m_QueryPool = nullptr;

        struct QueueInfo
        {
            uint32_t FamilyIdx = -1;
            Vector<VulkanQueue*> Queues;
        };
        QueueInfo m_QueueInfos[QUEUE_COUNT];

        VkPhysicalDevice m_PhysicalDevice;
        VkDevice m_LogicalDevice = nullptr;
        VmaAllocator m_Allocator = VK_NULL_HANDLE;

        VulkanDescriptorManager* m_DescriptorManager = nullptr;
        VulkanResourceManager* m_ResourceManager = nullptr;

        // Prefixes?
        VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_RayTracingPipelineProperties{};
        VkPhysicalDeviceAccelerationStructureFeaturesKHR rayTracingAccelerationStructureFeatures{};
        VkPhysicalDeviceBufferDeviceAddressFeatures enabledBufferDeviceAddressFeatures{};
        VkPhysicalDeviceRayTracingPipelineFeaturesKHR enabledRayTracingPipelineFeatures{};
        VkPhysicalDeviceAccelerationStructureFeaturesKHR enabledAccelerationPipelineFeatures{};

        struct AllocationRecord
        {
            String name;
            VkDeviceSize size = 0;
        };
        UnorderedMap<VmaAllocation, AllocationRecord> m_AllocationRecords;
        uint32_t m_AllocCounter = 0;

        VkPipelineCache m_PipelineCache = VK_NULL_HANDLE;
        VkPhysicalDeviceProperties m_DeviceProperties{};
        VkPhysicalDeviceFeatures2 m_DeviceFeatures{};
        VkPhysicalDeviceFeatures2 m_EnabledFeatures{};
        VkPhysicalDeviceMemoryProperties m_MemoryProperties{};
        Vector<String> m_SupportedExtensions;
    };
} // namespace Crowny
