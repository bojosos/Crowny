#include "cwpch.h"

#include "Crowny/Common/Timer.h"
#include "Platform/Vulkan/VulkanCommandBuffer.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanQueue.h"
#include "Platform/Vulkan/VulkanRenderWindow.h"
#include "Platform/Vulkan/VulkanSwapChain.h"

#include "Crowny/Application/Application.h"

#include <GLFW/glfw3.h>

namespace Crowny
{

    VulkanRenderWindow::~VulkanRenderWindow()
    {
        gVulkanRenderAPI().GetPresentDevice()->WaitIdle();
        m_SwapChain->Destroy();
        vkDestroySurfaceKHR(gVulkanRenderAPI().GetInstance(), m_Surface, gVulkanAllocator);
        delete m_Window;
    }

    VulkanRenderWindow::VulkanRenderWindow(const RenderWindowDesc& renderWindowDesc)
      : RenderWindow(renderWindowDesc), m_RequiresNewBackBuffer(true), m_ShowOnSwap(false), m_Properties(renderWindowDesc)
    {
        WindowDesc windowDesc;
        windowDesc.ShowTitleBar = renderWindowDesc.ShowTitleBar;
        windowDesc.ShowBorder = renderWindowDesc.ShowBorder;
        windowDesc.AllowResize = renderWindowDesc.AllowResize;
        windowDesc.Fullscreen = renderWindowDesc.Fullscreen;
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

        m_ShowOnSwap = renderWindowDesc.HideUntilSwap && !renderWindowDesc.Hidden;
        m_Properties.IsHidden = renderWindowDesc.HideUntilSwap || renderWindowDesc.Hidden;

        m_Window = Window::Create(windowDesc);
        
        const VkResult result = glfwCreateWindowSurface(
          gVulkanRenderAPI().GetInstance(), (GLFWwindow*)m_Window->GetNativeWindow(), gVulkanAllocator, &m_Surface);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);

        Ref<VulkanDevice> device = gVulkanRenderAPI().GetPresentDevice();
        VkPhysicalDevice physicalDevice = device->GetPhysicalDevice();

        m_PresentQueueFamily = device->GetQueueFamily(GRAPHICS_QUEUE);

        VkBool32 supportsPresent;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, m_PresentQueueFamily, m_Surface, &supportsPresent);
        if (!supportsPresent)
            CW_ENGINE_ASSERT(false, "Could not find present queue.");

        SurfaceFormat format = device->GetSurfaceFormat(m_Surface);
        m_ColorFormat = format.ColorFormat;
        m_DepthFormat = format.DepthFormat;
        m_ColorSpace = format.ColorSpace;
        m_Desc.DepthBuffer = false;
        m_SwapChain = device->GetResourceManager().Create<VulkanSwapChain>(
          m_Surface, m_Desc.Width, m_Desc.Height, m_Desc.VSync, m_ColorFormat, m_ColorSpace,
          m_Desc.DepthBuffer, m_DepthFormat);
    }

    void VulkanRenderWindow::SetHidden(bool hidden)
    {
        m_ShowOnSwap = false;
        m_Properties.IsHidden = hidden;
        m_Window->SetHidden(hidden);
    }

    void VulkanRenderWindow::SetVSync(bool enabled)
    {
        m_Properties.VSync = enabled;
        RebuildSwapChain();
    }

    void VulkanRenderWindow::AcquireBackBuffer()
    {
        if (!m_RequiresNewBackBuffer)
            return;
        VkResult result = m_SwapChain->AcquireBackBuffer();
        if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            RebuildSwapChain();
            m_SwapChain->AcquireBackBuffer();
        }
        m_RequiresNewBackBuffer = false;
    }

    void VulkanRenderWindow::SwapBuffers(uint32_t syncMask)
    {
        if (m_ShowOnSwap)
           SetHidden(false);

        Ref<VulkanDevice> device = gVulkanRenderAPI().GetPresentDevice();
        VulkanQueue* queue = device->GetQueue(GRAPHICS_QUEUE, 0);
        uint32_t queueMask = device->GetQueueMask(GRAPHICS_QUEUE, 0);
        syncMask &= ~queueMask;
        uint32_t deviceIdx = device->GetIndex();
        VulkanTransferManager& tbm = VulkanTransferManager::Get();
        uint32_t semaphores;
        tbm.GetSyncSemaphores(syncMask, m_SemaphoresTemp, semaphores);
        const SwapChainSurface& surface = m_SwapChain->GetBackBuffer();
        if (surface.NeedsWait)
        {
            m_SemaphoresTemp[semaphores] = m_SwapChain->GetBackBuffer().Sync;
            semaphores++;
            m_SwapChain->BackBufferWaitIssued();
        }

        VkResult result = queue->Present(m_SwapChain, m_SemaphoresTemp, semaphores);
        if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR)
            RebuildSwapChain();
        m_RequiresNewBackBuffer = true;
    }

    VulkanFramebuffer* VulkanRenderWindow::GetFramebuffer() const { return m_SwapChain->GetBackBuffer().Framebuffer; }

    void VulkanRenderWindow::Resize(uint32_t width, uint32_t height)
    {
        if (!m_Properties.Fullscreen)
        {
            m_Window->Resize(width, height);
            m_Properties.Width = width;
            m_Properties.Height = height;
        }
    }

    void VulkanRenderWindow::Move(int32_t left, int32_t top)
    {
        if (!m_Properties.Fullscreen)
        {
            m_Window->Move(left, top);
            m_Properties.Left = left;
            m_Properties.Top = top;
        }
    }

    void VulkanRenderWindow::Maximize()
    {
        m_Window->Maximize();
    }

    void VulkanRenderWindow::Minimize()
    {
        m_Window->Minimize();
    }

    void VulkanRenderWindow::Restore()
    {
        m_Window->Restore();
    }

    void VulkanRenderWindow::RebuildSwapChain()
    {
        const Ref<VulkanDevice> device = gVulkanRenderAPI().GetPresentDevice();
        gVulkanRenderAPI().SetRenderTarget(nullptr);
        device->WaitIdle();
        VulkanSwapChain* oldSwapChain = m_SwapChain;
        m_SwapChain = device->GetResourceManager().Create<VulkanSwapChain>(
          m_Surface, m_Properties.Width, m_Properties.Height, m_Desc.VSync, m_ColorFormat, m_ColorSpace,
          m_Desc.DepthBuffer, m_DepthFormat, oldSwapChain);
        oldSwapChain->Destroy();
    }

} // namespace Crowny