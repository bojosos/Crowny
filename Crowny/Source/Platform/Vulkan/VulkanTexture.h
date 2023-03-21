#pragma once

#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Utils/PixelUtils.h"

#include "Platform/Vulkan/VulkanGpuBuffer.h"
#include "Platform/Vulkan/VulkanResource.h"

namespace Crowny
{

    class VulkanTransferBuffer;
    class VulkanBuffer;

    struct VulkanImageDesc
    {
        VkImage Image;
        VmaAllocation Allocation;
        VkImageLayout Layout;
        VkFormat Format;
        uint32_t Faces;
        uint32_t NumMips;
        uint32_t Usage;
        TextureShape Shape;
    };

    class VulkanImageSubresource : public VulkanResource
    {
    public:
        VulkanImageSubresource(VulkanResourceManager* owner, VkImageLayout layout);
        VkImageLayout GetLayout() const
        {
            CW_ENGINE_ASSERT(m_Layout != 96);
            return m_Layout;
        }
        void SetLayout(VkImageLayout layout)
        { /*CW_ENGINE_INFO(layout);*/
            m_Layout = layout;
        }

    private:
        VkImageLayout m_Layout = VK_IMAGE_LAYOUT_UNDEFINED;
    };

    class VulkanImage : public VulkanResource
    {
    public:
        VulkanImage(VulkanResourceManager* owner, VkImage image, VmaAllocation allocation, VkImageLayout layout,
                    VkFormat format, const TextureParameters& params, bool ownsImage = true);
        VulkanImage(VulkanResourceManager* owner, const VulkanImageDesc& desc, bool ownsImage = true);
        ~VulkanImage();

        VkImage GetHandle() const { return m_Image; }

        VkImageView GetView(bool framebuffer) const;
        VkImageView GetView(VkFormat format, bool framebuffer) const;
        VkImageView GetView(const TextureSurface& surface, bool framebuffer) const;
        VkImageView GetView(VkFormat format, const TextureSurface& surface, bool framebuffer) const;

        VkImageAspectFlags GetAspectFlags() const;
        VkImageLayout GetOptimalLayout() const;
        VkImageSubresourceRange GetRange() const;
        VkImageSubresourceRange GetRange(const TextureSurface& surface) const;
        VulkanImageSubresource* GetSubresource(uint32_t face, uint32_t mipLevel);

        void Map(uint32_t face, uint32_t mipLevel, PixelData& output) const;
        uint8_t* Map(uint32_t offset, uint32_t size) const;
        void Unmap();
        void Copy(VulkanTransferBuffer* cb, VulkanBuffer* dest, const VkExtent3D& extent,
                  const VkImageSubresourceLayers& range, VkImageLayout layout);

        VkAccessFlags GetAccessFlags(VkImageLayout layout, bool readonly = false);
        void GetBarriers(const VkImageSubresourceRange& range, Vector<VkImageMemoryBarrier>& barriers);

    private:
        VkImageView CreateView(const TextureSurface& surface, VkFormat format, VkImageAspectFlags mask) const;

        struct ImageViewInfo
        {
            TextureSurface Surface;
            bool Framebuffer;
            VkImageView View;
            VkFormat Format;
        };

        VkImage m_Image;
        VmaAllocation m_Allocation;
        VkImageView m_MainView;
        VkImageView m_FramebufferMainView;
        int32_t m_Usage;
        bool m_OwnsImage;
        uint32_t m_NumFaces;
        uint32_t m_NumMipLevels;
        VulkanImageSubresource** m_Subresources;

        mutable VkImageViewCreateInfo m_ImageViewCreateInfo;
        mutable Vector<ImageViewInfo> m_ImageInfos;
    };

    class VulkanTexture : public Texture
    {
    public:
        VulkanTexture(const TextureParameters& parameters);
        ~VulkanTexture();

        virtual PixelData Lock(GpuLockOptions options, uint32_t mipLevel = 0, uint32_t face = 0,
                               uint32_t queueIdx = 0) override;
        virtual void Unlock() override;
        // virtual void Copy(const Ref<Texture>& target, )
        virtual void ReadData(PixelData& dest, uint32_t mipLevel = 0, uint32_t face = 0,
                              uint32_t queueIdx = 0) override;
        virtual void WriteData(const PixelData& src, uint32_t mipLevel = 0, uint32_t face = 0,
                               uint32_t queueIdx = 0) override;

        VulkanImage* GetImage() const { return m_Image; }

    private:
        friend class cereal::access;

        VulkanTexture(); // For serialization only
        void Init();

        VulkanImage* CreateImage(VulkanDevice& device, TextureFormat format);
        VulkanBuffer* CreateStagingBuffer(VulkanDevice& device, const PixelData& src, bool needsRead);
        void CopyImage(VulkanTransferBuffer* cb, VulkanImage* srcImage, VulkanImage* dstImage,
                       VkImageLayout srcFinalLayout, VkImageLayout dstFinalLayout);

        VulkanImage* m_Image;
        TextureFormat m_InternalFormat;
        VulkanBuffer* m_StagingBuffer;
        uint32_t m_MappedGlobalQueueIdx;
        uint32_t m_MappedMip;
        uint32_t m_MappedFace;
        uint32_t m_MappedRowPitch;
        uint32_t m_MappedSlicePitch;
        GpuLockOptions m_MappedLockOptions;

        VkImageCreateInfo m_ImageCreateInfo;
        bool m_DirectlyMappable : 1;
        bool m_SupportsGpuWrites : 1;
        bool m_IsMapped : 1;
    };

} // namespace Crowny