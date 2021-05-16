#include "cwpch.h"

#include "Platform/Vulkan/VulkanUniformBuffer.h"

namespace Crowny
{

    VulkanUniformBuffer::VulkanUniformBuffer(uint32_t size, BufferUsage usage)
    {
        m_Buffer = new VulkanGpuBuffer(VulkanGpuBuffer::BUFFER_UNIFORM, usage, size);
    }

    VulkanUniformBuffer::~VulkanUniformBuffer()
    {
        delete m_Buffer;
    }

}