#include "cwpch.h"

#include "Platform/OpenGL/OpenGLVertexArray.h"

#include "Platform/OpenGL/OpenGLIndexBuffer.h"
#include "Platform/OpenGL/OpenGLVertexBuffer.h"

#include <glad/glad.h>

namespace Crowny
{
    namespace
    {
        GLenum AttributeType(ShaderDataType type)
        {
            switch (type)
            {
            case ShaderDataType::Bool:
            case ShaderDataType::Int:
            case ShaderDataType::Int2:
            case ShaderDataType::Int3:
            case ShaderDataType::Int4: return GL_INT;
            case ShaderDataType::SByte:
            case ShaderDataType::SByte2:
            case ShaderDataType::SByte3:
            case ShaderDataType::SByte4: return GL_BYTE;
            case ShaderDataType::UByte4:
            case ShaderDataType::Color: return GL_UNSIGNED_BYTE;
            default: return GL_FLOAT;
            }
        }

        bool IsInteger(ShaderDataType type)
        {
            return type == ShaderDataType::Bool || type == ShaderDataType::Int || type == ShaderDataType::Int2 ||
                   type == ShaderDataType::Int3 || type == ShaderDataType::Int4;
        }
    } // namespace

    OpenGLVertexArray::OpenGLVertexArray(DrawMode drawMode) : m_DrawMode(drawMode) { glGenVertexArrays(1, &m_RendererID); }

    OpenGLVertexArray::~OpenGLVertexArray()
    {
        if (m_RendererID != 0)
            glDeleteVertexArrays(1, &m_RendererID);
    }

    void OpenGLVertexArray::Bind() const { glBindVertexArray(m_RendererID); }

    void OpenGLVertexArray::Unbind() const { glBindVertexArray(0); }

    void OpenGLVertexArray::AddVertexBuffer(const Ref<VertexBuffer>& vertexBuffer)
    {
        CW_ENGINE_ASSERT(vertexBuffer && vertexBuffer->GetLayout() && !vertexBuffer->GetLayout()->GetElements().empty(),
                         "OpenGL vertex array requires a buffer layout");
        glBindVertexArray(m_RendererID);
        glBindBuffer(GL_ARRAY_BUFFER, static_cast<OpenGLVertexBuffer*>(vertexBuffer.get())->GetRendererID());

        const BufferLayout& layout = *vertexBuffer->GetLayout();
        for (const BufferElement& element : layout)
        {
            const uint32_t baseLocation = element.Location == UINT32_MAX ? m_VertexBufferIndex : element.Location;
            const uint32_t columns = element.Type == ShaderDataType::Mat3 ? 3 : element.Type == ShaderDataType::Mat4 ? 4 : 1;
            const uint32_t components = element.Type == ShaderDataType::Color ? 4 : columns > 1 ? columns : element.GetComponentCount();
            for (uint32_t column = 0; column < columns; ++column)
            {
                const uint32_t location = baseLocation + column;
                const uintptr_t offset = element.Offset + column * components * sizeof(float);
                glEnableVertexAttribArray(location);
                if (IsInteger(element.Type) && !element.Normalized)
                    glVertexAttribIPointer(location, components, AttributeType(element.Type), layout.GetStride(),
                                           reinterpret_cast<const void*>(offset));
                else
                    glVertexAttribPointer(location, components, AttributeType(element.Type), element.Normalized ? GL_TRUE : GL_FALSE,
                                          layout.GetStride(), reinterpret_cast<const void*>(offset));
                glVertexAttribDivisor(location, element.InstanceRate);
            }
            m_VertexBufferIndex = std::max(m_VertexBufferIndex, baseLocation + columns);
        }
        m_VertexBuffers.push_back(vertexBuffer);
    }

    void OpenGLVertexArray::SetIndexBuffer(const Ref<IndexBuffer>& indexBuffer)
    {
        glBindVertexArray(m_RendererID);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,
                     indexBuffer ? static_cast<OpenGLIndexBuffer*>(indexBuffer.get())->GetRendererID() : 0);
        m_IndexBuffer = indexBuffer;
    }
} // namespace Crowny
