#include "cwpch.h"

#include "Platform/Vulkan/VulkanCommandBuffer.h"
#include "Platform/Vulkan/VulkanGpuBufferManager.h"
#include "Platform/Vulkan/VulkanQueue.h"
#include "Platform/Vulkan/VulkanResource.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/Version.h"
#include "Platform/Vulkan/VulkanDescriptorPool.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanQuery.h"
#include "Platform/Vulkan/VulkanRenderAPI.h"

#define LOG_EXTENSIONS false
#define raytracing false

namespace Crowny
{
    static Path GetPipelinePath(const VkPhysicalDeviceProperties& properties)
    {
        constexpr char HEX[] = "0123456789abcdef";
        String cacheUuid;
        cacheUuid.reserve(VK_UUID_SIZE * 2);
        for (uint8_t byte : properties.pipelineCacheUUID)
        {
            cacheUuid.push_back(HEX[byte >> 4]);
            cacheUuid.push_back(HEX[byte & 0x0f]);
        }

        const String filename = "vk-" + std::to_string(properties.vendorID) + "-" + std::to_string(properties.deviceID) + "-" +
                                std::to_string(properties.driverVersion) + "-" + cacheUuid + ".blob";
        return Application::TryGet()->GetInternalDirectory() / "pcache" / CROWNY_VERSION_STRING / filename;
    }

    static bool IsPipelineCacheCompatible(const Vector<uint8_t>& data, const VkPhysicalDeviceProperties& properties)
    {
        if (data.size() < sizeof(VkPipelineCacheHeaderVersionOne))
            return false;
        VkPipelineCacheHeaderVersionOne header{};
        std::memcpy(&header, data.data(), sizeof(header));
        return header.headerSize >= sizeof(header) && header.headerVersion == VK_PIPELINE_CACHE_HEADER_VERSION_ONE &&
               header.vendorID == properties.vendorID && header.deviceID == properties.deviceID &&
               std::memcmp(header.pipelineCacheUUID, properties.pipelineCacheUUID, VK_UUID_SIZE) == 0;
    }

    VulkanDevice::VulkanDevice(VkPhysicalDevice physicalDevice, uint32_t deviceIdx, uint32_t instanceApiVersion)
      : m_PhysicalDevice(physicalDevice), m_Index(deviceIdx == UINT32_MAX ? 0 : deviceIdx)
    {
        // Always populate device properties (needed for the discrete GPU check below)
        vkGetPhysicalDeviceProperties(physicalDevice, &m_DeviceProperties);

        m_DeviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        m_DeviceFeatures.pNext = nullptr;
        if (raytracing)
        {
            m_RayTracingPipelineProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

            VkPhysicalDeviceRayTracingInvocationReorderPropertiesNV reorderProperties{};
            reorderProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_INVOCATION_REORDER_PROPERTIES_NV;

            VkPhysicalDeviceAccelerationStructurePropertiesKHR accelerationProperties{};
            accelerationProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;

            VkPhysicalDeviceProperties2 deviceProperties2{};
            deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;

            deviceProperties2.pNext = &m_RayTracingPipelineProperties;
            m_RayTracingPipelineProperties.pNext = &reorderProperties;
            reorderProperties.pNext = &accelerationProperties;
            accelerationProperties.pNext = nullptr;

            vkGetPhysicalDeviceProperties2(physicalDevice, &deviceProperties2);

            CW_ENGINE_INFO("Max recursion depth: {}", m_RayTracingPipelineProperties.maxRayRecursionDepth);
            CW_ENGINE_INFO("Handles sizes: {}", m_RayTracingPipelineProperties.shaderGroupHandleSize);
            CW_ENGINE_INFO("Handles align: {}", m_RayTracingPipelineProperties.shaderGroupHandleAlignment);
            CW_ENGINE_INFO("Max recursion depth: {}", m_RayTracingPipelineProperties.maxRayRecursionDepth);
            CW_ENGINE_INFO("Shader execution reordering: {}", reorderProperties.rayTracingInvocationReorderReorderingHint);
            CW_ENGINE_INFO("Max geometries: {}", accelerationProperties.maxGeometryCount);
            CW_ENGINE_INFO("Max instances: {}", accelerationProperties.maxInstanceCount);
            CW_ENGINE_INFO("Max primitives: {}", accelerationProperties.maxPrimitiveCount);

            // Update m_DeviceProperties from the Properties2 query (same data, just also fills the pNext chain)
            m_DeviceProperties = deviceProperties2.properties;

            enabledBufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
            enabledBufferDeviceAddressFeatures.pNext = nullptr;

            enabledRayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
            enabledRayTracingPipelineFeatures.pNext = &enabledBufferDeviceAddressFeatures;

            rayTracingAccelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
            rayTracingAccelerationStructureFeatures.pNext = &enabledRayTracingPipelineFeatures;
            m_DeviceFeatures.pNext = &rayTracingAccelerationStructureFeatures;
        }
        vkGetPhysicalDeviceFeatures2(physicalDevice, &m_DeviceFeatures);

        bool rayTracingFeaturesSupported = true;
        if (raytracing)
        {
            rayTracingFeaturesSupported = enabledBufferDeviceAddressFeatures.bufferDeviceAddress &&
                                          enabledRayTracingPipelineFeatures.rayTracingPipeline &&
                                          rayTracingAccelerationStructureFeatures.accelerationStructure;
            if (!rayTracingFeaturesSupported)
                CW_ENGINE_WARN("Crowny ray tracing was requested but the selected GPU lacks required features");

            m_DeviceFeatures.pNext = nullptr;
            m_RayTracingPipelineProperties.pNext = nullptr;
        }

        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &m_MemoryProperties);

