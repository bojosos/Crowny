#include "cwpch.h"

#include "Crowny/Renderer/GpuBuffer.h"

namespace Crowny
{

    GpuBuffer::GpuBuffer(uint32_t size, BufferUsage usage) : m_Size(size), m_Usage(usage) { }

    void* GpuBuffer::Lock(uint32_t offset, uint32_t length, GpuLockOptions options)
    {
        CW_ENGINE_ASSERT(!IsLocked());
        void* data = Map(offset, length, options);
        m_IsLocked = true;
        return data;
    }

    void GpuBuffer::Unlock()
    {
        CW_ENGINE_ASSERT(IsLocked());
        Unmap();
        m_IsLocked = false;
    }

    void GpuBuffer::CopyData(GpuBuffer& src, const Ref<CommandBuffer>& cmdBuffer)
    {
        uint32_t size = std::min(m_Size, src.GetSize());
        CopyData(src, 0, 0, size, true, cmdBuffer);
    }

}