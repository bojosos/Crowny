#include "cwpch.h"

#include "Platform/Vulkan/VulkanUniformBufferBlock.h"

namespace Crowny
{

    VulkanUniformBufferBlock::VulkanUniformBufferBlock(uint32_t size, BufferUsage usage)
      : UniformBufferBlock(size, usage)
    {
        m_Buffer = new VulkanGpuBuffer(VulkanGpuBuffer::BUFFER_UNIFORM, usage, size);
    }

    VulkanUniformBufferBlock::~VulkanUniformBufferBlock() { delete m_Buffer; }

    VulkanBuffer* VulkanUniformBufferBlock::GetBuffer() const
    {
        return static_cast<VulkanGpuBuffer*>(m_Buffer)->GetBuffer();
    }

} // namespace Crowny