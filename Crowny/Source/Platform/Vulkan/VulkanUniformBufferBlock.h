#pragma once

#include "Crowny/RenderAPI/UniformBufferBlock.h"

#include "Platform/Vulkan/VulkanGpuBuffer.h"

namespace Crowny
{

    class VulkanUniformBufferBlock : public UniformBufferBlock
    {
    public:
        VulkanUniformBufferBlock(uint32_t size, BufferUsage usage);
        ~VulkanUniformBufferBlock();

        VulkanBuffer* GetBuffer() const;
    };

} // namespace Crowny