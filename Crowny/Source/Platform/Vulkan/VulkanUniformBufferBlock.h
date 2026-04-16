#pragma once

#include "Crowny/RenderAPI/UniformBufferBlock.h"

#include "Platform/Vulkan/VulkanGpuBuffer.h"

namespace Crowny
{

    class VulkanUniformBufferBlock : public UniformBufferBlock
    {
    public:
        friend class UniformBufferBlock;
        ~VulkanUniformBufferBlock();

        VulkanBuffer* GetBuffer() const;

    protected:
        VulkanUniformBufferBlock(uint32_t size, BufferUsage usage);
    };

} // namespace Crowny