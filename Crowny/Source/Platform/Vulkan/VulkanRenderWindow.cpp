#include "cwpch.h"

#include "Crowny/Common/Timer.h"
#include "Platform/Vulkan/VulkanCommandBuffer.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanQueue.h"
#include "Platform/Vulkan/VulkanRenderWindow.h"
#include "Platform/Vulkan/VulkanSwapChain.h"

#include "Crowny/Application/Application.h"

#include <GLFW/glfw3.h>
#include <stdexcept>

namespace Crowny
{
    namespace
    {
        struct SurfaceConstructionGuard
        {
            VkInstance Instance;
            VkSurfaceKHR Surface;

            ~SurfaceConstructionGuard()
            {
                if (Surface != VK_NULL_HANDLE)
                    vkDestroySurfaceKHR(Instance, Surface, gVulkanAllocator);
            }
        };
    } // namespace

    VulkanRenderWindow::~VulkanRenderWindow()
    {
        if (const Ref<VulkanDevice> device = gVulkanRenderAPI().GetPresentDevice())
            device->WaitIdle();
        if (m_SwapChain != nullptr)
        {
            m_SwapChain->Destroy();
            m_SwapChain = nullptr;
        }
        if (m_Surface != VK_NULL_HANDLE)
        {
            vkDestroySurfaceKHR(gVulkanRenderAPI().GetInstance(), m_Surface, gVulkanAllocator);
            m_Surface = VK_NULL_HANDLE;
        }
    }

    VulkanRenderWindow::VulkanRenderWindow(const RenderWindowDesc& renderWindowDesc)
      : RenderWindow(renderWindowDesc), m_RequiresNewBackBuffer(true), m_ShowOnSwap(false), m_Properties(renderWindowDesc)
    {
        WindowDesc windowDesc;
        windowDesc.ShowTitleBar = renderWindowDesc.ShowTitleBar;
        windowDesc.ShowBorder = renderWindowDesc.ShowBorder;
        windowDesc.AllowResize = renderWindowDesc.AllowResize;
        windowDesc.Fullscreen = renderWindowDesc.Fullscreen;
        windowDesc.Mode = renderWindowDesc.Mode;
        windowDesc.Width = renderWindowDesc.Width;
        windowDesc.Height = renderWindowDesc.Height;
        windowDesc.Hidden = renderWindowDesc.Hidden || renderWindowDesc.HideUntilSwap;
        windowDesc.Left = renderWindowDesc.Left;
        windowDesc.Top = renderWindowDesc.Top;
        windowDesc.Title = renderWindowDesc.Title;
        windowDesc.Modal = renderWindowDesc.Modal;
        windowDesc.MonitorIdx = renderWindowDesc.MonitorIdx;
        // Could do this here without glfw
        windowDesc.StartMaximized = renderWindowDesc.StartMaximized;
        windowDesc.MinWidth = renderWindowDesc.MinWidth;
        windowDesc.MinHeight = renderWindowDesc.MinHeight;
        windowDesc.MaxWidth = renderWindowDesc.MaxWidth;
        windowDesc.MaxHeight = renderWindowDesc.MaxHeight;
        windowDesc.AspectRatioNumerator = renderWindowDesc.AspectRatioNumerator;
        windowDesc.AspectRatioDenominator = renderWindowDesc.AspectRatioDenominator;

        m_ShowOnSwap = renderWindowDesc.HideUntilSwap && !renderWindowDesc.Hidden;
        m_Properties.IsHidden = renderWindowDesc.HideUntilSwap || renderWindowDesc.Hidden;

        m_Window = Window::Create(windowDesc);
        if (!m_Window)
            throw std::runtime_error("Could not create the native render window");

        const VkResult result =
          glfwCreateWindowSurface(gVulkanRenderAPI().GetInstance(), (GLFWwindow*)m_Window->GetNativeWindow(), gVulkanAllocator, &m_Surface);
        if (result != VK_SUCCESS)
            throw std::runtime_error("Could not create the Vulkan window surface");
        SurfaceConstructionGuard surfaceGuard{ gVulkanRenderAPI().GetInstance(), m_Surface };

        Ref<VulkanDevice> device = gVulkanRenderAPI().GetPresentDevice();
        VkPhysicalDevice physicalDevice = device->GetPhysicalDevice();

        m_PresentQueueFamily = device->GetQueueFamily(GRAPHICS_QUEUE);

        VkBool32 supportsPresent;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, m_PresentQueueFamily, m_Surface, &supportsPresent);
        if (!supportsPresent)
            throw std::runtime_error("The selected Vulkan queue cannot present to the window surface");

