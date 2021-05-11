#include "cwpch.h"

#include "Platform/Vulkan/VulkanRendererAPI.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Renderer/RenderCapabilities.h"

#include "Crowny/Common/Timer.h"

#include <GLFW/glfw3.h>

namespace Crowny
{
    
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

    VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
    {
        std::stringstream debugMessage;
        debugMessage << "[" << pCallbackData->messageIdNumber << "][" << pCallbackData->pMessageIdName << "] : " << pCallbackData->pMessage;
        if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
            CW_ENGINE_INFO(debugMessage.str());
        }
        else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
            CW_ENGINE_INFO(debugMessage.str());
        }
        else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
            CW_ENGINE_WARN(debugMessage.str());
        }
        else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
            CW_ENGINE_ERROR(debugMessage.str());
        }
        //CW_ENGINE_ASSERT(false);
        
        return VK_FALSE;
    }
    
    void VulkanRendererAPI::Init()
    {
        VkApplicationInfo appInfo;
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pNext = nullptr;
        appInfo.pApplicationName = "Crowny app";
        appInfo.applicationVersion = VK_MAKE_VERSION(1,0,0);
        appInfo.pEngineName = "Crowny";
        appInfo.engineVersion = VK_MAKE_VERSION(1,0,0); // TODO: Engine version

        appInfo.apiVersion = VK_API_VERSION_1_1;

//#ifdef CW_DEBUG
        std::vector<const char*> layers = { "VK_LAYER_KHRONOS_validation", "VK_LAYER_NV_optimus" };
        uint32_t numExtensions;
        const char** glfwExts = glfwGetRequiredInstanceExtensions(&numExtensions);
        std::vector<const char*> extensions(glfwExts, glfwExts + numExtensions);
        
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        for (auto ext : extensions)
            CW_ENGINE_INFO(ext);
        uint32_t numLayers = (uint32_t)layers.size();
//#else
//        std::vector<const char*> layers;
 //       uint32_t numExtensions;
 //       const char** glfwExts = glfwGetRequiredInstanceExtensions(&numExtensions);
  //      std::vector<const char*> extensions(glfwExts, glfwExts + numExtensions);
   //     uint32_t numLayers = (uint32_t)layers.size();
//#endif
        numExtensions = extensions.size();
        CW_ENGINE_INFO(numExtensions);
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> availableLayers(layerCount);
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
                CW_ENGINE_ASSERT(false, std::string("Validation layer not found: ") + layerName);
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
        
        //VkResult result = vkCreateInstance(&instanceInfo, gVulkanAllocator, &m_Instance);
        VkResult result = vkCreateInstance(&instanceInfo, nullptr, &m_Instance);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
        
//#if CW_DEBUG
        VkDebugReportFlagsEXT debugFlags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
        GET_INSTANCE_PROC_ADDR(m_Instance, CreateDebugUtilsMessengerEXT);
        GET_INSTANCE_PROC_ADDR(m_Instance, DestroyDebugUtilsMessengerEXT);
               
        VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCI{};
        debugUtilsMessengerCI.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugUtilsMessengerCI.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugUtilsMessengerCI.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugUtilsMessengerCI.pfnUserCallback = &DebugCallback;
        
        result = vkCreateDebugUtilsMessengerEXT(m_Instance, &debugUtilsMessengerCI, nullptr, &m_DebugUtilsMessenger);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
//#endif
        
        // need molten vk
        result = vkEnumeratePhysicalDevices(m_Instance, &m_NumDevices, nullptr);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
        std::vector<VkPhysicalDevice> physicalDevices(m_NumDevices);
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
                //m_Devices[i]->SetPrimary();
                m_PrimaryDevices.push_back(m_Devices[i]);

                if (i != 0)
                {
                    //m_Devices[0]->SetIndex(i);
                    //m_Devices[i]->SetIndex(0);
                    std::swap(m_Devices[0], m_Devices[i]);
                }
                
                break;
            }
        }
        
        if (m_PrimaryDevices.size() == 0)
        {
            //m_Devices[0]->SetPrimary();
            m_PrimaryDevices.push_back(m_Devices[0]);
        }
        
        //GPUInfo gpuInfo;
        //gpuInfo.numGPUs = std::min(4U, m_NumDevices);
        
        //for (uint32_t i = 0; i < gpuInfo.numGPUs; i++)
            //gpuInfo.names[i] = m_Devices[i]->GetDeviceProperties().deviceName;
        
        //PlatformUtility::SetGPUInfo(gpuInfo);
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
        result = glfwCreateWindowSurface(m_Instance, (GLFWwindow*)Application::Get().GetWindow().GetNativeWindow(), nullptr, &m_Surface);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
        /*
        VkXlibSurfaceCreateInfoKHR surfaceCreateInfo = {};
        surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
        
        surfaceCreateInfo.flags = 0;
        surfaceCreateInfo.pNext = nullptr;
        surfaceCreateInfo.window = glfwGetX11Window((GLFWwindow*)Application::Get().GetWindow().GetNativeWindow());
        surfaceCreateInfo.dpy = glfwGetX11Display();
        err = vkCreateWaylandSurfaceKHR(instance, &surfaceCreateInfo, nullptr, &surface);
        */
        VkPhysicalDevice device = m_PrimaryDevices[0]->GetPhysicalDevice();
        uint32_t queueCount;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueCount, nullptr);
        CW_ENGINE_ASSERT(queueCount > 0);
        
        std::vector<VkQueueFamilyProperties> queueProps(queueCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueCount, queueProps.data());

        std::vector<VkBool32> supportsPresent(queueCount);
        for (uint32_t i = 0; i < queueCount; i++)
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_Surface, &supportsPresent[i]);
        
        uint32_t graphicsQueueNodeIdx = std::numeric_limits<uint32_t>::max();
        uint32_t presentQueueNodeIdx = std::numeric_limits<uint32_t>::max();
        for (uint32_t i = 0; i < queueCount; i++)
        {
            if (queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                if (graphicsQueueNodeIdx == std::numeric_limits<uint32_t>::max())
                    graphicsQueueNodeIdx = i;
                if (supportsPresent[i] == VK_TRUE)
                {
                    graphicsQueueNodeIdx = i;
                    presentQueueNodeIdx = i;
                    break;
                }
            }
        }
        
        if (presentQueueNodeIdx == std::numeric_limits<uint32_t>::max())
        {
            for (uint32_t i = 0; i < queueCount; i++)
            {
                if (supportsPresent[i] == VK_TRUE)
                {
                    presentQueueNodeIdx = i;
                    break;
                }
            }
        }
        
        CW_ENGINE_ASSERT(graphicsQueueNodeIdx != std::numeric_limits<uint32_t>::max() && presentQueueNodeIdx != std::numeric_limits<uint32_t>::max());
        CW_ENGINE_ASSERT(graphicsQueueNodeIdx == presentQueueNodeIdx);
        uint32_t width = Application::Get().GetWindow().GetWidth();
        uint32_t height = Application::Get().GetWindow().GetHeight();
        bool vsync = Application::Get().GetWindow().GetVSync();
        SurfaceFormat surface = GetPresentDevice()->GetSurfaceFormat(m_Surface);
        VulkanRenderPasses::StartUp(); // has to be done before swapchain is created
        m_SwapChain = new VulkanSwapChain(m_Surface, width, height, vsync, surface.ColorFormat, surface.ColorSpace, true, surface.DepthFormat);
        m_CmdBuffer = std::static_pointer_cast<VulkanCmdBuffer>(CommandBuffer::Create(GRAPHICS_QUEUE));
        m_CommandBuffer = m_CmdBuffer.get()->GetBuffer();
    }

    void VulkanRendererAPI::SwapBuffers()
    {
        VulkanSemaphore* semaphore[1] = { m_CommandBuffer->GetRenderCompleteSemaphore() };
        SubmitCommandBuffer(m_CmdBuffer);
        
        VulkanQueue* queue = GetPresentDevice()->GetQueue(GRAPHICS_QUEUE, 0); // present queue
        VkResult result = queue->Present(m_SwapChain, semaphore, 1);
        if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR)
            RebuildSwapChain();
        GetPresentDevice()->Refresh();
    }

    void VulkanRendererAPI::SetRenderTarget(const Ref<Framebuffer>& framebuffer)
    {
        m_CommandBuffer->SetRenderTarget(framebuffer);
    }
    
    void VulkanRendererAPI::SetVertexBuffers(uint32_t idx, Ref<VertexBuffer>* buffers, uint32_t numBuffers)
    {
        m_CommandBuffer->SetVertexBuffers(idx, buffers, numBuffers);
    }

    void VulkanRendererAPI::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
    {
        m_CommandBuffer->SetViewport(Rect2F(x, y, width, height));
    }

    void VulkanRendererAPI::SetIndexBuffer(const Ref<IndexBuffer>& indexBuffer)
    {
        m_CommandBuffer->SetIndexBuffer(indexBuffer);
    }

    void VulkanRendererAPI::SubmitCommandBuffer(const Ref<CommandBuffer>& commandBuffer)
    {
        VulkanCommandBuffer* cmdBuffer = std::static_pointer_cast<VulkanCmdBuffer>(commandBuffer).get()->GetBuffer();
        if (cmdBuffer == nullptr)
            m_CommandBuffer->Submit();
        else
            cmdBuffer->Submit();

        if (cmdBuffer == m_CommandBuffer)
        {
            m_CmdBuffer = std::static_pointer_cast<VulkanCmdBuffer>(CommandBuffer::Create(GRAPHICS_QUEUE));
            m_CommandBuffer = m_CmdBuffer.get()->GetBuffer();
        }
    }
    
    void VulkanRendererAPI::RebuildSwapChain()
    {
        GetPresentDevice()->WaitIdle();
        VulkanSwapChain* old = m_SwapChain;
        uint32_t width = Application::Get().GetWindow().GetWidth();
        uint32_t height = Application::Get().GetWindow().GetHeight();
        bool vsync = Application::Get().GetWindow().GetVSync();
        SurfaceFormat surface = GetPresentDevice()->GetSurfaceFormat(m_Surface);
        m_SwapChain = new VulkanSwapChain(m_Surface, width, height, vsync, surface.ColorFormat, surface.ColorSpace, true, surface.DepthFormat);
    }

    void VulkanRendererAPI::SetDrawMode(DrawMode drawMode)
    {
        m_CommandBuffer->SetDrawMode(drawMode);
    }

    void VulkanRendererAPI::Draw(uint32_t vertexOffset, uint32_t vertexCount, uint32_t instanceCount)
    {
        m_CommandBuffer->Draw(vertexOffset, vertexCount, instanceCount);
        //TODO: Render stats: draw call, verts, prims
    }
    
    void VulkanRendererAPI::DrawIndexed(uint32_t startIndex, uint32_t indexCount, uint32_t vertexOffset, uint32_t vertexCount, uint32_t instanceCount)
    {
        uint32_t primCount = 0;
        m_CommandBuffer->DrawIndexed(startIndex, indexCount, vertexOffset, instanceCount);
        //TODO: Render stats: draw call, verts, prims
    }

    void VulkanRendererAPI::DispatchCompute(uint32_t x, uint32_t y, uint32_t z)
    {
        //m_CommandBuffer->Dispatch(x, y, z); //TODO: Compute calls
    }

    void VulkanRendererAPI::SetGraphicsPipeline(const Ref<GraphicsPipeline>& pipeline)
    {
        m_CommandBuffer->SetPipeline(pipeline); //TODO: stats for pipeline change
    }
    
    void VulkanRendererAPI::SetComputePipeline(const Ref<ComputePipeline>& pipeline)
    {
        m_CommandBuffer->SetPipeline(pipeline); //TODO: stats
    }
    
    void VulkanRendererAPI::Shutdown()
    {
#ifdef CW_DEBUG
        vkDestroyDebugUtilsMessengerEXT(m_Instance, m_DebugUtilsMessenger, nullptr);
#endif
        vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
        vkDestroyInstance(m_Instance, nullptr);
    }
    
    void VulkanRendererAPI::InitCaps()
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
                    caps.DeviceVendor = GPU_NVIDIA; break;
                case (0x1002):
                    caps.DeviceVendor = GPU_AMD; break;
                case (0x163C):
                case (0x8086):
                    caps.DeviceVendor = GPU_INTEL; break;
                default:
                    caps.DeviceVendor = GPU_UNKNOWN; break;
            }
            CW_ENGINE_INFO(RenderCapabilities::VendorToString(caps.DeviceVendor));
            
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

            caps.NumCombinedTextureUnits
                = caps.NumTextureUnitsPerStage[FRAGMENT_SHADER]
                + caps.NumTextureUnitsPerStage[VERTEX_SHADER]
                + caps.NumTextureUnitsPerStage[GEOMETRY_SHADER]
                + caps.NumTextureUnitsPerStage[HULL_SHADER]
                + caps.NumTextureUnitsPerStage[DOMAIN_SHADER]
                + caps.NumTextureUnitsPerStage[COMPUTE_SHADER];

            caps.NumCombinedParamBlockBuffers
                = caps.NumGpuParamBlockBuffersPerStage[FRAGMENT_SHADER]
                + caps.NumGpuParamBlockBuffersPerStage[VERTEX_SHADER]
                + caps.NumGpuParamBlockBuffersPerStage[GEOMETRY_SHADER]
                + caps.NumGpuParamBlockBuffersPerStage[HULL_SHADER]
                + caps.NumGpuParamBlockBuffersPerStage[DOMAIN_SHADER]
                + caps.NumGpuParamBlockBuffersPerStage[COMPUTE_SHADER];                

            caps.NumCombinedLoadStoreTextureUnits
                = caps.NumLoadStoreTextureUnitsPerStage[FRAGMENT_SHADER]
                + caps.NumLoadStoreTextureUnitsPerStage[COMPUTE_SHADER];

            caps.AddShaderProfile("glsl");
            deviceIdx++;
        }
    }
    
    
    VulkanRendererAPI& gVulkanRendererAPI()
    {
        return static_cast<VulkanRendererAPI&>(RendererAPI::Get());
    }
}
