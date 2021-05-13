#include "cwpch.h"

#include "Platform/Vulkan/VulkanVertexBuffer.h"

namespace Crowny
{

    VulkanVertexBuffer::VulkanVertexBuffer(uint32_t size, BufferUsage usage) : m_Usage(usage)
    {
        m_Buffer = new VulkanGpuBuffer(VulkanGpuBuffer::BUFFER_VERTEX, usage, size);
    }

    VulkanVertexBuffer::VulkanVertexBuffer(void* vertices, uint32_t size, BufferUsage usage) : m_Usage(usage)
    {
        m_Buffer = new VulkanGpuBuffer(VulkanGpuBuffer::BUFFER_VERTEX, usage, size);
        void* dest = m_Buffer->Map(0, size, GpuLockOptions::WRITE_DISCARD);
        memcpy(dest, vertices, size);
        m_Buffer->Unmap();
    }

    VulkanVertexBuffer::~VulkanVertexBuffer()
    {
        delete m_Buffer;
    }
    
    void* VulkanVertexBuffer::Map(uint32_t offset, uint32_t size, GpuLockOptions options)
    {
        return m_Buffer->Map(offset, size, options);
    }

    void VulkanVertexBuffer::Unmap()
    {
        m_Buffer->Unmap();
    }

}