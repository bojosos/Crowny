#pragma once

#include "Crowny/Window/RenderWindow.h"
#include "Crowny/Window/Window.h"

#include "Platform/Vulkan/VulkanUtils.h"
#include "Platform/Vulkan/VulkanRendererAPI.h"

#include "Crowny/Window/Window.h"
#include "Crowny/Window/RenderWindow.h"

namespace Crowny
{
    class VulkanRenderWindow : public RenderWindow
    {
    public:
        VulkanRenderWindow(const RenderWindowProperties& props);
        ~VulkanRenderWindow();

        virtual void SwapBuffers(uint32_t syncMask) override;
        void AcquireBackBuffer();
        
        virtual glm::vec2 ScreenToWindowPosition(const glm::vec2& screenPos) override { return glm::vec2(); };
        virtual glm::vec2 WindowToScreenPos(const glm::vec2& windowPos) override { return glm::vec2(); };
        virtual void Resize(uint32_t width, uint32_t height) override {};
        virtual void Hide() override {};
        virtual void Move(float left, float top) override {};
        virtual void Show() override {};
        virtual void Minimize() override {};
        virtual void Maximize() override {};
        virtual void Restore() override {};
        virtual void SetFullScreen(uint32_t width, uint32_t height, float refreshRate = 60.0f, uint32_t monitorIdx = 0) override {};
        virtual void SetWindowed(uint32_t width, uint32_t height) override {};
        virtual Window* GetWindow() const override { return m_Window; }
        
        VulkanSwapChain* GetSwapChain() const { return m_SwapChain; }
        VkFormat GetColorFormat() const { return m_ColorFormat; }
        VkFormat GetDepthFormat() const { return m_DepthFormat; }
        VulkanFramebuffer* GetFramebuffer() const { return m_SwapChain->GetBackBuffer().Framebuffer; }

    private:
        void RebuildSwapChain();
    private:
        Window* m_Window;
        VkSurfaceKHR m_Surface;
        VkColorSpaceKHR m_ColorSpace;
        VkFormat m_ColorFormat;
        VkFormat m_DepthFormat;
        uint32_t m_PresentQueueFamily;
        VulkanSwapChain* m_SwapChain = nullptr;
        VulkanSemaphore* m_SemaphoresTemp[MAX_UNIQUE_QUEUES + 1];
        bool m_RequiresNewBackBuffer;
    };
}
