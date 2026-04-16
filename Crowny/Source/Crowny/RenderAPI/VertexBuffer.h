#pragma once

#include "Crowny/RenderAPI/Buffer.h"
#include "Crowny/RenderAPI/GpuBuffer.h"

namespace Crowny
{

    struct VertexBufferDesc
    {
        uint32_t    Size  = 0;
        BufferUsage Usage = BufferUsage::BU_STATIC_DRAW;
        const void* Data  = nullptr; // null = allocate empty buffer
    };

    class VertexBuffer : public GpuBuffer
    {
    public:
        virtual ~VertexBuffer() = default;

        virtual void SetLayout(const Ref<BufferLayout>& layout) = 0;
        virtual const Ref<BufferLayout>& GetLayout() const = 0;

        virtual void* Map(uint32_t offset, uint32_t size, GpuLockOptions options) = 0;
        virtual void Unmap() = 0;

        virtual uint32_t GetBufferSize() const = 0;

        static Ref<VertexBuffer> Create(const VertexBufferDesc& desc);
    };
} // namespace Crowny