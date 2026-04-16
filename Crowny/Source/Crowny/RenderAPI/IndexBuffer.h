#pragma once

#include "Crowny/RenderAPI/Buffer.h"
#include "Crowny/RenderAPI/GpuBuffer.h"

namespace Crowny
{

    struct IndexBufferDesc
    {
        uint32_t    Count = 0;
        IndexType   Type  = IndexType::Index_32;
        BufferUsage Usage = BufferUsage::BU_STATIC_DRAW;
        const void* Data  = nullptr; // null = allocate empty buffer
    };

    class IndexBuffer : public GpuBuffer
    {
    public:
        virtual ~IndexBuffer() = default;

        virtual uint32_t GetCount() const = 0;

        virtual void* Map(uint32_t offset, uint32_t size, GpuLockOptions options) = 0;
        virtual void Unmap() = 0;

        virtual IndexType GetIndexType() const = 0;
        virtual uint32_t GetBufferSize() const = 0;

        static Ref<IndexBuffer> Create(const IndexBufferDesc& desc);
    };
} // namespace Crowny