        SurfaceFormat format = device->GetSurfaceFormat(m_Surface);
        m_ColorFormat = format.ColorFormat;
        m_DepthFormat = format.DepthFormat;
        m_ColorSpace = format.ColorSpace;
        const uint32_t framebufferWidth = std::max(1U, m_Window->GetFramebufferWidth());
        const uint32_t framebufferHeight = std::max(1U, m_Window->GetFramebufferHeight());
        m_Properties.Width = framebufferWidth;
        m_Properties.Height = framebufferHeight;
        m_SwapChain = device->GetResourceManager().Create<VulkanSwapChain>(m_Surface, framebufferWidth, framebufferHeight, m_Desc.VSync,
                                                                           m_ColorFormat, m_ColorSpace, m_Desc.DepthBuffer, m_DepthFormat);
        surfaceGuard.Surface = VK_NULL_HANDLE;
    }

    void VulkanRenderWindow::SetHidden(bool hidden)
    {
        m_ShowOnSwap = false;
        m_Properties.IsHidden = hidden;
        m_Window->SetHidden(hidden);
    }

    void VulkanRenderWindow::SetVSync(bool enabled)
    {
        if (m_Properties.VSync == enabled)
            return;

        m_Properties.VSync = enabled;
        m_Desc.VSync = enabled;
        m_SwapChainDirty = true;
    }

    void VulkanRenderWindow::AcquireBackBuffer()
    {
        if (!m_RequiresNewBackBuffer)
            return;

        if (m_SwapChainDirty && !RebuildSwapChain())
            return;

        VkResult result = m_SwapChain->AcquireBackBuffer();
        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            m_SwapChainDirty = true;
            if (!RebuildSwapChain())
                return;
            result = m_SwapChain->AcquireBackBuffer();
        }

        if (result == VK_SUCCESS)
            m_RequiresNewBackBuffer = false;
        else if (result == VK_SUBOPTIMAL_KHR)
        {
            m_SwapChainDirty = true;
            m_RequiresNewBackBuffer = false;
        }
        else
            CW_ENGINE_ERROR("Failed to acquire Vulkan swap-chain image: {}", (int32_t)result);
    }

    void VulkanRenderWindow::SwapBuffers(uint32_t syncMask)
    {
        if (m_ShowOnSwap)
            SetHidden(false);
        if (m_RequiresNewBackBuffer)
            return;

        Ref<VulkanDevice> device = gVulkanRenderAPI().GetPresentDevice();
        VulkanQueue* queue = device->GetQueue(GRAPHICS_QUEUE, 0);
        uint32_t queueMask = device->GetQueueMask(GRAPHICS_QUEUE, 0);
        syncMask &= ~queueMask;
        VulkanTransferManager& tbm = VulkanTransferManager::Get();
        uint32_t semaphores;
        tbm.GetSyncSemaphores(syncMask, m_SemaphoresTemp, semaphores);
        const SwapChainSurface& surface = m_SwapChain->GetBackBuffer();
        if (surface.NeedsWait)
        {
            m_SemaphoresTemp[semaphores] = surface.Sync;
            semaphores++;
            m_SwapChain->BackBufferWaitIssued();
        }

        const VkResult result = queue->Present(m_SwapChain, m_SemaphoresTemp, semaphores);
        if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR)
            m_SwapChainDirty = true;
        else if (result != VK_SUCCESS)
            CW_ENGINE_ERROR("Failed to present Vulkan swap-chain image: {}", (int32_t)result);
        m_RequiresNewBackBuffer = true;
    }

    VulkanFramebuffer* VulkanRenderWindow::GetFramebuffer() const { return m_SwapChain->GetBackBuffer().Framebuffer; }

    glm::vec2 VulkanRenderWindow::ScreenToWindowPosition(const glm::vec2& screenPos) { return m_Window->ScreenToWindowPosition(screenPos); }

    glm::vec2 VulkanRenderWindow::WindowToScreenPosition(const glm::vec2& windowPos) { return m_Window->WindowToScreenPosition(windowPos); }

    void VulkanRenderWindow::Resize(uint32_t width, uint32_t height)
    {
        if (m_Window->GetMode() == WindowMode::Windowed)
        {
            m_Window->Resize(width, height);
            m_Properties.Width = width;
            m_Properties.Height = height;
            m_Desc.Width = width;
            m_Desc.Height = height;
            m_SwapChainDirty = true;
        }
    }

    void VulkanRenderWindow::Move(int32_t left, int32_t top)
    {
        if (m_Window->GetMode() == WindowMode::Windowed)
        {
            m_Window->Move(left, top);
            m_Properties.Left = left;
            m_Properties.Top = top;
        }
    }

    void VulkanRenderWindow::Maximize()
    {
        m_Window->Maximize();
        SyncWindowProperties();
    }

    void VulkanRenderWindow::Minimize()
    {
        m_Window->Minimize();
        SyncWindowProperties();
    }

    void VulkanRenderWindow::Restore()
    {
        m_Window->Restore();
        SyncWindowProperties();
    }

    void VulkanRenderWindow::SetFullscreen(uint32_t width, uint32_t height, float refreshRate, uint32_t monitorIdx)
    {
        m_Window->SetFullscreen(width, height, static_cast<uint32_t>(std::max(0.0f, refreshRate)), monitorIdx);
        m_Desc.Mode = WindowMode::Fullscreen;
        m_Desc.Fullscreen = true;
        SyncWindowProperties();
        m_SwapChainDirty = true;
    }

    void VulkanRenderWindow::SetBorderlessFullscreen(uint32_t monitorIdx)
    {
        m_Window->SetBorderlessFullscreen(monitorIdx);
        m_Desc.Mode = WindowMode::BorderlessFullscreen;
        m_Desc.Fullscreen = true;
        SyncWindowProperties();
        m_SwapChainDirty = true;
    }

    void VulkanRenderWindow::SetWindowed(uint32_t width, uint32_t height)
    {
        m_Window->SetWindowed(width, height);
        m_Desc.Mode = WindowMode::Windowed;
        m_Desc.Fullscreen = false;
        SyncWindowProperties();
        m_SwapChainDirty = true;
    }

    void VulkanRenderWindow::SyncWindowProperties() const
    {
        m_Properties.Width = m_Window->GetFramebufferWidth();
        m_Properties.Height = m_Window->GetFramebufferHeight();
        const glm::ivec2 position = m_Window->GetPosition();
        m_Properties.Left = position.x;
        m_Properties.Top = position.y;
        m_Properties.Mode = m_Window->GetMode();
        m_Properties.Fullscreen = m_Properties.Mode != WindowMode::Windowed;
        m_Properties.IsFocused = m_Window->IsFocused();
        m_Properties.IsHidden = m_Window->IsHidden();
        m_Properties.IsMinimized = m_Window->IsMinimized();
        m_Properties.IsMaximized = m_Window->IsMaximized();
    }

    const RenderTargetProperties& VulkanRenderWindow::GetProperties() const
    {
        SyncWindowProperties();
        return m_Properties;
    }

    bool VulkanRenderWindow::RebuildSwapChain()
    {
        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(static_cast<GLFWwindow*>(m_Window->GetNativeWindow()), &width, &height);
        if (width <= 0 || height <= 0)
            return false;

        const Ref<VulkanDevice> device = gVulkanRenderAPI().GetPresentDevice();
        gVulkanRenderAPI().SetRenderTarget(nullptr);
        device->WaitIdle();

        const SurfaceFormat format = device->GetSurfaceFormat(m_Surface);
        m_ColorFormat = format.ColorFormat;
        m_DepthFormat = format.DepthFormat;
        m_ColorSpace = format.ColorSpace;
        m_Properties.Width = (uint32_t)width;
        m_Properties.Height = (uint32_t)height;

        VulkanSwapChain* oldSwapChain = m_SwapChain;
        m_SwapChain =
          device->GetResourceManager().Create<VulkanSwapChain>(m_Surface, m_Properties.Width, m_Properties.Height, m_Properties.VSync, m_ColorFormat,
                                                               m_ColorSpace, m_Desc.DepthBuffer, m_DepthFormat, oldSwapChain);
        oldSwapChain->Destroy();
        m_SwapChainDirty = false;
        return true;
    }

} // namespace Crowny
