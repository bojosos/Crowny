#pragma once

#include "Crowny/RenderAPI/Buffer.h"
#include "Crowny/RenderAPI/IndexBuffer.h"
#include "Crowny/RenderAPI/VertexBuffer.h"

namespace Crowny
{

    class VertexArray
    {
    public:
        virtual ~VertexArray() = default;

        virtual void Bind() const = 0;
        virtual void Unbind() const = 0;

        virtual void AddVertexBuffer(const Ref<VertexBuffer>& vbo) = 0;

        virtual void SetIndexBuffer(const Ref<IndexBuffer>& ibo) = 0;

        virtual const Vector<Ref<VertexBuffer>>& GetVertexBuffers() const = 0;
        virtual const Ref<IndexBuffer>& GetIndexBuffer() const = 0;
        virtual DrawMode GetDrawMode() const = 0;

        static Ref<VertexArray> Create(DrawMode = DrawMode::TRIANGLE_LIST);
    };
} // namespace Crowny
