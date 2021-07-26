#pragma once

#include "Crowny/Renderer/SamplerState.h"

#include "Platform/Vulkan/VulkanResource.h"

namespace Crowny
{

    class VulkanSampler : public VulkanResource
    {
    public:
        VulkanSampler(VulkanResourceManager* owner, VkSampler sampler);
        ~VulkanSampler();
        VkSampler GetHandle() const  {return m_Sampler; }

    private:
        VkSampler m_Sampler;
    };

    class VulkanSamplerState : public SamplerState
    {
    public:
        VulkanSamplerState(const SamplerStateDesc& desc);
        ~VulkanSamplerState();
        VulkanSampler* GetSampler() const { return m_Sampler; }

    private:
        VulkanSampler* m_Sampler;
        
    };

}