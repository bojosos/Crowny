#include "cwpch.h"

#include "Platform/OpenGL/OpenGLUtils.h"
#include "Platform/OpenGL/OpenGLVertexBuffer.h"

#include <glad/glad.h>

namespace Crowny
{

    OpenGLVertexBuffer::OpenGLVertexBuffer(uint32_t size, BufferUsage) : m_Size(size)
    {
#ifndef MC_WEB
        glGenBuffers(1, &m_RendererID);
#else
        glCreateBuffers(1, &m_RendererID);
#endif
    }

    OpenGLVertexBuffer::OpenGLVertexBuffer(void* data, uint32_t size, BufferUsage usage) : m_Size(size)
    {
#ifndef MC_WEB
        glGenBuffers(1, &m_RendererID);
        glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
        glBufferData(GL_ARRAY_BUFFER, size, data, OpenGLUtils::BufferUsageToOpenGLBufferUsage(usage));
#else
        glCreateBuffers(1, &m_RendererID);
        glNamedBufferData(m_RendererID, size, data, OpenGLUtils::BufferUsageToOpenGLBufferUsage(usage));
#endif
    }

    OpenGLVertexBuffer::~OpenGLVertexBuffer() { glDeleteBuffers(1, &m_RendererID); }

    void OpenGLVertexBuffer::Bind() const { glBindBuffer(GL_ARRAY_BUFFER, m_RendererID); }

    void OpenGLVertexBuffer::Unbind() const { glBindBuffer(GL_ARRAY_BUFFER, 0); }

    void* OpenGLVertexBuffer::Map(uint32_t offset, uint32_t size, GpuLockOptions options)
    {
        glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
        switch (options)
        {
        case (GpuLockOptions::READ_ONLY):
            return glMapBufferRange(GL_ARRAY_BUFFER, offset, size, GL_MAP_READ_BIT);
        case (GpuLockOptions::WRITE_ONLY):
            return glMapBufferRange(GL_ARRAY_BUFFER, offset, size, GL_MAP_WRITE_BIT);
        case (GpuLockOptions::WRITE_DISCARD):
            return glMapBufferRange(GL_ARRAY_BUFFER, offset, size, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
        case (GpuLockOptions::WRITE_DISCARD_RANGE):
            return glMapBufferRange(GL_ARRAY_BUFFER, offset, size, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT);
        case (GpuLockOptions::READ_WRITE):
            return glMapBufferRange(GL_ARRAY_BUFFER, offset, size, GL_MAP_WRITE_BIT | GL_MAP_READ_BIT);
        case (GpuLockOptions::WRITE_ONLY_NO_OVERWRITE):
            return glMapBufferRange(GL_ARRAY_BUFFER, offset, size, GL_MAP_WRITE_BIT);
        }
        return nullptr;
    }

    void OpenGLVertexBuffer::Unmap()
    {
        glUnmapNamedBuffer(m_RendererID); // TODO: es2, might work with emscripten
    }

} // namespace Crowny
