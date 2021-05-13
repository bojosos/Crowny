#pragma once

#include "Platform/Vulkan/VulkanUtils.h"

#include "Platform/Vulkan/VulkanDevice.h"
#include "Crowny/Renderer/VertexBuffer.h"

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
    
    class VulkanVertexBuffer : public VertexBuffer
    {
    public:
        VulkanVertexBuffer(void* data, uint32_t size, const VertexBufferProperties& props);
		~VulkanVertexBuffer();

		virtual void Bind() const override {};
		virtual void Unbind() const override {};

		virtual const BufferLayout& GetLayout() const override { return m_Layout; };
		virtual void SetLayout(const BufferLayout& layout) override { m_Layout = layout; }
		virtual void SetData(void* verts, uint32_t size) override {};

		virtual void* GetPointer(uint32_t size) override;
		virtual void FreePointer() override;
		
		void ReadData(uint32_t offset, uint32_t length, void* dest);

		void* Map(uint32_t offset, uint32_t length, GpuLockOptions options);
		void Unmap();
		
		VkBuffer GetHandle() const { return m_Buffer->GetHandle(); }
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
		VertexBufferProperties m_Properties;
		uint32_t m_Size;
		BufferLayout m_Layout;
    };
    
}        