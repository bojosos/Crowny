#pragma once

#include "Crowny/RenderAPI/CommandBuffer.h"

namespace Crowny
{
    class OpenGLCommandBuffer : public CommandBuffer
    {
    public:
        friend class CommandBuffer;

        CommandBufferState GetState() const override { return CommandBufferState::Done; }
        void Reset() override {}

    protected:
        OpenGLCommandBuffer(GpuQueueType type, uint32_t queueIdx, bool secondary) : CommandBuffer(type, queueIdx, secondary) {}
    };
} // namespace Crowny
