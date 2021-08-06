#include "cwpch.h"

#include "Crowny/Common/Timer.h"
#include "Platform/Vulkan/VulkanRenderWindow.h"

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

    VulkanRenderWindow::VulkanRenderWindow(const RenderWindowProperties& props)
      : RenderWindow(props), m_RequiresNewBackBuffer(true)
    {
        m_Properties.SwapChainTarget = true;
        WindowProperties windowProps;
        windowProps.Title = props.Title;
        windowProps.Width = props.Width;
        windowProps.Height = props.Height;
        m_Window = Window::Create(windowProps);

        VkResult result = glfwCreateWindowSurface(
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
        m_SwapChain = device->GetResourceManager().Create<VulkanSwapChain>(
          m_Surface, m_Properties.Width, m_Properties.Height, m_Properties.Vsync, m_ColorFormat, m_ColorSpace,
          m_Properties.DepthBuffer, m_DepthFormat);
        if (props.Fullscreen)
        {
        }
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
        // if (m_ShowOnSwap)
        //   SetHidden(false);

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

    void VulkanRenderWindow::RebuildSwapChain()
    {
        Ref<VulkanDevice> device = gVulkanRenderAPI().GetPresentDevice();
        device->WaitIdle();
        VulkanSwapChain* oldSwapChain = m_SwapChain;
        m_SwapChain = device->GetResourceManager().Create<VulkanSwapChain>(
          m_Surface, m_Properties.Width, m_Properties.Height, m_Properties.Vsync, m_ColorFormat, m_ColorSpace,
          m_Properties.DepthBuffer, m_DepthFormat);
        oldSwapChain->Destroy();
    }

} // namespace Crowny