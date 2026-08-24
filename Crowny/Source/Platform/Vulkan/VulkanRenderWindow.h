#pragma once

#include "Crowny/Window/RenderWindow.h"
#include "Crowny/Window/Window.h"

#include "Platform/Vulkan/VulkanRenderAPI.h"
#include "Platform/Vulkan/VulkanUtils.h"

namespace Crowny
{
    class VulkanRenderWindow : public RenderWindow
    {
    public:
        friend class RenderWindow;
        ~VulkanRenderWindow();

        virtual void SwapBuffers(uint32_t syncMask) override;
        void AcquireBackBuffer();

        virtual glm::vec2 ScreenToWindowPosition(const glm::vec2& screenPos) override;
        virtual glm::vec2 WindowToScreenPosition(const glm::vec2& windowPos) override;
        virtual void Resize(uint32_t width, uint32_t height) override;
        virtual void Move(int32_t left, int32_t top) override;
        virtual void Minimize() override;
        virtual void Maximize() override;
        virtual void Restore() override;
        virtual void SetFullscreen(uint32_t width, uint32_t height, float refreshRate = 60.0f, uint32_t monitorIdx = 0) override;
        virtual void SetBorderlessFullscreen(uint32_t monitorIdx = 0) override;
        virtual void SetWindowed(uint32_t width, uint32_t height) override;
        virtual Window* GetWindow() const override { return m_Window.get(); }
        virtual void SetHidden(bool hidden) override;
        virtual void SetVSync(bool enabled) override;
        virtual const RenderTargetProperties& GetProperties() const override;

        VulkanSwapChain* GetSwapChain() const { return m_SwapChain; }
        VkFormat GetColorFormat() const { return m_ColorFormat; }
        VkFormat GetDepthFormat() const { return m_DepthFormat; }
        VulkanFramebuffer* GetFramebuffer() const;

    protected:
        VulkanRenderWindow(const RenderWindowDesc& renderWindowDesc);

    private:
        bool RebuildSwapChain();
        void SyncWindowProperties() const;

    private:
        Scope<Window> m_Window;
        VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
        VkColorSpaceKHR m_ColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        VkFormat m_ColorFormat = VK_FORMAT_UNDEFINED;
        VkFormat m_DepthFormat = VK_FORMAT_UNDEFINED;
        uint32_t m_PresentQueueFamily = 0;
        VulkanSwapChain* m_SwapChain = nullptr;
        VulkanSemaphore* m_SemaphoresTemp[MAX_UNIQUE_QUEUES + 1] = {};
        bool m_RequiresNewBackBuffer;
        bool m_ShowOnSwap;
        bool m_SwapChainDirty = false;
        mutable RenderWindowProperties m_Properties;
    };
} // namespace Crowny
