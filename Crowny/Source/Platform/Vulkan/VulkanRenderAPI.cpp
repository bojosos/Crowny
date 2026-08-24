#include "cwpch.h"

#include "Platform/Vulkan/VulkanRenderAPI.h"

#include "Platform/Vulkan/VulkanCommandBuffer.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanGenericGpuBuffer.h"
#include "Platform/Vulkan/VulkanGpuBufferManager.h"
#include "Platform/Vulkan/VulkanRenderPass.h"

#include "Platform/Vulkan/VulkanUniformParams.h"
#include "Platform/Vulkan/VulkanVertexBuffer.h"

#include "Crowny/Application/Application.h"
#include "Crowny/RenderAPI/RenderCapabilities.h"
#include "Crowny/Window/RenderWindow.h"

#include "Crowny/Common/Timer.h"
#include "Crowny/Common/Version.h"

#include <GLFW/glfw3.h>

#define VMA_DEBUG_LOG_FORMAT(format, ...) // Suppress VMA allocation logging
#define VMA_IMPLEMENTATION

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"
#endif
#include <vma/vk_mem_alloc.h>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

namespace Crowny
{
    VkAllocationCallbacks* gVulkanAllocator = nullptr;

    PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT = nullptr;
    PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT = nullptr;
    PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT = nullptr;

    PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR = nullptr;
    PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR = nullptr;
    PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR = nullptr;
    PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR = nullptr;
    PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR = nullptr;
    PFN_vkQueuePresentKHR vkQueuePresentKHR = nullptr;

    PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR = nullptr;
    PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR = nullptr;
    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR = nullptr;
    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR = nullptr;
    PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR = nullptr;

    VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                 const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
    {
        // TODO: Re-enable these when I get to fixing the Vulkan queue errors with imgui.
        StringStream debugMessage;
        debugMessage << "[" << pCallbackData->messageIdNumber << "][" << pCallbackData->pMessageIdName << "] : " << pCallbackData->pMessage;
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
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pNext = nullptr;
        appInfo.pEngineName = "Crowny";
        appInfo.pApplicationName = "Crowny Game"; // Note that this is the game of the name and should not hard coded.
        const uint32_t version = VK_MAKE_VERSION(CROWNY_VERSION_MAJOR, CROWNY_VERSION_MINOR, CROWNY_VERSION_MINEST);
        appInfo.engineVersion = version;
        appInfo.applicationVersion = version; // Note that this is game version, not engine
        uint32_t loaderVersion = VK_API_VERSION_1_0;
        const auto enumerateInstanceVersion =
          reinterpret_cast<PFN_vkEnumerateInstanceVersion>(vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion"));
        if (enumerateInstanceVersion != nullptr)
            enumerateInstanceVersion(&loaderVersion);
        if (loaderVersion < VK_API_VERSION_1_1)
        {
            CW_ENGINE_ERROR("Crowny requires a Vulkan 1.1 loader or newer");
            return;
        }
        appInfo.apiVersion = std::min(loaderVersion, VK_API_VERSION_1_3);

        uint32_t availableExtensionsCount = 0;
        VkResult result = vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionsCount, nullptr);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
        Vector<VkExtensionProperties> availableExtensions(availableExtensionsCount);
        result = vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionsCount, availableExtensions.data());
        CW_ENGINE_ASSERT(result == VK_SUCCESS);

        auto hasExtension = [&availableExtensions](const char* name) {
            return std::any_of(availableExtensions.begin(), availableExtensions.end(),
                               [name](const VkExtensionProperties& extension) { return std::strcmp(name, extension.extensionName) == 0; });
        };

