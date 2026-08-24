#include "cwpch.h"

#include "Platform/OpenGL/OpenGLUniformBufferBlock.h"

#include <glad/glad.h>

namespace Crowny
{
    OpenGLUniformBufferBlock::OpenGLUniformBufferBlock(uint32_t size, BufferUsage usage) : UniformBufferBlock(size, usage)
    {
        m_Buffer = new OpenGLGpuBuffer(GL_UNIFORM_BUFFER, size, usage);
    }

    OpenGLUniformBufferBlock::~OpenGLUniformBufferBlock() { delete m_Buffer; }

    void OpenGLUniformBufferBlock::FlushToGpu()
    {
        if (!m_BufferDirty)
            return;

        m_Buffer->WriteData(0, m_Size, m_CachedData, BWT_DISCARD);
        m_BufferDirty = false;
    }

    uint32_t OpenGLUniformBufferBlock::GetRendererID() const { return static_cast<OpenGLGpuBuffer*>(m_Buffer)->GetRendererID(); }
} // namespace Crowny
