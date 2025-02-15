#include "cwpch.h"

#include "Platform/Vulkan/VulkanGpuBuffer.h"
#include "Platform/Vulkan/VulkanGpuBufferManager.h"

namespace Crowny
{

    VulkanGpuBufferManager::VulkanGpuBufferManager()
    {
        m_DummyUniformBuffer = new VulkanGpuBuffer(VulkanGpuBuffer::BUFFER_UNIFORM, BufferUsage::STATIC_DRAW, 16);
    }

    VulkanGpuBufferManager::~VulkanGpuBufferManager() { delete m_DummyUniformBuffer; }

} // namespace Crowny