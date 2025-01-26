#pragma once

#include "Crowny/RenderAPI/Buffer.h"
#include "Crowny/RenderAPI/GpuBuffer.h"

namespace Crowny
{

    class VertexBuffer : public GpuBuffer
    {
    public:
        virtual ~VertexBuffer() = default;

        virtual void Bind() const = 0;
        virtual void Unbind() const = 0;

        virtual void SetLayout(const Ref<BufferLayout>& layout) = 0;
        virtual const Ref<BufferLayout>& GetLayout() const = 0;

        virtual void* Map(uint32_t offset, uint32_t size, GpuLockOptions options) = 0;
        virtual void Unmap() = 0;

        virtual uint32_t GetBufferSize() const = 0;

        static Ref<VertexBuffer> Create(uint32_t size, BufferUsage usage = BufferUsage::STATIC_DRAW);
        static Ref<VertexBuffer> Create(void* vertices, uint32_t size, BufferUsage usage = BufferUsage::STATIC_DRAW);
    };
} // namespace Crowny