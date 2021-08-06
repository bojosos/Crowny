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

    private:
        VulkanGpuBuffer* m_DummyUniformBuffer;
    };

} // namespace Crowny