#include "cwpch.h"

#include "Platform/OpenGL/OpenGLIndexBuffer.h"
#include "Platform/OpenGL/OpenGLUtils.h"

#include <glad/glad.h>

namespace Crowny
{
    namespace
    {
        uint32_t IndexSize(IndexType type) { return type == IndexType::Index_16 ? sizeof(uint16_t) : sizeof(uint32_t); }

        class ElementBufferBinding
        {
        public:
            ElementBufferBinding()
            {
                GLint binding = 0;
                glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &binding);
                m_Binding = static_cast<GLuint>(binding);
            }
            ~ElementBufferBinding() { glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_Binding); }

        private:
            GLuint m_Binding = 0;
        };
    } // namespace

    OpenGLIndexBuffer::OpenGLIndexBuffer(uint32_t count, IndexType indexType, BufferUsage usage) : m_Count(count), m_IndexType(indexType)
    {
        CW_ENGINE_ASSERT(count > 0, "Cannot create an empty OpenGL index buffer");
        ElementBufferBinding restoreBinding;
        glGenBuffers(1, &m_RendererID);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RendererID);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * IndexSize(indexType), nullptr, OpenGLUtils::BufferUsageToOpenGLBufferUsage(usage));
    }

    OpenGLIndexBuffer::OpenGLIndexBuffer(uint16_t* indices, uint32_t count, BufferUsage usage)
      : m_Count(count), m_IndexType(IndexType::Index_16)
    {
        ElementBufferBinding restoreBinding;
        glGenBuffers(1, &m_RendererID);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RendererID);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof(uint16_t), indices, OpenGLUtils::BufferUsageToOpenGLBufferUsage(usage));
    }

    OpenGLIndexBuffer::OpenGLIndexBuffer(uint32_t* indices, uint32_t count, BufferUsage usage)
      : m_Count(count), m_IndexType(IndexType::Index_32)
    {
        ElementBufferBinding restoreBinding;
        glGenBuffers(1, &m_RendererID);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RendererID);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof(uint32_t), indices, OpenGLUtils::BufferUsageToOpenGLBufferUsage(usage));
    }

    OpenGLIndexBuffer::~OpenGLIndexBuffer()
    {
        if (m_RendererID != 0)
            glDeleteBuffers(1, &m_RendererID);
    }

    void* OpenGLIndexBuffer::Map(uint32_t offset, uint32_t size, GpuLockOptions options)
    {
        CW_ENGINE_ASSERT(offset <= GetBufferSize() && size <= GetBufferSize() - offset, "OpenGL index buffer map range is out of bounds");
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RendererID);
        return glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, offset, size, OpenGLUtils::LockOptionsToMapFlags(options));
    }

    void OpenGLIndexBuffer::Unmap()
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RendererID);
        CW_ENGINE_ASSERT(glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER) == GL_TRUE, "OpenGL index buffer data became invalid while mapped");
    }

    void OpenGLIndexBuffer::WriteData(uint32_t offset, uint32_t length, const void* src, BufferWriteOptions writeOptions)
    {
        CW_ENGINE_ASSERT(src != nullptr, "OpenGL index buffer write source is null");
        CW_ENGINE_ASSERT(offset <= GetBufferSize() && length <= GetBufferSize() - offset, "OpenGL index buffer write range is out of bounds");
        ElementBufferBinding restoreBinding;
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RendererID);
        if (writeOptions == BWT_DISCARD)
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, GetBufferSize(), nullptr,
                         OpenGLUtils::BufferUsageToOpenGLBufferUsage(BufferUsage::BU_DYNAMIC_DRAW));
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, offset, length, src);
    }

    void OpenGLIndexBuffer::ReadData(uint32_t offset, uint32_t length, void* dest)
    {
        CW_ENGINE_ASSERT(dest != nullptr, "OpenGL index buffer read destination is null");
        CW_ENGINE_ASSERT(offset <= GetBufferSize() && length <= GetBufferSize() - offset, "OpenGL index buffer read range is out of bounds");
        ElementBufferBinding restoreBinding;
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RendererID);
        glGetBufferSubData(GL_ELEMENT_ARRAY_BUFFER, offset, length, dest);
    }
} // namespace Crowny
