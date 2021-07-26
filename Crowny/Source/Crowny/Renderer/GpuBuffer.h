#pragma once

#include "Crowny/Renderer/CommandBuffer.h"

namespace Crowny
{
    class GpuBuffer
    {
    public:
        virtual ~GpuBuffer() = default;
        virtual void* Map(uint32_t offset, uint32_t length, GpuLockOptions options, uint32_t queueIdx = 0) { return nullptr; }
		virtual void Unmap() {};

        virtual void WriteData(uint32_t offset, uint32_t lenth, const void* src, BufferWriteOptions writeOptions = BWT_NORMAL) = 0;
        virtual void ReadData(uint32_t offset, uint32_t length, void* dest) = 0;
        
        virtual void CopyData(GpuBuffer& src, uint32_t srcOffset, uint32_t dstOffset, uint32_t length, bool discard = false, const Ref<CommandBuffer>& commandBuffer = nullptr) = 0;
        void CopyData(GpuBuffer& src, const Ref<CommandBuffer>& cmdBuffer = nullptr);

        void* Lock(uint32_t offset, uint32_t length, GpuLockOptions lockOptions);
        void Unlock();

        uint32_t GetSize() const { return m_Size; }
        bool IsLocked() const { return m_IsLocked; }
    protected:
        GpuBuffer(uint32_t size, BufferUsage usage);
    
    private:
        bool m_IsLocked = false;

    protected:
        uint32_t m_Size;
        BufferUsage m_Usage;
    };
}