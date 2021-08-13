#include "cwpch.h"

#include "Platform/Vulkan/VulkanCommandBuffer.h"
#include "Platform/Vulkan/VulkanRenderAPI.h"
#include "Platform/Vulkan/VulkanTexture.h"

#include "Crowny/Common/VirtualFileSystem.h"
#include "Crowny/Utils/PixelUtils.h"

#include <glad/glad.h>
#include <stb_image.h>

namespace Crowny
{

    VulkanImageDesc CreateDesc(VkImage image, VmaAllocation allocation, VkImageLayout layout, VkFormat actualFormat,
                               const TextureParameters& params)
    {
        VulkanImageDesc desc;
        desc.Image = image;
        desc.Allocation = allocation;
        desc.Shape = params.Shape;
        desc.NumMips = params.MipLevels + 1;
        desc.Layout = layout;
        desc.Faces = params.NumArraySlices;
        desc.Format = actualFormat;
        desc.Usage = params.Usage;

        return desc;
    }

    VulkanImage::VulkanImage(VulkanResourceManager* owner, VkImage image, VmaAllocation allocation,
                             VkImageLayout layout, VkFormat format, const TextureParameters& params, bool ownsImage)
      : VulkanImage(owner, CreateDesc(image, allocation, layout, format, params), ownsImage)
    {
    }

    VulkanImage::VulkanImage(VulkanResourceManager* owner, const VulkanImageDesc& desc, bool ownsImage)
      : VulkanResource(owner, false), m_FramebufferMainView(VK_NULL_HANDLE), m_Image(desc.Image),
        m_Allocation(desc.Allocation), m_Usage(desc.Usage), m_OwnsImage(ownsImage), m_NumFaces(desc.Faces),
        m_NumMipLevels(desc.NumMips)
    {
        m_ImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        m_ImageViewCreateInfo.pNext = nullptr;
        m_ImageViewCreateInfo.flags = 0;
        m_ImageViewCreateInfo.image = desc.Image;
        m_ImageViewCreateInfo.format = desc.Format;
        m_ImageViewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B,
                                             VK_COMPONENT_SWIZZLE_A };

        switch (desc.Shape)
        {
        case TextureShape::TEXTURE_1D:
            m_ImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_1D;
            break;
        case TextureShape::TEXTURE_2D:
            m_ImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            break;
        case TextureShape::TEXTURE_3D:
            m_ImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
            break;
        case TextureShape::TEXTURE_CUBE:
            m_ImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
            break;
        }

        TextureSurface completeSurface(0, desc.NumMips, 0, desc.Faces);
        if ((m_Usage & TextureUsage::TEXTURE_DEPTHSTENCIL) != 0)
        {
            m_FramebufferMainView = CreateView(completeSurface, desc.Format, GetAspectFlags());
            m_MainView = CreateView(completeSurface, desc.Format, VK_IMAGE_ASPECT_DEPTH_BIT);
        }
        else
            m_MainView = CreateView(completeSurface, desc.Format, VK_IMAGE_ASPECT_COLOR_BIT);

        ImageViewInfo mainViewInfo;
        mainViewInfo.Surface = completeSurface;
        mainViewInfo.Framebuffer = false;
        mainViewInfo.View = m_MainView;
        mainViewInfo.Format = desc.Format;

        m_ImageInfos.push_back(mainViewInfo);
        if (m_FramebufferMainView != VK_NULL_HANDLE)
        {
            ImageViewInfo fbMainViewInfo;
            fbMainViewInfo.Surface = completeSurface;
            fbMainViewInfo.Framebuffer = true;
            fbMainViewInfo.View = m_FramebufferMainView;
            fbMainViewInfo.Format = desc.Format;
            m_ImageInfos.push_back(fbMainViewInfo);
        }

