#pragma once

#include "Platform/Vulkan/VulkanUtils.h"

#include "Crowny/Renderer/VertexBuffer.h"

namespace Crowny
{
    
    class VulkanVertexBuffer : public VertexBuffer
    {
    public:
        VulkanVertexBuffer(void* data, uint32_t size, const VertexBufferProperties& props);
		~VulkanVertexBuffer();

		virtual void Bind() const override;
		virtual void Unbind() const override;

		virtual const BufferLayout& GetLayout() const override { return m_Layout; };
		virtual void SetLayout(const BufferLayout& layout) override { m_Layout = layout; }
		virtual void SetData(void* verts, uint32_t size) override;

		virtual void* GetPointer(uint32_t size) const override;
		virtual void FreePointer() const override;
		VkBuffer GetHandle() const { return m_Buffer; }
	private:
		VkDeviceMemory m_Memory;
		VkDevice m_Device;
		VkBuffer m_Buffer;
		VertexBufferProperties m_Properties;
		uint32_t m_RendererID, m_Size;
		BufferLayout m_Layout;
    };
    
}        