#include "cwpch.h"

#include "Platform/Vulkan/VulkanRenderAPI.h"

#include "Platform/Vulkan/VulkanCommandBuffer.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanGpuBufferManager.h"
#include "Platform/Vulkan/VulkanRenderPass.h"

#include "Platform/Vulkan/VulkanUniformParams.h"

#include "Crowny/Application/Application.h"
#include "Crowny/RenderAPI/RenderCapabilities.h"
#include "Crowny/Window/RenderWindow.h"

#include "Crowny/Common/Timer.h"

#include <GLFW/glfw3.h>

#define VMA_IMPLEMENTATION
//#define VMA_DEBUG_LOG(format, ...) do { \
//       printf(format, __VA_ARGS__); \
//       printf("\n"); \
//   } while(false)
#include <vma/vk_mem_alloc.h>

// #define CW_DEBUG 0
// #undef CW_DEBUG

namespace Crowny
{
    VkAllocationCallbacks* gVulkanAllocator = nullptr;

    PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT = nullptr;
    PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT = nullptr;

    PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR = nullptr;
    PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR = nullptr;
    PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR = nullptr;
    PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR = nullptr;
    PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR = nullptr;
    PFN_vkQueuePresentKHR vkQueuePresentKHR = nullptr;

    VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                 VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                 const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                 void* pUserData)
    {
        Stringstream debugMessage;
        debugMessage << "[" << pCallbackData->messageIdNumber << "][" << pCallbackData->pMessageIdName
                     << "] : " << pCallbackData->pMessage;
        if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
        {
            CW_ENGINE_INFO(debugMessage.str());
        }
        else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
        {
            CW_ENGINE_INFO(debugMessage.str());
        }
        else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        {
            CW_ENGINE_WARN(debugMessage.str());
        }
        else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        {
            CW_ENGINE_ERROR(debugMessage.str());
        }
        // CW_ENGINE_ASSERT(false);

        return VK_FALSE;
    }

    void VulkanRenderAPI::Init()
    {
        VkApplicationInfo appInfo;
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pNext = nullptr;
        appInfo.pApplicationName = "Crowny app";
        appInfo.applicationVersion = VK_VERSION_1_2;
        appInfo.pEngineName = "Crowny";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0); // TODO: Engine version

        appInfo.apiVersion = VK_API_VERSION_1_2;
        uint32_t count;
        vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
        Vector<VkExtensionProperties> exts(count);
        vkEnumerateInstanceExtensionProperties(nullptr, &count, exts.data());
#ifdef CW_DEBUG
        Vector<const char*> layers = { "VK_LAYER_KHRONOS_validation", "VK_LAYER_NV_optimus" };
        uint32_t numExtensions;
        const char** glfwExts = glfwGetRequiredInstanceExtensions(&numExtensions);
        Vector<const char*> extensions(glfwExts, glfwExts + numExtensions);

        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        uint32_t numLayers = (uint32_t)layers.size();
#else
        Vector<const char*> layers;
        uint32_t numExtensions;
        const char** glfwExts = glfwGetRequiredInstanceExtensions(&numExtensions);
        Vector<const char*> extensions(glfwExts, glfwExts + numExtensions);
        uint32_t numLayers = (uint32_t)layers.size();
#endif
        numExtensions = (uint32_t)extensions.size();
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        Vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
        for (uint32_t i = 0; i < numLayers; i++) // 1 validation layer
        {
            const char* layerName = layers[i];
            bool layerFound = false;
            for (const auto& layerProps : availableLayers)
            {
                if (std::strcmp(layerName, layerProps.layerName) == 0)
                {
                    layerFound = true;
                    break;
                }
            }
            if (!layerFound)
                CW_ENGINE_ASSERT(false, String("Validation layer not found: ") + layerName);
        }

        VkInstanceCreateInfo instanceInfo;
        instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceInfo.pNext = nullptr;
        instanceInfo.flags = 0;
        instanceInfo.pApplicationInfo = &appInfo;
        instanceInfo.enabledLayerCount = numLayers;
        instanceInfo.ppEnabledLayerNames = layers.data();
        instanceInfo.enabledExtensionCount = numExtensions;
        instanceInfo.ppEnabledExtensionNames = extensions.data();

        VkResult result = vkCreateInstance(&instanceInfo, gVulkanAllocator, &m_Instance);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);

