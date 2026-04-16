#pragma once

#include "Crowny/RenderAPI/SamplerState.h"

#include "Platform/Vulkan/VulkanResource.h"

namespace Crowny
{

    class VulkanSampler : public VulkanResource
    {
    public:
        VulkanSampler(VulkanResourceManager* owner, VkSampler sampler);
        ~VulkanSampler();
        VkSampler GetHandle() const { return m_Sampler; }

    private:
        VkSampler m_Sampler;
    };

    class VulkanSamplerState : public SamplerState
    {
    public:
        friend class SamplerState;
        ~VulkanSamplerState();
        VulkanSampler* GetSampler() const { return m_Sampler; }
    protected:
        VulkanSamplerState(const SamplerStateDesc& desc);

    private:
        VulkanSampler* m_Sampler;
    };

} // namespace Crowny