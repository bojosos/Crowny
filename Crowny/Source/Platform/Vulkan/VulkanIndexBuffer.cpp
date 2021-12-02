#include "cwpch.h"

#include "Platform/Vulkan/VulkanIndexBuffer.h"

namespace Crowny
{

    VulkanIndexBuffer::VulkanIndexBuffer(uint32_t count, IndexType indexType, BufferUsage usage)
      : m_Count(count), m_IndexType(indexType)
    {
        m_Buffer =
          new VulkanGpuBuffer(VulkanGpuBuffer::BUFFER_INDEX, usage,
                              indexType == IndexType::Index_16 ? sizeof(uint16_t) * count : sizeof(uint32_t) * count);
    }

    VulkanIndexBuffer::VulkanIndexBuffer(uint16_t* indices, uint32_t count, BufferUsage usage)
      : m_Count(count), m_IndexType(IndexType::Index_16)
    {
        m_Buffer = new VulkanGpuBuffer(VulkanGpuBuffer::BUFFER_INDEX, usage, (uint32_t)(sizeof(uint16_t) * count));
        void* dest = m_Buffer->Map(0, count * sizeof(uint16_t), GpuLockOptions::WRITE_DISCARD);
        memcpy(dest, indices, count * sizeof(uint16_t));
        m_Buffer->Unmap();
    }

    VulkanIndexBuffer::VulkanIndexBuffer(uint32_t* indices, uint32_t count, BufferUsage usage)
      : m_Count(count), m_IndexType(IndexType::Index_32)
    {
        m_Buffer = new VulkanGpuBuffer(VulkanGpuBuffer::BUFFER_INDEX, usage, (uint32_t)(sizeof(uint32_t) * count));
        void* dest = m_Buffer->Map(0, count * sizeof(uint32_t), GpuLockOptions::WRITE_DISCARD);
        memcpy(dest, indices, count * sizeof(uint32_t));
        m_Buffer->Unmap();
    }

    VulkanIndexBuffer::~VulkanIndexBuffer() { delete m_Buffer; }

    void* VulkanIndexBuffer::Map(uint32_t offset, uint32_t size, GpuLockOptions options)
    {
        return m_Buffer->Map(offset, size, options);
    }

    void VulkanIndexBuffer::Unmap() { m_Buffer->Unmap(); }

} // namespace Crowny