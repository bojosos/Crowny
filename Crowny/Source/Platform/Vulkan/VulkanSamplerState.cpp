#include "cwpch.h"

#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanRenderAPI.h"
#include "Platform/Vulkan/VulkanSamplerState.h"

namespace Crowny
{

    VulkanSampler::VulkanSampler(VulkanResourceManager* owner, VkSampler sampler) : VulkanResource(owner, true), m_Sampler(sampler) {}

    VulkanSampler::~VulkanSampler() { vkDestroySampler(m_Owner->GetDevice().GetLogicalDevice(), m_Sampler, gVulkanAllocator); }

    VulkanSamplerState::VulkanSamplerState(const SamplerStateDesc& desc) : SamplerState(desc)
    {
        VulkanDevice& device = *gVulkanRenderAPI().GetPresentDevice();
        const bool anisotropyRequested = desc.MaxAnsio > 1 || desc.MinFilter == TextureFilter::ANISOTROPIC || desc.MagFilter == TextureFilter::ANISOTROPIC ||
                                         desc.MipFilter == TextureFilter::ANISOTROPIC;
        const bool anisotropyEnabled = anisotropyRequested && device.GetDeviceFeatures().samplerAnisotropy == VK_TRUE;

        VkSamplerCreateInfo samplerCreateInfo{};
        samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerCreateInfo.flags = 0;
        samplerCreateInfo.pNext = nullptr;
        samplerCreateInfo.magFilter = VulkanUtils::GetFilter(desc.MagFilter);
        samplerCreateInfo.minFilter = VulkanUtils::GetFilter(desc.MinFilter);
        samplerCreateInfo.mipmapMode = VulkanUtils::GetMipFilter(desc.MipFilter);
        samplerCreateInfo.addressModeU = VulkanUtils::GetAddressingMode(desc.AddressMode.U);
        samplerCreateInfo.addressModeV = VulkanUtils::GetAddressingMode(desc.AddressMode.V);
        samplerCreateInfo.addressModeW = VulkanUtils::GetAddressingMode(desc.AddressMode.W);
        samplerCreateInfo.mipLodBias = desc.MipmapBias;
        samplerCreateInfo.anisotropyEnable = anisotropyEnabled;
        samplerCreateInfo.maxAnisotropy =
          anisotropyEnabled ? glm::clamp((float)std::max(desc.MaxAnsio, 1U), 1.0f, device.GetDeviceProperties().limits.maxSamplerAnisotropy) : 1.0f;
        samplerCreateInfo.compareEnable = desc.CompareFunc != CompareFunction::ALWAYS_PASS;
        samplerCreateInfo.compareOp = VulkanUtils::GetCompareOp(desc.CompareFunc);
        samplerCreateInfo.minLod = desc.MipFilter == TextureFilter::NONE ? 0.0f : desc.MipMin;
        samplerCreateInfo.maxLod = desc.MipFilter == TextureFilter::NONE ? 0.0f : desc.MipMax;
        samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
        samplerCreateInfo.unnormalizedCoordinates = false;

        VkSampler sampler;
        VkResult result = vkCreateSampler(device.GetLogicalDevice(), &samplerCreateInfo, gVulkanAllocator, &sampler);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
        m_Sampler = device.GetResourceManager().Create<VulkanSampler>(sampler);
    }

    VulkanSamplerState::~VulkanSamplerState() { m_Sampler->Destroy(); }

} // namespace Crowny
