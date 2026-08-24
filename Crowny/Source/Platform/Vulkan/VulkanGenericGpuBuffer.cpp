#include "cwpch.h"

#include "Platform/Vulkan/VulkanGenericGpuBuffer.h"

namespace Crowny
{
    VulkanGenericGpuBuffer::VulkanGenericGpuBuffer(uint32_t elementCount, uint32_t elementSize, GpuBufferType type, GpuBufferFormat format,
                                                   BufferUsage usage)
      : m_Usage(usage), m_BufferType(type), m_BufferFormat(format)
    {
        VulkanGpuBuffer::BufferType bufferType = VulkanGpuBuffer::BUFFER_GENERIC;
        if (type == GpuBufferType::Structured)
            bufferType = VulkanGpuBuffer::BUFFER_STRUCTURED;
        else if (type == GpuBufferType::IndirectDraw)
            bufferType = VulkanGpuBuffer::BUFFER_INDIRECT;
        m_Buffer = new VulkanGpuBuffer(bufferType, usage, elementSize * elementCount);
        UpdateViews();
    }

    VulkanGenericGpuBuffer::~VulkanGenericGpuBuffer()
    {
        if (m_Buffer && m_BufferView != VK_NULL_HANDLE)
        {
            VulkanBuffer* buffer = m_Buffer->GetBuffer();
            buffer->FreeView(m_BufferView);
        }
    }

    void VulkanGenericGpuBuffer::WriteData(uint32_t offset, uint32_t length, const void* src, BufferWriteOptions writeOptions /* = BWT_NORMAL */)
    {
        m_Buffer->WriteData(offset, length, src, writeOptions);
        UpdateViews();
    }

    void VulkanGenericGpuBuffer::ReadData(uint32_t offset, uint32_t length, void* dest)
    {
        m_Buffer->ReadData(offset, length, dest);
        UpdateViews();
    }

    void* VulkanGenericGpuBuffer::Map(uint32_t offset, uint32_t length, GpuLockOptions options)
    {
        void* data = m_Buffer->Map(offset, length, options);
        UpdateViews();
        return data;
    }

    void VulkanGenericGpuBuffer::Unmap()
    {
        m_Buffer->Unmap();
        UpdateViews();
    }

    void VulkanGenericGpuBuffer::UpdateViews()
    {
        if (m_BufferType == GpuBufferType::Structured)
            return;
        VulkanBuffer* buffer = m_Buffer->GetBuffer();
        VkBuffer newBufferHandle = VK_NULL_HANDLE;
        if (buffer)
            newBufferHandle = buffer->GetHandle();
        if (m_CacheBuffer != newBufferHandle)
        {
            if (newBufferHandle == VK_NULL_HANDLE)
                m_BufferView = buffer->GetView(VulkanUtils::GetBufferFormat(m_BufferFormat));
            else
                m_BufferView = VK_NULL_HANDLE;
            m_CacheBuffer = newBufferHandle;
        }
    }

} // namespace Crowny
