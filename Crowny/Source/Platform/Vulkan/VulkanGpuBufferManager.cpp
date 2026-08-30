#include "cwpch.h"

#include "Platform/Vulkan/VulkanGpuBuffer.h"
#include "Platform/Vulkan/VulkanGpuBufferManager.h"

namespace Crowny
{

    VulkanGpuBufferManager::VulkanGpuBufferManager()
      : m_DummyUniformBuffer(nullptr), m_DummyReadBuffer(nullptr), m_DummyStorageBuffer(nullptr), m_DummyStructuredBuffer(nullptr)
    {
        m_DummyUniformBuffer = new VulkanGpuBuffer(VulkanGpuBuffer::BUFFER_UNIFORM, BufferUsage::BU_STATIC_DRAW, 16);
        m_DummyReadBuffer = new VulkanGpuBuffer(VulkanGpuBuffer::BUFFER_GENERIC, BufferUsage::BU_STATIC_DRAW, 16);
        m_DummyStorageBuffer = new VulkanGpuBuffer(VulkanGpuBuffer::BUFFER_GENERIC, BufferUsage::BU_LOADSTORE, 16);
        m_DummyStructuredBuffer = new VulkanGpuBuffer(VulkanGpuBuffer::BUFFER_STRUCTURED, BufferUsage::BU_LOADSTORE, 16);
    }

    VulkanGpuBufferManager::~VulkanGpuBufferManager()
    {
        delete m_DummyUniformBuffer;
        delete m_DummyReadBuffer;
        delete m_DummyStorageBuffer;
        delete m_DummyStructuredBuffer;
    }

} // namespace Crowny
