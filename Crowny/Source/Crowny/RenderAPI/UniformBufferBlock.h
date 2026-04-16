#pragma once

#include "Crowny/Common/RefCounted.h"
#include "Crowny/RenderAPI/GpuBuffer.h"

class BinaryDataStreamOutputArchive;
class BinaryDataStreamInputArchive;

namespace Crowny
{

    class UniformBufferBlock : public RefCounted
    {
    public:
        virtual ~UniformBufferBlock();

        void Write(uint32_t offset, const void* data, uint32_t size);
        void Read(uint32_t offset, void* data, uint32_t size) const;
        void ZeroOut(uint32_t offset, uint32_t size);
        void FlushToGpu();

    public:
        static Ref<UniformBufferBlock> Create(uint32_t size, BufferUsage usage = BufferUsage::BU_STATIC_DRAW);

    protected:
        UniformBufferBlock(uint32_t size, BufferUsage usage);
        friend void Save(BinaryDataStreamOutputArchive&, const class Material&);
        friend void Load(BinaryDataStreamInputArchive&, class Material&);

        BufferUsage m_Usage;
        GpuBuffer* m_Buffer;
        uint8_t* m_CachedData;
        uint32_t m_Size;
        bool m_BufferDirty;
    };

} // namespace Crowny