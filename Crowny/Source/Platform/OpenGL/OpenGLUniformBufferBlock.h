#pragma once

#include "Crowny/RenderAPI/UniformBufferBlock.h"

#include "Platform/OpenGL/OpenGLGpuBuffer.h"

namespace Crowny
{
    class OpenGLUniformBufferBlock : public UniformBufferBlock
    {
    public:
        friend class UniformBufferBlock;
        ~OpenGLUniformBufferBlock() override;

        void FlushToGpu() override;
        uint32_t GetRendererID() const;

    protected:
        OpenGLUniformBufferBlock(uint32_t size, BufferUsage usage);
    };
} // namespace Crowny
