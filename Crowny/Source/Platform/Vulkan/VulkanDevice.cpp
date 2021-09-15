#include "cwpch.h"

#include "Platform/Vulkan/VulkanDescriptorPool.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanRenderAPI.h"

namespace Crowny
{
    VulkanDevice::VulkanDevice(VkPhysicalDevice physicalDevice, uint32_t idx) : m_PhysicalDevice(physicalDevice)
    {
        vkGetPhysicalDeviceProperties(physicalDevice, &m_DeviceProperties);
        vkGetPhysicalDeviceFeatures(physicalDevice, &m_DeviceFeatures);
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &m_MemoryProperties);

        uint32_t numQueueFamilies;
        vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &numQueueFamilies, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilyProperties(numQueueFamilies);
        vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &numQueueFamilies, queueFamilyProperties.data());

        const float defaultQueuePrios[MAX_QUEUES_PER_TYPE] = { 0.0f };
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

        auto populateQueueInfo = [&](GpuQueueType type, uint32_t familyIdx) {
            queueCreateInfos.push_back(VkDeviceQueueCreateInfo());
            VkDeviceQueueCreateInfo& createInfo = queueCreateInfos.back();
            createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            createInfo.pNext = nullptr;
            createInfo.flags = 0;
            createInfo.queueFamilyIndex = familyIdx;
            createInfo.queueCount =
              std::min(queueFamilyProperties[familyIdx].queueCount, (uint32_t)MAX_QUEUES_PER_TYPE);
            createInfo.pQueuePriorities = defaultQueuePrios;
            m_QueueInfos[type].FamilyIdx = familyIdx;
            m_QueueInfos[type].Queues.resize(createInfo.queueCount, nullptr);
        };

        for (uint32_t i = 0; i < (uint32_t)queueFamilyProperties.size(); i++)
        {
            if ((queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) &&
                (queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)
            {
                populateQueueInfo(COMPUTE_QUEUE, i);
                break;
            }
        }

        for (uint32_t i = 0; i < (uint32_t)queueFamilyProperties.size(); i++)
        {
            if ((queueFamilyProperties[i].queueFlags & VK_QUEUE_TRANSFER_BIT) &&
                ((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) &&
                ((queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0))
            {
                populateQueueInfo(UPLOAD_QUEUE, i);
                break;
            }
        }

        for (uint32_t i = 0; i < (uint32_t)queueFamilyProperties.size(); i++)
        {
            if (queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                populateQueueInfo(GRAPHICS_QUEUE, i);
                break;
            }
        }

        for (uint32_t i = 0; i < (uint32_t)queueFamilyProperties.size(); i++)
        { /*
             if (queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
             {
                 populateQueueInfo(COMPUTE_QUEUE, i);
             }*/
        }

        const char* extensions[1];
        uint32_t numExts = 0;
        extensions[0] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

        VkDeviceCreateInfo deviceInfo;
        deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceInfo.pNext = nullptr;
        deviceInfo.flags = 0;
        deviceInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();
        deviceInfo.pQueueCreateInfos = queueCreateInfos.data();
        deviceInfo.pEnabledFeatures = &m_DeviceFeatures;
        deviceInfo.enabledExtensionCount = 1;
        deviceInfo.ppEnabledExtensionNames = extensions;
        deviceInfo.enabledLayerCount = 0;
        deviceInfo.ppEnabledLayerNames = nullptr;

        if (m_DeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            VkResult result = vkCreateDevice(m_PhysicalDevice, &deviceInfo, gVulkanAllocator, &m_LogicalDevice);
            CW_ENGINE_ASSERT(result == VK_SUCCESS);
            for (uint32_t i = 0; i < QUEUE_COUNT; i++)
            {
                uint32_t numQueues = (uint32_t)m_QueueInfos[i].Queues.size();
                for (uint32_t j = 0; j < numQueues; j++)
                {
                    VkQueue queue;
                    vkGetDeviceQueue(m_LogicalDevice, m_QueueInfos[i].FamilyIdx, j, &queue);
                    m_QueueInfos[i].Queues[j] = new VulkanQueue(*this, queue, (GpuQueueType)i, j);
                }
            }

            VmaAllocatorCreateInfo allocatorCI = {};
            allocatorCI.physicalDevice = m_PhysicalDevice;
            allocatorCI.device = m_LogicalDevice;
            allocatorCI.pAllocationCallbacks = gVulkanAllocator;
            allocatorCI.flags |= VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
            allocatorCI.instance = gVulkanRenderAPI().GetInstance();
            allocatorCI.vulkanApiVersion = VK_API_VERSION_1_1;

            vmaCreateAllocator(&allocatorCI, &m_Allocator);

            m_CommandBufferPool = new VulkanCommandBufferPool(*this);
            m_DescriptorManager = new VulkanDescriptorManager(*this);
            m_ResourceManager = new VulkanResourceManager(*this);
        }
    }

    VulkanDevice::~VulkanDevice()
    {
        if (m_LogicalDevice == VK_NULL_HANDLE)
            return;
        VkResult result = vkDeviceWaitIdle(m_LogicalDevice);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);

        for (uint32_t i = 0; i < QUEUE_COUNT; i++)
        {
            uint32_t numq = (uint32_t)m_QueueInfos[i].Queues.size();
            for (uint32_t j = 0; j < numq; j++)
            {
                m_QueueInfos[i].Queues[j]->Refresh(true, true);
                delete m_QueueInfos[i].Queues[j];
            }
        }

        delete m_CommandBufferPool;
        delete m_DescriptorManager;
        delete m_ResourceManager;
        vmaDestroyAllocator(m_Allocator);
        vkDestroyDevice(m_LogicalDevice, gVulkanAllocator);
    }

    uint32_t VulkanDevice::GetQueueMask(GpuQueueType type, uint32_t queueIdx) const
    {
        uint32_t numQueues = GetNumQueues(type);
        if (numQueues == 0)
            return 0;

        uint32_t idMask = 0;
        uint32_t curIdx = queueIdx % numQueues;
        while (curIdx < MAX_QUEUES_PER_TYPE)
        {
            idMask |= CommandSyncMask::GetGlobalQueueMask(type, curIdx);
            curIdx += numQueues;
        }
        return idMask;
    }

    void VulkanDevice::Refresh(bool wait)
    {
        for (uint32_t i = 0; i < QUEUE_COUNT; i++)
        {
            uint32_t numQueues = GetNumQueues((GpuQueueType)i);
            for (uint32_t j = 0; j < numQueues; j++)
            {
                VulkanQueue* queue = GetQueue((GpuQueueType)i, j);
                queue->Refresh(wait, false);
            }
        }
    }

    SurfaceFormat VulkanDevice::GetSurfaceFormat(const VkSurfaceKHR& surface) const
    {
        SurfaceFormat resultFormat;
        uint32_t formatCount;
        VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, surface, &formatCount, nullptr);
        CW_ENGINE_ASSERT(result == VK_SUCCESS && formatCount > 0);

        std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
        result = vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, surface, &formatCount, surfaceFormats.data());
        CW_ENGINE_ASSERT(result == VK_SUCCESS);

        if (formatCount == 1 && surfaceFormats[0].format == VK_FORMAT_UNDEFINED)
        {
            resultFormat.ColorFormat = VK_FORMAT_B8G8R8A8_UNORM;
            resultFormat.ColorSpace = surfaceFormats[0].colorSpace;
        }
        else
        {
            bool foundFormat = false;
            for (auto&& surfaceFormat : surfaceFormats)
            {
                if (surfaceFormat.format == VK_FORMAT_R8G8B8A8_UNORM)
                {
                    resultFormat.ColorFormat = surfaceFormat.format;
                    resultFormat.ColorSpace = surfaceFormat.colorSpace;
                    foundFormat = true;
                    break;
                }
            }

            if (!foundFormat)
            {
                resultFormat.ColorFormat = surfaceFormats[0].format;
                resultFormat.ColorSpace = surfaceFormats[0].colorSpace;
            }
        }

        std::vector<VkFormat> depthFormats = { VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT,
                                               VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT,
                                               VK_FORMAT_D16_UNORM };
        VkBool32 validDepthFormat = false;
        for (auto& format : depthFormats)
        {
            VkFormatProperties formatProps;
            vkGetPhysicalDeviceFormatProperties(m_PhysicalDevice, format, &formatProps);
            if (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
            {
                resultFormat.DepthFormat = format;
                validDepthFormat = true;
                break;
            }
        }
        CW_ENGINE_ASSERT(resultFormat.DepthFormat);
        return resultFormat;
    }

    void VulkanDevice::WaitIdle()
    {
        if (m_LogicalDevice == VK_NULL_HANDLE)
            return;
        VkResult result = vkDeviceWaitIdle(m_LogicalDevice);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
        Refresh(true);
    }

    VmaAllocation VulkanDevice::AllocateMemory(VkImage image, VkMemoryPropertyFlags flags)
    {
        VmaAllocationCreateInfo allocCreateInfo{};
        allocCreateInfo.requiredFlags = flags;

        VmaAllocationInfo allocInfo;
        VmaAllocation memory;
        VkResult result = vmaAllocateMemoryForImage(m_Allocator, image, &allocCreateInfo, &memory, &allocInfo);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);

        result = vkBindImageMemory(m_LogicalDevice, image, allocInfo.deviceMemory, allocInfo.offset);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
        return memory;
    }

    VmaAllocation VulkanDevice::AllocateMemory(VkBuffer buffer, VkMemoryPropertyFlags flags)
    {
        VmaAllocationCreateInfo allocCreateInfo{};
        allocCreateInfo.requiredFlags = flags;

        VmaAllocationInfo allocInfo;
        VmaAllocation memory;
        VkResult result = vmaAllocateMemoryForBuffer(m_Allocator, buffer, &allocCreateInfo, &memory, &allocInfo);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);

        result = vkBindBufferMemory(m_LogicalDevice, buffer, allocInfo.deviceMemory, allocInfo.offset);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
        return memory;
    }

    void VulkanDevice::GetAllocationInfo(VmaAllocation allocation, VkDeviceMemory& memory, VkDeviceSize& offset)
    {
        VmaAllocationInfo allocInfo;
        vmaGetAllocationInfo(m_Allocator, allocation, &allocInfo);
        memory = allocInfo.deviceMemory;
        offset = allocInfo.offset;
    }

    void VulkanDevice::FreeMemory(VmaAllocation allocation) { vmaFreeMemory(m_Allocator, allocation); }

    uint32_t VulkanDevice::FindMemoryType(uint32_t requirement, VkMemoryPropertyFlags flags)
    {
        for (uint32_t i = 0; i < m_MemoryProperties.memoryTypeCount; i++)
        {
            if (requirement & (1 << i))
                if ((m_MemoryProperties.memoryTypes[i].propertyFlags & flags) == flags)
                    return i;
        }
        return -1;
    }

} // namespace Crowny