        uint32_t numQueueFamilies;
        vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &numQueueFamilies, nullptr);
        Vector<VkQueueFamilyProperties> queueFamilyProperties(numQueueFamilies);
        vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &numQueueFamilies, queueFamilyProperties.data());

        float defaultQueuePrios[MAX_QUEUES_PER_TYPE];
        std::fill(std::begin(defaultQueuePrios), std::end(defaultQueuePrios), 1.0f);
        Vector<VkDeviceQueueCreateInfo> queueCreateInfos;

        auto populateQueueInfo = [&](GpuQueueType type, uint32_t familyIdx) {
            queueCreateInfos.push_back(VkDeviceQueueCreateInfo());
            VkDeviceQueueCreateInfo& createInfo = queueCreateInfos.back();
            createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            createInfo.pNext = nullptr;
            createInfo.flags = 0;
            createInfo.queueFamilyIndex = familyIdx;
            createInfo.queueCount = std::min(queueFamilyProperties[familyIdx].queueCount, (uint32_t)MAX_QUEUES_PER_TYPE);
            createInfo.pQueuePriorities = defaultQueuePrios;
            m_QueueInfos[type].FamilyIdx = familyIdx;
            m_QueueInfos[type].Queues.resize(createInfo.queueCount, nullptr);
        };

        for (uint32_t i = 0; i < (uint32_t)queueFamilyProperties.size(); i++)
        {
            if ((queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) && (queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)
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
            TODO:
             if (queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
             {
                 populateQueueInfo(COMPUTE_QUEUE, i);
             }*/
        }

        uint32_t availableExtensionsCount = 0;
        VkResult result = vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &availableExtensionsCount, nullptr);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
        Vector<VkExtensionProperties> availableExtensions(availableExtensionsCount);
        result = vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &availableExtensionsCount, availableExtensions.data());
        CW_ENGINE_ASSERT(result == VK_SUCCESS);

#if LOG_EXTENSIONS
        for (const VkExtensionProperties& ext : availableExtensions)
            CW_ENGINE_INFO("Extension: {}, version: {}", ext.extensionName, ext.specVersion);
#endif

        void* pNext = nullptr;
        Vector<const char*> extensions;
        bool supportsSwapChain = false;
        bool supportsPortabilitySubset = false;
        bool supportsDemoteExtension = false;
        for (const VkExtensionProperties& extension : availableExtensions)
        {
            m_SupportedExtensions.emplace_back(extension.extensionName);
            if (std::strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0)
                supportsSwapChain = true;
            if (std::strcmp(extension.extensionName, VK_EXT_SHADER_DEMOTE_TO_HELPER_INVOCATION_EXTENSION_NAME) == 0)
                supportsDemoteExtension = true;
#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
            if (std::strcmp(extension.extensionName, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME) == 0)
                supportsPortabilitySubset = true;
#endif
        }

        CW_ENGINE_ASSERT(supportsSwapChain, "Selected Vulkan device does not support swap chains");
        extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
        if (supportsPortabilitySubset)
            extensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
