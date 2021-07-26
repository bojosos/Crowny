#pragma once

#include "Platform/Vulkan/VulkanUtils.h"

#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanResource.h"

#include "Crowny/Renderer/GpuBuffer.h"

namespace Crowny
{

	class VulkanCmdBuffer;
	class VulkanImage;

	class VulkanBuffer : public VulkanResource
    {
    public:
        VulkanBuffer(VulkanResourceManager* owner, VkBuffer buffer, VmaAllocation allocation, uint32_t rowPitch = 0, uint32_t slicePitch = 0);
        ~VulkanBuffer();
        VkBuffer GetHandle() const { return m_Buffer; }
		uint8_t* Map(VkDeviceSize offset, VkDeviceSize length) const;
		uint32_t GetRowPitch() const { return m_RowPitch; }
		uint32_t GetSliceHeight() const { return m_SliceHeight; }
		void Unmap();
		void Copy(VulkanCmdBuffer* cb, VulkanBuffer* dest, VkDeviceSize srcOffset, VkDeviceSize dstOffset, VkDeviceSize length);
		void Copy(VulkanCmdBuffer* cmdBuffer, VulkanImage* dest, const VkExtent3D& extent, const VkImageSubresourceLayers& range, VkImageLayout layout);
		void Update(VulkanCmdBuffer* buffer, uint8_t* data, VkDeviceSize offset, VkDeviceSize length);
		virtual void NotifyDone(uint32_t globalQueue, VulkanAccessFlags useFlags) override;
		virtual void NotifyUnbound() override;
		VkBufferView GetView(VkFormat format);
		void FreeView(VkBufferView view);
    private:
		struct ViewInfo
		{
			ViewInfo() = default;
			ViewInfo(VkFormat format, VkBufferView view)
				: Format(format), View(view), UseCount(1) { }

			VkFormat Format = VK_FORMAT_UNDEFINED;
			VkBufferView View = VK_NULL_HANDLE;
			uint32_t UseCount = 0;
		};

		void DestroyUnusedViews();

        VkBuffer m_Buffer;
		std::vector<ViewInfo> m_Views;
		uint32_t m_RowPitch;
		uint32_t m_SliceHeight;
        VmaAllocation m_Allocation;
    };
    
    class VulkanGpuBuffer : public GpuBuffer
    {
    public:
        enum BufferType
        {
            BUFFER_VERTEX,
            BUFFER_INDEX,
            BUFFER_UNIFORM
        };

        VulkanGpuBuffer(BufferType type, BufferUsage usage, uint32_t size);
		~VulkanGpuBuffer();
    
	public:
		virtual void WriteData(uint32_t offset, uint32_t length, const void* src, BufferWriteOptions writeOpts = BWT_NORMAL) override;
		virtual void ReadData(uint32_t offset, uint32_t length, void* dest) override;
		virtual void CopyData(GpuBuffer& src, uint32_t srcOffset, uint32_t dstOffset, uint32_t length, bool discard = false, const Ref<CommandBuffer>& cmdBuffer = nullptr) override;
		virtual void* Map(uint32_t offset, uint32_t length, GpuLockOptions options, uint32_t queueIdx = 0) override;
		virtual void Unmap() override;
		
		VkBuffer GetHandle() const { return m_Buffer->GetHandle(); }
		VulkanBuffer* GetBuffer() const { return m_Buffer; }
	
	private:
		VulkanBuffer* CreateBuffer(VulkanDevice& device, uint32_t size, bool staging, bool readable);
	
		VkBufferCreateInfo m_BufferCreateInfo{};
		VkBufferUsageFlags m_UsageFlags;
		uint8_t* m_StagingMemory;
		bool m_DirectlyMappable : 1;
		bool m_SupportsGpuWrites : 1;
		bool m_IsMapped : 1;
		GpuLockOptions m_MappedLockOptions;
		uint32_t m_MappedSize;
		uint32_t m_MappedOffset;
		uint32_t m_MappedGlobalQueueIdx;
		VulkanBuffer* m_StagingBuffer;
		
		VulkanBuffer* m_Buffer;
  };
    
}        
