#pragma once

#include "Crowny/RenderAPI/VertexArray.h"

namespace Crowny
{
    class OpenGLVertexArray : public VertexArray
    {
    public:
        OpenGLVertexArray(DrawMode drawMode);
        virtual ~OpenGLVertexArray();

        virtual void Bind() const override;
        virtual void Unbind() const override;

        virtual void AddVertexBuffer(const Ref<VertexBuffer>& vbo) override;
        virtual void SetIndexBuffer(const Ref<IndexBuffer>& ibo) override;

        virtual const std::vector<Ref<VertexBuffer>>& GetVertexBuffers() const override { return m_VertexBuffers; };
        virtual const Ref<IndexBuffer>& GetIndexBuffer() const override { return m_IndexBuffer; };
        virtual DrawMode GetDrawMode() const override { return m_DrawMode; }

    private:
        DrawMode m_DrawMode;
        uint32_t m_RendererID;
        uint32_t m_VertexBufferIndex = 0;
        std::vector<Ref<VertexBuffer>> m_VertexBuffers;
        Ref<IndexBuffer> m_IndexBuffer;
    };
} // namespace Crowny