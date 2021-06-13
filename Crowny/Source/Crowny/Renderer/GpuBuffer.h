#pragma once

namespace Crowny
{
    class GpuBuffer
    {
    public:
        virtual ~GpuBuffer() = default;
        virtual void* Map(uint32_t offset, uint32_t length, GpuLockOptions options) = 0;
		virtual void Unmap() = 0;
        
    protected:
        GpuBuffer(uint32_t size, BufferUsage usage) : m_Size(size), m_Usage(usage) { }
        
    protected:
        uint32_t m_Size;
        BuferUsage m_Usage;
    };
}