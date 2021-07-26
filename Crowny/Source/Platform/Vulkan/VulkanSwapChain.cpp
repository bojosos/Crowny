#include "cwpch.h"

#include "Platform/Vulkan/VulkanSwapChain.h"
#include "Platform/Vulkan/VulkanRendererAPI.h"

#include "Crowny/Application/Application.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>

#include <glm/glm.hpp>

namespace Crowny
{
    
    VulkanSwapChain::VulkanSwapChain(VulkanResourceManager* owner, VkSurfaceKHR surface, uint32_t width, uint32_t height, bool vsync, VkFormat colorFormat,VkColorSpaceKHR colorSpace, bool createDepth, VkFormat depthFormat, VulkanSwapChain* oldChain)
        : VulkanResource(owner, false)
    {
        VulkanDevice& device = owner->GetDevice();
        m_Device = device.GetLogicalDevice();
        VkPhysicalDevice physicalDevice = device.GetPhysicalDevice();
        VkSurfaceCapabilitiesKHR surfCaps;
        VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfCaps);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);

        uint32_t presentModeCount;
        result = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
        CW_ENGINE_ASSERT(result == VK_SUCCESS && presentModeCount > 0);

        std::vector<VkPresentModeKHR> presentModes(presentModeCount);
        result = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data());
        CW_ENGINE_ASSERT(result == VK_SUCCESS);

        VkExtent2D swapChainExtent{};
        if (surfCaps.currentExtent.width == (uint32_t)-1 || surfCaps.currentExtent.height == (uint32_t)-1)
        {
            swapChainExtent.width = glm::clamp(width, surfCaps.minImageExtent.width, surfCaps.maxImageExtent.width);
            swapChainExtent.height = glm::clamp(height, surfCaps.minImageExtent.height, surfCaps.maxImageExtent.height);
        }
        else
        {
            swapChainExtent = surfCaps.currentExtent;
        }
        
        m_Width = swapChainExtent.width;
        m_Height = swapChainExtent.height;

        VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;

        if (!vsync)
        {
            for (size_t i = 0; i < presentModeCount; i++)
            {
                if (presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)
                {
                    presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
                    break;
                }
                else if (presentModes[i] == VK_PRESENT_MODE_FIFO_RELAXED_KHR)
                    presentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
            }
        }
        else
        {
            //TODO: prob should not do this one mobile
            for (uint32_t i = 0; i < presentModeCount; i++)
            {
                if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
                {
                    presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
                    break;
                }
            }
        }
        
        
        uint32_t numOfImages = surfCaps.minImageCount;
        
        VkSurfaceTransformFlagsKHR preTransform;
        if (surfCaps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
            preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        else
            preTransform = surfCaps.currentTransform;

        VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        std::vector<VkCompositeAlphaFlagBitsKHR> compositeAlphaFlags = {
            VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		    VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
		    VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
		    VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
        };

        for (auto& flag : compositeAlphaFlags)
        {
            if (surfCaps.supportedCompositeAlpha & flag)
            {
                compositeAlpha = flag;
                break;
            }
        }

        VkSwapchainCreateInfoKHR swapchainCreateInfo{};
        swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchainCreateInfo.pNext = nullptr;
        swapchainCreateInfo.flags = 0;
        swapchainCreateInfo.surface = surface;
        swapchainCreateInfo.minImageCount = numOfImages;
        swapchainCreateInfo.imageFormat = colorFormat;
        swapchainCreateInfo.imageColorSpace = colorSpace;
        swapchainCreateInfo.imageExtent = { swapChainExtent.width, swapChainExtent.height };
        swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        swapchainCreateInfo.preTransform = (VkSurfaceTransformFlagBitsKHR)preTransform;
        swapchainCreateInfo.imageArrayLayers = 1;
        swapchainCreateInfo.queueFamilyIndexCount = 0;
        swapchainCreateInfo.pQueueFamilyIndices = nullptr;
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchainCreateInfo.presentMode = presentMode;
        swapchainCreateInfo.oldSwapchain = oldChain ? oldChain->GetHandle() : VK_NULL_HANDLE;
        swapchainCreateInfo.clipped = VK_TRUE;
        swapchainCreateInfo.compositeAlpha = compositeAlpha;

        result = vkCreateSwapchainKHR(m_Device, &swapchainCreateInfo, gVulkanAllocator, &m_SwapChain);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
        
        uint32_t imageCount;
        result = vkGetSwapchainImagesKHR(m_Device, m_SwapChain, &imageCount, nullptr);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);

        VkImage* images = new VkImage[imageCount];
        result = vkGetSwapchainImagesKHR(m_Device, m_SwapChain, &imageCount, images);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
        
        VulkanImageDesc imageDesc;
        imageDesc.Format = colorFormat;
        imageDesc.Shape = TextureShape::TEXTURE_2D;
        imageDesc.Usage = TextureUsage::TEXTURE_RENDERTARGET;
        imageDesc.Layout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageDesc.Faces = 1;
        imageDesc.NumMips = 1;
        imageDesc.Allocation = VK_NULL_HANDLE;

        m_Surfaces.resize(imageCount);
        for (uint32_t i = 0; i < imageCount; i++)
        {
            imageDesc.Image = images[i];
            m_Surfaces[i].NeedsWait = false;
            m_Surfaces[i].Acquired = false;
            m_Surfaces[i].Sync = owner->Create<VulkanSemaphore>();;
            m_Surfaces[i].Image = owner->Create<VulkanImage>(imageDesc, false);
        }
        
        delete[] images;
        if (createDepth)
        {
            VkImageCreateInfo depthStencilImageCI;
            depthStencilImageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            depthStencilImageCI.pNext = nullptr;
            depthStencilImageCI.flags = 0;
            depthStencilImageCI.imageType = VK_IMAGE_TYPE_2D;
            depthStencilImageCI.format = depthFormat;
            depthStencilImageCI.extent = { m_Width, m_Height, 1 };
            depthStencilImageCI.mipLevels = 1;
            depthStencilImageCI.arrayLayers = 1;
            depthStencilImageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            depthStencilImageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            depthStencilImageCI.samples = VK_SAMPLE_COUNT_1_BIT;
            depthStencilImageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
            depthStencilImageCI.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            depthStencilImageCI.pQueueFamilyIndices = nullptr;
            depthStencilImageCI.queueFamilyIndexCount = 0;

            VkImage depthStencilImage;
            result = vkCreateImage(m_Device, &depthStencilImageCI, gVulkanAllocator, &depthStencilImage);
            CW_ENGINE_ASSERT(result == VK_SUCCESS);

            imageDesc.Image = depthStencilImage;
            imageDesc.Usage = TextureUsage::TEXTURE_DEPTHSTENCIL;
            imageDesc.Format = depthFormat;
            imageDesc.Allocation = device.AllocateMemory(depthStencilImage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            m_DepthStencilImage = owner->Create<VulkanImage>(imageDesc, true);
        }
        else
            m_DepthStencilImage = nullptr;

        VulkanRenderPassDesc passDesc;
        passDesc.Samples = 1;
        passDesc.Offscreen = false;
        passDesc.Color[0].Format = colorFormat;
        passDesc.Color[0].Enabled = true;

        if (m_DepthStencilImage)
        {
            passDesc.Depth.Format = depthFormat;
            passDesc.Depth.Enabled = true;
        }

        VulkanRenderPass* renderPass = VulkanRenderPasses::Get().GetRenderPass(passDesc);
        uint32_t numFramebuffers = (uint32_t)m_Surfaces.size();
        for (uint32_t i = 0; i < numFramebuffers; i++)
        {
            VulkanFramebufferDesc desc;
            desc.Width = m_Width;
            desc.Height = m_Height;
            desc.LayerCount = 1;
            desc.Color[0].Image = m_Surfaces[i].Image;
            desc.Color[0].Surface = TextureSurface::COMPLETE;
            desc.Color[0].BaseLayer = 0;
            desc.Depth.Image = m_DepthStencilImage;
            desc.Depth.Surface = TextureSurface::COMPLETE;
            desc.Depth.BaseLayer = 0;
            m_Surfaces[i].Framebuffer = owner->Create<VulkanFramebuffer>(renderPass, desc);
        }
    }
    
    VulkanSwapChain::~VulkanSwapChain()
    {
        if (m_SwapChain != VK_NULL_HANDLE)
        {
            for (auto& surface : m_Surfaces)
            {
                surface.Framebuffer->Destroy();
                surface.Framebuffer = nullptr;
                surface.Sync->Destroy();
                surface.Sync = nullptr;
                surface.Image->Destroy();
                surface.Image = nullptr;
            }
            vkDestroySwapchainKHR(m_Device, m_SwapChain, gVulkanAllocator);
        }
        if (m_DepthStencilImage != nullptr)
        {
            m_DepthStencilImage->Destroy();
            m_DepthStencilImage = nullptr;
        }
    }

    void VulkanSwapChain::BackBufferWaitIssued()
    {
        if (!m_Surfaces[m_CurrentBackBufferIdx].Acquired)
            return;
        m_Surfaces[m_CurrentBackBufferIdx].NeedsWait = false;
    }

    VkResult VulkanSwapChain::AcquireBackBuffer()
    {
        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(m_Device, m_SwapChain, std::numeric_limits<uint64_t>::max(), m_Surfaces[m_CurrentSemaphoreIdx].Sync->GetHandle(), VK_NULL_HANDLE, &imageIndex);
        if (result != VK_SUCCESS)
            return result;

        if (imageIndex != m_CurrentSemaphoreIdx)
            std::swap(m_Surfaces[m_CurrentSemaphoreIdx].Sync, m_Surfaces[imageIndex].Sync);

        m_CurrentSemaphoreIdx = (m_CurrentSemaphoreIdx + 1) % m_Surfaces.size();
        
        CW_ENGINE_ASSERT(!m_Surfaces[imageIndex].Acquired, "Swap chain image acquired twice!");
        m_Surfaces[imageIndex].Acquired = true;
        m_Surfaces[imageIndex].NeedsWait = true;
        m_CurrentBackBufferIdx = imageIndex;
        return VK_SUCCESS;
    }

    bool VulkanSwapChain::PrepareForPresent(uint32_t& backBufferIdx)
    {
        CW_ENGINE_ASSERT(m_Surfaces[m_CurrentBackBufferIdx].Acquired, "Unacquired back buffer!");
        if (!m_Surfaces[m_CurrentBackBufferIdx].Acquired)
            return false;
        m_Surfaces[m_CurrentBackBufferIdx].Acquired = false;
        backBufferIdx = m_CurrentBackBufferIdx;
        return true;
    }
    
}