#if CW_DEBUG
        VkDebugReportFlagsEXT debugFlags =
          VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
        GET_INSTANCE_PROC_ADDR(m_Instance, CreateDebugUtilsMessengerEXT);
        GET_INSTANCE_PROC_ADDR(m_Instance, DestroyDebugUtilsMessengerEXT);

        VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCI{};
        debugUtilsMessengerCI.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugUtilsMessengerCI.messageSeverity =
          VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugUtilsMessengerCI.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugUtilsMessengerCI.pfnUserCallback = &DebugCallback;

        result =
          vkCreateDebugUtilsMessengerEXT(m_Instance, &debugUtilsMessengerCI, gVulkanAllocator, &m_DebugUtilsMessenger);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
#endif

        // need molten vk
        result = vkEnumeratePhysicalDevices(m_Instance, &m_NumDevices, nullptr);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
        Vector<VkPhysicalDevice> physicalDevices(m_NumDevices);
        result = vkEnumeratePhysicalDevices(m_Instance, &m_NumDevices, physicalDevices.data());
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
        m_Devices.resize(m_NumDevices);
        for (uint32_t i = 0; i < m_NumDevices; i++)
            m_Devices[i] = CreateRef<VulkanDevice>(physicalDevices[i], i);

        for (uint32_t i = 0; i < m_NumDevices; i++)
        {
            bool isPrimary = m_Devices[i]->GetDeviceProperties().deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
            if (isPrimary)
            {
                // m_Devices[i]->SetPrimary();
                m_PrimaryDevices.push_back(m_Devices[i]);

                if (i != 0)
                {
                    // m_Devices[0]->SetIndex(i);
                    // m_Devices[i]->SetIndex(0);
                    std::swap(m_Devices[0], m_Devices[i]);
                }

                break;
            }
        }

        if (m_PrimaryDevices.size() == 0)
        {
            // m_Devices[0]->SetPrimary();
            m_PrimaryDevices.push_back(m_Devices[0]);
        }

        VulkanGpuBufferManager::StartUp();

        // GPUInfo gpuInfo;
        // gpuInfo.numGPUs = std::min(4U, m_NumDevices);

        // for (uint32_t i = 0; i < gpuInfo.numGPUs; i++)
        // gpuInfo.names[i] = m_Devices[i]->GetDeviceProperties().deviceName;

        // PlatformUtility::SetGPUInfo(gpuInfo);
        // should these even be here?
        GET_INSTANCE_PROC_ADDR(m_Instance, GetPhysicalDeviceSurfaceSupportKHR);
        GET_INSTANCE_PROC_ADDR(m_Instance, GetPhysicalDeviceSurfaceFormatsKHR);
        GET_INSTANCE_PROC_ADDR(m_Instance, GetPhysicalDeviceSurfaceCapabilitiesKHR);
        GET_INSTANCE_PROC_ADDR(m_Instance, GetPhysicalDeviceSurfacePresentModesKHR);

        VkDevice presentDevice = m_PrimaryDevices[0]->GetLogicalDevice();
        GET_DEVICE_PROC_ADDR(presentDevice, CreateSwapchainKHR);
        GET_DEVICE_PROC_ADDR(presentDevice, DestroySwapchainKHR);
        GET_DEVICE_PROC_ADDR(presentDevice, GetSwapchainImagesKHR);
        GET_DEVICE_PROC_ADDR(presentDevice, AcquireNextImageKHR);
        GET_DEVICE_PROC_ADDR(presentDevice, QueuePresentKHR);

        InitCaps();

        VulkanRenderPasses::StartUp();
        VulkanTransferManager::StartUp();
        m_CommandBuffer = std::static_pointer_cast<VulkanCommandBuffer>(CommandBuffer::Create(GRAPHICS_QUEUE));
    }

    VulkanCommandBuffer* VulkanRenderAPI::GetCB(const Ref<CommandBuffer>& buffer)
    {
        if (buffer != nullptr)
            return static_cast<VulkanCommandBuffer*>(buffer.get());
        return static_cast<VulkanCommandBuffer*>(m_CommandBuffer.get());
    }

    void VulkanRenderAPI::SwapBuffers(const Ref<RenderTarget>& renderTarget, uint32_t syncMask)
    {
        SubmitCommandBuffer(m_CommandBuffer, syncMask);
        renderTarget->SwapBuffers(syncMask);

        GetPresentDevice()->Refresh();
    }

    void VulkanRenderAPI::SetRenderTarget(const Ref<RenderTarget>& renderTarget, uint32_t readOnlyFlags,
                                          RenderSurfaceMask loadMask, const Ref<CommandBuffer>& commandBuffer)
    {
        VulkanCmdBuffer* vkCB = GetCB(commandBuffer)->GetInternal();
        vkCB->SetRenderTarget(renderTarget, readOnlyFlags, loadMask);
    }

    void VulkanRenderAPI::SetVertexBuffers(uint32_t idx, Ref<VertexBuffer>* buffers, uint32_t numBuffers,
                                           const Ref<CommandBuffer>& commandBuffer)
    {
        VulkanCmdBuffer* vkCB = GetCB(commandBuffer)->GetInternal();
        vkCB->SetVertexBuffers(idx, buffers, numBuffers);
    }

    void VulkanRenderAPI::SetViewport(float x, float y, float width, float height,
                                      const Ref<CommandBuffer>& commandBuffer)
    {
        VulkanCmdBuffer* vkCB = GetCB(commandBuffer)->GetInternal();
        vkCB->SetViewport({ x, y, width, height });
    }

    void VulkanRenderAPI::SetIndexBuffer(const Ref<IndexBuffer>& indexBuffer, const Ref<CommandBuffer>& commandBuffer)
    {
        VulkanCmdBuffer* vkCB = GetCB(commandBuffer)->GetInternal();
        vkCB->SetIndexBuffer(indexBuffer);
    }

    void VulkanRenderAPI::SubmitCommandBuffer(const Ref<CommandBuffer>& commandBuffer, uint32_t syncMask)
    {
        VulkanCommandBuffer* cmdBuffer = GetCB(commandBuffer);
        VulkanTransferManager& cbm = VulkanTransferManager::Get();
        cbm.FlushTransferBuffers();
        cmdBuffer->Submit(syncMask);
        if (cmdBuffer == m_CommandBuffer.get())
            m_CommandBuffer = std::static_pointer_cast<VulkanCommandBuffer>(CommandBuffer::Create(GRAPHICS_QUEUE));
    }

    void VulkanRenderAPI::SetDrawMode(DrawMode drawMode, const Ref<CommandBuffer>& commandBuffer)
    {
        VulkanCmdBuffer* vkCB = GetCB(commandBuffer)->GetInternal();
        vkCB->SetDrawMode(drawMode);
    }

    void VulkanRenderAPI::ClearViewport(uint32_t buffers, const glm::vec4& color, float depth, uint16_t stencil,
                                        uint8_t targetMask, const Ref<CommandBuffer>& commandBuffer)
    {
        VulkanCmdBuffer* vkCB = GetCB(commandBuffer)->GetInternal();
        vkCB->ClearViewport(buffers, color, depth, stencil, targetMask);
    }

    void VulkanRenderAPI::ClearRenderTarget(uint32_t buffers, const glm::vec4& color, float depth, uint16_t stencil,
                                            uint8_t targetMask, const Ref<CommandBuffer>& commandBuffer)
    {
        VulkanCmdBuffer* vkCB = GetCB(commandBuffer)->GetInternal();
        vkCB->ClearRenderTarget(buffers, color, depth, stencil, targetMask);
    }

    void VulkanRenderAPI::Draw(uint32_t vertexOffset, uint32_t vertexCount, uint32_t instanceCount,
                               const Ref<CommandBuffer>& commandBuffer)
    {
        VulkanCmdBuffer* vkCB = GetCB(commandBuffer)->GetInternal();
        vkCB->Draw(vertexOffset, vertexCount, instanceCount);
        // TODO: Render stats: draw call, verts, prims
    }

    void VulkanRenderAPI::DrawIndexed(uint32_t startIndex, uint32_t indexCount, uint32_t vertexOffset,
                                      uint32_t vertexCount, uint32_t instanceCount,
                                      const Ref<CommandBuffer>& commandBuffer)
    {
        uint32_t primCount = 0;
        VulkanCmdBuffer* vkCB = GetCB(commandBuffer)->GetInternal();
        vkCB->DrawIndexed(startIndex, indexCount, vertexOffset, instanceCount);
        // TODO: Render stats: draw call, verts, prims
    }

    void VulkanRenderAPI::DispatchCompute(uint32_t x, uint32_t y, uint32_t z, const Ref<CommandBuffer>& commandBuffer)
    {
        // m_CommandBuffer->Dispatch(x, y, z); //TODO: Compute calls
    }

    void VulkanRenderAPI::SetUniforms(const Ref<UniformParams>& params, const Ref<CommandBuffer>& commandBuffer)
    {
        for (uint32_t i = 0; i < 6; i++)
        {
            Ref<UniformDesc> desc = params->GetUniformDesc((ShaderType)i);
            if (desc == nullptr)
                continue;

            for (auto iter = desc->Uniforms.begin(); iter != desc->Uniforms.end(); ++iter)
            {
                Ref<UniformBufferBlock> block = params->GetUniformBlockBuffer(iter->second.Set, iter->second.Slot);
                if (block != nullptr)
                    block->FlushToGpu();
            }
        }
        VulkanCmdBuffer* vkCB = GetCB(commandBuffer)->GetInternal();
        vkCB->SetUniforms(params);
    }

    void VulkanRenderAPI::SetGraphicsPipeline(const Ref<GraphicsPipeline>& pipeline,
                                              const Ref<CommandBuffer>& commandBuffer)
    {
        VulkanCmdBuffer* vkCB = GetCB(commandBuffer)->GetInternal();
        vkCB->SetPipeline(pipeline); // TODO: stats for pipeline change
    }

    void VulkanRenderAPI::SetComputePipeline(const Ref<ComputePipeline>& pipeline,
                                             const Ref<CommandBuffer>& commandBuffer)
    {
        VulkanCmdBuffer* vkCB = GetCB(commandBuffer)->GetInternal();
        vkCB->SetPipeline(pipeline); // TODO: stats
    }

    void VulkanRenderAPI::Shutdown()
    {
        VulkanTransferManager::Shutdown();
        VulkanRenderPasses::Shutdown();
        VulkanGpuBufferManager::Shutdown();
        m_CommandBuffer = nullptr;
        for (uint32_t i = 0; i < (uint32_t)m_Devices.size(); i++)
            m_Devices[i]->WaitIdle();
        m_Devices.clear();
        m_PrimaryDevices.clear();
#ifdef CW_DEBUG
        vkDestroyDebugUtilsMessengerEXT(m_Instance, m_DebugUtilsMessenger, gVulkanAllocator);
#endif

        vkDestroyInstance(m_Instance, gVulkanAllocator);
    }

    void VulkanRenderAPI::InitCaps()
    {
        m_NumDevices = (uint32_t)m_Devices.size();
        m_CurrentCapabilities = new RenderCapabilities[m_NumDevices];

        uint32_t deviceIdx = 0;
        for (auto& device : m_Devices)
        {
            RenderCapabilities& caps = m_CurrentCapabilities[deviceIdx];
            const VkPhysicalDeviceProperties& devProps = device->GetDeviceProperties();
            const VkPhysicalDeviceFeatures& devFeatures = device->GetDeviceFeatures();
            const VkPhysicalDeviceLimits& devLimits = devProps.limits;

            DriverVersion driverVersion;
            driverVersion.major = ((uint32_t)(devProps.apiVersion) >> 22);
            driverVersion.minor = ((uint32_t)(devProps.apiVersion) >> 12) & 0x3ff;
            driverVersion.release = (uint32_t)(devProps.apiVersion) & 0xfff;
            driverVersion.build = 0;

            caps.DriverVersion = driverVersion;
            caps.DeviceName = devProps.deviceName;

            switch (devProps.vendorID)
            {
            case (0x10DE):
                caps.DeviceVendor = GPU_NVIDIA;
                break;
            case (0x1002):
                caps.DeviceVendor = GPU_AMD;
                break;
            case (0x163C):
            case (0x8086):
                caps.DeviceVendor = GPU_INTEL;
                break;
            default:
                caps.DeviceVendor = GPU_UNKNOWN;
                break;
            }

            caps.RenderAPIName = "Vulkan";

            if (devFeatures.textureCompressionBC)
                caps.SetCapability(CW_TEXTURE_COMPRESSION_BC);

            if (devFeatures.textureCompressionETC2)
                caps.SetCapability(CW_TEXTURE_COMPRESSION_ETC2);

            if (devFeatures.textureCompressionASTC_LDR)
                caps.SetCapability(CW_TEXTURE_COMPRESSION_ASTC);

            caps.SetCapability(CW_COMPUTE_SHADER);
            caps.SetCapability(CW_LOAD_STORE);
            caps.SetCapability(CW_LOAD_STORE_MSAA);

            caps.SetCapability(CW_COMPUTE_SHADER);
            caps.SetCapability(CW_LOAD_STORE);
            caps.SetCapability(CW_LOAD_STORE_MSAA);
            caps.SetCapability(CW_TEXTURE_VIEWS);
            caps.SetCapability(CW_RENDER_TARGET_LAYERS);
            caps.SetCapability(CW_MULTITHREADED_CB);
            caps.Conventions.YAxis = Conventions::Axis::Down;
            caps.Conventions.MatrixOrder = Conventions::MatrixOrder::ColumnMajor;
            caps.MaxBoundVertexBuffers = devLimits.maxVertexInputBindings;
            caps.NumMultiRenderTargets = devLimits.maxColorAttachments;

            caps.NumTextureUnitsPerStage[FRAGMENT_SHADER] = devLimits.maxPerStageDescriptorSampledImages;
            caps.NumTextureUnitsPerStage[VERTEX_SHADER] = devLimits.maxPerStageDescriptorSampledImages;
            caps.NumTextureUnitsPerStage[COMPUTE_SHADER] = devLimits.maxPerStageDescriptorSampledImages;

            caps.NumGpuParamBlockBuffersPerStage[FRAGMENT_SHADER] = devLimits.maxPerStageDescriptorUniformBuffers;
            caps.NumGpuParamBlockBuffersPerStage[VERTEX_SHADER] = devLimits.maxPerStageDescriptorUniformBuffers;
            caps.NumGpuParamBlockBuffersPerStage[COMPUTE_SHADER] = devLimits.maxPerStageDescriptorUniformBuffers;
            caps.NumLoadStoreTextureUnitsPerStage[FRAGMENT_SHADER] = devLimits.maxPerStageDescriptorStorageImages;
            caps.NumLoadStoreTextureUnitsPerStage[COMPUTE_SHADER] = devLimits.maxPerStageDescriptorStorageImages;

            if (devFeatures.geometryShader)
            {
                caps.SetCapability(CW_GEOMETRY_SHADER);
                caps.AddShaderProfile("gs_5_0");
                caps.NumTextureUnitsPerStage[GEOMETRY_SHADER] = devLimits.maxPerStageDescriptorSampledImages;
                caps.NumGpuParamBlockBuffersPerStage[GEOMETRY_SHADER] = devLimits.maxPerStageDescriptorUniformBuffers;
                caps.GeometryShaderNumOutputVertices = devLimits.maxGeometryOutputVertices;
            }

            if (devFeatures.tessellationShader)
            {
                caps.SetCapability(CW_TESSELLATION_SHADER);
                caps.NumTextureUnitsPerStage[HULL_SHADER] = devLimits.maxPerStageDescriptorSampledImages;
                caps.NumTextureUnitsPerStage[DOMAIN_SHADER] = devLimits.maxPerStageDescriptorSampledImages;
                caps.NumGpuParamBlockBuffersPerStage[HULL_SHADER] = devLimits.maxPerStageDescriptorUniformBuffers;
                caps.NumGpuParamBlockBuffersPerStage[DOMAIN_SHADER] = devLimits.maxPerStageDescriptorUniformBuffers;
            }

            caps.NumCombinedTextureUnits =
              caps.NumTextureUnitsPerStage[FRAGMENT_SHADER] + caps.NumTextureUnitsPerStage[VERTEX_SHADER] +
              caps.NumTextureUnitsPerStage[GEOMETRY_SHADER] + caps.NumTextureUnitsPerStage[HULL_SHADER] +
              caps.NumTextureUnitsPerStage[DOMAIN_SHADER] + caps.NumTextureUnitsPerStage[COMPUTE_SHADER];

            caps.NumCombinedParamBlockBuffers = caps.NumGpuParamBlockBuffersPerStage[FRAGMENT_SHADER] +
                                                caps.NumGpuParamBlockBuffersPerStage[VERTEX_SHADER] +
                                                caps.NumGpuParamBlockBuffersPerStage[GEOMETRY_SHADER] +
                                                caps.NumGpuParamBlockBuffersPerStage[HULL_SHADER] +
                                                caps.NumGpuParamBlockBuffersPerStage[DOMAIN_SHADER] +
                                                caps.NumGpuParamBlockBuffersPerStage[COMPUTE_SHADER];

            caps.NumCombinedLoadStoreTextureUnits = caps.NumLoadStoreTextureUnitsPerStage[FRAGMENT_SHADER] +
                                                    caps.NumLoadStoreTextureUnitsPerStage[COMPUTE_SHADER];

            caps.AddShaderProfile("glsl");
            deviceIdx++;
        }
    }

    VulkanRenderAPI& gVulkanRenderAPI() { return static_cast<VulkanRenderAPI&>(RenderAPI::Get()); }
} // namespace Crowny
#undef CW_DEBUG