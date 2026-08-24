#include "cwpch.h"

#include "Platform/OpenGL/OpenGLUtils.h"
#include "Platform/OpenGL/OpenGLVertexBuffer.h"

#include <glad/glad.h>

namespace Crowny
{
    namespace
    {
        class ArrayBufferBinding
        {
        public:
            ArrayBufferBinding()
            {
                GLint binding = 0;
                glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &binding);
                m_Binding = static_cast<GLuint>(binding);
            }
            ~ArrayBufferBinding() { glBindBuffer(GL_ARRAY_BUFFER, m_Binding); }

        private:
            GLuint m_Binding = 0;
        };
    } // namespace

    OpenGLVertexBuffer::OpenGLVertexBuffer(uint32_t size, BufferUsage usage) : m_Size(size)
    {
        CW_ENGINE_ASSERT(size > 0, "Cannot create an empty OpenGL vertex buffer");
        ArrayBufferBinding restoreBinding;
        glGenBuffers(1, &m_RendererID);
        glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
        glBufferData(GL_ARRAY_BUFFER, size, nullptr, OpenGLUtils::BufferUsageToOpenGLBufferUsage(usage));
    }

    OpenGLVertexBuffer::OpenGLVertexBuffer(void* data, uint32_t size, BufferUsage usage) : m_Size(size)
    {
        CW_ENGINE_ASSERT(size > 0, "Cannot create an empty OpenGL vertex buffer");
        ArrayBufferBinding restoreBinding;
        glGenBuffers(1, &m_RendererID);
        glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
        glBufferData(GL_ARRAY_BUFFER, size, data, OpenGLUtils::BufferUsageToOpenGLBufferUsage(usage));
    }

    OpenGLVertexBuffer::~OpenGLVertexBuffer()
    {
        if (m_RendererID != 0)
            glDeleteBuffers(1, &m_RendererID);
    }

    void* OpenGLVertexBuffer::Map(uint32_t offset, uint32_t size, GpuLockOptions options)
    {
        CW_ENGINE_ASSERT(offset <= m_Size && size <= m_Size - offset, "OpenGL vertex buffer map range is out of bounds");
        glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
        return glMapBufferRange(GL_ARRAY_BUFFER, offset, size, OpenGLUtils::LockOptionsToMapFlags(options));
    }

    void OpenGLVertexBuffer::Unmap()
    {
        glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
        CW_ENGINE_ASSERT(glUnmapBuffer(GL_ARRAY_BUFFER) == GL_TRUE, "OpenGL vertex buffer data became invalid while mapped");
    }

    void OpenGLVertexBuffer::WriteData(uint32_t offset, uint32_t length, const void* src, BufferWriteOptions writeOptions)
    {
        CW_ENGINE_ASSERT(src != nullptr, "OpenGL vertex buffer write source is null");
        CW_ENGINE_ASSERT(offset <= m_Size && length <= m_Size - offset, "OpenGL vertex buffer write range is out of bounds");
        ArrayBufferBinding restoreBinding;
        glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
        if (writeOptions == BWT_DISCARD)
            glBufferData(GL_ARRAY_BUFFER, m_Size, nullptr, OpenGLUtils::BufferUsageToOpenGLBufferUsage(BufferUsage::BU_DYNAMIC_DRAW));
        glBufferSubData(GL_ARRAY_BUFFER, offset, length, src);
    }

    void OpenGLVertexBuffer::ReadData(uint32_t offset, uint32_t length, void* dest)
    {
        CW_ENGINE_ASSERT(dest != nullptr, "OpenGL vertex buffer read destination is null");
        CW_ENGINE_ASSERT(offset <= m_Size && length <= m_Size - offset, "OpenGL vertex buffer read range is out of bounds");
        ArrayBufferBinding restoreBinding;
        glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
        glGetBufferSubData(GL_ARRAY_BUFFER, offset, length, dest);
    }
} // namespace Crowny
