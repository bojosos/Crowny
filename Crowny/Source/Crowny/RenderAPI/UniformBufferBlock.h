#pragma once

#include "Crowny/RenderAPI/GpuBuffer.h"

namespace Crowny
{

    class UniformBufferBlock
    {
    public:
        UniformBufferBlock(uint32_t size, BufferUsage usage);
        virtual ~UniformBufferBlock();

        void Write(uint32_t offset, const void* data, uint32_t size);
        void Read(uint32_t offset, void* data, uint32_t size);
        void ZeroOut(uint32_t offset, uint32_t size);
        void FlushToGpu();

    public:
        static Ref<UniformBufferBlock> Create(uint32_t size, BufferUsage usage = BufferUsage::STATIC_DRAW);

    protected:
        BufferUsage m_Usage;
        GpuBuffer* m_Buffer;
        uint8_t* m_CachedData;
        uint32_t m_Size;
        bool m_BufferDirty;
    };

} // namespace Crowny