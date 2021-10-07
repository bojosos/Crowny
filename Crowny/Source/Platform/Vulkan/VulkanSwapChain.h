#pragma once

#include "Platform/Vulkan/VulkanUtils.h"

#include "Platform/Vulkan/VulkanFramebuffer.h"
#include "Platform/Vulkan/VulkanResource.h"
#include "Platform/Vulkan/VulkanTexture.h"

namespace Crowny
{

    struct SwapChainSurface
    {
        VulkanFramebuffer* Framebuffer;
        VulkanImage* Image;
        VulkanSemaphore* Sync;
        bool Acquired;
        bool NeedsWait;
    };

    // TODO: Prevent copying
    class VulkanSwapChain : public VulkanResource
    {
    public:
        VulkanSwapChain(VulkanResourceManager* owner, VkSurfaceKHR surface, uint32_t width, uint32_t height, bool vsync,
                        VkFormat colorFormat, VkColorSpaceKHR colorSpace, bool createDepth, VkFormat depthFormat,
                        VulkanSwapChain* oldChain = nullptr);
        ~VulkanSwapChain();

        bool PrepareForPresent(uint32_t& backBufferIdx);
        VkResult AcquireBackBuffer();
        void BackBufferWaitIssued();

        uint32_t GetWidth() const { return m_Width; }
        uint32_t GetHeight() const { return m_Height; }
        const SwapChainSurface& GetBackBuffer() const { return m_Surfaces[m_CurrentBackBufferIdx]; }
        uint32_t GetColorSurfacesCount() const { return (uint32_t)m_Surfaces.size(); }
        VkSwapchainKHR GetHandle() const { return m_SwapChain; }

    private:
        void Clear(VkSwapchainKHR swapChain);

    private:
        Vector<VkFramebuffer> m_Framebuffers;
        VkDevice m_Device = VK_NULL_HANDLE;
        VkSwapchainKHR m_SwapChain = VK_NULL_HANDLE;
        uint32_t m_Width, m_Height;
        Vector<SwapChainSurface> m_Surfaces;
        VulkanImage* m_DepthStencilImage = nullptr;

        uint32_t m_CurrentSemaphoreIdx = 0;
        uint32_t m_CurrentBackBufferIdx = 0;
    };

} // namespace Crowny