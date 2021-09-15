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

        virtual uint32_t GetCount() const override { return m_Count; }

        virtual IndexType GetIndexType() const override { return m_IndexType; }

    private:
        uint32_t m_RendererID, m_Count;
        IndexType m_IndexType;
    };
} // namespace Crowny
