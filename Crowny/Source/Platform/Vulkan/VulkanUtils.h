#pragma once

#define VK_USE_PLATFORM_XLIB_KHR
/*
#if defined(CW_PLATFORM_WIN32)
    #define VK_USE_PLATFORM_WIN32_KHR
#elif defined(CW_PLATFORM_LINUX)
    #define VK_USE_PLATFORM_XLIB_KHR
#elif defined(CW_PLATFORM_ANDROID)
    #define VK_USE_PLATFORM_ANDROID_KHR
#elif defined(CW_PLATFORM_OSX)
    #define VK_USR_PLATFORM_MACOS_MVK
#endif
  */

#include <vulkan/vulkan.hpp>
#undef None
#undef Bool
#include <vulkan/vk_mem_alloc.h>

namespace Crowny
{
    extern VkAllocationCallbacks* gVulkanAllocator;

    struct TransitionInfo
    {
        std::vector<VkImageMemoryBarrier> ImageBarriers;
        std::vector<VkBufferMemoryBarrier> MemoryBarriers;
    };
    
    class VulkanUtils
    {
    public:
         static VkSampleCountFlagBits GetSampleFlags(uint32_t numSamples);
    };
}

#define GET_INSTANCE_PROC_ADDR(instance, name) \
    vk##name = reinterpret_cast<PFN_vk##name>(vkGetInstanceProcAddr(instance, "vk"#name));

#define GET_DEVICE_PROC_ADDR(device, name) \
    vk##name = reinterpret_cast<PFN_vk##name>(vkGetDeviceProcAddr(device, "vk"#name));