#pragma once

#include "Platform/Vulkan/VulkanUtils.h"

#include "Platform/Vulkan/VulkanDevice.h"

#include "Crowny/Renderer/GpuBuffer.h"

namespace Crowny
{

	class VulkanBuffer
    {
    public:
        VulkanBuffer(VulkanDevice& device, VkBuffer buffer, VmaAllocation allocation);
        ~VulkanBuffer();
        VkBuffer GetHandle() const { return m_Buffer; }
		uint8_t* Map(VkDeviceSize offset, VkDeviceSize length) const;
		void Unmap();
		void Copy(VulkanCommandBuffer* cb, VulkanBuffer* dest, VkDeviceSize srcOffset, VkDeviceSize dstOffset, VkDeviceSize length);
		void Update(VulkanCommandBuffer* buffer, uint8_t* data, VkDeviceSize offset, VkDeviceSize length);
		
    private:
		VulkanDevice& m_Device;
        VkBuffer m_Buffer;
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
        
		virtual void* Map(uint32_t offset, uint32_t length, GpuLockOptions options) override;
		virtual void Unmap() override;
		
		VkBuffer GetHandle() const { return m_Buffer->GetHandle(); }
		VulkanBuffer* GetBuffer() const { return m_Buffer; }
		
	private:
		VulkanBuffer* CreateBuffer(VulkanDevice& device, uint32_t size, bool staging, bool readable);
	
	private:
		VkBufferCreateInfo m_BufferCreateInfo{};
		VkBufferUsageFlags m_UsageFlags;
		uint8_t* m_StagingMemory;
		bool m_DirectlyMappable : 1;
		bool m_SupportsGpuWrites : 1;
		bool m_IsMapped : 1;
		GpuLockOptions m_MappedLockOptions;
		uint32_t m_MappedSize;
		uint32_t m_MappedOffset;
		VulkanBuffer* m_StagingBuffer;
		
		VulkanBuffer* m_Buffer;
		BufferLayout m_Layout;
    };
    
}        