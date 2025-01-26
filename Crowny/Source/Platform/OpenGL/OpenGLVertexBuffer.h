#pragma once

#include "Crowny/RenderAPI/VertexBuffer.h"

namespace Crowny
{

    class OpenGLVertexBuffer : public VertexBuffer
    {
    public:
        OpenGLVertexBuffer(void* vertices, uint32_t size, BufferUsage usage);
        OpenGLVertexBuffer(uint32_t size, BufferUsage usage);
        ~OpenGLVertexBuffer();

        virtual void Bind() const override;
        virtual void Unbind() const override;

        virtual const Ref<BufferLayout>& GetLayout() const override { return m_Layout; };
        virtual void SetLayout(const Ref<BufferLayout>& layout) override { m_Layout = layout; }

        virtual void WriteData(uint32_t offset, uint32_t length, const void* src,
                               BufferWriteOptions writeOptions /* = BWT_NORMAL */) override
        {
            // m_Buffer->WriteData(offset, length, src, writeOptions);
        }

        virtual uint32_t GetBufferSize() const override { return 0; }
        virtual void ReadData(uint32_t offset, uint32_t length, void* dest) override
        {
            //   m_Buffer->ReadData(offset, length, dest);
        }

        virtual void* Map(uint32_t offset, uint32_t size, GpuLockOptions options) override;
        virtual void Unmap() override;

    private:
        uint32_t m_RendererID, m_Size;
        Ref<BufferLayout> m_Layout;
    };
} // namespace Crowny