#pragma once

#include "Platform/Vulkan/VulkanUtils.h"

#include "Crowny/RenderAPI/RenderAPI.h"
#include "Crowny/RenderAPI/RenderCapabilities.h"

namespace Crowny
{
    class VulkanCmdBuffer;

    class VulkanRenderAPI : public RenderAPI
    {
    public:
        VulkanRenderAPI() : RenderAPI(API::Vulkan) {}

        virtual void Init() override;
        virtual const RenderCapabilities& GetCapabilities(uint32_t deviceIndex = 0) const override;
        virtual void SetViewport(float x, float y, float width, float height, const Ref<CommandBuffer>& commandBuffer = nullptr) override;
        virtual void SetScissorRect(const Rect2I& rect, const Ref<CommandBuffer>& commandBuffer) override;

        virtual void SetClearColor(const glm::vec4& color) override {};
        virtual void SwapBuffers(const Ref<RenderTarget>& renderTarget, uint32_t syncMask = 0xFFFFFFFF) override;

        virtual void SubmitCommandBuffer(const Ref<CommandBuffer>& commandBuffer, uint32_t syncMask = 0xFFFFFFFF) override;

        virtual void SetGraphicsPipeline(const Ref<GraphicsPipeline>& pipeline, const Ref<CommandBuffer>& commandBuffer = nullptr) override;
        virtual void SetRayTracingPipeline(const Ref<RayTracingPipeline>& pipeline, const Ref<CommandBuffer>& commandBuffer = nullptr) override;
        virtual void SetComputePipeline(const Ref<ComputePipeline>& pipeline, const Ref<CommandBuffer>& commandBuffer = nullptr) override;
        virtual void ClearViewport(uint32_t buffers, const glm::vec4& color = glm::vec4(0.0f), float depth = 1.0f, uint16_t stencil = 0,
                                   uint8_t targetMask = 0xFF, const Ref<CommandBuffer>& commandBuffer = nullptr) override;
        virtual void ClearRenderTarget(uint32_t buffers, const glm::vec4& color = glm::vec4(0.0f), float depth = 1.0f, uint16_t stencil = 0,
                                       uint8_t targetMask = 0xFF, const Ref<CommandBuffer>& commandBuffer = nullptr) override;
        virtual void SetIndexBuffer(const Ref<IndexBuffer>& buffer, const Ref<CommandBuffer>& commandBuffer = nullptr) override;
        virtual void SetVertexBuffers(uint32_t idx, Ref<VertexBuffer>* buffers, uint32_t bufferCount,
                                      const Ref<CommandBuffer>& commandBuffer = nullptr) override;
        virtual void SetVertexLayout(const Ref<BufferLayout>& vertexLayout, const Ref<CommandBuffer>& commandBuffer = nullptr) override;
        virtual void Draw(uint32_t vertexOffset, uint32_t vertexCount, uint32_t instanceCount = 1,
                          const Ref<CommandBuffer>& commandBuffer = nullptr) override;
        virtual void DrawIndexed(uint32_t startIndex, uint32_t indexCount, uint32_t vertexOffset, uint32_t vertexCount, uint32_t instanceCount = 1,
                                 const Ref<CommandBuffer>& commandBuffer = nullptr) override;
        virtual void DrawIndexedIndirect(const Ref<GenericGpuBuffer>& argumentBuffer, uint32_t argumentOffset, uint32_t drawCount,
                                         uint32_t stride = sizeof(DrawIndexedIndirectCommand),
                                         const Ref<CommandBuffer>& commandBuffer = nullptr) override;
        virtual void DrawIndexedIndirectCount(const Ref<GenericGpuBuffer>& argumentBuffer, uint32_t argumentOffset,
                                              const Ref<GenericGpuBuffer>& countBuffer, uint32_t countOffset, uint32_t maxDrawCount,
                                              uint32_t stride = sizeof(DrawIndexedIndirectCommand),
                                              const Ref<CommandBuffer>& commandBuffer = nullptr) override;
        virtual void TraceRays(uint32_t width, uint32_t height, const Ref<CommandBuffer>& commandBuffer = nullptr) override;
        virtual void DispatchCompute(uint32_t groupsX, uint32_t groupsY = 1, uint32_t groupsZ = 1,
                                     const Ref<CommandBuffer>& commandBuffer = nullptr) override;
        virtual void DispatchComputeIndirect(const Ref<GenericGpuBuffer>& argumentBuffer, uint32_t argumentOffset,
                                             const Ref<CommandBuffer>& commandBuffer = nullptr) override;
        virtual void SetRenderTarget(const Ref<RenderTarget>& target, uint32_t readOnlyFlags = 0, RenderSurfaceMask loadMask = RT_NONE,
                                     const Ref<CommandBuffer>& commandBuffer = nullptr) override;
        virtual void SetDrawMode(DrawMode drawMode, const Ref<CommandBuffer>& commandBuffer = nullptr) override;
        virtual void SetUniforms(const Ref<UniformParams>& params, const Ref<CommandBuffer>& commandBuffer = nullptr) override;

        virtual void OnShutdown() override;

        VkInstance GetInstance() const { return m_Instance; }
        VulkanCommandBuffer* GetMainCommandBuffer() const { return m_CommandBuffer.Get(); }
        const Vector<Ref<VulkanDevice>>& GetPrimaryDevices() const { return m_PrimaryDevices; }
        const Ref<VulkanDevice>& GetPresentDevice() const { return m_PrimaryDevices[0]; }
        uint32_t GetDeviceCount() const { return (uint32_t)m_Devices.size(); }
        Ref<VulkanDevice> GetDevice(uint32_t idx) const { return m_Devices[idx]; }
        bool IsReadyForRender() const; // TODO:

    private:
        VulkanCommandBuffer* GetCB(const Ref<CommandBuffer>& buffer);
        void RebuildSwapChain(); // TODO:
        void InitCaps();

    private:
        VkDebugUtilsMessengerEXT m_DebugUtilsMessenger = VK_NULL_HANDLE;
        VkInstance m_Instance = VK_NULL_HANDLE;
        Vector<Ref<VulkanDevice>> m_Devices;
        Vector<Ref<VulkanDevice>> m_PrimaryDevices;
        RenderCapabilities* m_CurrentCapabilities = nullptr;
        Ref<VulkanCmdBuffer> m_CmdBuffer;
        Ref<VulkanCommandBuffer> m_CommandBuffer;
        VulkanSwapChain* m_SwapChain = nullptr;
        Ref<GraphicsPipeline> m_Pipeline;
        DrawMode m_DrawMode = DrawMode::TRIANGLE_LIST;
#ifdef CW_DEBUG
        VkDebugReportCallbackEXT m_DebugReportCallback;
#endif

        uint32_t m_NumDevices = 0;
    };

    VulkanRenderAPI& gVulkanRenderAPI();

    extern PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT;

    extern PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR;
    extern PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR;
    extern PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
    extern PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR;

    extern PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR;
    extern PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR;
    extern PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR;
    extern PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR;
    extern PFN_vkQueuePresentKHR vkQueuePresentKHR;

    // The ray tracing.
    extern PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR;
    extern PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
    extern PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;
    extern PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
    extern PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
    extern PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
    extern PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;
    extern PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR;
    extern PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR;
} // namespace Crowny
