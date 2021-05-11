#include "cwpch.h"

#include "Platform/Vulkan/VulkanDevice.h"

namespace Crowny
{
    VulkanDevice::VulkanDevice(VkPhysicalDevice physicalDevice, uint32_t idx)
        : m_PhysicalDevice(physicalDevice)
    {
        vkGetPhysicalDeviceProperties(physicalDevice, &m_DeviceProperties);
        vkGetPhysicalDeviceFeatures(physicalDevice, &m_DeviceFeatures);
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &m_MemoryProperties);
         
        uint32_t numQueueFamilies;
        vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &numQueueFamilies, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilyProperties(numQueueFamilies);
        vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &numQueueFamilies, queueFamilyProperties.data());
        
        const float defaultQueuePrios[] = { 0.0f };
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        
        auto populateQueueInfo = [&](GpuQueueType type, uint32_t familyIdx)
        {
            queueCreateInfos.push_back(VkDeviceQueueCreateInfo());
            VkDeviceQueueCreateInfo& createInfo = queueCreateInfos.back();
            createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            createInfo.pNext = nullptr;
            createInfo.flags = 0;
            createInfo.queueFamilyIndex = familyIdx;
            createInfo.queueCount = std::min(queueFamilyProperties[familyIdx].queueCount, (uint32_t)8); // TODO: do not hardcode max gpu queues
            createInfo.pQueuePriorities = defaultQueuePrios;
            m_QueueInfos[type].FamilyIdx = familyIdx;
            m_QueueInfos[type].Queues.resize(createInfo.queueCount, nullptr);
        };
        
        for (uint32_t i = 0; i < (uint32_t)queueFamilyProperties.size(); i++)
        {
            if ((queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) && 
                (queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)
            {
                CW_ENGINE_INFO("Compute queue");
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
                CW_ENGINE_INFO("Transfer queue");
                populateQueueInfo(UPLOAD_QUEUE, i);
                break;
            }
        }
        
        for (uint32_t i = 0; i < (uint32_t)queueFamilyProperties.size(); i++)
        {
            if (queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                CW_ENGINE_INFO("Graphics queue");
                populateQueueInfo(GRAPHICS_QUEUE, i);
                break;
            }
        }
        
        for (uint32_t i = 0; i < (uint32_t)queueFamilyProperties.size(); i++)
        {/*
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
        
        //VkResult result = vkCreateDevice(m_PhysicalDevice, &deviceInfo, gVulkanAllocator, &m_LogicalDevice);
        CW_ENGINE_INFO("Before create");
        if (m_DeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            VkResult result = vkCreateDevice(m_PhysicalDevice, &deviceInfo, nullptr, &m_LogicalDevice);
            CW_ENGINE_ASSERT(result == VK_SUCCESS);
            CW_ENGINE_INFO(result);
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
            m_CommandBufferPool = new VulkanCommandBufferPool(*this);
        }
        /*
        VmaAllocatorCreateInfo allocatorCI = {};
        allocatorCI.physicalDevice = m_PhysicalDevice;
        allocatorCI.device = m_LogicalDevice;
        allocatorCI.pAllocationCallbacks = gVulkanAllocator;
        
        if (dedicatedAllocExt && getMemReqExt)
            allocatorCI.flags |= VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
        
        vmaCreateAllocator(&allocatorCI, &m_Allocator);*/
        CW_ENGINE_INFO("Device init");
    }
    
    VulkanDevice::~VulkanDevice()
    {
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
        vkDestroyDevice(m_LogicalDevice, nullptr);
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

        std::vector<VkFormat> depthFormats = { VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D16_UNORM };
        VkBool32 validDepthFormat = false;
        for (auto& format : depthFormats) {
            VkFormatProperties formatProps;
            vkGetPhysicalDeviceFormatProperties(m_PhysicalDevice, format, &formatProps);
            if (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
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
        VkResult result = vkDeviceWaitIdle(m_LogicalDevice);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
        Refresh(true);
    }

    /*
    uint32_t VulkanDevice::GetMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties, VkBool32* found = nullptr)
    {
        for (uint32_t i = 0; m_MemoryProperties.memoryTypeCount; i++)
        {
            if ((typeBits & 1) == 1)
            {
                if ((memoryProperies.memoryTypes[i].propertyFlags & propertoes) == properties)
                {
                    if (memTypeFound)
                        *memTypeFound = true;
                    return i;
                }
            }
            typeBits >>= 1;
        }

        if (memTypeFound)
        {
            *memTypeFound = false;
            return 0;
        }
        else
            CW_ENGINE_ASSERT(false);

    }*/

}