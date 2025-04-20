#pragma once

#include "Crowny/Common/Module.h"

namespace Crowny
{

    class VulkanGpuBuffer;

    class VulkanGpuBufferManager : public Module<VulkanGpuBufferManager>
    {
    public:
        VulkanGpuBufferManager();
        ~VulkanGpuBufferManager();

        VulkanGpuBuffer* GetDummyUniformBuffer() const { return m_DummyUniformBuffer; }

        VulkanGpuBuffer* GetDummyReadBuffer() const { return m_DummyReadBuffer; }
        VulkanGpuBuffer* GetDummyStorageBuffer() const { return m_DummyStorageBuffer; }
        VulkanGpuBuffer* GetDummyStructuredBuffer() const { return m_DummyStructuredBuffer; }

    private:
        VulkanGpuBuffer* m_DummyUniformBuffer;
        VulkanGpuBuffer* m_DummyReadBuffer;
        VulkanGpuBuffer* m_DummyStorageBuffer;
        VulkanGpuBuffer* m_DummyStructuredBuffer;
    };

} // namespace Crowny