        uint32_t numSubresources = m_NumFaces * m_NumMipLevels;
        m_Subresources = new VulkanImageSubresource*[numSubresources];
        for (uint32_t i = 0; i < numSubresources; i++)
            m_Subresources[i] = owner->Create<VulkanImageSubresource>(desc.Layout);
    }

    VulkanImage::~VulkanImage()
    {
        VulkanDevice& device = m_Owner->GetDevice();
        VkDevice vkDevice = device.GetLogicalDevice();

        uint32_t numSubresources = m_NumFaces * m_NumMipLevels;
        for (uint32_t i = 0; i < numSubresources; i++)
        {
            CW_ENGINE_ASSERT(!m_Subresources[i]->IsBound());
            m_Subresources[i]->Destroy();
        }

        for (auto& entry : m_ImageInfos)
            vkDestroyImageView(vkDevice, entry.View, gVulkanAllocator);

        if (m_OwnsImage)
        {
            vkDestroyImage(vkDevice, m_Image, gVulkanAllocator);
            device.FreeMemory(m_Allocation);
        }
    }

    VkImageView VulkanImage::GetView(bool framebuffer) const
    {
        if (framebuffer && (m_Usage & TextureUsage::TEXTURE_DEPTHSTENCIL) != 0)
            return m_FramebufferMainView;
        return m_MainView;
    }

    VkImageView VulkanImage::GetView(const TextureSurface& surface, bool framebuffer) const
    {
        return GetView(m_ImageViewCreateInfo.format, surface, framebuffer);
    }

    VkImageView VulkanImage::GetView(VkFormat format, bool framebuffer) const
    {
        TextureSurface complete(0, m_NumMipLevels, 0, m_NumFaces);
        return GetView(format, complete, framebuffer);
    }

    VkImageView VulkanImage::GetView(VkFormat format, const TextureSurface& surface, bool framebuffer) const
    {
        for (auto& entry : m_ImageInfos)
        {
            if (surface.MipLevel == entry.Surface.MipLevel && surface.NumMipLevels == entry.Surface.NumMipLevels &&
                surface.Face == entry.Surface.Face && surface.NumFaces == entry.Surface.NumFaces &&
                format == entry.Format)
            {
                if ((m_Usage & TextureUsage::TEXTURE_DEPTHSTENCIL) == 0)
                    return entry.View;
                else if (framebuffer == entry.Framebuffer)
                    return entry.View;
            }
        }

        ImageViewInfo viewInfo;
        viewInfo.Surface = surface;
        viewInfo.Framebuffer = framebuffer;
        viewInfo.Format = format;

        if ((m_Usage & TextureUsage::TEXTURE_DEPTHSTENCIL) != 0)
        {
            if (framebuffer)
                viewInfo.View = CreateView(surface, format, GetAspectFlags());
            else
                viewInfo.View = CreateView(surface, format, VK_IMAGE_ASPECT_DEPTH_BIT);
        }
        else
            viewInfo.View = CreateView(surface, format, VK_IMAGE_ASPECT_COLOR_BIT);
        m_ImageInfos.push_back(viewInfo);
        return viewInfo.View;
    }

    VkImageView VulkanImage::CreateView(const TextureSurface& surface, VkFormat format,
                                        VkImageAspectFlags aspectFlags) const
    {
        VkImageViewType oldViewType = m_ImageViewCreateInfo.viewType;
        VkFormat oldFormat = m_ImageViewCreateInfo.format;

        uint32_t numFaces = surface.NumFaces == 0 ? m_NumFaces : surface.NumFaces;
        switch (oldViewType)
        {
        case VK_IMAGE_VIEW_TYPE_CUBE:
            if (numFaces == 1)
                m_ImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            else if (numFaces % 6 == 0)
            {
                if (m_NumFaces > 6)
                    m_ImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
            }
            else
                m_ImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            break;
        case VK_IMAGE_VIEW_TYPE_1D:
            if (m_NumFaces > 0)
                m_ImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_1D_ARRAY;
            break;
        case VK_IMAGE_VIEW_TYPE_2D:
        case VK_IMAGE_VIEW_TYPE_3D:
            if (m_NumFaces > 1)
                m_ImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            break;
        default:
            break;
        }

        m_ImageViewCreateInfo.subresourceRange.aspectMask = aspectFlags;
        m_ImageViewCreateInfo.subresourceRange.baseMipLevel = surface.MipLevel;
        m_ImageViewCreateInfo.subresourceRange.levelCount =
          surface.NumMipLevels == 0 ? VK_REMAINING_MIP_LEVELS : surface.NumMipLevels;
        m_ImageViewCreateInfo.subresourceRange.baseArrayLayer = surface.Face;
        m_ImageViewCreateInfo.subresourceRange.layerCount =
          surface.NumFaces == 0 ? VK_REMAINING_ARRAY_LAYERS : surface.NumFaces;
        m_ImageViewCreateInfo.format = format;

        VkImageView view;
        VkResult result =
          vkCreateImageView(m_Owner->GetDevice().GetLogicalDevice(), &m_ImageViewCreateInfo, gVulkanAllocator, &view);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
        m_ImageViewCreateInfo.viewType = oldViewType;
        m_ImageViewCreateInfo.format = oldFormat;
        return view;
    }

    VkImageLayout VulkanImage::GetOptimalLayout() const
    {
        if ((m_Usage & TextureUsage::TEXTURE_LOADSTORE) != 0)
            return VK_IMAGE_LAYOUT_GENERAL;
        if ((m_Usage & TextureUsage::TEXTURE_RENDERTARGET) != 0)
            return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        if ((m_Usage & TextureUsage::TEXTURE_DEPTHSTENCIL) != 0)
            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        if ((m_Usage & TextureUsage::TEXTURE_DYNAMIC) != 0)
            return VK_IMAGE_LAYOUT_GENERAL;

        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    VkImageSubresourceRange VulkanImage::GetRange() const
    {
        VkImageSubresourceRange range;
        range.baseArrayLayer = 0;
        range.layerCount = m_NumFaces;
        range.baseMipLevel = 0;
        range.levelCount = m_NumMipLevels;
        range.aspectMask = GetAspectFlags();

        return range;
    }

    VkImageSubresourceRange VulkanImage::GetRange(const TextureSurface& surface) const
    {
        VkImageSubresourceRange range;
        range.baseArrayLayer = surface.Face;
        range.layerCount = surface.NumFaces == 0 ? m_NumFaces : surface.NumFaces;
        range.baseMipLevel = surface.MipLevel;
        range.levelCount = surface.NumMipLevels == 0 ? m_NumMipLevels : surface.NumMipLevels;
        range.aspectMask = GetAspectFlags();

        return range;
    }

    VulkanImageSubresource* VulkanImage::GetSubresource(uint32_t face, uint32_t mipLevel)
    {
        CW_ENGINE_ASSERT(mipLevel * m_NumFaces + face < m_NumFaces * m_NumMipLevels);
        return m_Subresources[mipLevel * m_NumFaces + face];
    }

    void VulkanImage::Map(uint32_t face, uint32_t mip, PixelData& output) const
    {
        VulkanDevice& device = m_Owner->GetDevice();

        VkImageSubresource range;
        range.mipLevel = mip;
        range.arrayLayer = face;

        if (m_ImageViewCreateInfo.subresourceRange.aspectMask == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        else
            range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

        VkSubresourceLayout layout;
        vkGetImageSubresourceLayout(device.GetLogicalDevice(), m_Image, &range, &layout);

        output.SetRowPitch((uint32_t)layout.rowPitch);
        output.SetSlicePitch((uint32_t)layout.depthPitch);
        VkDeviceMemory memory;
        VkDeviceSize memoryOffset;
        device.GetAllocationInfo(m_Allocation, memory, memoryOffset);

        uint8_t* data;
        VkResult result =
          vkMapMemory(device.GetLogicalDevice(), memory, memoryOffset + layout.offset, layout.size, 0, (void**)&data);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);

        output.SetBuffer(data);
    }

    uint8_t* VulkanImage::Map(uint32_t offset, uint32_t size) const
    {
        VulkanDevice& device = m_Owner->GetDevice();

        VkDeviceMemory memory;
        VkDeviceSize memoryOffset;
        device.GetAllocationInfo(m_Allocation, memory, memoryOffset);
        uint8_t* data;
        VkResult result = vkMapMemory(device.GetLogicalDevice(), memory, memoryOffset + offset, size, 0, (void**)&data);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
        return data;
    }

    void VulkanImage::Unmap()
    {
        VulkanDevice& device = m_Owner->GetDevice();
        VkDeviceMemory memory;
        VkDeviceSize memoryOffset;
        device.GetAllocationInfo(m_Allocation, memory, memoryOffset);
        vkUnmapMemory(device.GetLogicalDevice(), memory);
    }

    void VulkanImage::Copy(VulkanTransferBuffer* cb, VulkanBuffer* dest, const VkExtent3D& extent,
                           const VkImageSubresourceLayers& range, VkImageLayout layout)
    {
        VkBufferImageCopy region;
        region.bufferRowLength = dest->GetRowPitch();
        region.bufferImageHeight = dest->GetSliceHeight();
        region.bufferOffset = 0;
        region.imageOffset.x = 0;
        region.imageOffset.y = 0;
        region.imageOffset.z = 0;
        region.imageExtent = extent;
        region.imageSubresource = range;

        vkCmdCopyImageToBuffer(cb->GetCB()->GetHandle(), m_Image, layout, dest->GetHandle(), 1, &region);
    }

    VkAccessFlags VulkanImage::GetAccessFlags(VkImageLayout layout, bool readOnly)
    {
        VkAccessFlags accessFlags;
        switch (layout)
        {
        case VK_IMAGE_LAYOUT_GENERAL: {
            accessFlags = VK_ACCESS_SHADER_READ_BIT;
            if ((m_Usage & TextureUsage::TEXTURE_LOADSTORE) != 0)
            {
                if (!readOnly)
                    accessFlags |= VK_ACCESS_SHADER_WRITE_BIT;
            }
            else if ((m_Usage & TextureUsage::TEXTURE_RENDERTARGET) != 0)
            {
                accessFlags |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
                if (!readOnly)
                    accessFlags |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            }
            else if ((m_Usage & TextureUsage::TEXTURE_DEPTHSTENCIL) != 0)
            {
                accessFlags |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
                if (!readOnly)
                    accessFlags |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            }
        }
        break;
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            accessFlags = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
            break;
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            accessFlags = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
        case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL_KHR:
        case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL_KHR:
            accessFlags = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT;
            break;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            accessFlags = VK_ACCESS_SHADER_READ_BIT;
            break;
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            accessFlags = VK_ACCESS_TRANSFER_READ_BIT;
            break;
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            accessFlags = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
            accessFlags = VK_ACCESS_MEMORY_READ_BIT;
            break;
        case VK_IMAGE_LAYOUT_UNDEFINED:
        case VK_IMAGE_LAYOUT_PREINITIALIZED:
            accessFlags = 0;
            break;
        default:
            accessFlags = 0;
            CW_ENGINE_ASSERT(false);
            break;
        }
        return accessFlags;
    }

    void VulkanImage::GetBarriers(const VkImageSubresourceRange& range, std::vector<VkImageMemoryBarrier>& barriers)
    {
        uint32_t numSubresources = range.levelCount * range.layerCount;
        if (numSubresources == 0)
            return;
        uint32_t mip = range.baseMipLevel;
        uint32_t face = range.baseArrayLayer;
        uint32_t lastMip = range.baseMipLevel + range.levelCount - 1;
        uint32_t lastFace = range.baseArrayLayer + range.layerCount - 1;

        VkImageMemoryBarrier defaultBarrier;
        defaultBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        defaultBarrier.pNext = nullptr;
        defaultBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        defaultBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        defaultBarrier.image = m_Image;
        defaultBarrier.subresourceRange.aspectMask = range.aspectMask;
        defaultBarrier.subresourceRange.levelCount = 1;
        defaultBarrier.subresourceRange.layerCount = 1;
        defaultBarrier.subresourceRange.baseArrayLayer = 0;
        defaultBarrier.subresourceRange.baseMipLevel = 0;

        auto createNewBarrier = [&](VulkanImageSubresource* subresource, uint32_t face, uint32_t mip) {
            barriers.push_back(defaultBarrier);
            VkImageMemoryBarrier* barrier = &barriers.back();
            barrier->subresourceRange.baseArrayLayer = face;
            barrier->subresourceRange.baseMipLevel = mip;
            barrier->srcAccessMask = GetAccessFlags(subresource->GetLayout());
            barrier->oldLayout = subresource->GetLayout();
            return barrier;
        };

        std::vector<bool> processed(numSubresources);
        VulkanImageSubresource* subresource = GetSubresource(face, mip);
        createNewBarrier(subresource, face, mip);
        numSubresources--;
        while (numSubresources > 0)
        {
            VkImageMemoryBarrier* barrier = &barriers.back();
            while (true)
            {
                bool expandedFace = true;
                if (face < lastFace)
                {
                    for (uint32_t i = 0; i < barrier->subresourceRange.levelCount; i++)
                    {
                        uint32_t curMip = barrier->subresourceRange.baseMipLevel + i;
                        VulkanImageSubresource* subresource = GetSubresource(face + 1, curMip);
                        if (barrier->oldLayout != subresource->GetLayout())
                        {
                            expandedFace = false;
                            break;
                        }
                    }

                    if (expandedFace)
                    {
                        barrier->subresourceRange.layerCount++;
                        numSubresources -= barrier->subresourceRange.levelCount;
                        face++;

                        for (uint32_t i = 0; i < barrier->subresourceRange.levelCount; i++)
                        {
                            uint32_t curMip = (barrier->subresourceRange.baseMipLevel + i) - range.baseMipLevel;
                            uint32_t idx = curMip * range.layerCount + (face - range.baseArrayLayer);
                            processed[idx] = true;
                        }
                    }
                }
                else
                    expandedFace = false;
                bool expandedMip = true;
                if (mip < lastMip)
                {
                    for (uint32_t i = 0; i < barrier->subresourceRange.layerCount; i++)
                    {
                        uint32_t curFace = barrier->subresourceRange.baseArrayLayer + i;
                        VulkanImageSubresource* subresource = GetSubresource(curFace, mip + 1);
                        if (barrier->oldLayout != subresource->GetLayout())
                        {
                            expandedMip = false;
                            break;
                        }
                    }

                    if (expandedMip)
                    {
                        barrier->subresourceRange.layerCount++;
                        numSubresources -= barrier->subresourceRange.layerCount;
                        mip++;
                        for (uint32_t i = 0; i < barrier->subresourceRange.layerCount; i++)
                        {
                            uint32_t curFace = (barrier->subresourceRange.baseArrayLayer + i) - range.baseArrayLayer;
                            uint32_t idx = (mip - range.baseMipLevel) * range.layerCount + curFace;
                            processed[idx] = true;
                        }
                    }
                }
                else
                    expandedMip = false;
                if (!expandedMip && !expandedFace)
                    break;
            }

            for (uint32_t i = 0; i < range.levelCount; i++)
            {
                bool found = false;
                for (uint32_t j = 0; j < range.layerCount; j++)
                {
                    uint32_t idx = i * range.layerCount + j;
                    if (!processed[idx])
                    {
                        mip = range.baseMipLevel + i;
                        face = range.baseArrayLayer + j;
                        found = true;
                        processed[idx] = true;
                        break;
                    }
                }
                if (found)
                {
                    VulkanImageSubresource* subresource = GetSubresource(face, mip);
                    createNewBarrier(subresource, face, mip);
                    numSubresources--;
                    break;
                }
            }
        }
    }

    VkImageAspectFlags VulkanImage::GetAspectFlags() const
    {
        if ((m_Usage & TextureUsage::TEXTURE_DEPTHSTENCIL) != 0)
        {
            bool hasStencil = m_ImageViewCreateInfo.format == VK_FORMAT_D16_UNORM_S8_UINT ||
                              m_ImageViewCreateInfo.format == VK_FORMAT_D24_UNORM_S8_UINT ||
                              m_ImageViewCreateInfo.format == VK_FORMAT_D32_SFLOAT_S8_UINT;
            if (hasStencil)
                return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
            return VK_IMAGE_ASPECT_DEPTH_BIT;
        }

        return VK_IMAGE_ASPECT_COLOR_BIT;
    }

    VulkanImageSubresource::VulkanImageSubresource(VulkanResourceManager* owner, VkImageLayout layout)
      : VulkanResource(owner, false), m_Layout(layout)
    {
    }

    VulkanTexture::VulkanTexture(const TextureParameters& params)
      : Texture(params), m_Image(nullptr), m_InternalFormat(), m_StagingBuffer(nullptr),
        m_MappedGlobalQueueIdx((uint32_t)-1), m_MappedMip(0), m_MappedFace(0), m_MappedRowPitch(0),
        m_MappedSlicePitch(0), m_MappedLockOptions(GpuLockOptions::WRITE_ONLY), m_DirectlyMappable(false),
        m_SupportsGpuWrites(false), m_IsMapped(false)
    {
        m_ImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        m_ImageCreateInfo.pNext = nullptr;
        m_ImageCreateInfo.flags = 0;

        switch (params.Shape)
        {
        case TextureShape::TEXTURE_1D:
            m_ImageCreateInfo.imageType = VK_IMAGE_TYPE_1D;
            break;
        case TextureShape::TEXTURE_2D:
            m_ImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
            break;
        case TextureShape::TEXTURE_3D:
            m_ImageCreateInfo.imageType = VK_IMAGE_TYPE_3D;
            break;
        case TextureShape::TEXTURE_CUBE:
            m_ImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
            m_ImageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
            break;
        }
        m_ImageCreateInfo.usage =
          VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        int32_t usage = (int32_t)params.Usage;
        if ((usage & TEXTURE_RENDERTARGET) != 0)
        {
            m_ImageCreateInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            m_SupportsGpuWrites = true;
        }
        else if ((usage & TEXTURE_DEPTHSTENCIL) != 0)
        {
            m_ImageCreateInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            m_SupportsGpuWrites = true;
        }

        if ((usage & TEXTURE_LOADSTORE) != 0)
        {
            m_ImageCreateInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
            m_SupportsGpuWrites = true;
        }

        VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;

        if ((usage & TEXTURE_DYNAMIC) != 0)
        {
            if (params.Shape == TextureShape::TEXTURE_2D && params.Samples <= 1 && params.MipLevels == 0 &&
                params.Faces == 1 && (m_ImageCreateInfo.usage & VK_IMAGE_USAGE_SAMPLED_BIT) != 0)
            {
                if (!m_SupportsGpuWrites)
                {
                    m_DirectlyMappable = true;
                    tiling = VK_IMAGE_TILING_LINEAR;
                    layout = VK_IMAGE_LAYOUT_PREINITIALIZED;
                }
            }
        }

        uint32_t width = std::max(m_Params.Width, 1U);
        uint32_t height = std::max(m_Params.Height, 1U);
        uint32_t depth = std::max(m_Params.Depth, 1U);

        m_ImageCreateInfo.extent = { width, height, depth };
        m_ImageCreateInfo.mipLevels = params.MipLevels + 1;
        m_ImageCreateInfo.arrayLayers = params.Faces;
        m_ImageCreateInfo.samples = VulkanUtils::GetSampleFlags(params.Samples);
        m_ImageCreateInfo.tiling = tiling;
        m_ImageCreateInfo.initialLayout = layout;
        m_ImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        m_ImageCreateInfo.queueFamilyIndexCount = 0;
        m_ImageCreateInfo.pQueueFamilyIndices = nullptr;

        VulkanDevice& device = *gVulkanRenderAPI().GetPresentDevice().get();

        bool optimalTiling = tiling == VK_IMAGE_TILING_OPTIMAL;
        m_InternalFormat = VulkanUtils::GetClosestSupportedTextureFormat(device, params.Format, params.Shape,
                                                                         params.Usage, optimalTiling);
        m_Image = CreateImage(device, m_InternalFormat);
    }
    VulkanTexture::~VulkanTexture()
    {
        m_Image->Destroy();
        CW_ENGINE_ASSERT(m_StagingBuffer == nullptr);
    }

    VulkanImage* VulkanTexture::CreateImage(VulkanDevice& device, TextureFormat format)
    {
        bool directlyMappable = m_ImageCreateInfo.tiling == VK_IMAGE_TILING_LINEAR;
        VkMemoryPropertyFlags memoryFlags;
        if (directlyMappable)
            memoryFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        else
            memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        m_ImageCreateInfo.format = VulkanUtils::GetTextureFormat(format, false); // TODO: Fix this somehow.
        VkImage image;
        VkResult result = vkCreateImage(device.GetLogicalDevice(), &m_ImageCreateInfo, gVulkanAllocator, &image);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
        
        VmaAllocation allocation = device.AllocateMemory(image, memoryFlags);
        return device.GetResourceManager().Create<VulkanImage>(image, allocation, m_ImageCreateInfo.initialLayout,
                                                               m_ImageCreateInfo.format, m_Params);
    }

    VulkanBuffer* VulkanTexture::CreateStagingBuffer(VulkanDevice& device, const PixelData& data, bool readable)
    {
        uint32_t blockSize = PixelUtils::GetBlockSize(data.GetFormat());
        VkBufferCreateInfo bufferCreateInfo;
        bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCreateInfo.pNext = nullptr;
        bufferCreateInfo.flags = 0;
        bufferCreateInfo.size = data.GetSize();
        bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferCreateInfo.queueFamilyIndexCount = 0;
        bufferCreateInfo.pQueueFamilyIndices = nullptr;

        if (readable)
            bufferCreateInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        VkBuffer buffer;
        VkResult result = vkCreateBuffer(device.GetLogicalDevice(), &bufferCreateInfo, gVulkanAllocator, &buffer);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);

        VkMemoryPropertyFlags flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        VmaAllocation allocation = device.AllocateMemory(buffer, flags);

        uint32_t rowPitchInPixels = data.GetRowPitch() / blockSize;
        uint32_t slicePitchInPixels = data.GetSlicePitch() / blockSize;
        // TODO: Texture compression.

        return device.GetResourceManager().Create<VulkanBuffer>(buffer, allocation, rowPitchInPixels,
                                                                slicePitchInPixels);
    }

    void VulkanTexture::CopyImage(VulkanTransferBuffer* cb, VulkanImage* srcImage, VulkanImage* dstImage,
                                  VkImageLayout srcFinalLayout, VkImageLayout dstFinalLayout)
    {
        uint32_t numFaces = m_Params.Faces;
        uint32_t numMips = m_Params.MipLevels;

        uint32_t mipWidth = m_Params.Width;
        uint32_t mipHeight = m_Params.Height;
        uint32_t mipDepth = m_Params.Depth;

        VkImageCopy* imageRegions = new VkImageCopy[numMips];
        for (uint32_t i = 0; i < numMips; i++)
        {
            VkImageCopy& region = imageRegions[i];
            region.srcOffset = { 0, 0, 0 };
            region.dstOffset = { 0, 0, 0 };
            region.extent = { mipWidth, mipHeight, mipDepth };
            region.srcSubresource.baseArrayLayer = 0;
            region.srcSubresource.layerCount = numFaces;
            region.srcSubresource.mipLevel = i;
            region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.srcSubresource.baseArrayLayer = 0;
            region.srcSubresource.layerCount = numFaces;
            region.srcSubresource.mipLevel = i;
            region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

            if (mipWidth != 1)
                mipWidth /= 2;
            if (mipHeight != 1)
                mipHeight /= 2;
            if (mipDepth != 1)
                mipDepth /= 2;
        }

        VkImageSubresourceRange range;
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range.baseArrayLayer = 0;
        range.layerCount = numFaces;
        range.baseMipLevel = 0;
        range.levelCount = numMips;

        VkImageLayout transferSrcLayout, transferDstLayout;
        if (m_DirectlyMappable)
        {
            transferSrcLayout = VK_IMAGE_LAYOUT_GENERAL;
            transferDstLayout = VK_IMAGE_LAYOUT_GENERAL;
        }
        else
        {
            transferSrcLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            transferDstLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        }

        cb->SetLayout(srcImage, range, VK_ACCESS_TRANSFER_READ_BIT, transferSrcLayout);
        cb->SetLayout(dstImage, range, VK_ACCESS_TRANSFER_WRITE_BIT, transferDstLayout);
        vkCmdCopyImage(cb->GetCB()->GetHandle(), srcImage->GetHandle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       dstImage->GetHandle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, numMips, imageRegions);

        VkAccessFlags srcAccessMask = srcImage->GetAccessFlags(srcFinalLayout);
        cb->SetLayout(srcImage->GetHandle(), VK_ACCESS_TRANSFER_READ_BIT, srcAccessMask, transferSrcLayout,
                      srcFinalLayout, range);

        VkAccessFlags dstAccessMask = dstImage->GetAccessFlags(dstFinalLayout);
        cb->SetLayout(dstImage->GetHandle(), VK_ACCESS_TRANSFER_WRITE_BIT, dstAccessMask, transferDstLayout,
                      dstFinalLayout, range);
        cb->GetCB()->RegisterImageTransfer(srcImage, range, srcFinalLayout, VulkanAccessFlagBits::Read);
        cb->GetCB()->RegisterImageTransfer(dstImage, range, dstFinalLayout, VulkanAccessFlagBits::Write);
        delete[] imageRegions;
    }

    // void VulkanTexture::Copy(const Ref<Texture>& tranfer, const Te)

    PixelData VulkanTexture::Lock(GpuLockOptions options, uint32_t mipLevel, uint32_t face, uint32_t queueIdx)
    {
        uint32_t mipWidth = std::max(1U, m_Params.Width >> mipLevel);
        uint32_t mipHeight = std::max(1U, m_Params.Height >> mipLevel);
        uint32_t mipDepth = std::max(1U, m_Params.Depth >> mipLevel);

        m_IsMapped = true;
        m_MappedGlobalQueueIdx = queueIdx;
        m_MappedFace = face;
        m_MappedMip = mipLevel;
        m_MappedLockOptions = options;

        PixelData lockedData(mipWidth, mipHeight, mipDepth, m_InternalFormat);
        VulkanDevice& device = *gVulkanRenderAPI().GetPresentDevice().get();
        VulkanTransferManager& vtm = VulkanTransferManager::Get();
        GpuQueueType queueType;
        uint32_t localQueueIdx = CommandSyncMask::GetQueueIdxAndType(queueIdx, queueType);
        VulkanImageSubresource* subresource = m_Image->GetSubresource(face, mipLevel);

        if (m_DirectlyMappable)
        {
            CW_ENGINE_ASSERT(subresource->GetLayout() == VK_IMAGE_LAYOUT_PREINITIALIZED ||
                             subresource->GetLayout() == VK_IMAGE_LAYOUT_GENERAL);
            CW_ENGINE_ASSERT(!m_SupportsGpuWrites);

            uint32_t useMask = subresource->GetUseInfo(VulkanAccessFlagBits::Read);
            bool isUsedOnGpu = useMask != 0;
            if (!isUsedOnGpu)
            {
                if (subresource->IsBound())
                {
                    VulkanImage* newImage = CreateImage(device, m_InternalFormat);
                    if (options != GpuLockOptions::WRITE_ONLY_NO_OVERWRITE)
                    {
                        VkMemoryRequirements memReqs;
                        vkGetImageMemoryRequirements(device.GetLogicalDevice(), m_Image->GetHandle(), &memReqs);
                        uint8_t* src = m_Image->Map(0, (uint32_t)memReqs.size);
                        uint8_t* dst = newImage->Map(0, (uint32_t)memReqs.size);
                        std::memcpy(dst, src, memReqs.size);

                        m_Image->Unmap();
                        newImage->Unmap();
                    }

                    m_Image->Destroy();
                    m_Image = newImage;
                }

                m_Image->Map(face, mipLevel, lockedData);
                return lockedData;
            }

            if (options == GpuLockOptions::WRITE_ONLY_NO_OVERWRITE)
            {
                m_Image->Map(face, mipLevel, lockedData);
                return lockedData;
            }

            if (options == GpuLockOptions::WRITE_DISCARD)
            {
                m_Image->Destroy();
                m_Image = CreateImage(device, m_InternalFormat);
                m_Image->Map(face, mipLevel, lockedData);
                return lockedData;
            }

            if (options == GpuLockOptions::READ_ONLY || options == GpuLockOptions::READ_WRITE)
            {
                VulkanTransferBuffer* transferCB = vtm.GetTransferBuffer(queueType, localQueueIdx);
                if (options == GpuLockOptions::READ_ONLY)
                    useMask = subresource->GetUseInfo(VulkanAccessFlagBits::Write);
                else
                    useMask = subresource->GetUseInfo(VulkanAccessFlagBits::Read | VulkanAccessFlagBits::Write);

                transferCB->AppendMask(useMask);
                transferCB->Flush(true);

                if (options == GpuLockOptions::READ_WRITE && subresource->IsBound())
                {
                    VulkanImage* newImage = CreateImage(device, m_InternalFormat);
                    VkMemoryRequirements memReqs;
                    vkGetImageMemoryRequirements(device.GetLogicalDevice(), m_Image->GetHandle(), &memReqs);

                    uint8_t* src = m_Image->Map(0, (uint32_t)memReqs.size);
                    uint8_t* dst = newImage->Map(0, (uint32_t)memReqs.size);
                    std::memcpy(dst, src, memReqs.size);
                    m_Image->Unmap();
                    newImage->Unmap();

                    m_Image->Destroy();
                    m_Image = newImage;
                }

                m_Image->Map(face, mipLevel, lockedData);
                return lockedData;
            }
        }

        bool needRead = options != GpuLockOptions::WRITE_DISCARD && options != GpuLockOptions::WRITE_DISCARD_RANGE;
        m_StagingBuffer = CreateStagingBuffer(device, lockedData, needRead);
        if (needRead)
        {
            VulkanTransferBuffer* transferCB = vtm.GetTransferBuffer(queueType, localQueueIdx);
            uint32_t writeUseMask = subresource->GetUseInfo(VulkanAccessFlagBits::Write);
            if (m_SupportsGpuWrites || writeUseMask != 0)
                transferCB->AppendMask(writeUseMask);

            VkImageSubresourceRange range;
            range.aspectMask = m_Image->GetAspectFlags();
            range.baseArrayLayer = face;
            range.layerCount = 1;
            range.baseMipLevel = mipLevel;
            range.levelCount = 1;

            VkImageSubresourceLayers rangeLayers;
            if ((m_Params.Usage & TEXTURE_DEPTHSTENCIL) != 0)
                rangeLayers.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            else
                rangeLayers.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

            rangeLayers.baseArrayLayer = range.baseArrayLayer;
            rangeLayers.layerCount = range.layerCount;
            rangeLayers.mipLevel = range.baseMipLevel;

            VkExtent3D extent;
            PixelUtils::GetMipSizeForLevel(m_Params.Width, m_Params.Height, m_Params.Depth, m_MappedMip, extent.width,
                                           extent.height, extent.depth);

            VkAccessFlags currentAccessMask = m_Image->GetAccessFlags(subresource->GetLayout());
            transferCB->SetLayout(m_Image->GetHandle(), currentAccessMask, VK_ACCESS_TRANSFER_READ_BIT,
                                  subresource->GetLayout(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, range);

            m_Image->Copy(transferCB, m_StagingBuffer, extent, rangeLayers, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            VkImageLayout dstLayout = m_Image->GetOptimalLayout();
            currentAccessMask = m_Image->GetAccessFlags(dstLayout);

            transferCB->SetLayout(m_Image->GetHandle(), VK_ACCESS_TRANSFER_READ_BIT, currentAccessMask,
                                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstLayout, range);
            transferCB->GetCB()->RegisterImageTransfer(m_Image, range, dstLayout, VulkanAccessFlagBits::Read);

            VkAccessFlags stagingAccessFlags;
            if (options == GpuLockOptions::READ_ONLY)
                stagingAccessFlags = VK_ACCESS_HOST_READ_BIT;
            else
                stagingAccessFlags = VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT;

            transferCB->MemoryBarrier(m_StagingBuffer->GetHandle(), VK_ACCESS_TRANSFER_WRITE_BIT, stagingAccessFlags,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT);

            transferCB->Flush(true);
        }

        uint8_t* data = m_StagingBuffer->Map(0, lockedData.GetSize());
        lockedData.SetBuffer(data);
        return lockedData;
    }

    void VulkanTexture::Unlock()
    {
        if (!m_IsMapped)
            return;
        if (m_StagingBuffer == nullptr)
            m_Image->Unmap();
        else
        {
            m_StagingBuffer->Unmap();
            bool isWrite = m_MappedLockOptions != GpuLockOptions::READ_ONLY;
            if (isWrite)
            {
                VulkanDevice& device = *gVulkanRenderAPI().GetPresentDevice().get();
                VulkanTransferManager& vtm = VulkanTransferManager::Get();
                GpuQueueType queueType;
                uint32_t localQueueIdx = CommandSyncMask::GetQueueIdxAndType(m_MappedGlobalQueueIdx, queueType);

                VulkanImage* image = m_Image;
                VulkanTransferBuffer* transferCB = vtm.GetTransferBuffer(queueType, localQueueIdx);
                VulkanImageSubresource* subresource = image->GetSubresource(m_MappedFace, m_MappedMip);
                VkImageLayout curLayout = subresource->GetLayout();

                uint32_t useMask = subresource->GetUseInfo(VulkanAccessFlagBits::Read | VulkanAccessFlagBits::Write);
                bool isNormalWrite = false;
                if (useMask != 0)
                {
                    if (m_MappedLockOptions == GpuLockOptions::WRITE_ONLY_NO_OVERWRITE)
                    {
                    }
                    else if (m_MappedLockOptions == GpuLockOptions::WRITE_DISCARD)
                    {
                        image->Destroy();
                        image = CreateImage(device, m_InternalFormat);
                        subresource = image->GetSubresource(m_MappedFace, m_MappedMip);
                    }
                    else
                    {
                        transferCB->AppendMask(useMask);
                        isNormalWrite = true;
                    }
                }
                else
                    isNormalWrite = true;

                if (isNormalWrite)
                {
                    uint32_t useCount = subresource->GetUseCount();
                    uint32_t boundCount = subresource->GetBoundCount();
                    bool isBoundWithoutUse = boundCount > useCount;
                    if (isBoundWithoutUse)
                    {
                        VulkanImage* newImage = CreateImage(device, m_InternalFormat);
                        if (m_Params.MipLevels > 0 || m_Params.Faces > 1)
                        {
                            VkImageLayout oldImageLayout = image->GetOptimalLayout();
                            curLayout = newImage->GetOptimalLayout();
                            CopyImage(transferCB, image, newImage, oldImageLayout, curLayout);
                        }

                        image->Destroy();
                        image = newImage;
                        m_Image = image;
                    }
                }

                VkImageSubresourceRange range;
                range.aspectMask = image->GetAspectFlags();
                range.baseArrayLayer = m_MappedFace;
                range.layerCount = 1;
                range.baseMipLevel = m_MappedMip;
                range.levelCount = 1;

                VkImageSubresourceLayers rangeLayers;
                rangeLayers.aspectMask = range.aspectMask;
                rangeLayers.baseArrayLayer = range.baseArrayLayer;
                rangeLayers.layerCount = range.layerCount;
                rangeLayers.mipLevel = range.baseMipLevel;

                VkExtent3D extent;
                PixelUtils::GetMipSizeForLevel(m_Params.Width, m_Params.Height, m_Params.Depth, m_MappedMip,
                                               extent.width, extent.height, extent.depth);

                VkImageLayout transferLayout;
                if (m_DirectlyMappable)
                    transferLayout = VK_IMAGE_LAYOUT_GENERAL;
                else
                    transferLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

                VkAccessFlags currentAccessMask = image->GetAccessFlags(curLayout);
                transferCB->SetLayout(image->GetHandle(), currentAccessMask, VK_ACCESS_TRANSFER_WRITE_BIT, curLayout,
                                      transferLayout, range);

                m_StagingBuffer->Copy(transferCB->GetCB(), image, extent, rangeLayers, transferLayout);
                VkImageLayout dstLayout = image->GetOptimalLayout();
                currentAccessMask = image->GetAccessFlags(dstLayout);
                transferCB->SetLayout(image->GetHandle(), VK_ACCESS_TRANSFER_WRITE_BIT, currentAccessMask,
                                      transferLayout, dstLayout, range);

                transferCB->GetCB()->RegisterBuffer(m_StagingBuffer, BufferUseFlagBits::Transfer,
                                                    VulkanAccessFlagBits::Read);
                transferCB->GetCB()->RegisterImageTransfer(image, range, dstLayout, VulkanAccessFlagBits::Write);
            }
            m_StagingBuffer->Destroy();
            m_StagingBuffer = nullptr;
        }

        m_IsMapped = false;
    }

    void VulkanTexture::ReadData(PixelData& dest, uint32_t mipLevel, uint32_t face, uint32_t queueIdx)
    {
        PixelData data = Lock(GpuLockOptions::READ_ONLY, mipLevel, face, queueIdx);
        std::memcpy(dest.GetData(), data.GetData(), dest.GetSize());
        data.SetBuffer(nullptr); // TODO: temp fix.
        Unlock();
    }

    void VulkanTexture::WriteData(const PixelData& src, uint32_t mipLevel, uint32_t face, uint32_t queueIdx)
    {
        mipLevel = glm::clamp(mipLevel, (uint32_t)mipLevel, m_Params.MipLevels);
        face = glm::clamp(face, (uint32_t)0, m_Params.Faces - 1);

        PixelData data =
          Lock(/*discardWholeBuffer ? */ GpuLockOptions::WRITE_DISCARD /* : GpuLockOptions::WRITE_DISCARD_RANGE*/,
               mipLevel, face, queueIdx);
        std::memcpy(data.GetData(), src.GetData(), src.GetSize());
        data.SetBuffer(nullptr); // TODO: temp fix.
        Unlock();
    }
} // namespace Crowny