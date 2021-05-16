#pragma once

#include "Crowny/Renderer/UniformBuffer.h"

#include "Platform/Vulkan/VulkanGpuBuffer.h"

namespace Crowny
{

    class VulkanUniformBuffer : public UniformBuffer
    {
    public:
        VulkanUniformBuffer(uint32_t size, BufferUsage usage);
        ~VulkanUniformBuffer();
        
    private:
        VulkanGpuBuffer* m_Buffer;
    };

}