        Vector<const char*> layers;
        uint32_t numGLFWExtensions = 0;
        const char** glfwExts = glfwGetRequiredInstanceExtensions(&numGLFWExtensions);
        CW_ENGINE_ASSERT(glfwExts != nullptr && numGLFWExtensions > 0, "GLFW did not provide Vulkan instance extensions");
        Vector<const char*> extensions(glfwExts, glfwExts + numGLFWExtensions);
        auto hasRequestedExtension = [&extensions](const char* name) {
            return std::any_of(extensions.begin(), extensions.end(), [name](const char* extension) { return std::strcmp(name, extension) == 0; });
        };

#ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
        const bool portabilityEnumeration = hasExtension(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        if (portabilityEnumeration && !hasRequestedExtension(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME))
            extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
#else
        const bool portabilityEnumeration = false;
#endif

        uint32_t layerCount = 0;
        result = vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
        Vector<VkLayerProperties> availableLayers(layerCount);
        result = vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
        CW_ENGINE_ASSERT(result == VK_SUCCESS);

#ifdef CW_DEBUG
        const char* validationLayer = "VK_LAYER_KHRONOS_validation";
        const bool hasValidationLayer =
          std::any_of(availableLayers.begin(), availableLayers.end(),
                      [validationLayer](const VkLayerProperties& layer) { return std::strcmp(validationLayer, layer.layerName) == 0; });
        if (hasValidationLayer)
            layers.push_back(validationLayer);
        else
            CW_ENGINE_WARN("Vulkan validation layer is unavailable; continuing without validation");

        const bool hasDebugUtils = hasExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        if (hasDebugUtils && !hasRequestedExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        if (!hasDebugUtils)
            CW_ENGINE_WARN("VK_EXT_debug_utils is unavailable; Vulkan validation messages will not be captured");
#else
        const bool hasDebugUtils = false;
#endif

        for (const char* extension : extensions)
            CW_ENGINE_ASSERT(hasExtension(extension), String("Required Vulkan instance extension not found: ") + extension);

        VkInstanceCreateInfo instanceInfo{};
        instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceInfo.pNext = nullptr;
#ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
        if (portabilityEnumeration)
            instanceInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif
        instanceInfo.pApplicationInfo = &appInfo;
        instanceInfo.enabledLayerCount = (uint32_t)layers.size();
        instanceInfo.ppEnabledLayerNames = layers.data();
        instanceInfo.enabledExtensionCount = (uint32_t)extensions.size();
        instanceInfo.ppEnabledExtensionNames = extensions.data();

        result = vkCreateInstance(&instanceInfo, gVulkanAllocator, &m_Instance);
        if (result != VK_SUCCESS || m_Instance == VK_NULL_HANDLE)
        {
            CW_ENGINE_ERROR("Could not create Vulkan instance: {}", (int32_t)result);
            return;
        }

#if CW_DEBUG
        if (hasDebugUtils)
        {
            GET_INSTANCE_PROC_ADDR(m_Instance, CreateDebugUtilsMessengerEXT);
            GET_INSTANCE_PROC_ADDR(m_Instance, DestroyDebugUtilsMessengerEXT);

            VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCI{};
            debugUtilsMessengerCI.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            debugUtilsMessengerCI.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            debugUtilsMessengerCI.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            debugUtilsMessengerCI.pfnUserCallback = &DebugCallback;

            result = vkCreateDebugUtilsMessengerEXT(m_Instance, &debugUtilsMessengerCI, gVulkanAllocator, &m_DebugUtilsMessenger);
            CW_ENGINE_ASSERT(result == VK_SUCCESS);
            GET_INSTANCE_PROC_ADDR(m_Instance, SetDebugUtilsObjectNameEXT);
        }
#endif

        // need molten vk
        result = vkEnumeratePhysicalDevices(m_Instance, &m_NumDevices, nullptr);
        CW_ENGINE_ASSERT(result == VK_SUCCESS && m_NumDevices > 0, "No Vulkan-capable physical device was found");
        if (result != VK_SUCCESS || m_NumDevices == 0)
            return;
        Vector<VkPhysicalDevice> physicalDevices(m_NumDevices);
        result = vkEnumeratePhysicalDevices(m_Instance, &m_NumDevices, physicalDevices.data());
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
        uint32_t selectedDeviceIdx = UINT32_MAX;
        int32_t selectedDeviceScore = -1;
        for (uint32_t i = 0; i < m_NumDevices; i++)
        {
            VkPhysicalDeviceProperties props{};
            vkGetPhysicalDeviceProperties(physicalDevices[i], &props);

            uint32_t extensionCount = 0;
            result = vkEnumerateDeviceExtensionProperties(physicalDevices[i], nullptr, &extensionCount, nullptr);
            if (result != VK_SUCCESS)
                continue;
            Vector<VkExtensionProperties> deviceExtensions(extensionCount);
            result = vkEnumerateDeviceExtensionProperties(physicalDevices[i], nullptr, &extensionCount, deviceExtensions.data());
            if (result != VK_SUCCESS)
                continue;
            const bool supportsSwapChain = std::any_of(deviceExtensions.begin(), deviceExtensions.end(), [](const VkExtensionProperties& extension) {
                return std::strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0;
            });

            uint32_t queueFamilyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[i], &queueFamilyCount, nullptr);
            Vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[i], &queueFamilyCount, queueFamilies.data());
            const bool supportsGraphics = std::any_of(queueFamilies.begin(), queueFamilies.end(), [](const VkQueueFamilyProperties& family) {
                return family.queueCount > 0 && (family.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
            });
            if (!supportsSwapChain || !supportsGraphics)
                continue;

            int32_t score = 0;
            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
                score = 2;
            else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
                score = 1;

            if (score > selectedDeviceScore)
            {
                selectedDeviceIdx = i;
                selectedDeviceScore = score;
            }
        }

        CW_ENGINE_ASSERT(selectedDeviceIdx != UINT32_MAX, "No Vulkan device supports graphics and swap chains");
        if (selectedDeviceIdx == UINT32_MAX)
            return;

        m_Devices.push_back(CreateRef<VulkanDevice>(physicalDevices[selectedDeviceIdx], selectedDeviceIdx, appInfo.apiVersion));
        m_PrimaryDevices.push_back(m_Devices[0]);
        /*
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
        }*/

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
        GET_DEVICE_PROC_ADDR(presentDevice, GetBufferDeviceAddressKHR);
        GET_DEVICE_PROC_ADDR(presentDevice, CreateAccelerationStructureKHR);
        GET_DEVICE_PROC_ADDR(presentDevice, DestroyAccelerationStructureKHR);
        GET_DEVICE_PROC_ADDR(presentDevice, GetAccelerationStructureBuildSizesKHR);
        GET_DEVICE_PROC_ADDR(presentDevice, GetAccelerationStructureDeviceAddressKHR);
        GET_DEVICE_PROC_ADDR(presentDevice, CmdBuildAccelerationStructuresKHR);
        GET_DEVICE_PROC_ADDR(presentDevice, CmdTraceRaysKHR);
        GET_DEVICE_PROC_ADDR(presentDevice, GetRayTracingShaderGroupHandlesKHR);
        GET_DEVICE_PROC_ADDR(presentDevice, CreateRayTracingPipelinesKHR);

        InitCaps();

        VulkanRenderPasses::StartUp();
        VulkanTransferManager::StartUp();
        VulkanTextureManager::StartUp();
        VulkanBufferLayoutManager::StartUp();
        m_CommandBuffer = StaticRefCast<VulkanCommandBuffer>(CommandBuffer::Create(GRAPHICS_QUEUE));
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

    void VulkanRenderAPI::SetRenderTarget(const Ref<RenderTarget>& renderTarget, uint32_t readOnlyFlags, RenderSurfaceMask loadMask,
                                          const Ref<CommandBuffer>& commandBuffer)
    {
        VulkanCmdBuffer* vkCB = GetCB(commandBuffer)->GetInternal();
        vkCB->SetRenderTarget(renderTarget, readOnlyFlags, loadMask);
    }

    void VulkanRenderAPI::SetVertexBuffers(uint32_t idx, Ref<VertexBuffer>* buffers, uint32_t numBuffers, const Ref<CommandBuffer>& commandBuffer)
    {
        VulkanCmdBuffer* vkCB = GetCB(commandBuffer)->GetInternal();
        vkCB->SetVertexBuffers(idx, buffers, numBuffers);
    }

    void VulkanRenderAPI::SetVertexLayout(const Ref<BufferLayout>& vertexLayout, const Ref<CommandBuffer>& commandBuffer)
    {
        VulkanCmdBuffer* vkCB = GetCB(commandBuffer)->GetInternal();
        vkCB->SetVertexLayout(vertexLayout);
    }

    void VulkanRenderAPI::SetViewport(float x, float y, float width, float height, const Ref<CommandBuffer>& commandBuffer)
    {
        VulkanCmdBuffer* vkCB = GetCB(commandBuffer)->GetInternal();
        vkCB->SetViewport({ x, y, width, height });
    }

    void VulkanRenderAPI::SetScissorRect(const Rect2I& rect, const Ref<CommandBuffer>& commandBuffer)
    {
        VulkanCmdBuffer* vkCB = GetCB(commandBuffer)->GetInternal();
        vkCB->SetScissorRect(rect);
    }

    void VulkanRenderAPI::SetIndexBuffer(const Ref<IndexBuffer>& indexBuffer, const Ref<CommandBuffer>& commandBuffer)
    {
        VulkanCmdBuffer* vkCB = GetCB(commandBuffer)->GetInternal();
        vkCB->SetIndexBuffer(indexBuffer);
    }

    void VulkanRenderAPI::SubmitCommandBuffer(const Ref<CommandBuffer>& commandBuffer, uint32_t syncMask)
    {
        VulkanCommandBuffer* cmdBuffer = GetCB(commandBuffer);
        VulkanTransferManager::Get().FlushTransferBuffers();
        cmdBuffer->Submit(syncMask);
        if (cmdBuffer == m_CommandBuffer.get())
            m_CommandBuffer = StaticRefCast<VulkanCommandBuffer>(CommandBuffer::Create(GRAPHICS_QUEUE));
    }

    void VulkanRenderAPI::SetDrawMode(DrawMode drawMode, const Ref<CommandBuffer>& commandBuffer)
    {
        m_DrawMode = drawMode;
        VulkanCmdBuffer* vkCB = GetCB(commandBuffer)->GetInternal();
        vkCB->SetDrawMode(drawMode);
    }

    void VulkanRenderAPI::ClearViewport(uint32_t buffers, const glm::vec4& color, float depth, uint16_t stencil, uint8_t targetMask,
                                        const Ref<CommandBuffer>& commandBuffer)
    {
        VulkanCmdBuffer* vkCB = GetCB(commandBuffer)->GetInternal();
        vkCB->ClearViewport(buffers, color, depth, stencil, targetMask);
    }

    void VulkanRenderAPI::ClearRenderTarget(uint32_t buffers, const glm::vec4& color, float depth, uint16_t stencil, uint8_t targetMask,
                                            const Ref<CommandBuffer>& commandBuffer)
    {
        VulkanCmdBuffer* vkCB = GetCB(commandBuffer)->GetInternal();
        vkCB->ClearRenderTarget(buffers, color, depth, stencil, targetMask);
    }

    void VulkanRenderAPI::Draw(uint32_t vertexOffset, uint32_t vertexCount, uint32_t instanceCount, const Ref<CommandBuffer>& commandBuffer)
    {
        VulkanCmdBuffer* vkCB = GetCB(commandBuffer)->GetInternal();
        vkCB->Draw(vertexOffset, vertexCount, instanceCount);
        RecordDraw(m_DrawMode, vertexCount, instanceCount);
    }

    void VulkanRenderAPI::DrawIndexed(uint32_t startIndex, uint32_t indexCount, uint32_t vertexOffset, uint32_t vertexCount, uint32_t instanceCount,
                                      const Ref<CommandBuffer>& commandBuffer)
    {
        VulkanCmdBuffer* vkCB = GetCB(commandBuffer)->GetInternal();
        vkCB->DrawIndexed(startIndex, indexCount, vertexOffset, instanceCount);
        RecordDraw(m_DrawMode, indexCount, instanceCount);
    }

    void VulkanRenderAPI::DrawIndexedIndirect(const Ref<GenericGpuBuffer>& argumentBuffer, uint32_t argumentOffset, uint32_t drawCount,
                                              uint32_t stride, const Ref<CommandBuffer>& commandBuffer)
    {
        CW_ENGINE_ASSERT(argumentBuffer != nullptr, "Vulkan indirect draw requires an argument buffer");
        CW_ENGINE_ASSERT(stride >= sizeof(DrawIndexedIndirectCommand) && (stride & 3u) == 0,
                         "Vulkan indirect draw stride is invalid");
        if (argumentBuffer == nullptr || drawCount == 0)
            return;

        const uint64_t requiredSize = static_cast<uint64_t>(argumentOffset) + static_cast<uint64_t>(drawCount - 1u) * stride +
                                      sizeof(DrawIndexedIndirectCommand);
        CW_ENGINE_ASSERT(requiredSize <= argumentBuffer->GetBufferSize(), "Vulkan indirect draw range exceeds its argument buffer");
        if (requiredSize > argumentBuffer->GetBufferSize())
            return;

        VulkanGenericGpuBuffer* arguments = static_cast<VulkanGenericGpuBuffer*>(argumentBuffer.get());
        GetCB(commandBuffer)->GetInternal()->DrawIndexedIndirect(arguments->GetBuffer(), argumentOffset, drawCount, stride);
        RecordIndirectDraw(drawCount);
    }

    void VulkanRenderAPI::DrawIndexedIndirectCount(const Ref<GenericGpuBuffer>& argumentBuffer, uint32_t argumentOffset,
                                                   const Ref<GenericGpuBuffer>& countBuffer, uint32_t countOffset, uint32_t maxDrawCount,
                                                   uint32_t stride, const Ref<CommandBuffer>& commandBuffer)
    {
        CW_ENGINE_ASSERT(argumentBuffer != nullptr && countBuffer != nullptr, "Vulkan indirect count draw requires two buffers");
        if (argumentBuffer == nullptr || countBuffer == nullptr || maxDrawCount == 0)
            return;
        CW_ENGINE_ASSERT(countOffset + sizeof(uint32_t) <= countBuffer->GetBufferSize(),
                         "Vulkan indirect draw count range exceeds its buffer");
        if (countOffset + sizeof(uint32_t) > countBuffer->GetBufferSize())
            return;

        const uint64_t requiredSize = static_cast<uint64_t>(argumentOffset) + static_cast<uint64_t>(maxDrawCount - 1u) * stride +
                                      sizeof(DrawIndexedIndirectCommand);
        CW_ENGINE_ASSERT(requiredSize <= argumentBuffer->GetBufferSize(), "Vulkan indirect draw range exceeds its argument buffer");
        if (requiredSize > argumentBuffer->GetBufferSize())
            return;

        VulkanGenericGpuBuffer* arguments = static_cast<VulkanGenericGpuBuffer*>(argumentBuffer.get());
        VulkanGenericGpuBuffer* count = static_cast<VulkanGenericGpuBuffer*>(countBuffer.get());
        GetCB(commandBuffer)->GetInternal()->DrawIndexedIndirectCount(arguments->GetBuffer(), argumentOffset, count->GetBuffer(), countOffset,
                                                                      maxDrawCount, stride);
        RecordIndirectDraw(0); // The GPU count buffer is deliberately not read back for statistics.
    }

    void VulkanRenderAPI::TraceRays(uint32_t width, uint32_t height, const Ref<CommandBuffer>& commandBuffer)
    {
        VulkanCmdBuffer* vkCB = GetCB(commandBuffer)->GetInternal();
        vkCB->TraceRays(width, height);
        RecordRayTracingDispatch();
    }

    void VulkanRenderAPI::DispatchCompute(uint32_t x, uint32_t y, uint32_t z, const Ref<CommandBuffer>& commandBuffer)
    {
        VulkanCmdBuffer* vkCB = GetCB(commandBuffer)->GetInternal();
        vkCB->Dispatch(x, y, z);
        RecordComputeDispatch();
    }

    void VulkanRenderAPI::DispatchComputeIndirect(const Ref<GenericGpuBuffer>& argumentBuffer, uint32_t argumentOffset,
                                                  const Ref<CommandBuffer>& commandBuffer)
    {
        CW_ENGINE_ASSERT(argumentBuffer != nullptr && argumentOffset + sizeof(DispatchIndirectCommand) <= argumentBuffer->GetBufferSize(),
                         "Vulkan indirect dispatch range exceeds its argument buffer");
        if (argumentBuffer == nullptr || argumentOffset + sizeof(DispatchIndirectCommand) > argumentBuffer->GetBufferSize())
            return;

        VulkanGenericGpuBuffer* arguments = static_cast<VulkanGenericGpuBuffer*>(argumentBuffer.get());
        GetCB(commandBuffer)->GetInternal()->DispatchIndirect(arguments->GetBuffer(), argumentOffset);
        RecordComputeDispatch();
    }

    void VulkanRenderAPI::SetUniforms(const Ref<UniformParams>& params, const Ref<CommandBuffer>& commandBuffer)
    {
        for (uint32_t i = 0; i < SHADER_COUNT; i++)
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

    void VulkanRenderAPI::SetGraphicsPipeline(const Ref<GraphicsPipeline>& pipeline, const Ref<CommandBuffer>& commandBuffer)
    {
        VulkanCmdBuffer* vkCB = GetCB(commandBuffer)->GetInternal();
        vkCB->SetPipeline(pipeline); // TODO: stats for pipeline change
    }

    void VulkanRenderAPI::SetRayTracingPipeline(const Ref<RayTracingPipeline>& pipeline, const Ref<CommandBuffer>& commandBuffer)
    {
        VulkanCmdBuffer* vkCB = GetCB(commandBuffer)->GetInternal();
        vkCB->SetPipeline(pipeline); // TODO: stats for pipeline change
    }

    void VulkanRenderAPI::SetComputePipeline(const Ref<ComputePipeline>& pipeline, const Ref<CommandBuffer>& commandBuffer)
    {
        VulkanCmdBuffer* vkCB = GetCB(commandBuffer)->GetInternal();
        vkCB->SetPipeline(pipeline); // TODO: stats
    }

    void VulkanRenderAPI::OnShutdown()
    {
        for (uint32_t i = 0; i < (uint32_t)m_Devices.size(); i++)
            m_Devices[i]->WaitIdle();

        m_CommandBuffer = nullptr;
        VulkanBufferLayoutManager::Shutdown();
        VulkanTextureManager::Shutdown();
        VulkanTransferManager::Shutdown();
        VulkanRenderPasses::Shutdown();
        VulkanGpuBufferManager::Shutdown();

        delete[] m_CurrentCapabilities;
        m_CurrentCapabilities = nullptr;
        m_Devices.clear();
        m_PrimaryDevices.clear();
#ifdef CW_DEBUG
        if (vkDestroyDebugUtilsMessengerEXT != nullptr && m_DebugUtilsMessenger != VK_NULL_HANDLE)
            vkDestroyDebugUtilsMessengerEXT(m_Instance, m_DebugUtilsMessenger, gVulkanAllocator);
        m_DebugUtilsMessenger = VK_NULL_HANDLE;
#endif

        if (m_Instance != VK_NULL_HANDLE)
            vkDestroyInstance(m_Instance, gVulkanAllocator);
        m_Instance = VK_NULL_HANDLE;
        RenderAPI::OnShutdown();
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
            const VulkanOptionalFeatures& optionalFeatures = device->GetOptionalFeatures();
            const VkPhysicalDeviceLimits& devLimits = devProps.limits;

            DriverVersion driverVersion;
            driverVersion.major = ((uint32_t)(devProps.apiVersion) >> 22);
            driverVersion.minor = ((uint32_t)(devProps.apiVersion) >> 12) & 0x3ff;
            driverVersion.release = (uint32_t)(devProps.apiVersion) & 0xfff;
            driverVersion.build = 0;

            caps.DriverVersion = driverVersion;
            caps.DeviceName = devProps.deviceName;
            caps.IntegratedGpu = devProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;

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
            {
                caps.SetCapability(CW_TEXTURE_COMPRESSION_BC);
                caps.SetCapability(CW_TEXTURE_COMPRESSION_BPTC);
            }

            if (devFeatures.textureCompressionETC2)
                caps.SetCapability(CW_TEXTURE_COMPRESSION_ETC2);

            if (devFeatures.textureCompressionASTC_LDR)
                caps.SetCapability(CW_TEXTURE_COMPRESSION_ASTC);

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
            caps.MaxDrawIndirectCount = devLimits.maxDrawIndirectCount;
            caps.MaxStorageBufferRange = devLimits.maxStorageBufferRange;
            caps.MaxBindlessSampledImages = devLimits.maxDescriptorSetSampledImages;

            if (optionalFeatures.MultiDrawIndirect)
                caps.SetCapability(CW_MULTI_DRAW_INDIRECT);
            if (optionalFeatures.DrawIndirectCount)
                caps.SetCapability(CW_DRAW_INDIRECT_COUNT);
            if (optionalFeatures.ShaderDrawParameters)
                caps.SetCapability(CW_SHADER_DRAW_PARAMETERS);
            if (optionalFeatures.DescriptorIndexing)
                caps.SetCapability(CW_DESCRIPTOR_INDEXING);
            if (optionalFeatures.NonUniformTextureIndexing)
                caps.SetCapability(CW_NON_UNIFORM_TEXTURE_INDEXING);
            if (optionalFeatures.UpdateAfterBind)
                caps.SetCapability(CW_UPDATE_AFTER_BIND);
            if (optionalFeatures.BufferDeviceAddress)
                caps.SetCapability(CW_BUFFER_DEVICE_ADDRESS);
            if (optionalFeatures.TimelineSemaphore)
                caps.SetCapability(CW_TIMELINE_SEMAPHORE);
            if (optionalFeatures.Synchronization2)
                caps.SetCapability(CW_SYNCHRONIZATION_2);
            if (optionalFeatures.DynamicRendering)
                caps.SetCapability(CW_DYNAMIC_RENDERING);
            if (optionalFeatures.DedicatedComputeQueue)
                caps.SetCapability(CW_DEDICATED_COMPUTE_QUEUE);
            if (optionalFeatures.DedicatedTransferQueue)
                caps.SetCapability(CW_DEDICATED_TRANSFER_QUEUE);

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

            caps.NumCombinedTextureUnits = caps.NumTextureUnitsPerStage[FRAGMENT_SHADER] + caps.NumTextureUnitsPerStage[VERTEX_SHADER] +
                                           caps.NumTextureUnitsPerStage[GEOMETRY_SHADER] + caps.NumTextureUnitsPerStage[HULL_SHADER] +
                                           caps.NumTextureUnitsPerStage[DOMAIN_SHADER] + caps.NumTextureUnitsPerStage[COMPUTE_SHADER];

            caps.NumCombinedParamBlockBuffers =
              caps.NumGpuParamBlockBuffersPerStage[FRAGMENT_SHADER] + caps.NumGpuParamBlockBuffersPerStage[VERTEX_SHADER] +
              caps.NumGpuParamBlockBuffersPerStage[GEOMETRY_SHADER] + caps.NumGpuParamBlockBuffersPerStage[HULL_SHADER] +
              caps.NumGpuParamBlockBuffersPerStage[DOMAIN_SHADER] + caps.NumGpuParamBlockBuffersPerStage[COMPUTE_SHADER];

            caps.NumCombinedLoadStoreTextureUnits =
              caps.NumLoadStoreTextureUnitsPerStage[FRAGMENT_SHADER] + caps.NumLoadStoreTextureUnitsPerStage[COMPUTE_SHADER];

            caps.AddShaderProfile("glsl");
            deviceIdx++;
        }
    }

    const RenderCapabilities& VulkanRenderAPI::GetCapabilities(uint32_t deviceIndex) const
    {
        CW_ENGINE_ASSERT(m_CurrentCapabilities != nullptr && deviceIndex < m_NumDevices, "Invalid Vulkan capability device index");
        return m_CurrentCapabilities[deviceIndex];
    }

    VulkanRenderAPI& gVulkanRenderAPI() { return static_cast<VulkanRenderAPI&>((*RenderAPI::TryGet())); }
} // namespace Crowny
