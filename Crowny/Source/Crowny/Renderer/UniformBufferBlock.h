#pragma once

#include "Crowny/Renderer/GpuBuffer.h"

namespace Crowny
{
    
    class UniformBufferBlock
    {
    public:
        UniformBufferBlock(uint32_t size, BufferUsage usage);
        virtual ~UniformBufferBlock();
        
        virtual void Write(uint32_t offset, const void* data, uint32_t size) = 0;
        virtual void Read(uint32_t offset, void* data, uint32_t size) = 0;
        void FlushToGpu();
    public:
        static Ref<UniformBufferBlock> Create(uint32_t size, BufferUsage usage);
    private:
        BufferUsage m_Usage;
        GpuBuffer* m_Buffer;
        uint8_t m_CachedData;
        uint32_t m_Size;
        bool m_BufferDirty = true;
    };
    
}