#include "cwpch.h"

#include "Platform/OpenGL/OpenGLGpuBuffer.h"
#include "Platform/OpenGL/OpenGLUtils.h"

#include <glad/glad.h>

#include <stdexcept>

namespace Crowny
{
    OpenGLGpuBuffer::OpenGLGpuBuffer(uint32_t target, uint32_t size, BufferUsage usage) : GpuBuffer(size, usage), m_Target(target)
    {
        CW_ENGINE_ASSERT(size > 0, "Cannot create an empty OpenGL GPU buffer");
        GLint previous = 0;
        const GLenum bindingQuery = target == GL_UNIFORM_BUFFER ? GL_UNIFORM_BUFFER_BINDING
                                    : target == GL_SHADER_STORAGE_BUFFER ? GL_SHADER_STORAGE_BUFFER_BINDING
                                                                         : GL_ARRAY_BUFFER_BINDING;
        glGetIntegerv(bindingQuery, &previous);
        glGenBuffers(1, &m_RendererID);
        glBindBuffer(m_Target, m_RendererID);
        glBufferData(m_Target, size, nullptr, OpenGLUtils::BufferUsageToOpenGLBufferUsage(usage));
        glBindBuffer(m_Target, static_cast<GLuint>(previous));
    }

    OpenGLGpuBuffer::~OpenGLGpuBuffer()
    {
        if (m_RendererID != 0)
            glDeleteBuffers(1, &m_RendererID);
    }

    void* OpenGLGpuBuffer::Map(uint32_t offset, uint32_t length, GpuLockOptions options, CW_MAYBE_UNUSED uint32_t queueIdx)
    {
        CW_ENGINE_ASSERT(offset <= m_Size && length <= m_Size - offset, "OpenGL GPU buffer map range is out of bounds");
        glBindBuffer(m_Target, m_RendererID);
        void* data = glMapBufferRange(m_Target, offset, length, OpenGLUtils::LockOptionsToMapFlags(options));
        if (data == nullptr)
        {
            const GLenum error = glGetError();
            throw std::runtime_error("OpenGL buffer mapping failed with error " + std::to_string(error));
        }
        return data;
    }

    void OpenGLGpuBuffer::Unmap()
    {
        glBindBuffer(m_Target, m_RendererID);
        CW_ENGINE_ASSERT(glUnmapBuffer(m_Target) == GL_TRUE, "OpenGL GPU buffer data became invalid while mapped");
    }

    void OpenGLGpuBuffer::WriteData(uint32_t offset, uint32_t length, const void* src, BufferWriteOptions writeOptions)
    {
        CW_ENGINE_ASSERT(src != nullptr, "OpenGL GPU buffer write source is null");
        CW_ENGINE_ASSERT(offset <= m_Size && length <= m_Size - offset, "OpenGL GPU buffer write range is out of bounds");
        glBindBuffer(m_Target, m_RendererID);
        if (writeOptions == BWT_DISCARD)
            glBufferData(m_Target, m_Size, nullptr, OpenGLUtils::BufferUsageToOpenGLBufferUsage(m_Usage));
        glBufferSubData(m_Target, offset, length, src);
    }

    void OpenGLGpuBuffer::ReadData(uint32_t offset, uint32_t length, void* dest)
    {
        CW_ENGINE_ASSERT(dest != nullptr, "OpenGL GPU buffer read destination is null");
        CW_ENGINE_ASSERT(offset <= m_Size && length <= m_Size - offset, "OpenGL GPU buffer read range is out of bounds");
        glBindBuffer(m_Target, m_RendererID);
        glGetBufferSubData(m_Target, offset, length, dest);
    }

    OpenGLGenericGpuBuffer::OpenGLGenericGpuBuffer(uint32_t elementCount, uint32_t elementSize, GpuBufferType type, GpuBufferFormat format,
                                                   BufferUsage usage)
      : m_Buffer(GLAD_GL_VERSION_4_3 ? GL_SHADER_STORAGE_BUFFER : GL_ARRAY_BUFFER, elementCount * elementSize, usage),
        m_Size(elementCount * elementSize), m_Type(type), m_Format(format)
    {
        if (!GLAD_GL_VERSION_4_3)
            CW_ENGINE_WARN("Generic OpenGL GPU buffers require OpenGL 4.3 before they can be bound to shaders");
    }

    void* OpenGLGenericGpuBuffer::Map(uint32_t offset, uint32_t size, GpuLockOptions options) { return m_Buffer.Map(offset, size, options); }

    void OpenGLGenericGpuBuffer::Unmap() { m_Buffer.Unmap(); }

    void OpenGLGenericGpuBuffer::WriteData(uint32_t offset, uint32_t length, const void* src, BufferWriteOptions writeOptions)
    {
        m_Buffer.WriteData(offset, length, src, writeOptions);
    }

    void OpenGLGenericGpuBuffer::ReadData(uint32_t offset, uint32_t length, void* dest) { m_Buffer.ReadData(offset, length, dest); }
} // namespace Crowny
