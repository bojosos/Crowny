#pragma once

#include "Crowny/RenderAPI/VertexBuffer.h"

namespace Crowny
{

    class OpenGLVertexBuffer : public VertexBuffer
    {
    public:
        friend class VertexBuffer;
        ~OpenGLVertexBuffer();
        uint32_t GetRendererID() const { return m_RendererID; }
    protected:
        OpenGLVertexBuffer(void* vertices, uint32_t size, BufferUsage usage);
        OpenGLVertexBuffer(uint32_t size, BufferUsage usage);

        virtual const Ref<BufferLayout>& GetLayout() const override { return m_Layout; };
        virtual void SetLayout(const Ref<BufferLayout>& layout) override { m_Layout = layout; }

        virtual void WriteData(uint32_t offset, uint32_t length, const void* src, BufferWriteOptions writeOptions = BWT_NORMAL) override;

        virtual uint32_t GetBufferSize() const override { return m_Size; }
        virtual void ReadData(uint32_t offset, uint32_t length, void* dest) override;

        virtual void* Map(uint32_t offset, uint32_t size, GpuLockOptions options) override;
        virtual void Unmap() override;

    private:
        uint32_t m_RendererID = 0, m_Size = 0;
        Ref<BufferLayout> m_Layout;
    };
} // namespace Crowny
