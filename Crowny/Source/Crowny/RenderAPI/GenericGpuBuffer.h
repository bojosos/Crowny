#pragma once

#include "Crowny/Common/StdHeaders.h"
#include "Crowny/Common/Types.h"
#include "Crowny/RenderAPI/Buffer.h"
#include "Crowny/RenderAPI/GpuBuffer.h"

namespace Crowny
{
    class GenericGpuBuffer : public GpuBuffer
    {
    public:
        virtual ~GenericGpuBuffer() = default;

        virtual void* Map(uint32_t offset, uint32_t size, GpuLockOptions options) = 0;
        virtual void Unmap() = 0;

        virtual uint32_t GetBufferSize() const = 0;

        static Ref<GenericGpuBuffer> Create(uint32_t elementCount, uint32_t elementSize, GpuBufferType type, GpuBufferFormat format,
                                            BufferUsage usage);
    };
} // namespace Crowny