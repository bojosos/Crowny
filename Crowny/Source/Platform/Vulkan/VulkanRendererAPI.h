#pragma once

#include "Platform/Vulkan/VulkanUtils.h"

#include "Crowny/Renderer/RendererAPI.h"
#include "Crowny/Renderer/CommandBuffer.h"
#include "Crowny/Renderer/RenderCapabilities.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanCommandBuffer.h"
#include "Platform/Vulkan/VulkanSwapChain.h"

namespace Crowny
{
    class VulkanRendererAPI : public RendererAPI
    {
    public:
        virtual void Init() override;
        virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height, const Ref<CommandBuffer>& commandBuffer = nullptr) override;

        virtual void SetClearColor(const glm::vec4& color) override {};
        virtual void SetDepthTest(bool value, const Ref<CommandBuffer>& commandBuffer = nullptr) override {};
        virtual void SwapBuffers(const Ref<RenderTarget>& renderTarget, uint32_t syncMask = 0xFFFFFFFF) override;
        

        virtual void SubmitCommandBuffer(const Ref<CommandBuffer>& commandBuffer, uint32_t syncMask = 0xFFFFFFFF) override;

        virtual void SetGraphicsPipeline(const Ref<GraphicsPipeline>& pipeline, const Ref<CommandBuffer>& commandBuffer = nullptr) override;
        virtual void SetComputePipeline(const Ref<ComputePipeline>& pipeline, const Ref<CommandBuffer>& commandBuffer = nullptr) override;
        virtual void ClearViewport(uint32_t buffers, const glm::vec4& color = glm::vec4(0.0f), float depth = 1.0f, uint16_t stencil = 0, uint8_t targetMask = 0xFF, const Ref<CommandBuffer>& commandBuffer = nullptr) override;
        virtual void ClearRenderTarget(uint32_t buffers, const glm::vec4& color = glm::vec4(0.0f), float depth = 1.0f, uint16_t stencil = 0, uint8_t targetMask = 0xFF, const Ref<CommandBuffer>& commandBuffer = nullptr) override;
        virtual void DispatchCompute(uint32_t groupsX, uint32_t groupsY = 1, uint32_t groupsZ = 1, const Ref<CommandBuffer>& commandBuffer = nullptr) override;
        virtual void SetIndexBuffer(const Ref<IndexBuffer>& buffer, const Ref<CommandBuffer>& commandBuffer = nullptr) override;
        virtual void SetVertexBuffers(uint32_t idx, Ref<VertexBuffer>* buffers, uint32_t bufferCount, const Ref<CommandBuffer>& commandBuffer = nullptr) override;
        virtual void Draw(uint32_t vertexOffset, uint32_t vertexCount, uint32_t instanceCount = 0, const Ref<CommandBuffer>& commandBuffer = nullptr) override;
        virtual void DrawIndexed(uint32_t startIndex, uint32_t indexCount, uint32_t vertexOffset, uint32_t vertexCount, uint32_t instanceCount = 0, const Ref<CommandBuffer>& commandBuffer = nullptr) override;
        virtual void SetRenderTarget(const Ref<RenderTarget>& target, uint32_t readOnlyFlags = 0, RenderSurfaceMask loadMask = RT_NONE, const Ref<CommandBuffer>& commandBuffer = nullptr) override;
        virtual void SetDrawMode(DrawMode drawMode, const Ref<CommandBuffer>& commandBuffer = nullptr) override;
        virtual void SetUniforms(const Ref<UniformParams>& params, const Ref<CommandBuffer>& commandBuffer = nullptr) override;

        virtual void Shutdown() override;

        VkInstance GetInstance() const { return m_Instance; }
        VulkanCommandBuffer* GetMainCommandBuffer() const { return m_CommandBuffer.get(); }
        const std::vector<Ref<VulkanDevice>>& GetPrimaryDevices() const { return m_PrimaryDevices; }
        const Ref<VulkanDevice>& GetPresentDevice() const { return m_PrimaryDevices[0]; }
        uint32_t GetDeviceCount() const { return (uint32_t)m_Devices.size(); }
        Ref<VulkanDevice> GetDevice(uint32_t idx) const { return m_Devices[idx]; }
        bool IsReadyForRender() const;
    private:
        VulkanCommandBuffer* GetCB(const Ref<CommandBuffer>& buffer);
        void RebuildSwapChain();
        void InitCaps();
    
    private:
        VkDebugUtilsMessengerEXT m_DebugUtilsMessenger;
        VkInstance m_Instance = nullptr;
        std::vector<Ref<VulkanDevice>> m_Devices;
        std::vector<Ref<VulkanDevice>> m_PrimaryDevices;
        RenderCapabilities* m_CurrentCapabilities;
        Ref<VulkanCmdBuffer> m_CmdBuffer;
        Ref<VulkanCommandBuffer> m_CommandBuffer;
        VulkanSwapChain* m_SwapChain;
        Ref<GraphicsPipeline> m_Pipeline;
#ifdef CW_DEBUG
        VkDebugReportCallbackEXT m_DebugReportCallback;
#endif

        uint32_t m_NumDevices;
    };

    VulkanRendererAPI& gVulkanRendererAPI();
    
    extern PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR;
	extern PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR;
	extern PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
	extern PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR;

	extern PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR;
	extern PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR;
	extern PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR;
	extern PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR;
	extern PFN_vkQueuePresentKHR vkQueuePresentKHR;
}