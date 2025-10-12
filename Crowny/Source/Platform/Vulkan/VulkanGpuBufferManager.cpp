#include "cwpch.h"

#include "Platform/Vulkan/VulkanGpuBuffer.h"
#include "Platform/Vulkan/VulkanGpuBufferManager.h"

namespace Crowny
{

    VulkanGpuBufferManager::VulkanGpuBufferManager()
    {
        m_DummyUniformBuffer = new VulkanGpuBuffer(VulkanGpuBuffer::BUFFER_UNIFORM, BufferUsage::STATIC_DRAW, 16);
        // m_DummyReadBuffer = new VulkanGpuBuffer(VulkanGpuBuffer::BUFFER_GENERIC, BF_32X1F, BufferUsage::STATIC_DRAW, 16);
        // m_DummyStorageBuffer = new VulkanGpuBuffer(VulkanGpuBuffer::BUFFER_GENERIC, BF_32X1F, BufferUsage::LOADSTORE, 16);
        // m_DummyStructuredBuffer = new VulkanGpuBuffer(VulkanGpuBuffer::BUFFER_GENERIC, BF_UNKNOWN, BufferUsage::LOADSTORE, 16);
    }

    VulkanGpuBufferManager::~VulkanGpuBufferManager()
    {
        delete m_DummyUniformBuffer;
        // delete m_DummyReadBuffer;
        // delete m_DummyStorageBuffer;
        // delete m_DummyStructuredBuffer;
    }

} // namespace Crowny