#pragma once

#include "Platform/Vulkan/VulkanUtils.h"

#include "Crowny/Renderer/RendererAPI.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanCommandBuffer.h"

namespace Crowny
{
    class VulkanRendererAPI : public RendererAPI
    {
    public:
        virtual void Init() override;
        virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;

        virtual void SetClearColor(const glm::vec4& color) override;
        virtual void SetDepthTest(bool value) override;
        virtual void SwapBuffers() override;
        virtual void Clear() override;

        virtual void SetGraphicsPipeline(const Ref<GraphicsPipeline>& pipeline) override;
        void SetComputePipeline(const Ref<ComputePipeline>& pipeline); // TODO: Make virtual
        virtual void DispatchCompute(uint32_t groupsX, uint32_t groupsY = 1, uint32_t groupsZ = 1);
        virtual void SetIndexBuffer(const Ref<IndexBuffer>& buffer) override;
        virtual void SetVertexBuffer(uint32_t idx, Ref<VertexBuffer>* buffers, uint32_t bufferCount) override;
        virtual void DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount) override;
        virtual void SetRenderTarget(const Ref<Framebuffer>& target) override;
        
        virtual void Shutdown() override;

        VkInstance GetInstance() const { return m_Instance; }
        VulkanCommandBuffer* GetMainCommandBuffer() const { return m_CommandBuffer; }
        const std::vector<Ref<VulkanDevice>>& GetPrimaryDevices() const { return m_PrimaryDevices; }
        uint32_t GetDeviceCount() const { return (uint32_t)m_Devices.size(); }
        Ref<VulkanDevice> GetDevice(uint32_t idx) const { return m_Devices[idx]; }
    private:
        void InitCaps();
    
    private:
        VkSurfaceKHR m_Surface;
        VkInstance m_Instance = nullptr;
        std::vector<Ref<VulkanDevice>> m_Devices;
        std::vector<Ref<VulkanDevice>> m_PrimaryDevices;
        RenderCapabilities* m_CurrentCapabilities;
        Ref<VukanCommandBuffer> m_CommandBuffer;
#ifdef CW_DEBUG
        VkDebugReportCallbackEXT m_DebugReportCallback;
#endif

        uint32_t m_NumDevices;
    };
    
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