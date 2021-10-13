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

#include <vulkan/vulkan.h>
#undef None
#undef Button1
#undef Button2
#undef Button3
#undef Button4
#undef Button5
#undef Button6
#undef Button7
#undef Button8
#undef Button9

#include <vulkan/vk_mem_alloc.h>

#define MAX_UNIQUE_QUEUES MAX_QUEUES_PER_TYPE* QUEUE_COUNT

namespace Crowny
{
    class VulkanRenderAPI;
    class VulkanTexture;
    class VulkanDevice;
    class VulkanSwapChain;
    class VulkanFramebuffer;
    class VulkanDescriptorSet;
    class VulkanCommandBufferPool;
    class VulkanCommandBuffer;
    class VulkanVertexBuffer;
    class VulkanIndexBuffer;
    class VulkanUniformBuffer;
    class VulkanSemaphore;
    class VulkanQueue;

    extern VkAllocationCallbacks* gVulkanAllocator;

    struct TransitionInfo
    {
        Vector<VkImageMemoryBarrier> ImageBarriers;
        Vector<VkBufferMemoryBarrier> BufferBarriers;
    };

    enum ClearMaskBits
    {
        CLEAR_NONE = 0,
        CLEAR_COLOR0 = 1 << 0,
        CLEAR_COLOR1 = 1 << 1,
        CLEAR_COLOR2 = 1 << 2,
        CLEAR_COLOR3 = 1 << 3,
        CLEAR_COLOR4 = 1 << 4,
        CLEAR_COLOR5 = 1 << 5,
        CLEAR_COLOR6 = 1 << 6,
        CLEAR_COLOR7 = 1 << 7,
        CLEAR_STENCIL = 1 << 30,
        CLEAR_DEPTH = 1 << 31,
        CLEAR_ALL = 0xFF
    };
    typedef Flags<ClearMaskBits> ClearMask;
    CW_FLAGS_OPERATORS(ClearMaskBits);

    class VulkanUtils
    {
    public:
        static VkFilter GetFilter(TextureFilter MagFilter);
        static VkSamplerMipmapMode GetMipFilter(TextureFilter MipFilter);
        static VkSamplerAddressMode GetAddressingMode(TextureWrap mode);
        static VkCompareOp GetCompareOp(CompareFunction compareFunc);
        static VkCullModeFlagBits GetCullMode(CullingMode mode);
        static VkBlendOp GetBlendOp(BlendFunction blendFunc);
        static VkBlendFactor GetBlendFactor(BlendFactor factor);
        static VkFormat GetTextureFormat(TextureFormat format, bool sRGB);
        static TextureFormat GetClosestSupportedTextureFormat(const VulkanDevice& device, TextureFormat format,
                                                              TextureShape shape, int usage, bool optimapTiling);
        static VkSampleCountFlagBits GetSampleFlags(uint32_t numSamples);
        static VkPrimitiveTopology GetDrawFlags(DrawMode drawMode);
        static VkShaderStageFlagBits GetShaderFlags(ShaderType shaderType);
        static VkIndexType GetIndexType(IndexType indexType);
        static VkPipelineStageFlags ShaderToPipelineStage(VkShaderStageFlags shaderStageFlags);
        static bool RangeOverlaps(const VkImageSubresourceRange& a, const VkImageSubresourceRange& b);
        static VkFormat GetDummyViewFormat(GpuBufferFormat format);

        static void CutRange(const VkImageSubresourceRange& a, const VkImageSubresourceRange& b,
                             std::array<VkImageSubresourceRange, 5>& output, uint32_t& numAreas);
        static void CutVertical(const VkImageSubresourceRange& toCut, const VkImageSubresourceRange& cutWith,
                                VkImageSubresourceRange* output, uint32_t& numAreas);
        static void CutHorizontal(const VkImageSubresourceRange& toCut, const VkImageSubresourceRange& cutWith,
                                  VkImageSubresourceRange* output, uint32_t& numAreas);
    };
} // namespace Crowny

#define GET_INSTANCE_PROC_ADDR(instance, name)                                                                         \
    vk##name = reinterpret_cast<PFN_vk##name>(vkGetInstanceProcAddr(instance, "vk" #name));

#define GET_DEVICE_PROC_ADDR(device, name)                                                                             \
    vk##name = reinterpret_cast<PFN_vk##name>(vkGetDeviceProcAddr(device, "vk" #name));
