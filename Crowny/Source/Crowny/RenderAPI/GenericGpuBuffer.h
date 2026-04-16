#pragma once

#include "Crowny/Common/StdHeaders.h"
#include "Crowny/Common/Types.h"
#include "Crowny/RenderAPI/Buffer.h"
#include "Crowny/RenderAPI/GpuBuffer.h"

namespace Crowny
{

    struct GenericGpuBufferDesc
    {
        uint32_t        ElementCount = 0;
        uint32_t        ElementSize  = 0;
        GpuBufferType   Type         = GpuBufferType::Standard;
        GpuBufferFormat Format       = BF_UNKNOWN;
        BufferUsage     Usage        = BufferUsage::BU_STATIC_DRAW;
    };

    class GenericGpuBuffer : public GpuBuffer
    {
    public:
        virtual ~GenericGpuBuffer() = default;

        virtual void* Map(uint32_t offset, uint32_t size, GpuLockOptions options) = 0;
        virtual void Unmap() = 0;

        virtual uint32_t GetBufferSize() const = 0;

        static Ref<GenericGpuBuffer> Create(const GenericGpuBufferDesc& desc);
    };
} // namespace Crowny