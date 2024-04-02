#pragma once

#include "Crowny/RenderAPI/IndexBuffer.h"

namespace Crowny
{
    class OpenGLIndexBuffer : public IndexBuffer
    {
    public:
        OpenGLIndexBuffer(uint32_t count, IndexType indexType, BufferUsage usage);
        OpenGLIndexBuffer(uint16_t* indices, uint32_t count, BufferUsage usage);
        OpenGLIndexBuffer(uint32_t* indices, uint32_t count, BufferUsage usage);
        virtual ~OpenGLIndexBuffer();

        virtual void Bind() const override;
        virtual void Unbind() const override;

        virtual void* Map(uint32_t offset, uint32_t size, GpuLockOptions options) override;
        virtual void Unmap() override;
        virtual void WriteData(uint32_t offset, uint32_t length, const void* src,
                               BufferWriteOptions writeOptions /* = BWT_NORMAL */) override
        {
            // m_Buffer->WriteData(offset, length, src, writeOptions);
        }

        virtual void ReadData(uint32_t offset, uint32_t length, void* dest) override
        {
            // m_Buffer->ReadData(offset, length, dest);
        }

        virtual uint32_t GetCount() const override { return m_Count; }
        virtual IndexType GetIndexType() const override { return m_IndexType; }
        virtual uint32_t GetBufferSize() const override
        {
            return m_Count * (m_IndexType == IndexType::Index_16 ? sizeof(uint16_t) : sizeof(uint32_t));
        }

    private:
        uint32_t m_RendererID, m_Count;
        IndexType m_IndexType;
    };
} // namespace Crowny
