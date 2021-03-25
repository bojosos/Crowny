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
    
    VulkanSwapChain::VulkanSwapChain(VkSurfaceKHR surface, uint32_t width, uint32_t height, bool vsync, VkFormat colorFormat,VkColorSpaceKHR colorSpace, bool createDepth, VkFormat depthFormat, VulkanSwapChain* oldChain)
    {
        VkPhysicalDevice physicalDevice;
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

        if (surfCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
            swapchainCreateInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        
        if (surfCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            swapchainCreateInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        uint32_t imageCount;
        result = vkGetSwapchainImagesKHR(m_Device, m_SwapChain, &imageCount, nullptr);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);

        VkImage* images = new VkImage[imageCount];
        result = vkGetSwapchainImagesKHR(m_Device, m_SwapChain, &imageCount, images);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
        
        m_Surfaces.resize(imageCount);
        for (uint32_t i = 0; i < imageCount; i++)
        {
            VkImageViewCreateInfo viewCreateInfo{};
            viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewCreateInfo.pNext = nullptr;
            viewCreateInfo.flags = 0;
            viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewCreateInfo.format = colorFormat;
            viewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
            viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewCreateInfo.subresourceRange.baseMipLevel = 0;
            viewCreateInfo.subresourceRange.levelCount = 1;
            viewCreateInfo.subresourceRange.baseArrayLayer = 0;
            viewCreateInfo.subresourceRange.layerCount = 1;
            viewCreateInfo.image = images[i];
            result = vkCreateImageView(m_Device, &viewCreateInfo, nullptr, &m_Surfaces[i]);
            CW_ENGINE_ASSERT(result == VK_SUCCESS);
            
            m_Surfaces[i].Image = images[i];
            m_Surfaces[i].NeedsWait = false;
            m_Surfaces[i].Acquired = false;
            m_Surfaces[i].Sync = new VulkanSemaphore();
        }
        
        if (createDepth)
        {
            VkImageCreateInfo depthStencilImageCreateInfo;
            depthStencilImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            depthStencilImageCreateInfo.pNext = nullptr;
            depthStencilImageCreateInfo.flags = 0;
            depthStencilImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
            depthStencilImageCreateInfo.format = depthFormat;
            depthStencilImageCreateInfo.extent = { m_Width, m_Height, 1 };
            depthStencilImageCreateInfo.mipLevels = 1;
            depthStencilImageCreateInfo.arrayLayers = 1;
            depthStencilImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            depthStencilImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            depthStencilImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            depthStencilImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            depthStencilImageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            depthStencilImageCreateInfo.pQueueFamilyIndices = nullptr;
            depthStencilImageCreateInfo.queueFamilyIndexCount = 0;

            VkImage depthStencilImage;
            result = vkCreateImage(m_Device, &depthStencilImageCreateInfo, nullptr, &depthStencilImage);
            CW_ENGINE_ASSERT(result == VK_SUCCESS);

            VkMemoryRequirements memReqs;
		    vkGetImageMemoryRequirements(device, image, &memReqs);
		    VkMemoryAllocateInfo memAllocInfo{};
		    memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		    memAllocInfo.allocationSize = memReqs.size;
		    memAllocInfo.memoryTypeIndex = ;
		    result = vkAllocateMemory(device, &memAllocInfo, nullptr, &memory);
		    CW_ENGINE_ASSERT(result == VK_SUCCESS);
		    result = vkBindImageMemory(device, image, memory, 0);

            m_DepthStencilImage = new VulkanTexture2D(depthStencilImage);
        }
        else
            m_DepthStencilImage = nullptr;
        delete[] images;

        VulkanRenderPassDesc passDesc;
        passDesc.Samples = 1;
        passDesc.Offscreen = false;
        passDesc.Color[0].Format = colorFormat;

        if (m_DepthStencilImage)
            passDesc.Depth.Format = depthFormat;
        VulkanRenderPass* renderPass = VulkanRenderPasses::Get().Get(m_Device, passDesc);

        uint32_t numFramebuffers = (uint32_t)m_Surfaces.size();
        for (uint32_t i = 0; i < numFramebuffers; i++)
        {
            VulkanFramebufferDesc desc;
            desc.Width = m_Width;
            desc.Height = m_Height;
            desc.Layers = 1;
            desc.Color[0].Image = m_Surfaces[i].Image;
            desc.Depth.Image = m_DepthStencilImage;
            m_Surfaces[i].Framebuffer = new VulkanFramebuffer(renderPass, desc);
        }
    }
    
    VulkanSwapChain::~VulkanSwapChain()
    {
        if (m_SwapChain != VK_NULL_HANDLE)
        {
            for (auto& surface : m_Surfaces)
            {
                delete surface.Framebuffer;
                surface.Framebuffer = nullptr;
                delete surface.Sync;
                surface.Sync = nullptr;
                delete surface.Image;
                surface.Image = nullptr;
            }
            vkDestroySwapchainKHR(m_Device, m_SwapChain, nullptr);
        }
        if (m_DepthStencilImage != nullptr)
        {
            delete m_DepthStencilImage;
            m_DepthStencilImage = nullptr;
        }
    }

    void VulkanSwapChain::BackBufferWaitIssued()
    {
        if (m_Surfaces[m_CurrentBackBufferIdx].Acquired)
            return;
        m_Surfaces[m_CurrentBackBufferIdx].NeedsWait = false;
    }

    VkResult VulkanSwapChain::AcquireBackBuffer()
    {
        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(m_Device, m_SwapChain, std::numeric_limits<uint64_t>::max(), m_Surfaces[m_CurrentSemaphoreIdx].Sync, VK_NULL_HANDLE, &imageIndex);
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
        backBufferidx = m_CurrentBackBufferIdx;
        return true;
    }
    
}