#endif

        const bool useVulkan11Features = instanceApiVersion >= VK_API_VERSION_1_1 && m_DeviceProperties.apiVersion >= VK_API_VERSION_1_1;
        const bool useVulkan12Features = instanceApiVersion >= VK_API_VERSION_1_2 && m_DeviceProperties.apiVersion >= VK_API_VERSION_1_2;
        const bool useVulkan13Features = instanceApiVersion >= VK_API_VERSION_1_3 && m_DeviceProperties.apiVersion >= VK_API_VERSION_1_3;
        VkPhysicalDeviceVulkan11Features supportedVulkan11Features{};
        VkPhysicalDeviceVulkan12Features supportedVulkan12Features{};
        VkPhysicalDeviceVulkan13Features supportedVulkan13Features{};
        VkPhysicalDeviceShaderDemoteToHelperInvocationFeaturesEXT supportedDemoteFeatures{};
        VkPhysicalDeviceFeatures2 extendedFeatureQuery{};
        extendedFeatureQuery.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        void* featureQueryChain = nullptr;
        if (supportsDemoteExtension && !useVulkan13Features)
        {
            supportedDemoteFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DEMOTE_TO_HELPER_INVOCATION_FEATURES_EXT;
            supportedDemoteFeatures.pNext = featureQueryChain;
            featureQueryChain = &supportedDemoteFeatures;
        }
        if (useVulkan13Features)
        {
            supportedVulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
            supportedVulkan13Features.pNext = featureQueryChain;
            featureQueryChain = &supportedVulkan13Features;
        }
        if (useVulkan12Features)
        {
            supportedVulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
            supportedVulkan12Features.pNext = featureQueryChain;
            featureQueryChain = &supportedVulkan12Features;
        }
        if (useVulkan11Features)
        {
            supportedVulkan11Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
            supportedVulkan11Features.pNext = featureQueryChain;
            featureQueryChain = &supportedVulkan11Features;
        }
        extendedFeatureQuery.pNext = featureQueryChain;

        if (extendedFeatureQuery.pNext != nullptr)
            vkGetPhysicalDeviceFeatures2(physicalDevice, &extendedFeatureQuery);

        const bool demoteToHelperInvocationSupported =
          useVulkan13Features ? supportedVulkan13Features.shaderDemoteToHelperInvocation == VK_TRUE
                              : supportsDemoteExtension && supportedDemoteFeatures.shaderDemoteToHelperInvocation == VK_TRUE;

        VkPhysicalDeviceVulkan11Features enabledVulkan11Features{};
        VkPhysicalDeviceVulkan12Features enabledVulkan12Features{};
        VkPhysicalDeviceVulkan13Features enabledVulkan13Features{};
        VkPhysicalDeviceShaderDemoteToHelperInvocationFeaturesEXT enabledDemoteFeatures{};
        if (useVulkan13Features)
        {
            enabledVulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
            enabledVulkan13Features.shaderDemoteToHelperInvocation = supportedVulkan13Features.shaderDemoteToHelperInvocation;
            enabledVulkan13Features.synchronization2 = supportedVulkan13Features.synchronization2;
            enabledVulkan13Features.dynamicRendering = supportedVulkan13Features.dynamicRendering;
            enabledVulkan13Features.pNext = pNext;
            pNext = &enabledVulkan13Features;
        }
        else if (demoteToHelperInvocationSupported)
        {
            enabledDemoteFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DEMOTE_TO_HELPER_INVOCATION_FEATURES_EXT;
            enabledDemoteFeatures.shaderDemoteToHelperInvocation = VK_TRUE;
            enabledDemoteFeatures.pNext = pNext;
            pNext = &enabledDemoteFeatures;
            extensions.push_back(VK_EXT_SHADER_DEMOTE_TO_HELPER_INVOCATION_EXTENSION_NAME);
        }

        if (useVulkan12Features)
        {
            enabledVulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
            enabledVulkan12Features.drawIndirectCount = supportedVulkan12Features.drawIndirectCount;
            enabledVulkan12Features.descriptorIndexing = supportedVulkan12Features.descriptorIndexing;
            enabledVulkan12Features.shaderSampledImageArrayNonUniformIndexing =
              supportedVulkan12Features.shaderSampledImageArrayNonUniformIndexing;
            enabledVulkan12Features.shaderStorageBufferArrayNonUniformIndexing =
              supportedVulkan12Features.shaderStorageBufferArrayNonUniformIndexing;
            enabledVulkan12Features.shaderStorageImageArrayNonUniformIndexing =
              supportedVulkan12Features.shaderStorageImageArrayNonUniformIndexing;
            enabledVulkan12Features.descriptorBindingSampledImageUpdateAfterBind =
              supportedVulkan12Features.descriptorBindingSampledImageUpdateAfterBind;
            enabledVulkan12Features.descriptorBindingStorageImageUpdateAfterBind =
              supportedVulkan12Features.descriptorBindingStorageImageUpdateAfterBind;
            enabledVulkan12Features.descriptorBindingStorageBufferUpdateAfterBind =
              supportedVulkan12Features.descriptorBindingStorageBufferUpdateAfterBind;
            enabledVulkan12Features.descriptorBindingPartiallyBound = supportedVulkan12Features.descriptorBindingPartiallyBound;
            enabledVulkan12Features.descriptorBindingVariableDescriptorCount =
              supportedVulkan12Features.descriptorBindingVariableDescriptorCount;
            enabledVulkan12Features.runtimeDescriptorArray = supportedVulkan12Features.runtimeDescriptorArray;
            enabledVulkan12Features.timelineSemaphore = supportedVulkan12Features.timelineSemaphore;
            enabledVulkan12Features.bufferDeviceAddress = supportedVulkan12Features.bufferDeviceAddress;
            enabledVulkan12Features.pNext = pNext;
            pNext = &enabledVulkan12Features;
        }

        if (useVulkan11Features)
        {
            enabledVulkan11Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
            enabledVulkan11Features.shaderDrawParameters = supportedVulkan11Features.shaderDrawParameters;
            enabledVulkan11Features.pNext = pNext;
            pNext = &enabledVulkan11Features;
        }

        if (!demoteToHelperInvocationSupported)
        {
            CW_ENGINE_WARN("Selected Vulkan device does not support shader demote-to-helper invocation");
        }

        m_OptionalFeatures.MultiDrawIndirect = m_DeviceFeatures.features.multiDrawIndirect == VK_TRUE;
        m_OptionalFeatures.DrawIndirectCount = enabledVulkan12Features.drawIndirectCount == VK_TRUE;
        m_OptionalFeatures.ShaderDrawParameters = enabledVulkan11Features.shaderDrawParameters == VK_TRUE;
        m_OptionalFeatures.NonUniformTextureIndexing = enabledVulkan12Features.shaderSampledImageArrayNonUniformIndexing == VK_TRUE;
        m_OptionalFeatures.UpdateAfterBind = enabledVulkan12Features.descriptorBindingSampledImageUpdateAfterBind == VK_TRUE;
        m_OptionalFeatures.DescriptorIndexing = enabledVulkan12Features.descriptorIndexing == VK_TRUE &&
                                                enabledVulkan12Features.runtimeDescriptorArray == VK_TRUE &&
                                                enabledVulkan12Features.descriptorBindingPartiallyBound == VK_TRUE &&
                                                enabledVulkan12Features.descriptorBindingVariableDescriptorCount == VK_TRUE;
        m_OptionalFeatures.BufferDeviceAddress = enabledVulkan12Features.bufferDeviceAddress == VK_TRUE;
        m_OptionalFeatures.TimelineSemaphore = enabledVulkan12Features.timelineSemaphore == VK_TRUE;
        m_OptionalFeatures.Synchronization2 = enabledVulkan13Features.synchronization2 == VK_TRUE;
        m_OptionalFeatures.DynamicRendering = enabledVulkan13Features.dynamicRendering == VK_TRUE;
        m_OptionalFeatures.DedicatedComputeQueue = GetNumQueues(COMPUTE_QUEUE) > 0;
        m_OptionalFeatures.DedicatedTransferQueue = GetNumQueues(UPLOAD_QUEUE) > 0;

        bool rayTracingEnabled = false;
        if (raytracing)
        {
            enabledBufferDeviceAddressFeatures = {};
            enabledBufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
            enabledBufferDeviceAddressFeatures.bufferDeviceAddress = VK_TRUE;
            enabledBufferDeviceAddressFeatures.pNext = pNext;

            enabledRayTracingPipelineFeatures = {};
            enabledRayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
            enabledRayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;
            enabledRayTracingPipelineFeatures.pNext = &enabledBufferDeviceAddressFeatures;

            enabledAccelerationPipelineFeatures = {};
            enabledAccelerationPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
            enabledAccelerationPipelineFeatures.accelerationStructure = VK_TRUE;
            enabledAccelerationPipelineFeatures.pNext = &enabledRayTracingPipelineFeatures;

            Vector<const char*> rayTracingExts;
            rayTracingExts.push_back(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);
            rayTracingExts.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
            rayTracingExts.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);

            rayTracingExts.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
            rayTracingExts.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
            rayTracingExts.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
            rayTracingExts.push_back(VK_KHR_SPIRV_1_4_EXTENSION_NAME);

            bool rtSupported = true;
            for (const char* rtExt : rayTracingExts)
            {
                const bool isExtSupported =
                  std::any_of(availableExtensions.begin(), availableExtensions.end(), [rtExt](const VkExtensionProperties& ext) {
                      return std::strncmp(rtExt, ext.extensionName, VK_MAX_EXTENSION_NAME_SIZE) == 0;
                  });
                if (!isExtSupported)
                {
                    CW_ENGINE_ERROR("Vulkan extension {} is not supported on this GPU", rtExt);
                    rtSupported = false;
                }
            }
            if (rtSupported && rayTracingFeaturesSupported)
            {
                extensions.insert(extensions.end(), rayTracingExts.begin(), rayTracingExts.end());
                pNext = &enabledAccelerationPipelineFeatures;
                rayTracingEnabled = true;
            }
            else
                CW_ENGINE_WARN("Crowny ray tracing is disabled for the selected GPU");
        }

        VkDeviceCreateInfo deviceInfo;
        deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceInfo.flags = 0;
        deviceInfo.pNext = pNext;
        deviceInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();
        deviceInfo.pQueueCreateInfos = queueCreateInfos.data();
        deviceInfo.enabledExtensionCount = (uint32_t)extensions.size();
        deviceInfo.ppEnabledExtensionNames = extensions.data();
        deviceInfo.enabledLayerCount = 0;
        deviceInfo.ppEnabledLayerNames = nullptr;
        deviceInfo.pEnabledFeatures = &m_DeviceFeatures.features; // TODO: More fine control

        result = vkCreateDevice(m_PhysicalDevice, &deviceInfo, gVulkanAllocator, &m_LogicalDevice);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
        for (uint32_t i = 0; i < QUEUE_COUNT; i++)
        {
            const uint32_t numQueues = (uint32_t)m_QueueInfos[i].Queues.size();
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
        if (rayTracingEnabled || m_OptionalFeatures.BufferDeviceAddress)
            allocatorCI.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
        allocatorCI.instance = gVulkanRenderAPI().GetInstance();
        allocatorCI.vulkanApiVersion = std::min(m_DeviceProperties.apiVersion, VK_API_VERSION_1_2);

        result = vmaCreateAllocator(&allocatorCI, &m_Allocator);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);

        m_CommandBufferPool = new VulkanCommandBufferPool(*this);
        m_QueryPool = new VulkanQueryPool(*this);
        m_DescriptorManager = new VulkanDescriptorManager(*this);
        m_ResourceManager = new VulkanResourceManager(*this);

        VkPipelineCacheCreateInfo pipelineCacheCI;
        pipelineCacheCI.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
        pipelineCacheCI.pNext = nullptr;
        pipelineCacheCI.flags = 0;
        pipelineCacheCI.initialDataSize = 0;
        pipelineCacheCI.pInitialData = nullptr;

        // Project loading changes Application::InternalDirectory. Capture one
        // absolute path so load and save always address the same cache file.
        m_PipelineCachePath = fs::absolute(GetPipelinePath(m_DeviceProperties)).lexically_normal();
        Vector<uint8_t> pipelineCacheData;
        if (fs::exists(m_PipelineCachePath))
        {
            Ref<DataStream> dataStream = FileSystem::OpenFile(m_PipelineCachePath);
            if (dataStream)
                pipelineCacheData = dataStream->ReadAll();
            if (IsPipelineCacheCompatible(pipelineCacheData, m_DeviceProperties))
            {
                pipelineCacheCI.initialDataSize = pipelineCacheData.size();
                pipelineCacheCI.pInitialData = pipelineCacheData.data();
            }
            else
            {
                CW_ENGINE_WARN("Ignoring incompatible Vulkan pipeline cache at {}", m_PipelineCachePath.string());
                pipelineCacheData.clear();
            }
        }
        result = vkCreatePipelineCache(m_LogicalDevice, &pipelineCacheCI, gVulkanAllocator, &m_PipelineCache);
        if (result != VK_SUCCESS && pipelineCacheCI.initialDataSize != 0)
        {
            CW_ENGINE_WARN("Vulkan rejected the pipeline cache ({}); retrying empty.", static_cast<int32_t>(result));
            pipelineCacheCI.initialDataSize = 0;
            pipelineCacheCI.pInitialData = nullptr;
            result = vkCreatePipelineCache(m_LogicalDevice, &pipelineCacheCI, gVulkanAllocator, &m_PipelineCache);
        }
        CW_ENGINE_ASSERT(result == VK_SUCCESS, "Unable to create Vulkan pipeline cache");
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

        delete m_DescriptorManager;
        delete m_QueryPool;
        delete m_CommandBufferPool;
        delete m_ResourceManager;

        for (uint32_t i = 0; i < m_MemoryProperties.memoryTypeCount; i++)
        {
            if (m_StagingPools[i] != VK_NULL_HANDLE)
            {
                vmaDestroyPool(m_Allocator, m_StagingPools[i]);
                m_StagingPools[i] = VK_NULL_HANDLE;
            }
        }

        // Store the pipeline data in a file.
        size_t dataSize = 0;
        if (m_PipelineCache != VK_NULL_HANDLE)
        {

            result = vkGetPipelineCacheData(m_LogicalDevice, m_PipelineCache, &dataSize, nullptr);
            CW_ENGINE_ASSERT(result == VK_SUCCESS);
            if (dataSize > 0)
            {
                Vector<uint8_t> data;
                data.resize(dataSize);
                result = vkGetPipelineCacheData(m_LogicalDevice, m_PipelineCache, &dataSize, data.data());
                CW_ENGINE_ASSERT(result == VK_SUCCESS);
                if (!fs::exists(m_PipelineCachePath.parent_path()))
                    fs::create_directories(m_PipelineCachePath.parent_path());
                FileSystem::WriteFile(m_PipelineCachePath, data.data(), dataSize);
            }
            vkDestroyPipelineCache(m_LogicalDevice, m_PipelineCache, gVulkanAllocator);
        }

        if (!m_AllocationRecords.empty())
        {
            VkDeviceSize totalLeaked = 0;
            for (const auto& [alloc, rec] : m_AllocationRecords)
                totalLeaked += rec.size;

            CW_ENGINE_ERROR("VMA LEAK: {} allocation(s) not freed, total ~{} bytes ({:.2f} MB)", m_AllocationRecords.size(), totalLeaked,
                            totalLeaked / (1024.0 * 1024.0));
            for (const auto& [alloc, rec] : m_AllocationRecords)
                CW_ENGINE_ERROR("  LEAKED: {} — {} bytes", rec.name, rec.size);
        }

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

        Vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
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

        Vector<VkFormat> depthFormats = { VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT,
                                          VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D16_UNORM };
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
        CW_ENGINE_ASSERT(validDepthFormat, "Selected Vulkan device has no supported depth attachment format");
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

    VmaAllocation VulkanDevice::AllocateMemory(VkImage image, VkMemoryPropertyFlags flags, const char* tag)
    {
        Lock lock(m_AllocationMutex);
        VmaAllocationCreateInfo allocCreateInfo{};
        allocCreateInfo.requiredFlags = flags;

        VmaAllocationInfo allocInfo;
        VmaAllocation memory;
        VkResult result = vmaAllocateMemoryForImage(m_Allocator, image, &allocCreateInfo, &memory, &allocInfo);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);

        result = vkBindImageMemory(m_LogicalDevice, image, allocInfo.deviceMemory, allocInfo.offset);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);

        uint32_t id = m_AllocCounter++;
        String name = tag ? fmt::format("Image #{} [{}]", id, tag) : fmt::format("Image #{}", id);
        m_AllocationRecords[memory] = { name, allocInfo.size };
        return memory;
    }

    VmaAllocation VulkanDevice::AllocateMemory(VkBuffer buffer, VkMemoryPropertyFlags flags, const char* tag, VulkanAllocationType type)
    {
        Lock lock(m_AllocationMutex);
        VmaAllocationCreateInfo allocCreateInfo{};
        allocCreateInfo.requiredFlags = flags;

        if (type == VulkanAllocationType::Staging)
        {
            VkMemoryRequirements requirements{};
            vkGetBufferMemoryRequirements(m_LogicalDevice, buffer, &requirements);

            // Keep normal-size upload allocations in a retained block. In addition to
            // reducing allocation churn, this avoids Intel drivers entering
            // vkFreeMemory while a completed upload is being retired.
            if (requirements.size <= STAGING_POOL_BLOCK_SIZE)
            {
                uint32_t memoryTypeIndex = 0;
                VkResult result = vmaFindMemoryTypeIndex(m_Allocator, requirements.memoryTypeBits, &allocCreateInfo, &memoryTypeIndex);
                CW_ENGINE_ASSERT(result == VK_SUCCESS);

                if (result == VK_SUCCESS && m_StagingPools[memoryTypeIndex] == VK_NULL_HANDLE)
                {
                    VmaPoolCreateInfo poolCreateInfo{};
                    poolCreateInfo.memoryTypeIndex = memoryTypeIndex;
                    poolCreateInfo.blockSize = STAGING_POOL_BLOCK_SIZE;
                    poolCreateInfo.minBlockCount = 1;

                    result = vmaCreatePool(m_Allocator, &poolCreateInfo, &m_StagingPools[memoryTypeIndex]);
                    CW_ENGINE_ASSERT(result == VK_SUCCESS);
                }

                if (result == VK_SUCCESS)
                    allocCreateInfo.pool = m_StagingPools[memoryTypeIndex];
            }
        }

        VmaAllocationInfo allocInfo;
        VmaAllocation memory;
        VkResult result = vmaAllocateMemoryForBuffer(m_Allocator, buffer, &allocCreateInfo, &memory, &allocInfo);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
        if (result == VK_SUCCESS)
        {
            result = vkBindBufferMemory(m_LogicalDevice, buffer, allocInfo.deviceMemory, allocInfo.offset);
            CW_ENGINE_ASSERT(result == VK_SUCCESS);
        }

        uint32_t id = m_AllocCounter++;
        String name = tag ? fmt::format("Buffer #{} [{}]", id, tag) : fmt::format("Buffer #{}", id);
        m_AllocationRecords[memory] = { name, allocInfo.size };
        return memory;
    }

    void VulkanDevice::SetAllocationName(VmaAllocation allocation, const char* name)
    {
        Lock lock(m_AllocationMutex);
        auto it = m_AllocationRecords.find(allocation);
        if (it != m_AllocationRecords.end())
            it->second.name = name;
    }

    void VulkanDevice::GetAllocationInfo(VmaAllocation allocation, VkDeviceMemory& memory, VkDeviceSize& offset)
    {
        VmaAllocationInfo allocInfo;
        vmaGetAllocationInfo(m_Allocator, allocation, &allocInfo);
        memory = allocInfo.deviceMemory;
        offset = allocInfo.offset;
    }

    void* VulkanDevice::MapMemory(VmaAllocation allocation)
    {
        void* data = nullptr;
        const VkResult result = vmaMapMemory(m_Allocator, allocation, &data);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
        return result == VK_SUCCESS ? data : nullptr;
    }

    void VulkanDevice::UnmapMemory(VmaAllocation allocation) { vmaUnmapMemory(m_Allocator, allocation); }

    void VulkanDevice::FreeMemory(VmaAllocation allocation)
    {
        Lock lock(m_AllocationMutex);
        m_AllocationRecords.erase(allocation);
        vmaFreeMemory(m_Allocator, allocation);
    }

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
