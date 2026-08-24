#pragma once

#include "Crowny/Common/RefCounted.h"
#include "Platform/Vulkan/VulkanUtils.h"

namespace Crowny
{

    class VulkanDescriptorManager;
    class VulkanQueryPool;

    struct SurfaceFormat
    {
        VkFormat ColorFormat = VK_FORMAT_UNDEFINED;
        VkFormat DepthFormat = VK_FORMAT_UNDEFINED;
        VkColorSpaceKHR ColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    };

    class VulkanDescriptorManager;
    class VulkanResourceManager;

    enum class VulkanAllocationType
    {
        Default,
        Staging
    };

    struct VulkanOptionalFeatures
    {
        bool MultiDrawIndirect = false;
        bool DrawIndirectCount = false;
        bool ShaderDrawParameters = false;
        bool DescriptorIndexing = false;
        bool NonUniformTextureIndexing = false;
        bool UpdateAfterBind = false;
        bool BufferDeviceAddress = false;
        bool TimelineSemaphore = false;
        bool Synchronization2 = false;
        bool DynamicRendering = false;
        bool DedicatedComputeQueue = false;
        bool DedicatedTransferQueue = false;
    };

    class VulkanDevice : public RefCounted
    {
    public:
        VulkanDevice(VkPhysicalDevice device, uint32_t deviceIdx, uint32_t instanceApiVersion);
        ~VulkanDevice();

        SurfaceFormat GetSurfaceFormat(const VkSurfaceKHR& surface) const;

        VulkanCommandBufferPool& GetCmdBufferPool() const { return *m_CommandBufferPool; }
        VulkanQueryPool& GetQueryPool() const { return *m_QueryPool; }
        VkDevice GetLogicalDevice() const { return m_LogicalDevice; }
        const VkPhysicalDeviceProperties& GetDeviceProperties() const { return m_DeviceProperties; }
        VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
        const VkPhysicalDeviceFeatures& GetDeviceFeatures() const { return m_DeviceFeatures.features; }
        const VulkanOptionalFeatures& GetOptionalFeatures() const { return m_OptionalFeatures; }
        VkPipelineCache GetPipelineCache() const { return m_PipelineCache; }
        void SetPrimary();
        void SetIndex(uint32_t idx) { m_Index = idx; }
        uint32_t GetIndex() const { return m_Index; }
        uint32_t GetNumQueues(GpuQueueType type) const { return (uint32_t)m_QueueInfos[(int)type].Queues.size(); }
        VulkanQueue* GetQueue(GpuQueueType type, uint32_t idx) const { return m_QueueInfos[(int)type].Queues[idx]; }
        uint32_t GetQueueFamily(GpuQueueType type) const { return m_QueueInfos[(int)type].FamilyIdx; }
        uint32_t GetQueueMask(GpuQueueType type, uint32_t queueIdx) const;
        void Refresh(bool wait = false);

        uint32_t FindMemoryType(uint32_t requirement, VkMemoryPropertyFlags flags);
        VmaAllocation AllocateMemory(VkBuffer buffer, VkMemoryPropertyFlags flags, const char* tag = nullptr,
                                     VulkanAllocationType type = VulkanAllocationType::Default);
        VmaAllocation AllocateMemory(VkImage image, VkMemoryPropertyFlags flags, const char* tag = nullptr);
        void GetAllocationInfo(VmaAllocation allocation, VkDeviceMemory& memory, VkDeviceSize& offset);
        void* MapMemory(VmaAllocation allocation);
        void UnmapMemory(VmaAllocation allocation);
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

        VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
        VkDevice m_LogicalDevice = VK_NULL_HANDLE;
        VmaAllocator m_Allocator = VK_NULL_HANDLE;

        VulkanDescriptorManager* m_DescriptorManager = nullptr;
        VulkanResourceManager* m_ResourceManager = nullptr;
        uint32_t m_Index = 0;

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
        Mutex m_AllocationMutex;

        static constexpr VkDeviceSize STAGING_POOL_BLOCK_SIZE = 32ull * 1024ull * 1024ull;
        VmaPool m_StagingPools[VK_MAX_MEMORY_TYPES]{};

        VkPipelineCache m_PipelineCache = VK_NULL_HANDLE;
        Path m_PipelineCachePath;
        VkPhysicalDeviceProperties m_DeviceProperties{};
        VkPhysicalDeviceFeatures2 m_DeviceFeatures{};
        VkPhysicalDeviceFeatures2 m_EnabledFeatures{};
        VulkanOptionalFeatures m_OptionalFeatures;
        VkPhysicalDeviceMemoryProperties m_MemoryProperties{};
        Vector<String> m_SupportedExtensions;
    };
} // namespace Crowny
