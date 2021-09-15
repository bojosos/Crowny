#pragma once

#include "Crowny/RenderAPI/Buffer.h"

namespace Crowny
{

    class IndexBuffer
    {
    public:
        virtual ~IndexBuffer() = default;

        virtual void Bind() const = 0;
        virtual void Unbind() const = 0;

        virtual uint32_t GetCount() const = 0;

        virtual void* Map(uint32_t offset, uint32_t size, GpuLockOptions options) = 0;
        virtual void Unmap() = 0;

        virtual IndexType GetIndexType() const = 0;

        // TODO: make this take size, not count
        static Ref<IndexBuffer> Create(uint32_t count, IndexType indexType = IndexType::Index_32,
                                       BufferUsage usage = BufferUsage::STATIC_DRAW);
        static Ref<IndexBuffer> Create(uint16_t* indices, uint32_t count, BufferUsage usage = BufferUsage::STATIC_DRAW);
        static Ref<IndexBuffer> Create(uint32_t* indices, uint32_t count, BufferUsage usage = BufferUsage::STATIC_DRAW);
    };
} // namespace Crowny